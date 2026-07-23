#include "desk_display/screen_sports.hpp"

#include <cstdio>
#include <cstring>

namespace desk_display {
namespace {

void copyStr(char* dest, std::size_t destLen, const char* src) {
  if (destLen == 0) {
    return;
  }
  if (!src) {
    dest[0] = '\0';
    return;
  }
  std::strncpy(dest, src, destLen - 1);
  dest[destLen - 1] = '\0';
}

int wrapIndex(int index, int count) {
  if (count <= 0) {
    return 0;
  }
  int m = index % count;
  if (m < 0) {
    m += count;
  }
  return m;
}

}  // namespace

ScreenSports::ScreenSports() { reset(); }

void ScreenSports::reset() {
  ready_ = false;
  detail_ = false;
  card_ = SportsCard::Mlb;
  std::memset(&scores_, 0, sizeof(scores_));
}

void ScreenSports::bind(const Scores& scores) {
  scores_ = scores;
  ready_ = true;
  detail_ = false;
  card_ = SportsCard::Mlb;
}

void ScreenSports::unbind() { reset(); }

void ScreenSports::onRotate(int delta) {
  if (!ready_ || delta == 0) {
    return;
  }
  const int count = static_cast<int>(kSportsCardCount);
  int next = static_cast<int>(card_) + delta;
  card_ = static_cast<SportsCard>(wrapIndex(next, count));
}

void ScreenSports::onTap() {
  if (!ready_) {
    return;
  }
  detail_ = true;
}

void ScreenSports::exitDetail() { detail_ = false; }

void ScreenSports::formatRaceSummary(char* dest, std::size_t destLen,
                                     const FlagstandRace& race) {
  if (!dest || destLen == 0) {
    return;
  }
  dest[0] = '\0';
  if (!race.present) {
    return;
  }
  if (race.hasTrackName && race.trackName[0] != '\0') {
    std::snprintf(dest, destLen, "%s @ %s", race.name, race.trackName);
  } else {
    copyStr(dest, destLen, race.name);
  }
}

void ScreenSports::fillMlbView(SportsMlbView& out) const {
  std::memset(&out, 0, sizeof(out));
  if (!ready_) {
    return;
  }

  const MlbScores& m = scores_.mlb;
  out.live = m.live;
  out.hasScore = m.hasScore;
  out.hasInning = m.hasInning;
  out.hasNextGame = m.hasNextGame;
  copyStr(out.score, sizeof(out.score), m.score);
  copyStr(out.inning, sizeof(out.inning), m.inning);
  copyStr(out.nextGame, sizeof(out.nextGame), m.nextGame);

  if (m.live) {
    if (m.hasScore) {
      copyStr(out.primary, sizeof(out.primary), m.score);
    }
    if (m.hasInning) {
      copyStr(out.secondary, sizeof(out.secondary), m.inning);
    }
  } else if (m.hasNextGame) {
    copyStr(out.primary, sizeof(out.primary), m.nextGame);
  }
}

void ScreenSports::fillFlagstandView(SportsFlagstandView& out) const {
  std::memset(&out, 0, sizeof(out));
  if (!ready_) {
    return;
  }

  out.lastResult = scores_.flagstand.lastResult;
  out.nextRace = scores_.flagstand.nextRace;
  out.hasLastResult = out.lastResult.present;
  out.hasNextRace = out.nextRace.present;

  if (out.hasLastResult) {
    formatRaceSummary(out.lastResultSummary, sizeof(out.lastResultSummary),
                      out.lastResult);
  }
  if (out.hasNextRace) {
    formatRaceSummary(out.nextRaceSummary, sizeof(out.nextRaceSummary),
                      out.nextRace);
  }
}

SportsView ScreenSports::view() const {
  SportsView v{};
  v.ready = ready_;
  v.detail = detail_;
  v.card = card_;
  fillMlbView(v.mlb);
  fillFlagstandView(v.flagstand);
  return v;
}

}  // namespace desk_display
