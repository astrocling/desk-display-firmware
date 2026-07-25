#pragma once

#include "desk_display/mlb_live_format.hpp"
#include "desk_display/scores.hpp"

#include <cstddef>
#include <cstdint>

namespace desk_display {

/** Only MLB + Flagstand in this phase (no WoO / High Limit / USAC slots). */
enum class SportsCard : uint8_t {
  Mlb = 0,
  Flagstand = 1,
};

constexpr std::size_t kSportsCardCount = 2;
constexpr std::size_t kSportsSummaryLen = 128;

/** Compact MLB card for list / summary mode. */
struct SportsMlbView {
  bool live;
  char primary[kSportsSummaryLen];    // live: score; else: matchup (fallback nextGame)
  char secondary[kSportsSummaryLen];  // live: inning; else: whenEt
  bool hasScore;
  bool hasInning;
  bool hasNextGame;
  bool hasMatchup;
  bool hasWhenEt;
  bool hasRecord;
  bool hasStandingLine;
  char score[kMaxScoreStr];
  char inning[kMaxInningStr];
  char nextGame[kMaxIsoStr];
  char matchup[kMaxMlbMatchup];
  char whenEt[kMaxMlbWhenEt];
  char record[kMaxMlbRecord];
  char standingLine[kMaxMlbStanding];
  bool hasTeamAbbr;
  char teamAbbr[kMaxMlbAbbr];
  bool hasOpponentAbbr;
  char opponentAbbr[kMaxMlbAbbr];
  MlbHomeAway homeAway;
  bool hasConnector;
  char connector[4];
  bool showLogoHero;
  /** Live scorebug with logos + split runs. */
  bool showLiveScorebug;
  bool hasTeamRuns;
  int teamRuns;
  bool hasOpponentRuns;
  int opponentRuns;
  char countLine[kMlbCountLineLen];
  /** Compact diamond mask from formatMlbBasesLine (2nd/3rd/1st/home). */
  char basesLine[kMlbBasesLineLen];
  bool hasBases;
  bool onFirst;
  bool onSecond;
  bool onThird;
  char batterPitcherLine[kMlbPitchersLineLen];
};

/** Compact Flagstand card: last result + optional next race. */
struct SportsFlagstandView {
  bool hasLastResult;
  char lastResultSummary[kSportsSummaryLen];
  bool hasNextRace;
  char nextRaceSummary[kSportsSummaryLen];
  FlagstandRace lastResult;
  FlagstandRace nextRace;
};

/** Snapshot the UI layer will render (no LVGL). */
struct SportsView {
  bool ready;
  bool detail;
  SportsCard card;
  SportsMlbView mlb;
  SportsFlagstandView flagstand;
};

/**
 * Sports screen view-model: rotate MLB ↔ Flagstand, tap for detail.
 * Center-tap (back to carousel) is owned by Nav — not handled here.
 */
class ScreenSports {
 public:
  ScreenSports();

  void reset();

  /** Bind parsed scores. Marks screen ready. */
  void bind(const Scores& scores);

  /** Clear bound data; not ready until bind again. */
  void unbind();

  bool ready() const { return ready_; }
  bool detail() const { return detail_; }
  SportsCard card() const { return card_; }
  const Scores& scores() const { return scores_; }

  /** Encoder rotate while focused: cycle MLB ↔ Flagstand. */
  void onRotate(int delta);

  /** Tap current card → enter detail mode (richer fields via view()). */
  void onTap();

  /** Leave detail mode back to card summary. */
  void exitDetail();

  /** Focused idle settle: leave detail mode. */
  void onIdleSettle();

  SportsView view() const;

 private:
  void fillMlbView(SportsMlbView& out) const;
  void fillFlagstandView(SportsFlagstandView& out) const;
  static void formatRaceSummary(char* dest, std::size_t destLen,
                                const FlagstandRace& race);

  bool ready_;
  bool detail_;
  SportsCard card_;
  Scores scores_;
};

}  // namespace desk_display
