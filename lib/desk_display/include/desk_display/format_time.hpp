#pragma once

#include <cstddef>

namespace desk_display {

/**
 * Format 24h civil time as 12-hour wall clock text.
 * Examples: 0:00 → "12:00 AM", 13:05 → "1:05 PM", 12:00 → "12:00 PM".
 * Returns bytes written (excluding NUL), or 0 on error / short buffer.
 */
std::size_t format12Hour(char* buf, std::size_t bufLen, int hour24, int minute);

/** Same as format12Hour but includes seconds: "1:05:09 PM". */
std::size_t format12HourWithSeconds(char* buf, std::size_t bufLen, int hour24,
                                    int minute, int second);

}  // namespace desk_display
