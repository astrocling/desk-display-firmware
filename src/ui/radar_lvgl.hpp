#pragma once

#include "desk_display/screen_radar.hpp"

#include <lvgl.h>

namespace desk_ui {

/** Diameter (px) of the radar disc (rings + sweep + blips). Near full 360 display. */
constexpr lv_coord_t kRadarDiscPx = 340;

/** Blip plot radius (px) — slightly inside the outer ring. */
constexpr float kRadarPlotRadiusPx = 160.0f;

/** Content-area Y nudge used by the sim shell; hit-tests must match. */
constexpr lv_coord_t kRadarContentOffsetY = 4;

inline float radar_blip_scale(float rangeMiles) {
  return kRadarPlotRadiusPx / (rangeMiles > 0.0f ? rangeMiles : 1.0f);
}

/**
 * Build the radar screen (header, concentric rings, sweep line in Classic,
 * aircraft blips) as children of `parent`. Shared by the sim and the dial's
 * `screen_radar_*` LVGL implementation.
 *
 * Caller owns `parent`'s lifetime and should clear any previous children
 * (e.g. `lv_obj_clean`) before a full rebuild — then call
 * `radar_lvgl_invalidate()` so animate caches are dropped.
 */
void radar_lvgl_build(lv_obj_t* parent, const desk_display::RadarView& v);

/**
 * In-place Classic sweep + phosphor + dots update (no disc teardown).
 * Returns false if `parent` is not the currently built Classic disc (caller
 * should `lv_obj_clean` + `radar_lvgl_build`).
 */
bool radar_lvgl_animate_classic(lv_obj_t* parent,
                                const desk_display::RadarView& v);

/** Drop cached object pointers after the parent was cleaned or destroyed. */
void radar_lvgl_invalidate();

}  // namespace desk_ui
