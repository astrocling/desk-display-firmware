#pragma once

#include <cstddef>
#include <cstdint>

namespace desk_display {

constexpr std::size_t kClockDateLen = 32;

/** Snapshot the UI layer will render (no LVGL). */
struct ClockView {
  int year;
  int month;   // 1–12
  int day;     // 1–31
  int hour;    // 0–23
  int minute;  // 0–59
  int second;  // 0–59
  char dateText[kClockDateLen];  // e.g. "Thu, Jul 23"
  bool timezoneBoardHint;        // timezone board is available from home
};

/**
 * Clock screen view-model: analog face inputs + date string.
 * No LVGL / hardware — pure state for tests and later UI binding.
 */
class ScreenClock {
 public:
  ScreenClock();

  void reset();

  /** Set civil time (preferred for deterministic tests). Month 1–12. */
  void setTime(int year, int month, int day, int hour, int minute, int second);

  /** Set from UTC unix seconds (deterministic via gmtime). */
  void setUnixUtc(std::int64_t unixSeconds);

  /** Whether to show the small hint that the timezone board exists. */
  void setTimezoneBoardHint(bool visible);

  ClockView view() const;

  int hour() const { return hour_; }
  int minute() const { return minute_; }
  int second() const { return second_; }
  const char* dateText() const { return dateText_; }
  bool timezoneBoardHint() const { return timezoneBoardHint_; }

  /** Hand angles in degrees (0 = 12 o'clock, clockwise). */
  float hourHandAngleDeg() const;
  float minuteHandAngleDeg() const;
  float secondHandAngleDeg() const;

 private:
  void refreshDateText();

  int year_;
  int month_;
  int day_;
  int hour_;
  int minute_;
  int second_;
  char dateText_[kClockDateLen];
  bool timezoneBoardHint_;
};

}  // namespace desk_display
