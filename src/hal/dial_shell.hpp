#pragma once

#include <cstdint>

namespace desk_hal {

/** Build carousel + focused hosts after LVGL is ready. */
bool dialShellInit();

/** Encoder ticks (positive = next in carousel / timezone scrub when focused). */
void dialShellOnRotate(int8_t delta);

/** Center tap — Nav Carousel ↔ Focused. */
void dialShellOnCenterTap();

/**
 * Advance idle, sync clock from NTP when available, refresh UI as needed.
 * Call once per loop with elapsed ms.
 */
void dialShellOnTick(uint32_t elapsed_ms);

}  // namespace desk_hal
