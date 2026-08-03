/**
 * Desk Display — Dial firmware entry (Waveshare ESP32-S3 Knob 1.8).
 */
#include <Arduino.h>

#include "desk_display/nav.hpp"
#include "desk_display/nav_status.hpp"

#include "hal/encoder.hpp"
#include "hal/lvgl_port.hpp"
#include "hal/nav_overlay.hpp"
#include "hal/touch.hpp"
#include "net/wifi.hpp"

namespace {

desk_display::Nav g_nav;
uint32_t g_last_ms = 0;
desk_display::NavMode g_last_mode = desk_display::NavMode::Focused;
desk_display::Screen g_last_screen = desk_display::Screen::Clock;

void publishNavStatus() {
  char overlay[32];
  char serial[40];
  const auto mode = g_nav.mode();
  const auto screen = g_nav.active_screen();
  desk_display::formatNavOverlay(mode, screen, overlay, sizeof(overlay));
  desk_display::formatNavSerial(mode, screen, serial, sizeof(serial));
  desk_hal::navOverlaySetText(overlay);
  Serial.println(serial);
  g_last_mode = mode;
  g_last_screen = screen;
}

bool navChanged() {
  return g_nav.mode() != g_last_mode || g_nav.active_screen() != g_last_screen;
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("desk-display-firmware: dial");
  desk_net::wifiSetup();
  if (!desk_hal::lvglPortInit()) {
    Serial.println("display: lvgl port failed");
  }
  // display HAL already prints "display: ready" on success — do not duplicate
  desk_hal::encoderInit();
  desk_hal::touchInit();  // may fail; rotate-only still OK
  desk_hal::navOverlayInit();
  g_nav.reset();
  g_last_ms = millis();
  publishNavStatus();
}

void loop() {
  desk_net::wifiLoop();
  desk_hal::lvglPortHandler();

  const uint32_t now = millis();
  uint32_t elapsed = now - g_last_ms;
  g_last_ms = now;

  const int8_t rot = desk_hal::encoderDrain();
  if (rot != 0) {
    g_nav.on_rotate(rot);
  }
  if (desk_hal::touchPollCenterTap()) {
    g_nav.on_center_tap();
  }

  g_nav.on_tick(elapsed);

  if (navChanged()) {
    publishNavStatus();
  }

  delay(5);
}
