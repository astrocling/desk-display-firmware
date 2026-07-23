#pragma once

#include <cstddef>

namespace desk_display {

constexpr std::size_t kMaxTimezoneEntries = 16;
constexpr std::size_t kMaxIanaLen = 48;
constexpr std::size_t kMaxSunIsoLen = 40;

struct TimezoneEntry {
  char iana[kMaxIanaLen];
  char sunrise[kMaxSunIsoLen];
  char sunset[kMaxSunIsoLen];
};

struct Timezones {
  TimezoneEntry entries[kMaxTimezoneEntries];
  std::size_t count;
};

/** Parse flat IANA → {sunrise, sunset} map. */
bool parseTimezones(const char* json, Timezones& out);

}  // namespace desk_display
