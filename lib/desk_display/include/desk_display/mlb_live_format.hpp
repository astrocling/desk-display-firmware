#pragma once

#include "desk_display/scores.hpp"

#include <cstddef>

namespace desk_display {

constexpr std::size_t kMlbCountLineLen = 32;
constexpr std::size_t kMlbBasesLineLen = 32;
constexpr std::size_t kMlbPitchersLineLen = 160;

/** e.g. "2-1 - 1 out" / "2 outs". Empty if nothing to show. */
void formatMlbCountLine(char* dest, std::size_t destLen, const MlbScores& m);

/**
 * Four-diamond mask: positions 2nd, 3rd, 1st, home.
 * `*` = occupied, `.` = empty; home is always `.`.
 * Empty string if base flags absent.
 */
void formatMlbBasesLine(char* dest, std::size_t destLen, const MlbScores& m);

/** e.g. "AB: A. Judge .311 - 1-3, BB\nP: F. Valdez 2.85 - 5.0 IP, 2 ER". Empty if both names missing. */
void formatMlbBatterPitcherLine(char* dest, std::size_t destLen, const MlbScores& m);

}  // namespace desk_display
