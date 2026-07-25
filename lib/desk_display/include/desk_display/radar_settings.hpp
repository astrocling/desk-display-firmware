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

RadarSettings radarSettingsFactoryDefaults();
RadarUnselectedLabel radarUnselectedLabel(RadarDeclutterMode mode);

}  // namespace desk_display
