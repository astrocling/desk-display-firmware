#pragma once

#include <cstddef>
#include <cstdint>

#include "desk_display/weather.hpp"
#include "desk_display/weather_icons.hpp"

namespace desk_display {

/** Scrub index for live "now" (current conditions, not an hourly slot). */
constexpr int kWeatherScrubNow = -1;

/** Visible hourly strip slot count on the dial. */
constexpr std::size_t kWeatherStripSlots = 5;

/** One slot in the centered hourly strip window. */
struct WeatherStripSlot {
  bool valid;
  bool selected;
  float temp;
  WeatherIconId icon;
  char hourDigit[8];
};

/** Snapshot for LVGL (or tests) to render the weather screen. */
struct WeatherScreenView {
  bool ready;
  bool showingNow;
  int scrubIndex;  // kWeatherScrubNow or [0, hourlyCount)

  float displayTemp;
  float feelsLike;
  int weatherCode;
  WeatherIconId icon;

  /** "Current" or short hour like "6 PM". */
  char whenLabel[16];

  float todayHigh;
  float todayLow;

  const WeatherHourly* hourly;
  std::size_t hourlyCount;

  WeatherStripSlot strip[kWeatherStripSlots];
  std::size_t stripCount;

  bool alertBadge;
  bool alertDetailOpen;
  const char* alertSeverity;
  const char* alertHeadline;
};

/**
 * Weather screen view-model (no LVGL).
 * Bind a parsed Weather model; rotate scrubs the hourly strip; snap returns to now.
 */
class WeatherScreen {
 public:
  WeatherScreen();

  /** Clear bound data → empty / not-ready. */
  void clear();

  /** Copy weather model; resets scrub to now and closes alert detail. */
  void bind(const Weather& weather);

  bool ready() const { return ready_; }
  bool notReady() const { return !ready_; }

  /** Encoder scrub through hourly forecast. Positive = later hours. */
  void onRotate(int delta);

  /** Idle / explicit reset: center display back to live current conditions. */
  void snapToNow();

  /**
   * Open alert detail (severity + headline). Returns false when no alert
   * or not ready.
   */
  bool openAlertDetail();
  void closeAlertDetail();
  bool alertDetailOpen() const { return alertDetailOpen_; }

  int scrubIndex() const { return scrubIndex_; }

  WeatherScreenView view() const;

 private:
  bool ready_;
  Weather weather_;
  int scrubIndex_;
  bool alertDetailOpen_;
};

}  // namespace desk_display
