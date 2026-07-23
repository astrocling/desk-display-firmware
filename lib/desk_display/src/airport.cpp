#include "desk_display/airport.hpp"

#include <ArduinoJson.h>
#include <cstring>

namespace desk_display {

bool parseAirport(const char* json, Airport& out) {
  if (!json) {
    return false;
  }

  std::memset(&out, 0, sizeof(out));

  JsonDocument doc;
  if (deserializeJson(doc, json)) {
    return false;
  }

  if (doc["lat"].isNull() || doc["lon"].isNull()) {
    return false;
  }

  out.lat = doc["lat"].as<double>();
  out.lon = doc["lon"].as<double>();
  return true;
}

}  // namespace desk_display
