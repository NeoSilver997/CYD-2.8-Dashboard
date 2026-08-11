// ===========================================================================
// CYD Clock & Weather Station -- application (ESP32-2432S028R, 2.8")
// ===========================================================================
// Four scenes on a continuous rotation, with touch: tap to advance, long press
// to pin, and a 4 s hold to re-run touch calibration (plan §6).
//
// Board facts come from board.h, display settings from TFT_eSPI's User_Setup.h
// (ILI9341_2, TFT_INVERSION_ON, USE_HSPI_PORT, 55 MHz -- docs/stage0_results.md).
// Display on HSPI, touch on VSPI: separate buses, verified together in 0.2e.
//
// Touch calibration is measured on the device and kept in NVS, so it is not a
// firmware constant. See touch.h for why.

#include <WiFi.h>
#include <TFT_eSPI.h>

#include "board.h"
#include "config.h"
#include "theme.h"
#include "app_data.h"
#include "time_manager.h"
#include "touch.h"
#include "calibrate.h"
#include "status_strip.h"
#include "scenes.h"
#include "weather.h"
#include "airquality.h"
#include "sun_moon.h"
#include "bus_eta.h"

// The shared globals declared extern in the headers.
TFT_eSPI tft = TFT_eSPI();
AppData  g_data;

// Compile-time guard: the display must be on HSPI or it takes the bus the
// touch panel needs. Silent when wrong at runtime, so it is caught here.
#ifndef USE_HSPI_PORT
  #error "User_Setup.h is missing '#define USE_HSPI_PORT' -- the display would take VSPI, which touch needs. Copy config/User_Setup_2432S028R.h.template into the TFT_eSPI library."
#endif

// Centred one-line message on the content area (boot progress).
static void bootMessage(const char* msg) {
  tft.fillRect(0, 0, SCREEN_W, CONTENT_H, COL_BG);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.drawString(msg, SCREEN_W / 2, CONTENT_H / 2);
}

static void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("WiFi: connecting to \"%s\"", WIFI_SSID);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(300);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED)
    Serial.printf(" ok  IP %s  RSSI %d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  else
    Serial.println(" TIMEOUT (clock still runs; will retry in loop)");
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== CYD clock & weather station ===");
  cydPrintBanner();

  cydRgbLedOff();          // active LOW: powers up white and washes out the panel
  tft.init();
  tft.setRotation(CYD_ROTATION);
  tft.fillScreen(COL_BG);
  cydBacklightOn();        // after init, so there's no flash of power-up garbage

  // Touch before anything slow: if the panel has never been calibrated we want
  // the wizard in front of the user now, not after a 15 s network wait.
  touch_begin();
  if (!touch_hasStoredCalibration()) calibrate_run(true);

  bootMessage("Connecting WiFi...");
  connectWiFi();

  bootMessage("Syncing time...");
  timeManager_begin(TZ_STRING);
  // Give NTP a few seconds so the clock shows real time on first paint; not
  // fatal if it misses -- the clock renders "----" and fills in once synced.
  struct tm t;
  uint32_t start = millis();
  while (!timeManager_now(t) && millis() - start < 8000) delay(200);

  sunmoon_recompute();     // prints sunrise/sunset/moon to Serial for verification
  tft.fillScreen(COL_BG);
  statusStrip_init();
  sceneManager_begin();
  weather_begin();
  airquality_begin();
  busEta_begin();
  Serial.println("running.  tap = next scene | hold = pin | hold 4 s = recalibrate");
}

void loop() {
  // Input first, so a tap is acted on in the same frame it is released.
  TouchEvent ev = touch_poll();
  if (ev == TOUCH_RECALIBRATE) {
    calibrate_run(true);
    // The wizard owns the whole screen, so rebuild everything behind it.
    tft.fillScreen(COL_BG);
    statusStrip_init();
    sceneManager_begin();
  } else if (ev != TOUCH_NONE) {
    sceneManager_handleTouch(ev);
    statusStrip_tick(false);      // reflect pin/freeze immediately, not in 500 ms
  }

  sceneManager_tick();
  weather_tick();       // fetches when due (first fetch shortly after boot)
  airquality_tick();    // AQI fetch, staggered ~5 s after weather
  busEta_tick();        // bus ETA fetch every 60 s

  // Refresh the status strip twice a second (cheap; only changed bits repaint),
  // but five times faster while a finger is down -- that is when the strip is
  // previewing what releasing would do, and a 500 ms lag makes the hint useless.
  static uint32_t lastStrip = 0;
  if (millis() - lastStrip >= (touch_isDown() ? 100u : 500u)) {
    lastStrip = millis();
    statusStrip_tick(false);
  }

  // Lightweight WiFi keep-alive so the strip's bars/time stay live.
  static uint32_t lastWifiTry = 0;
  if (WiFi.status() != WL_CONNECTED && millis() - lastWifiTry >= 10000) {
    lastWifiTry = millis();
    WiFi.reconnect();
  }

  // 5 ms keeps touch sampling well inside the debounce window while leaving
  // the CPU mostly idle.
  delay(5);
}
