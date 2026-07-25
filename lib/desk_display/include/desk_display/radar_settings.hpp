#pragma once
#include <cstdint>

namespace desk_display {

enum class RadarDeclutterMode : uint8_t {
  TargetOnly = 0,
  TargetCallsign = 1,
  TargetTag = 2,
};

enum class RadarUnselectedLabel : uint8_t {
  None = 0,
  Callsign = 1,
  DenseTag = 2,
};

struct RadarSettings {
  RadarDeclutterMode declutter;
  bool showAirports;
  bool showAirspace;
  bool showRoads;
  bool demoMode;
};

/** Absolute hit rect for a settings overlay control (LVGL area → tap test). */
struct RadarSettingsHitRect {
  float x0;
  float y0;
  float x1;
  float y1;
};

/** Map an LVGL object area to absolute pixel bounds (exclusive max). */
inline RadarSettingsHitRect radarSettingsHitRectFromArea(int x1, int y1, int x2,
                                                           int y2) {
  return {static_cast<float>(x1), static_cast<float>(y1),
          static_cast<float>(x2 + 1), static_cast<float>(y2 + 1)};
}

inline bool radarSettingsHitContains(const RadarSettingsHitRect& r, float px,
                                     float py) {
  return px >= r.x0 && px < r.x1 && py >= r.y0 && py < r.y1;
}

RadarSettings radarSettingsFactoryDefaults();
RadarUnselectedLabel radarUnselectedLabel(RadarDeclutterMode mode);

}  // namespace desk_display
