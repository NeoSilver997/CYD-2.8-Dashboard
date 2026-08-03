// ===========================================================================
// touch.h -- XPT2046 input: raw reads, calibration, and gestures (plan §6)
// ===========================================================================
// The panel lives on VSPI, its own bus, with the display on HSPI (see board.h).
// The driver is constructed with the CS pin ONLY: given an IRQ pin as well,
// XPT2046_Touchscreen reads once and then blocks every later read until a
// falling edge fires, so a missed interrupt means a permanently dead panel.
// Polling always works, and at our loop rate it costs nothing.
//
// Calibration is measured on the device and stored in NVS, not hard-coded.
// A resistive panel's raw range depends on the individual panel, so a constant
// baked into firmware is a constant that is wrong on the next unit. First boot
// runs the on-screen wizard (calibrate.h); after that the stored values load
// automatically and the wizard never appears again unless asked for.
#pragma once

#include <Arduino.h>

// Gestures, classified on RELEASE. Deciding at release means one press can only
// ever produce one event -- no "long press fired, then the tap fired too".
enum TouchEvent : uint8_t {
  TOUCH_NONE = 0,
  TOUCH_TAP,          // < 800 ms   -> next scene + freeze auto-rotation
  TOUCH_LONG_PRESS,   // 0.8 - 4 s  -> pin / unpin the current scene
  TOUCH_RECALIBRATE   // > 4 s      -> re-run the calibration wizard
};

// Gesture timing (plan §6: long press > 800 ms, debounce ~250 ms).
static const uint32_t TOUCH_LONG_MS     = 800;
static const uint32_t TOUCH_RECAL_MS    = 4000;
static const uint32_t TOUCH_DEBOUNCE_MS = 250;

// Raw-to-pixel mapping. xAt0/xAtW are the raw values at screen x = 0 and
// x = width-1; they are often in descending order, which is fine -- map()
// handles a reversed range, and that is how axis flips are represented.
struct TouchCal {
  long xAt0, xAtW;
  long yAt0, yAtH;
  bool swapXY;        // true when the raw X axis drives the SCREEN's Y axis
  int  zThreshold;    // press gate, set above the measured noise floor
  bool valid;
};

void touch_begin();

// Call every loop. Returns a gesture on release, TOUCH_NONE otherwise.
// On a gesture, *x/*y receive the screen coordinates of the press.
TouchEvent touch_poll(int* x = nullptr, int* y = nullptr);

bool touch_isDown();

// How long the current press has been held, 0 when nothing is down. Lets the
// UI preview what a release would do -- without it, a 4 s hold gives the user
// no feedback at all until they let go.
uint32_t touch_heldMs();

// Raw sample, bypassing gesture handling. Returns false when z is below the
// press threshold. Used by the calibration wizard.
bool touch_readRaw(int& rx, int& ry, int& z);

// Measure the idle noise floor and set the press threshold above it.
int  touch_measureNoiseFloor();

void touch_screenPoint(int rx, int ry, int& sx, int& sy);

TouchCal touch_calibration();
void     touch_setCalibration(const TouchCal& c, bool persist);
bool     touch_hasStoredCalibration();
void     touch_forgetCalibration();
