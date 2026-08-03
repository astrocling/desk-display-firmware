/**
 * Desk Display — Dial firmware entry (Waveshare ESP32-S3 Knob 1.8).
 */
#include <Arduino.h>

#include "hal/lvgl_port.hpp"
#include "net/wifi.hpp"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("desk-display-firmware: dial");
  desk_net::wifiSetup();
  if (!desk_hal::lvglPortInit()) {
    Serial.println("display: lvgl port failed");
  }
}

void loop() {
  desk_net::wifiLoop();
  desk_hal::lvglPortHandler();
  delay(5);
}
