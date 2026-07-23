#include <unity.h>

#include <cmath>

#include "desk_display/radar.hpp"
#include "desk_display/scrub.hpp"
#include "desk_display/timezone_status.hpp"
#include "desk_display/weather_icons.hpp"

using namespace desk_display;

void test_timezone_row_status_windows(void) {
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Night),
                    static_cast<int>(timezoneRowStatus(6)));
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Awake),
                    static_cast<int>(timezoneRowStatus(7)));
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Awake),
                    static_cast<int>(timezoneRowStatus(8)));
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Working),
                    static_cast<int>(timezoneRowStatus(9)));
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Working),
                    static_cast<int>(timezoneRowStatus(16)));
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Awake),
                    static_cast<int>(timezoneRowStatus(17)));
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Awake),
                    static_cast<int>(timezoneRowStatus(20)));
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Night),
                    static_cast<int>(timezoneRowStatus(21)));
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Night),
                    static_cast<int>(timezoneRowStatus(0)));
}

void test_timezone_row_status_daylight_override(void) {
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Working),
                    static_cast<int>(timezoneRowStatus(12, true)));
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Night),
                    static_cast<int>(timezoneRowStatus(12, false)));
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Awake),
                    static_cast<int>(timezoneRowStatus(8, true)));
  TEST_ASSERT_EQUAL(static_cast<int>(TzRowStatus::Night),
                    static_cast<int>(timezoneRowStatus(8, false)));
}

void test_scrub_offset_fifteen_minutes(void) {
  const std::int64_t anchor = 1'000'000;
  TEST_ASSERT_EQUAL_INT64(anchor + 15 * 60, applyScrubOffset(anchor, 1));
  TEST_ASSERT_EQUAL_INT64(anchor - 15 * 60, applyScrubOffset(anchor, -1));
  TEST_ASSERT_EQUAL_INT64(anchor + 4 * 15 * 60, applyScrubOffset(anchor, 4));
  TEST_ASSERT_EQUAL_INT64(anchor, applyScrubOffset(anchor, 0));
}

void test_wmo_to_icon(void) {
  TEST_ASSERT_EQUAL(static_cast<int>(WeatherIconId::Clear),
                    static_cast<int>(wmoToIcon(0)));
  TEST_ASSERT_EQUAL(static_cast<int>(WeatherIconId::MostlyClear),
                    static_cast<int>(wmoToIcon(1)));
  TEST_ASSERT_EQUAL(static_cast<int>(WeatherIconId::Cloudy),
                    static_cast<int>(wmoToIcon(3)));
  TEST_ASSERT_EQUAL(static_cast<int>(WeatherIconId::Fog),
                    static_cast<int>(wmoToIcon(45)));
  TEST_ASSERT_EQUAL(static_cast<int>(WeatherIconId::Drizzle),
                    static_cast<int>(wmoToIcon(51)));
  TEST_ASSERT_EQUAL(static_cast<int>(WeatherIconId::Rain),
                    static_cast<int>(wmoToIcon(61)));
  TEST_ASSERT_EQUAL(static_cast<int>(WeatherIconId::Snow),
                    static_cast<int>(wmoToIcon(71)));
  TEST_ASSERT_EQUAL(static_cast<int>(WeatherIconId::Showers),
                    static_cast<int>(wmoToIcon(80)));
  TEST_ASSERT_EQUAL(static_cast<int>(WeatherIconId::Thunderstorm),
                    static_cast<int>(wmoToIcon(95)));
  TEST_ASSERT_EQUAL(static_cast<int>(WeatherIconId::Unknown),
                    static_cast<int>(wmoToIcon(999)));
}

void test_radar_clamp_range(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, clampRadarRangeMiles(1.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, clampRadarRangeMiles(100.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.0f, clampRadarRangeMiles(25.0f));
}

void test_radar_offset_and_distance(void) {
  float x = 0.0f;
  float y = 0.0f;
  // ~1 degree latitude ≈ 69.1 miles north
  aircraftOffsetMiles(40.0, -84.0, 41.0, -84.0, x, y);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, x);
  TEST_ASSERT_TRUE(y > 60.0f && y < 75.0f);

  // East offset should be positive X
  aircraftOffsetMiles(40.0, -84.0, 40.0, -83.0, x, y);
  TEST_ASSERT_TRUE(x > 40.0f);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, y);

  const float d = distanceMiles(40.03353, -84.19588, 40.03353, -84.19588);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, d);

  const float d2 = distanceMiles(40.0, -84.0, 40.1, -84.0);
  TEST_ASSERT_TRUE(d2 > 5.0f && d2 < 10.0f);
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_timezone_row_status_windows);
  RUN_TEST(test_timezone_row_status_daylight_override);
  RUN_TEST(test_scrub_offset_fifteen_minutes);
  RUN_TEST(test_wmo_to_icon);
  RUN_TEST(test_radar_clamp_range);
  RUN_TEST(test_radar_offset_and_distance);
  return UNITY_END();
}
