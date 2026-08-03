#include "time_manager.h"
#include <Arduino.h>   // ESP32 core: declares configTzTime() and getLocalTime()

void timeManager_begin(const char* tz) {
  // configTzTime installs the TZ rules AND kicks off SNTP; DST is handled by
  // the POSIX string, so getLocalTime() returns correct wall-clock time.
  configTzTime(tz, "pool.ntp.org", "time.nist.gov");
}

bool timeManager_now(struct tm& out) {
  // getLocalTime returns false until the year looks sane (i.e. NTP has synced).
  // Small 10 ms budget keeps this effectively non-blocking in the render loop.
  return getLocalTime(&out, 10);
}
