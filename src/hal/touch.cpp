#include "hal/touch.hpp"
#include "hal/board_pins.hpp"

#include "desk_display/center_tap.hpp"

#include <Arduino.h>
#include <Wire.h>

namespace desk_hal {
namespace {

bool s_ok = false;
desk_display::CenterTapDetector s_tap;

bool readFingerDown(bool& down) {
  Wire.beginTransmission(pins::kTouchAddr);
  Wire.write(0x00);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  const size_t n = Wire.requestFrom(pins::kTouchAddr, static_cast<uint8_t>(7));
  if (n < 3) {
    return false;
  }
  uint8_t data[7] = {};
  for (size_t i = 0; i < n && i < 7; ++i) {
    data[i] = static_cast<uint8_t>(Wire.read());
  }
  down = data[2] > 0;
  return true;
}

}  // namespace

bool touchInit() {
  Wire.begin(pins::kTouchSda, pins::kTouchScl);
  Wire.beginTransmission(pins::kTouchAddr);
  if (Wire.endTransmission() != 0) {
    Serial.println("touch: not found");
    s_ok = false;
    return false;
  }
  s_tap.reset();
  s_ok = true;
  Serial.println("touch: ready");
  return true;
}

bool touchPollCenterTap() {
  if (!s_ok) {
    return false;
  }
  bool down = false;
  if (!readFingerDown(down)) {
    return false;
  }
  return s_tap.onContact(down, millis());
}

}  // namespace desk_hal
