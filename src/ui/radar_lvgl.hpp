#pragma once

#include "desk_display/screen_radar.hpp"

#include <lvgl.h>

namespace desk_ui {

/** Diameter (px) of the radar disc (rings + sweep + blips). */
constexpr lv_coord_t kRadarDiscPx = 240;

/**
 * Build the radar screen (header, concentric rings, sweep line in Classic,
 * aircraft blips) as children of `parent`. Shared by the sim and the dial's
 * `screen_radar_*` LVGL implementation.
 *
 * Caller owns `parent`'s lifetime and should clear any previous children
 * (e.g. `lv_obj_clean`) before rebuilding on each view refresh.
 */
void radar_lvgl_build(lv_obj_t* parent, const desk_display::RadarView& v);

}  // namespace desk_ui
