#pragma once

#include "desk_display/adsb_poll.hpp"
#include "desk_display/weather.hpp"

#include <cstddef>
#include <cstdint>

namespace desk_display {

/** Cadence between successful binds while Weather is the active screen. */
constexpr uint32_t kWeatherPollIntervalMs = 5 * 60 * 1000;

bool buildWeatherUrl(char* buf, std::size_t bufLen);

class WeatherPoller {
 public:
  void setHttpGet(AdsbHttpGetFn fn, void* user);
  void setActive(bool weatherIsActiveScreen);
  void onTick(uint32_t elapsedMs);
  bool takeWeather(Weather& out);  // true once per success until consumed
  bool hasLastGood() const;

 private:
  bool tryPollOnce();

  AdsbHttpGetFn httpGet_{nullptr};
  void* httpUser_{nullptr};
  bool active_{false};
  uint32_t pollMs_{0};
  bool hasLastGood_{false};
  bool hasPending_{false};
  Weather lastGood_{};
  static constexpr std::size_t kBodyCap = 8 * 1024;
  char body_[kBodyCap]{};
};

}  // namespace desk_display
