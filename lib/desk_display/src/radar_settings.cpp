#include "desk_display/radar_settings.hpp"

namespace desk_display {

RadarSettings radarSettingsFactoryDefaults() {
  return RadarSettings{
      RadarDeclutterMode::TargetTag,
      true,
      true,
      true,
      false,
  };
}

RadarUnselectedLabel radarUnselectedLabel(RadarDeclutterMode mode) {
  switch (mode) {
    case RadarDeclutterMode::TargetOnly:
      return RadarUnselectedLabel::None;
    case RadarDeclutterMode::TargetCallsign:
      return RadarUnselectedLabel::Callsign;
    case RadarDeclutterMode::TargetTag:
    default:
      return RadarUnselectedLabel::DenseTag;
  }
}

}  // namespace desk_display
