#include "hal/encoder.hpp"
#include "hal/board_pins.hpp"
#include "hal/bidi_switch_knob.h"

#include "desk_display/encoder_decode.hpp"

#include <Arduino.h>

namespace desk_hal {
namespace {

// Invert so clockwise (viewer-facing, USB-at-top desk mount) maps to positive
// ticks for focused screens (zoom out / later hours). Display MADCTL is also
// 180° for that mount. Carousel browse flips the sign again in dial_shell.
constexpr bool kEncoderInvert = true;

portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
volatile int32_t s_accum = 0;
knob_handle_t s_knob = nullptr;

void onKnobRight(void*, void*) {
  portENTER_CRITICAL(&s_mux);
  s_accum += 1;
  portEXIT_CRITICAL(&s_mux);
}

void onKnobLeft(void*, void*) {
  portENTER_CRITICAL(&s_mux);
  s_accum -= 1;
  portEXIT_CRITICAL(&s_mux);
}

}  // namespace

bool encoderInit() {
  s_accum = 0;

  // Official Waveshare Knob 1.8 driver (bidi pulse, 3 ms timer poll).
  const knob_config_t cfg = {
      .gpio_encoder_a = static_cast<uint8_t>(pins::kEncoderA),
      .gpio_encoder_b = static_cast<uint8_t>(pins::kEncoderB),
  };
  s_knob = iot_knob_create(&cfg);
  if (s_knob == nullptr) {
    Serial.println("encoder: iot_knob create failed");
    return false;
  }
  if (iot_knob_register_cb(s_knob, KNOB_RIGHT, onKnobRight, nullptr) != ESP_OK ||
      iot_knob_register_cb(s_knob, KNOB_LEFT, onKnobLeft, nullptr) != ESP_OK) {
    Serial.println("encoder: iot_knob cb register failed");
    return false;
  }
  Serial.println("encoder: ready");
  return true;
}

int8_t encoderDrain() {
  int32_t raw = 0;
  portENTER_CRITICAL(&s_mux);
  raw = s_accum;
  s_accum = 0;
  portEXIT_CRITICAL(&s_mux);
  if (kEncoderInvert) {
    raw = -raw;
  }
  return desk_display::clampEncoderDelta(raw);
}

}  // namespace desk_hal
