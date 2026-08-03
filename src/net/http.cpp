#include "net/http.hpp"

#include "net/wifi.hpp"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <cstring>
#include <memory>

namespace desk_net {

bool httpGet(const char* url, char* body, std::size_t bodyCap, std::size_t& bodyLen,
             void* /*user*/) {
  bodyLen = 0;
  if (!url || !body || bodyCap == 0) {
    return false;
  }
  if (!wifiIsConnected()) {
    return false;
  }

  // Heap-allocate: WiFiClientSecure + TLS must not live on the 8KB Arduino loop stack.
  auto client = std::make_unique<WiFiClientSecure>();
  client->setInsecure();  // Vercel / public API; no onboard CA bundle this pass

  HTTPClient http;
  http.setTimeout(8000);
  http.setReuse(false);
  if (!http.begin(*client, url)) {
    Serial.printf("http: begin failed %s\n", url);
    return false;
  }

  Serial.printf("http: GET %s …\n", url);
  const int status = http.GET();
  if (status < 200 || status >= 300) {
    Serial.printf("http: GET %s -> %d\n", url, status);
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();

  if (payload.length() == 0 || payload.length() + 1 > bodyCap) {
    Serial.printf("http: response for %s empty or exceeded buffer (%u)\n", url,
                  static_cast<unsigned>(payload.length()));
    return false;
  }

  std::memcpy(body, payload.c_str(), payload.length());
  body[payload.length()] = '\0';
  bodyLen = payload.length();
  Serial.printf("http: GET %s ok (%u bytes)\n", url, static_cast<unsigned>(bodyLen));
  return true;
}

}  // namespace desk_net
