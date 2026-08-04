#include <unity.h>

#include "desk_display/center_tap.hpp"
#include "desk_display/encoder_decode.hpp"
#include "desk_display/touch_gesture.hpp"
#include "desk_display/nav.hpp"
#include "desk_display/nav_status.hpp"

using desk_display::CenterTapDetector;
using desk_display::EncoderDecoder;
using desk_display::NavMode;
using desk_display::Screen;
using desk_display::clampEncoderDelta;
using desk_display::formatNavOverlay;
using desk_display::formatNavSerial;
using desk_display::kCenterTapMaxMs;
using desk_display::TouchGestureDetector;
using desk_display::TouchGestureKind;
using desk_display::kTouchDoubleGapMs;
using desk_display::kTouchLongPressMs;
using desk_display::kTouchTapMaxMs;
using desk_display::kEncoderDeltaClamp;
using desk_display::navModeUpper;
using desk_display::screenTitleUpper;

void test_encoder_forward_quarter_turns(void) {
  EncoderDecoder d;
  // Idle 00; CW gray: 00 → 01 → 11 → 10 → 00 = +1 per full cycle of 4 edges.
  // AB state is encoded as (A << 1) | B.
  int8_t sum = 0;
  sum += d.update(false, false);
  sum += d.update(false, true);   // 00 → 01
  sum += d.update(true, true);    // 01 → 11
  sum += d.update(true, false);   // 11 → 10
  sum += d.update(false, false);  // 10 → 00
  TEST_ASSERT_EQUAL_INT8(1, sum);
}

void test_encoder_backward(void) {
  EncoderDecoder d;
  int8_t sum = 0;
  sum += d.update(false, false);
  sum += d.update(true, false);   // 00 → 10
  sum += d.update(true, true);    // 10 → 11
  sum += d.update(false, true);   // 11 → 01
  sum += d.update(false, false);  // 01 → 00
  TEST_ASSERT_EQUAL_INT8(-1, sum);
}

void test_encoder_clamp(void) {
  TEST_ASSERT_EQUAL_INT8(kEncoderDeltaClamp, clampEncoderDelta(100));
  TEST_ASSERT_EQUAL_INT8(static_cast<int8_t>(-kEncoderDeltaClamp),
                         clampEncoderDelta(-100));
  TEST_ASSERT_EQUAL_INT8(3, clampEncoderDelta(3));
}

void test_center_tap_short_press(void) {
  CenterTapDetector t;
  TEST_ASSERT_FALSE(t.onContact(true, 1000));
  TEST_ASSERT_TRUE(t.onContact(false, 1000 + 200));
}

void test_center_tap_long_hold_ignored(void) {
  CenterTapDetector t;
  TEST_ASSERT_FALSE(t.onContact(true, 1000));
  TEST_ASSERT_FALSE(t.onContact(false, 1000 + kCenterTapMaxMs + 1));
}

void test_center_tap_refractory(void) {
  CenterTapDetector t;
  TEST_ASSERT_FALSE(t.onContact(true, 1000));
  TEST_ASSERT_TRUE(t.onContact(false, 1100));
  TEST_ASSERT_FALSE(t.onContact(true, 1120));
  TEST_ASSERT_FALSE(t.onContact(false, 1200));  // still in refractory from 1100
}

void test_touch_single_tap_after_gap(void) {
  TouchGestureDetector d;
  TEST_ASSERT_EQUAL(TouchGestureKind::None, d.update(true, 10, 20, 1000).kind);
  TEST_ASSERT_EQUAL(TouchGestureKind::None, d.update(false, 10, 20, 1100).kind);
  // Still inside double window — no Tap yet
  TEST_ASSERT_EQUAL(TouchGestureKind::None,
                    d.update(false, 10, 20, 1100 + kTouchDoubleGapMs - 1).kind);
  const auto tap = d.update(false, 10, 20, 1100 + kTouchDoubleGapMs);
  TEST_ASSERT_EQUAL(TouchGestureKind::Tap, tap.kind);
  TEST_ASSERT_EQUAL_INT16(10, tap.x);
  TEST_ASSERT_EQUAL_INT16(20, tap.y);
}

void test_touch_double_tap(void) {
  TouchGestureDetector d;
  d.update(true, 1, 1, 1000);
  d.update(false, 1, 1, 1100);
  d.update(true, 2, 3, 1200);
  const auto dbl = d.update(false, 2, 3, 1300);
  TEST_ASSERT_EQUAL(TouchGestureKind::DoubleTap, dbl.kind);
  TEST_ASSERT_EQUAL_INT16(2, dbl.x);
  TEST_ASSERT_EQUAL_INT16(3, dbl.y);
  // Pending single must not fire later
  TEST_ASSERT_EQUAL(TouchGestureKind::None,
                    d.update(false, 0, 0, 1300 + kTouchDoubleGapMs + 50).kind);
}

void test_touch_long_press(void) {
  TouchGestureDetector d;
  TEST_ASSERT_EQUAL(TouchGestureKind::None, d.update(true, 5, 6, 1000).kind);
  TEST_ASSERT_EQUAL(TouchGestureKind::None,
                    d.update(true, 5, 6, 1000 + kTouchLongPressMs - 1).kind);
  const auto lp = d.update(true, 5, 6, 1000 + kTouchLongPressMs);
  TEST_ASSERT_EQUAL(TouchGestureKind::LongPress, lp.kind);
  TEST_ASSERT_EQUAL_INT16(5, lp.x);
  TEST_ASSERT_EQUAL_INT16(6, lp.y);
  // Up after long must not emit Tap
  TEST_ASSERT_EQUAL(TouchGestureKind::None, d.update(false, 5, 6, 2000).kind);
  TEST_ASSERT_EQUAL(TouchGestureKind::None,
                    d.update(false, 5, 6, 2000 + kTouchDoubleGapMs).kind);
}

void test_touch_hold_too_long_for_tap_without_long(void) {
  // Hold past tap max but release before long → neither tap nor long
  TouchGestureDetector d;
  d.update(true, 0, 0, 1000);
  TEST_ASSERT_EQUAL(TouchGestureKind::None,
                    d.update(false, 0, 0, 1000 + kTouchTapMaxMs + 1).kind);
  TEST_ASSERT_EQUAL(TouchGestureKind::None,
                    d.update(false, 0, 0, 1000 + kTouchTapMaxMs + 1 + kTouchDoubleGapMs)
                        .kind);
}

void test_nav_status_strings(void) {
  TEST_ASSERT_EQUAL_STRING("CLOCK", screenTitleUpper(Screen::Clock));
  TEST_ASSERT_EQUAL_STRING("WEATHER", screenTitleUpper(Screen::Weather));
  TEST_ASSERT_EQUAL_STRING("FOCUSED", navModeUpper(NavMode::Focused));
  TEST_ASSERT_EQUAL_STRING("CAROUSEL", navModeUpper(NavMode::Carousel));

  char overlay[32];
  formatNavOverlay(NavMode::Focused, Screen::Clock, overlay, sizeof(overlay));
  TEST_ASSERT_EQUAL_STRING("FOCUSED CLOCK", overlay);
  formatNavOverlay(NavMode::Carousel, Screen::Weather, overlay, sizeof(overlay));
  TEST_ASSERT_EQUAL_STRING("CAROUSEL WEATHER", overlay);

  char serial[40];
  formatNavSerial(NavMode::Focused, Screen::Clock, serial, sizeof(serial));
  TEST_ASSERT_EQUAL_STRING("nav: Focused Clock", serial);
  formatNavSerial(NavMode::Carousel, Screen::Weather, serial, sizeof(serial));
  TEST_ASSERT_EQUAL_STRING("nav: Carousel Weather", serial);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_encoder_forward_quarter_turns);
  RUN_TEST(test_encoder_backward);
  RUN_TEST(test_encoder_clamp);
  RUN_TEST(test_center_tap_short_press);
  RUN_TEST(test_center_tap_long_hold_ignored);
  RUN_TEST(test_center_tap_refractory);
  RUN_TEST(test_touch_single_tap_after_gap);
  RUN_TEST(test_touch_double_tap);
  RUN_TEST(test_touch_long_press);
  RUN_TEST(test_touch_hold_too_long_for_tap_without_long);
  RUN_TEST(test_nav_status_strings);
  return UNITY_END();
}
