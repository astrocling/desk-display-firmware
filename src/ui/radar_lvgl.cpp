#include "radar_lvgl.hpp"

#include "desk_display/radar.hpp"
#include "desk_display/screen_radar.hpp"
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
constexpr uint32_t kSweepColor = 0x00FF00;
constexpr uint32_t kDotColor = 0x00FF00;
constexpr uint32_t kSelectedColor = 0xFFFFFF;
constexpr uint32_t kLeaderColor = 0x3D9CF0;

// Compact phosphor trail — enough to read motion without washing the map.
constexpr float kTrailArcDeg = 12.0f;
constexpr int kTrailSlices = 8;
constexpr lv_coord_t kTrailLineWidth = 1;

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

bool show_vectors(float rangeMiles) {
  return rangeMiles <= desk_display::kRadarVectorMaxRangeMi + 0.01f;
}

// LVGL line objects keep a pointer to caller-owned point storage for their
// lifetime; static buffers sized for the worst case are safe while a single
// disc is alive (torn down on full rebuild).
lv_point_t g_sweep_points[2];
lv_point_t g_trail_points[kTrailSlices][2];
lv_point_t g_vector_points[kMaxDots][2];
lv_point_t g_leader_points[2];

// Animate cache — valid only while the built disc for `g_parent` is intact.
lv_obj_t* g_parent = nullptr;
lv_obj_t* g_hdr = nullptr;
lv_obj_t* g_card = nullptr;
lv_obj_t* g_disc = nullptr;
lv_obj_t* g_trail_rays[kTrailSlices] = {};
lv_obj_t* g_sweep_line = nullptr;
lv_obj_t* g_blips_layer = nullptr;
bool g_built = false;

// Geometry for the currently built disc — sized from the parent at build
// time (Task 5); animate-path helpers reuse these instead of a fixed const.
lv_coord_t g_disc_px = 0;
float g_plot_radius_px = 0.0f;

void sweep_endpoint(float angleDeg, lv_coord_t cx, lv_coord_t cy, float r,
                    lv_point_t& out) {
  const float rad = angleDeg * kPi / 180.0f;
  out.x = static_cast<lv_coord_t>(cx + r * std::sin(rad));
  out.y = static_cast<lv_coord_t>(cy - r * std::cos(rad));
}

void format_header(char* hdr, std::size_t hdrLen, const desk_display::RadarView& v) {
  std::snprintf(hdr, hdrLen, "%.0f mi · %zu%s",
                static_cast<double>(v.rangeMiles), v.blipCount,
                show_vectors(v.rangeMiles) ? " · vec" : "");
}

void format_detail_card_line(char* line, std::size_t lineLen,
                             const desk_display::RadarView& v) {
  std::snprintf(line, lineLen, "%s%s%s%s%s", v.detail.callsign,
                v.detail.altLabel[0] ? "  " : "", v.detail.altLabel,
                v.detail.speedLabel[0] ? "  " : "", v.detail.speedLabel);
}

void build_rings(lv_obj_t* disc) {
  for (int i = 1; i <= kRingCount; ++i) {
    const lv_coord_t d = static_cast<lv_coord_t>(g_disc_px * i / kRingCount);
    lv_obj_t* ring = lv_obj_create(disc);
    lv_obj_set_size(ring, d, d);
    lv_obj_center(ring);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ring, 1, 0);
    lv_obj_set_style_border_color(ring, rgb(0x006900), 0);
    lv_obj_set_style_pad_all(ring, 0, 0);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);
  }
}

void apply_trail_geometry(float sweepAngleDeg) {
  const float r = static_cast<float>(g_disc_px) / 2.0f - 1.0f;
  const lv_coord_t cx = g_disc_px / 2;
  const lv_coord_t cy = g_disc_px / 2;

  for (int i = 1; i <= kTrailSlices; ++i) {
    const float frac = static_cast<float>(i) / static_cast<float>(kTrailSlices);
    const float ang = sweepAngleDeg - kTrailArcDeg * (1.0f - frac);
    const int g6 = static_cast<int>(40.0f * frac * frac);
    const int g = g6 < 1 ? 1 : g6;
    const uint8_t green = static_cast<uint8_t>((g * 255) / 63);

    g_trail_points[i - 1][0] = {cx, cy};
    sweep_endpoint(ang, cx, cy, r, g_trail_points[i - 1][1]);

    if (g_trail_rays[i - 1]) {
      lv_line_set_points(g_trail_rays[i - 1], g_trail_points[i - 1], 2);
      lv_obj_set_style_line_color(g_trail_rays[i - 1], lv_color_make(0, green, 0),
                                  0);
      lv_obj_invalidate(g_trail_rays[i - 1]);
    }
  }

  g_sweep_points[0] = {cx, cy};
  sweep_endpoint(sweepAngleDeg, cx, cy, r, g_sweep_points[1]);
  if (g_sweep_line) {
    lv_line_set_points(g_sweep_line, g_sweep_points, 2);
    lv_obj_invalidate(g_sweep_line);
  }
}

void build_sweep(lv_obj_t* disc, float sweepAngleDeg) {
  for (int i = 0; i < kTrailSlices; ++i) {
    g_trail_rays[i] = lv_line_create(disc);
    lv_obj_set_pos(g_trail_rays[i], 0, 0);
    lv_obj_set_style_line_width(g_trail_rays[i], kTrailLineWidth, 0);
    lv_obj_set_style_line_opa(g_trail_rays[i], LV_OPA_COVER, 0);
    lv_obj_set_style_line_rounded(g_trail_rays[i], false, 0);
    lv_obj_clear_flag(g_trail_rays[i], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g_trail_rays[i], LV_OBJ_FLAG_SCROLLABLE);
  }

  g_sweep_line = lv_line_create(disc);
  lv_obj_set_pos(g_sweep_line, 0, 0);
  lv_obj_set_style_line_width(g_sweep_line, 2, 0);
  lv_obj_set_style_line_color(g_sweep_line, rgb(kSweepColor), 0);
  lv_obj_set_style_line_rounded(g_sweep_line, false, 0);
  lv_obj_clear_flag(g_sweep_line, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(g_sweep_line, LV_OBJ_FLAG_SCROLLABLE);

  apply_trail_geometry(sweepAngleDeg);
}

void draw_selected_tag(lv_obj_t* layer, const desk_display::RadarView& v,
                       lv_coord_t bx, lv_coord_t by) {
  constexpr lv_coord_t kLeaderDx = 16;
  constexpr lv_coord_t kLeaderDy = -16;
  const lv_coord_t tagX = bx + kLeaderDx;
  const lv_coord_t tagY = by + kLeaderDy;

  g_leader_points[0] = {bx, by};
  g_leader_points[1] = {tagX, tagY};

  lv_obj_t* leader = lv_line_create(layer);
  lv_obj_set_pos(leader, 0, 0);
  lv_line_set_points(leader, g_leader_points, 2);
  lv_obj_set_style_line_width(leader, 1, 0);
  lv_obj_set_style_line_color(leader, rgb(kLeaderColor), 0);
  lv_obj_set_style_line_rounded(leader, false, 0);
  lv_obj_clear_flag(leader, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(leader, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* tag = lv_label_create(layer);
  lv_label_set_text(tag, v.detail.callsign[0] ? v.detail.callsign : "?");
  lv_obj_set_style_text_font(tag, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(tag, rgb(kSelectedColor), 0);
  lv_obj_set_pos(tag, tagX, tagY - 14);

  if (v.detail.tagLine2[0] != '\0') {
    lv_obj_t* tag2 = lv_label_create(layer);
    lv_label_set_text(tag2, v.detail.tagLine2);
    lv_obj_set_style_text_font(tag2, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(tag2, rgb(desk_display::theme::kAccent), 0);
    lv_obj_set_pos(tag2, tagX, tagY);
  }
}

/**
 * Traffic layer: dense dots at long range; stars + velocity vectors when
 * zoomed in. Selected target stays bright and gets an ATC-lite tag.
 */
void build_traffic(lv_obj_t* layer, const desk_display::RadarView& v) {
  const float scale = radar_blip_scale(v.rangeMiles, g_plot_radius_px);
  const bool vectors = show_vectors(v.rangeMiles);
  const std::size_t count =
      v.blipCount < static_cast<std::size_t>(kMaxDots) ? v.blipCount : kMaxDots;
  const lv_coord_t cx = g_disc_px / 2;
  const lv_coord_t cy = g_disc_px / 2;

  for (std::size_t i = 0; i < count; ++i) {
    const auto& b = v.blips[i];
    const bool selected = v.hasSelection && v.selectedIndex == i;
    if (!selected && b.litAgeMs >= desk_display::kRadarBlipFadeMs) {
      continue;
    }

    const lv_coord_t x = static_cast<lv_coord_t>(b.offsetXMi * scale);
    const lv_coord_t y = static_cast<lv_coord_t>(-b.offsetYMi * scale);
    const lv_coord_t bx = cx + x;
    const lv_coord_t by = cy + y;
    const uint32_t markColor = selected ? kSelectedColor : kDotColor;

    if (vectors) {
      lv_obj_t* star = lv_label_create(layer);
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

        g_vector_points[i][0] = {bx, by};
        g_vector_points[i][1] = {
            static_cast<lv_coord_t>(bx + dx),
            static_cast<lv_coord_t>(by + dy),
        };

        lv_obj_t* vec = lv_line_create(layer);
        lv_obj_set_pos(vec, 0, 0);
        lv_line_set_points(vec, g_vector_points[i], 2);
        lv_obj_set_style_line_width(vec, selected ? 2 : 1, 0);
        lv_obj_set_style_line_color(vec, rgb(markColor), 0);
        lv_obj_set_style_line_rounded(vec, false, 0);
        lv_obj_clear_flag(vec, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(vec, LV_OBJ_FLAG_SCROLLABLE);
      }
    } else {
      const float ageFrac =
          static_cast<float>(b.litAgeMs) /
          static_cast<float>(desk_display::kRadarBlipFadeMs);
      const float alpha = selected ? 1.0f : (1.0f - ageFrac * 0.88f);
      const lv_opa_t opa = static_cast<lv_opa_t>(
          clampf(alpha * 255.0f, selected ? 255.0f : 20.0f, 255.0f));

      lv_obj_t* dot = lv_obj_create(layer);
      lv_obj_set_size(dot, selected ? kDotPx + 2 : kDotPx,
                      selected ? kDotPx + 2 : kDotPx);
      lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(dot, rgb(markColor), 0);
      lv_obj_set_style_bg_opa(dot, opa, 0);
      lv_obj_set_style_border_width(dot, selected ? 1 : 0, 0);
      lv_obj_set_style_border_color(dot, rgb(kSelectedColor), 0);
      lv_obj_set_style_pad_all(dot, 0, 0);
      lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_align(dot, LV_ALIGN_CENTER, x, y);
    }

    if (selected && v.detail.present) {
      draw_selected_tag(layer, v, bx, by);
    }
  }
}

void update_detail_card(lv_obj_t* parent, const desk_display::RadarView& v) {
  if (!(v.hasSelection && v.detail.present)) {
    if (g_card) {
      lv_obj_add_flag(g_card, LV_OBJ_FLAG_HIDDEN);
    }
    return;
  }

  char line[48];
  format_detail_card_line(line, sizeof(line), v);

  if (!g_card) {
    g_card = lv_label_create(parent);
    // Below header, inside the round viewport (bottom was clipped before).
    lv_obj_set_style_text_font(g_card, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(g_card, rgb(kSelectedColor), 0);
    lv_obj_align(g_card, LV_ALIGN_TOP_MID, 0, 18);
  }
  lv_label_set_text(g_card, line);
  lv_obj_clear_flag(g_card, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(g_card);
}

void clear_animate_cache() {
  g_parent = nullptr;
  g_hdr = nullptr;
  g_card = nullptr;
  g_disc = nullptr;
  g_sweep_line = nullptr;
  g_blips_layer = nullptr;
  g_built = false;
  g_disc_px = 0;
  g_plot_radius_px = 0.0f;
  for (int i = 0; i < kTrailSlices; ++i) {
    g_trail_rays[i] = nullptr;
  }
}

}  // namespace

void radar_lvgl_invalidate() { clear_animate_cache(); }

bool radar_lvgl_animate_classic(lv_obj_t* parent,
                                const desk_display::RadarView& v) {
  if (!parent || parent != g_parent || !g_built || !g_disc || !g_blips_layer ||
      !g_sweep_line) {
    return false;
  }
  for (int i = 0; i < kTrailSlices; ++i) {
    if (!g_trail_rays[i]) {
      return false;
    }
  }

  if (g_hdr) {
    char hdr[48];
    format_header(hdr, sizeof(hdr), v);
    lv_label_set_text(g_hdr, hdr);
  }

  apply_trail_geometry(v.sweepAngleDeg);

  lv_obj_clean(g_blips_layer);
  build_traffic(g_blips_layer, v);
  update_detail_card(parent, v);
  return true;
}

void radar_lvgl_build(lv_obj_t* parent, const desk_display::RadarView& v) {
  clear_animate_cache();

  char hdr[48];
  format_header(hdr, sizeof(hdr), v);
  g_hdr = lv_label_create(parent);
  lv_label_set_text(g_hdr, hdr);
  lv_obj_set_style_text_font(g_hdr, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(g_hdr, rgb(desk_display::theme::kDim), 0);
  lv_obj_align(g_hdr, LV_ALIGN_TOP_MID, 0, 2);

  g_disc_px = radar_disc_px_for_parent(parent);
  g_plot_radius_px = radar_plot_radius_px(g_disc_px);

  g_disc = lv_obj_create(parent);
  lv_obj_set_size(g_disc, g_disc_px, g_disc_px);
  lv_obj_center(g_disc);
  lv_obj_set_style_bg_opa(g_disc, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_disc, 0, 0);
  lv_obj_set_style_pad_all(g_disc, 0, 0);
  lv_obj_clear_flag(g_disc, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(g_disc, LV_OBJ_FLAG_CLICKABLE);

  build_rings(g_disc);
  build_sweep(g_disc, v.sweepAngleDeg);

  g_blips_layer = lv_obj_create(g_disc);
  lv_obj_set_size(g_blips_layer, g_disc_px, g_disc_px);
  lv_obj_set_pos(g_blips_layer, 0, 0);
  lv_obj_set_style_bg_opa(g_blips_layer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_blips_layer, 0, 0);
  lv_obj_set_style_pad_all(g_blips_layer, 0, 0);
  lv_obj_clear_flag(g_blips_layer, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(g_blips_layer, LV_OBJ_FLAG_CLICKABLE);
  build_traffic(g_blips_layer, v);

  g_parent = parent;
  g_built = true;
  update_detail_card(parent, v);
}

}  // namespace desk_ui
