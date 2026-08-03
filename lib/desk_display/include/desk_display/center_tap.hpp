#pragma once

#include <cstdint>

namespace desk_display {

constexpr uint32_t kCenterTapMaxMs = 400;
constexpr uint32_t kCenterTapRefractoryMs = 80;

class CenterTapDetector {
 public:
  void reset();
  /** Returns true when a short press completes on this call. */
  bool onContact(bool down, uint32_t now_ms);

 private:
  bool down_ = false;
  uint32_t down_at_ms_ = 0;
  uint32_t refractory_until_ms_ = 0;
};

}  // namespace desk_display
