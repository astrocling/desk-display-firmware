#pragma once

#include "desk_display/touch_gesture.hpp"

namespace desk_hal {

bool touchInit();
/** Poll CST816; returns true when a gesture event is ready in `out`. */
bool touchPoll(desk_display::TouchGesture& out);

}  // namespace desk_hal
