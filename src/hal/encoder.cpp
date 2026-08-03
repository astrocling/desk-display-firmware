#include "hal/encoder.hpp"
#include "hal/board_pins.hpp"

#include "desk_display/encoder_decode.hpp"

#include <Arduino.h>

namespace desk_hal {
namespace {

// Set true if rotation direction is inverted on hardware.
constexpr bool kEncoderInvert = false;

portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
desk_display::EncoderDecoder s_decoder;
volatile int32_t s_accum = 0;

void IRAM_ATTR onEncoderEdge() {
  const bool a = digitalRead(pins::kEncoderA);
  const bool b = digitalRead(pins::kEncoderB);
  const int8_t d = s_decoder.update(a, b);
  if (d != 0) {
    portENTER_CRITICAL_ISR(&s_mux);
    s_accum += d;
    portEXIT_CRITICAL_ISR(&s_mux);
  }
}

}  // namespace

bool encoderInit() {
  pinMode(pins::kEncoderA, INPUT_PULLUP);
  pinMode(pins::kEncoderB, INPUT_PULLUP);
  s_decoder.reset();
  s_accum = 0;
  s_decoder.update(digitalRead(pins::kEncoderA), digitalRead(pins::kEncoderB));
  attachInterrupt(digitalPinToInterrupt(pins::kEncoderA), onEncoderEdge, CHANGE);
  attachInterrupt(digitalPinToInterrupt(pins::kEncoderB), onEncoderEdge, CHANGE);
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
