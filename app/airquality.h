// ===========================================================================
// airquality.h -- air-quality fetch scheduler (plan §5.2)
// ===========================================================================
// Open-Meteo air-quality API (US AQI + PM2.5) into g_data every 30 min, with
// the same backoff and last-good semantics as weather. Runs sequentially with
// the weather fetch (both are blocking calls in loop(), never concurrent).
#pragma once

void airquality_begin();
void airquality_tick();
