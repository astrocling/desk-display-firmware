#pragma once

#include <cstdint>

namespace desk_display {

constexpr int8_t kEncoderDeltaClamp = 8;

class EncoderDecoder {
 public:
  void reset();
  /** Feed current A/B levels; return ticks from this transition (−1/0/+1). */
  int8_t update(bool a_high, bool b_high);

 private:
  bool have_prev_ = false;
  uint8_t prev_ = 0;
  int8_t partial_ = 0;  // accumulate edge direction; emit ±1 every 4 valid steps
};

int8_t clampEncoderDelta(int32_t delta);

}  // namespace desk_display
