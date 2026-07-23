#pragma once

#include "desk_display/weather_icons.hpp"
#include "lvgl.h"

/** LVGL image descriptor for a weather icon id (Meteocons monochrome). */
const lv_img_dsc_t* weatherIconImg(desk_display::WeatherIconId id);
