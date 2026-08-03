# CYD Clock & Weather Station

Firmware for a **"cheap yellow display"** ESP32 board: a wall/desk clock that
rotates continuously between clock, weather, sun/moon and air-quality scenes,
with a touch panel for advancing, pinning and setup.

**Target board: ESP32-2432S028R** — 2.8" 240×320 ILI9341, resistive touch
(XPT2046), micro-USB *and* USB-C. The earlier 2.4" ESP32-2432S024R is still
supported by the same code via `CYD_BOARD` in `config/board.h`.

See [plan.md](plan.md) for the full design and [docs/](docs) for what the
hardware actually turned out to do.

---

## Quick start

**1. Install the display config** (once, and again after any TFT_eSPI update):

```bash
cp config/User_Setup_2432S028R.h.template ~/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h
```

**2. Create your own config.** The repo ships only a placeholder template —
there is no working `config.h` until you make one. Copy it:

```bash
cp config/config.example.h config/config.h
```

Then **edit `config/config.h`** and replace every value in it. The firmware will
not connect to anything until you do. See [Configuration](#configuration) below
for what each setting means and how to find your coordinates.

> ⚠️ **Never commit `config/config.h`.** It holds your WiFi password in plain
> text. It is listed in `.gitignore`, so leave it that way — and see
> [If you already committed it](#if-you-already-committed-it) if it slipped
> through. `config.example.h` is the only config file that belongs in git.

**3. Build everything, then flash the app:**

```bash
./tools/build_all.sh
```

```bash
./tools/flash.sh app
```

First boot runs the touch calibration wizard. After that it goes straight to
the clock.

> If the upload dies right after `Changing baud rate to 921600`, drop the upload
> speed — see [Upload speed](#upload-speed) below.

> If you build from the Arduino IDE rather than the scripts, run
> `./tools/sync_shared.sh` first — see [Shared headers](#shared-headers-and-the-arduino-include-rule).

### Upload speed

The Arduino-ESP32 default upload speed is **921600**, and this board's CH340
does not reliably survive the switch to it. The failure is distinctive — the
handshake succeeds, then dies the moment esptool changes rate:

```
Changing baud rate to 921600
Changed.
A fatal error occurred: Unable to verify flash chip connection
(Invalid head of packet (0xE0): Possible serial noise or corruption.)
```

Nothing is written when this happens, so the board still holds its previous
firmware — it is safe to just retry slower. Both scripts take an `FQBN`
override, so no edit is needed:

```bash
FQBN=esp32:esp32:esp32:UploadSpeed=115200 ./tools/flash.sh app
```

115200 works consistently; the cost is about a minute for a full app image
instead of ten seconds. 460800 is worth a try as a middle ground. If you flash
from the Arduino IDE instead, the same setting is **Tools → Upload Speed**.

This is a property of the individual USB-serial chip, not of the firmware — a
different cable or the other USB connector may well behave differently, so treat
the slow speed as the fallback rather than the default.

---

## Configuration

Everything user-specific lives in **one file: `config/config.h`**. It is not in
the repo — you create it from `config/config.example.h` (step 2 above) and edit
it. There is no runtime settings UI, so changing any of these means a rebuild
and reflash.

`config/config.h` is the **canonical** copy. `tools/sync_shared.sh` copies it
into `app/` and the two network Stage 0 sketches at build time, because the
Arduino build can't reach outside a sketch folder — those copies are generated
artifacts, so never edit them directly and never commit them either.

### WiFi

```c
#define WIFI_SSID   "CHANGE_ME"
#define WIFI_PASS   "CHANGE_ME"
```

2.4 GHz only — the ESP32 has no 5 GHz radio, so a network that is 5 GHz-only
will never appear. If your router publishes both bands under one name this
usually still works, but a separate 2.4 GHz SSID is the reliable option. The
SSID is case-sensitive. Open networks: leave `WIFI_PASS` as `""`.

`tools/sync_shared.sh` refuses to copy the config into the sketch folders while
either value is still `CHANGE_ME`, printing `SKIPPED config.h` — so forgetting
this step fails loudly instead of looking like a WiFi or hardware fault.

### Location — latitude and longitude

```c
#define LATITUDE    51.4779f      // placeholder: Greenwich Observatory
#define LONGITUDE   -0.0015f
```

These drive both the weather fetch and the local sunrise/sunset/moon math, so
getting them wrong gives you someone else's weather and the wrong golden hour.

**Format: decimal degrees, with the trailing `f`.** Not degrees-minutes-seconds.
Signs are what people get wrong most often:

| | Positive | Negative |
|---|---|---|
| `LATITUDE` | north of the equator | south of the equator |
| `LONGITUDE` | east of Greenwich | **west** of Greenwich |

So anywhere in the Americas needs a **negative longitude**. Worked examples:
New York `40.7128f` / `-74.0060f`; Tokyo `35.6762f` / `139.6503f`; Sydney
`-33.8688f` / `151.2093f`; São Paulo `-23.5505f` / `-46.6333f`.

**To find yours**, easiest first:

- **Google Maps** — right-click your location; the first item in the menu is the
  coordinate pair, already in decimal degrees and correctly signed. Click to
  copy.
- **OpenStreetMap** — right-click → *Show address*; the coordinates appear in
  the left panel.
- **[latlong.net](https://www.latlong.net/)** — search a place name, read the
  pair off directly.

If you have DMS from another source (`51°28'40"N`), convert with
degrees + minutes/60 + seconds/3600, then negate for S or W.

**On precision:** four decimals is about 11 m — far finer than any forecast
resolves, and precise enough to point at your house. Two decimals (~1 km) gives
identical weather. Use two, and pick a nearby landmark rather than your address
if you plan to share screenshots or logs: these coordinates are the one setting
that says where you live.

To confirm they took, watch the boot log: the sun/moon block prints sunrise and
sunset for your coordinates, which is easy to sanity-check against an almanac.

### Time zone

```c
#define TZ_STRING   "GMT0BST,M3.5.0/1,M10.5.0"
```

A **POSIX TZ string**, not an IANA name like `Europe/London` — the ESP32's libc
has no timezone database, so the DST rules have to be spelled out. The format is
`STDoffsetDST,start,end`, where the offset is *hours west* of UTC (so US Pacific
is `8`, and zones east of UTC take a negative offset).

| Zone | String |
|---|---|
| UK | `GMT0BST,M3.5.0/1,M10.5.0` |
| US Pacific | `PST8PDT,M3.2.0,M11.1.0` |
| US Eastern | `EST5EDT,M3.2.0,M11.1.0` |
| Central Europe | `CET-1CEST,M3.5.0,M10.5.0/3` |
| Japan (no DST) | `JST-9` |
| Sydney | `AEST-10AEDT,M10.1.0,M4.1.0/3` |
| UTC | `UTC0` |

Search "POSIX TZ string" plus your zone if it isn't listed.

Note the weather request uses `timezone=auto` and derives the day boundary from
your coordinates independently, so a wrong `TZ_STRING` shows up as a wrong
*clock*, not wrong weather.

### Units

```c
#define UNITS          UNITS_METRIC     // or UNITS_IMPERIAL
```

Display only — `AppData` always stores metric, and `units.h` converts at draw
time. Imperial switches temperature to °F, wind to mph and pressure to inHg
together; there is no per-field choice.

### If you already committed it

Adding `config.h` to `.gitignore` stops *future* commits. It does nothing about
a copy already in the repository — git history keeps it, and if the repo was
pushed, so does GitHub.

If that happened, in this order:

1. **Change your WiFi password.** Everything else is cleanup; this is the only
   step that actually revokes access. Treat any password that reached a public
   repo as burned, however briefly.
2. Stop tracking the file without deleting your local copy:
   ```bash
   git rm --cached config/config.h app/config.h stage0/*/config.h
   ```
3. Commit that, so the file is gone from the current tree.
4. Only then consider purging history with
   [git-filter-repo](https://github.com/newren/git-filter-repo) or the BFG. This
   rewrites every commit hash and needs a force-push, which breaks anyone else's
   clone — worth it for a public repo, overkill for a private one you've already
   rotated the password on.

---

## The application

Four scenes on a continuous rotation, with a persistent status strip:

| Scene | Dwell | Shows |
|---|---|---|
| Clock | 35 s | HH:MM in per-digit sprites, blinking colon, date line |
| Weather | 12 s | vector condition icon, temperature, feels-like, today's high/low, cloud/humidity/wind |
| Sun & Moon | 12 s | sunrise→sunset arc, moon phase + illumination, UV, golden hour |
| Air Quality | 12 s | US AQI colour-coded to the standard bands, PM2.5, pressure trend |

Weather and air quality come from Open-Meteo (no API key). Sun and moon are
**local math** — no network call, so they stay correct with WiFi down. Data is
stored metric in `AppData`; flipping `UNITS` in `config.h` is the only change
needed for imperial.

Today's high and low ride along on the same weather request via `&daily=`, so
they cost no extra fetch. `timezone=auto` on that URL is what makes the daily
bucket the local calendar day rather than a UTC one. They render as an amber ▲ /
blue ▼ row beneath the feels-like line, and are tracked by their own
`dailyValid` flag — if a response ever arrives without the `daily` block the row
is simply absent rather than showing a stray `0°`.

### Touch

| Gesture | Action |
|---|---|
| Tap | Advance to the next scene, and freeze auto-rotation for 45 s |
| Hold 0.8–4 s | Pin the current scene until pressed again |
| Hold > 4 s | Re-run touch calibration |

Gestures are classified **on release**, so one press can only ever produce one
event — no "the long press fired, and then the tap fired too". The cost is no
feedback during a hold, so the status strip previews what releasing would do:
the pin glyph goes dim past 0.8 s and cyan past 4 s, and the strip refreshes
five times faster while a finger is down.

Both stopped states are shown, because a display that has stopped rotating is
otherwise indistinguishable from a crashed one: an **orange pin** while pinned,
**cyan pause bars** while frozen after a tap.

### Touch calibration is not a firmware constant

It is measured on the device and stored in NVS. First boot runs a four-corner
wizard; afterwards the stored mapping loads silently.

A resistive panel's raw range is a property of *that individual panel* and
drifts with temperature, so a constant that is right on this unit is wrong on
the next one — and re-flashing is a poor repair path for something hanging on a
wall. The press threshold is likewise derived from the panel's own measured
noise floor at wizard time rather than assumed.

Two properties matter because this is an appliance that reboots unattended:

- The wizard **times out after 60 s** of no touch and boots the clock anyway on
  the previous mapping. A power cut with nobody home can never strand the
  device on a setup screen.
- Nothing is written to NVS until a **confirmation tap lands within 25 px** of a
  centre target. A wrong mapping that survives a reboot is worse than none.

It prints the equivalent constants to serial on every success, if you ever want
them as a baked-in default.

---

## Stage 0 — hardware verification

The rule (plan §1): every Stage 0 test produces a value the real code depends
on, so all of them pass before application code is written. Results live in
[docs/stage0_results.md](docs/stage0_results.md); the 2.4" board's completed
run is archived in `docs/stage0_results_2432S024R.md`.

| Test | Sketch | Proves | 2.8" status |
|---|---|---|---|
| 0.1 | `stage0/s01_display_test` | driver, backlight, inversion, RGB order, rotation, SPI speed | ✅ |
| 0.2 | `stage0/s02_touch_test` | the XPT2046 answers at all — display-free, so a panel fault can't mask a touch fault | ✅ |
| 0.2e | `stage0/s02e_touch_calibrate` | display + touch together: calibration, crosshair accuracy, hit-testing | ✅ |
| 0.3/0.4 | `stage0/s03_wifi_time_heap_test` | WiFi, local time with DST, heap baseline | ✅ (RSSI outstanding) |
| 0.5 | `stage0/s04_https_fetch_test` | one HTTPS fetch end-to-end + 60-fetch leak soak | fetch ✅, soak running |

`s02b`, `s02c`, `s02d` are diagnostic probes written to chase the 2.4" board's
dead touch line (hardware SPI, bit-bang, and VSPI). Keep them: if 0.2 ever
fails, they narrow down *why*.

### What the 2.8" board actually needed

Measured, not assumed — two of these contradict the usual advice:

| | 2.4" 2432S024R | 2.8" 2432S028R |
|---|---|---|
| Backlight | GPIO 27 | **GPIO 21** |
| Rotation (right way up) | 3 | **1** |
| Colour inversion | `TFT_INVERSION_ON` | `TFT_INVERSION_ON` — *same*, though the 2.8" is usually documented as needing it off |
| Touch | controller data line dead (hardware fault) | fully working |

Everything else — TFT pins, touch pins, SD, LDR, RGB LED, speaker — is identical
on both boards, which is why one `board.h` profile covers them.

---

## Two SPI buses — the one structural fact

- **Display → HSPI.** Requires `#define USE_HSPI_PORT` in TFT_eSPI's `User_Setup.h`.
- **Touch → VSPI.** Its own `SPIClass`, created in code.

Without `USE_HSPI_PORT`, TFT_eSPI takes VSPI as well. The display still works,
so the mistake surfaces much later as a touch panel that reads a constant zero.
`s01`, `s02e` and `app` all check this at **compile time** and refuse to build
against the wrong `User_Setup.h` — including a leftover 2.4" one.

The backlight is deliberately *not* configured in `User_Setup.h`, since the pin
is one of the things that differs between the boards. It is driven from code by
`cydBacklightOn()` in `board.h`, which also leaves room for LDR-driven dimming
later.

The touch driver is constructed with the **CS pin only**, never the IRQ pin.
Given an IRQ pin, `XPT2046_Touchscreen` reads once and then blocks every later
read until a falling edge fires — and if that interrupt never arrives, the panel
looks dead forever. Polling always works and costs nothing at our loop rate.
PENIRQ is still read as a diagnostic; it just isn't trusted to gate reads.

---

## Layout

```
.gitignore                keeps config.h (your WiFi password) out of git
config/
  board.h                 THE pin map + board profile (2.8" and 2.4")
  pins.h                  deprecated alias for board.h
  config.example.h        template — copy to config.h and fill in
  config.h                YOUR credentials — never commit; you create this
  User_Setup_2432S028R.h.template   TFT_eSPI config for the 2.8"   <- current
  User_Setup_2432S024R.h.template   TFT_eSPI config for the 2.4"
  User_Setup_2432S024R.h.backup     the exact library file that worked on the 2.4"
tools/
  sync_shared.sh          push config.h + board.h into every sketch folder
  build_all.sh            compile every sketch in one command
  flash.sh                compile + upload + serial monitor for one sketch
stage0/
  s01_display_test/       0.1  driver / backlight / inversion / RGB / rotation
  s02_touch_test/         0.2  raw touch readings, VSPI, polling
  s02e_touch_calibrate/   0.2e display + touch: calibration and hit-testing
  s02b, s02c, s02d/       touch fault-finding probes (from the 2.4" board)
  s03_wifi_time_heap_test/0.3 + 0.4  WiFi, local time w/ DST, heap baseline
  s04_https_fetch_test/   0.5  one HTTPS fetch + 60-fetch leak soak
app/
  app.ino                 setup/loop: display, touch, WiFi, time, scene machine
  touch.*                 XPT2046 input: polling, gestures, NVS calibration
  calibrate.*             on-screen calibration wizard (ported from s02e)
  scenes.*                scene machine + all four scenes, tap/pin handling
  status_strip.*          persistent bottom strip (time/WiFi/freshness/pin/dots)
  theme.h                 layout geometry + colour palette + shared `tft`
  app_data.h              global data model (plan §5.1)
  time_manager.*          SNTP + local-time helpers
  weather.*               Open-Meteo current + daily hi/lo + UV (15 min, backoff)
  airquality.*            Open-Meteo air-quality scheduler (30 min, backoff)
  sun_moon.*              local NOAA sun/moon math (no API)
  units.h                 metric↔imperial display conversion
docs/
  stage0_results.md            the live gate for the 2.8"
  stage0_results_2432S024R.md  archived 2.4" results (incl. the touch autopsy)
  decisions.md                 scope/architecture decisions, newest first
```

---

## Shared headers and the Arduino include rule

The Arduino build copies a sketch folder to a temp dir before compiling, so a
sketch **cannot** `#include "../../config/board.h"`. Every sketch folder
therefore carries its own copy of `board.h`, and the network sketches carry a
copy of `config.h`.

Those copies are **generated** — the canonical files are the ones in `config/`.
The generated `config.h` copies are git-ignored along with the canonical one, so
your password can't leak through a sketch folder either. (The `board.h` copies
are tracked, since they hold no secrets and a fresh clone can then build the
display sketches without running the sync first.) Edit in `config/`, then:

```bash
./tools/sync_shared.sh
```

`build_all.sh` and `flash.sh` run it for you, so in practice you edit `config/`
and rebuild. The script refuses to overwrite working credentials with a
`CHANGE_ME` placeholder.

---

## Toolchain

- **Arduino-ESP32** core 2.0.7 — board `esp32:esp32:esp32` ("ESP32 Dev Module")
- `TFT_eSPI` 2.5.43, `XPT2046_Touchscreen` 1.4, `ArduinoJson` **6**.17.2
  (`WiFi` / `WiFiClientSecure` / `HTTPClient` / `Preferences` / `time.h` ship
  with the core)
- The `tools/*.sh` scripts use the `arduino-cli` bundled inside Arduino IDE 2
  when there isn't one on `PATH` — nothing extra to install.

ArduinoJson is **v6** here, so `DynamicJsonDocument` is correct and the plan's
v7 `JsonDocument` syntax is not. Two other plan snippets are also wrong for this
target and were fixed during Stage 0.5: `setBufferSizes()` is ESP8266-only, and
parsing straight from `https.getStream()` mis-reads chunked responses and yields
an all-zeros document — buffer with `getString()` first.

---

## Troubleshooting

**Screen stays dark.** The backlight is on GPIO 21 and must be driven HIGH; a
sketch that never calls `cydBacklightOn()` looks like a dead board.

**Colours are the exact complement of what you expect** (red→cyan,
green→magenta, blue→yellow) — that's inversion, toggle `TFT_INVERSION_ON`. If
only red and blue swap while green stays green, that's RGB order instead.

**Touch reads a constant 0.** Check `USE_HSPI_PORT` is defined. If it is, run
`s02_touch_test`: if PENIRQ moves when you press but the data never does, the
controller's data line is faulty — that's what happened to the 2.4" unit, and it
isn't fixable in software.

**Touch is offset.** Hold the panel for 4 seconds to re-run calibration.

**Upload fails or resets mid-transfer.** The 2.8" board's micro-USB and USB-C
connectors are wired to the same CH340 — **use one at a time**. Otherwise it's
usually a charge-only cable, or the Arduino IDE still holding the serial port.

**Upload dies immediately after `Changing baud rate to 921600`**, with
`Invalid head of packet (0xE0)`. That's the CH340 failing the switch to the
default upload speed, not a bad image — retry at 115200 with
`FQBN=esp32:esp32:esp32:UploadSpeed=115200 ./tools/flash.sh app`. See
[Upload speed](#upload-speed).

**No serial port appears at all.** Charge-only cable, or a missing CH340 driver.
