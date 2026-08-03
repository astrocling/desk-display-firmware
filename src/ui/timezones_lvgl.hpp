#pragma once

#include "desk_display/screen_timezones.hpp"

#include <lvgl.h>

namespace desk_ui {

/** host_h: parent height used to vertically center the board (sim used kDispH-48). */
void timezones_lvgl_build(lv_obj_t* parent, const desk_display::TimezoneBoardView& v,
                          lv_coord_t host_h);

}  // namespace desk_ui
