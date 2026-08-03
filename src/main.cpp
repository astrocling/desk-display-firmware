/**
 * Desk Display — Dial firmware entry (Waveshare ESP32-S3 Knob 1.8).
 */
#include <Arduino.h>

#include "net/wifi.hpp"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("desk-display-firmware: dial");
  desk_net::wifiSetup();
}

void loop() {
  desk_net::wifiLoop();
  delay(100);
}
