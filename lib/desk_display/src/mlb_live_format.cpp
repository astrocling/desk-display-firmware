#include "desk_display/mlb_live_format.hpp"

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

}  // namespace

void formatMlbCountLine(char* dest, std::size_t destLen, const MlbScores& m) {
  if (!dest || destLen == 0) {
    return;
  }
  dest[0] = '\0';

  char count[16]{};
  if (m.hasBalls && m.hasStrikes) {
    std::snprintf(count, sizeof(count), "%d-%d", m.balls, m.strikes);
  }

  char outs[16]{};
  if (m.hasOuts) {
    std::snprintf(outs, sizeof(outs), "%d out%s", m.outs, m.outs == 1 ? "" : "s");
  }

  if (count[0] && outs[0]) {
    std::snprintf(dest, destLen, "%s - %s", count, outs);
  } else if (count[0]) {
    copyStr(dest, destLen, count);
  } else if (outs[0]) {
    copyStr(dest, destLen, outs);
  }
}

void formatMlbBasesLine(char* dest, std::size_t destLen, const MlbScores& m) {
  if (!dest || destLen == 0) {
    return;
  }
  dest[0] = '\0';

  if (!m.hasOnFirst && !m.hasOnSecond && !m.hasOnThird) {
    return;
  }

  const bool first = m.hasOnFirst && m.onFirst;
  const bool second = m.hasOnSecond && m.onSecond;
  const bool third = m.hasOnThird && m.onThird;
  // Order: 2nd, 3rd, 1st, home (home always empty — decorative 4th diamond).
  std::snprintf(dest, destLen, "%c%c%c.", second ? '*' : '.', third ? '*' : '.',
                first ? '*' : '.');
}

void formatMlbBatterPitcherLine(char* dest, std::size_t destLen, const MlbScores& m) {
  if (!dest || destLen == 0) {
    return;
  }
  dest[0] = '\0';

  const char* batter = (m.hasBatterName && m.batterName[0]) ? m.batterName : nullptr;
  const char* pitcher =
      (m.hasPitcherName && m.pitcherName[0]) ? m.pitcherName : nullptr;

  if (batter && pitcher) {
    std::snprintf(dest, destLen, "%s - %s", batter, pitcher);
  } else if (batter) {
    copyStr(dest, destLen, batter);
  } else if (pitcher) {
    copyStr(dest, destLen, pitcher);
  }
}

}  // namespace desk_display
