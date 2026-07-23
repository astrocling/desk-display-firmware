#pragma once

#include <cstddef>
#include <cstdint>

namespace desk_display {

constexpr std::size_t kMaxHourly = 48;
constexpr std::size_t kMaxAlertSeverity = 32;
constexpr std::size_t kMaxAlertHeadline = 160;
constexpr std::size_t kMaxUpdatedAt = 40;

struct WeatherHourly {
  char time[32];
  float temp;
  int code;
};

struct WeatherAlert {
  bool present;
  char severity[kMaxAlertSeverity];
  char headline[kMaxAlertHeadline];
};

struct Weather {
  float currentTemp;
  float currentFeelsLike;
  int currentCode;
  float todayHigh;
  float todayLow;
  WeatherHourly hourly[kMaxHourly];
  std::size_t hourlyCount;
  WeatherAlert alert;
  bool hasUpdatedAt;
  char updatedAt[kMaxUpdatedAt];
};

/** Parse weather JSON. Returns false on malformed / missing required fields. */
bool parseWeather(const char* json, Weather& out);

}  // namespace desk_display
