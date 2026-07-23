#include "desk_display/format_time.hpp"

#include <cstdio>

namespace desk_display {
namespace {

int hour12From24(int hour24) {
  const int h = hour24 % 24;
  if (h == 0) {
    return 12;
  }
  if (h > 12) {
    return h - 12;
  }
  return h;
}

const char* amPmFrom24(int hour24) {
  return (hour24 % 24) < 12 ? "AM" : "PM";
}

}  // namespace

std::size_t format12Hour(char* buf, std::size_t bufLen, int hour24, int minute) {
  if (!buf || bufLen == 0) {
    return 0;
  }
  const int n = std::snprintf(buf, bufLen, "%d:%02d %s", hour12From24(hour24),
                              minute, amPmFrom24(hour24));
  if (n < 0 || static_cast<std::size_t>(n) >= bufLen) {
    return 0;
  }
  return static_cast<std::size_t>(n);
}

std::size_t format12HourWithSeconds(char* buf, std::size_t bufLen, int hour24,
                                    int minute, int second) {
  if (!buf || bufLen == 0) {
    return 0;
  }
  const int n =
      std::snprintf(buf, bufLen, "%d:%02d:%02d %s", hour12From24(hour24), minute,
                    second, amPmFrom24(hour24));
  if (n < 0 || static_cast<std::size_t>(n) >= bufLen) {
    return 0;
  }
  return static_cast<std::size_t>(n);
}

}  // namespace desk_display
