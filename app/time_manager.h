// ===========================================================================
// time_manager.h -- SNTP time with a POSIX TZ string (plan §5.2)
// ===========================================================================
#pragma once

#include <time.h>

// Start SNTP with the given POSIX TZ string (e.g. "PST8PDT,M3.2.0,M11.1.0").
void timeManager_begin(const char* tz);

// Fill `out` with current LOCAL time. Returns false until NTP has synced, so
// callers can show a placeholder instead of a bogus 1970 clock.
bool timeManager_now(struct tm& out);
