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
constexpr uint32_t kSweepColor = 0x3DFF7A;
constexpr uint32_t kDotColor = 0x3DFF7A;
constexpr uint32_t kSelectedColor = 0xFFFFFF;
constexpr uint32_t kLeaderColor = 0x3D9CF0;

// Velocity vector length (px) scaled from ground speed (kt); mid length when
// speed is unknown but a track is present.
constexpr float kVectorLenMinPx = 8.0f;
constexpr float kVectorLenMaxPx = 28.0f;
constexpr float kVectorLenDefaultPx = 16.0f;
constexpr float kVectorLenScale = 0.04f;

lv_color_t rgb(uint32_t c) {
  return lv_color_make(static_cast<uint8_t>((c >> 16) & 0xFF),
                        static_cast<uint8_t>((c >> 8) & 0xFF),
                        static_cast<uint8_t>(c & 0xFF));
}

float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// LVGL line objects keep a pointer to caller-owned point storage for their
// lifetime; since only one disc (and thus one set of lines) exists at a
// time (fully torn down and rebuilt on every refresh), static buffers sized
// for the worst case are safe to reuse across calls.
lv_point_t g_sweep_points[2];
lv_point_t g_vector_points[kMaxDots][2];
lv_point_t g_leader_points[2];

void build_rings(lv_obj_t* disc) {
  for (int i = 1; i <= kRingCount; ++i) {
    const lv_coord_t d = static_cast<lv_coord_t>(kRadarDiscPx * i / kRingCount);
    lv_obj_t* ring = lv_obj_create(disc);
    lv_obj_set_size(ring, d, d);
    lv_obj_center(ring);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ring, 1, 0);
    lv_obj_set_style_border_color(ring, rgb(desk_display::theme::kDim), 0);
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

/**
 * Detail path: every blip gets a star mark, plus a velocity vector when
 * `hasTrack`. The selected blip (if any) also gets a leader line to its
 * callsign + `tagLine2` ATC-lite tag.
 */
void build_detail(lv_obj_t* disc, const desk_display::RadarView& v) {
  const float scale = 110.0f / (v.rangeMiles > 0 ? v.rangeMiles : 1.0f);
  const std::size_t count =
      v.blipCount < static_cast<std::size_t>(kMaxDots) ? v.blipCount : kMaxDots;
  const lv_coord_t cx = kRadarDiscPx / 2;
  const lv_coord_t cy = kRadarDiscPx / 2;

  for (std::size_t i = 0; i < count; ++i) {
    const auto& b = v.blips[i];
    const bool selected = v.hasSelection && v.selectedIndex == i;
    const lv_coord_t x = static_cast<lv_coord_t>(b.offsetXMi * scale);
    const lv_coord_t y = static_cast<lv_coord_t>(-b.offsetYMi * scale);
    const uint32_t markColor = selected ? kSelectedColor : kDotColor;

    lv_obj_t* star = lv_label_create(disc);
    lv_label_set_text(star, "*");
    lv_obj_set_style_text_font(star, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(star, rgb(markColor), 0);
    lv_obj_clear_flag(star, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(star, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(star, LV_ALIGN_CENTER, x, y);

    if (b.aircraft.hasTrack) {
      const float rad = b.aircraft.trackDeg * kPi / 180.0f;
      const float len = b.aircraft.hasSpeed
                             ? clampf(b.aircraft.speedKt * kVectorLenScale,
                                      kVectorLenMinPx, kVectorLenMaxPx)
                             : kVectorLenDefaultPx;
      const float dx = std::sin(rad) * len;
      const float dy = -std::cos(rad) * len;
      const lv_coord_t bx = cx + x;
      const lv_coord_t by = cy + y;

      g_vector_points[i][0] = {bx, by};
      g_vector_points[i][1] = {
          static_cast<lv_coord_t>(bx + dx),
          static_cast<lv_coord_t>(by + dy),
      };

      lv_obj_t* vec = lv_line_create(disc);
      lv_obj_set_pos(vec, 0, 0);
      lv_line_set_points(vec, g_vector_points[i], 2);
      lv_obj_set_style_line_width(vec, selected ? 2 : 1, 0);
      lv_obj_set_style_line_color(vec, rgb(markColor), 0);
      lv_obj_set_style_line_rounded(vec, false, 0);
      lv_obj_clear_flag(vec, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_clear_flag(vec, LV_OBJ_FLAG_SCROLLABLE);
    }

    if (selected) {
      constexpr lv_coord_t kLeaderDx = 16;
      constexpr lv_coord_t kLeaderDy = -16;
      const lv_coord_t bx = cx + x;
      const lv_coord_t by = cy + y;
      const lv_coord_t tagX = bx + kLeaderDx;
      const lv_coord_t tagY = by + kLeaderDy;

      g_leader_points[0] = {bx, by};
      g_leader_points[1] = {tagX, tagY};

      lv_obj_t* leader = lv_line_create(disc);
      lv_obj_set_pos(leader, 0, 0);
      lv_line_set_points(leader, g_leader_points, 2);
      lv_obj_set_style_line_width(leader, 1, 0);
      lv_obj_set_style_line_color(leader, rgb(kLeaderColor), 0);
      lv_obj_set_style_line_rounded(leader, false, 0);
      lv_obj_clear_flag(leader, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_clear_flag(leader, LV_OBJ_FLAG_SCROLLABLE);

      lv_obj_t* tag = lv_label_create(disc);
      lv_label_set_text(tag, v.detail.callsign);
      lv_obj_set_style_text_font(tag, &lv_font_montserrat_12, 0);
      lv_obj_set_style_text_color(tag, rgb(kSelectedColor), 0);
      lv_obj_set_pos(tag, tagX, tagY - 14);

      if (v.detail.tagLine2[0] != '\0') {
        lv_obj_t* tag2 = lv_label_create(disc);
        lv_label_set_text(tag2, v.detail.tagLine2);
        lv_obj_set_style_text_font(tag2, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(tag2, rgb(desk_display::theme::kAccent), 0);
        lv_obj_set_pos(tag2, tagX, tagY);
      }
    }
  }
}

/** Bottom detail card: callsign + altitude + speed for the selected blip. */
void build_detail_card(lv_obj_t* parent, const desk_display::RadarView& v) {
  char line[48];
  std::snprintf(line, sizeof(line), "%s%s%s%s%s", v.detail.callsign,
                v.detail.altLabel[0] ? "  " : "", v.detail.altLabel,
                v.detail.speedLabel[0] ? "  " : "", v.detail.speedLabel);

  lv_obj_t* card = lv_label_create(parent);
  lv_label_set_text(card, line);
  lv_obj_set_style_text_font(card, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(card, rgb(kSelectedColor), 0);
  lv_obj_align(card, LV_ALIGN_BOTTOM_MID, 0, -4);
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
    build_dots(disc, v);
  } else {
    build_detail(disc, v);
    if (v.hasSelection && v.detail.present) {
      build_detail_card(parent, v);
    }
  }
}

}  // namespace desk_ui
