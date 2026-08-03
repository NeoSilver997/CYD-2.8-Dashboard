#include "touch.h"
#include "board.h"
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>

static SPIClass            touchSPI(VSPI);              // display keeps HSPI
static XPT2046_Touchscreen ts(CYD_TOUCH_CS_PIN);        // polling mode
static Preferences         prefs;

static const char* NVS_NS = "cydtouch";

// Fallback mapping, used only if NVS is empty AND the wizard is skipped. These
// are rough full-panel values, not measured -- deliberately wide so a stray
// touch can't land on nothing, but they will be a few tens of pixels out. The
// wizard replaces them on first boot.
static TouchCal cal = {
  /*xAt0*/ 3900, /*xAtW*/ 200,
  /*yAt0*/ 200,  /*yAtH*/ 3900,
  /*swapXY*/ true,
  /*zThreshold*/ 400,
  /*valid*/ false
};

// ---- gesture state --------------------------------------------------------
static bool     isDown     = false;
static uint32_t downAt     = 0;
static uint32_t lastSeenMs = 0;   // last sample that read as pressed
static uint32_t lastUpAt   = 0;
static int      lastSx = 0, lastSy = 0;

// A resistive panel drops the odd sample mid-press. Requiring a quiet gap
// before declaring release stops one dropped read from splitting a single
// press into two gestures.
static const uint32_t RELEASE_GRACE_MS = 60;
// Anything shorter than this is electrical noise, not a finger.
static const uint32_t MIN_PRESS_MS = 40;

void touch_begin() {
  pinMode(CYD_TOUCH_IRQ_PIN, INPUT);          // watched for diagnostics only
  touchSPI.begin(CYD_TOUCH_CLK_PIN, CYD_TOUCH_MISO_PIN,
                 CYD_TOUCH_MOSI_PIN, CYD_TOUCH_CS_PIN);
  ts.begin(touchSPI);
  ts.setRotation(0);                          // raw; we map it ourselves

  if (touch_hasStoredCalibration()) {
    Serial.printf("touch: calibration loaded from NVS  x[%ld..%ld] y[%ld..%ld] swap=%d z>=%d\n",
                  cal.xAt0, cal.xAtW, cal.yAt0, cal.yAtH, cal.swapXY, cal.zThreshold);
  } else {
    Serial.println("touch: no stored calibration -- the wizard will run");
  }
}

bool touch_readRaw(int& rx, int& ry, int& z) {
  TS_Point p = ts.getPoint();
  rx = p.x; ry = p.y; z = p.z;
  return p.z >= cal.zThreshold;
}

int touch_measureNoiseFloor() {
  int worst = 0;
  for (int i = 0; i < 60; i++) {
    TS_Point p = ts.getPoint();
    if (p.z > worst) worst = p.z;
    delay(5);
  }
  return worst;
}

void touch_screenPoint(int rx, int ry, int& sx, int& sy) {
  long a = cal.swapXY ? ry : rx;              // raw axis driving screen X
  long b = cal.swapXY ? rx : ry;              // raw axis driving screen Y
  sx = constrain((int)map(a, cal.xAt0, cal.xAtW, 0, CYD_SCREEN_W - 1), 0, CYD_SCREEN_W - 1);
  sy = constrain((int)map(b, cal.yAt0, cal.yAtH, 0, CYD_SCREEN_H - 1), 0, CYD_SCREEN_H - 1);
}

bool touch_isDown() { return isDown; }

uint32_t touch_heldMs() { return isDown ? (millis() - downAt) : 0; }

TouchEvent touch_poll(int* x, int* y) {
  const uint32_t now = millis();

  // Note: ts.touched() is deliberately NOT consulted. It gates on the library's
  // own hard-coded z threshold of 400, which can sit below this panel's
  // measured noise floor. Our threshold is the calibrated one, so it wins.
  int rx, ry, z;
  const bool pressed = touch_readRaw(rx, ry, z);

  if (pressed) {
    lastSeenMs = now;
    if (!isDown) {
      // Ignore a new press that arrives inside the debounce window after the
      // last one -- resistive panels bounce, and a bounced "tap" would skip a
      // scene the user never asked to skip.
      if (now - lastUpAt < TOUCH_DEBOUNCE_MS) return TOUCH_NONE;
      isDown = true;
      downAt = now;
    }
    touch_screenPoint(rx, ry, lastSx, lastSy);   // track the latest position
    return TOUCH_NONE;
  }

  if (isDown && (now - lastSeenMs) >= RELEASE_GRACE_MS) {
    isDown   = false;
    lastUpAt = now;
    const uint32_t held = lastSeenMs - downAt;   // exclude the grace period

    if (held < MIN_PRESS_MS) return TOUCH_NONE;
    if (x) *x = lastSx;
    if (y) *y = lastSy;

    if (held >= TOUCH_RECAL_MS) {
      Serial.printf("touch: RECALIBRATE (held %lu ms)\n", held);
      return TOUCH_RECALIBRATE;
    }
    if (held >= TOUCH_LONG_MS) {
      Serial.printf("touch: long press (%lu ms) at %d,%d\n", held, lastSx, lastSy);
      return TOUCH_LONG_PRESS;
    }
    Serial.printf("touch: tap (%lu ms) at %d,%d\n", held, lastSx, lastSy);
    return TOUCH_TAP;
  }

  return TOUCH_NONE;
}

// ---- calibration storage --------------------------------------------------

TouchCal touch_calibration() { return cal; }

void touch_setCalibration(const TouchCal& c, bool persist) {
  cal = c;
  cal.valid = true;
  if (!persist) return;

  prefs.begin(NVS_NS, false);
  prefs.putLong("xAt0", cal.xAt0);
  prefs.putLong("xAtW", cal.xAtW);
  prefs.putLong("yAt0", cal.yAt0);
  prefs.putLong("yAtH", cal.yAtH);
  prefs.putBool("swap", cal.swapXY);
  prefs.putInt ("z",    cal.zThreshold);
  prefs.putBool("ok",   true);
  prefs.end();
  Serial.println("touch: calibration saved to NVS");
}

bool touch_hasStoredCalibration() {
  // Opened read-write even though nothing is written: opening a namespace that
  // does not exist yet read-only logs an NVS error on every first boot.
  prefs.begin(NVS_NS, false);
  const bool ok = prefs.getBool("ok", false);
  if (ok) {
    cal.xAt0       = prefs.getLong("xAt0", cal.xAt0);
    cal.xAtW       = prefs.getLong("xAtW", cal.xAtW);
    cal.yAt0       = prefs.getLong("yAt0", cal.yAt0);
    cal.yAtH       = prefs.getLong("yAtH", cal.yAtH);
    cal.swapXY     = prefs.getBool("swap", cal.swapXY);
    cal.zThreshold = prefs.getInt ("z",    cal.zThreshold);
    cal.valid      = true;
  }
  prefs.end();
  return ok;
}

void touch_forgetCalibration() {
  prefs.begin(NVS_NS, false);
  prefs.clear();
  prefs.end();
  cal.valid = false;
  Serial.println("touch: stored calibration cleared");
}
