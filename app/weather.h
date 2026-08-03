// ===========================================================================
// weather.h -- weather fetch scheduler (plan §5.2)
// ===========================================================================
// Sequential, single TLS connection. Fetches Open-Meteo current weather into
// g_data every 15 min, with 1→2→4→8→15 min backoff on failure. A failed fetch
// never clears last-good data (plan §5.1) -- it just doesn't refresh
// weatherUpdatedAt, so the status strip's freshness dot ages.
#pragma once

void weather_begin();   // arm the scheduler (first fetch happens asap)
void weather_tick();     // call every loop; performs a fetch when one is due
