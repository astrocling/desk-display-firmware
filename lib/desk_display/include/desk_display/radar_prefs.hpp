#pragma once

#include "desk_display/radar_settings.hpp"

namespace desk_display {

constexpr const char* kRadarPrefsNvsNamespace = "radar";

bool saveRadarSettingsToFile(const RadarSettings& s, const char* path);
bool loadRadarSettingsFromFile(RadarSettings& out, const char* path);

#if defined(ARDUINO) && defined(ESP32)
bool saveRadarSettingsNvs(const RadarSettings& s);
bool loadRadarSettingsNvs(RadarSettings& out);
#endif

}  // namespace desk_display
