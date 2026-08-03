#pragma once

#include <cstdint>

namespace desk_display {

constexpr const char* kWifiNvsNamespace = "wifi";
constexpr const char* kWifiNvsKeySsid = "ssid";
constexpr const char* kWifiNvsKeyPass = "pass";
constexpr const char* kWifiPlaceholderSsid = "your-ssid";

constexpr uint32_t kWifiConnectTimeoutMs = 20000;
constexpr uint32_t kWifiRetryDelayInitialMs = 5000;
constexpr uint32_t kWifiRetryDelayMaxMs = 30000;

bool isPlaceholderWifiSsid(const char* ssid);
uint32_t nextWifiRetryDelayMs(uint32_t previousDelayMs);

}  // namespace desk_display
