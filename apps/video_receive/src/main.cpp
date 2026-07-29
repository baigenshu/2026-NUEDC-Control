/*
  video_receive — ESP32 SoftAP + HTTP MJPEG pull + ST7789 preview + SD AVI record

  Tuned for classic ESP32 (no PSRAM): small frames, chunked read, faster SPI.
  Pair with apps/video_send (MaixCAM-Pro JpegStreamer).
  Recording: AVI + MJPEG to /REC/VID_xxxx.avi (not MP4).
*/

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <TJpg_Decoder.h>
#include <esp_heap_caps.h>
#include <esp32-hal-psram.h>
#include <SD.h>
#include "avi_mjpeg.h"

#ifndef ST77XX_MADCTL
#define ST77XX_MADCTL 0x36
#endif
#ifndef ST77XX_MADCTL_MY
#define ST77XX_MADCTL_MY 0x80
#endif
#ifndef ST77XX_MADCTL_MX
#define ST77XX_MADCTL_MX 0x40
#endif
#ifndef ST77XX_MADCTL_MV
#define ST77XX_MADCTL_MV 0x20
#endif
#ifndef ST77XX_MADCTL_RGB
#define ST77XX_MADCTL_RGB 0x00
#endif

// ======================= SoftAP (must match video_send) =======================
static const char *AP_SSID = "MaixCam-ESP";
static const char *AP_PASS = "grx060313";

static const char *STREAM_HOST = "192.168.4.2";
static const uint16_t STREAM_PORT = 8000;
static const char *STREAM_PATH = "/stream";

// ======================= ST7789 pins =======================
#ifndef TFT_SCLK
#define TFT_SCLK 18
#endif
#ifndef TFT_MISO
#define TFT_MISO 19
#endif
#ifndef TFT_MOSI
#define TFT_MOSI 23
#endif
#ifndef TFT_CS
#define TFT_CS 5
#endif
#ifndef TFT_DC
#define TFT_DC 2
#endif
#ifndef TFT_RST
#define TFT_RST 4
#endif
#ifndef TFT_BL
#define TFT_BL 15
#endif

// 14-pin red TFT+SD module: SD_CS is separate; map to free ESP32 GPIO
#ifndef SD_CS
#define SD_CS 13
#endif

constexpr uint16_t TFT_WIDTH = 240;
constexpr uint16_t TFT_HEIGHT = 320;
constexpr uint32_t TFT_SPI_HZ = 40000000;
constexpr int32_t SD_SPI_HZ = 20000000;

// Auto-record when stream runs and SD is ready
#ifndef AUTO_RECORD
#define AUTO_RECORD 1
#endif
// Rotate file after this many frames (~15fps * 60s * 5 ≈ 4500)
#ifndef MAX_FRAMES_PER_FILE
#define MAX_FRAMES_PER_FILE 4500
#endif

constexpr uint8_t BL_PWM_CHANNEL = 0;
constexpr uint16_t BL_PWM_FREQ = 5000;
constexpr uint8_t BL_PWM_BITS = 8;

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

static constexpr uint8_t BASE_ROTATION = 3;
static constexpr bool MIRROR_X = false;
static constexpr bool MIRROR_Y = true;

#define MJPEG_BUFFER_SIZE (20 * 1024)
#define CHUNK_SIZE 512

static constexpr uint8_t JPG_SCALE = 1;
static constexpr int16_t STREAM_W = 160;
static constexpr int16_t STREAM_H = 120;
static constexpr uint16_t REC_FPS_HINT = 15;

HTTPClient http;
WiFiClient *stream = nullptr;
static char mjpegUrl[96];
static char modeUrl[96];
static bool urlReady = false;
static unsigned long lastStationWaitMs = 0;

static uint8_t chunkBuf[CHUNK_SIZE];
static uint8_t *frameBuffer = nullptr;
static int frameBufferSize = 0;
static int frameDataLen = 0;
static bool inFrame = false;
static bool pendingFfd = false;

static bool sdReady = false;
static AviMjpegWriter avi;
static bool wantRecord = (AUTO_RECORD != 0);
static unsigned long lastRecStatusMs = 0;

// Maix 页面 data-mode=web|tft；web 时本机不拉流（只当 SoftAP）
enum class PullMode : uint8_t
{
  Unknown = 0,
  Tft,
  Web
};
static PullMode pullMode = PullMode::Unknown;
static unsigned long lastModeCheckMs = 0;
static const unsigned long MODE_CHECK_MS = 4000;

static void tftCsIdle()
{
  digitalWrite(TFT_CS, HIGH);
}

static void sdCsIdle()
{
  digitalWrite(SD_CS, HIGH);
}

void setRotationWithMirror(uint8_t rot, bool mirrorX, bool mirrorY)
{
  tft.setRotation(rot);
  uint8_t madctl = 0;
  switch (rot & 3)
  {
  case 0:
    madctl = ST77XX_MADCTL_MX | ST77XX_MADCTL_MY | ST77XX_MADCTL_RGB;
    break;
  case 1:
    madctl = ST77XX_MADCTL_MY | ST77XX_MADCTL_MV | ST77XX_MADCTL_RGB;
    break;
  case 2:
    madctl = ST77XX_MADCTL_RGB;
    break;
  case 3:
    madctl = ST77XX_MADCTL_MX | ST77XX_MADCTL_MV | ST77XX_MADCTL_RGB;
    break;
  }
  if (mirrorX)
    madctl ^= ST77XX_MADCTL_MX;
  if (mirrorY)
    madctl ^= ST77XX_MADCTL_MY;
  tft.sendCommand(ST77XX_MADCTL, &madctl, 1);
}

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
  const int16_t ox = (tft.width() - STREAM_W) / 2;
  const int16_t oy = (tft.height() - STREAM_H) / 2;
  const int16_t dx = (int16_t)(ox + x);
  const int16_t dy = (int16_t)(oy + y);

  if (dy >= tft.height() || dx >= tft.width())
    return 0;
  if (dx + (int16_t)w <= 0 || dy + (int16_t)h <= 0)
    return 0;

  tft.drawRGBBitmap(dx, dy, bitmap, w, h);
  return 1;
}

void buildStreamUrl()
{
  snprintf(mjpegUrl, sizeof(mjpegUrl), "http://%s:%u%s", STREAM_HOST, STREAM_PORT, STREAM_PATH);
  snprintf(modeUrl, sizeof(modeUrl), "http://%s:%u/", STREAM_HOST, STREAM_PORT);
  urlReady = true;
  Serial.printf("Stream URL: %s\n", mjpegUrl);
  Serial.printf("Mode URL: %s\n", modeUrl);
}

// 读 Maix 首页 HTML，识别 data-mode="web|tft"
PullMode detectMaixPullMode()
{
  HTTPClient probe;
  probe.setTimeout(1500);
  probe.setConnectTimeout(1500);
  if (!probe.begin(modeUrl))
  {
    Serial.println("mode probe begin fail");
    return PullMode::Unknown;
  }

  int code = probe.GET();
  if (code != HTTP_CODE_OK)
  {
    Serial.printf("mode probe HTTP %d\n", code);
    probe.end();
    return PullMode::Unknown;
  }

  // HTML 很小，整页读入
  String body = probe.getString();
  probe.end();

  if (body.indexOf("data-mode=\"web\"") >= 0 || body.indexOf("data-mode='web'") >= 0)
  {
    Serial.println("Maix mode: WEB -> ESP will NOT pull (AP only)");
    return PullMode::Web;
  }
  if (body.indexOf("data-mode=\"tft\"") >= 0 || body.indexOf("data-mode='tft'") >= 0)
  {
    Serial.println("Maix mode: TFT -> ESP pulls stream");
    return PullMode::Tft;
  }

  // 旧页面无标记时默认拉流（兼容）
  Serial.println("Maix mode: unknown, default TFT pull");
  return PullMode::Tft;
}

void startSoftAP()
{
  Serial.printf("Starting SoftAP: %s\n", AP_SSID);
  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 4);
  if (!ok)
  {
    Serial.println("SoftAP failed!");
    return;
  }
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}

void showStatus(const char *line1, const char *line2 = nullptr, const char *line3 = nullptr)
{
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("video_receive");
  tft.setTextSize(1);
  tft.setCursor(10, 50);
  tft.println(line1);
  if (line2)
  {
    tft.setCursor(10, 70);
    tft.println(line2);
  }
  if (line3)
  {
    tft.setCursor(10, 90);
    tft.println(line3);
  }
}

bool waitForStation()
{
  int n = WiFi.softAPgetStationNum();
  if (n > 0)
    return true;

  unsigned long now = millis();
  if (now - lastStationWaitMs > 2000)
  {
    lastStationWaitMs = now;
    Serial.println("Waiting for MaixCAM to join SoftAP...");
    char buf[32];
    snprintf(buf, sizeof(buf), "AP: %s", AP_SSID);
    showStatus("Waiting STA...", buf, WiFi.softAPIP().toString().c_str());
  }
  return false;
}

void closeStream()
{
  stream = nullptr;
  http.end();
  inFrame = false;
  frameDataLen = 0;
  pendingFfd = false;
}

bool ensureFrameBuffer()
{
  if (frameBuffer)
    return true;
  frameBufferSize = MJPEG_BUFFER_SIZE;
  frameBuffer = (uint8_t *)heap_caps_malloc(frameBufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!frameBuffer)
    frameBuffer = (uint8_t *)malloc(frameBufferSize);
  if (!frameBuffer)
  {
    Serial.println("frame buffer alloc failed");
    return false;
  }
  Serial.printf("frame buffer %d bytes\n", frameBufferSize);
  return true;
}

static void ensureRecording()
{
  if (!wantRecord || !sdReady)
    return;
  if (avi.isOpen())
    return;

  tftCsIdle();
  sdCsIdle();
  if (!openNextRecording(avi, STREAM_W, STREAM_H, REC_FPS_HINT))
  {
    Serial.println("open recording failed");
  }
  sdCsIdle();
}

static void stopRecording(const char *why)
{
  if (!avi.isOpen())
    return;
  tftCsIdle();
  avi.end();
  sdCsIdle();
  Serial.printf("recording stopped (%s)\n", why ? why : "");
}

static void enterWebApOnlyUi()
{
  stopRecording("web mode");
  closeStream();
  showStatus("AP only", "Web mode", "phone :8000");
}

static void maybeRotateFile()
{
  if (!avi.isOpen())
    return;
  if (avi.frameCount() < (uint32_t)MAX_FRAMES_PER_FILE)
    return;
  stopRecording("rotate");
  ensureRecording();
}

void onCompleteJpegFrame()
{
  static unsigned long frameCount = 0;
  static unsigned long fpsTimer = 0;
  static unsigned long dropCount = 0;
  static unsigned long recDrop = 0;

  // Prefer latest frame for display if network backlog is large
  bool skipHeavy = (stream && stream->available() > 4096);
  if (skipHeavy)
  {
    dropCount++;
    // still try to record this complete frame (cheap vs decode)
  }

  // --- record first (JPEG already complete) ---
  if (wantRecord && sdReady)
  {
    ensureRecording();
    if (avi.isOpen())
    {
      tftCsIdle();
      if (!avi.writeFrame(frameBuffer, (uint32_t)frameDataLen))
        recDrop++;
      sdCsIdle();
      maybeRotateFile();
    }
  }

  // --- preview ---
  if (!skipHeavy)
  {
    unsigned long currentTime = millis();
    if (TJpgDec.drawJpg(0, 0, frameBuffer, frameDataLen) == 0)
    {
      frameCount++;
      if (currentTime - fpsTimer >= 1000)
      {
        float fps = frameCount * 1000.0f / (currentTime - fpsTimer);
        Serial.printf("FPS: %.1f frame:%dB drop:%lu recDrop:%lu recF:%u STA:%d heap:%u\n",
                      fps, frameDataLen, dropCount, recDrop,
                      (unsigned)avi.frameCount(),
                      WiFi.softAPgetStationNum(), ESP.getFreeHeap());
        frameCount = 0;
        dropCount = 0;
        recDrop = 0;
        fpsTimer = currentTime;
      }

      // small REC indicator top-left (landscape)
      if (avi.isOpen() && (millis() - lastRecStatusMs > 500))
      {
        lastRecStatusMs = millis();
        tft.fillRect(2, 2, 48, 12, ST77XX_RED);
        tft.setTextColor(ST77XX_WHITE);
        tft.setTextSize(1);
        tft.setCursor(4, 4);
        tft.print("REC");
      }
    }
  }

  frameDataLen = 0;
  inFrame = false;
  pendingFfd = false;
}

void feedBytes(const uint8_t *data, int len)
{
  for (int i = 0; i < len; i++)
  {
    uint8_t b = data[i];

    if (!inFrame)
    {
      if (pendingFfd)
      {
        pendingFfd = false;
        if (b == 0xD8)
        {
          if (!ensureFrameBuffer())
            return;
          frameBuffer[0] = 0xFF;
          frameBuffer[1] = 0xD8;
          frameDataLen = 2;
          inFrame = true;
        }
        else if (b == 0xFF)
        {
          pendingFfd = true;
        }
      }
      else if (b == 0xFF)
      {
        pendingFfd = true;
      }
      continue;
    }

    if (frameDataLen >= frameBufferSize - 1)
    {
      Serial.println("frame overflow, drop");
      frameDataLen = 0;
      inFrame = false;
      pendingFfd = false;
      continue;
    }

    frameBuffer[frameDataLen++] = b;
    if (frameDataLen >= 2 &&
        frameBuffer[frameDataLen - 2] == 0xFF &&
        frameBuffer[frameDataLen - 1] == 0xD9)
    {
      onCompleteJpegFrame();
    }
  }
}

void playMjpegStream()
{
  if (!urlReady)
    buildStreamUrl();

  // 定期探测 Maix 模式：Web → 不拉流；TFT → 拉流
  unsigned long now = millis();
  if (pullMode == PullMode::Unknown || (now - lastModeCheckMs) >= MODE_CHECK_MS)
  {
    lastModeCheckMs = now;
    PullMode m = detectMaixPullMode();
    if (m != PullMode::Unknown && m != pullMode)
    {
      PullMode prev = pullMode;
      pullMode = m;
      if (pullMode == PullMode::Web)
      {
        enterWebApOnlyUi();
      }
      else if (pullMode == PullMode::Tft && prev == PullMode::Web)
      {
        Serial.println("switch to TFT pull");
        showStatus("TFT mode", "pulling stream", mjpegUrl);
      }
    }
    else if (m != PullMode::Unknown)
    {
      pullMode = m;
    }
  }

  if (pullMode == PullMode::Web)
  {
    // 纯热点：给手机/Maix 省带宽，本机不上屏不录
    if (stream)
      closeStream();
    if (avi.isOpen())
      stopRecording("web mode");
    delay(200);
    return;
  }

  if (pullMode == PullMode::Unknown)
  {
    // 尚无页面：慢重试，避免狂打
    delay(300);
    return;
  }

  // ----- TFT 模式：拉流 + 预览 + 可选录制 -----
  if (!stream || !stream->connected())
  {
    stopRecording("stream reconnect");

    Serial.printf("Connecting MJPEG: %s\n", mjpegUrl);
    closeStream();

    if (!http.begin(mjpegUrl))
    {
      Serial.println("http.begin failed");
      delay(500);
      return;
    }

    http.setTimeout(3000);
    http.setConnectTimeout(3000);
    http.setReuse(true);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK)
    {
      stream = http.getStreamPtr();
      if (stream)
        stream->setTimeout(50);
      Serial.println("MJPEG connected");
      ensureRecording();
    }
    else
    {
      Serial.printf("HTTP error: %d\n", httpCode);
      closeStream();
      delay(500);
      return;
    }
  }

  if (!(stream && stream->available()))
    return;

  int loops = 0;
  while (stream->available() > 0 && loops < 16)
  {
    loops++;
    int n = stream->readBytes(chunkBuf, min((int)CHUNK_SIZE, stream->available()));
    if (n <= 0)
      break;
    feedBytes(chunkBuf, n);
  }
}

void setup()
{
  setCpuFrequencyMhz(240);
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== video_receive (MJPEG + SD AVI) ===");
  Serial.printf("CPU: %d MHz\n", getCpuFrequencyMhz());

  if (psramFound())
    Serial.printf("PSRAM: %d bytes\n", ESP.getPsramSize());
  else
    Serial.println("No PSRAM — 160x120 stream recommended");

  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  ledcSetup(BL_PWM_CHANNEL, BL_PWM_FREQ, BL_PWM_BITS);
  ledcAttachPin(TFT_BL, BL_PWM_CHANNEL);
  ledcWrite(BL_PWM_CHANNEL, 255);

  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI);
  SPI.setFrequency(TFT_SPI_HZ);
  tft.init(TFT_WIDTH, TFT_HEIGHT);
  tft.setSPISpeed(TFT_SPI_HZ);
  tft.invertDisplay(false);
  setRotationWithMirror(BASE_ROTATION, MIRROR_X, MIRROR_Y);
  tft.fillScreen(ST77XX_BLACK);
  tftCsIdle();

  // SD on shared SPI
  sdReady = sdInitSharedSpi(SD_CS, SD_SPI_HZ);
  sdCsIdle();
  if (sdReady)
    Serial.printf("SD_CS=%d AUTO_RECORD=%d\n", SD_CS, AUTO_RECORD);
  else
    Serial.println("SD not ready — preview only");

  startSoftAP();
  buildStreamUrl();

  char line2[40];
  char line3[40];
  snprintf(line2, sizeof(line2), "AP %s", WiFi.softAPIP().toString().c_str());
  if (sdReady)
    snprintf(line3, sizeof(line3), "SD OK REC->/REC");
  else
    snprintf(line3, sizeof(line3), "SD FAIL CS=%d", SD_CS);
  showStatus(AP_SSID, line2, line3);

  TJpgDec.setJpgScale(JPG_SCALE);
  TJpgDec.setSwapBytes(false);
  TJpgDec.setCallback(tft_output);

  ensureFrameBuffer();
  Serial.println("Ready — SoftAP on; pull only if Maix data-mode=tft");
  Serial.println("Web mode: AP only (no TFT pull) for smoother phone view");
}

void loop()
{
  if (!waitForStation())
  {
    delay(100);
    yield();
    return;
  }

  playMjpegStream();
  yield();
}
