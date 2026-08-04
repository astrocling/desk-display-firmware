#pragma once

#include "desk_display/adsb_poll.hpp"
#include "desk_display/map_context.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

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
  enum class PollAttemptResult { Success, Retry, HardFail };
  PollAttemptResult tryPollOnce();

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
  /** Reused across ticks — async transports poll every frame while in flight. */
  std::unique_ptr<char[]> bodyBuf_{};
  /** Response buffer size; heap-allocated once (Dial TLS needs free heap). */
  static constexpr std::size_t kBodyCap = 64 * 1024;
};

}  // namespace desk_display
