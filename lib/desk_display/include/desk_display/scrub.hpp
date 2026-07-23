#pragma once

#include <cstdint>

namespace desk_display {

constexpr int kScrubStepMinutes = 60;

/** Apply scrub offset of `steps` × 60 minutes relative to `anchorUnix`. */
std::int64_t applyScrubOffset(std::int64_t anchorUnix, int steps);

}  // namespace desk_display
