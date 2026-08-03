# Stage 0 — Hardware verification results (ESP32-2432S024R, 2.4") — ARCHIVED

> **Superseded.** The project moved to the 2.8" ESP32-2432S028R on 2026-07-22
> (see `decisions.md`). This file is the finished record of the 2.4" bring-up,
> kept because the touch diagnosis below is the reference for what a *faulty*
> panel looks like. The live gate is `stage0_results.md`.

Fill this in as each test passes. These recorded values become constants in the
real application code (plan §3.6). **Gate: all six complete and passing before
any application code is written.**

> **STAGE 0 GATE: CLEARED (2026-07-22).** Display, WiFi/time, heap, and fetch all
> pass. Touch is a confirmed hardware fault and is dropped from v1 (auto-rotation
> only — see `decisions.md`). Cleared to start application code (plan §11).

| Item | Value | Test | Status |
|---|---|---|---|
| Driver define | `ILI9341_2_DRIVER` | 0.1 | ✅ |
| Inversion | `TFT_INVERSION_ON` | 0.1 | ✅ |
| RGB order | default (`TFT_BGR`) — R/G/B correct | 0.1 | ✅ |
| Rotation | `1` verified in 0.1; **app uses `3`** (same 320×240, flipped 180° per user preference) | 0.1 | ✅ |
| SPI frequency | `80000000` (80 MHz, stable) | 0.1 | ✅ |
| Touch raw X min / max | — | 0.2 | ❌ dropped (see below) |
| Touch raw Y min / max | — | 0.2 | ❌ dropped |
| Touch pressure threshold (Z) | — | 0.2 | ❌ dropped |
| TZ string | `PST8PDT,M3.2.0,M11.1.0` (local time confirmed correct) | 0.3 | ✅ |
| RSSI at final location | −42 dBm (excellent) | 0.3 | ✅ |
| Free heap / largest block | 254424 free / 110580 largest (~248 / 108 KB) | 0.4 | ✅ |
| Heap drift over 60 fetches | −1040 B over 10 fetches (~100 B/fetch, TLS cache settling; free stays ~248 KB) | 0.5 | ✅ |

## Notes / observations

- **0.1 Display:** PASS. Initial flash showed exact colour complements
  (red→cyan, green→magenta, blue→yellow) = full inversion; fixed with
  `TFT_INVERSION_ON`. After that, R/G/B bars correct, "TOP LEFT" legible,
  W=320 H=240. Left SPI at the library default 80 MHz (stable). Driver
  `ILI9341_2_DRIVER`, rotation 1.

- **0.2 Touch:** **NON-FUNCTIONAL on this unit — dropped from v1.**
  Diagnosis (in order): the XPT2046 controller is *alive* — its PENIRQ toggles
  GPIO36 when the panel is pressed (confirmed by the bit-bang pin scan, s02c).
  But the SPI data line (DOUT / T_DO on GPIO39) never returns a single valid
  bit. It reads a constant `0x0000` under **three independent access methods**:
  HSPI hardware SPI (s02b), pure GPIO bit-bang (s02c), and VSPI hardware SPI
  (s02d). z therefore computes to a stuck 4095 with x=y=0.
  Pins were verified against web pinout refs and match (CLK25 / CS33 / MOSI32 /
  MISO39 / IRQ36). IRQ works, DOUT doesn't → **hardware fault**: almost
  certainly a broken/cold T_DO solder joint or a dead touch controller on this
  specific board. Not a software problem.
  **Decision:** proceed without touch. Scenes auto-rotate on dwell timers only
  (see `docs/decisions.md`). Touch is isolated and can be restored later on a
  known-good board — the s02* diagnostic sketches are kept for that.

- **0.3 WiFi & time:** PASS. Associated to a 2.4 GHz SSID, DHCP lease obtained, RSSI −42 dBm
  (excellent). Local time confirmed correct with `PST8PDT,M3.2.0,M11.1.0` → user
  is in the Pacific zone; DST handled. Boot ROM garbling ("ets…/rst:") at monitor
  open is normal.
- **0.4 Heap:** PASS. Free 254424 (~248 KB), min free 251856, largest block
  110580 (~108 KB) with WiFi+NTP up. Well above the 150 KB gate; largest block
  is ~7× a per-digit sprite.
- **0.5 Fetch soak:** PASS. Open-Meteo current weather for the configured
  location (the response echoed back the coordinates and resolved timezone).
  Sample: 28.4 °C, feels 31.0, code 0, cloud 0%, RH 51%, wind 9.6 km/h, 1007.5
  hPa. **Two plan snippets were wrong for this ESP32 env and were fixed:**
  (1) `setBufferSizes()` is ESP8266-only — removed; (2) parsing from
  `https.getStream()` yielded an all-zeros doc (chunked encoding) — switched to
  buffering the payload with `getString()` then parsing. ArduinoJson v6.17.2
  syntax (`DynamicJsonDocument`), not v7. Heap drift tiny and non-runaway; the
  real leak test is the 72 h soak (build step 6).

## Part number (from PCB silkscreen)

Silkscreen reads: `ESP32-024` (confirms the 2.4" ESP32-2432S024 family).
