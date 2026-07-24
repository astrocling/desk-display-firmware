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

/**
 * At or below this range, traffic uses velocity vectors; above it, dense dots
 * (readable when the map is crowded at long range).
 */
constexpr float kRadarVectorMaxRangeMi = 15.0f;

/** Classic sweep period — matches DeskRad / firmware (10 s per revolution). */
constexpr uint32_t kRadarSweepPeriodMs = 10000;

/** Classic sweep rotation speed (degrees per second). */
constexpr float kRadarSweepDegPerSec =
    360.0f / (static_cast<float>(kRadarSweepPeriodMs) / 1000.0f);

/** Sweep illuminates a blip for this many degrees after its bearing. */
constexpr float kRadarSweepGateDeg = 5.0f;

/** Phosphor fade after a paint (ms). */
constexpr uint32_t kRadarBlipFadeMs = 9000;

enum class RadarMode : uint8_t {
  ClassicSweep = 0,
  Detail = 1,
};

/** Aircraft blip with equirectangular offset from current center (miles). */
struct RadarBlip {
  Aircraft aircraft;
  float offsetXMi;  // +east
  float offsetYMi;  // +north
  /** Ms since last sweep paint; used for Classic phosphor fade. */
  uint32_t litAgeMs;
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

  /**
   * Ingest latest ADS-B list.
   * Keeps displayed paint-on-scan positions; paint updates when the sweep
   * crosses each aircraft (avoids a full-screen jump every poll).
   */
  void bind(const AircraftList& list);

  /**
   * Advance sweep; paint any blips whose bearing enters the gate.
   * Ages phosphor fade timers. Always runs (selection does not pause sweep).
   */
  void onTick(uint32_t elapsedMs);

  /** Clear bound data; not ready until bind again. */
  void unbind();

  bool ready() const { return ready_; }

  RadarMode mode() const { return mode_; }

  /**
   * Switch modes without clearing selection.
   * Both modes keep paint-on-scan + sweep; mode is retained for API/tests.
   * Traffic symbology follows zoom (`kRadarVectorMaxRangeMi`), not mode.
   */
  void setMode(RadarMode mode);

  /** Tap empty area legacy toggle. Clears blip selection. */
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

  /** Focused idle settle: clear blip selection only. */
  void onIdleSettle();

  RadarView view() const;

 private:
  /** Immediate full refresh of displayed blips from source_ (tests / zoom). */
  void rebuildBlips();
  void applyRange(float rangeMiles);
  void setActiveCenter(double lat, double lon, bool temp);

  /** Save selected callsign before rebuild; returns false if none. */
  bool captureSelectionCallsign(char* dst, std::size_t dstLen) const;
  /** Re-find callsign in blips_; clear selection if missing. */
  void restoreSelectionByCallsign(const char* callsign);

  /** Bearing clockwise from north (degrees) for an offset, [0, 360). */
  static float bearingDegFromOffset(float offsetXMi, float offsetYMi);
  void paintBlipFromAircraft(const Aircraft& ac);
  void paintSweepAtAngle(float sweepDeg);
  void pruneDisplayedOutsideRange();
  void reprojectDisplayedOffsets();

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
