#include "calibrate.h"
#include "touch.h"
#include "theme.h"
#include <math.h>

static const int TARGET_INSET   = 25;    // px from each edge to a target centre
static const int SAMPLES_WANTED = 24;    // stable samples before a press counts
static const int ACCEPT_ERR_PX  = 25;    // confirmation tap must land this close
static const int MAX_ATTEMPTS   = 3;
static const uint32_t PRESS_TIMEOUT_MS = 60000;

static void drawTarget(int x, int y, uint16_t colour) {
  tft.drawCircle(x, y, 12, colour);
  tft.drawCircle(x, y, 6,  colour);
  tft.drawFastHLine(x - 16, y, 33, colour);
  tft.drawFastVLine(x, y - 16, 33, colour);
}

static void message(const char* line1, const char* line2, uint16_t colour) {
  tft.fillScreen(COL_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(colour, COL_BG);
  tft.setTextFont(4);
  tft.drawString(line1, SCREEN_W / 2, SCREEN_H / 2 - 14);
  if (line2) {
    tft.setTextColor(COL_DIM, COL_BG);
    tft.setTextFont(2);
    tft.drawString(line2, SCREEN_W / 2, SCREEN_H / 2 + 18);
  }
}

// Average a burst of samples from one steady press. Bails out if the finger
// lifts mid-burst, so a glancing tap can never produce a half-formed reading.
static bool readStablePress(long& rx, long& ry) {
  const uint32_t deadline = millis() + PRESS_TIMEOUT_MS;
  while ((int32_t)(millis() - deadline) < 0) {
    int x, y, z;
    if (touch_readRaw(x, y, z)) {
      long sx = 0, sy = 0;
      int n = 0;
      while (n < SAMPLES_WANTED && touch_readRaw(x, y, z)) {
        sx += x; sy += y; n++;
        delay(4);
      }
      if (n >= SAMPLES_WANTED) {
        rx = sx / n;
        ry = sy / n;
        return true;
      }
    }
    delay(5);
  }
  return false;
}

static void waitForRelease() {
  uint32_t quietSince = millis();
  while (millis() - quietSince < 150) {
    int x, y, z;
    if (touch_readRaw(x, y, z)) quietSince = millis();
    delay(5);
  }
}

// Press the four corners and turn them into a raw-to-pixel mapping.
static bool collectCorners(TouchCal& out) {
  const int xL = TARGET_INSET, xR = SCREEN_W - 1 - TARGET_INSET;
  const int yT = TARGET_INSET, yB = SCREEN_H - 1 - TARGET_INSET;
  const int tx[4] = {xL, xR, xR, xL};                 // TL, TR, BR, BL
  const int ty[4] = {yT, yT, yB, yB};
  const char* names[4] = {"top left", "top right", "bottom right", "bottom left"};
  long rx[4], ry[4];

  for (int i = 0; i < 4; i++) {
    tft.fillScreen(COL_BG);
    drawTarget(tx[i], ty[i], COL_ACCENT);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.setTextFont(4);
    tft.drawString(names[i], SCREEN_W / 2, SCREEN_H / 2 - 12);
    tft.setTextColor(COL_DIM, COL_BG);
    tft.setTextFont(2);
    tft.drawString("press firmly and hold", SCREEN_W / 2, SCREEN_H / 2 + 16);

    Serial.printf("calibrate: target %d/4 (%s) at %d,%d\n", i + 1, names[i], tx[i], ty[i]);
    if (!readStablePress(rx[i], ry[i])) {
      Serial.println("calibrate: timed out waiting for a press");
      return false;
    }
    drawTarget(tx[i], ty[i], COL_FRESH_OK);
    Serial.printf("calibrate:   raw %ld,%ld\n", rx[i], ry[i]);
    waitForRelease();
    delay(120);
  }

  // Which raw axis drives which screen axis? Compare how far raw X moves
  // across the screen's width against how far it moves down its height; the
  // bigger swing wins. Measuring this covers every rotation without a table of
  // special cases -- and it is not guessable, panels differ.
  const long rawXacrossWidth = labs(((rx[1] + rx[2]) / 2) - ((rx[0] + rx[3]) / 2));
  const long rawXdownHeight  = labs(((rx[2] + rx[3]) / 2) - ((rx[0] + rx[1]) / 2));
  out.swapXY = (rawXdownHeight > rawXacrossWidth);

  const long aL = out.swapXY ? (ry[0] + ry[3]) / 2 : (rx[0] + rx[3]) / 2;
  const long aR = out.swapXY ? (ry[1] + ry[2]) / 2 : (rx[1] + rx[2]) / 2;
  const long bT = out.swapXY ? (rx[0] + rx[1]) / 2 : (ry[0] + ry[1]) / 2;
  const long bB = out.swapXY ? (rx[2] + rx[3]) / 2 : (ry[2] + ry[3]) / 2;

  // Targets are inset, so extrapolate the line out to the true screen edges.
  const float slopeX = float(aR - aL) / float(xR - xL);
  const float slopeY = float(bB - bT) / float(yB - yT);
  out.xAt0 = lround(aL - slopeX * xL);
  out.xAtW = lround(aL + slopeX * ((SCREEN_W - 1) - xL));
  out.yAt0 = lround(bT - slopeY * yT);
  out.yAtH = lround(bT + slopeY * ((SCREEN_H - 1) - yT));

  // A real sweep of the panel spans thousands of counts. Anything tighter
  // means the presses landed nowhere near the targets.
  const bool sane = labs(out.xAtW - out.xAt0) > 500 && labs(out.yAtH - out.yAt0) > 500;
  if (!sane) Serial.println("calibrate: corner spread too small -- bad presses");
  return sane;
}

// One tap on a centre target, checked against the mapping we just derived.
// Nothing is saved until this passes: a wrong mapping written to NVS would be
// worse than no mapping at all, because it survives a reboot.
static bool confirmTap(int& errPx) {
  const int cx = SCREEN_W / 2, cy = SCREEN_H / 2;
  tft.fillScreen(COL_BG);
  drawTarget(cx, cy, COL_ACCENT);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextFont(2);
  tft.drawString("tap the centre to confirm", SCREEN_W / 2, SCREEN_H - 30);

  long rx, ry;
  if (!readStablePress(rx, ry)) return false;

  int sx, sy;
  touch_screenPoint(rx, ry, sx, sy);
  errPx = (int)lround(sqrt(double((sx - cx) * (sx - cx) + (sy - cy) * (sy - cy))));
  tft.fillCircle(sx, sy, 4, (errPx <= ACCEPT_ERR_PX) ? COL_FRESH_OK : COL_FRESH_OLD);
  Serial.printf("calibrate: confirm tap landed at %d,%d (%d px off)\n", sx, sy, errPx);
  waitForRelease();
  return errPx <= ACCEPT_ERR_PX;
}

bool calibrate_run(bool persist) {
  Serial.println("calibrate: starting wizard");

  const TouchCal previous = touch_calibration();

  message("Touch setup", "press each target, firmly", COL_TEXT);
  delay(1200);

  // Set the press gate from the panel's own idle noise, measured now rather
  // than assumed -- it drifts with temperature and between panels.
  const int noise = touch_measureNoiseFloor();

  for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
    TouchCal c = previous;
    c.zThreshold = max(300, noise + 150);
    c.valid = true;

    if (!collectCorners(c)) {
      // Timeout or nonsense presses. On timeout there is nobody there, so stop
      // rather than loop forever in front of an empty room.
      message("Touch setup skipped", "using previous calibration", COL_FRESH_WARN);
      Serial.println("calibrate: aborted, keeping previous calibration");
      delay(1500);
      return false;
    }

    touch_setCalibration(c, false);          // apply, but do not save yet

    int errPx = 0;
    if (confirmTap(errPx)) {
      touch_setCalibration(c, persist);
      Serial.printf("calibrate: accepted  x[%ld..%ld] y[%ld..%ld] swap=%d z>=%d (noise %d)\n",
                    c.xAt0, c.xAtW, c.yAt0, c.yAtH, c.swapXY, c.zThreshold, noise);
      Serial.println("calibrate: equivalent constants, if you ever want them baked in:");
      Serial.printf("  TOUCH_SWAP_XY %d / X %ld..%ld / Y %ld..%ld / Z %d\n",
                    c.swapXY, c.xAt0, c.xAtW, c.yAt0, c.yAtH, c.zThreshold);
      message("Touch calibrated", nullptr, COL_FRESH_OK);
      delay(900);
      return true;
    }

    if (attempt < MAX_ATTEMPTS) {
      char buf[48];
      snprintf(buf, sizeof(buf), "off by %d px - try again", errPx);
      message("Not quite", buf, COL_FRESH_WARN);
      delay(1500);
    } else {
      // Out of attempts. Keep the last mapping anyway: approximate touch beats
      // none, and the user can retry any time with a long press.
      touch_setCalibration(c, persist);
      message("Saved (rough)", "hold 4s any time to redo", COL_FRESH_WARN);
      Serial.println("calibrate: confirmation never passed; saved the last attempt anyway");
      delay(1800);
      return true;
    }
  }
  return false;
}
