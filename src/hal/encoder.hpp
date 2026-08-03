#pragma once

#include <cstdint>

namespace desk_hal {

bool encoderInit();
/** Signed ticks since last call, clamped to ±kEncoderDeltaClamp. */
int8_t encoderDrain();

}  // namespace desk_hal
