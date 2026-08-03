#pragma once

#include "desk_display/screen_sports.hpp"

#include <lvgl.h>

namespace desk_ui {

/** Build the sports screen (MLB live/next + Flagstand) under `parent`. */
void sports_lvgl_build(lv_obj_t* parent, const desk_display::SportsView& v);

}  // namespace desk_ui
