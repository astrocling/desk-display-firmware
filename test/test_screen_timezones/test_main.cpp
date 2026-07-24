#include <unity.h>

#include <cstring>

#include "desk_display/scrub.hpp"
#include "desk_display/screen_timezones.hpp"
#include "desk_display/timezones.hpp"
#include "fixture_loader.hpp"

using namespace desk_display;

static char g_buf[64 * 1024];

static int g_injectHours[kTimezoneBoardRows];

static int injectLocalHour(std::size_t row, std::int64_t /*scrubbed*/,
                           void* /*user*/) {
  if (row >= kTimezoneBoardRows) {
    return 0;
  }
  return g_injectHours[row];
}

void test_board_fixed_iana_and_labels(void) {
  TEST_ASSERT_EQUAL_UINT(7, kTimezoneBoardRows);
  TEST_ASSERT_EQUAL_STRING("America/New_York", timezoneBoardIana(0));
  TEST_ASSERT_EQUAL_STRING("America/Chicago", timezoneBoardIana(1));
  TEST_ASSERT_EQUAL_STRING("America/Los_Angeles", timezoneBoardIana(2));
  TEST_ASSERT_EQUAL_STRING("Etc/GMT", timezoneBoardIana(3));
  TEST_ASSERT_EQUAL_STRING("Europe/Rome", timezoneBoardIana(4));
  TEST_ASSERT_EQUAL_STRING("Europe/Kyiv", timezoneBoardIana(5));
  TEST_ASSERT_EQUAL_STRING("Europe/Chisinau", timezoneBoardIana(6));

  TEST_ASSERT_EQUAL_STRING("Eastern", timezoneBoardLabel(0));
  TEST_ASSERT_EQUAL_STRING("Las Vegas", timezoneBoardLabel(2));
  TEST_ASSERT_EQUAL_STRING("Moldova", timezoneBoardLabel(6));
}

void test_default_anchor_eastern(void) {
  ScreenTimezones board;
  TEST_ASSERT_EQUAL_UINT(0, board.anchorIndex());
  TEST_ASSERT_EQUAL(0, board.scrubSteps());
  const TimezoneBoardView v = board.view();
  TEST_ASSERT_TRUE(v.rows[0].isAnchor);
  for (std::size_t i = 1; i < kTimezoneBoardRows; ++i) {
    TEST_ASSERT_FALSE(v.rows[i].isAnchor);
  }
}

void test_scrub_steps_update_displayed_times(void) {
  ScreenTimezones board;
  // 12:00 UTC → Eastern 8:00 AM with fixed -4 offset
  board.setLiveUnix(12 * 3600);
  TimezoneBoardView v = board.view();
  TEST_ASSERT_EQUAL_STRING("8:00 AM", v.rows[0].timeText);
  TEST_ASSERT_EQUAL_STRING("7:00 AM", v.rows[1].timeText);  // Chicago -5
  TEST_ASSERT_EQUAL_STRING("5:00 AM", v.rows[2].timeText);  // LA -7
  TEST_ASSERT_EQUAL_STRING("12:00 PM", v.rows[3].timeText);  // GMT
  TEST_ASSERT_EQUAL_STRING("2:00 PM", v.rows[4].timeText);  // Rome +2

  board.onRotate(1);
  TEST_ASSERT_EQUAL(1, board.scrubSteps());
  TEST_ASSERT_EQUAL_INT64(applyScrubOffset(12 * 3600, 1), board.scrubbedUnix());
  v = board.view();
  TEST_ASSERT_EQUAL_STRING("9:00 AM", v.rows[0].timeText);
  TEST_ASSERT_EQUAL_STRING("1:00 PM", v.rows[3].timeText);

  board.onRotate(-3);
  TEST_ASSERT_EQUAL(-2, board.scrubSteps());
  v = board.view();
  TEST_ASSERT_EQUAL_STRING("6:00 AM", v.rows[0].timeText);
}

void test_anchor_change_on_tap(void) {
  ScreenTimezones board;
  board.setLiveUnix(12 * 3600);
  board.onRotate(2);
  board.onTapRow(3);  // GMT
  TEST_ASSERT_EQUAL_UINT(3, board.anchorIndex());
  TEST_ASSERT_EQUAL(2, board.scrubSteps());  // scrub preserved
  const TimezoneBoardView v = board.view();
  TEST_ASSERT_TRUE(v.rows[3].isAnchor);
  TEST_ASSERT_FALSE(v.rows[0].isAnchor);
  // Absolute times unchanged by anchor alone
  TEST_ASSERT_EQUAL_STRING("2:00 PM", v.rows[3].timeText);
}

void test_double_tap_resets_scrub_and_anchor(void) {
  ScreenTimezones board;
  board.setLiveUnix(12 * 3600);
  board.onTapRow(5);
  board.onRotate(4);
  board.onDoubleTap();
  TEST_ASSERT_EQUAL_UINT(0, board.anchorIndex());
  TEST_ASSERT_EQUAL(0, board.scrubSteps());
  TEST_ASSERT_EQUAL_INT64(12 * 3600, board.scrubbedUnix());
}

void test_long_press_resets_scrub_and_anchor(void) {
  ScreenTimezones board;
  board.onTapRow(2);
  board.onRotate(-5);
  board.onLongPress();
  TEST_ASSERT_EQUAL_UINT(0, board.anchorIndex());
  TEST_ASSERT_EQUAL(0, board.scrubSteps());
}

void test_status_icons_from_injected_local_hours(void) {
  ScreenTimezones board;
  board.setLiveUnix(0);
  // Working / Awake / Night mix
  g_injectHours[0] = 10;  // Working
  g_injectHours[1] = 8;   // Awake
  g_injectHours[2] = 22;  // Night
  g_injectHours[3] = 17;  // Awake
  g_injectHours[4] = 9;   // Working
  g_injectHours[5] = 6;   // Night
  g_injectHours[6] = 20;  // Awake
  board.setLocalHourFn(injectLocalHour);

  const TimezoneBoardView v = board.view();
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Working),
                    static_cast<int>(v.rows[0].status));
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Awake),
                    static_cast<int>(v.rows[1].status));
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Night),
                    static_cast<int>(v.rows[2].status));
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Awake),
                    static_cast<int>(v.rows[3].status));
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Working),
                    static_cast<int>(v.rows[4].status));
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Night),
                    static_cast<int>(v.rows[5].status));
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Awake),
                    static_cast<int>(v.rows[6].status));
}

void test_status_from_offset_table(void) {
  ScreenTimezones board;
  // 15:00 UTC → Eastern 11:00 Working, LA 08:00 Awake, Rome 17:00 Awake
  board.setLiveUnix(15 * 3600);
  const TimezoneBoardView v = board.view();
  TEST_ASSERT_EQUAL(11, v.rows[0].localHour);
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Working),
                    static_cast<int>(v.rows[0].status));
  TEST_ASSERT_EQUAL(8, v.rows[2].localHour);
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Awake),
                    static_cast<int>(v.rows[2].status));
  TEST_ASSERT_EQUAL(17, v.rows[4].localHour);
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Awake),
                    static_cast<int>(v.rows[4].status));
}

void test_idle_settle_resets_scrub(void) {
  ScreenTimezones board;
  board.setLiveUnix(12 * 3600);
  board.onTapRow(5);
  board.onRotate(4);
  board.onIdleSettle();
  TEST_ASSERT_EQUAL(0, board.scrubSteps());
  TEST_ASSERT_EQUAL_UINT(0, board.anchorIndex());
}

void test_sun_times_fixture_daylight_override(void) {
  TEST_ASSERT_TRUE(loadFixture("timezones.json", g_buf, sizeof(g_buf)));
  Timezones tz{};
  TEST_ASSERT_TRUE(parseTimezones(g_buf, tz));

  ScreenTimezones board;
  board.setSunTimes(tz);
  // Inject Working-hour local times, but pick a unix before sunrise for NY
  // (sunrise 2026-07-23T09:43:11+00:00). Force Night via daylight=false.
  g_injectHours[0] = 12;
  for (std::size_t i = 1; i < kTimezoneBoardRows; ++i) {
    g_injectHours[i] = 12;
  }
  board.setLocalHourFn(injectLocalHour);
  board.setLiveUnix(1784790000);  // ~2026-07-23 07:00 UTC, before NY sunrise

  const TimezoneBoardView v = board.view();
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Night),
                    static_cast<int>(v.rows[0].status));
}

void setUp(void) {
  for (std::size_t i = 0; i < kTimezoneBoardRows; ++i) {
    g_injectHours[i] = 12;
  }
}

void tearDown(void) {}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_board_fixed_iana_and_labels);
  RUN_TEST(test_default_anchor_eastern);
  RUN_TEST(test_scrub_steps_update_displayed_times);
  RUN_TEST(test_anchor_change_on_tap);
  RUN_TEST(test_double_tap_resets_scrub_and_anchor);
  RUN_TEST(test_long_press_resets_scrub_and_anchor);
  RUN_TEST(test_status_icons_from_injected_local_hours);
  RUN_TEST(test_status_from_offset_table);
  RUN_TEST(test_sun_times_fixture_daylight_override);
  RUN_TEST(test_idle_settle_resets_scrub);
  return UNITY_END();
}
