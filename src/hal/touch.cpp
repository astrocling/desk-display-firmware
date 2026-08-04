#include "hal/touch.hpp"
#include "hal/board_pins.hpp"
#include "hal/display.hpp"

#include <Arduino.h>
#include <Wire.h>

namespace desk_hal {
namespace {

bool s_ok = false;
desk_display::TouchGestureDetector s_gest;
int16_t s_last_x = 0;
int16_t s_last_y = 0;

bool readTouchSample(bool& down, int16_t& x, int16_t& y) {
  Wire.beginTransmission(pins::kTouchAddr);
  Wire.write(0x00);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  const size_t n = Wire.requestFrom(pins::kTouchAddr, static_cast<uint8_t>(7));
  if (n < 7) {
    return false;
  }
  uint8_t data[7] = {};
  for (size_t i = 0; i < n && i < 7; ++i) {
    data[i] = static_cast<uint8_t>(Wire.read());
  }
  // data[0]=? data[1]=GestureID data[2]=FingerNum
  // data[3]=XposH data[4]=XposL data[5]=YposH data[6]=YposL
  down = data[2] > 0;
  const int raw_x = ((data[3] & 0x0F) << 8) | data[4];
  const int raw_y = ((data[5] & 0x0F) << 8) | data[6];

  if (down) {
    // Desk-mount MADCTL 180° — coords match LVGL/display space.
    constexpr bool kTouchMap180 = true;
    int mapped_x = raw_x;
    int mapped_y = raw_y;
    if constexpr (kTouchMap180) {
      mapped_x = kLcdWidth - 1 - raw_x;
      mapped_y = kLcdHeight - 1 - raw_y;
    }
    if (mapped_x < 0) {
      mapped_x = 0;
    } else if (mapped_x > kLcdWidth - 1) {
      mapped_x = kLcdWidth - 1;
    }
    if (mapped_y < 0) {
      mapped_y = 0;
    } else if (mapped_y > kLcdHeight - 1) {
      mapped_y = kLcdHeight - 1;
    }
    s_last_x = static_cast<int16_t>(mapped_x);
    s_last_y = static_cast<int16_t>(mapped_y);
    x = s_last_x;
    y = s_last_y;
  } else {
    x = s_last_x;
    y = s_last_y;
  }
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
  s_gest.reset();
  s_last_x = 0;
  s_last_y = 0;
  s_ok = true;
  Serial.println("touch: ready");
  return true;
}

bool touchPoll(desk_display::TouchGesture& out) {
  out = {};
  if (!s_ok) {
    return false;
  }
  bool down = false;
  int16_t x = 0;
  int16_t y = 0;
  if (!readTouchSample(down, x, y)) {
    // Still advance detector time with last known up so pending Tap can flush
    out = s_gest.update(false, 0, 0, millis());
    return out.kind != desk_display::TouchGestureKind::None;
  }
  out = s_gest.update(down, x, y, millis());
  return out.kind != desk_display::TouchGestureKind::None;
}

}  // namespace desk_hal
