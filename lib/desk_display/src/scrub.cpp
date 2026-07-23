#include "desk_display/scrub.hpp"

namespace desk_display {

std::int64_t applyScrubOffset(std::int64_t anchorUnix, int steps) {
  return anchorUnix + static_cast<std::int64_t>(steps) * kScrubStepMinutes * 60;
}

}  // namespace desk_display
