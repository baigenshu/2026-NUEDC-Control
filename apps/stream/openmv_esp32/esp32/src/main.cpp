#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <cstring>

// OpenMV UART: ESP32 Serial2 RX=16 TX=17  <->  OpenMV P4(TX) P5(RX)
static const int UART_RX_PIN = 16;
static const int UART_TX_PIN = 17;
static const uint32_t UART_BAUD = 921600;

static const char *AP_SSID = "OpenMV-ESP";
static const char *AP_PASS = "12345678";

// QQVGA JPEG is typically a few KB; keep dual buffers in DRAM budget.
static const size_t MAX_JPEG = 16 * 1024;
static const uint8_t MAGIC0 = 0xAA;
static const uint8_t MAGIC1 = 0x55;

static uint8_t buf_a[MAX_JPEG];
static uint8_t buf_b[MAX_JPEG];
static uint8_t *write_buf = buf_a;
static uint8_t *display_buf = buf_b;
static size_t display_len = 0;
static uint32_t frame_count = 0;
static uint32_t last_frame_len = 0;
static volatile bool display_lock = false;

enum ParseState : uint8_t {
  WAIT_AA = 0,
  WAIT_55,
  READ_LEN,
  READ_PAYLOAD,
};

static ParseState state = WAIT_AA;
static uint8_t len_bytes[4];
static uint8_t len_idx = 0;
static uint32_t payload_len = 0;
static uint32_t payload_got = 0;

WebServer server(80);

static void commit_frame() {
  if (display_lock) {
    // Drop frame while HTTP is copying display_buf.
    payload_len = 0;
    payload_got = 0;
    state = WAIT_AA;
    return;
  }
  uint8_t *tmp = display_buf;
  display_buf = write_buf;
  write_buf = tmp;
  display_len = payload_len;
  last_frame_len = payload_len;
  frame_count++;
  payload_len = 0;
  payload_got = 0;
  state = WAIT_AA;
}

static void feed_byte(uint8_t b) {
  switch (state) {
    case WAIT_AA:
      if (b == MAGIC0) {
        state = WAIT_55;
      }
      break;
    case WAIT_55:
      if (b == MAGIC1) {
        len_idx = 0;
        state = READ_LEN;
      } else if (b == MAGIC0) {
        state = WAIT_55;
      } else {
        state = WAIT_AA;
      }
      break;
    case READ_LEN:
      len_bytes[len_idx++] = b;
      if (len_idx >= 4) {
        payload_len = (uint32_t)len_bytes[0] |
                      ((uint32_t)len_bytes[1] << 8) |
                      ((uint32_t)len_bytes[2] << 16) |
                      ((uint32_t)len_bytes[3] << 24);
        if (payload_len < 1 || payload_len > MAX_JPEG) {
          state = WAIT_AA;
          payload_len = 0;
        } else {
          payload_got = 0;
          state = READ_PAYLOAD;
        }
      }
      break;
    case READ_PAYLOAD:
      write_buf[payload_got++] = b;
      if (payload_got >= payload_len) {
        commit_frame();
      }
      break;
  }
}

static void poll_uart() {
  while (Serial2.available() > 0) {
    feed_byte((uint8_t)Serial2.read());
  }
}

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<meta http-equiv="Cache-Control" content="no-store"/>
<title>OpenMV Stream</title>
<style>
body{margin:0;background:#111;color:#ccc;font-family:sans-serif;text-align:center}
h3{margin:12px 0 4px}
img{max-width:100%;background:#000;min-height:120px}
.meta{font-size:12px;opacity:.7;margin:8px}
</style>
</head>
<body>
<h3>OpenMV + ESP32</h3>
<p class="meta">MJPEG live</p>
<img id="live" src="/stream" alt="live" decoding="async"/>
</body>
</html>
)HTML";

static void handle_root() {
  server.send_P(200, "text/html", INDEX_HTML);
}

static void handle_health() {
  char msg[128];
  snprintf(msg, sizeof(msg),
           "{\"ok\":true,\"frames\":%lu,\"last_len\":%lu,\"ip\":\"%s\"}",
           (unsigned long)frame_count,
           (unsigned long)last_frame_len,
           WiFi.softAPIP().toString().c_str());
  server.send(200, "application/json", msg);
}

static void handle_stream() {
  WiFiClient client = server.client();
  String response =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
      "Cache-Control: no-store, no-cache, must-revalidate\r\n"
      "Pragma: no-cache\r\n"
      "Connection: close\r\n\r\n";
  client.print(response);

  uint32_t sent_id = 0;
  while (client.connected()) {
    poll_uart();
    if (display_len > 0 && frame_count != sent_id) {
      sent_id = frame_count;
      display_lock = true;
      size_t len = display_len;
      const uint8_t *ptr = display_buf;
      client.print("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: ");
      client.print(len);
      client.print("\r\n\r\n");
      client.write(ptr, len);
      client.print("\r\n");
      display_lock = false;
    } else {
      delay(5);
    }
    yield();
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("OpenMV-ESP stream boot");

  Serial2.setRxBufferSize(4096);
  Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

  WiFi.mode(WIFI_AP);
  bool ap_ok = WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress ip = WiFi.softAPIP();
  Serial.printf("AP %s  pass=%s  ok=%d  ip=%s\n",
                AP_SSID, AP_PASS, ap_ok ? 1 : 0, ip.toString().c_str());

  server.on("/", HTTP_GET, handle_root);
  server.on("/stream", HTTP_GET, handle_stream);
  server.on("/health", HTTP_GET, handle_health);
  server.begin();
  Serial.println("HTTP server on :80  open http://192.168.4.1/");
}

void loop() {
  poll_uart();
  server.handleClient();
}
