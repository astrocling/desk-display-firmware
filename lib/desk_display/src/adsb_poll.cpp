#include "desk_display/adsb_poll.hpp"

#include "desk_display/radar.hpp"

#include <cstdio>
#include <cstring>

namespace desk_display {

bool buildAdsbLolUrl(char* buf, std::size_t bufLen, double lat, double lon,
                     float rangeStatuteMi) {
  if (!buf || bufLen == 0) {
    return false;
  }

  const float radiusNm =
      clampAdsbQueryRadiusNm(statuteMilesToNauticalMiles(rangeStatuteMi));
  const int written = std::snprintf(
      buf, bufLen,
      "https://api.adsb.lol/v2/lat/%g/lon/%g/dist/%g", lat, lon, radiusNm);
  return written > 0 && static_cast<std::size_t>(written) < bufLen;
}

void AdsbPoller::setHttpGet(AdsbHttpGetFn fn, void* user) {
  httpGet_ = fn;
  httpUser_ = user;
}

void AdsbPoller::setActive(bool radarIsActiveScreen) {
  active_ = radarIsActiveScreen;
}

void AdsbPoller::setCenter(double lat, double lon, float rangeStatuteMi) {
  centerLat_ = lat;
  centerLon_ = lon;
  rangeStatuteMi_ = rangeStatuteMi;
}

void AdsbPoller::onTick(uint32_t elapsedMs) {
  if (!active_ || !httpGet_) {
    return;
  }

  pollMs_ += elapsedMs;

  // Prefetch before the interval boundary so async transports can finish by
  // the time the Classic sweep wraps through north (aligned ~10s cadence).
  constexpr uint32_t kPrefetchAtMs =
      kAdsbPollIntervalMs > kAdsbPrefetchLeadMs
          ? kAdsbPollIntervalMs - kAdsbPrefetchLeadMs
          : 0;
  if (pollMs_ < kPrefetchAtMs) {
    return;
  }

  if (tryPollOnce()) {
    pollMs_ = 0;
    return;
  }

  // In-flight or failed attempt: keep retrying until the wait budget expires,
  // then start a fresh interval (keeps last-good traffic on screen).
  if (pollMs_ >= kPrefetchAtMs + kAdsbFetchMaxWaitMs) {
    pollMs_ = 0;
  }
}

bool AdsbPoller::tryPollOnce() {
  char url[160];
  if (!buildAdsbLolUrl(url, sizeof(url), centerLat_, centerLon_,
                       rangeStatuteMi_)) {
    return false;
  }

  std::size_t bodyLen = 0;
  if (!httpGet_(url, body_, sizeof(body_), bodyLen, httpUser_)) {
    return false;
  }
  if (bodyLen >= sizeof(body_)) {
    return false;
  }
  body_[bodyLen] = '\0';

  AircraftList parsed{};
  if (!parseAdsb(body_, parsed)) {
    return false;
  }

  lastGood_ = parsed;
  hasLastGood_ = true;
  hasPending_ = true;
  return true;
}

bool AdsbPoller::takeAircraft(AircraftList& out) {
  if (!hasPending_) {
    return false;
  }
  out = lastGood_;
  hasPending_ = false;
  return true;
}

bool AdsbPoller::hasLastGood() const { return hasLastGood_; }

}  // namespace desk_display
