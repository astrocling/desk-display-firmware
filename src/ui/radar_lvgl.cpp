#include "radar_lvgl.hpp"

#include "desk_display/radar.hpp"
#include "desk_display/theme.hpp"

#include <cmath>
#include <cstddef>
#include <cstdio>

namespace desk_ui {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr int kRingCount = 3;
constexpr int kMaxDots = 40;
constexpr lv_coord_t kDotPx = 6;
constexpr uint32_t kRingColor = 0x2A323D;
constexpr uint32_t kSweepColor = 0x3DFF7A;
constexpr uint32_t kDotColor = 0x3DFF7A;

lv_color_t rgb(uint32_t c) {
  return lv_color_make(static_cast<uint8_t>((c >> 16) & 0xFF),
                        static_cast<uint8_t>((c >> 8) & 0xFF),
                        static_cast<uint8_t>(c & 0xFF));
}

// LVGL line objects keep a pointer to caller-owned point storage for their
// lifetime; since only one sweep line exists at a time (the disc is fully
// torn down and rebuilt on every refresh), a single static buffer is safe to
// reuse across calls.
lv_point_t g_sweep_points[2];

void build_rings(lv_obj_t* disc) {
  for (int i = 1; i <= kRingCount; ++i) {
    const lv_coord_t d = static_cast<lv_coord_t>(kRadarDiscPx * i / kRingCount);
    lv_obj_t* ring = lv_obj_create(disc);
    lv_obj_set_size(ring, d, d);
    lv_obj_center(ring);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ring, 1, 0);
    lv_obj_set_style_border_color(ring, rgb(kRingColor), 0);
    lv_obj_set_style_pad_all(ring, 0, 0);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);
  }
}

/** Radial line at `sweepAngleDeg`, 0° = north (up), increasing clockwise. */
void build_sweep(lv_obj_t* disc, float sweepAngleDeg) {
  const float rad = sweepAngleDeg * kPi / 180.0f;
  const float r = static_cast<float>(kRadarDiscPx) / 2.0f;
  const lv_coord_t cx = kRadarDiscPx / 2;
  const lv_coord_t cy = kRadarDiscPx / 2;

  g_sweep_points[0] = {cx, cy};
  g_sweep_points[1] = {
      static_cast<lv_coord_t>(cx + r * std::sin(rad)),
      static_cast<lv_coord_t>(cy - r * std::cos(rad)),
  };

  lv_obj_t* line = lv_line_create(disc);
  lv_obj_set_pos(line, 0, 0);
  lv_line_set_points(line, g_sweep_points, 2);
  lv_obj_set_style_line_width(line, 2, 0);
  lv_obj_set_style_line_color(line, rgb(kSweepColor), 0);
  lv_obj_set_style_line_rounded(line, false, 0);
  lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
}

void build_dots(lv_obj_t* disc, const desk_display::RadarView& v) {
  const float scale = 110.0f / (v.rangeMiles > 0 ? v.rangeMiles : 1.0f);
  const std::size_t count =
      v.blipCount < static_cast<std::size_t>(kMaxDots) ? v.blipCount : kMaxDots;
  for (std::size_t i = 0; i < count; ++i) {
    const auto& b = v.blips[i];
    lv_obj_t* dot = lv_obj_create(disc);
    lv_obj_set_size(dot, kDotPx, kDotPx);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, rgb(kDotColor), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_pad_all(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    const lv_coord_t x = static_cast<lv_coord_t>(b.offsetXMi * scale);
    const lv_coord_t y = static_cast<lv_coord_t>(-b.offsetYMi * scale);
    lv_obj_align(dot, LV_ALIGN_CENTER, x, y);
  }
}

}  // namespace

void radar_lvgl_build(lv_obj_t* parent, const desk_display::RadarView& v) {
  using desk_display::RadarMode;

  char hdr[48];
  std::snprintf(hdr, sizeof(hdr), "%s · %.0f mi · %zu",
                v.mode == RadarMode::ClassicSweep ? "Sweep" : "Detail",
                static_cast<double>(v.rangeMiles), v.blipCount);
  lv_obj_t* hdr_lab = lv_label_create(parent);
  lv_label_set_text(hdr_lab, hdr);
  lv_obj_set_style_text_font(hdr_lab, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(hdr_lab, rgb(desk_display::theme::kDim), 0);
  lv_obj_align(hdr_lab, LV_ALIGN_TOP_MID, 0, 4);

  lv_obj_t* disc = lv_obj_create(parent);
  lv_obj_set_size(disc, kRadarDiscPx, kRadarDiscPx);
  lv_obj_center(disc);
  lv_obj_set_style_bg_opa(disc, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(disc, 0, 0);
  lv_obj_set_style_pad_all(disc, 0, 0);
  lv_obj_clear_flag(disc, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(disc, LV_OBJ_FLAG_CLICKABLE);

  build_rings(disc);
  if (v.mode == RadarMode::ClassicSweep) {
    build_sweep(disc, v.sweepAngleDeg);
  }
  build_dots(disc, v);
}

}  // namespace desk_ui
