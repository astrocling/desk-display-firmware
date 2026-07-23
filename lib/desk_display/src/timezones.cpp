#include "desk_display/timezones.hpp"

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

bool parseTimezones(const char* json, Timezones& out) {
  if (!json) {
    return false;
  }

  std::memset(&out, 0, sizeof(out));

  JsonDocument doc;
  if (deserializeJson(doc, json)) {
    return false;
  }

  if (!doc.is<JsonObjectConst>()) {
    return false;
  }

  for (JsonPairConst kv : doc.as<JsonObjectConst>()) {
    if (out.count >= kMaxTimezoneEntries) {
      break;
    }
    if (!kv.value().is<JsonObjectConst>()) {
      continue;
    }
    JsonObjectConst city = kv.value().as<JsonObjectConst>();
    if (city["sunrise"].isNull() || city["sunset"].isNull()) {
      continue;
    }

    TimezoneEntry& entry = out.entries[out.count++];
    copyStr(entry.iana, sizeof(entry.iana), kv.key().c_str());
    copyStr(entry.sunrise, sizeof(entry.sunrise),
            city["sunrise"].as<const char*>());
    copyStr(entry.sunset, sizeof(entry.sunset),
            city["sunset"].as<const char*>());
  }

  return out.count > 0;
}

}  // namespace desk_display
