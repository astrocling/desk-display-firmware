#include "desk_display/adsb.hpp"

#include "desk_display/radar.hpp"

#include <ArduinoJson.h>
#include <cstdio>
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

    item.type[0] = '\0';
    if (!ac["t"].isNull() && ac["t"].is<const char*>()) {
      copyStr(item.type, sizeof(item.type), ac["t"].as<const char*>());
    }

    item.registration[0] = '\0';
    if (!ac["r"].isNull() && ac["r"].is<const char*>()) {
      copyStr(item.registration, sizeof(item.registration),
              ac["r"].as<const char*>());
    }

    item.squawk[0] = '\0';
    if (!ac["squawk"].isNull()) {
      if (ac["squawk"].is<const char*>()) {
        copyStr(item.squawk, sizeof(item.squawk), ac["squawk"].as<const char*>());
      } else {
        // Some feeds encode squawk as an integer (e.g. 1200).
        const long code = ac["squawk"].as<long>();
        if (code >= 0 && code <= 7777) {
          std::snprintf(item.squawk, sizeof(item.squawk), "%04ld", code);
        }
      }
    }

    item.emergency[0] = '\0';
    if (!ac["emergency"].isNull() && ac["emergency"].is<const char*>()) {
      copyStr(item.emergency, sizeof(item.emergency),
              ac["emergency"].as<const char*>());
    }

    item.dbFlags = 0;
    if (!ac["dbFlags"].isNull() && !ac["dbFlags"].is<const char*>()) {
      const unsigned flags = ac["dbFlags"].as<unsigned>();
      if (flags <= 255u) {
        item.dbFlags = static_cast<uint8_t>(flags);
      }
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

    item.hasTrack = false;
    if (!ac["track"].isNull()) {
      item.hasTrack = true;
      item.trackDeg = ac["track"].as<float>();
    } else if (!ac["calc_track"].isNull()) {
      item.hasTrack = true;
      item.trackDeg = ac["calc_track"].as<float>();
    }

    item.hasBaroRate = false;
    if (!ac["baro_rate"].isNull()) {
      item.hasBaroRate = true;
      item.baroRateFpm = ac["baro_rate"].as<float>();
    } else if (!ac["geom_rate"].isNull()) {
      item.hasBaroRate = true;
      item.baroRateFpm = ac["geom_rate"].as<float>();
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
