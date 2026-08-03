// ===========================================================================
// units.h -- display-unit conversion (plan §10)
// ===========================================================================
// AppData stores canonical METRIC values (as fetched). The display layer calls
// these to present metric or imperial per the UNITS setting in config.h. Keeps
// the data model unit-agnostic.
#pragma once

#include "config.h"

static inline float dispTemp(float c) {
  return (UNITS == UNITS_IMPERIAL) ? (c * 9.0f / 5.0f + 32.0f) : c;
}
static inline float dispWind(float kph) {
  return (UNITS == UNITS_IMPERIAL) ? (kph * 0.621371f) : kph;
}
static inline float dispPress(float hpa) {
  return (UNITS == UNITS_IMPERIAL) ? (hpa * 0.0295300f) : hpa;
}

static inline const char* tempUnit()  { return (UNITS == UNITS_IMPERIAL) ? "F"    : "C"; }
static inline const char* windUnit()  { return (UNITS == UNITS_IMPERIAL) ? "mph"  : "km/h"; }
static inline const char* pressUnit() { return (UNITS == UNITS_IMPERIAL) ? "inHg" : "hPa"; }
