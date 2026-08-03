#pragma once

#include "desk_display/screen_clock.hpp"

#include <lvgl.h>

namespace desk_ui {

/** Build the clock screen (date, 12h time, arc face, optional TZ hint) under `parent`. */
void clock_lvgl_build(lv_obj_t* parent, const desk_display::ClockView& v);

}  // namespace desk_ui
