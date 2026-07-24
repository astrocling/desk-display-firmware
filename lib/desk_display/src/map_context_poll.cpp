#include "desk_display/map_context_poll.hpp"

#include <cstdio>
#include <cstring>

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

  if (tryPollOnce()) {
    needsFetch_ = false;
  }
}

bool MapContextPoller::tryPollOnce() {
  char url[256];
  if (!buildMapContextUrl(url, sizeof(url), centerLat_, centerLon_, rangeMi_)) {
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

  MapContext parsed{};
  if (!parseMapContext(body_, parsed)) {
    return false;
  }

  lastGood_ = parsed;
  hasLastGood_ = true;
  hasPending_ = true;
  return true;
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
