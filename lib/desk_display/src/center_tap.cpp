#include "desk_display/center_tap.hpp"

namespace desk_display {

void CenterTapDetector::reset() {
  down_ = false;
  down_at_ms_ = 0;
  refractory_until_ms_ = 0;
}

bool CenterTapDetector::onContact(bool down, uint32_t now_ms) {
  if (down) {
    if (now_ms < refractory_until_ms_) {
      return false;
    }
    down_ = true;
    down_at_ms_ = now_ms;
    return false;
  }

  if (!down_) {
    return false;
  }

  down_ = false;
  const uint32_t held_ms = now_ms - down_at_ms_;
  if (held_ms > kCenterTapMaxMs) {
    return false;
  }

  refractory_until_ms_ = now_ms + kCenterTapRefractoryMs;
  return true;
}

}  // namespace desk_display
