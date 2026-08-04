#include "desk_display/touch_gesture.hpp"

namespace desk_display {

void TouchGestureDetector::reset() {
  down_ = false;
  long_fired_ = false;
  suppress_up_ = false;
  down_at_ms_ = 0;
  down_x_ = 0;
  down_y_ = 0;
  pending_tap_ = false;
  pending_deadline_ms_ = 0;
  pending_x_ = 0;
  pending_y_ = 0;
  refractory_until_ms_ = 0;
}

TouchGesture TouchGestureDetector::tick(uint32_t now_ms) {
  TouchGesture out{};
  if (pending_tap_ && now_ms >= pending_deadline_ms_ && !down_) {
    pending_tap_ = false;
    out = {TouchGestureKind::Tap, pending_x_, pending_y_};
    refractory_until_ms_ = now_ms + kTouchRefractoryMs;
  }
  return out;
}

TouchGesture TouchGestureDetector::update(bool down, int16_t x, int16_t y,
                                          uint32_t now_ms) {
  TouchGesture out = tick(now_ms);

  if (down) {
    if (!down_) {
      if (now_ms < refractory_until_ms_) {
        return out;
      }
      down_ = true;
      long_fired_ = false;
      suppress_up_ = false;
      down_at_ms_ = now_ms;
      down_x_ = x;
      down_y_ = y;
      return out;
    }
    if (!long_fired_ && (now_ms - down_at_ms_) >= kTouchLongPressMs) {
      long_fired_ = true;
      suppress_up_ = true;
      pending_tap_ = false;
      out = {TouchGestureKind::LongPress, x, y};
      refractory_until_ms_ = now_ms + kTouchRefractoryMs;
    }
    return out;
  }

  if (!down_) {
    return out;
  }
  down_ = false;
  if (suppress_up_ || long_fired_) {
    suppress_up_ = false;
    return out;
  }
  const uint32_t held = now_ms - down_at_ms_;
  if (held > kTouchTapMaxMs) {
    return out;
  }
  if (pending_tap_) {
    pending_tap_ = false;
    out = {TouchGestureKind::DoubleTap, x, y};
    refractory_until_ms_ = now_ms + kTouchRefractoryMs;
    return out;
  }
  pending_tap_ = true;
  pending_x_ = x;
  pending_y_ = y;
  pending_deadline_ms_ = now_ms + kTouchDoubleGapMs;
  return out;
}

}  // namespace desk_display
