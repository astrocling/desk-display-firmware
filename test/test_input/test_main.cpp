#include <unity.h>

#include "desk_display/center_tap.hpp"
#include "desk_display/encoder_decode.hpp"
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
  RUN_TEST(test_nav_status_strings);
  return UNITY_END();
}
