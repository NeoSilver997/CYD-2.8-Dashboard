// ===========================================================================
// status_strip.h -- persistent bottom strip on all scenes (plan §7)
// ===========================================================================
// Contains: time (HH:MM), WiFi bars, data-freshness dot, a pin glyph while a
// scene is pinned, a pause glyph while auto-rotation is frozen after a tap,
// and the scene-position dots.
//
// The two touch indicators matter more than they look: a display that has
// stopped rotating is indistinguishable from a crashed one unless it says why.
//
// Only the content area is cleared on scene change, so the strip persists; call
// statusStrip_tick() periodically to refresh its dynamic parts in place.
#pragma once

void statusStrip_init();           // draw static background + first paint
void statusStrip_tick(bool force); // refresh dynamic parts (cheap, cached)
