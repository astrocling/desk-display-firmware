#include <unity.h>

#include <cstring>

#include "desk_display/scores.hpp"
#include "desk_display/screen_sports.hpp"
#include "fixture_loader.hpp"

using namespace desk_display;

static char g_buf[256 * 1024];
static ScreenSports g_screen;

void setUp(void) { g_screen.reset(); }

void tearDown(void) {}

void test_unbound_not_ready(void) {
  TEST_ASSERT_FALSE(g_screen.ready());
  SportsView v = g_screen.view();
  TEST_ASSERT_FALSE(v.ready);
  TEST_ASSERT_FALSE(v.detail);
  TEST_ASSERT_EQUAL(static_cast<int>(SportsCard::Mlb),
                    static_cast<int>(v.card));
}

void test_bind_fixture_ready_mlb_next_game(void) {
  TEST_ASSERT_TRUE_MESSAGE(loadFixture("scores.json", g_buf, sizeof(g_buf)),
                           "load scores.json");

  Scores s{};
  TEST_ASSERT_TRUE(parseScores(g_buf, s));
  TEST_ASSERT_TRUE(s.mlb.hasMatchup);
  TEST_ASSERT_EQUAL_STRING("Astros @ White Sox", s.mlb.matchup);
  TEST_ASSERT_TRUE(s.mlb.hasWhenEt);
  TEST_ASSERT_EQUAL_STRING("Fri 7/24 7:40 PM", s.mlb.whenEt);
  TEST_ASSERT_TRUE(s.mlb.hasRecord);
  TEST_ASSERT_EQUAL_STRING("50-54", s.mlb.record);
  TEST_ASSERT_TRUE(s.mlb.hasStandingLine);
  TEST_ASSERT_EQUAL_STRING("3rd AL West · 2 GB", s.mlb.standingLine);

  g_screen.bind(s);
  TEST_ASSERT_TRUE(g_screen.ready());
  TEST_ASSERT_EQUAL(static_cast<int>(SportsCard::Mlb),
                    static_cast<int>(g_screen.card()));

  SportsView v = g_screen.view();
  TEST_ASSERT_TRUE(v.ready);
  TEST_ASSERT_FALSE(v.mlb.live);
  TEST_ASSERT_TRUE(v.mlb.hasNextGame);
  TEST_ASSERT_TRUE(v.mlb.hasMatchup);
  TEST_ASSERT_EQUAL_STRING("Astros @ White Sox", v.mlb.primary);
  TEST_ASSERT_EQUAL_STRING("Fri 7/24 7:40 PM", v.mlb.secondary);
  TEST_ASSERT_EQUAL_STRING("2026-07-24T23:40Z", v.mlb.nextGame);
  TEST_ASSERT_EQUAL_STRING("50-54", v.mlb.record);
  // View normalizes U+00B7 to ASCII hyphen for LVGL fonts.
  TEST_ASSERT_EQUAL_STRING("3rd AL West - 2 GB", v.mlb.standingLine);
  TEST_ASSERT_TRUE(v.mlb.hasTeamAbbr);
  TEST_ASSERT_EQUAL_STRING("HOU", v.mlb.teamAbbr);
  TEST_ASSERT_TRUE(v.mlb.hasOpponentAbbr);
  TEST_ASSERT_EQUAL_STRING("CHW", v.mlb.opponentAbbr);
  TEST_ASSERT_EQUAL(static_cast<int>(MlbHomeAway::Away),
                    static_cast<int>(v.mlb.homeAway));
  TEST_ASSERT_TRUE(v.mlb.hasConnector);
  TEST_ASSERT_EQUAL_STRING("@", v.mlb.connector);
  TEST_ASSERT_TRUE(v.mlb.showLogoHero);
}

void test_rotate_mlb_flagstand_cycle(void) {
  TEST_ASSERT_TRUE(loadFixture("scores.json", g_buf, sizeof(g_buf)));
  Scores s{};
  TEST_ASSERT_TRUE(parseScores(g_buf, s));
  g_screen.bind(s);

  TEST_ASSERT_EQUAL(static_cast<int>(SportsCard::Mlb),
                    static_cast<int>(g_screen.card()));

  g_screen.onRotate(1);
  TEST_ASSERT_EQUAL(static_cast<int>(SportsCard::Flagstand),
                    static_cast<int>(g_screen.card()));

  SportsView fs = g_screen.view();
  TEST_ASSERT_EQUAL(static_cast<int>(SportsCard::Flagstand),
                    static_cast<int>(fs.card));
  TEST_ASSERT_TRUE(fs.flagstand.hasLastResult);
  TEST_ASSERT_FALSE(fs.flagstand.hasNextRace);
  TEST_ASSERT_EQUAL_STRING("Round 8 @ Lernerville Speedway",
                           fs.flagstand.lastResultSummary);
  TEST_ASSERT_EQUAL_STRING("Round 8", fs.flagstand.lastResult.name);
  TEST_ASSERT_TRUE(fs.flagstand.lastResult.hasTrackName);

  g_screen.onRotate(1);
  TEST_ASSERT_EQUAL(static_cast<int>(SportsCard::Mlb),
                    static_cast<int>(g_screen.card()));

  g_screen.onRotate(-1);
  TEST_ASSERT_EQUAL(static_cast<int>(SportsCard::Flagstand),
                    static_cast<int>(g_screen.card()));
}

void test_mlb_live_score_and_inning(void) {
  Scores s{};
  s.mlb.live = true;
  s.mlb.hasScore = true;
  std::strncpy(s.mlb.score, "4-2", sizeof(s.mlb.score) - 1);
  s.mlb.hasInning = true;
  std::strncpy(s.mlb.inning, "Top 7", sizeof(s.mlb.inning) - 1);
  s.mlb.hasNextGame = false;

  g_screen.bind(s);
  SportsView v = g_screen.view();
  TEST_ASSERT_TRUE(v.mlb.live);
  TEST_ASSERT_EQUAL_STRING("4-2", v.mlb.primary);
  TEST_ASSERT_EQUAL_STRING("Top 7", v.mlb.secondary);
  TEST_ASSERT_EQUAL_STRING("4-2", v.mlb.score);
  TEST_ASSERT_EQUAL_STRING("Top 7", v.mlb.inning);
  TEST_ASSERT_FALSE(v.mlb.showLogoHero);
}

void test_flagstand_next_race_when_present(void) {
  Scores s{};
  s.flagstand.lastResult.present = true;
  std::strncpy(s.flagstand.lastResult.name, "Round 7",
               sizeof(s.flagstand.lastResult.name) - 1);
  s.flagstand.lastResult.hasTrackName = true;
  std::strncpy(s.flagstand.lastResult.trackName, "Home Track",
               sizeof(s.flagstand.lastResult.trackName) - 1);

  s.flagstand.nextRace.present = true;
  std::strncpy(s.flagstand.nextRace.name, "Race Night 13",
               sizeof(s.flagstand.nextRace.name) - 1);
  s.flagstand.nextRace.hasTrackName = true;
  std::strncpy(s.flagstand.nextRace.trackName, "Main Track",
               sizeof(s.flagstand.nextRace.trackName) - 1);
  s.flagstand.nextRace.hasStatus = true;
  std::strncpy(s.flagstand.nextRace.status, "SCHEDULED",
               sizeof(s.flagstand.nextRace.status) - 1);

  g_screen.bind(s);
  g_screen.onRotate(1);

  SportsView v = g_screen.view();
  TEST_ASSERT_TRUE(v.flagstand.hasLastResult);
  TEST_ASSERT_TRUE(v.flagstand.hasNextRace);
  TEST_ASSERT_EQUAL_STRING("Round 7 @ Home Track",
                           v.flagstand.lastResultSummary);
  TEST_ASSERT_EQUAL_STRING("Race Night 13 @ Main Track",
                           v.flagstand.nextRaceSummary);
  TEST_ASSERT_EQUAL_STRING("SCHEDULED", v.flagstand.nextRace.status);
}

void test_detail_toggle(void) {
  TEST_ASSERT_TRUE(loadFixture("scores.json", g_buf, sizeof(g_buf)));
  Scores s{};
  TEST_ASSERT_TRUE(parseScores(g_buf, s));
  g_screen.bind(s);

  TEST_ASSERT_FALSE(g_screen.detail());
  g_screen.onTap();
  TEST_ASSERT_TRUE(g_screen.detail());
  TEST_ASSERT_TRUE(g_screen.view().detail);

  // Detail exposes richer MLB fields (nextGame still available when not live).
  SportsView v = g_screen.view();
  TEST_ASSERT_TRUE(v.mlb.hasNextGame);
  TEST_ASSERT_TRUE(std::strlen(v.mlb.nextGame) > 0);

  g_screen.onRotate(1);
  TEST_ASSERT_TRUE(g_screen.detail());  // detail persists across card change
  SportsView fs = g_screen.view();
  TEST_ASSERT_TRUE(fs.detail);
  TEST_ASSERT_TRUE(fs.flagstand.hasLastResult);
  TEST_ASSERT_TRUE(std::strlen(fs.flagstand.lastResult.leagueName) > 0);
  TEST_ASSERT_TRUE(std::strlen(fs.flagstand.lastResult.seasonName) > 0);
  TEST_ASSERT_TRUE(std::strlen(fs.flagstand.lastResult.id) > 0);

  g_screen.exitDetail();
  TEST_ASSERT_FALSE(g_screen.detail());
  TEST_ASSERT_FALSE(g_screen.view().detail);
}

void test_unbind_clears_ready(void) {
  TEST_ASSERT_TRUE(loadFixture("scores.json", g_buf, sizeof(g_buf)));
  Scores s{};
  TEST_ASSERT_TRUE(parseScores(g_buf, s));
  g_screen.bind(s);
  g_screen.onTap();
  TEST_ASSERT_TRUE(g_screen.ready());
  TEST_ASSERT_TRUE(g_screen.detail());

  g_screen.unbind();
  TEST_ASSERT_FALSE(g_screen.ready());
  TEST_ASSERT_FALSE(g_screen.detail());
  TEST_ASSERT_FALSE(g_screen.view().ready);
}

void test_rotate_ignored_when_unbound(void) {
  g_screen.onRotate(1);
  TEST_ASSERT_EQUAL(static_cast<int>(SportsCard::Mlb),
                    static_cast<int>(g_screen.card()));
  g_screen.onTap();
  TEST_ASSERT_FALSE(g_screen.detail());
}

int main(int argc, char** argv) {
  (void)argc;
  (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_unbound_not_ready);
  RUN_TEST(test_bind_fixture_ready_mlb_next_game);
  RUN_TEST(test_rotate_mlb_flagstand_cycle);
  RUN_TEST(test_mlb_live_score_and_inning);
  RUN_TEST(test_flagstand_next_race_when_present);
  RUN_TEST(test_detail_toggle);
  RUN_TEST(test_unbind_clears_ready);
  RUN_TEST(test_rotate_ignored_when_unbound);
  return UNITY_END();
}
