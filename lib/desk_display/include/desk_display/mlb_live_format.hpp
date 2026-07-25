#pragma once

#include "desk_display/scores.hpp"

#include <cstddef>

namespace desk_display {

constexpr std::size_t kMlbCountLineLen = 32;
constexpr std::size_t kMlbBasesLineLen = 32;
constexpr std::size_t kMlbPitchersLineLen = 72;

/** e.g. "2-1 · 1 out" / "2 outs". Empty if nothing to show. */
void formatMlbCountLine(char* dest, std::size_t destLen, const MlbScores& m);

/** e.g. "Empty", "1st", "1st & 2nd", "Loaded". Empty if base flags absent. */
void formatMlbBasesLine(char* dest, std::size_t destLen, const MlbScores& m);

/** e.g. "M. Murakami · A. Blubaugh". Empty if both names missing. */
void formatMlbBatterPitcherLine(char* dest, std::size_t destLen, const MlbScores& m);

}  // namespace desk_display
