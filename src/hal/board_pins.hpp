#pragma once

#include <cstdint>

namespace desk_hal {
namespace pins {

constexpr int kLcdCs = 14;
constexpr int kLcdPclk = 13;
constexpr int kLcdData0 = 15;
constexpr int kLcdData1 = 16;
constexpr int kLcdData2 = 17;
constexpr int kLcdData3 = 18;
constexpr int kLcdRst = 21;
constexpr int kLcdBl = 47;

constexpr int kEncoderA = 8;
constexpr int kEncoderB = 7;
constexpr int kTouchSda = 11;
constexpr int kTouchScl = 12;
constexpr uint8_t kTouchAddr = 0x15;

}  // namespace pins
}  // namespace desk_hal
