#pragma once

#include <cstdint>

namespace desk_hal {

bool displayInit();
void displaySetBacklight(bool on);
/** Draw RGB565 buffer for inclusive rectangle [x0,y0]–[x1,y1]. */
bool displayFlush(int x0, int y0, int x1, int y1, const uint16_t* pixels);

constexpr int kLcdWidth = 360;
constexpr int kLcdHeight = 360;

}  // namespace desk_hal
