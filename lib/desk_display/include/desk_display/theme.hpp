#pragma once

#include <cstdint>

namespace desk_display {
namespace theme {

/** Shared color tokens (RGB888). No LVGL dependency — Track C maps these to styles. */

constexpr uint32_t kBg = 0x0B0F14;      // near-black background
constexpr uint32_t kAccent = 0x3D9CF0;  // primary accent (links, focus ring)
constexpr uint32_t kDim = 0x6B7280;     // secondary / muted text
constexpr uint32_t kAlert = 0xE85D4C;   // warnings / attention

}  // namespace theme
}  // namespace desk_display
