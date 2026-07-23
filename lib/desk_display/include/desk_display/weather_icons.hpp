#pragma once

namespace desk_display {

enum class WeatherIconId {
  Clear = 0,
  MostlyClear,
  Cloudy,
  Fog,
  Drizzle,
  Rain,
  Snow,
  Showers,
  Thunderstorm,
  Unknown
};

/** Map WMO weather interpretation code → icon id. */
WeatherIconId wmoToIcon(int code);

}  // namespace desk_display
