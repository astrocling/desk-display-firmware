#include "desk_display/screen_weather.hpp"

#include <cstdio>
#include <cstring>

#include "desk_display/format_time.hpp"

namespace desk_display {
namespace {

void fillWhenLabel(WeatherScreenView& v, const Weather& weather, int scrubIndex) {
  v.whenLabel[0] = '\0';
  if (!v.ready) {
    return;
  }
  if (v.showingNow || weather.hourlyCount == 0) {
    std::snprintf(v.whenLabel, sizeof(v.whenLabel), "Current");
    return;
  }
  const WeatherHourly& hour = weather.hourly[static_cast<std::size_t>(scrubIndex)];
  int hour24 = 0;
  if (!parseHourlyIsoHour(hour.time, hour24) ||
      format12HourShort(v.whenLabel, sizeof(v.whenLabel), hour24) == 0) {
    std::snprintf(v.whenLabel, sizeof(v.whenLabel), "--");
  }
}

}  // namespace

WeatherScreen::WeatherScreen() { clear(); }

void WeatherScreen::clear() {
  ready_ = false;
  weather_ = Weather{};
  scrubIndex_ = kWeatherScrubNow;
  alertDetailOpen_ = false;
}

void WeatherScreen::bind(const Weather& weather) {
  weather_ = weather;
  ready_ = true;
  scrubIndex_ = kWeatherScrubNow;
  alertDetailOpen_ = false;
}

void WeatherScreen::onRotate(int delta) {
  if (!ready_ || delta == 0) {
    return;
  }
  if (weather_.hourlyCount == 0) {
    scrubIndex_ = kWeatherScrubNow;
    return;
  }

  // Map "now" (-1) into the same axis as hourly indices for stepping.
  int pos = scrubIndex_;
  pos += delta;
  if (pos < kWeatherScrubNow) {
    pos = kWeatherScrubNow;
  }
  const int maxIndex = static_cast<int>(weather_.hourlyCount) - 1;
  if (pos > maxIndex) {
    pos = maxIndex;
  }
  scrubIndex_ = pos;
}

void WeatherScreen::snapToNow() {
  scrubIndex_ = kWeatherScrubNow;
}

void WeatherScreen::onIdleSettle() {
  snapToNow();
  closeAlertDetail();
}

bool WeatherScreen::openAlertDetail() {
  if (!ready_ || !weather_.alert.present) {
    return false;
  }
  alertDetailOpen_ = true;
  return true;
}

void WeatherScreen::closeAlertDetail() { alertDetailOpen_ = false; }

WeatherScreenView WeatherScreen::view() const {
  WeatherScreenView v{};
  v.ready = ready_;
  v.scrubIndex = scrubIndex_;
  v.showingNow = (scrubIndex_ == kWeatherScrubNow);
  v.hourly = weather_.hourly;
  v.hourlyCount = weather_.hourlyCount;
  v.todayHigh = weather_.todayHigh;
  v.todayLow = weather_.todayLow;
  v.feelsLike = weather_.currentFeelsLike;
  v.alertBadge = ready_ && weather_.alert.present;
  v.alertDetailOpen = alertDetailOpen_;
  v.alertSeverity = weather_.alert.severity;
  v.alertHeadline = weather_.alert.headline;

  if (!ready_) {
    v.displayTemp = 0.0f;
    v.weatherCode = 0;
    v.icon = WeatherIconId::Unknown;
    v.whenLabel[0] = '\0';
    return v;
  }

  if (v.showingNow || weather_.hourlyCount == 0) {
    v.displayTemp = weather_.currentTemp;
    v.weatherCode = weather_.currentCode;
  } else {
    const WeatherHourly& hour = weather_.hourly[static_cast<std::size_t>(scrubIndex_)];
    v.displayTemp = hour.temp;
    v.weatherCode = hour.code;
  }
  v.icon = wmoToIcon(v.weatherCode);
  fillWhenLabel(v, weather_, scrubIndex_);
  return v;
}

}  // namespace desk_display
