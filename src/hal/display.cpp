#include "hal/display.hpp"
#include "hal/board_pins.hpp"

#include <Arduino.h>

namespace desk_hal {

void displaySetBacklight(bool on) {
  pinMode(pins::kLcdBl, OUTPUT);
  digitalWrite(pins::kLcdBl, on ? HIGH : LOW);
}

bool displayInit() {
  displaySetBacklight(true);
  Serial.println("display: backlight on (panel init pending)");
  return false;  // Task 2 replaces with real panel init
}

bool displayFlush(int, int, int, int, const uint16_t*) { return false; }

}  // namespace desk_hal
