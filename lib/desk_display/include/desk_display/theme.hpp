#pragma once

#include <cstdint>

namespace desk_display {
namespace theme {

/** Shared color tokens (RGB888). No LVGL dependency — Track C maps these to styles. */

constexpr uint32_t kBg = 0x0B0F14;      // near-black background
constexpr uint32_t kAccent = 0x3D9CF0;  // primary accent (links, focus ring)
constexpr uint32_t kDim = 0x6B7280;     // secondary / muted text
constexpr uint32_t kAlert = 0xE85D4C;   // warnings / attention
constexpr uint32_t kMilitary = 0xC4A35A;  // military / government traffic mark

/** Timezone board row status icons. */
constexpr uint32_t kStatusWorking = 0x34D399;  // green — work hours
constexpr uint32_t kStatusAwake = 0xF5A623;    // amber — awake off-hours
constexpr uint32_t kStatusNight = 0x9AA8C7;    // moon / night

}  // namespace theme
}  // namespace desk_display
