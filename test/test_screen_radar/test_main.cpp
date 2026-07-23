#include <unity.h>

#include <cstring>

#include "desk_display/adsb.hpp"
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
  TEST_ASSERT_TRUE(screen.blipCount() > 0);

  screen.unbind();
  TEST_ASSERT_FALSE(screen.ready());
  TEST_ASSERT_EQUAL(0, screen.blipCount());
}

void test_radar_range_clamp_and_zoom(void) {
  ScreenRadar screen;
  const AircraftList list = loadAdsbFixture();
  screen.bind(list);

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
  TEST_ASSERT_EQUAL(at25, screen.blipCount());
}

void test_radar_filter_blips_and_offsets(void) {
  ScreenRadar screen;
  const AircraftList list = loadAdsbFixture();
  screen.bind(list);

  AircraftList expected{};
  const std::size_t n = filterAircraftByRange(
      list, kRadarHomeLat, kRadarHomeLon, kRadarDefaultRangeMi, expected);
  TEST_ASSERT_EQUAL(n, screen.blipCount());
  TEST_ASSERT_TRUE(n > 0);

  for (std::size_t i = 0; i < n; ++i) {
    const RadarBlip& b = screen.blip(i);
    TEST_ASSERT_EQUAL_STRING(expected.items[i].callsign, b.aircraft.callsign);

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
  }

  // Tight zoom should drop some aircraft vs 25 mi
  screen.onRotate(-4);  // 25 → 5
  TEST_ASSERT_TRUE(screen.blipCount() < n);
}

void test_radar_mode_toggle(void) {
  ScreenRadar screen;
  screen.bind(loadAdsbFixture());
  TEST_ASSERT_EQUAL(static_cast<int>(RadarMode::ClassicSweep),
                    static_cast<int>(screen.mode()));

  screen.toggleMode();
  TEST_ASSERT_EQUAL(static_cast<int>(RadarMode::Detail),
                    static_cast<int>(screen.mode()));

  TEST_ASSERT_TRUE(screen.selectBlip(0));
  TEST_ASSERT_TRUE(screen.hasSelection());

  screen.toggleMode();  // back to sweep — clears selection
  TEST_ASSERT_EQUAL(static_cast<int>(RadarMode::ClassicSweep),
                    static_cast<int>(screen.mode()));
  TEST_ASSERT_FALSE(screen.hasSelection());
}

void test_radar_select_detail_card(void) {
  ScreenRadar screen;
  screen.bind(loadAdsbFixture());
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
  RUN_TEST(test_radar_temp_vs_pin_center);
  return UNITY_END();
}
