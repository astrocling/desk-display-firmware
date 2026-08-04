#pragma once

#include <cstddef>
#include <cstdint>

namespace desk_net {

/** Separate result slots so pollers can prefetch without sharing in-flight state. */
enum class HttpAsyncChannel : std::uint8_t {
  RadarMap = 0,
  RadarAdsb = 1,
  Weather = 2,
  Scores = 3,
};

/**
 * Non-blocking HTTPS GET for Dial (FreeRTOS worker + TLS).
 * Same contract as sim async / AdsbHttpGetFn:
 * - true + bodyLen>0: complete response copied into `body`
 * - true + bodyLen==0: terminal failure (pollers HardFail / give up interval)
 * - false: in flight or not ready — poller retries next tick
 *
 * Call httpAsyncInit() once from shell init before first use.
 */
bool httpAsyncInit();

bool httpGetAsync(HttpAsyncChannel channel, const char* url, char* body,
                  std::size_t bodyCap, std::size_t& bodyLen, void* user = nullptr);

}  // namespace desk_net
