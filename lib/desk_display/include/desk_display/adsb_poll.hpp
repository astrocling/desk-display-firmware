#pragma once

#include "desk_display/adsb.hpp"

#include <cstddef>
#include <cstdint>

namespace desk_display {

constexpr uint32_t kAdsbPollIntervalMs = 10000;

using AdsbHttpGetFn = bool (*)(const char* url, char* body, std::size_t bodyCap,
                               std::size_t& bodyLen, void* user);

bool buildAdsbLolUrl(char* buf, std::size_t bufLen, double lat, double lon,
                     float rangeStatuteMi);
// https://api.adsb.lol/v2/lat/{lat}/lon/{lon}/dist/{radiusNm}

class AdsbPoller {
 public:
  void setHttpGet(AdsbHttpGetFn fn, void* user);
  void setActive(bool radarIsActiveScreen);
  void setCenter(double lat, double lon, float rangeStatuteMi);
  void onTick(uint32_t elapsedMs);
  bool takeAircraft(AircraftList& out);  // true once per success until consumed
  bool hasLastGood() const;

 private:
  void pollOnce();

  AdsbHttpGetFn httpGet_{nullptr};
  void* httpUser_{nullptr};
  bool active_{false};
  double centerLat_{0.0};
  double centerLon_{0.0};
  float rangeStatuteMi_{25.0f};
  uint32_t pollMs_{0};
  bool hasLastGood_{false};
  bool hasPending_{false};
  AircraftList lastGood_{};
  static constexpr std::size_t kBodyCap = 256 * 1024;
  char body_[kBodyCap]{};
};

}  // namespace desk_display
