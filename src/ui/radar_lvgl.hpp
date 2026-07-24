#pragma once

#include "desk_display/screen_radar.hpp"

#include <lvgl.h>

namespace desk_ui {

/** Inset (px) kept between the disc's outer ring and the parent's round clip. */
constexpr lv_coord_t kRadarDiscInsetPx = 4;

/** Floor (px) for the computed disc diameter, guarding degenerate parents. */
constexpr lv_coord_t kRadarDiscMinPx = 60;

/** Pixels trimmed from the disc radius so plotted blips stay inside the outer ring. */
constexpr float kRadarPlotRadiusInsetPx = 10.0f;

/**
 * Disc diameter (px), sized from `parent`'s content box: the smaller of its
 * content width/height, inset by `kRadarDiscInsetPx` on each side so the
 * outer ring stays inside a round clip (display bezel or carousel preview).
 * Clamped to `kRadarDiscMinPx` for degenerate parents.
 *
 * Forces a layout pass on `parent` first: callers may invoke this right
 * after creating/resizing `parent` (e.g. `lv_pct(100)` on a freshly created
 * object), before LVGL has resolved percentage sizes, which would otherwise
 * measure a 0x0 content box and permanently clamp to `kRadarDiscMinPx`.
 */
inline lv_coord_t radar_disc_px_for_parent(lv_obj_t* parent) {
  lv_obj_update_layout(parent);
  const lv_coord_t cw = lv_obj_get_content_width(parent);
  const lv_coord_t ch = lv_obj_get_content_height(parent);
  const lv_coord_t avail = cw < ch ? cw : ch;
  const lv_coord_t disc = avail - kRadarDiscInsetPx * 2;
  return disc > kRadarDiscMinPx ? disc : kRadarDiscMinPx;
}

/** Blip plot radius (px) for a disc of the given diameter — inside the outer ring. */
inline float radar_plot_radius_px(lv_coord_t discPx) {
  return static_cast<float>(discPx) / 2.0f - kRadarPlotRadiusInsetPx;
}

inline float radar_blip_scale(float rangeMiles, float plotRadiusPx) {
  return plotRadiusPx / (rangeMiles > 0.0f ? rangeMiles : 1.0f);
}

/**
 * Build the radar screen (header, concentric rings, sweep line in Classic,
 * aircraft blips) as children of `parent`. Shared by the sim and the dial's
 * `screen_radar_*` LVGL implementation.
 *
 * Caller owns `parent`'s lifetime and should clear any previous children
 * (e.g. `lv_obj_clean`) before a full rebuild — then call
 * `radar_lvgl_invalidate()` so animate caches are dropped.
 */
void radar_lvgl_build(lv_obj_t* parent, const desk_display::RadarView& v);

/**
 * In-place Classic sweep + phosphor + dots update (no disc teardown).
 * Returns false if `parent` is not the currently built Classic disc (caller
 * should `lv_obj_clean` + `radar_lvgl_build`).
 */
bool radar_lvgl_animate_classic(lv_obj_t* parent,
                                const desk_display::RadarView& v);

/** Drop cached object pointers after the parent was cleaned or destroyed. */
void radar_lvgl_invalidate();

/**
 * Hit-test a tap in absolute display coordinates against the live radar disc.
 * Uses the same disc origin / scale as drawing. Only phosphor-visible blips
 * (or the current selection) are candidates. Returns true and writes
 * `*outIndex` when a blip is within the hit radius.
 */
bool radar_lvgl_hit_blip(lv_obj_t* parent, const desk_display::RadarView& v,
                         lv_coord_t absX, lv_coord_t absY, std::size_t* outIndex);

/**
 * Hit-test a tap against projected airport / POI marks (under traffic).
 * Aircraft hits take priority — call `radar_lvgl_hit_blip` first.
 */
bool radar_lvgl_hit_static(lv_obj_t* parent, const desk_display::RadarView& v,
                           lv_coord_t absX, lv_coord_t absY, std::size_t* outIndex);

}  // namespace desk_ui
