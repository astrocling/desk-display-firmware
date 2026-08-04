#pragma once

#include <cstdint>

namespace desk_display {

enum class TouchGestureKind : uint8_t { None, Tap, DoubleTap, LongPress };

struct TouchGesture {
  TouchGestureKind kind = TouchGestureKind::None;
  int16_t x = 0;
  int16_t y = 0;
};

constexpr uint32_t kTouchTapMaxMs = 400;
constexpr uint32_t kTouchDoubleGapMs = 350;
constexpr uint32_t kTouchLongPressMs = 600;
constexpr uint32_t kTouchRefractoryMs = 80;

class TouchGestureDetector {
 public:
  void reset();
  TouchGesture update(bool down, int16_t x, int16_t y, uint32_t now_ms);

 private:
  bool down_ = false;
  bool long_fired_ = false;
  bool suppress_up_ = false;
  uint32_t down_at_ms_ = 0;
  int16_t down_x_ = 0;
  int16_t down_y_ = 0;
  bool pending_tap_ = false;
  uint32_t pending_deadline_ms_ = 0;
  int16_t pending_x_ = 0;
  int16_t pending_y_ = 0;
  uint32_t refractory_until_ms_ = 0;
};

}  // namespace desk_display
