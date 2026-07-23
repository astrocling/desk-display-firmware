#pragma once

namespace desk_display {

enum class TzRowStatus {
  Working,  // green — 09:00–17:00 local
  Awake,    // amber — 07:00–09:00 and 17:00–21:00 local
  Night     // moon/dim — 21:00–07:00 local
};

/**
 * 3-state status from local hour (0–23).
 * Working 9–17; awake 7–9 and 17–21; night otherwise (21–7).
 */
TzRowStatus timezoneRowStatus(int localHour);

/**
 * Same windows, but if `isDaylight` is false (outside sunrise–sunset),
 * status is forced to Night.
 */
TzRowStatus timezoneRowStatus(int localHour, bool isDaylight);

}  // namespace desk_display
