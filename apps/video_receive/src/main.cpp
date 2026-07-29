/*
  video_receive — ESP32 SoftAP only (TFT / pull / SD temporarily disabled)

  Provides WiFi hotspot MaixCam-ESP for MaixCAM + phone.
  Screen preview, stream pull, and SD record are compiled out (ENABLE_* = 0).
  Re-enable later by setting macros to 1.
*/

#include <Arduino.h>
#include <WiFi.h>

// ======================= Feature switches (screen plan deprecated) =======================
#ifndef ENABLE_TFT_PREVIEW
#define ENABLE_TFT_PREVIEW 0
#endif
#ifndef ENABLE_SD_RECORD
#define ENABLE_SD_RECORD 0
#endif
#ifndef ENABLE_STREAM_PULL
#define ENABLE_STREAM_PULL 0
#endif

// ======================= SoftAP (must match video_send) =======================
static const char *AP_SSID = "MaixCam-ESP";
static const char *AP_PASS = "grx060313";

static unsigned long lastLogMs = 0;

void startSoftAP()
{
  Serial.printf("Starting SoftAP: %s\n", AP_SSID);
  WiFi.mode(WIFI_AP);
  // channel 1, max 4 stations
  bool ok = WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 4);
  if (!ok)
  {
    Serial.println("SoftAP failed!");
    return;
  }
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("Mode: hotspot only (TFT/pull/SD disabled)");
}

void setup()
{
  setCpuFrequencyMhz(240);
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== video_receive (SoftAP only) ===");
  Serial.printf("CPU: %d MHz\n", getCpuFrequencyMhz());
  Serial.printf("ENABLE_TFT_PREVIEW=%d ENABLE_STREAM_PULL=%d ENABLE_SD_RECORD=%d\n",
                ENABLE_TFT_PREVIEW, ENABLE_STREAM_PULL, ENABLE_SD_RECORD);

#if ENABLE_TFT_PREVIEW || ENABLE_STREAM_PULL || ENABLE_SD_RECORD
  // Legacy full stack lived here; re-enable macros and restore from git if needed.
  Serial.println("WARNING: feature macros on but legacy code stripped — set all to 0 or restore full main");
#endif

  startSoftAP();
  Serial.println("Ready — Maix/phone join MaixCam-ESP; open http://<maix-ip>:8000 on phone");
}

void loop()
{
  unsigned long now = millis();
  if (now - lastLogMs >= 5000)
  {
    lastLogMs = now;
    Serial.printf("AP up, STA=%d, IP=%s\n",
                  WiFi.softAPgetStationNum(),
                  WiFi.softAPIP().toString().c_str());
  }
  delay(200);
  yield();
}
