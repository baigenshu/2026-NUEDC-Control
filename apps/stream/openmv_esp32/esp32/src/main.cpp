#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClient.h>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// OpenMV UART: Serial2 RX=16 TX=17 <-> OpenMV P4(TX) P5(RX)
// Push model + FreeRTOS RX. A/B stabilizations for QVGA:
//   - baud 460800, MAX_JPEG 24KB
//   - sending_buf: do not overwrite buffer while WiFi is sending it
//   - clients_mux: protect web_clients / client_count

static const int UART_RX_PIN = 16;
static const int UART_TX_PIN = 17;
static const uint32_t UART_BAUD = 460800;

static const char *AP_SSID = "OpenMV-ESP";
static const char *AP_PASS = "12345678";

static const size_t MAX_JPEG = 24 * 1024;
static const uint8_t MAGIC0 = 0xAA;
static const uint8_t MAGIC1 = 0x55;
static const int MAX_CLIENTS = 4;
static const uint32_t RESYNC_IDLE_MS = 2000;

static uint8_t buf_a[MAX_JPEG];
static uint8_t buf_b[MAX_JPEG];
static uint8_t *write_buf = buf_a;
static uint8_t *display_buf = buf_b;
static size_t display_len = 0;

static volatile uint32_t frame_count = 0;
static volatile uint32_t last_frame_len = 0;
static volatile uint32_t drop_count = 0;
static volatile uint32_t resync_count = 0;
static volatile uint32_t push_fail = 0;
static volatile uint8_t *sending_buf = nullptr;

static portMUX_TYPE frame_mux = portMUX_INITIALIZER_UNLOCKED;
static QueueHandle_t frame_queue = nullptr;
static SemaphoreHandle_t clients_mux = nullptr;

static WiFiClient web_clients[MAX_CLIENTS];
static int client_count = 0;
static const char *BOUNDARY = "frame";

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
static uint32_t last_byte_ms = 0;

WebServer server(80);

static void parser_reset() {
  state = WAIT_AA;
  len_idx = 0;
  payload_len = 0;
  payload_got = 0;
}

static void commit_frame() {
  portENTER_CRITICAL(&frame_mux);
  if (sending_buf != nullptr && write_buf == (uint8_t *)sending_buf) {
    // Would overwrite the buffer currently on the wire — drop this frame.
    drop_count++;
    portEXIT_CRITICAL(&frame_mux);
    parser_reset();
    return;
  }
  uint8_t *tmp = display_buf;
  display_buf = write_buf;
  write_buf = tmp;
  display_len = payload_len;
  last_frame_len = payload_len;
  frame_count++;
  uint32_t sz = payload_len;
  portEXIT_CRITICAL(&frame_mux);
  parser_reset();

  if (frame_queue) {
    xQueueOverwrite(frame_queue, &sz);
  }
}

static void feed_byte(uint8_t b) {
  last_byte_ms = millis();
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
          resync_count++;
          parser_reset();
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

static void poll_uart_bytes() {
  int n = Serial2.available();
  while (n-- > 0) {
    feed_byte((uint8_t)Serial2.read());
  }
  uint32_t now = millis();
  if (state != WAIT_AA && (now - last_byte_ms) > RESYNC_IDLE_MS) {
    resync_count++;
    parser_reset();
  }
}

static void uart_task(void *param) {
  (void)param;
  last_byte_ms = millis();
  for (;;) {
    poll_uart_bytes();
    if (Serial2.available() == 0) {
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }
}

static void remove_client_at(int i) {
  if (i < 0 || i >= client_count) {
    return;
  }
  if (web_clients[i]) {
    web_clients[i].stop();
  }
  web_clients[i] = web_clients[client_count - 1];
  client_count--;
}

// Caller must hold clients_mux.
static void send_image_to_clients_locked(const uint8_t *image, size_t image_size) {
  if (image_size < 2 || image[0] != 0xFF || image[1] != 0xD8) {
    drop_count++;
    return;
  }

  char header[128];
  int hlen = snprintf(header, sizeof(header),
                      "--%s\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n"
                      "Cache-Control: no-cache\r\nPragma: no-cache\r\n\r\n",
                      BOUNDARY, (unsigned)image_size);
  if (hlen <= 0) {
    return;
  }

  for (int i = 0; i < client_count;) {
    if (web_clients[i] && web_clients[i].connected()) {
      bool ok =
          web_clients[i].write((const uint8_t *)header, (size_t)hlen) == (size_t)hlen;
      if (ok) {
        size_t off = 0;
        while (off < image_size) {
          size_t chunk = image_size - off;
          if (chunk > 1024) {
            chunk = 1024;
          }
          size_t n = web_clients[i].write(image + off, chunk);
          if (n == 0) {
            ok = false;
            break;
          }
          off += n;
        }
      }
      if (ok) {
        ok = web_clients[i].print("\r\n");
      }
      if (ok) {
        i++;
      } else {
        push_fail++;
        remove_client_at(i);
      }
    } else {
      remove_client_at(i);
    }
  }
}

static void push_task(void *param) {
  (void)param;
  uint32_t sz = 0;
  for (;;) {
    if (xQueueReceive(frame_queue, &sz, portMAX_DELAY) != pdPASS) {
      continue;
    }

    if (clients_mux == nullptr) {
      continue;
    }
    if (xSemaphoreTake(clients_mux, pdMS_TO_TICKS(50)) != pdTRUE) {
      continue;
    }
    int ncli = client_count;
    xSemaphoreGive(clients_mux);
    if (ncli <= 0) {
      continue;
    }

    const uint8_t *ptr = nullptr;
    size_t len = 0;

    portENTER_CRITICAL(&frame_mux);
    ptr = display_buf;
    len = display_len;
    if (ptr && len > 0) {
      sending_buf = (volatile uint8_t *)ptr;
    }
    portEXIT_CRITICAL(&frame_mux);

    if (!ptr || len == 0) {
      continue;
    }

    if (xSemaphoreTake(clients_mux, pdMS_TO_TICKS(200)) == pdTRUE) {
      send_image_to_clients_locked(ptr, len);
      xSemaphoreGive(clients_mux);
    }

    portENTER_CRITICAL(&frame_mux);
    sending_buf = nullptr;
    portEXIT_CRITICAL(&frame_mux);
  }
}

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no"/>
<meta http-equiv="Cache-Control" content="no-store"/>
<title>OpenMV Stream</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#0b0f14;color:#e8eef5;font-family:system-ui,-apple-system,sans-serif;
     height:100vh;display:flex;flex-direction:column;overflow:hidden}
header{flex:0 0 auto;padding:10px 12px;background:#121821;border-bottom:1px solid #1e2a38}
header h1{font-size:1rem;font-weight:600}
header p{font-size:.72rem;color:#8fa3b8;margin-top:3px}
.video-pane{flex:0 0 50vh;min-height:200px;padding:8px 10px;display:flex;align-items:center;justify-content:center;
            background:#000;border-bottom:1px solid #1e2a38;position:relative}
#live{max-width:100%;max-height:100%;object-fit:contain;border-radius:6px}
#cv{position:absolute;left:-9999px;top:0;width:1px;height:1px}
.panel{flex:1 1 auto;overflow:auto;padding:12px;max-width:720px;width:100%;margin:0 auto}
.row{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:10px}
button{flex:1 1 130px;padding:14px 10px;border:0;border-radius:8px;font-size:.95rem;font-weight:600;color:#fff}
#btnStart{background:#1a7a45}
#btnStop{background:#a33a3a}
button:disabled{opacity:.45}
.status{font-size:.82rem;color:#9ab;margin-bottom:8px;min-height:1.3em}
.hint{font-size:.72rem;color:#667;line-height:1.45}
</style>
</head>
<body>
<header>
  <h1>OpenMV 图传</h1>
  <p>上半：QVGA 实时画面 · 下半：录制到手机</p>
</header>
<div class="video-pane">
  <img id="live" src="/stream" alt="live" decoding="async"/>
  <canvas id="cv"></canvas>
</div>
<div class="panel">
  <div class="row">
    <button id="btnStart" type="button">开始录制</button>
    <button id="btnStop" type="button" disabled>结束录制</button>
  </div>
  <div class="status" id="status">空闲 — 录像保存到手机</div>
  <p class="hint">建议 Chrome。QVGA · 波特率 460800 · WiFi OpenMV-ESP / 12345678</p>
</div>
<script>
(function(){
  var img = document.getElementById('live');
  var cv = document.getElementById('cv');
  var ctx = cv.getContext('2d');
  var st = document.getElementById('status');
  var btnStart = document.getElementById('btnStart');
  var btnStop = document.getElementById('btnStop');
  var n = 0, rec = null, chunks = [], drawTimer = null, t0 = 0;

  function bump(){
    n++;
    img.src = '/stream?t=' + Date.now() + '-' + n;
  }
  img.onerror = function(){ setTimeout(bump, 1500); };

  function fitCanvas(){
    var w = img.naturalWidth || 320, h = img.naturalHeight || 240;
    if (cv.width !== w || cv.height !== h) { cv.width = w; cv.height = h; }
  }
  function drawLoop(){
    if (!rec) return;
    try {
      if (img.complete && img.naturalWidth > 0) {
        fitCanvas();
        ctx.drawImage(img, 0, 0, cv.width, cv.height);
      }
    } catch (e) {}
    drawTimer = requestAnimationFrame(drawLoop);
  }
  function setUi(recording){
    btnStart.disabled = !!recording;
    btnStop.disabled = !recording;
  }
  function pickMime(){
    var cands = ['video/webm;codecs=vp9','video/webm;codecs=vp8','video/webm','video/mp4'];
    for (var i = 0; i < cands.length; i++) {
      if (window.MediaRecorder && MediaRecorder.isTypeSupported && MediaRecorder.isTypeSupported(cands[i]))
        return cands[i];
    }
    return '';
  }
  btnStart.onclick = function(){
    if (!window.MediaRecorder) { st.textContent = '请用 Chrome'; return; }
    if (!img.complete || img.naturalWidth === 0) {
      st.textContent = '等待画面…'; bump(); return;
    }
    fitCanvas();
    ctx.drawImage(img, 0, 0, cv.width, cv.height);
    var stream;
    try { stream = cv.captureStream(15); }
    catch (e) { st.textContent = 'captureStream 失败: ' + e; return; }
    chunks = [];
    var mime = pickMime();
    try {
      rec = mime ? new MediaRecorder(stream, { mimeType: mime, videoBitsPerSecond: 1500000 })
                 : new MediaRecorder(stream);
    } catch (e) { st.textContent = 'MediaRecorder 失败: ' + e; return; }
    rec.ondataavailable = function(ev){ if (ev.data && ev.data.size > 0) chunks.push(ev.data); };
    rec.onerror = function(){ st.textContent = '录制错误'; };
    rec.onstop = function(){
      if (drawTimer) cancelAnimationFrame(drawTimer);
      drawTimer = null;
      var type = (rec && rec.mimeType) ? rec.mimeType : 'video/webm';
      var blob = new Blob(chunks, { type: type });
      chunks = []; rec = null; setUi(false);
      if (!blob.size) { st.textContent = '空闲 — 未录到数据'; return; }
      var ext = type.indexOf('mp4') >= 0 ? 'mp4' : 'webm';
      var a = document.createElement('a');
      var url = URL.createObjectURL(blob);
      a.href = url; a.download = 'openmv_' + Date.now() + '.' + ext;
      document.body.appendChild(a); a.click();
      setTimeout(function(){ URL.revokeObjectURL(url); a.remove(); }, 2000);
      st.textContent = '已保存到手机 (' + ext + ', ' + Math.round(blob.size/1024) + ' KB)';
    };
    rec.start(500); t0 = Date.now(); setUi(true); st.textContent = '录制中…'; drawLoop();
    var tick = setInterval(function(){
      if (!rec) { clearInterval(tick); return; }
      st.textContent = '录制中… ' + Math.floor((Date.now() - t0) / 1000) + 's';
    }, 500);
  };
  btnStop.onclick = function(){
    if (rec && rec.state !== 'inactive') {
      try { rec.stop(); } catch (e) { st.textContent = '停止失败: ' + e; setUi(false); }
    }
  };
})();
</script>
</body>
</html>
)HTML";

static void handle_root() {
  server.send_P(200, "text/html", INDEX_HTML);
}

static void handle_health() {
  char msg[220];
  int ncli = 0;
  if (clients_mux && xSemaphoreTake(clients_mux, pdMS_TO_TICKS(20)) == pdTRUE) {
    ncli = client_count;
    xSemaphoreGive(clients_mux);
  }
  snprintf(msg, sizeof(msg),
           "{\"ok\":true,\"frames\":%lu,\"last_len\":%lu,\"drops\":%lu,"
           "\"resync\":%lu,\"push_fail\":%lu,\"clients\":%d,\"baud\":%lu,\"ip\":\"%s\"}",
           (unsigned long)frame_count, (unsigned long)last_frame_len,
           (unsigned long)drop_count, (unsigned long)resync_count,
           (unsigned long)push_fail, ncli, (unsigned long)UART_BAUD,
           WiFi.softAPIP().toString().c_str());
  server.send(200, "application/json", msg);
}

static void handle_stream() {
  if (clients_mux == nullptr) {
    server.send(500, "text/plain", "no mux");
    return;
  }
  if (xSemaphoreTake(clients_mux, pdMS_TO_TICKS(200)) != pdTRUE) {
    server.send(503, "text/plain", "busy");
    return;
  }
  if (client_count >= MAX_CLIENTS) {
    xSemaphoreGive(clients_mux);
    server.send(503, "text/plain", "too many clients");
    return;
  }

  WiFiClient client = server.client();
  client.setNoDelay(true);
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  client.println("Cache-Control: no-store, no-cache, must-revalidate");
  client.println("Pragma: no-cache");
  client.println("Access-Control-Allow-Origin: *");
  client.println("Connection: close");
  client.println();

  web_clients[client_count] = client;
  client_count++;
  int n = client_count;
  xSemaphoreGive(clients_mux);
  Serial.printf("stream client +1 -> %d\n", n);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("OpenMV-ESP QVGA A+B (460800, 24KB, send protect, client mux)");

  Serial2.setRxBufferSize(4096);
  Serial2.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);

  WiFi.mode(WIFI_AP);
  bool ap_ok = WiFi.softAP(AP_SSID, AP_PASS);
  Serial.printf("AP %s pass=%s ok=%d ip=%s\n", AP_SSID, AP_PASS, ap_ok ? 1 : 0,
                WiFi.softAPIP().toString().c_str());
  Serial.printf("UART baud=%lu RX=%d TX=%d max_jpeg=%u\n", (unsigned long)UART_BAUD,
                UART_RX_PIN, UART_TX_PIN, (unsigned)MAX_JPEG);

  clients_mux = xSemaphoreCreateMutex();
  frame_queue = xQueueCreate(1, sizeof(uint32_t));
  if (!clients_mux || !frame_queue) {
    Serial.println("mux/queue create FAILED");
  }

  server.on("/", HTTP_GET, handle_root);
  server.on("/stream", HTTP_GET, handle_stream);
  server.on("/health", HTTP_GET, handle_health);
  server.begin();
  Serial.println("HTTP :80 http://192.168.4.1/");

  BaseType_t uok =
      xTaskCreatePinnedToCore(uart_task, "uart_rx", 4096, nullptr, 2, nullptr, 0);
  BaseType_t pok =
      xTaskCreatePinnedToCore(push_task, "mjpeg_push", 6144, nullptr, 1, nullptr, 1);
  Serial.printf("uart_task=%s push_task=%s\n", uok == pdPASS ? "ok" : "FAIL",
                pok == pdPASS ? "ok" : "FAIL");
}

void loop() {
  server.handleClient();
  static uint32_t last_log = 0;
  uint32_t now = millis();
  if (now - last_log >= 2000) {
    last_log = now;
    int ncli = 0;
    if (clients_mux && xSemaphoreTake(clients_mux, pdMS_TO_TICKS(10)) == pdTRUE) {
      ncli = client_count;
      xSemaphoreGive(clients_mux);
    }
    Serial.printf("frames=%lu len=%lu drops=%lu resync=%lu fail=%lu clients=%d rx=%d\n",
                  (unsigned long)frame_count, (unsigned long)last_frame_len,
                  (unsigned long)drop_count, (unsigned long)resync_count,
                  (unsigned long)push_fail, ncli, Serial2.available());
  }
  delay(1);
}
