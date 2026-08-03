#include "desk_display/scores_poll.hpp"

#include <cstdio>
#include <cstring>
#include <memory>
#include <new>

namespace desk_display {

bool buildScoresUrl(char* buf, std::size_t bufLen) {
  if (!buf || bufLen == 0) {
    return false;
  }

#if defined(API_BASE_URL)
  const char* const base = API_BASE_URL;
#else
  const char* const base = "https://desk-display-backend.vercel.app";
#endif

  const int written = std::snprintf(buf, bufLen, "%s/api/scores", base);
  return written > 0 && static_cast<std::size_t>(written) < bufLen;
}

void ScoresPoller::setHttpGet(AdsbHttpGetFn fn, void* user) {
  httpGet_ = fn;
  httpUser_ = user;
}

void ScoresPoller::setActive(bool sportsIsActiveScreen) {
  if (sportsIsActiveScreen && !active_) {
    // Became active — fetch on the next tick instead of waiting a full interval.
    pollMs_ = kScoresPollIntervalMs;
  }
  if (!sportsIsActiveScreen) {
    pollMs_ = 0;
  }
  active_ = sportsIsActiveScreen;
}

void ScoresPoller::onTick(uint32_t elapsedMs) {
  if (!active_ || !httpGet_) {
    return;
  }

  pollMs_ += elapsedMs;
  if (pollMs_ < kScoresPollIntervalMs) {
    return;
  }

  if (tryPollOnce()) {
    pollMs_ = 0;
    return;
  }

  // In-flight or failed: retry until the wait budget expires, then start a
  // fresh interval (keeps last-good scores on screen).
  if (pollMs_ >= kScoresPollIntervalMs + kAdsbFetchMaxWaitMs) {
    pollMs_ = 0;
  }
}

bool ScoresPoller::tryPollOnce() {
  char url[160];
  if (!buildScoresUrl(url, sizeof(url))) {
    return false;
  }

  // Heap buffer: keep BSS small so Dial TLS (mbedTLS) can allocate.
  auto body = std::unique_ptr<char[]>(new (std::nothrow) char[kBodyCap]);
  if (!body) {
    return false;
  }

  std::size_t bodyLen = 0;
  if (!httpGet_(url, body.get(), kBodyCap, bodyLen, httpUser_)) {
    return false;
  }
  if (bodyLen >= kBodyCap) {
    return false;
  }
  body[bodyLen] = '\0';

  Scores parsed{};
  if (!parseScores(body.get(), parsed)) {
    return false;
  }

  lastGood_ = parsed;
  hasLastGood_ = true;
  hasPending_ = true;
  return true;
}

bool ScoresPoller::takeScores(Scores& out) {
  if (!hasPending_) {
    return false;
  }
  out = lastGood_;
  hasPending_ = false;
  return true;
}

bool ScoresPoller::hasLastGood() const { return hasLastGood_; }

}  // namespace desk_display
