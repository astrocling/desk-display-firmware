#include "desk_display/screen_timezones.hpp"

#include "desk_display/format_time.hpp"
#include "desk_display/scrub.hpp"

#include <cstdio>
#include <cstring>

namespace desk_display {
namespace {

const char* kIana[kTimezoneBoardRows] = {
    "America/New_York",    "America/Chicago", "America/Los_Angeles",
    "Etc/GMT",             "Europe/Rome",     "Europe/Kyiv",
    "Europe/Chisinau",
};

const char* kLabels[kTimezoneBoardRows] = {
    "Eastern", "Chicago", "Las Vegas", "GMT", "Italy", "Ukraine", "Moldova",
};

// Fixed offsets — no TZDB. See timezoneBoardUtcOffsetHours().
const int kUtcOffsetsHours[kTimezoneBoardRows] = {-4, -5, -7, 0, 2, 3, 3};

int floorDiv(std::int64_t a, std::int64_t b) {
  if (a >= 0) {
    return static_cast<int>(a / b);
  }
  return static_cast<int>((a - b + 1) / b);
}

int positiveMod(int x, int m) {
  const int r = x % m;
  return r < 0 ? r + m : r;
}

/**
 * Parse fixture ISO-8601 instants: YYYY-MM-DDTHH:MM:SSZ or
 * YYYY-MM-DDTHH:MM:SS±HH:MM (Howard Hinnant days → unix, then offset).
 */
bool parseIsoToUnix(const char* iso, std::int64_t& outUnix) {
  if (!iso || !iso[0]) {
    return false;
  }
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  int consumed = 0;
  if (std::sscanf(iso, "%d-%d-%dT%d:%d:%d%n", &year, &month, &day, &hour,
                  &minute, &second, &consumed) != 6) {
    return false;
  }

  int y = year;
  y -= month <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy =
      (153 * static_cast<unsigned>(month + (month > 2 ? -3 : 9)) + 2) / 5 +
      static_cast<unsigned>(day) - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  const std::int64_t days =
      static_cast<std::int64_t>(era) * 146097 + static_cast<std::int64_t>(doe) -
      719468;
  outUnix = days * 86400 + hour * 3600 + minute * 60 + second;

  const char* p = iso + consumed;
  if (*p == 'Z' || *p == '\0') {
    return true;
  }
  if (*p == '+' || *p == '-') {
    const int sign = (*p == '-') ? -1 : 1;
    int oh = 0;
    int om = 0;
    if (std::sscanf(p + 1, "%d:%d", &oh, &om) >= 1) {
      outUnix -= static_cast<std::int64_t>(sign) * (oh * 3600 + om * 60);
    }
  }
  return true;
}

const TimezoneEntry* findSunEntry(const Timezones& sun, const char* iana) {
  for (std::size_t i = 0; i < sun.count; ++i) {
    if (std::strcmp(sun.entries[i].iana, iana) == 0) {
      return &sun.entries[i];
    }
  }
  return nullptr;
}

}  // namespace

const char* timezoneBoardIana(std::size_t index) {
  if (index >= kTimezoneBoardRows) {
    return "";
  }
  return kIana[index];
}

const char* timezoneBoardLabel(std::size_t index) {
  if (index >= kTimezoneBoardRows) {
    return "";
  }
  return kLabels[index];
}

int timezoneBoardUtcOffsetHours(std::size_t index) {
  if (index >= kTimezoneBoardRows) {
    return 0;
  }
  return kUtcOffsetsHours[index];
}

ScreenTimezones::ScreenTimezones() { reset(); }

void ScreenTimezones::reset() {
  liveUnix_ = 0;
  scrubSteps_ = 0;
  anchorIndex_ = 0;
  localHourFn_ = nullptr;
  localHourUser_ = nullptr;
  hasSun_ = false;
  std::memset(&sun_, 0, sizeof(sun_));
}

void ScreenTimezones::setLiveUnix(std::int64_t unixSeconds) {
  liveUnix_ = unixSeconds;
}

void ScreenTimezones::setSunTimes(const Timezones& sun) {
  sun_ = sun;
  hasSun_ = true;
}

void ScreenTimezones::clearSunTimes() {
  hasSun_ = false;
  std::memset(&sun_, 0, sizeof(sun_));
}

void ScreenTimezones::setLocalHourFn(LocalHourFn fn, void* user) {
  localHourFn_ = fn;
  localHourUser_ = user;
}

std::int64_t ScreenTimezones::scrubbedUnix() const {
  return applyScrubOffset(liveUnix_, scrubSteps_);
}

void ScreenTimezones::onRotate(int deltaSteps) { scrubSteps_ += deltaSteps; }

void ScreenTimezones::onTapRow(std::size_t rowIndex) {
  if (rowIndex < kTimezoneBoardRows) {
    anchorIndex_ = rowIndex;
  }
}

void ScreenTimezones::onDoubleTap() {
  scrubSteps_ = 0;
  anchorIndex_ = 0;
}

void ScreenTimezones::onIdleSettle() { onDoubleTap(); }

void ScreenTimezones::onLongPress() {
  scrubSteps_ = 0;
  anchorIndex_ = 0;
}

int ScreenTimezones::localHourForRow(std::size_t row,
                                     std::int64_t scrubbed) const {
  if (localHourFn_) {
    return localHourFn_(row, scrubbed, localHourUser_);
  }
  const int off = timezoneBoardUtcOffsetHours(row);
  const std::int64_t localSec = scrubbed + static_cast<std::int64_t>(off) * 3600;
  return positiveMod(floorDiv(localSec, 3600), 24);
}

bool ScreenTimezones::isDaylightForRow(std::size_t row,
                                       std::int64_t scrubbed) const {
  if (!hasSun_) {
    return true;
  }
  const TimezoneEntry* e = findSunEntry(sun_, timezoneBoardIana(row));
  if (!e) {
    return true;
  }
  std::int64_t rise = 0;
  std::int64_t set = 0;
  if (!parseIsoToUnix(e->sunrise, rise) || !parseIsoToUnix(e->sunset, set)) {
    return true;
  }
  return scrubbed >= rise && scrubbed < set;
}

void ScreenTimezones::fillRowView(TimezoneBoardRowView& out, std::size_t row,
                                  std::int64_t scrubbed) const {
  std::memset(&out, 0, sizeof(out));
  std::snprintf(out.label, sizeof(out.label), "%s", timezoneBoardLabel(row));
  std::snprintf(out.iana, sizeof(out.iana), "%s", timezoneBoardIana(row));

  const int hour = localHourForRow(row, scrubbed);
  out.localHour = hour;

  int minute = 0;
  if (!localHourFn_) {
    const int off = timezoneBoardUtcOffsetHours(row);
    const std::int64_t localSec =
        scrubbed + static_cast<std::int64_t>(off) * 3600;
    minute = positiveMod(floorDiv(localSec, 60), 60);
  } else {
    minute = positiveMod(floorDiv(scrubbed, 60), 60);
  }
  format12Hour(out.timeText, sizeof(out.timeText), hour, minute);

  if (hasSun_) {
    out.status = timezoneRowStatus(hour, isDaylightForRow(row, scrubbed));
  } else {
    out.status = timezoneRowStatus(hour);
  }
  out.isAnchor = (row == anchorIndex_);
}

TimezoneBoardView ScreenTimezones::view() const {
  TimezoneBoardView v{};
  v.anchorIndex = anchorIndex_;
  v.scrubSteps = scrubSteps_;
  v.liveUnix = liveUnix_;
  v.scrubbedUnix = scrubbedUnix();
  for (std::size_t i = 0; i < kTimezoneBoardRows; ++i) {
    fillRowView(v.rows[i], i, v.scrubbedUnix);
  }
  return v;
}

}  // namespace desk_display
