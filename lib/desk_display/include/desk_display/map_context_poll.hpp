#pragma once

#include "desk_display/adsb_poll.hpp"
#include "desk_display/map_context.hpp"

#include <cstddef>
#include <cstdint>

namespace desk_display {

/** Quiet period after center/range change before issuing a map-context GET. */
constexpr uint32_t kMapContextDebounceMs = 400;

bool buildMapContextUrl(char* buf, std::size_t bufLen, double lat, double lon,
                        float radiusMi);

class MapContextPoller {
 public:
  void setHttpGet(AdsbHttpGetFn fn, void* user);
  void setActive(bool radarIsActiveScreen);
  void setCenter(double lat, double lon, float rangeMi);
  void onTick(uint32_t elapsedMs);
  bool takeContext(MapContext& out);
  bool hasLastGood() const;

 private:
  bool tryPollOnce();

  AdsbHttpGetFn httpGet_{nullptr};
  void* httpUser_{nullptr};
  bool active_{false};
  double centerLat_{0.0};
  double centerLon_{0.0};
  float rangeMi_{25.0f};
  uint32_t debounceMs_{0};
  bool needsFetch_{false};
  bool hasLastGood_{false};
  bool hasPending_{false};
  MapContext lastGood_{};
  static constexpr std::size_t kBodyCap = 256 * 1024;
  char body_[kBodyCap]{};
};

}  // namespace desk_display
