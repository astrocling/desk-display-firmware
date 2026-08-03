/**
 * Desk Display — Dial firmware entry (Waveshare ESP32-S3 Knob 1.8).
 */
#include <Arduino.h>

#include "hal/dial_shell.hpp"
#include "hal/encoder.hpp"
#include "hal/lvgl_port.hpp"
#include "hal/touch.hpp"
#include "net/ntp.hpp"
#include "net/wifi.hpp"

namespace {

uint32_t g_last_ms = 0;

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("desk-display-firmware: dial");
  desk_net::wifiSetup();
  desk_net::ntpSetup();
  if (!desk_hal::lvglPortInit()) {
    Serial.println("display: lvgl port failed");
  }
  desk_hal::encoderInit();
  desk_hal::touchInit();  // may fail; rotate-only still OK
  if (!desk_hal::dialShellInit()) {
    Serial.println("shell: init failed");
  }
  g_last_ms = millis();
}

void loop() {
  desk_net::wifiLoop();
  desk_net::ntpLoop();
  desk_hal::lvglPortHandler();

  const uint32_t now = millis();
  uint32_t elapsed = now - g_last_ms;
  g_last_ms = now;

  const int8_t rot = desk_hal::encoderDrain();
  if (rot != 0) {
    desk_hal::dialShellOnRotate(rot);
  }
  if (desk_hal::touchPollCenterTap()) {
    desk_hal::dialShellOnCenterTap();
  }

  desk_hal::dialShellOnTick(elapsed);

  delay(5);
}
