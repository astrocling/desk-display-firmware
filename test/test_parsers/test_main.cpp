#include <unity.h>

#include <cstring>

#include "desk_display/adsb.hpp"
#include "desk_display/airport.hpp"
#include "desk_display/map_context.hpp"
#include "desk_display/not_ready.hpp"
#include "desk_display/scores.hpp"
#include "desk_display/timezones.hpp"
#include "desk_display/weather.hpp"
#include "fixture_loader.hpp"

using namespace desk_display;

static char g_buf[256 * 1024];

void test_parse_weather_fixture(void) {
  TEST_ASSERT_TRUE_MESSAGE(loadFixture("weather.json", g_buf, sizeof(g_buf)),
                           "load weather.json");

  Weather w{};
  TEST_ASSERT_TRUE(parseWeather(g_buf, w));
  TEST_ASSERT_TRUE(w.currentTemp > 0.0f);
  TEST_ASSERT_TRUE(w.hourlyCount > 0);
  TEST_ASSERT_FALSE(w.alert.present);  // live fixture has alert: null
  TEST_ASSERT_TRUE(w.hasUpdatedAt);
  TEST_ASSERT_TRUE(std::strlen(w.updatedAt) > 0);
}

void test_parse_weather_with_alert(void) {
  const char* json =
      R"({"current":{"temp":70,"feelsLike":68,"code":3},"todayHigh":80,"todayLow":60,)"
      R"("hourly":[{"time":"2026-07-23T12:00","temp":72,"code":3}],)"
      R"("alert":{"severity":"Moderate","headline":"Heat Advisory"}})";

  Weather w{};
  TEST_ASSERT_TRUE(parseWeather(json, w));
  TEST_ASSERT_TRUE(w.alert.present);
  TEST_ASSERT_EQUAL_STRING("Moderate", w.alert.severity);
  TEST_ASSERT_EQUAL_STRING("Heat Advisory", w.alert.headline);
  TEST_ASSERT_FALSE(w.hasUpdatedAt);
}

void test_parse_weather_rejects_bad(void) {
  Weather w{};
  TEST_ASSERT_FALSE(parseWeather("{", w));
  TEST_ASSERT_FALSE(parseWeather("{\"error\":\"weather not ready\"}", w));
}

void test_parse_timezones_fixture(void) {
  TEST_ASSERT_TRUE(loadFixture("timezones.json", g_buf, sizeof(g_buf)));

  Timezones tz{};
  TEST_ASSERT_TRUE(parseTimezones(g_buf, tz));
  TEST_ASSERT_TRUE(tz.count >= 7);

  bool foundNy = false;
  for (std::size_t i = 0; i < tz.count; ++i) {
    if (std::strcmp(tz.entries[i].iana, "America/New_York") == 0) {
      foundNy = true;
      TEST_ASSERT_TRUE(std::strlen(tz.entries[i].sunrise) > 0);
      TEST_ASSERT_TRUE(std::strlen(tz.entries[i].sunset) > 0);
    }
  }
  TEST_ASSERT_TRUE(foundNy);
}

void test_parse_scores_fixture(void) {
  TEST_ASSERT_TRUE(loadFixture("scores.json", g_buf, sizeof(g_buf)));

  Scores s{};
  TEST_ASSERT_TRUE(parseScores(g_buf, s));
  TEST_ASSERT_FALSE(s.mlb.live);
  TEST_ASSERT_FALSE(s.mlb.hasScore);
  TEST_ASSERT_TRUE(s.mlb.hasNextGame);
  TEST_ASSERT_TRUE(s.mlb.hasMatchup);
  TEST_ASSERT_EQUAL_STRING("Astros @ White Sox", s.mlb.matchup);
  TEST_ASSERT_TRUE(s.mlb.hasWhenEt);
  TEST_ASSERT_EQUAL_STRING("Fri 7/24 7:40 PM", s.mlb.whenEt);
  TEST_ASSERT_TRUE(s.mlb.hasRecord);
  TEST_ASSERT_EQUAL_STRING("50-54", s.mlb.record);
  TEST_ASSERT_TRUE(s.mlb.hasStandingLine);
  TEST_ASSERT_EQUAL_STRING("3rd AL West · 2 GB", s.mlb.standingLine);
  TEST_ASSERT_TRUE(s.mlb.hasTeamAbbr);
  TEST_ASSERT_EQUAL_STRING("HOU", s.mlb.teamAbbr);
  TEST_ASSERT_TRUE(s.mlb.hasOpponentAbbr);
  TEST_ASSERT_EQUAL_STRING("CHW", s.mlb.opponentAbbr);
  TEST_ASSERT_EQUAL(static_cast<int>(MlbHomeAway::Away),
                    static_cast<int>(s.mlb.homeAway));
  TEST_ASSERT_TRUE(s.flagstand.lastResult.present);
  TEST_ASSERT_EQUAL_STRING("Round 8", s.flagstand.lastResult.name);
  TEST_ASSERT_TRUE(s.flagstand.lastResult.hasTrackName);
  TEST_ASSERT_FALSE(s.flagstand.nextRace.present);
  TEST_ASSERT_TRUE(s.hasUpdatedAt);
}

void test_parse_scores_home_away_home(void) {
  const char* json =
      R"({"mlb":{"live":false,"score":null,"inning":null,"nextGame":null,)"
      R"("matchup":"Astros @ White Sox","whenEt":"Fri 7/24 7:40 PM",)"
      R"("record":"50-54","standingLine":"3rd AL West · 2 GB",)"
      R"("teamAbbr":"HOU","opponentAbbr":"CHW","homeAway":"home"},)"
      R"("flagstand":{"lastResult":null,"nextRace":null}})";

  Scores s{};
  TEST_ASSERT_TRUE(parseScores(json, s));
  TEST_ASSERT_EQUAL(static_cast<int>(MlbHomeAway::Home),
                    static_cast<int>(s.mlb.homeAway));
}

void test_parse_scores_abbrs_optional(void) {
  const char* json =
      R"({"mlb":{"live":true,"score":"4-2","inning":"Top 7","nextGame":null},)"
      R"("flagstand":{"lastResult":null,"nextRace":null}})";

  Scores s{};
  TEST_ASSERT_TRUE(parseScores(json, s));
  TEST_ASSERT_FALSE(s.mlb.hasTeamAbbr);
  TEST_ASSERT_FALSE(s.mlb.hasOpponentAbbr);
  TEST_ASSERT_EQUAL(static_cast<int>(MlbHomeAway::Unknown),
                    static_cast<int>(s.mlb.homeAway));
}

void test_parse_scores_live_situation(void) {
  const char* json =
      R"({"mlb":{"live":true,"score":"0-3","inning":"Bot 2","nextGame":null,)"
      R"("teamAbbr":"HOU","opponentAbbr":"CHW","homeAway":"away",)"
      R"("teamRuns":0,"opponentRuns":3,"balls":2,"strikes":1,"outs":1,)"
      R"("onFirst":false,"onSecond":true,"onThird":false,)"
      R"("batterName":"M. Murakami","pitcherName":"A. Blubaugh"},)"
      R"("flagstand":{"lastResult":null,"nextRace":null}})";

  Scores s{};
  TEST_ASSERT_TRUE(parseScores(json, s));
  TEST_ASSERT_TRUE(s.mlb.live);
  TEST_ASSERT_TRUE(s.mlb.hasTeamRuns);
  TEST_ASSERT_EQUAL(0, s.mlb.teamRuns);
  TEST_ASSERT_TRUE(s.mlb.hasOpponentRuns);
  TEST_ASSERT_EQUAL(3, s.mlb.opponentRuns);
  TEST_ASSERT_TRUE(s.mlb.hasBalls);
  TEST_ASSERT_EQUAL(2, s.mlb.balls);
  TEST_ASSERT_TRUE(s.mlb.hasOnSecond);
  TEST_ASSERT_TRUE(s.mlb.onSecond);
  TEST_ASSERT_TRUE(s.mlb.hasBatterName);
  TEST_ASSERT_EQUAL_STRING("M. Murakami", s.mlb.batterName);
}

void test_parse_scores_flagstand_next_with_status(void) {
  const char* json =
      R"({"mlb":{"live":true,"score":"4-2","inning":"Top 7","nextGame":null},)"
      R"("flagstand":{"lastResult":null,"nextRace":{)"
      R"("id":"abc","name":"Race Night 13","scheduledAt":"2026-07-27T00:00:00.000Z",)"
      R"("trackName":"Main Track","leagueName":"League A","seasonName":"2026",)"
      R"("status":"SCHEDULED"}},"updatedAt":"2026-07-23T12:00:00.000Z"})";

  Scores s{};
  TEST_ASSERT_TRUE(parseScores(json, s));
  TEST_ASSERT_TRUE(s.mlb.live);
  TEST_ASSERT_EQUAL_STRING("4-2", s.mlb.score);
  TEST_ASSERT_TRUE(s.flagstand.nextRace.present);
  TEST_ASSERT_TRUE(s.flagstand.nextRace.hasStatus);
  TEST_ASSERT_EQUAL_STRING("SCHEDULED", s.flagstand.nextRace.status);
}

void test_parse_airport_fixture(void) {
  TEST_ASSERT_TRUE(loadFixture("airport_kday.json", g_buf, sizeof(g_buf)));

  Airport a{};
  TEST_ASSERT_TRUE(parseAirport(g_buf, a));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 39.902f, static_cast<float>(a.lat));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, -84.219f, static_cast<float>(a.lon));
}

void test_parse_map_context_dayton(void) {
  TEST_ASSERT_TRUE(loadFixture("map_context_dayton.json", g_buf, sizeof(g_buf)));

  MapContext ctx{};
  TEST_ASSERT_TRUE(parseMapContext(g_buf, ctx));
  TEST_ASSERT_EQUAL(2, ctx.airportCount);
  TEST_ASSERT_EQUAL_STRING("KDAY", ctx.airports[0].icao);
  TEST_ASSERT_EQUAL(3, ctx.ringCount);
  TEST_ASSERT_TRUE(ctx.rings[0].cls == AirspaceClass::D);
  TEST_ASSERT_TRUE(ctx.rings[0].pointCount >= 4);
  TEST_ASSERT_EQUAL_STRING("KDAY_D_0", ctx.rings[0].id);
  TEST_ASSERT_EQUAL(2, ctx.highwayCount);
  TEST_ASSERT_EQUAL_STRING("I-75", ctx.highways[0].route);
  TEST_ASSERT_TRUE(ctx.highways[0].pointCount >= 2);
}

void test_parse_map_context_rejects_bad_class(void) {
  MapContext ctx{};
  TEST_ASSERT_TRUE(parseMapContext(
      "{\"airports\":[],\"rings\":[{\"class\":\"E\",\"id\":\"x\",\"points\":[[1,2],[3,4],[5,6]]}]}",
      ctx));
  TEST_ASSERT_EQUAL(0, ctx.ringCount);
}

void test_parse_adsb_fixture(void) {
  TEST_ASSERT_TRUE(loadFixture("adsb_sample.json", g_buf, sizeof(g_buf)));

  AircraftList list{};
  TEST_ASSERT_TRUE(parseAdsb(g_buf, list));
  TEST_ASSERT_TRUE(list.count > 0);
  TEST_ASSERT_TRUE(list.items[0].hasPosition);
  TEST_ASSERT_TRUE(std::strlen(list.items[0].callsign) > 0);

  AircraftList filtered{};
  const std::size_t n =
      filterAircraftByRange(list, 40.03353, -84.19588, 25.0f, filtered);
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_EQUAL(n, filtered.count);
  TEST_ASSERT_TRUE(n <= list.count);
}

void test_not_ready_helper(void) {
  TEST_ASSERT_TRUE(isNotReadyError("{\"error\":\"weather not ready\"}"));
  TEST_ASSERT_TRUE(isNotReadyError("{\"error\":\"scores not ready\"}"));
  TEST_ASSERT_FALSE(isNotReadyError("{\"ok\":true}"));
  TEST_ASSERT_FALSE(isNotReadyError("{"));
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_parse_weather_fixture);
  RUN_TEST(test_parse_weather_with_alert);
  RUN_TEST(test_parse_weather_rejects_bad);
  RUN_TEST(test_parse_timezones_fixture);
  RUN_TEST(test_parse_scores_fixture);
  RUN_TEST(test_parse_scores_home_away_home);
  RUN_TEST(test_parse_scores_abbrs_optional);
  RUN_TEST(test_parse_scores_live_situation);
  RUN_TEST(test_parse_scores_flagstand_next_with_status);
  RUN_TEST(test_parse_airport_fixture);
  RUN_TEST(test_parse_map_context_dayton);
  RUN_TEST(test_parse_map_context_rejects_bad_class);
  RUN_TEST(test_parse_adsb_fixture);
  RUN_TEST(test_not_ready_helper);
  return UNITY_END();
}
