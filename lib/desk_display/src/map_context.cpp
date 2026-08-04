#include "desk_display/map_context.hpp"

#include <ArduinoJson.h>
#include <cstring>

namespace desk_display {
namespace {

constexpr std::size_t kMaxAirports = 40;
constexpr std::size_t kMaxRings = 24;
constexpr std::size_t kMaxRingPoints = 64;
constexpr std::size_t kMaxHighways = 12;
constexpr std::size_t kMaxHighwayPoints = 80;

void copyStr(char* dest, std::size_t destLen, const char* src) {
  if (destLen == 0) {
    return;
  }
  if (!src) {
    dest[0] = '\0';
    return;
  }
  std::strncpy(dest, src, destLen - 1);
  dest[destLen - 1] = '\0';
}

bool parseAirspaceClass(const char* s, AirspaceClass& out) {
  if (!s) {
    return false;
  }
  if (s[0] == 'B' && s[1] == '\0') {
    out = AirspaceClass::B;
    return true;
  }
  if (s[0] == 'C' && s[1] == '\0') {
    out = AirspaceClass::C;
    return true;
  }
  if (s[0] == 'D' && s[1] == '\0') {
    out = AirspaceClass::D;
    return true;
  }
  return false;
}

bool parseLatLonPoints(JsonArrayConst points, float* outLat, float* outLon,
                       uint8_t maxPoints, uint8_t minPoints, uint8_t& outCount) {
  outCount = 0;
  if (points.size() < minPoints) {
    return false;
  }

  uint8_t count = 0;
  for (JsonVariantConst pt : points) {
    if (count >= maxPoints) {
      break;
    }
    if (!pt.is<JsonArrayConst>()) {
      return false;
    }
    JsonArrayConst coords = pt.as<JsonArrayConst>();
    if (coords.size() < 2) {
      return false;
    }
    if (coords[0].isNull() || coords[1].isNull()) {
      return false;
    }
    outLat[count] = coords[0].as<float>();
    outLon[count] = coords[1].as<float>();
    ++count;
  }

  if (count < minPoints) {
    return false;
  }
  outCount = count;
  return true;
}

bool parseRing(JsonObjectConst ringObj, MapAirspaceRing& out) {
  std::memset(&out, 0, sizeof(out));

  if (ringObj["class"].isNull() || !ringObj["class"].is<const char*>()) {
    return false;
  }
  if (!parseAirspaceClass(ringObj["class"].as<const char*>(), out.cls)) {
    return false;
  }

  if (ringObj["id"].isNull() || !ringObj["id"].is<const char*>()) {
    return false;
  }
  copyStr(out.id, sizeof(out.id), ringObj["id"].as<const char*>());

  if (!ringObj["points"].is<JsonArrayConst>()) {
    return false;
  }

  return parseLatLonPoints(ringObj["points"].as<JsonArrayConst>(), out.pointsLat,
                           out.pointsLon, kMaxRingPoints, 3, out.pointCount);
}

bool parseHighway(JsonObjectConst hwObj, MapHighway& out) {
  std::memset(&out, 0, sizeof(out));

  if (hwObj["id"].isNull() || !hwObj["id"].is<const char*>()) {
    return false;
  }
  copyStr(out.id, sizeof(out.id), hwObj["id"].as<const char*>());

  if (!hwObj["route"].isNull() && hwObj["route"].is<const char*>()) {
    copyStr(out.route, sizeof(out.route), hwObj["route"].as<const char*>());
  }

  if (!hwObj["points"].is<JsonArrayConst>()) {
    return false;
  }

  return parseLatLonPoints(hwObj["points"].as<JsonArrayConst>(), out.pointsLat,
                           out.pointsLon, kMaxHighwayPoints, 2, out.pointCount);
}

}  // namespace

bool parseMapContext(const char* json, MapContext& out) {
  if (!json) {
    return false;
  }

  // Deserialize before touching `out` so a failed parse (OOM / truncated
  // body) does not wipe a previously good Dial bind (sticky stub hexagon).
  JsonDocument doc;
  if (deserializeJson(doc, json)) {
    return false;
  }

  std::memset(&out, 0, sizeof(out));

  if (doc["airports"].is<JsonArrayConst>()) {
    for (JsonObjectConst ap : doc["airports"].as<JsonArrayConst>()) {
      if (out.airportCount >= kMaxAirports) {
        break;
      }

      if (ap["icao"].isNull() || !ap["icao"].is<const char*>()) {
        continue;
      }
      if (ap["lat"].isNull() || ap["lon"].isNull()) {
        continue;
      }

      // Live payloads tag non-towered; omit them so the 40-slot cap keeps
      // towered ICAOs (KDAY/KFFO). Missing field = include (dayton fixture).
      const bool towered =
          ap["towered"].isNull() ? true : ap["towered"].as<bool>();
      if (!towered) {
        continue;
      }

      MapAirport& item = out.airports[out.airportCount];
      std::memset(&item, 0, sizeof(item));
      copyStr(item.icao, sizeof(item.icao), ap["icao"].as<const char*>());
      if (!ap["name"].isNull() && ap["name"].is<const char*>()) {
        copyStr(item.name, sizeof(item.name), ap["name"].as<const char*>());
      }
      item.lat = ap["lat"].as<double>();
      item.lon = ap["lon"].as<double>();
      item.towered = true;
      ++out.airportCount;
    }
  }

  if (doc["rings"].is<JsonArrayConst>()) {
    for (JsonObjectConst ringObj : doc["rings"].as<JsonArrayConst>()) {
      if (out.ringCount >= kMaxRings) {
        break;
      }

      MapAirspaceRing ring{};
      if (!parseRing(ringObj, ring)) {
        continue;
      }
      out.rings[out.ringCount] = ring;
      ++out.ringCount;
    }
  }

  if (doc["highways"].is<JsonArrayConst>()) {
    for (JsonObjectConst hwObj : doc["highways"].as<JsonArrayConst>()) {
      if (out.highwayCount >= kMaxHighways) {
        break;
      }

      MapHighway highway{};
      if (!parseHighway(hwObj, highway)) {
        continue;
      }
      out.highways[out.highwayCount] = highway;
      ++out.highwayCount;
    }
  }

  return true;
}

}  // namespace desk_display
