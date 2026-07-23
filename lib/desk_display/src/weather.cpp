#include "desk_display/weather.hpp"

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

}  // namespace

bool parseWeather(const char* json, Weather& out) {
  if (!json) {
    return false;
  }

  std::memset(&out, 0, sizeof(out));

  JsonDocument doc;
  if (deserializeJson(doc, json)) {
    return false;
  }

  if (!doc["current"].is<JsonObjectConst>()) {
    return false;
  }
  if (doc["current"]["temp"].isNull() || doc["current"]["feelsLike"].isNull() ||
      doc["current"]["code"].isNull()) {
    return false;
  }
  if (doc["todayHigh"].isNull() || doc["todayLow"].isNull()) {
    return false;
  }

  out.currentTemp = doc["current"]["temp"].as<float>();
  out.currentFeelsLike = doc["current"]["feelsLike"].as<float>();
  out.currentCode = doc["current"]["code"].as<int>();
  out.todayHigh = doc["todayHigh"].as<float>();
  out.todayLow = doc["todayLow"].as<float>();

  out.hourlyCount = 0;
  if (doc["hourly"].is<JsonArrayConst>()) {
    for (JsonObjectConst h : doc["hourly"].as<JsonArrayConst>()) {
      if (out.hourlyCount >= kMaxHourly) {
        break;
      }
      if (h["time"].isNull() || h["temp"].isNull() || h["code"].isNull()) {
        continue;
      }
      WeatherHourly& slot = out.hourly[out.hourlyCount++];
      copyStr(slot.time, sizeof(slot.time), h["time"].as<const char*>());
      slot.temp = h["temp"].as<float>();
      slot.code = h["code"].as<int>();
    }
  }

  out.alert.present = false;
  if (!doc["alert"].isNull() && doc["alert"].is<JsonObjectConst>()) {
    JsonObjectConst alert = doc["alert"].as<JsonObjectConst>();
    out.alert.present = true;
    copyStr(out.alert.severity, sizeof(out.alert.severity),
            alert["severity"].as<const char*>());
    copyStr(out.alert.headline, sizeof(out.alert.headline),
            alert["headline"].as<const char*>());
  }

  out.hasUpdatedAt = false;
  if (!doc["updatedAt"].isNull() && doc["updatedAt"].is<const char*>()) {
    out.hasUpdatedAt = true;
    copyStr(out.updatedAt, sizeof(out.updatedAt),
            doc["updatedAt"].as<const char*>());
  }

  return true;
}

}  // namespace desk_display
