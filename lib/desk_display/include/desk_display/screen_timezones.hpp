#pragma once

#include "desk_display/timezone_status.hpp"
#include "desk_display/timezones.hpp"

#include <cstddef>
#include <cstdint>

namespace desk_display {

constexpr std::size_t kTimezoneBoardRows = 7;
constexpr std::size_t kTimezoneLabelLen = 24;
constexpr std::size_t kTimezoneTimeTextLen = 12;  // "12:00 PM"

/**
 * Fixed board order (matches firmware config / TIMEZONE_IANA).
 * Index 0 = Eastern (home / default anchor).
 */
const char* timezoneBoardIana(std::size_t index);
const char* timezoneBoardLabel(std::size_t index);

/**
 * Fixed UTC hour offsets for the 7 board zones (no TZDB on native).
 * Summer/standard approximation: EDT -4, CDT -5, PDT -7, GMT 0,
 * Rome/CEST +2, Kyiv/Chisinau EEST +3.
 * Device firmware may replace via setLocalHourFn with a real converter.
 */
int timezoneBoardUtcOffsetHours(std::size_t index);

/** Optional override: compute local hour 0–23 for a row at scrubbed unix. */
using LocalHourFn = int (*)(std::size_t rowIndex, std::int64_t scrubbedUnix,
                            void* user);

struct TimezoneBoardRowView {
  char label[kTimezoneLabelLen];
  char iana[kMaxIanaLen];
  char timeText[kTimezoneTimeTextLen];  // "h:mm AM/PM"
  int localHour;                        // 0–23
  TzRowStatus status;
  bool isAnchor;
};

struct TimezoneBoardView {
  TimezoneBoardRowView rows[kTimezoneBoardRows];
  std::size_t anchorIndex;
  int scrubSteps;
  std::int64_t liveUnix;
  std::int64_t scrubbedUnix;
};

/**
 * Timezone board view-model: scrub, anchor, status icons.
 * Center-tap (back) is owned by Nav — not handled here.
 */
class ScreenTimezones {
 public:
  ScreenTimezones();

  void reset();

  /** Live "now" unix (UTC). Scrub is relative to this. */
  void setLiveUnix(std::int64_t unixSeconds);

  /**
   * Optional sunrise/sunset map from parseTimezones().
   * When present and an entry matches a row IANA, status uses
   * timezoneRowStatus(hour, isDaylight); otherwise hour-only windows.
   */
  void setSunTimes(const Timezones& sun);

  void clearSunTimes();

  /**
   * Inject local-hour conversion (preferred for tests without TZDB).
   * Pass nullptr to use the built-in fixed UTC offset table.
   */
  void setLocalHourFn(LocalHourFn fn, void* user = nullptr);

  int scrubSteps() const { return scrubSteps_; }
  std::size_t anchorIndex() const { return anchorIndex_; }
  std::int64_t liveUnix() const { return liveUnix_; }
  std::int64_t scrubbedUnix() const;

  /** Encoder rotate while focused: ±1 tick = ±1 hour. */
  void onRotate(int deltaSteps);

  /** Tap a row to make it the scrub anchor (0 .. kTimezoneBoardRows-1). */
  void onTapRow(std::size_t rowIndex);

  /** Double-tap / long-press: scrub → 0, anchor → Eastern (0). */
  void onDoubleTap();
  void onLongPress();

  /** Focused idle settle: same reset as double-tap. */
  void onIdleSettle();

  TimezoneBoardView view() const;

 private:
  int localHourForRow(std::size_t row, std::int64_t scrubbed) const;
  bool isDaylightForRow(std::size_t row, std::int64_t scrubbed) const;
  void fillRowView(TimezoneBoardRowView& out, std::size_t row,
                   std::int64_t scrubbed) const;

  std::int64_t liveUnix_;
  int scrubSteps_;
  std::size_t anchorIndex_;
  LocalHourFn localHourFn_;
  void* localHourUser_;
  bool hasSun_;
  Timezones sun_;
};

}  // namespace desk_display
