#include "desk_display/weather_icons.hpp"

namespace desk_display {

WeatherIconId wmoToIcon(int code) {
  if (code == 0) {
    return WeatherIconId::Clear;
  }
  if (code == 1 || code == 2) {
    return WeatherIconId::MostlyClear;
  }
  if (code == 3) {
    return WeatherIconId::Cloudy;
  }
  if (code == 45 || code == 48) {
    return WeatherIconId::Fog;
  }
  if (code == 51 || code == 53 || code == 55 || code == 56 || code == 57) {
    return WeatherIconId::Drizzle;
  }
  if (code == 61 || code == 63 || code == 65 || code == 66 || code == 67) {
    return WeatherIconId::Rain;
  }
  if (code == 71 || code == 73 || code == 75 || code == 77) {
    return WeatherIconId::Snow;
  }
  if (code == 80 || code == 81 || code == 82 || code == 85 || code == 86) {
    return WeatherIconId::Showers;
  }
  if (code == 95 || code == 96 || code == 99) {
    return WeatherIconId::Thunderstorm;
  }
  return WeatherIconId::Unknown;
}

}  // namespace desk_display
