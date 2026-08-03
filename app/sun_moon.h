// ===========================================================================
// sun_moon.h -- local sun/moon math (plan §8). No API: stays correct offline.
// ===========================================================================
// NOAA solar position for sunrise/sunset (and twilight/golden-hour elevations),
// moon phase from the synodic month. Roll-forward is triggered by SUNSET, not
// midnight: after today's sunset we show tomorrow's times (showingNextDay).
#pragma once

#include <time.h>
#include <stddef.h>

// Recompute today's + tomorrow's sun times, the showingNextDay flag, and the
// moon phase/illumination into g_data. Cheap; safe to call on scene enter.
// Prints the results to Serial once per boot for offline almanac verification.
void sunmoon_recompute();

// Human-readable moon phase name for a phase in [0,1] (0/1 = new, 0.5 = full).
const char* moonPhaseName(float phase);

// Write a golden-hour status string ("now (12m)", "in 1h 20m", ...) into buf.
void goldenHourStatus(char* buf, size_t n);

// UTC epoch of the sun reaching `elevDeg` on the given local date; morning=true
// for the rising crossing, false for setting. 0 if it never reaches it (polar).
// Exposed for the scene's arc/marker use.
time_t sunEventUTC(int year, int mon, int day, double elevDeg, bool morning);
