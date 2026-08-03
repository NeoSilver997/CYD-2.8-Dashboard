# Stage 0 — Hardware verification results (ESP32-2432S028R, 2.8")

The board this project now targets: **2.8" 240×320 ILI9341 with resistive
touch, micro-USB *and* USB-C**. Fill this in as each test passes — the recorded
values become constants in the application code (plan §3.6).

> **GATE: all rows below must be complete and passing before the application is
> rewritten for this board.** Touch is *in scope* this time (the 2.4" unit's
> touch controller was faulty — see `stage0_results_2432S024R.md`), so the
> touch rows are part of the gate, not optional.

**Status: 0.1 display sorted (inversion + rotation fixed, needs one confirming
reflash). Touch and network tests not run yet.**

| # | Item | Value | Test | Status |
|---|---|---|---|---|
| 1 | Driver define | `ILI9341_2_DRIVER` — bars rendered, geometry correct | 0.1 | ✅ |
| 1 | Inversion | `TFT_INVERSION_ON` — panel showed exact complements without it | 0.1 | ✅ |
| 1 | RGB order | `TFT_BGR` — complements were straight, no red/blue swap | 0.1 | ✅ |
| 1 | Backlight | GPIO 21, driven HIGH from code — screen lit | 0.1 | ✅ |
| 1 | Rotation | `1` (320×240; `3` was upside-down on this unit) | 0.1 | 🔄 reflash to confirm |
| 1 | SPI frequency | `55000000` (stable); `80000000` untried | 0.1 | ✅ |
| 2 | Touch bus | VSPI, polling mode — works | 0.2 | ✅ |
| 2 | Touch controller alive | PENIRQ = 0 on every press, data line returns real values | 0.2 | ✅ |
| 2 | Touch raw X min / max | `1009` / `3503` (partial sweep — 0.2e sets the real edge values) | 0.2 | ✅ |
| 2 | Touch raw Y min / max | `520` / `3101` (partial sweep) | 0.2 | ✅ |
| 2 | Touch pressure (z) | ~1600–2700 on a deliberate press, Zmax `2703` | 0.2 | ✅ |
| 2e | Crosshair accuracy / button hits | works — calibration, tracking and hit-testing all good | 0.2e | ✅ |
| 2e | Both buses under load | display (HSPI) + touch (VSPI) together, no interference | 0.2e | ✅ |
| 2e | `TOUCH_SWAP_XY` | **⚠️ not captured — needed for the rewrite** | 0.2e | ⬜ |
| 2e | `TOUCH_RAW_X_MIN` / `_MAX` | **⚠️ not captured** | 0.2e | ⬜ |
| 2e | `TOUCH_RAW_Y_MIN` / `_MAX` | **⚠️ not captured** | 0.2e | ⬜ |
| 2e | `TOUCH_Z_THRESHOLD` | **⚠️ not captured** | 0.2e | ⬜ |
| 3 | TZ string | `PST8PDT,M3.2.0,M11.1.0` — PDT confirmed correct | 0.3 | ✅ |
| 3 | RSSI at final location | | 0.3 | ⬜ |
| 4 | Free heap / largest block | 254400 free / 110580 largest (~248 / 108 KB), min free 251396 | 0.4 | ✅ |
| 5 | HTTPS fetch works | HTTP 200, correct location, plausible values | 0.5 | ✅ |
| 5 | Heap drift over 60 fetches | −220 B after 2 fetches; soak not yet complete | 0.5 | 🔄 running |

## How to run

```bash
cp config/User_Setup_2432S028R.h.template ~/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h
./tools/build_all.sh
./tools/flash.sh stage0/s01_display_test
```

Then in order: `s02_touch_test`, `s02e_touch_calibrate`,
`s03_wifi_time_heap_test`, `s04_https_fetch_test`. A failure at 0.1 makes every
later result meaningless (plan §3) — fix it before moving on.

## Notes / observations

_(fill in as you go — what you saw, what you changed, what finally worked)_

- **0.1 Display:** First flash lit the screen and drew all three bars in the
  right places, but in the exact complement of every colour — the RED bar came
  up cyan, GREEN came up magenta/purple, BLUE came up yellow. A clean
  complement across all three (rather than red and blue trading places) means
  inversion, not RGB order, so `TFT_BGR` stays and `TFT_INVERSION_ON` was
  added. Worth noting: this is the *opposite* of what the 2.8" CYD is usually
  documented to need, and it now matches the 2.4" board. Panel lots vary —
  trust the test.
  Orientation at rotation `3` was upside-down for how this unit is mounted, so
  `CYD_ROTATION` is `1` for this board (same 320×240 view, 180° over). Both
  changes are in `config/board.h` and
  `config/User_Setup_2432S028R.h.template`; reflash s01 to confirm bars read
  RED · GREEN · BLUE left-to-right with "TOP LEFT" genuinely upper-left.
- **0.2 Touch (raw): PASS — the panel is fully functional on this board.**
  Every press printed a reading, values moved smoothly and repeatably while
  dragging, and z sat around 1600–2700 on a deliberate press (Zmax 2703) —
  far above the idle floor. PENIRQ read 0 throughout every press, so on this
  unit *both* halves of the controller work: the interrupt line and the SPI
  data line. That is the direct contrast with the 2.4" board, where PENIRQ
  toggled but DOUT never returned a bit (see the archived results).
  Ranges seen were X `1009…3503`, Y `520…3101`. These are from a freehand
  sweep, not the extreme corners, so they are **not** the calibration
  constants — 0.2e presses known corner targets and extrapolates to the true
  screen edges. Use its numbers, not these.
- **0.2e Touch (calibration + hit test): PASS on behaviour.** All four corner
  targets accepted a press, the crosshair tracked, the buttons hit-tested
  correctly, and nothing glitched with the display on HSPI and touch on VSPI
  running together. **But the printed `TOUCH_*` constants were not captured**,
  and they are the whole output of this test — the application needs them to
  turn a raw reading into a pixel. Rerun and copy the block from serial before
  the rewrite starts. (It prints immediately after the fourth target, and
  RECAL in the corner reruns it without reflashing.)
- **0.3 WiFi & time: PASS.** Connected, and NTP gave
  `Wednesday 22 July 2026 20:31:49 PDT` — correct local wall-clock time with
  DST applied, so `PST8PDT,M3.2.0,M11.1.0` carries over from the 2.4" board
  unchanged. RSSI at the final mounting location still to be recorded (leave
  the sketch running where the clock will live; it prints RSSI once a minute).
- **0.4 Heap: PASS.** Free 254400 (~248 KB), min free 251396, largest
  contiguous block 110580 (~108 KB) with WiFi + NTP up. Comfortably above the
  150 KB gate, and the largest block — the number that actually limits sprite
  allocation — is ~7× a per-digit clock sprite. Essentially identical to the
  2.4" board, as expected: same ESP32, same core.
- **0.5 Fetch soak: fetch path PASS, soak still running.** HTTP 200 with a
  631-byte payload; the response echoed back the configured coordinates and
  resolved timezone, so the values in `config.h` are right. Sample:
  25.0 °C, feels 25.1, weather code 3 (overcast), cloud 89 %, RH 46 %, wind
  4.9 km/h, 1006.0 hPa. Heap −840 B on the call and −220 B against the first
  fetch — TLS cache settling, not a leak; the shape to watch for is a steady
  downward creep across all 60. Note the JSON filter keeps RAM tiny by
  discarding most of that payload, and parsing is done from a buffered
  `getString()` rather than the stream (chunked encoding makes stream parsing
  return an all-zeros doc — the trap found on the 2.4" board).

## What is expected to differ from the 2.4" board

| | 2.4" ESP32-2432S024R | 2.8" ESP32-2432S028R |
|---|---|---|
| Backlight pin | GPIO 27 | **GPIO 21** |
| Colour inversion | `TFT_INVERSION_ON` | `TFT_INVERSION_ON` (measured — same, despite the usual advice) |
| Rotation | `3` | **`1`** |
| USB | micro-USB only | micro-USB **+ USB-C** (same CH340 — use one at a time) |
| Touch | controller data line dead (hardware fault) | **expected working** |

Everything else — TFT pins, touch pins, SD, LDR, RGB LED, speaker — is the same
on both boards, which is why `config/board.h` covers them with one profile.

## Part number

Check the PCB silkscreen and record it here (the 2.4" unit read `ESP32-024`;
this one should read something like `ESP32-2432S028`):
