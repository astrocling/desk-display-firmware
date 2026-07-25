#include "desk_display/radar_prefs.hpp"

#include <cstdio>
#include <cstring>

#if defined(ARDUINO)
#include <Preferences.h>
#endif

namespace desk_display {

namespace {

constexpr uint32_t kMagic = 0x52445253u;  // 'RDRS'
constexpr uint16_t kVersion = 1;

struct RadarPrefsRecord {
  uint32_t magic;
  uint16_t version;
  uint8_t declutter;
  uint8_t showAirports;
  uint8_t showAirspace;
  uint8_t showRoads;
  uint8_t demoMode;
  uint8_t reserved[3];
};

static bool isValidBoolByte(uint8_t v) { return v == 0 || v == 1; }

static void fillRecord(const RadarSettings& s, RadarPrefsRecord& rec) {
  rec.magic = kMagic;
  rec.version = kVersion;
  rec.declutter = static_cast<uint8_t>(s.declutter);
  rec.showAirports = s.showAirports ? 1 : 0;
  rec.showAirspace = s.showAirspace ? 1 : 0;
  rec.showRoads = s.showRoads ? 1 : 0;
  rec.demoMode = s.demoMode ? 1 : 0;
  rec.reserved[0] = 0;
  rec.reserved[1] = 0;
  rec.reserved[2] = 0;
}

static bool parseRecord(const RadarPrefsRecord& rec, RadarSettings& out) {
  if (rec.magic != kMagic) {
    return false;
  }
  if (rec.version != kVersion) {
    return false;
  }
  if (rec.declutter > 2) {
    return false;
  }
  if (!isValidBoolByte(rec.showAirports) || !isValidBoolByte(rec.showAirspace) ||
      !isValidBoolByte(rec.showRoads) || !isValidBoolByte(rec.demoMode)) {
    return false;
  }
  out.declutter = static_cast<RadarDeclutterMode>(rec.declutter);
  out.showAirports = rec.showAirports != 0;
  out.showAirspace = rec.showAirspace != 0;
  out.showRoads = rec.showRoads != 0;
  out.demoMode = rec.demoMode != 0;
  return true;
}

static void assignDefaults(RadarSettings& out) {
  out = radarSettingsFactoryDefaults();
}

}  // namespace

bool saveRadarSettingsToFile(const RadarSettings& s, const char* path) {
  if (path == nullptr) {
    return false;
  }
  RadarPrefsRecord rec{};
  fillRecord(s, rec);
  FILE* f = std::fopen(path, "wb");
  if (f == nullptr) {
    return false;
  }
  const std::size_t written = std::fwrite(&rec, 1, sizeof(rec), f);
  const bool ok = written == sizeof(rec);
  std::fclose(f);
  return ok;
}

bool loadRadarSettingsFromFile(RadarSettings& out, const char* path) {
  if (path == nullptr) {
    assignDefaults(out);
    return false;
  }
  FILE* f = std::fopen(path, "rb");
  if (f == nullptr) {
    assignDefaults(out);
    return false;
  }
  RadarPrefsRecord rec{};
  const std::size_t nread = std::fread(&rec, 1, sizeof(rec), f);
  std::fclose(f);
  if (nread != sizeof(rec) || !parseRecord(rec, out)) {
    assignDefaults(out);
    return false;
  }
  return true;
}

#if defined(ARDUINO)

bool saveRadarSettingsNvs(const RadarSettings& s) {
  Preferences prefs;
  if (!prefs.begin(kRadarPrefsNvsNamespace, false)) {
    return false;
  }
  const bool ok = prefs.putUChar("dcl", static_cast<uint8_t>(s.declutter)) > 0 &&
                  prefs.putBool("ap", s.showAirports) > 0 &&
                  prefs.putBool("as", s.showAirspace) > 0 &&
                  prefs.putBool("rd", s.showRoads) > 0 &&
                  prefs.putBool("dm", s.demoMode) > 0;
  prefs.end();
  return ok;
}

bool loadRadarSettingsNvs(RadarSettings& out) {
  Preferences prefs;
  if (!prefs.begin(kRadarPrefsNvsNamespace, true)) {
    assignDefaults(out);
    return false;
  }
  if (!prefs.isKey("dcl")) {
    prefs.end();
    assignDefaults(out);
    return false;
  }
  const uint8_t declutter = prefs.getUChar("dcl", 255);
  if (declutter > 2) {
    prefs.end();
    assignDefaults(out);
    return false;
  }
  out.declutter = static_cast<RadarDeclutterMode>(declutter);
  out.showAirports = prefs.getBool("ap", true);
  out.showAirspace = prefs.getBool("as", true);
  out.showRoads = prefs.getBool("rd", true);
  out.demoMode = prefs.getBool("dm", false);
  prefs.end();
  return true;
}

#endif

}  // namespace desk_display
