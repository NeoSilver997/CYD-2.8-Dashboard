// ===========================================================================
// app_data.h -- the single global data model (plan §5.1)
// ===========================================================================
// Written only by the fetch layer (added later), read only by scenes. A failed
// fetch NEVER clears existing values; it only stops refreshing *UpdatedAt, so
// scenes always render last-good data and the status strip shows staleness.
//
// For Scene 1 + the scene machine, only the clock (system time) is live; the
// weather/aqi/uv/sun-moon fields are declared now so the struct is stable, and
// are populated in later build steps. Global zero-init leaves all *Valid=false.
#pragma once

#include <Arduino.h>
#include <time.h>

struct AppData {
  // weather
  float    tempC = 0, feelsLikeC = 0;
  float    tempMaxC = 0, tempMinC = 0;   // today's forecast high/low
  bool     dailyValid = false;           // daily block seen at least once
  int      weatherCode = 0;
  int      cloudCoverPct = 0, humidityPct = 0;
  float    windKph = 0, pressureHpa = 0, pressureTrend = 0;
  uint32_t weatherUpdatedAt = 0;      // epoch secs; 0 = never
  bool     weatherValid = false;

  // air quality
  int      aqi = 0, pm25 = 0;
  uint32_t aqiUpdatedAt = 0;
  bool     aqiValid = false;

  // uv
  float    uvIndex = 0;
  uint32_t uvUpdatedAt = 0;
  bool     uvValid = false;

  // sun/moon -- computed locally, always valid once computed
  time_t   sunriseToday = 0, sunsetToday = 0;
  time_t   sunriseTomorrow = 0, sunsetTomorrow = 0;
  bool     showingNextDay = false;
  float    moonPhase = 0;              // 0.0-1.0
  float    moonIlluminationPct = 0;
  time_t   moonrise = 0, moonset = 0;

  // bus ETA
  static const int BUS_MAX = 40;
  static const int BUS_STOP_MAX = 10;
  struct BusRoute {
    char route[8];
    char dest[28];
    char eta1[8];
    char eta2[8];
    char remark1[16];
    char remark2[16];
  };
  struct BusStop {
    char stopId[20];
    char name[32];
    BusRoute routes[BUS_MAX];
    int     routeCount = 0;
  };
  BusStop busStops[BUS_STOP_MAX];
  int      busStopCount = 0;
  uint32_t busUpdatedAt = 0;
  bool     busValid = false;
};

extern AppData g_data;
