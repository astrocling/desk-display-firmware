#include "desk_display/map_context_poll.hpp"

#include <cstdio>
#include <cstring>
#include <memory>
#include <new>

namespace desk_display {

bool buildMapContextUrl(char* buf, std::size_t bufLen, double lat, double lon,
                        float radiusMi) {
  if (!buf || bufLen == 0) {
    return false;
  }

#if defined(API_BASE_URL)
  const char* const base = API_BASE_URL;
#else
  const char* const base = "https://desk-display-backend.vercel.app";
#endif

  const int written = std::snprintf(buf, bufLen,
                                    "%s/api/map/context?lat=%g&lon=%g&radiusMi=%g", base, lat,
                                    lon, radiusMi);
  return written > 0 && static_cast<std::size_t>(written) < bufLen;
}

void MapContextPoller::setHttpGet(AdsbHttpGetFn fn, void* user) {
  httpGet_ = fn;
  httpUser_ = user;
}

void MapContextPoller::setActive(bool radarIsActiveScreen) {
  active_ = radarIsActiveScreen;
}

void MapContextPoller::setCenter(double lat, double lon, float rangeMi) {
  if (lat == centerLat_ && lon == centerLon_ && rangeMi == rangeMi_) {
    return;
  }
  centerLat_ = lat;
  centerLon_ = lon;
  rangeMi_ = rangeMi;
  debounceMs_ = 0;
  needsFetch_ = true;
}

void MapContextPoller::onTick(uint32_t elapsedMs) {
  if (!active_ || !httpGet_ || !needsFetch_) {
    return;
  }

  debounceMs_ += elapsedMs;
  if (debounceMs_ < kMapContextDebounceMs) {
    return;
  }

  const uint32_t fetchMs = debounceMs_ - kMapContextDebounceMs;

  switch (tryPollOnce()) {
    case PollAttemptResult::Success:
    case PollAttemptResult::HardFail:
      needsFetch_ = false;
      break;
    case PollAttemptResult::Retry:
      if (fetchMs >= kAdsbFetchMaxWaitMs) {
        needsFetch_ = false;
      }
      break;
  }
}

MapContextPoller::PollAttemptResult MapContextPoller::tryPollOnce() {
  char url[256];
  if (!buildMapContextUrl(url, sizeof(url), centerLat_, centerLon_, rangeMi_)) {
    return PollAttemptResult::HardFail;
  }

  if (!bodyBuf_) {
    bodyBuf_.reset(new (std::nothrow) char[kBodyCap]);
    if (!bodyBuf_) {
      return PollAttemptResult::Retry;
    }
  }

  std::size_t bodyLen = 0;
  if (!httpGet_(url, bodyBuf_.get(), kBodyCap, bodyLen, httpUser_)) {
    return PollAttemptResult::Retry;
  }
  // Async transport reports terminal HTTP failures as true + empty body so we
  // stop Retry-spamming (e.g. map/context 404) instead of hammering for 8s.
  if (bodyLen == 0 || bodyLen >= kBodyCap) {
    return PollAttemptResult::HardFail;
  }
  bodyBuf_[bodyLen] = '\0';

  // Parse straight into the member — MapContext is ~24KB; a stack temporary
  // would overflow the Dial Arduino loop task (even at 48KB with TLS frames).
  if (!parseMapContext(bodyBuf_.get(), lastGood_)) {
    return PollAttemptResult::HardFail;
  }

  hasLastGood_ = true;
  hasPending_ = true;
  return PollAttemptResult::Success;
}

bool MapContextPoller::takeContext(MapContext& out) {
  if (!hasPending_) {
    return false;
  }
  out = lastGood_;
  hasPending_ = false;
  return true;
}

bool MapContextPoller::hasLastGood() const { return hasLastGood_; }

}  // namespace desk_display
