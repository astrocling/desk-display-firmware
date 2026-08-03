#include "desk_display/encoder_decode.hpp"

namespace desk_display {

namespace {

// Gray-code transition direction: +1 CW, −1 CCW, 0 illegal (Hamming distance ≠ 1).
// State encoding: (A << 1) | B.
constexpr int8_t kTransitionDir[4][4] = {
    // to:  00   01   10   11
    /*00*/ {0, +1, -1, 0},
    /*01*/ {-1, 0, 0, +1},
    /*10*/ {+1, 0, 0, -1},
    /*11*/ {0, -1, +1, 0},
};

}  // namespace

void EncoderDecoder::reset() {
  have_prev_ = false;
  prev_ = 0;
  partial_ = 0;
}

int8_t EncoderDecoder::update(bool a_high, bool b_high) {
  const uint8_t state = static_cast<uint8_t>((a_high ? 2 : 0) | (b_high ? 1 : 0));
  if (!have_prev_) {
    prev_ = state;
    have_prev_ = true;
    return 0;
  }

  if (state == prev_) {
    return 0;
  }

  const int8_t step = kTransitionDir[prev_][state];
  if (step == 0) {
    return 0;
  }

  prev_ = state;
  partial_ = static_cast<int8_t>(partial_ + step);

  if (partial_ >= 4) {
    partial_ = static_cast<int8_t>(partial_ - 4);
    return 1;
  }
  if (partial_ <= -4) {
    partial_ = static_cast<int8_t>(partial_ + 4);
    return -1;
  }
  return 0;
}

int8_t clampEncoderDelta(int32_t delta) {
  if (delta > kEncoderDeltaClamp) {
    return kEncoderDeltaClamp;
  }
  if (delta < -kEncoderDeltaClamp) {
    return static_cast<int8_t>(-kEncoderDeltaClamp);
  }
  return static_cast<int8_t>(delta);
}

}  // namespace desk_display
