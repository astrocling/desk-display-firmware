#include <unity.h>

#include <cmath>
#include <cstring>

#include "desk_display/adsb.hpp"
#include "desk_display/adsb_poll.hpp"
#include "desk_display/format_time.hpp"
#include "desk_display/mlb_live_format.hpp"
#include "desk_display/radar.hpp"
#include "desk_display/radar_format.hpp"
#include "desk_display/radar_settings.hpp"
#include "desk_display/scores_poll.hpp"
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

void test_scrub_offset_one_hour(void) {
  const std::int64_t anchor = 1'000'000;
  TEST_ASSERT_EQUAL_INT64(anchor + 60 * 60, applyScrubOffset(anchor, 1));
  TEST_ASSERT_EQUAL_INT64(anchor - 60 * 60, applyScrubOffset(anchor, -1));
  TEST_ASSERT_EQUAL_INT64(anchor + 4 * 60 * 60, applyScrubOffset(anchor, 4));
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

void test_format12_hour_short(void) {
  char buf[16];
  TEST_ASSERT_TRUE(format12HourShort(buf, sizeof(buf), 0) > 0);
  TEST_ASSERT_EQUAL_STRING("12 AM", buf);
  TEST_ASSERT_TRUE(format12HourShort(buf, sizeof(buf), 12) > 0);
  TEST_ASSERT_EQUAL_STRING("12 PM", buf);
  TEST_ASSERT_TRUE(format12HourShort(buf, sizeof(buf), 18) > 0);
  TEST_ASSERT_EQUAL_STRING("6 PM", buf);
  TEST_ASSERT_TRUE(format12HourShort(buf, sizeof(buf), 6) > 0);
  TEST_ASSERT_EQUAL_STRING("6 AM", buf);
}

void test_parse_hourly_iso_hour(void) {
  int hour = -1;
  TEST_ASSERT_TRUE(parseHourlyIsoHour("2026-07-23T18:00", hour));
  TEST_ASSERT_EQUAL(18, hour);
  TEST_ASSERT_TRUE(parseHourlyIsoHour("2026-07-23T00:00:00Z", hour));
  TEST_ASSERT_EQUAL(0, hour);
  TEST_ASSERT_TRUE(parseHourlyIsoHour("2026-07-24T12:30", hour));
  TEST_ASSERT_EQUAL(12, hour);
  TEST_ASSERT_FALSE(parseHourlyIsoHour("bad", hour));
  TEST_ASSERT_FALSE(parseHourlyIsoHour(nullptr, hour));
}

void test_radar_clamp_range(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, clampRadarRangeMiles(1.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, clampRadarRangeMiles(100.0f));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.0f, clampRadarRangeMiles(25.0f));
}

void test_format_radar_altitude_examples(void) {
  char buf[8];
  TEST_ASSERT_TRUE(formatRadarAltitude(buf, sizeof(buf), 33000.0f));
  TEST_ASSERT_EQUAL_STRING("F330", buf);
  TEST_ASSERT_TRUE(formatRadarAltitude(buf, sizeof(buf), 5200.0f));
  TEST_ASSERT_EQUAL_STRING("A052", buf);
  TEST_ASSERT_TRUE(formatRadarAltitude(buf, sizeof(buf), 18000.0f));
  TEST_ASSERT_EQUAL_STRING("F180", buf);
  TEST_ASSERT_TRUE(formatRadarAltitude(buf, sizeof(buf), 17999.0f));
  TEST_ASSERT_EQUAL_STRING("A180", buf);
}

void test_radar_trend_deadband(void) {
  TEST_ASSERT_EQUAL(static_cast<int>(RadarTrend::None),
                    static_cast<int>(radarTrendFromRate(50.0f, true)));
  TEST_ASSERT_EQUAL(static_cast<int>(RadarTrend::Climb),
                    static_cast<int>(radarTrendFromRate(128.0f, true)));
  TEST_ASSERT_EQUAL(static_cast<int>(RadarTrend::Descend),
                    static_cast<int>(radarTrendFromRate(-192.0f, true)));
  TEST_ASSERT_EQUAL(static_cast<int>(RadarTrend::None),
                    static_cast<int>(radarTrendFromRate(999.0f, false)));
}

void test_format_radar_tag_line2_omits_missing(void) {
  Aircraft ac{};
  ac.hasAlt = true;
  ac.altFt = 33000.0f;
  ac.hasSpeed = true;
  ac.speedKt = 474.5f;
  ac.hasBaroRate = false;
  char buf[32];
  TEST_ASSERT_TRUE(formatRadarTagLine2(buf, sizeof(buf), ac));
  TEST_ASSERT_EQUAL_STRING("F330 G475", buf);

  Aircraft onlyAlt{};
  onlyAlt.hasAlt = true;
  onlyAlt.altFt = 4500.0f;
  TEST_ASSERT_TRUE(formatRadarTagLine2(buf, sizeof(buf), onlyAlt));
  TEST_ASSERT_EQUAL_STRING("A045", buf);

  Aircraft empty{};
  TEST_ASSERT_FALSE(formatRadarTagLine2(buf, sizeof(buf), empty));
}

void test_format_radar_dense_and_line3(void) {
  char buf[32];
  TEST_ASSERT_TRUE(formatRadarAltitudeDense(buf, sizeof(buf), 33000.0f));
  TEST_ASSERT_EQUAL_STRING("330", buf);
  TEST_ASSERT_TRUE(formatRadarAltitudeDense(buf, sizeof(buf), 5200.0f));
  TEST_ASSERT_EQUAL_STRING("052", buf);
  TEST_ASSERT_TRUE(formatRadarSpeedDense(buf, sizeof(buf), 474.5f));
  TEST_ASSERT_EQUAL_STRING("475", buf);

  Aircraft ac{};
  ac.hasAlt = true;
  ac.altFt = 33000.0f;
  ac.hasSpeed = true;
  ac.speedKt = 450.0f;
  ac.hasBaroRate = true;
  ac.baroRateFpm = 128.0f;
  TEST_ASSERT_TRUE(
      formatRadarTagLine2(buf, sizeof(buf), ac, RadarTagStyle::Dense));
  TEST_ASSERT_EQUAL_STRING("330 ^ 450", buf);

  TEST_ASSERT_TRUE(formatRadarTagLine3(buf, sizeof(buf), "B738", "1200"));
  TEST_ASSERT_EQUAL_STRING("B738 1200", buf);
  TEST_ASSERT_TRUE(formatRadarTagLine3(buf, sizeof(buf), "C172", nullptr));
  TEST_ASSERT_EQUAL_STRING("C172", buf);
  TEST_ASSERT_TRUE(formatRadarTagLine3(buf, sizeof(buf), "", "7700"));
  TEST_ASSERT_EQUAL_STRING("7700", buf);
  TEST_ASSERT_FALSE(formatRadarTagLine3(buf, sizeof(buf), nullptr, ""));
}

void test_format_radar_tag_line4() {
  char buf[8];
  TEST_ASSERT_FALSE(formatRadarTagLine4(buf, sizeof(buf), nullptr));
  TEST_ASSERT_EQUAL_STRING("", buf);
  TEST_ASSERT_FALSE(formatRadarTagLine4(buf, sizeof(buf), ""));
  TEST_ASSERT_TRUE(formatRadarTagLine4(buf, sizeof(buf), "KORD"));
  TEST_ASSERT_EQUAL_STRING("KORD", buf);
}

void test_statute_to_nm_and_url(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 21.7244f, statuteMilesToNauticalMiles(25.0f));
  char url[160];
  TEST_ASSERT_TRUE(buildAdsbLolUrl(url, sizeof(url), 40.03353, -84.19588, 25.0f));
  TEST_ASSERT_NOT_NULL(std::strstr(url, "api.adsb.lol/v2/lat/"));
  TEST_ASSERT_NOT_NULL(std::strstr(url, "/lon/"));
  TEST_ASSERT_NOT_NULL(std::strstr(url, "/dist/"));
}

static int g_http_calls;
static bool fake_http(const char* url, char* body, std::size_t cap,
                      std::size_t& len, void*) {
  (void)url;
  ++g_http_calls;
  const char* json =
      "{\"ac\":[{\"hex\":\"a\",\"flight\":\"P1\",\"lat\":40.03,\"lon\":-84.19,"
      "\"alt_baro\":1000,\"gs\":100,\"track\":90}]}";
  len = std::strlen(json);
  if (len + 1 > cap) return false;
  std::memcpy(body, json, len + 1);
  return true;
}

void test_adsb_poller_only_when_active(void) {
  g_http_calls = 0;
  AdsbPoller poll;
  poll.setHttpGet(fake_http, nullptr);
  poll.setCenter(40.03353, -84.19588, 25.0f);
  poll.setActive(false);
  poll.onTick(15000);
  TEST_ASSERT_EQUAL(0, g_http_calls);
  poll.setActive(true);
  poll.onTick(10000);
  TEST_ASSERT_EQUAL(1, g_http_calls);
  AircraftList list{};
  TEST_ASSERT_TRUE(poll.takeAircraft(list));
  TEST_ASSERT_EQUAL(1, list.count);
  TEST_ASSERT_FALSE(poll.takeAircraft(list));
}

void test_adsb_poller_prefetches_before_interval(void) {
  g_http_calls = 0;
  AdsbPoller poll;
  poll.setHttpGet(fake_http, nullptr);
  poll.setCenter(40.03353, -84.19588, 25.0f);
  poll.setActive(true);

  const uint32_t prefetchAt =
      kAdsbPollIntervalMs > kAdsbPrefetchLeadMs
          ? kAdsbPollIntervalMs - kAdsbPrefetchLeadMs
          : 0;
  if (prefetchAt > 0) {
    poll.onTick(prefetchAt - 1);
    TEST_ASSERT_EQUAL(0, g_http_calls);
    poll.onTick(1);
  } else {
    poll.onTick(1);
  }
  TEST_ASSERT_EQUAL(1, g_http_calls);
  AircraftList list{};
  TEST_ASSERT_TRUE(poll.takeAircraft(list));
}

static int g_http_fail_then_ok = 0;

bool fake_http_pending_then_ok(const char* /*url*/, char* body, std::size_t cap,
                               std::size_t& len, void* /*user*/) {
  ++g_http_calls;
  if (g_http_fail_then_ok > 0) {
    --g_http_fail_then_ok;
    return false;  // async in-flight
  }
  const char* json =
      "{\"ac\":[{\"hex\":\"abc\",\"flight\":\"PEND1\",\"lat\":40.0,\"lon\":-84.0,"
      "\"alt_baro\":1000,\"gs\":100,\"track\":90}]}";
  len = std::strlen(json);
  if (len + 1 > cap) return false;
  std::memcpy(body, json, len + 1);
  return true;
}

void test_adsb_poller_retries_while_http_pending(void) {
  g_http_calls = 0;
  g_http_fail_then_ok = 2;
  AdsbPoller poll;
  poll.setHttpGet(fake_http_pending_then_ok, nullptr);
  poll.setCenter(40.03353, -84.19588, 25.0f);
  poll.setActive(true);

  const uint32_t prefetchAt =
      kAdsbPollIntervalMs > kAdsbPrefetchLeadMs
          ? kAdsbPollIntervalMs - kAdsbPrefetchLeadMs
          : 0;
  poll.onTick(prefetchAt + 1);
  TEST_ASSERT_EQUAL(1, g_http_calls);
  AircraftList list{};
  TEST_ASSERT_FALSE(poll.takeAircraft(list));

  poll.onTick(16);
  TEST_ASSERT_EQUAL(2, g_http_calls);
  TEST_ASSERT_FALSE(poll.takeAircraft(list));

  poll.onTick(16);
  TEST_ASSERT_EQUAL(3, g_http_calls);
  TEST_ASSERT_TRUE(poll.takeAircraft(list));
  TEST_ASSERT_EQUAL(1, list.count);
}

bool fake_scores_http(const char* /*url*/, char* body, std::size_t cap, std::size_t& len,
                      void* /*user*/) {
  ++g_http_calls;
  const char* json =
      "{\"mlb\":{\"live\":true,\"score\":\"3-1\",\"inning\":\"Top 5\",\"nextGame\":null},"
      "\"flagstand\":{\"lastResult\":null,\"nextRace\":null},"
      "\"updatedAt\":\"2026-07-24T00:00:00.000Z\"}";
  len = std::strlen(json);
  if (len + 1 > cap) {
    return false;
  }
  std::memcpy(body, json, len + 1);
  return true;
}

void test_build_scores_url(void) {
  char url[160];
  TEST_ASSERT_TRUE(buildScoresUrl(url, sizeof(url)));
  TEST_ASSERT_NOT_NULL(std::strstr(url, "/api/scores"));
}

void test_scores_poller_only_when_active(void) {
  g_http_calls = 0;
  ScoresPoller poll;
  poll.setHttpGet(fake_scores_http, nullptr);
  poll.setActive(false);
  poll.onTick(kScoresPollIntervalMs + 1);
  TEST_ASSERT_EQUAL(0, g_http_calls);

  // Becoming active arms an immediate fetch on the next tick.
  poll.setActive(true);
  poll.onTick(1);
  TEST_ASSERT_EQUAL(1, g_http_calls);
  Scores scores{};
  TEST_ASSERT_TRUE(poll.takeScores(scores));
  TEST_ASSERT_TRUE(scores.mlb.live);
  TEST_ASSERT_EQUAL_STRING("3-1", scores.mlb.score);
  TEST_ASSERT_FALSE(poll.takeScores(scores));
}

void test_scores_poller_waits_full_interval_after_success(void) {
  g_http_calls = 0;
  ScoresPoller poll;
  poll.setHttpGet(fake_scores_http, nullptr);
  poll.setActive(true);
  poll.onTick(1);
  TEST_ASSERT_EQUAL(1, g_http_calls);
  Scores scores{};
  TEST_ASSERT_TRUE(poll.takeScores(scores));

  poll.onTick(kScoresPollIntervalMs - 1);
  TEST_ASSERT_EQUAL(1, g_http_calls);
  poll.onTick(1);
  TEST_ASSERT_EQUAL(2, g_http_calls);
}

void test_mlb_live_format_helpers(void) {
  MlbScores m{};
  m.hasBalls = true;
  m.balls = 3;
  m.hasStrikes = true;
  m.strikes = 2;
  m.hasOuts = true;
  m.outs = 2;
  char count[32];
  formatMlbCountLine(count, sizeof(count), m);
  TEST_ASSERT_EQUAL_STRING("3-2 - 2 outs", count);

  m.hasOnFirst = true;
  m.onFirst = true;
  m.hasOnSecond = true;
  m.onSecond = true;
  m.hasOnThird = true;
  m.onThird = false;
  char bases[32];
  formatMlbBasesLine(bases, sizeof(bases), m);
  // Diamond mask: 2nd, 3rd, 1st, home (`*` occupied, `.` empty; home always `.`).
  TEST_ASSERT_EQUAL_STRING("*.*.", bases);

  m.onFirst = true;
  m.onSecond = true;
  m.onThird = true;
  formatMlbBasesLine(bases, sizeof(bases), m);
  TEST_ASSERT_EQUAL_STRING("***.", bases);

  m.onFirst = false;
  m.onSecond = false;
  m.onThird = false;
  formatMlbBasesLine(bases, sizeof(bases), m);
  TEST_ASSERT_EQUAL_STRING("....", bases);

  m.hasBatterName = true;
  std::strncpy(m.batterName, "A. Judge", sizeof(m.batterName) - 1);
  m.hasBatterAvg = true;
  std::strncpy(m.batterAvg, ".311", sizeof(m.batterAvg) - 1);
  m.hasBatterSummary = true;
  std::strncpy(m.batterSummary, "1-3, BB", sizeof(m.batterSummary) - 1);
  m.hasPitcherName = true;
  std::strncpy(m.pitcherName, "F. Valdez", sizeof(m.pitcherName) - 1);
  m.hasPitcherEra = true;
  std::strncpy(m.pitcherEra, "2.85", sizeof(m.pitcherEra) - 1);
  m.hasPitcherSummary = true;
  std::strncpy(m.pitcherSummary, "5.0 IP, 2 ER", sizeof(m.pitcherSummary) - 1);
  char names[128];
  formatMlbBatterPitcherLine(names, sizeof(names), m);
  TEST_ASSERT_EQUAL_STRING(
      "AB: A. Judge .311 - 1-3, BB\nP: F. Valdez 2.85 - 5.0 IP, 2 ER", names);

  MlbScores m2{};
  m2.hasBatterName = true;
  std::strncpy(m2.batterName, "A. Judge", sizeof(m2.batterName) - 1);
  m2.hasBatterAvg = true;
  std::strncpy(m2.batterAvg, ".311", sizeof(m2.batterAvg) - 1);
  formatMlbBatterPitcherLine(names, sizeof(names), m2);
  TEST_ASSERT_EQUAL_STRING("AB: A. Judge .311", names);

  MlbScores m3{};
  m3.hasBatterName = true;
  std::strncpy(m3.batterName, "A. Judge", sizeof(m3.batterName) - 1);
  m3.hasBatterSummary = true;
  std::strncpy(m3.batterSummary, "1-3, BB", sizeof(m3.batterSummary) - 1);
  formatMlbBatterPitcherLine(names, sizeof(names), m3);
  TEST_ASSERT_EQUAL_STRING("AB: A. Judge - 1-3, BB", names);

  MlbScores m4{};
  m4.hasPitcherName = true;
  std::strncpy(m4.pitcherName, "F. Valdez", sizeof(m4.pitcherName) - 1);
  m4.hasPitcherEra = true;
  std::strncpy(m4.pitcherEra, "2.85", sizeof(m4.pitcherEra) - 1);
  m4.hasPitcherSummary = true;
  std::strncpy(m4.pitcherSummary, "5.0 IP, 2 ER", sizeof(m4.pitcherSummary) - 1);
  formatMlbBatterPitcherLine(names, sizeof(names), m4);
  TEST_ASSERT_EQUAL_STRING("P: F. Valdez 2.85 - 5.0 IP, 2 ER", names);

  MlbScores m5{};
  m5.hasPitcherName = true;
  std::strncpy(m5.pitcherName, "F. Valdez", sizeof(m5.pitcherName) - 1);
  m5.hasPitcherEra = true;
  std::strncpy(m5.pitcherEra, "2.85", sizeof(m5.pitcherEra) - 1);
  m5.hasPitcherSummary = true;
  std::strncpy(m5.pitcherSummary, "5.0 IP, 2 ER, 4 H, 6 K, BB, HR",
               sizeof(m5.pitcherSummary) - 1);
  formatMlbBatterPitcherLine(names, sizeof(names), m5);
  TEST_ASSERT_EQUAL_STRING("P: F. Valdez 2.85 - 5.0 IP, 2 ER, 4 H, 6 K, BB", names);
}

void test_radar_settings_hit_rect_mapping(void) {
  using desk_display::radarSettingsHitContains;
  using desk_display::radarSettingsHitRectFromArea;
  const auto rect = radarSettingsHitRectFromArea(100, 200, 159, 227);
  TEST_ASSERT_TRUE(radarSettingsHitContains(rect, 120.0f, 210.0f));
  TEST_ASSERT_FALSE(radarSettingsHitContains(rect, 99.0f, 210.0f));
  TEST_ASSERT_FALSE(radarSettingsHitContains(rect, 160.0f, 210.0f));
  // Pre-layout zero-area coords must not register taps.
  const auto empty = radarSettingsHitRectFromArea(0, 0, -1, -1);
  TEST_ASSERT_FALSE(radarSettingsHitContains(empty, 0.0f, 0.0f));
}

void test_radar_settings_defaults(void) {
  const auto s = desk_display::radarSettingsFactoryDefaults();
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(desk_display::RadarDeclutterMode::TargetTag),
      static_cast<uint8_t>(s.declutter));
  TEST_ASSERT_TRUE(s.showAirports);
  TEST_ASSERT_TRUE(s.showAirspace);
  TEST_ASSERT_TRUE(s.showRoads);
  TEST_ASSERT_FALSE(s.demoMode);
}

void test_radar_unselected_label_modes(void) {
  using desk_display::RadarDeclutterMode;
  using desk_display::RadarUnselectedLabel;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RadarUnselectedLabel::None),
      static_cast<uint8_t>(
          desk_display::radarUnselectedLabel(RadarDeclutterMode::TargetOnly)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RadarUnselectedLabel::Callsign),
      static_cast<uint8_t>(desk_display::radarUnselectedLabel(
          RadarDeclutterMode::TargetCallsign)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RadarUnselectedLabel::DenseTag),
      static_cast<uint8_t>(
          desk_display::radarUnselectedLabel(RadarDeclutterMode::TargetTag)));
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
  RUN_TEST(test_scrub_offset_one_hour);
  RUN_TEST(test_wmo_to_icon);
  RUN_TEST(test_format12_hour_short);
  RUN_TEST(test_parse_hourly_iso_hour);
  RUN_TEST(test_radar_clamp_range);
  RUN_TEST(test_format_radar_altitude_examples);
  RUN_TEST(test_radar_trend_deadband);
  RUN_TEST(test_format_radar_tag_line2_omits_missing);
  RUN_TEST(test_format_radar_dense_and_line3);
  RUN_TEST(test_format_radar_tag_line4);
  RUN_TEST(test_radar_settings_hit_rect_mapping);
  RUN_TEST(test_radar_settings_defaults);
  RUN_TEST(test_radar_unselected_label_modes);
  RUN_TEST(test_radar_offset_and_distance);
  RUN_TEST(test_statute_to_nm_and_url);
  RUN_TEST(test_adsb_poller_only_when_active);
  RUN_TEST(test_adsb_poller_prefetches_before_interval);
  RUN_TEST(test_adsb_poller_retries_while_http_pending);
  RUN_TEST(test_build_scores_url);
  RUN_TEST(test_scores_poller_only_when_active);
  RUN_TEST(test_scores_poller_waits_full_interval_after_success);
  RUN_TEST(test_mlb_live_format_helpers);
  return UNITY_END();
}
