#include "desk_display/timezone_status.hpp"

namespace desk_display {

TzRowStatus timezoneRowStatus(int localHour) {
  int h = localHour % 24;
  if (h < 0) {
    h += 24;
  }

  if (h >= 9 && h < 17) {
    return TzRowStatus::Working;
  }
  if ((h >= 7 && h < 9) || (h >= 17 && h < 21)) {
    return TzRowStatus::Awake;
  }
  return TzRowStatus::Night;
}

TzRowStatus timezoneRowStatus(int localHour, bool isDaylight) {
  if (!isDaylight) {
    return TzRowStatus::Night;
  }
  return timezoneRowStatus(localHour);
}

}  // namespace desk_display
