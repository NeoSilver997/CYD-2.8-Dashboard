// ===========================================================================
// calibrate.h -- on-screen touch calibration wizard
// ===========================================================================
// Ported from stage0/s02e_touch_calibrate, which is where the method was
// proven. Four corner targets, the axis mapping worked out by measurement
// rather than guessed, then a confirmation tap before anything is saved.
//
// Runs automatically on first boot (nothing stored in NVS) and on demand from
// a > 4 s press. It is deliberately escapable: this is a wall clock, and an
// unattended reboot must never leave it stuck on a wizard nobody is there to
// answer. If no one touches the panel for a minute it gives up and the clock
// starts with whatever mapping it already had.
#pragma once

#include <Arduino.h>

// Returns true if a new calibration was accepted (and saved when persist).
// Returns false on timeout, leaving the previous mapping untouched.
bool calibrate_run(bool persist = true);
