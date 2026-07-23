#include <unity.h>

#include "desk_display/screen_weather.hpp"
#include "desk_display/weather.hpp"
#include "fixture_loader.hpp"

using namespace desk_display;

static char g_buf[256 * 1024];
static WeatherScreen g_screen;

void setUp(void) { g_screen.clear(); }

void tearDown(void) {}

void test_unbound_is_not_ready(void) {
  TEST_ASSERT_FALSE(g_screen.ready());
  TEST_ASSERT_TRUE(g_screen.notReady());
  const WeatherScreenView v = g_screen.view();
  TEST_ASSERT_FALSE(v.ready);
  TEST_ASSERT_FALSE(v.alertBadge);
  TEST_ASSERT_EQUAL(kWeatherScrubNow, v.scrubIndex);
}

void test_bind_fixture_display_state(void) {
  TEST_ASSERT_TRUE_MESSAGE(loadFixture("weather.json", g_buf, sizeof(g_buf)),
                           "load weather.json");

  Weather w{};
  TEST_ASSERT_TRUE(parseWeather(g_buf, w));
  g_screen.bind(w);

  TEST_ASSERT_TRUE(g_screen.ready());
  const WeatherScreenView v = g_screen.view();
  TEST_ASSERT_TRUE(v.ready);
  TEST_ASSERT_TRUE(v.showingNow);
  TEST_ASSERT_EQUAL(kWeatherScrubNow, v.scrubIndex);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, w.currentTemp, v.displayTemp);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, w.currentFeelsLike, v.feelsLike);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, w.todayHigh, v.todayHigh);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, w.todayLow, v.todayLow);
  TEST_ASSERT_EQUAL(w.currentCode, v.weatherCode);
  TEST_ASSERT_EQUAL(static_cast<int>(wmoToIcon(w.currentCode)),
                    static_cast<int>(v.icon));
  TEST_ASSERT_TRUE(v.hourlyCount > 0);
  TEST_ASSERT_EQUAL(w.hourlyCount, v.hourlyCount);
  TEST_ASSERT_FALSE(v.alertBadge);
  TEST_ASSERT_FALSE(v.alertDetailOpen);
}

void test_scrub_hourly_updates_center(void) {
  TEST_ASSERT_TRUE(loadFixture("weather.json", g_buf, sizeof(g_buf)));
  Weather w{};
  TEST_ASSERT_TRUE(parseWeather(g_buf, w));
  TEST_ASSERT_TRUE(w.hourlyCount >= 2);
  g_screen.bind(w);

  g_screen.onRotate(1);
  TEST_ASSERT_EQUAL(0, g_screen.scrubIndex());
  WeatherScreenView v = g_screen.view();
  TEST_ASSERT_FALSE(v.showingNow);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, w.hourly[0].temp, v.displayTemp);
  TEST_ASSERT_EQUAL(w.hourly[0].code, v.weatherCode);
  TEST_ASSERT_EQUAL(static_cast<int>(wmoToIcon(w.hourly[0].code)),
                    static_cast<int>(v.icon));

  g_screen.onRotate(1);
  TEST_ASSERT_EQUAL(1, g_screen.scrubIndex());
  v = g_screen.view();
  TEST_ASSERT_FLOAT_WITHIN(0.05f, w.hourly[1].temp, v.displayTemp);
  TEST_ASSERT_EQUAL(w.hourly[1].code, v.weatherCode);

  // Clamp at end of strip
  g_screen.onRotate(static_cast<int>(w.hourlyCount) + 10);
  TEST_ASSERT_EQUAL(static_cast<int>(w.hourlyCount) - 1, g_screen.scrubIndex());
}

void test_snap_to_now(void) {
  TEST_ASSERT_TRUE(loadFixture("weather.json", g_buf, sizeof(g_buf)));
  Weather w{};
  TEST_ASSERT_TRUE(parseWeather(g_buf, w));
  g_screen.bind(w);

  g_screen.onRotate(3);
  TEST_ASSERT_EQUAL(2, g_screen.scrubIndex());
  TEST_ASSERT_FALSE(g_screen.view().showingNow);

  g_screen.snapToNow();
  TEST_ASSERT_EQUAL(kWeatherScrubNow, g_screen.scrubIndex());
  const WeatherScreenView v = g_screen.view();
  TEST_ASSERT_TRUE(v.showingNow);
  TEST_ASSERT_FLOAT_WITHIN(0.05f, w.currentTemp, v.displayTemp);
  TEST_ASSERT_EQUAL(w.currentCode, v.weatherCode);
}

void test_scrub_backward_to_now(void) {
  TEST_ASSERT_TRUE(loadFixture("weather.json", g_buf, sizeof(g_buf)));
  Weather w{};
  TEST_ASSERT_TRUE(parseWeather(g_buf, w));
  g_screen.bind(w);

  g_screen.onRotate(2);
  g_screen.onRotate(-1);
  TEST_ASSERT_EQUAL(0, g_screen.scrubIndex());
  g_screen.onRotate(-1);
  TEST_ASSERT_EQUAL(kWeatherScrubNow, g_screen.scrubIndex());
  g_screen.onRotate(-5);
  TEST_ASSERT_EQUAL(kWeatherScrubNow, g_screen.scrubIndex());
}

void test_alert_absent_on_fixture(void) {
  TEST_ASSERT_TRUE(loadFixture("weather.json", g_buf, sizeof(g_buf)));
  Weather w{};
  TEST_ASSERT_TRUE(parseWeather(g_buf, w));
  g_screen.bind(w);

  TEST_ASSERT_FALSE(g_screen.view().alertBadge);
  TEST_ASSERT_FALSE(g_screen.openAlertDetail());
  TEST_ASSERT_FALSE(g_screen.alertDetailOpen());
}

void test_alert_present_open_detail(void) {
  const char* json =
      R"({"current":{"temp":70,"feelsLike":68,"code":3},"todayHigh":80,"todayLow":60,)"
      R"("hourly":[{"time":"2026-07-23T12:00","temp":72,"code":3}],)"
      R"("alert":{"severity":"Moderate","headline":"Heat Advisory"}})";

  Weather w{};
  TEST_ASSERT_TRUE(parseWeather(json, w));
  g_screen.bind(w);

  WeatherScreenView v = g_screen.view();
  TEST_ASSERT_TRUE(v.alertBadge);
  TEST_ASSERT_FALSE(v.alertDetailOpen);
  TEST_ASSERT_EQUAL_STRING("Moderate", v.alertSeverity);
  TEST_ASSERT_EQUAL_STRING("Heat Advisory", v.alertHeadline);

  TEST_ASSERT_TRUE(g_screen.openAlertDetail());
  TEST_ASSERT_TRUE(g_screen.alertDetailOpen());
  v = g_screen.view();
  TEST_ASSERT_TRUE(v.alertDetailOpen);
  TEST_ASSERT_EQUAL_STRING("Moderate", v.alertSeverity);
  TEST_ASSERT_EQUAL_STRING("Heat Advisory", v.alertHeadline);

  g_screen.closeAlertDetail();
  TEST_ASSERT_FALSE(g_screen.alertDetailOpen());
}

void test_clear_returns_not_ready(void) {
  TEST_ASSERT_TRUE(loadFixture("weather.json", g_buf, sizeof(g_buf)));
  Weather w{};
  TEST_ASSERT_TRUE(parseWeather(g_buf, w));
  g_screen.bind(w);
  TEST_ASSERT_TRUE(g_screen.ready());

  g_screen.clear();
  TEST_ASSERT_TRUE(g_screen.notReady());
  TEST_ASSERT_FALSE(g_screen.view().ready);
}

void test_when_label_current_and_hourly(void) {
  TEST_ASSERT_TRUE(loadFixture("weather.json", g_buf, sizeof(g_buf)));
  Weather w{};
  TEST_ASSERT_TRUE(parseWeather(g_buf, w));
  TEST_ASSERT_TRUE(w.hourlyCount >= 1);
  g_screen.bind(w);

  WeatherScreenView v = g_screen.view();
  TEST_ASSERT_EQUAL_STRING("Current", v.whenLabel);

  g_screen.onRotate(1);
  v = g_screen.view();
  TEST_ASSERT_FALSE(v.showingNow);
  // Fixture first hour is 18:00 → "6 PM"
  TEST_ASSERT_EQUAL_STRING("6 PM", v.whenLabel);
}

void test_strip_window_now_and_scrub(void) {
  TEST_ASSERT_TRUE(loadFixture("weather.json", g_buf, sizeof(g_buf)));
  Weather w{};
  TEST_ASSERT_TRUE(parseWeather(g_buf, w));
  TEST_ASSERT_TRUE(w.hourlyCount >= 5);
  g_screen.bind(w);

  WeatherScreenView v = g_screen.view();
  TEST_ASSERT_EQUAL(5, static_cast<int>(v.stripCount));
  TEST_ASSERT_TRUE(v.strip[0].valid);
  TEST_ASSERT_FALSE(v.strip[0].selected);  // Now: no hourly slot selected
  TEST_ASSERT_FLOAT_WITHIN(0.05f, w.hourly[0].temp, v.strip[0].temp);

  g_screen.onRotate(1);  // scrub index 0
  v = g_screen.view();
  TEST_ASSERT_TRUE(v.strip[0].selected);
  TEST_ASSERT_FALSE(v.strip[1].selected);
  TEST_ASSERT_EQUAL_STRING("6", v.strip[0].hourDigit);

  // Jump near end — window clamps, last slot selected
  g_screen.onRotate(static_cast<int>(w.hourlyCount) + 10);
  v = g_screen.view();
  TEST_ASSERT_EQUAL(5, static_cast<int>(v.stripCount));
  TEST_ASSERT_TRUE(v.strip[4].selected);
  const std::size_t last = w.hourlyCount - 1;
  TEST_ASSERT_FLOAT_WITHIN(0.05f, w.hourly[last].temp, v.strip[4].temp);
}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_unbound_is_not_ready);
  RUN_TEST(test_bind_fixture_display_state);
  RUN_TEST(test_scrub_hourly_updates_center);
  RUN_TEST(test_snap_to_now);
  RUN_TEST(test_scrub_backward_to_now);
  RUN_TEST(test_alert_absent_on_fixture);
  RUN_TEST(test_alert_present_open_detail);
  RUN_TEST(test_clear_returns_not_ready);
  RUN_TEST(test_when_label_current_and_hourly);
  RUN_TEST(test_strip_window_now_and_scrub);
  return UNITY_END();
}
