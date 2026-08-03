#pragma once

#include <cstddef>

#include "desk_display/nav.hpp"

namespace desk_display {

const char* screenTitleUpper(Screen s);
const char* navModeUpper(NavMode m);
void formatNavOverlay(NavMode mode, Screen screen, char* buf, size_t buf_len);
void formatNavSerial(NavMode mode, Screen screen, char* buf, size_t buf_len);

}  // namespace desk_display
