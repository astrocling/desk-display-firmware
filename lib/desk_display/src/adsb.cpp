#include "desk_display/adsb.hpp"

#include "desk_display/radar.hpp"

#include <ArduinoJson.h>
#include <cstring>

namespace desk_display {
namespace {

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

void trimTrailingSpaces(char* s) {
  if (!s) {
    return;
  }
  std::size_t n = std::strlen(s);
  while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) {
    s[--n] = '\0';
  }
}

}  // namespace

bool parseAdsb(const char* json, AircraftList& out) {
  if (!json) {
    return false;
  }

  std::memset(&out, 0, sizeof(out));

  JsonDocument doc;
  if (deserializeJson(doc, json)) {
    return false;
  }

  if (!doc["ac"].is<JsonArrayConst>()) {
    return false;
  }

  for (JsonObjectConst ac : doc["ac"].as<JsonArrayConst>()) {
    if (out.count >= kMaxAircraft) {
      break;
    }

    Aircraft& item = out.items[out.count];
    std::memset(&item, 0, sizeof(item));

    if (!ac["flight"].isNull() && ac["flight"].is<const char*>()) {
      copyStr(item.callsign, sizeof(item.callsign),
              ac["flight"].as<const char*>());
      trimTrailingSpaces(item.callsign);
    } else if (!ac["r"].isNull() && ac["r"].is<const char*>()) {
      copyStr(item.callsign, sizeof(item.callsign), ac["r"].as<const char*>());
    } else if (!ac["hex"].isNull() && ac["hex"].is<const char*>()) {
      copyStr(item.callsign, sizeof(item.callsign), ac["hex"].as<const char*>());
    }

    item.hasAlt = false;
    if (!ac["alt_baro"].isNull() && !ac["alt_baro"].is<const char*>()) {
      item.hasAlt = true;
      item.altFt = ac["alt_baro"].as<float>();
    } else if (!ac["alt_geom"].isNull() && !ac["alt_geom"].is<const char*>()) {
      item.hasAlt = true;
      item.altFt = ac["alt_geom"].as<float>();
    }

    item.hasSpeed = false;
    if (!ac["gs"].isNull()) {
      item.hasSpeed = true;
      item.speedKt = ac["gs"].as<float>();
    }

    item.hasPosition = false;
    if (!ac["lat"].isNull() && !ac["lon"].isNull()) {
      item.hasPosition = true;
      item.lat = ac["lat"].as<double>();
      item.lon = ac["lon"].as<double>();
    }

    ++out.count;
  }

  return true;
}

std::size_t filterAircraftByRange(const AircraftList& in, double centerLat,
                                  double centerLon, float rangeMiles,
                                  AircraftList& out) {
  std::memset(&out, 0, sizeof(out));
  const float range = clampRadarRangeMiles(rangeMiles);

  for (std::size_t i = 0; i < in.count; ++i) {
    const Aircraft& ac = in.items[i];
    if (!ac.hasPosition) {
      continue;
    }
    if (out.count >= kMaxAircraft) {
      break;
    }
    const float d =
        distanceMiles(centerLat, centerLon, ac.lat, ac.lon);
    if (d <= range) {
      out.items[out.count++] = ac;
    }
  }

  return out.count;
}

}  // namespace desk_display
