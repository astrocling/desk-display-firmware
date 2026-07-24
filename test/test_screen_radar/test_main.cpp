#include <unity.h>

#include <cstdio>
#include <cstring>

#include "desk_display/adsb.hpp"
#include "desk_display/adsb_poll.hpp"
#include "desk_display/airport.hpp"
#include "desk_display/radar.hpp"
#include "desk_display/screen_radar.hpp"
#include "fixture_loader.hpp"

using namespace desk_display;

static char g_buf[256 * 1024];

static AircraftList loadAdsbFixture() {
  AircraftList list{};
  TEST_ASSERT_TRUE_MESSAGE(loadFixture("adsb_sample.json", g_buf, sizeof(g_buf)),
                           "load adsb_sample.json");
  TEST_ASSERT_TRUE(parseAdsb(g_buf, list));
  TEST_ASSERT_TRUE(list.count > 0);
  return list;
}

static Airport loadAirportFixture() {
  Airport a{};
  TEST_ASSERT_TRUE_MESSAGE(
      loadFixture("airport_kday.json", g_buf, sizeof(g_buf)),
      "load airport_kday.json");
  TEST_ASSERT_TRUE(parseAirport(g_buf, a));
  return a;
}

/** Classic mode paints blips only as the sweep crosses them. */
static void paintFullRevolution(ScreenRadar& screen) {
  screen.onTick(kRadarSweepPeriodMs);
}

void test_radar_defaults_and_ready(void) {
  ScreenRadar screen;
  TEST_ASSERT_FALSE(screen.ready());
  TEST_ASSERT_EQUAL(static_cast<int>(RadarMode::ClassicSweep),
                    static_cast<int>(screen.mode()));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, kRadarDefaultRangeMi, screen.rangeMiles());
  TEST_ASSERT_TRUE(screen.isHomeCenter());
  TEST_ASSERT_FALSE(screen.isTempCenter());
  TEST_ASSERT_FALSE(screen.isPinned());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, static_cast<float>(kRadarHomeLat),
                           static_cast<float>(screen.centerLat()));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, static_cast<float>(kRadarHomeLon),
                           static_cast<float>(screen.centerLon()));

  const AircraftList list = loadAdsbFixture();
  screen.bind(list);
  TEST_ASSERT_TRUE(screen.ready());
  TEST_ASSERT_EQUAL(0, screen.blipCount());  // Classic: nothing until sweep
  paintFullRevolution(screen);
  TEST_ASSERT_TRUE(screen.blipCount() > 0);

  screen.unbind();
  TEST_ASSERT_FALSE(screen.ready());
  TEST_ASSERT_EQUAL(0, screen.blipCount());
}

void test_radar_range_clamp_and_zoom(void) {
  ScreenRadar screen;
  const AircraftList list = loadAdsbFixture();
  screen.bind(list);
  paintFullRevolution(screen);

  const std::size_t at25 = screen.blipCount();

  screen.onRotate(10);  // way past max
  TEST_ASSERT_FLOAT_WITHIN(0.01f, kRadarRangeMaxMi, screen.rangeMiles());

  screen.onRotate(-20);  // way past min
  TEST_ASSERT_FLOAT_WITHIN(0.01f, kRadarRangeMinMi, screen.rangeMiles());
  const std::size_t at5 = screen.blipCount();
  TEST_ASSERT_TRUE(at5 <= at25);

  // Step back toward default in 5 mi increments
  screen.onRotate(1);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, screen.rangeMiles());
  screen.onRotate(3);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.0f, screen.rangeMiles());
  // Classic zoom does not re-paint; count stays at pruned set until sweep.
  TEST_ASSERT_TRUE(screen.blipCount() <= at25);
}

void test_radar_filter_blips_and_offsets(void) {
  ScreenRadar screen;
  const AircraftList list = loadAdsbFixture();
  screen.bind(list);
  paintFullRevolution(screen);

  AircraftList expected{};
  const std::size_t n = filterAircraftByRange(
      list, kRadarHomeLat, kRadarHomeLon, kRadarDefaultRangeMi, expected);
  TEST_ASSERT_EQUAL(n, screen.blipCount());
  TEST_ASSERT_TRUE(n > 0);

  for (std::size_t i = 0; i < n; ++i) {
    bool found = false;
    for (std::size_t j = 0; j < screen.blipCount(); ++j) {
      const RadarBlip& b = screen.blip(j);
      if (std::strcmp(expected.items[i].callsign, b.aircraft.callsign) != 0) {
        continue;
      }
      found = true;
      float x = 0.0f;
      float y = 0.0f;
      aircraftOffsetMiles(kRadarHomeLat, kRadarHomeLon, b.aircraft.lat,
                          b.aircraft.lon, x, y);
      TEST_ASSERT_FLOAT_WITHIN(0.01f, x, b.offsetXMi);
      TEST_ASSERT_FLOAT_WITHIN(0.01f, y, b.offsetYMi);

      const float dist =
          distanceMiles(kRadarHomeLat, kRadarHomeLon, b.aircraft.lat,
                        b.aircraft.lon);
      TEST_ASSERT_TRUE(dist <= kRadarDefaultRangeMi + 0.01f);
      break;
    }
    TEST_ASSERT_TRUE(found);
  }

  // Tight zoom should drop some aircraft vs 25 mi
  screen.onRotate(-4);  // 25 → 5
  TEST_ASSERT_TRUE(screen.blipCount() < n);
}

void test_radar_mode_toggle(void) {
  ScreenRadar screen;
  screen.bind(loadAdsbFixture());
  paintFullRevolution(screen);
  TEST_ASSERT_EQUAL(static_cast<int>(RadarMode::ClassicSweep),
                    static_cast<int>(screen.mode()));

  screen.toggleMode();
  TEST_ASSERT_EQUAL(static_cast<int>(RadarMode::Detail),
                    static_cast<int>(screen.mode()));
  TEST_ASSERT_TRUE(screen.blipCount() > 0);

  TEST_ASSERT_TRUE(screen.selectBlip(0));
  TEST_ASSERT_TRUE(screen.hasSelection());

  // Sweep keeps advancing while a target is selected.
  const float before = screen.view().sweepAngleDeg;
  screen.onTick(500);  // +18° at 36 deg/s
  float expected = before + 18.0f;
  if (expected >= 360.0f) {
    expected -= 360.0f;
  }
  TEST_ASSERT_FLOAT_WITHIN(0.5f, expected, screen.view().sweepAngleDeg);
  TEST_ASSERT_TRUE(screen.hasSelection());

  screen.toggleMode();  // clears selection
  TEST_ASSERT_EQUAL(static_cast<int>(RadarMode::ClassicSweep),
                    static_cast<int>(screen.mode()));
  TEST_ASSERT_FALSE(screen.hasSelection());
}

void test_radar_select_detail_card(void) {
  ScreenRadar screen;
  screen.bind(loadAdsbFixture());
  paintFullRevolution(screen);
  TEST_ASSERT_FALSE(screen.selectBlip(999));
  TEST_ASSERT_FALSE(screen.hasSelection());

  TEST_ASSERT_TRUE(screen.selectBlip(0));
  TEST_ASSERT_TRUE(screen.hasSelection());
  TEST_ASSERT_EQUAL(0, screen.selectedIndex());

  const RadarDetailCard card = screen.detailCard();
  TEST_ASSERT_TRUE(card.present);
  TEST_ASSERT_EQUAL_STRING(screen.blip(0).aircraft.callsign, card.callsign);
  TEST_ASSERT_EQUAL(screen.blip(0).aircraft.hasAlt, card.hasAlt);
  TEST_ASSERT_EQUAL(screen.blip(0).aircraft.hasSpeed, card.hasSpeed);
  if (card.hasAlt) {
    TEST_ASSERT_FLOAT_WITHIN(0.1f, screen.blip(0).aircraft.altFt, card.altFt);
  }
  if (card.hasSpeed) {
    TEST_ASSERT_FLOAT_WITHIN(0.1f, screen.blip(0).aircraft.speedKt,
                             card.speedKt);
  }

  const RadarView v = screen.view();
  TEST_ASSERT_TRUE(v.hasSelection);
  TEST_ASSERT_TRUE(v.detail.present);
  TEST_ASSERT_EQUAL_STRING(card.callsign, v.detail.callsign);

  screen.clearSelection();
  TEST_ASSERT_FALSE(screen.hasSelection());
  TEST_ASSERT_FALSE(screen.detailCard().present);
}

void test_parse_adsb_track_and_rate(void) {
  const char* json =
      "{\"ac\":[{\"hex\":\"abc\",\"flight\":\"TST1  \",\"lat\":40.0,\"lon\":-84.0,"
      "\"alt_baro\":5200,\"gs\":106.2,\"track\":75.83,\"baro_rate\":-192}]}";
  AircraftList list{};
  TEST_ASSERT_TRUE(parseAdsb(json, list));
  TEST_ASSERT_EQUAL(1, list.count);
  TEST_ASSERT_TRUE(list.items[0].hasTrack);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 75.83f, list.items[0].trackDeg);
  TEST_ASSERT_TRUE(list.items[0].hasBaroRate);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, -192.0f, list.items[0].baroRateFpm);
}

void test_parse_adsb_calc_track_and_geom_rate_fallback(void) {
  const char* json =
      "{\"ac\":[{\"hex\":\"def\",\"lat\":40.0,\"lon\":-84.0,"
      "\"calc_track\":309,\"geom_rate\":448}]}";
  AircraftList list{};
  TEST_ASSERT_TRUE(parseAdsb(json, list));
  TEST_ASSERT_TRUE(list.items[0].hasTrack);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 309.0f, list.items[0].trackDeg);
  TEST_ASSERT_TRUE(list.items[0].hasBaroRate);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 448.0f, list.items[0].baroRateFpm);
}

void test_parse_adsb_missing_track_rate(void) {
  const char* json =
      "{\"ac\":[{\"hex\":\"ghi\",\"lat\":40.0,\"lon\":-84.0,\"alt_baro\":1000}]}";
  AircraftList list{};
  TEST_ASSERT_TRUE(parseAdsb(json, list));
  TEST_ASSERT_FALSE(list.items[0].hasTrack);
  TEST_ASSERT_FALSE(list.items[0].hasBaroRate);
}

void test_radar_sweep_advances_and_wraps(void) {
  ScreenRadar screen;
  screen.bind(loadAdsbFixture());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, screen.view().sweepAngleDeg);
  screen.onTick(1000);
  // 10 s/rev → 36 deg/s
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 36.0f, screen.view().sweepAngleDeg);
  screen.onTick(10000);  // +360 → wrap to same angle
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 36.0f, screen.view().sweepAngleDeg);
}

void test_radar_classic_paints_on_sweep_not_bind(void) {
  ScreenRadar screen;
  const AircraftList list = loadAdsbFixture();
  screen.bind(list);
  TEST_ASSERT_EQUAL(0, screen.blipCount());

  // Move slightly — still empty until a bearing is gated.
  screen.onTick(50);
  // After a full revolution every in-range aircraft has been painted once.
  paintFullRevolution(screen);
  AircraftList expected{};
  const std::size_t n = filterAircraftByRange(
      list, kRadarHomeLat, kRadarHomeLon, kRadarDefaultRangeMi, expected);
  TEST_ASSERT_EQUAL(n, screen.blipCount());

  // Re-bind with same data must not wipe displayed blips (no poll glitch).
  const std::size_t before = screen.blipCount();
  screen.bind(list);
  TEST_ASSERT_EQUAL(before, screen.blipCount());
}

void test_radar_bind_preserves_selection_by_callsign(void) {
  ScreenRadar screen;
  AircraftList list = loadAdsbFixture();
  screen.bind(list);
  paintFullRevolution(screen);
  std::size_t idx = 0;
  bool found = false;
  for (std::size_t i = 0; i < screen.blipCount(); ++i) {
    if (std::strcmp(screen.blip(i).aircraft.callsign, "UAL2402") == 0) {
      idx = i;
      found = true;
      break;
    }
  }
  TEST_ASSERT_TRUE(found);
  TEST_ASSERT_TRUE(screen.selectBlip(idx));
  screen.bind(list);
  TEST_ASSERT_TRUE(screen.hasSelection());
  TEST_ASSERT_EQUAL_STRING("UAL2402", screen.detailCard().callsign);
}

void test_radar_bind_clears_selection_when_gone(void) {
  ScreenRadar screen;
  AircraftList list = loadAdsbFixture();
  screen.bind(list);
  paintFullRevolution(screen);
  TEST_ASSERT_TRUE(screen.selectBlip(0));
  AircraftList other{};
  other.count = 1;
  std::snprintf(other.items[0].callsign, sizeof(other.items[0].callsign), "ZZZZZZ");
  other.items[0].hasPosition = true;
  other.items[0].lat = kRadarHomeLat;
  other.items[0].lon = kRadarHomeLon;
  screen.bind(other);
  TEST_ASSERT_FALSE(screen.hasSelection());
}

void test_radar_zoom_preserves_selection_by_callsign(void) {
  ScreenRadar screen;
  const AircraftList list = loadAdsbFixture();
  screen.bind(list);
  paintFullRevolution(screen);

  constexpr const char* kTarget = "N5953Q";
  std::size_t idx = 0;
  bool found = false;
  for (std::size_t i = 0; i < screen.blipCount(); ++i) {
    if (std::strcmp(screen.blip(i).aircraft.callsign, kTarget) == 0) {
      idx = i;
      found = true;
      break;
    }
  }
  TEST_ASSERT_TRUE(found);
  TEST_ASSERT_TRUE(screen.selectBlip(idx));
  TEST_ASSERT_EQUAL_STRING(kTarget, screen.detailCard().callsign);

  // Zoom out: painted set may grow after later sweeps; selection stays.
  screen.onRotate(5);
  TEST_ASSERT_TRUE(screen.hasSelection());
  TEST_ASSERT_EQUAL_STRING(kTarget, screen.detailCard().callsign);

  // Zoom in: blip count drops but callsign still in range.
  screen.onRotate(-3);
  TEST_ASSERT_TRUE(screen.hasSelection());
  TEST_ASSERT_EQUAL_STRING(kTarget, screen.detailCard().callsign);

  // Tighten to minimum range until the callsign is filtered out.
  while (screen.rangeMiles() > kRadarRangeMinMi) {
    screen.onRotate(-1);
  }
  bool stillVisible = false;
  for (std::size_t i = 0; i < screen.blipCount(); ++i) {
    if (std::strcmp(screen.blip(i).aircraft.callsign, kTarget) == 0) {
      stillVisible = true;
      break;
    }
  }
  TEST_ASSERT_FALSE(stillVisible);
  TEST_ASSERT_FALSE(screen.hasSelection());
}

void test_adsb_url_from_radar_home(void) {
  char url[160];
  TEST_ASSERT_TRUE(buildAdsbLolUrl(url, sizeof(url), kRadarHomeLat, kRadarHomeLon,
                                 kRadarDefaultRangeMi));
  TEST_ASSERT_NOT_NULL(std::strstr(url, "api.adsb.lol/v2/lat/"));
  TEST_ASSERT_NOT_NULL(std::strstr(url, "/lon/"));
  TEST_ASSERT_NOT_NULL(std::strstr(url, "/dist/"));
}

void test_radar_temp_vs_pin_center(void) {
  ScreenRadar screen;
  screen.bind(loadAdsbFixture());
  const Airport kday = loadAirportFixture();

  screen.setTempCenter(kday);
  TEST_ASSERT_TRUE(screen.isTempCenter());
  TEST_ASSERT_FALSE(screen.isHomeCenter());
  TEST_ASSERT_FALSE(screen.isPinned());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, static_cast<float>(kday.lat),
                           static_cast<float>(screen.centerLat()));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, static_cast<float>(kday.lon),
                           static_cast<float>(screen.centerLon()));

  // Carousel exit without pin → back to home
  screen.revertTempCenter();
  TEST_ASSERT_FALSE(screen.isTempCenter());
  TEST_ASSERT_TRUE(screen.isHomeCenter());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, static_cast<float>(kRadarHomeLat),
                           static_cast<float>(screen.centerLat()));

  // Temp again, then pin
  screen.setTempCenter(kday);
  screen.pinCenter();
  TEST_ASSERT_TRUE(screen.isPinned());
  TEST_ASSERT_FALSE(screen.isTempCenter());
  TEST_ASSERT_FALSE(screen.isHomeCenter());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, static_cast<float>(kday.lat),
                           static_cast<float>(screen.centerLat()));

  // Another temp recenter while pinned
  const double otherLat = 39.5;
  const double otherLon = -84.0;
  screen.setTempCenter(otherLat, otherLon);
  TEST_ASSERT_TRUE(screen.isTempCenter());
  TEST_ASSERT_TRUE(screen.isPinned());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, static_cast<float>(otherLat),
                           static_cast<float>(screen.centerLat()));

  // Carousel exit → back to pinned airport, not home
  screen.revertTempCenter();
  TEST_ASSERT_FALSE(screen.isTempCenter());
  TEST_ASSERT_TRUE(screen.isPinned());
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, static_cast<float>(kday.lat),
                           static_cast<float>(screen.centerLat()));
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, static_cast<float>(kday.lon),
                           static_cast<float>(screen.centerLon()));

  screen.clearPin();
  TEST_ASSERT_FALSE(screen.isPinned());
  // Active center still at KDAY until explicit revert/home
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, static_cast<float>(kday.lat),
                           static_cast<float>(screen.centerLat()));
  screen.revertTempCenter();  // no-op (not temp)
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, static_cast<float>(kday.lat),
                           static_cast<float>(screen.centerLat()));

  // Fresh temp + revert after clearPin uses home permanent
  screen.setTempCenter(otherLat, otherLon);
  screen.revertTempCenter();
  TEST_ASSERT_TRUE(screen.isHomeCenter());
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_radar_defaults_and_ready);
  RUN_TEST(test_radar_range_clamp_and_zoom);
  RUN_TEST(test_radar_filter_blips_and_offsets);
  RUN_TEST(test_radar_mode_toggle);
  RUN_TEST(test_radar_select_detail_card);
  RUN_TEST(test_parse_adsb_track_and_rate);
  RUN_TEST(test_parse_adsb_calc_track_and_geom_rate_fallback);
  RUN_TEST(test_parse_adsb_missing_track_rate);
  RUN_TEST(test_radar_sweep_advances_and_wraps);
  RUN_TEST(test_radar_classic_paints_on_sweep_not_bind);
  RUN_TEST(test_radar_bind_preserves_selection_by_callsign);
  RUN_TEST(test_radar_bind_clears_selection_when_gone);
  RUN_TEST(test_radar_zoom_preserves_selection_by_callsign);
  RUN_TEST(test_radar_temp_vs_pin_center);
  RUN_TEST(test_adsb_url_from_radar_home);
  return UNITY_END();
}
