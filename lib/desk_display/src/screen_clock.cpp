#include "desk_display/screen_clock.hpp"

#include <cstdio>
#include <ctime>

namespace desk_display {
namespace {

const char* kDow[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char* kMon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

/** Sakamoto: 0 = Sunday … 6 = Saturday. */
int weekdaySun0(int year, int month, int day) {
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (month < 3) {
    --year;
  }
  return (year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7;
}

}  // namespace

ScreenClock::ScreenClock() { reset(); }

void ScreenClock::reset() {
  year_ = 1970;
  month_ = 1;
  day_ = 1;
  hour_ = 0;
  minute_ = 0;
  second_ = 0;
  timezoneBoardHint_ = true;
  refreshDateText();
}

void ScreenClock::setTime(int year, int month, int day, int hour, int minute,
                          int second) {
  year_ = year;
  month_ = month;
  day_ = day;
  hour_ = hour;
  minute_ = minute;
  second_ = second;
  refreshDateText();
}

void ScreenClock::setUnixUtc(std::int64_t unixSeconds) {
  const std::time_t t = static_cast<std::time_t>(unixSeconds);
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif
  year_ = tm.tm_year + 1900;
  month_ = tm.tm_mon + 1;
  day_ = tm.tm_mday;
  hour_ = tm.tm_hour;
  minute_ = tm.tm_min;
  second_ = tm.tm_sec;
  refreshDateText();
}

void ScreenClock::setTimezoneBoardHint(bool visible) {
  timezoneBoardHint_ = visible;
}

ClockView ScreenClock::view() const {
  ClockView v{};
  v.year = year_;
  v.month = month_;
  v.day = day_;
  v.hour = hour_;
  v.minute = minute_;
  v.second = second_;
  std::snprintf(v.dateText, sizeof(v.dateText), "%s", dateText_);
  v.timezoneBoardHint = timezoneBoardHint_;
  return v;
}

float ScreenClock::hourHandAngleDeg() const {
  const int h12 = hour_ % 12;
  return 30.0f * static_cast<float>(h12) + 0.5f * static_cast<float>(minute_) +
         (0.5f / 60.0f) * static_cast<float>(second_);
}

float ScreenClock::minuteHandAngleDeg() const {
  return 6.0f * static_cast<float>(minute_) +
         0.1f * static_cast<float>(second_);
}

float ScreenClock::secondHandAngleDeg() const {
  return 6.0f * static_cast<float>(second_);
}

void ScreenClock::refreshDateText() {
  const int monIdx = (month_ >= 1 && month_ <= 12) ? (month_ - 1) : 0;
  const int dowIdx = weekdaySun0(year_, month_, day_);
  std::snprintf(dateText_, sizeof(dateText_), "%s, %s %d", kDow[dowIdx],
                kMon[monIdx], day_);
}

}  // namespace desk_display
