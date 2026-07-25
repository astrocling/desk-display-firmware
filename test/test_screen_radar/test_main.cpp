#include <unity.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "desk_display/adsb.hpp"
#include "desk_display/adsb_poll.hpp"
#include "desk_display/map_context_poll.hpp"
#include "desk_display/aircraft_notable.hpp"
#include "desk_display/airport.hpp"
#include "desk_display/map_context.hpp"
#include "desk_display/radar.hpp"
#include "desk_display/radar_prefs.hpp"
#include "desk_display/radar_settings.hpp"
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

static MapContext loadMapContextDaytonFixture() {
  MapContext ctx{};
  TEST_ASSERT_TRUE_MESSAGE(
      loadFixture("map_context_dayton.json", g_buf, sizeof(g_buf)),
      "load map_context_dayton.json");
  TEST_ASSERT_TRUE(parseMapContext(g_buf, ctx));
  return ctx;
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
  TEST_ASSERT_EQUAL_STRING(screen.blip(0).aircraft.type, card.type);
  TEST_ASSERT_EQUAL_STRING(screen.blip(0).aircraft.squawk, card.squawk);
  TEST_ASSERT_EQUAL(screen.blip(0).aircraft.hasAlt, card.hasAlt);
  TEST_ASSERT_EQUAL(screen.blip(0).aircraft.hasSpeed, card.hasSpeed);
  if (card.hasAlt) {
    TEST_ASSERT_FLOAT_WITHIN(0.1f, screen.blip(0).aircraft.altFt, card.altFt);
  }
  if (card.hasSpeed) {
    TEST_ASSERT_FLOAT_WITHIN(0.1f, screen.blip(0).aircraft.speedKt,
                             card.speedKt);
  }
  if (card.type[0] || card.squawk[0]) {
    TEST_ASSERT_TRUE(card.tagLine3[0] != '\0');
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

void test_parse_adsb_type_reg_squawk(void) {
  const char* json =
      "{\"ac\":[{\"hex\":\"abc\",\"flight\":\"UAL1\",\"r\":\"N12345\",\"t\":\"B738\","
      "\"squawk\":\"1200\",\"lat\":40.0,\"lon\":-84.0,\"alt_baro\":30000}]}";
  AircraftList list{};
  TEST_ASSERT_TRUE(parseAdsb(json, list));
  TEST_ASSERT_EQUAL(1, list.count);
  TEST_ASSERT_EQUAL_STRING("UAL1", list.items[0].callsign);
  TEST_ASSERT_EQUAL_STRING("B738", list.items[0].type);
  TEST_ASSERT_EQUAL_STRING("N12345", list.items[0].registration);
  TEST_ASSERT_EQUAL_STRING("1200", list.items[0].squawk);

  const char* jsonNum =
      "{\"ac\":[{\"hex\":\"def\",\"t\":\"C172\",\"squawk\":7700,\"lat\":40.0,\"lon\":-84.0}]}";
  AircraftList list2{};
  TEST_ASSERT_TRUE(parseAdsb(jsonNum, list2));
  TEST_ASSERT_EQUAL_STRING("C172", list2.items[0].type);
  TEST_ASSERT_EQUAL_STRING("7700", list2.items[0].squawk);
}

void test_radar_notable_military_and_watchlist(void) {
  ScreenRadar screen;
  screen.bind(loadAdsbFixture());
  paintFullRevolution(screen);

  bool foundMil = false;
  for (std::size_t i = 0; i < screen.blipCount(); ++i) {
    const RadarBlip& b = screen.blip(i);
    if (std::strcmp(b.aircraft.callsign, "COBRA01") == 0) {
      TEST_ASSERT_EQUAL(1, b.aircraft.dbFlags);
      TEST_ASSERT_EQUAL(static_cast<int>(AircraftNotable::Military),
                        static_cast<int>(b.notable));
      TEST_ASSERT_TRUE(screen.selectBlip(i));
      const RadarDetailCard card = screen.detailCard();
      TEST_ASSERT_EQUAL(static_cast<int>(AircraftNotable::Military),
                        static_cast<int>(card.notable));
      TEST_ASSERT_NOT_NULL(std::strstr(card.tagLine3, "MIL"));
      foundMil = true;
      break;
    }
  }
  TEST_ASSERT_TRUE_MESSAGE(foundMil, "COBRA01 should be painted in range");

  AircraftList helo{};
  helo.count = 1;
  std::snprintf(helo.items[0].callsign, sizeof(helo.items[0].callsign), "CF9");
  std::snprintf(helo.items[0].registration, sizeof(helo.items[0].registration),
                "N130NB");
  helo.items[0].hasPosition = true;
  helo.items[0].lat = kRadarHomeLat + 0.02;
  helo.items[0].lon = kRadarHomeLon;
  helo.items[0].hasTrack = true;
  helo.items[0].trackDeg = 90.0f;

  ScreenRadar screen2;
  screen2.bind(helo);
  paintFullRevolution(screen2);
  TEST_ASSERT_TRUE(screen2.blipCount() >= 1);
  TEST_ASSERT_EQUAL(static_cast<int>(AircraftNotable::Interesting),
                    static_cast<int>(screen2.blip(0).notable));
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

static int g_map_ctx_http_calls;
static char g_last_map_ctx_url[256];
static bool g_map_ctx_http_fail;

static bool fake_map_ctx_http(const char* url, char* body, std::size_t cap,
                              std::size_t& len, void*) {
  std::snprintf(g_last_map_ctx_url, sizeof(g_last_map_ctx_url), "%s", url);
  ++g_map_ctx_http_calls;
  if (g_map_ctx_http_fail) {
    return false;
  }
  const char* json =
      "{\"airports\":[{\"icao\":\"KT\",\"name\":\"Test\",\"lat\":40.0,\"lon\":-84.0}],"
      "\"rings\":[]}";
  len = std::strlen(json);
  if (len + 1 > cap) {
    return false;
  }
  std::memcpy(body, json, len + 1);
  return true;
}

void test_map_context_poller_debounces_center(void) {
  g_map_ctx_http_calls = 0;
  g_last_map_ctx_url[0] = '\0';
  MapContextPoller poll;
  poll.setHttpGet(fake_map_ctx_http, nullptr);
  poll.setActive(true);

  poll.setCenter(40.03353, -84.19588, 25.0f);
  poll.onTick(100);
  poll.setCenter(40.05, -84.2, 25.0f);
  poll.onTick(100);
  poll.setCenter(40.05, -84.2, 30.0f);
  poll.onTick(200);
  TEST_ASSERT_EQUAL(0, g_map_ctx_http_calls);

  poll.onTick(200);
  TEST_ASSERT_EQUAL(1, g_map_ctx_http_calls);
  TEST_ASSERT_NOT_NULL(std::strstr(g_last_map_ctx_url, "/api/map/context"));
  TEST_ASSERT_NOT_NULL(std::strstr(g_last_map_ctx_url, "radiusMi=30"));

  MapContext ctx{};
  TEST_ASSERT_TRUE(poll.takeContext(ctx));
  TEST_ASSERT_EQUAL(1, ctx.airportCount);
  TEST_ASSERT_FALSE(poll.takeContext(ctx));

  poll.setCenter(40.05, -84.2, 30.0f);
  poll.onTick(500);
  TEST_ASSERT_EQUAL(1, g_map_ctx_http_calls);
}

void test_map_context_poller_only_when_active(void) {
  g_map_ctx_http_calls = 0;
  MapContextPoller poll;
  poll.setHttpGet(fake_map_ctx_http, nullptr);
  poll.setCenter(40.03353, -84.19588, 25.0f);
  poll.setActive(false);
  poll.onTick(500);
  TEST_ASSERT_EQUAL(0, g_map_ctx_http_calls);
  poll.setActive(true);
  poll.onTick(500);
  TEST_ASSERT_EQUAL(1, g_map_ctx_http_calls);
}

void test_map_context_poller_stops_on_persistent_failure(void) {
  g_map_ctx_http_calls = 0;
  g_map_ctx_http_fail = false;
  MapContextPoller poll;
  poll.setHttpGet(fake_map_ctx_http, nullptr);
  poll.setActive(true);
  poll.setCenter(40.03353, -84.19588, 25.0f);
  poll.onTick(500);
  TEST_ASSERT_EQUAL(1, g_map_ctx_http_calls);

  MapContext ctx{};
  TEST_ASSERT_TRUE(poll.takeContext(ctx));
  TEST_ASSERT_EQUAL(1, ctx.airportCount);
  TEST_ASSERT_EQUAL_STRING("KT", ctx.airports[0].icao);

  g_map_ctx_http_fail = true;
  poll.setCenter(40.05, -84.2, 25.0f);
  const int callsAfterSuccess = g_map_ctx_http_calls;
  for (int i = 0; i < 200; ++i) {
    poll.onTick(100);
  }

  const int failurePhaseCalls = g_map_ctx_http_calls - callsAfterSuccess;
  TEST_ASSERT_TRUE(failurePhaseCalls > 0);
  // Debounce + retries only until kAdsbFetchMaxWaitMs, not every tick forever.
  TEST_ASSERT_TRUE(failurePhaseCalls <= 90);

  const int callsAfterBudget = g_map_ctx_http_calls;
  for (int i = 0; i < 50; ++i) {
    poll.onTick(100);
  }
  TEST_ASSERT_EQUAL(callsAfterBudget, g_map_ctx_http_calls);
  TEST_ASSERT_TRUE(poll.hasLastGood());
  TEST_ASSERT_EQUAL(1, ctx.airportCount);
  TEST_ASSERT_EQUAL_STRING("KT", ctx.airports[0].icao);
}

void test_map_context_url_from_radar_home(void) {
  char url[256];
  TEST_ASSERT_TRUE(buildMapContextUrl(url, sizeof(url), kRadarHomeLat, kRadarHomeLon,
                                    kRadarDefaultRangeMi));
  TEST_ASSERT_NOT_NULL(std::strstr(url, "/api/map/context"));
  TEST_ASSERT_NOT_NULL(std::strstr(url, "lat="));
  TEST_ASSERT_NOT_NULL(std::strstr(url, "lon="));
  TEST_ASSERT_NOT_NULL(std::strstr(url, "radiusMi="));
}

void test_adsb_url_from_radar_home(void) {
  char url[160];
  TEST_ASSERT_TRUE(buildAdsbLolUrl(url, sizeof(url), kRadarHomeLat, kRadarHomeLon,
                                 kRadarDefaultRangeMi));
  TEST_ASSERT_NOT_NULL(std::strstr(url, "api.adsb.lol/v2/lat/"));
  TEST_ASSERT_NOT_NULL(std::strstr(url, "/lon/"));
  TEST_ASSERT_NOT_NULL(std::strstr(url, "/dist/"));
}

void test_radar_set_pois_adds_home_mark(void) {
  ScreenRadar screen;
  const RadarPoi pois[] = {{"Home", kRadarHomeLat, kRadarHomeLon}};
  screen.setPois(pois, 1);
  const RadarView v = screen.view();
  TEST_ASSERT_EQUAL(1, v.staticMarkCount);
  TEST_ASSERT_EQUAL(static_cast<int>(RadarStaticMark::Kind::Poi),
                    static_cast<int>(v.staticMarks[0].kind));
  TEST_ASSERT_EQUAL_STRING("Home", v.staticMarks[0].label);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, v.staticMarks[0].offsetXMi);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, v.staticMarks[0].offsetYMi);
}

void test_radar_bind_map_context_projects_airport(void) {
  ScreenRadar r;
  MapContext ctx{};
  ctx.airportCount = 1;
  std::snprintf(ctx.airports[0].icao, sizeof(ctx.airports[0].icao), "KTEST");
  ctx.airports[0].lat = kRadarHomeLat;
  ctx.airports[0].lon =
      kRadarHomeLon + 10.0 / (69.0 * std::cos(kRadarHomeLat * 0.017453292519943295));
  r.bindMapContext(ctx);
  const RadarView v = r.view();
  TEST_ASSERT_EQUAL(1, v.staticMarkCount);
  TEST_ASSERT_FLOAT_WITHIN(1.5f, 10.0f, v.staticMarks[0].offsetXMi);
  TEST_ASSERT_FLOAT_WITHIN(1.5f, 0.0f, v.staticMarks[0].offsetYMi);
  TEST_ASSERT_EQUAL(static_cast<int>(RadarStaticMark::Kind::Airport),
                    static_cast<int>(v.staticMarks[0].kind));
  TEST_ASSERT_EQUAL_STRING("KTEST", v.staticMarks[0].label);
}

void test_radar_bind_map_context_projects_highways_and_shelves(void) {
  ScreenRadar r;
  MapContext ctx{};
  ctx.ringCount = 3;
  for (std::size_t i = 0; i < 3; ++i) {
    ctx.rings[i].cls = (i < 2) ? AirspaceClass::D : AirspaceClass::C;
    std::snprintf(ctx.rings[i].id, sizeof(ctx.rings[i].id), "RING_%zu", i);
    ctx.rings[i].pointCount = 4;
    ctx.rings[i].pointsLat[0] = static_cast<float>(kRadarHomeLat + 0.05);
    ctx.rings[i].pointsLon[0] = static_cast<float>(kRadarHomeLon - 0.05);
    ctx.rings[i].pointsLat[1] = static_cast<float>(kRadarHomeLat + 0.05);
    ctx.rings[i].pointsLon[1] = static_cast<float>(kRadarHomeLon + 0.05);
    ctx.rings[i].pointsLat[2] = static_cast<float>(kRadarHomeLat - 0.05);
    ctx.rings[i].pointsLon[2] = static_cast<float>(kRadarHomeLon + 0.05);
    ctx.rings[i].pointsLat[3] = static_cast<float>(kRadarHomeLat - 0.05);
    ctx.rings[i].pointsLon[3] = static_cast<float>(kRadarHomeLon - 0.05);
  }
  ctx.highwayCount = 1;
  std::snprintf(ctx.highways[0].id, sizeof(ctx.highways[0].id), "I-75");
  std::snprintf(ctx.highways[0].route, sizeof(ctx.highways[0].route), "I-75");
  ctx.highways[0].pointCount = 2;
  ctx.highways[0].pointsLat[0] = static_cast<float>(kRadarHomeLat + 0.02);
  ctx.highways[0].pointsLon[0] = static_cast<float>(kRadarHomeLon);
  ctx.highways[0].pointsLat[1] = static_cast<float>(kRadarHomeLat - 0.02);
  ctx.highways[0].pointsLon[1] = static_cast<float>(kRadarHomeLon);

  r.bindMapContext(ctx);
  const RadarView v = r.view();
  TEST_ASSERT_EQUAL(3, v.airspaceRingCount);
  TEST_ASSERT_EQUAL(1, v.highwayCount);
  TEST_ASSERT_EQUAL(2, v.highways[0].pointCount);
}

void test_radar_static_selection_survives_reproject(void) {
  ScreenRadar r;
  MapContext ctx{};
  ctx.airportCount = 1;
  std::snprintf(ctx.airports[0].icao, sizeof(ctx.airports[0].icao), "KTEST");
  ctx.airports[0].lat = kRadarHomeLat;
  ctx.airports[0].lon =
      kRadarHomeLon + 10.0 / (69.0 * std::cos(kRadarHomeLat * 0.017453292519943295));
  r.bindMapContext(ctx);

  TEST_ASSERT_TRUE(r.selectStaticMark(0));
  TEST_ASSERT_TRUE(r.view().hasStaticSelection);
  TEST_ASSERT_EQUAL(0, r.view().selectedStaticIndex);

  r.onRotate(1);

  const RadarView v = r.view();
  TEST_ASSERT_TRUE(v.hasStaticSelection);
  TEST_ASSERT_EQUAL(0, v.selectedStaticIndex);
  TEST_ASSERT_TRUE(v.staticMarkCount >= 1);
}

void test_radar_static_select_clears_blip_select(void) {
  ScreenRadar r;
  r.bind(loadAdsbFixture());
  paintFullRevolution(r);

  MapContext ctx{};
  ctx.airportCount = 1;
  std::snprintf(ctx.airports[0].icao, sizeof(ctx.airports[0].icao), "KTEST");
  ctx.airports[0].lat = kRadarHomeLat + 0.02;
  ctx.airports[0].lon = kRadarHomeLon;
  r.bindMapContext(ctx);

  TEST_ASSERT_TRUE(r.selectBlip(0));
  TEST_ASSERT_TRUE(r.hasSelection());
  TEST_ASSERT_TRUE(r.selectStaticMark(0));
  TEST_ASSERT_FALSE(r.hasSelection());
  TEST_ASSERT_TRUE(r.view().hasStaticSelection);
  TEST_ASSERT_EQUAL(0, r.view().selectedStaticIndex);

  TEST_ASSERT_TRUE(r.selectBlip(0));
  TEST_ASSERT_FALSE(r.view().hasStaticSelection);
  TEST_ASSERT_TRUE(r.hasSelection());
}

void test_radar_settings_gate_airports(void) {
  ScreenRadar screen;
  screen.bindMapContext(loadMapContextDaytonFixture());
  const RadarPoi pois[] = {{"Home", kRadarHomeLat, kRadarHomeLon}};
  screen.setPois(pois, 1);
  TEST_ASSERT_TRUE(screen.view().staticMarkCount > 0);

  RadarSettings s = screen.settings();
  s.showAirports = false;
  s.showAirspace = true;
  s.showRoads = true;
  screen.setSettings(s);
  const RadarView v = screen.view();
  for (std::size_t i = 0; i < v.staticMarkCount; ++i) {
    TEST_ASSERT_NOT_EQUAL(static_cast<int>(RadarStaticMark::Kind::Airport),
                          static_cast<int>(v.staticMarks[i].kind));
    TEST_ASSERT_NOT_EQUAL(static_cast<int>(RadarStaticMark::Kind::Poi),
                          static_cast<int>(v.staticMarks[i].kind));
  }
  TEST_ASSERT_TRUE(v.airspaceRingCount > 0 || v.highwayCount > 0);
}

void test_radar_settings_gate_airspace_and_roads(void) {
  ScreenRadar screen;
  screen.bindMapContext(loadMapContextDaytonFixture());
  RadarSettings s = screen.settings();
  s.showAirspace = false;
  s.showRoads = false;
  screen.setSettings(s);
  const RadarView v = screen.view();
  TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(v.airspaceRingCount));
  TEST_ASSERT_EQUAL_UINT32(0, static_cast<uint32_t>(v.highwayCount));
}

void test_radar_prefs_round_trip(void) {
  RadarSettings in = radarSettingsFactoryDefaults();
  in.declutter = RadarDeclutterMode::TargetOnly;
  in.showRoads = false;
  in.demoMode = true;
  const char* path = "radar_prefs_test.bin";
  TEST_ASSERT_TRUE(saveRadarSettingsToFile(in, path));
  RadarSettings out{};
  TEST_ASSERT_TRUE(loadRadarSettingsFromFile(out, path));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(in.declutter),
                          static_cast<uint8_t>(out.declutter));
  TEST_ASSERT_FALSE(out.showRoads);
  TEST_ASSERT_TRUE(out.demoMode);
  std::remove(path);
}

void test_radar_prefs_corrupt_uses_defaults(void) {
  const char* path = "radar_prefs_bad.bin";
  FILE* f = std::fopen(path, "wb");
  TEST_ASSERT_NOT_NULL(f);
  const char junk[] = "nope";
  std::fwrite(junk, 1, sizeof(junk), f);
  std::fclose(f);
  RadarSettings out{};
  TEST_ASSERT_FALSE(loadRadarSettingsFromFile(out, path));
  const auto d = radarSettingsFactoryDefaults();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(d.declutter),
                          static_cast<uint8_t>(out.declutter));
  std::remove(path);
}

void test_radar_idle_settle_closes_settings(void) {
  ScreenRadar screen;
  screen.openSettings();
  TEST_ASSERT_TRUE(screen.settingsOpen());
  screen.onIdleSettle();
  TEST_ASSERT_FALSE(screen.settingsOpen());
}

void test_idle_settle_clears_selection_keeps_range(void) {
  ScreenRadar screen;
  screen.bind(loadAdsbFixture());
  paintFullRevolution(screen);
  screen.onRotate(1);
  const float rangeAfterZoom = screen.rangeMiles();
  TEST_ASSERT_TRUE(screen.selectBlip(0));
  TEST_ASSERT_TRUE(screen.hasSelection());
  screen.onIdleSettle();
  TEST_ASSERT_FALSE(screen.hasSelection());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, rangeAfterZoom, screen.rangeMiles());
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
  RUN_TEST(test_parse_adsb_type_reg_squawk);
  RUN_TEST(test_radar_notable_military_and_watchlist);
  RUN_TEST(test_parse_adsb_calc_track_and_geom_rate_fallback);
  RUN_TEST(test_parse_adsb_missing_track_rate);
  RUN_TEST(test_radar_sweep_advances_and_wraps);
  RUN_TEST(test_radar_classic_paints_on_sweep_not_bind);
  RUN_TEST(test_radar_bind_preserves_selection_by_callsign);
  RUN_TEST(test_radar_bind_clears_selection_when_gone);
  RUN_TEST(test_radar_zoom_preserves_selection_by_callsign);
  RUN_TEST(test_radar_temp_vs_pin_center);
  RUN_TEST(test_map_context_poller_debounces_center);
  RUN_TEST(test_map_context_poller_only_when_active);
  RUN_TEST(test_map_context_poller_stops_on_persistent_failure);
  RUN_TEST(test_map_context_url_from_radar_home);
  RUN_TEST(test_adsb_url_from_radar_home);
  RUN_TEST(test_idle_settle_clears_selection_keeps_range);
  RUN_TEST(test_radar_settings_gate_airports);
  RUN_TEST(test_radar_settings_gate_airspace_and_roads);
  RUN_TEST(test_radar_prefs_round_trip);
  RUN_TEST(test_radar_prefs_corrupt_uses_defaults);
  RUN_TEST(test_radar_idle_settle_closes_settings);
  RUN_TEST(test_radar_set_pois_adds_home_mark);
  RUN_TEST(test_radar_bind_map_context_projects_airport);
  RUN_TEST(test_radar_bind_map_context_projects_highways_and_shelves);
  RUN_TEST(test_radar_static_selection_survives_reproject);
  RUN_TEST(test_radar_static_select_clears_blip_select);
  return UNITY_END();
}
