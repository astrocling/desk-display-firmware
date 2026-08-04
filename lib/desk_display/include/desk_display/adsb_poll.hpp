#pragma once

#include "desk_display/adsb.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace desk_display {

/** Cadence between successful binds while Radar is the active screen. */
constexpr uint32_t kAdsbPollIntervalMs = 10000;

/**
 * Start the HTTP GET this many ms before the interval elapses so a typical
 * response lands before the Classic sweep wraps through north. The transport
 * may return false while a request is still in flight (non-blocking / async);
 * the poller retries each tick until success or `kAdsbFetchMaxWaitMs`.
 */
constexpr uint32_t kAdsbPrefetchLeadMs = 2500;

/** Abandon an in-flight / failing attempt after this many ms past prefetch start. */
constexpr uint32_t kAdsbFetchMaxWaitMs = 8000;

/**
 * Blocking or non-blocking HTTPS GET.
 * - true: `body`/`bodyLen` hold a complete response
 * - false: not ready yet (async in flight) or hard failure — poller will retry
 *   until `kAdsbFetchMaxWaitMs`, then wait for the next interval
 */
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
  /** Returns true when a new list was parsed into lastGood_/hasPending_. */
  bool tryPollOnce();

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
  /** Reused across ticks — async transports poll every frame while in flight. */
  std::unique_ptr<char[]> bodyBuf_{};
  /** Response buffer size; heap-allocated once (Dial TLS needs free heap). */
  static constexpr std::size_t kBodyCap = 64 * 1024;
};

}  // namespace desk_display
