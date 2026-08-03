#include "net/wifi.hpp"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <cstring>

#include "config.h"
#include "desk_display/wifi_policy.hpp"

namespace desk_net {
namespace {

constexpr size_t kSsidMax = 32;
constexpr size_t kPassMax = 63;

char gSsid[kSsidMax + 1];
char gPass[kPassMax + 1];
bool gConfigured = false;
bool gWasConnected = false;
uint32_t gRetryDelayMs = 0;
uint32_t gNextRetryAtMs = 0;

void copyCred(const char* ssid, const char* pass) {
  strncpy(gSsid, ssid ? ssid : "", kSsidMax);
  gSsid[kSsidMax] = '\0';
  strncpy(gPass, pass ? pass : "", kPassMax);
  gPass[kPassMax] = '\0';
}

bool loadNvs() {
  Preferences prefs;
  if (!prefs.begin(desk_display::kWifiNvsNamespace, true)) {
    return false;
  }
  if (!prefs.isKey(desk_display::kWifiNvsKeySsid)) {
    prefs.end();
    return false;
  }
  String ssid = prefs.getString(desk_display::kWifiNvsKeySsid, "");
  String pass = prefs.getString(desk_display::kWifiNvsKeyPass, "");
  prefs.end();
  if (ssid.length() == 0) {
    return false;
  }
  copyCred(ssid.c_str(), pass.c_str());
  return true;
}

bool saveNvs() {
  Preferences prefs;
  if (!prefs.begin(desk_display::kWifiNvsNamespace, false)) {
    return false;
  }
  const bool ok = prefs.putString(desk_display::kWifiNvsKeySsid, gSsid) > 0 &&
                  prefs.putString(desk_display::kWifiNvsKeyPass, gPass) > 0;
  prefs.end();
  return ok;
}

bool resolveCredentials() {
#ifdef WIFI_FORCE_CONFIG
  copyCred(WIFI_SSID, WIFI_PASS);
  saveNvs();
  Serial.println("wifi: seeded from config.h (FORCE)");
#else
  if (loadNvs() && !desk_display::isPlaceholderWifiSsid(gSsid)) {
    Serial.println("wifi: credentials from NVS");
  } else {
    copyCred(WIFI_SSID, WIFI_PASS);
    if (!desk_display::isPlaceholderWifiSsid(gSsid)) {
      saveNvs();
    }
    Serial.println("wifi: seeded from config.h");
  }
#endif

  if (desk_display::isPlaceholderWifiSsid(gSsid)) {
    Serial.println("wifi: set WIFI_SSID/WIFI_PASS in include/config.h");
    return false;
  }
  return true;
}

bool connectBlocking() {
  WiFi.begin(gSsid, gPass);
  const uint32_t start = millis();
  uint32_t elapsedCount = 0;
  while (WiFi.status() != WL_CONNECTED &&
         (millis() - start) < desk_display::kWifiConnectTimeoutMs) {
    delay(200);
    if (++elapsedCount % 5 == 0) {
      Serial.print(".");
    }
  }
  return WiFi.status() == WL_CONNECTED;
}

void logConnected() {
  Serial.printf("wifi: connected ssid=%s ip=%s rssi=%d\n", gSsid,
                WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

}  // namespace

void wifiSetup() {
  Serial.println("wifi: setup");
  WiFi.mode(WIFI_STA);
  if (!resolveCredentials()) {
    gConfigured = false;
    return;
  }
  gConfigured = true;
  if (connectBlocking()) {
    gWasConnected = true;
    gRetryDelayMs = 0;
    logConnected();
  } else {
    gWasConnected = false;
    gRetryDelayMs = desk_display::nextWifiRetryDelayMs(0);
    gNextRetryAtMs = millis() + gRetryDelayMs;
    Serial.printf("wifi: connect failed status=%d; retry in %u ms\n",
                  static_cast<int>(WiFi.status()), gRetryDelayMs);
  }
}

void wifiLoop() {
  if (!gConfigured) {
    return;
  }
  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected) {
    if (!gWasConnected) {
      logConnected();
    }
    gWasConnected = true;
    gRetryDelayMs = 0;
    return;
  }
  if (gWasConnected) {
    Serial.println("wifi: disconnected");
    gWasConnected = false;
    gRetryDelayMs = desk_display::nextWifiRetryDelayMs(0);
    gNextRetryAtMs = millis() + gRetryDelayMs;
  }
  if (static_cast<int32_t>(millis() - gNextRetryAtMs) < 0) {
    return;
  }
  Serial.println("wifi: retrying");
  WiFi.disconnect();
  WiFi.begin(gSsid, gPass);
  gRetryDelayMs = desk_display::nextWifiRetryDelayMs(gRetryDelayMs);
  gNextRetryAtMs = millis() + gRetryDelayMs;
}

bool wifiIsConnected() { return WiFi.status() == WL_CONNECTED; }

}  // namespace desk_net
