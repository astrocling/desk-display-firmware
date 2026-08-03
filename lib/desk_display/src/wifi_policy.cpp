#include "desk_display/wifi_policy.hpp"

#include <cstring>

namespace desk_display {

bool isPlaceholderWifiSsid(const char* ssid) {
  if (ssid == nullptr || ssid[0] == '\0') {
    return true;
  }
  return std::strcmp(ssid, kWifiPlaceholderSsid) == 0;
}

uint32_t nextWifiRetryDelayMs(uint32_t previousDelayMs) {
  if (previousDelayMs == 0) {
    return kWifiRetryDelayInitialMs;
  }
  const uint64_t doubled = static_cast<uint64_t>(previousDelayMs) * 2u;
  if (doubled >= kWifiRetryDelayMaxMs) {
    return kWifiRetryDelayMaxMs;
  }
  return static_cast<uint32_t>(doubled);
}

}  // namespace desk_display
