#pragma once

#include <cstddef>
#include <cstdint>

namespace desk_display {

constexpr std::size_t kMaxScoreStr = 32;
constexpr std::size_t kMaxInningStr = 32;
constexpr std::size_t kMaxIsoStr = 40;
constexpr std::size_t kMaxMlbMatchup = 64;
constexpr std::size_t kMaxMlbWhenEt = 32;
constexpr std::size_t kMaxMlbRecord = 16;
constexpr std::size_t kMaxMlbStanding = 48;
constexpr std::size_t kMaxMlbAbbr = 8;
constexpr std::size_t kMaxMlbPlayerName = 32;
constexpr std::size_t kMaxMlbStat = 8;
constexpr std::size_t kMaxMlbPlayerSummary = 48;
constexpr std::size_t kMaxRaceId = 48;
constexpr std::size_t kMaxRaceName = 96;
constexpr std::size_t kMaxTrackName = 96;
constexpr std::size_t kMaxLeagueName = 96;
constexpr std::size_t kMaxSeasonName = 96;
constexpr std::size_t kMaxSeriesName = 96;
constexpr std::size_t kMaxRaceStatus = 32;

enum class MlbHomeAway : uint8_t { Unknown = 0, Home, Away };

struct MlbScores {
  bool live;
  bool hasScore;
  char score[kMaxScoreStr];
  bool hasInning;
  char inning[kMaxInningStr];
  bool hasNextGame;
  char nextGame[kMaxIsoStr];
  bool hasMatchup;
  char matchup[kMaxMlbMatchup];
  bool hasWhenEt;
  char whenEt[kMaxMlbWhenEt];
  bool hasRecord;
  char record[kMaxMlbRecord];
  bool hasStandingLine;
  char standingLine[kMaxMlbStanding];
  bool hasTeamAbbr;
  char teamAbbr[kMaxMlbAbbr];
  bool hasOpponentAbbr;
  char opponentAbbr[kMaxMlbAbbr];
  MlbHomeAway homeAway;

  bool hasTeamRuns;
  int teamRuns;
  bool hasOpponentRuns;
  int opponentRuns;
  bool hasBalls;
  int balls;
  bool hasStrikes;
  int strikes;
  bool hasOuts;
  int outs;
  bool hasOnFirst;
  bool onFirst;
  bool hasOnSecond;
  bool onSecond;
  bool hasOnThird;
  bool onThird;
  bool hasBatterName;
  char batterName[kMaxMlbPlayerName];
  bool hasBatterAvg;
  char batterAvg[kMaxMlbStat];
  bool hasBatterSummary;
  char batterSummary[kMaxMlbPlayerSummary];
  bool hasPitcherName;
  char pitcherName[kMaxMlbPlayerName];
  bool hasPitcherEra;
  char pitcherEra[kMaxMlbStat];
  bool hasPitcherSummary;
  char pitcherSummary[kMaxMlbPlayerSummary];
};

struct FlagstandRace {
  bool present;
  char id[kMaxRaceId];
  char name[kMaxRaceName];
  char scheduledAt[kMaxIsoStr];
  bool hasTrackName;
  char trackName[kMaxTrackName];
  char leagueName[kMaxLeagueName];
  char seasonName[kMaxSeasonName];
  bool hasSeriesName;
  char seriesName[kMaxSeriesName];
  bool hasStatus;
  char status[kMaxRaceStatus];
};

struct FlagstandScores {
  FlagstandRace lastResult;
  FlagstandRace nextRace;
};

struct Scores {
  MlbScores mlb;
  FlagstandScores flagstand;
  bool hasUpdatedAt;
  char updatedAt[kMaxIsoStr];
};

bool parseScores(const char* json, Scores& out);

}  // namespace desk_display
