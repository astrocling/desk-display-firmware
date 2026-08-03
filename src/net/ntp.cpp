#include "net/ntp.hpp"

#include <Arduino.h>
#include <time.h>

#include "net/wifi.hpp"

namespace desk_net {
namespace {

constexpr int kMinValidYear = 2024;

bool gSetupCalled = false;
bool gSntpStarted = false;
bool gSynced = false;

bool timeLooksValid(time_t unixUtc) {
  if (unixUtc <= 0) {
    return false;
  }
  struct tm timeinfo {};
  if (gmtime_r(&unixUtc, &timeinfo) == nullptr) {
    return false;
  }
  return (timeinfo.tm_year + 1900) >= kMinValidYear;
}

void maybeStartSntp() {
  if (gSntpStarted || !wifiIsConnected()) {
    return;
  }
  configTime(0, 0, "pool.ntp.org");
  gSntpStarted = true;
  Serial.println("ntp: syncing");
}

void maybeMarkSynced() {
  if (gSynced || !gSntpStarted || !wifiIsConnected()) {
    return;
  }
  const time_t now = time(nullptr);
  if (!timeLooksValid(now)) {
    return;
  }
  gSynced = true;
  Serial.printf("ntp: synced unix=%lld\n", static_cast<long long>(now));
}

}  // namespace

void ntpSetup() {
  if (gSetupCalled) {
    return;
  }
  gSetupCalled = true;
}

void ntpLoop() {
  if (!gSetupCalled) {
    return;
  }
  maybeStartSntp();
  maybeMarkSynced();
}

bool ntpIsSynced() {
  if (!gSynced) {
    return false;
  }
  return timeLooksValid(time(nullptr));
}

std::int64_t ntpUnixUtc() {
  if (!ntpIsSynced()) {
    return 0;
  }
  return static_cast<std::int64_t>(time(nullptr));
}

}  // namespace desk_net
