#pragma once

#include "desk_display/screen_weather.hpp"

#include <lvgl.h>

namespace desk_ui {

/** Build the weather screen (temp, icon, feels, alert) under `parent`. */
void weather_lvgl_build(lv_obj_t* parent, const desk_display::WeatherScreenView& v);

}  // namespace desk_ui
