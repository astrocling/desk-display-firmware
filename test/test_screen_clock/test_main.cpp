#include <unity.h>

#include <cmath>
#include <cstring>

#include "desk_display/screen_clock.hpp"

using namespace desk_display;

void test_clock_default_hint_and_epoch_date(void) {
  ScreenClock clock;
  TEST_ASSERT_FALSE(clock.timezoneBoardHint());
  TEST_ASSERT_EQUAL_STRING("Thu, Jan 1", clock.dateText());

  const ClockView v = clock.view();
  TEST_ASSERT_FALSE(v.timezoneBoardHint);
  TEST_ASSERT_EQUAL_STRING("Thu, Jan 1", v.dateText);
  TEST_ASSERT_EQUAL(1970, v.year);
  TEST_ASSERT_EQUAL(1, v.month);
  TEST_ASSERT_EQUAL(1, v.day);
}

void test_clock_date_formatting_set_time(void) {
  ScreenClock clock;
  clock.setTime(2026, 7, 23, 14, 30, 45);
  TEST_ASSERT_EQUAL_STRING("Thu, Jul 23", clock.dateText());
  TEST_ASSERT_EQUAL(14, clock.hour());
  TEST_ASSERT_EQUAL(30, clock.minute());
  TEST_ASSERT_EQUAL(45, clock.second());

  const ClockView v = clock.view();
  TEST_ASSERT_EQUAL_STRING("Thu, Jul 23", v.dateText);
  TEST_ASSERT_EQUAL(2026, v.year);
  TEST_ASSERT_EQUAL(7, v.month);
  TEST_ASSERT_EQUAL(23, v.day);
  TEST_ASSERT_EQUAL(14, v.hour);
  TEST_ASSERT_EQUAL(30, v.minute);
  TEST_ASSERT_EQUAL(45, v.second);
}

void test_clock_date_formatting_other_weekdays(void) {
  ScreenClock clock;
  clock.setTime(2026, 7, 24, 0, 0, 0);
  TEST_ASSERT_EQUAL_STRING("Fri, Jul 24", clock.dateText());
  clock.setTime(2026, 12, 25, 12, 0, 0);
  TEST_ASSERT_EQUAL_STRING("Fri, Dec 25", clock.dateText());
}

void test_clock_hand_angles(void) {
  ScreenClock clock;
  clock.setTime(2026, 7, 23, 3, 0, 0);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f, clock.hourHandAngleDeg());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, clock.minuteHandAngleDeg());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, clock.secondHandAngleDeg());

  clock.setTime(2026, 7, 23, 0, 30, 30);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 183.0f, clock.minuteHandAngleDeg());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 180.0f, clock.secondHandAngleDeg());
  // hour: 0.5 * 30 + (0.5/60)*30 = 15 + 0.25 = 15.25
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 15.25f, clock.hourHandAngleDeg());
}

void test_clock_timezone_hint_toggle(void) {
  ScreenClock clock;
  clock.setTimezoneBoardHint(false);
  TEST_ASSERT_FALSE(clock.view().timezoneBoardHint);
  clock.setTimezoneBoardHint(true);
  TEST_ASSERT_TRUE(clock.view().timezoneBoardHint);
}

void test_clock_set_unix_utc_shows_home_eastern(void) {
  ScreenClock clock;
  // 2026-07-23 16:00:00 UTC → 12:00:00 Eastern (UTC-4, EDT)
  clock.setUnixUtc(1784822400);
  TEST_ASSERT_EQUAL(2026, clock.view().year);
  TEST_ASSERT_EQUAL(7, clock.view().month);
  TEST_ASSERT_EQUAL(23, clock.view().day);
  TEST_ASSERT_EQUAL(12, clock.view().hour);
  TEST_ASSERT_EQUAL(0, clock.view().minute);
  TEST_ASSERT_EQUAL(0, clock.view().second);
  TEST_ASSERT_EQUAL_STRING("Thu, Jul 23", clock.dateText());

  // 2026-07-24 02:30:00 UTC → 2026-07-23 22:30:00 Eastern (date rolls back)
  clock.setUnixUtc(1784822400 + 10 * 3600 + 30 * 60);
  TEST_ASSERT_EQUAL(2026, clock.view().year);
  TEST_ASSERT_EQUAL(7, clock.view().month);
  TEST_ASSERT_EQUAL(23, clock.view().day);
  TEST_ASSERT_EQUAL(22, clock.view().hour);
  TEST_ASSERT_EQUAL(30, clock.view().minute);
  TEST_ASSERT_EQUAL(0, clock.view().second);
  TEST_ASSERT_EQUAL_STRING("Thu, Jul 23", clock.dateText());
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_clock_default_hint_and_epoch_date);
  RUN_TEST(test_clock_date_formatting_set_time);
  RUN_TEST(test_clock_date_formatting_other_weekdays);
  RUN_TEST(test_clock_hand_angles);
  RUN_TEST(test_clock_timezone_hint_toggle);
  RUN_TEST(test_clock_set_unix_utc_shows_home_eastern);
  return UNITY_END();
}
