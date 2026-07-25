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

void appendRoleLine(char* dest, std::size_t destLen, const char* prefix, const char* name,
                    const char* seasonStat, const char* summary) {
  if (!dest || destLen == 0 || !name || !name[0]) {
    return;
  }

  std::size_t offset = std::strlen(dest);
  char* p = dest + offset;
  std::size_t rem = destLen - offset;

  int n = std::snprintf(p, rem, "%s%s", prefix, name);
  if (n < 0 || static_cast<std::size_t>(n) >= rem) {
    dest[destLen - 1] = '\0';
    return;
  }
  offset += static_cast<std::size_t>(n);
  p = dest + offset;
  rem = destLen - offset;

  if (seasonStat && seasonStat[0]) {
    n = std::snprintf(p, rem, " %s", seasonStat);
    if (n < 0 || static_cast<std::size_t>(n) >= rem) {
      dest[destLen - 1] = '\0';
      return;
    }
    offset += static_cast<std::size_t>(n);
    p = dest + offset;
    rem = destLen - offset;
  }

  if (summary && summary[0]) {
    std::snprintf(p, rem, " · %s", summary);
  }
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
  const char* batterAvg = (m.hasBatterAvg && m.batterAvg[0]) ? m.batterAvg : nullptr;
  const char* batterSummary =
      (m.hasBatterSummary && m.batterSummary[0]) ? m.batterSummary : nullptr;
  const char* pitcher = (m.hasPitcherName && m.pitcherName[0]) ? m.pitcherName : nullptr;
  const char* pitcherEra = (m.hasPitcherEra && m.pitcherEra[0]) ? m.pitcherEra : nullptr;
  const char* pitcherSummary =
      (m.hasPitcherSummary && m.pitcherSummary[0]) ? m.pitcherSummary : nullptr;

  if (batter) {
    appendRoleLine(dest, destLen, "AB: ", batter, batterAvg, batterSummary);
  }
  if (pitcher) {
    if (batter && dest[0]) {
      const std::size_t len = std::strlen(dest);
      if (len + 1 < destLen) {
        dest[len] = '\n';
        dest[len + 1] = '\0';
      }
    }
    appendRoleLine(dest, destLen, "P: ", pitcher, pitcherEra, pitcherSummary);
  }
}

}  // namespace desk_display
