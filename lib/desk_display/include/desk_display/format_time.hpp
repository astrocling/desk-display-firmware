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

/**
 * Short 12h label without minutes: 0 → "12 AM", 18 → "6 PM".
 * Returns bytes written (excluding NUL), or 0 on error / short buffer.
 */
std::size_t format12HourShort(char* buf, std::size_t bufLen, int hour24);

/**
 * Extract hour-of-day (0–23) from hourly ISO-ish strings like
 * "2026-07-23T18:00", "2026-07-23T18:00:00Z". Returns false if unparseable.
 */
bool parseHourlyIsoHour(const char* iso, int& hour24);

}  // namespace desk_display
