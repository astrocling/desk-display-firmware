#pragma once

#include "desk_display/adsb.hpp"
#include "desk_display/airport.hpp"
#include "desk_display/radar.hpp"

#include <cstddef>
#include <cstdint>

namespace desk_display {

/** Default home center (Dayton, OH area — matches config.h.example). */
constexpr double kRadarHomeLat = 40.03353;
constexpr double kRadarHomeLon = -84.19588;

/** Default zoom on bind/boot (miles). */
constexpr float kRadarDefaultRangeMi = 25.0f;

/** Encoder zoom step (miles). */
constexpr float kRadarRangeStepMi = 5.0f;

/** Classic sweep rotation speed (degrees per second). */
constexpr float kRadarSweepDegPerSec = 150.0f;

enum class RadarMode : uint8_t {
  ClassicSweep = 0,
  Detail = 1,
};

/** Aircraft blip with equirectangular offset from current center (miles). */
struct RadarBlip {
  Aircraft aircraft;
  float offsetXMi;  // +east
  float offsetYMi;  // +north
};

/** Selected aircraft detail card fields. */
struct RadarDetailCard {
  bool present;
  char callsign[kMaxCallsign];
  float altFt;
  float speedKt;
  bool hasAlt;
  bool hasSpeed;
  char tagLine2[24];
  char altLabel[8];
  char speedLabel[8];
};

/** Snapshot for LVGL (or tests) to render the radar screen. */
struct RadarView {
  bool ready;
  RadarMode mode;
  float rangeMiles;
  double centerLat;
  double centerLon;
  bool isTempCenter;
  bool isPinned;
  bool isHomeCenter;

  const RadarBlip* blips;
  std::size_t blipCount;

  bool hasSelection;
  std::size_t selectedIndex;
  RadarDetailCard detail;
  float sweepAngleDeg;
};

/**
 * Radar / ADS-B screen view-model (no LVGL).
 * Bind AircraftList; filter by range; zoom; sweep/detail toggle; ICAO recenter.
 * Center-tap (back to carousel) is owned by Nav — call revertTempCenter() then.
 */
class ScreenRadar {
 public:
  ScreenRadar();

  void reset();

  /** Copy aircraft list and rebuild blips for current center/range. */
  void bind(const AircraftList& list);

  /** Advance classic sweep angle (degrees, wraps [0, 360)). */
  void onTick(uint32_t elapsedMs);

  /** Clear bound data; not ready until bind again. */
  void unbind();

  bool ready() const { return ready_; }

  RadarMode mode() const { return mode_; }

  /** Tap empty area: ClassicSweep ↔ Detail. Clears blip selection. */
  void toggleMode();

  float rangeMiles() const { return rangeMiles_; }

  /**
   * Encoder rotate while focused: zoom range in 5 mi steps, clamped 5–50.
   * Positive delta = zoom out (larger range); negative = zoom in.
   */
  void onRotate(int delta);

  std::size_t blipCount() const { return blips_.count; }
  const RadarBlip& blip(std::size_t index) const;

  bool hasSelection() const { return hasSelection_; }
  std::size_t selectedIndex() const { return selectedIndex_; }

  /** Tap a blip by index → detail card. Returns false if index invalid. */
  bool selectBlip(std::size_t index);
  void clearSelection();

  RadarDetailCard detailCard() const;

  double centerLat() const { return centerLat_; }
  double centerLon() const { return centerLon_; }
  bool isTempCenter() const { return isTempCenter_; }
  bool isPinned() const { return isPinned_; }
  bool isHomeCenter() const;

  /**
   * ICAO lookup result: temporary recenter to airport lat/lon.
   * Does not change pin / permanent center until pinCenter().
   */
  void setTempCenter(double lat, double lon);
  void setTempCenter(const Airport& airport);

  /** Make the current center the permanent default until cleared. */
  void pinCenter();

  /** Drop pin; permanent center returns to home. Active center unchanged. */
  void clearPin();

  /**
   * Carousel-exit simulation: if temp, revert active center to permanent
   * (pinned or home). No-op when not temp.
   */
  void revertTempCenter();

  RadarView view() const;

 private:
  void rebuildBlips();
  void applyRange(float rangeMiles);
  void setActiveCenter(double lat, double lon, bool temp);

  /** Save selected callsign before rebuild; returns false if none. */
  bool captureSelectionCallsign(char* dst, std::size_t dstLen) const;
  /** Re-find callsign in blips_; clear selection if missing. */
  void restoreSelectionByCallsign(const char* callsign);

  bool ready_;
  RadarMode mode_;
  float rangeMiles_;

  double homeLat_;
  double homeLon_;
  double permanentLat_;
  double permanentLon_;
  double centerLat_;
  double centerLon_;
  bool isTempCenter_;
  bool isPinned_;

  AircraftList source_;
  struct BlipList {
    RadarBlip items[kMaxAircraft];
    std::size_t count;
  };
  BlipList blips_;

  bool hasSelection_;
  std::size_t selectedIndex_;
  float sweepAngleDeg_;
};

}  // namespace desk_display
