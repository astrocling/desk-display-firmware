#pragma once

namespace desk_hal {

bool touchInit();
/** True if a short press completed since last successful poll path. */
bool touchPollCenterTap();

}  // namespace desk_hal
