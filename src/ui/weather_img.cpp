#include "weather_img.hpp"

#include "assets/weather/weather_icons_assets.h"

namespace desk_ui {

const lv_img_dsc_t* weatherIconImg(desk_display::WeatherIconId id) {
  using desk_display::WeatherIconId;
  switch (id) {
    case WeatherIconId::Clear:
      return &weather_icon_clear;
    case WeatherIconId::MostlyClear:
      return &weather_icon_mostly_clear;
    case WeatherIconId::Cloudy:
      return &weather_icon_cloudy;
    case WeatherIconId::Fog:
      return &weather_icon_fog;
    case WeatherIconId::Drizzle:
      return &weather_icon_drizzle;
    case WeatherIconId::Rain:
      return &weather_icon_rain;
    case WeatherIconId::Snow:
      return &weather_icon_snow;
    case WeatherIconId::Showers:
      return &weather_icon_showers;
    case WeatherIconId::Thunderstorm:
      return &weather_icon_thunderstorm;
    case WeatherIconId::Unknown:
    default:
      return &weather_icon_unknown;
  }
}

}  // namespace desk_ui
