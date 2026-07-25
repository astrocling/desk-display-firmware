#pragma once

#include "desk_display/adsb_poll.hpp"
#include "desk_display/scores.hpp"

#include <cstddef>
#include <cstdint>

namespace desk_display {

/** Cadence between successful binds while Sports is the active screen. */
constexpr uint32_t kScoresPollIntervalMs = 30000;

bool buildScoresUrl(char* buf, std::size_t bufLen);

class ScoresPoller {
 public:
  void setHttpGet(AdsbHttpGetFn fn, void* user);
  void setActive(bool sportsIsActiveScreen);
  void onTick(uint32_t elapsedMs);
  bool takeScores(Scores& out);  // true once per success until consumed
  bool hasLastGood() const;

 private:
  bool tryPollOnce();

  AdsbHttpGetFn httpGet_{nullptr};
  void* httpUser_{nullptr};
  bool active_{false};
  uint32_t pollMs_{0};
  bool hasLastGood_{false};
  bool hasPending_{false};
  Scores lastGood_{};
  static constexpr std::size_t kBodyCap = 64 * 1024;
  char body_[kBodyCap]{};
};

}  // namespace desk_display
