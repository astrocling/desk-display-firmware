#include "desk_display/not_ready.hpp"

#include <ArduinoJson.h>

namespace desk_display {

bool isNotReadyError(const char* json) {
  if (!json) {
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, json)) {
    return false;
  }

  if (!doc["error"].is<const char*>()) {
    return false;
  }

  const char* msg = doc["error"].as<const char*>();
  return msg != nullptr && msg[0] != '\0';
}

}  // namespace desk_display
