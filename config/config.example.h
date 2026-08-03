// config.example.h  --  TEMPLATE ONLY. Placeholder values; edit nothing here.
//
//   cp config/config.example.h config/config.h
//
// Then fill in YOUR values in config.h. tools/sync_shared.sh copies it into
// each sketch folder that needs it (the Arduino build can't reach outside a
// sketch folder), so config/config.h is the only one you ever edit.
//
// NEVER COMMIT config.h -- it carries your WiFi password in plain text. It is
// listed in .gitignore; if you ever see it in `git status`, something removed
// that rule. See the Configuration section of README.md.
//
// Every Stage 0 sketch expects these symbols.

#pragma once

// ---- WiFi -----------------------------------------------------------------
// 2.4 GHz only -- the ESP32 has no 5 GHz radio. Open network: leave PASS "".
// tools/sync_shared.sh refuses to sync while the placeholders below are
// unchanged, so a forgotten edit fails loudly instead of looking like a
// hardware fault.
#define WIFI_SSID   "CHANGE_ME"
#define WIFI_PASS   "CHANGE_ME"

// ---- Location (decimal degrees) -------------------------------------------
// Drives the weather fetch AND the local sun/moon math -- wrong values give you
// someone else's weather and the wrong golden hour.
//
// Decimal degrees, keep the trailing `f`. Latitude +N/-S, longitude +E/-W, so
// anywhere in the Americas needs a NEGATIVE longitude. Right-click your spot in
// Google Maps to get the pair already correctly signed.
//
// Placeholder below is the Greenwich Observatory -- replace it. 2-4 decimals is
// plenty (4 dp ~ 11 m, far finer than any forecast resolves).
#define LATITUDE    51.4779f
#define LONGITUDE   -0.0015f

// ---- Time zone (POSIX TZ string) ------------------------------------------
// A POSIX TZ string, NOT an IANA name like "Europe/London" -- the ESP32's libc
// has no tz database, so DST rules must be spelled out. Offset is hours WEST
// of UTC (negative for zones east of it).
//   UK:            "GMT0BST,M3.5.0/1,M10.5.0"
//   US Pacific:    "PST8PDT,M3.2.0,M11.1.0"
//   US Eastern:    "EST5EDT,M3.2.0,M11.1.0"
//   Central Europe:"CET-1CEST,M3.5.0,M10.5.0/3"
//   Japan (no DST):"JST-9"
//   UTC:           "UTC0"
#define TZ_STRING   "GMT0BST,M3.5.0/1,M10.5.0"

// ---- Units ----------------------------------------------------------------
// 0 = metric (C, kph, hPa), 1 = imperial (F, mph, inHg)
#define UNITS_METRIC   0
#define UNITS_IMPERIAL 1
#define UNITS          UNITS_METRIC
