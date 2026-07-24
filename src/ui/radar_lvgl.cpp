#include "radar_lvgl.hpp"

#include "desk_display/aircraft_notable.hpp"
#include "desk_display/map_context.hpp"
#include "desk_display/radar.hpp"
#include "desk_display/radar_format.hpp"
#include "desk_display/screen_radar.hpp"
#include "desk_display/theme.hpp"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>

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
/** Unselected tag/leader opacity — readable but quieter than selection. */
constexpr lv_opa_t kTagDimOpa = LV_OPA_60;

constexpr uint32_t kAirspaceColorB = 0x3A6AA8;
constexpr uint32_t kAirspaceColorC = 0xA83A7A;
constexpr uint32_t kAirspaceColorD = 0x3A6AA8;
constexpr uint32_t kHighwayColor = 0x2A323C;
constexpr lv_coord_t kAirspaceDashWidth = 4;
constexpr lv_coord_t kAirspaceDashGap = 3;
constexpr uint32_t kAirportMarkColor = 0xFFFFFF;
constexpr uint32_t kPoiMarkColor = 0xAAAAAA;
constexpr lv_coord_t kPoiMarkPx = 6;
constexpr float kStaticHitRadiusPx = 28.0f;

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
lv_point_t g_leader_points[kMaxDots][2];

/** Absolute display-pixel hit targets written during the last build_traffic. */
struct BlipHitTarget {
  bool valid;
  bool hasTag;
  float markX;
  float markY;
  float tagX0;
  float tagY0;
  float tagX1;
  float tagY1;
};
BlipHitTarget g_hit_targets[kMaxDots];

void clear_hit_targets() {
  for (int i = 0; i < kMaxDots; ++i) {
    g_hit_targets[i].valid = false;
  }
}

// Animate cache — valid only while the built disc for `g_parent` is intact.
lv_obj_t* g_parent = nullptr;
lv_obj_t* g_hdr = nullptr;
lv_obj_t* g_disc = nullptr;
lv_obj_t* g_trail_rays[kTrailSlices] = {};
lv_obj_t* g_sweep_line = nullptr;
lv_obj_t* g_static_layer = nullptr;
lv_obj_t* g_blips_layer = nullptr;
bool g_built = false;

// Geometry for the currently built disc — sized from the parent at build
// time (Task 5); animate-path helpers reuse these instead of a fixed const.
lv_coord_t g_disc_px = 0;
float g_plot_radius_px = 0.0f;

// Overlay rebuild signature — static/airspace only when center/range/selection
// changes (sweep animate path skips when unchanged).
float g_cached_range = -1.0f;
double g_cached_center_lat = 0.0;
double g_cached_center_lon = 0.0;
std::size_t g_cached_static_count = 0;
std::size_t g_cached_ring_count = 0;
std::size_t g_cached_highway_count = 0;
bool g_cached_has_static_sel = false;
std::size_t g_cached_selected_static = 0;

constexpr int kMaxAirspaceSegs = 1280;
lv_point_t g_airspace_seg_pts[kMaxAirspaceSegs][2];
int g_airspace_seg_used = 0;

constexpr int kMaxHighwaySegs = 960;
lv_point_t g_highway_seg_pts[kMaxHighwaySegs][2];
int g_highway_seg_used = 0;

void sweep_endpoint(float angleDeg, lv_coord_t cx, lv_coord_t cy, float r,
                    lv_point_t& out) {
  const float rad = angleDeg * kPi / 180.0f;
  out.x = static_cast<lv_coord_t>(cx + r * std::sin(rad));
  out.y = static_cast<lv_coord_t>(cy - r * std::cos(rad));
}

void format_header(char* hdr, std::size_t hdrLen, const desk_display::RadarView& v) {
  // ASCII separators only — LVGL Montserrat fonts lack U+00B7 (·).
  std::snprintf(hdr, hdrLen, "%.0f mi - %zu%s",
                static_cast<double>(v.rangeMiles), v.blipCount,
                show_vectors(v.rangeMiles) ? " - vec" : "");
}

bool disc_hit_geometry(lv_obj_t* parent, const desk_display::RadarView& v,
                       float* outCx, float* outCy, float* outScale) {
  if (!parent || parent != g_parent || !g_built || !g_disc || g_disc_px <= 0 ||
      g_plot_radius_px <= 0.0f) {
    return false;
  }
  lv_area_t discArea{};
  lv_obj_get_coords(g_disc, &discArea);
  *outCx = static_cast<float>(discArea.x1) + static_cast<float>(g_disc_px) / 2.0f;
  *outCy = static_cast<float>(discArea.y1) + static_cast<float>(g_disc_px) / 2.0f;
  *outScale = radar_blip_scale(v.rangeMiles, g_plot_radius_px);
  return true;
}

void cache_overlay_state(const desk_display::RadarView& v) {
  g_cached_range = v.rangeMiles;
  g_cached_center_lat = v.centerLat;
  g_cached_center_lon = v.centerLon;
  g_cached_static_count = v.staticMarkCount;
  g_cached_ring_count = v.airspaceRingCount;
  g_cached_highway_count = v.highwayCount;
  g_cached_has_static_sel = v.hasStaticSelection;
  g_cached_selected_static = v.selectedStaticIndex;
}

bool overlay_needs_rebuild(const desk_display::RadarView& v) {
  return v.rangeMiles != g_cached_range || v.centerLat != g_cached_center_lat ||
         v.centerLon != g_cached_center_lon ||
         v.staticMarkCount != g_cached_static_count ||
         v.airspaceRingCount != g_cached_ring_count ||
         v.highwayCount != g_cached_highway_count ||
         v.hasStaticSelection != g_cached_has_static_sel ||
         v.selectedStaticIndex != g_cached_selected_static;
}

uint32_t airspace_color(desk_display::AirspaceClass cls) {
  switch (cls) {
    case desk_display::AirspaceClass::B:
      return kAirspaceColorB;
    case desk_display::AirspaceClass::C:
      return kAirspaceColorC;
    case desk_display::AirspaceClass::D:
      return kAirspaceColorD;
  }
  return kAirspaceColorB;
}

bool airspace_dashed(desk_display::AirspaceClass cls) {
  return cls == desk_display::AirspaceClass::D;
}

lv_obj_t* add_airspace_segment(lv_obj_t* layer, lv_coord_t x0, lv_coord_t y0,
                               lv_coord_t x1, lv_coord_t y1, uint32_t color) {
  if (g_airspace_seg_used >= kMaxAirspaceSegs) {
    return nullptr;
  }
  g_airspace_seg_pts[g_airspace_seg_used][0] = {x0, y0};
  g_airspace_seg_pts[g_airspace_seg_used][1] = {x1, y1};
  lv_obj_t* line = lv_line_create(layer);
  lv_obj_set_pos(line, 0, 0);
  lv_line_set_points(line, g_airspace_seg_pts[g_airspace_seg_used], 2);
  lv_obj_set_style_line_width(line, 1, 0);
  lv_obj_set_style_line_color(line, rgb(color), 0);
  lv_obj_set_style_line_rounded(line, false, 0);
  lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
  ++g_airspace_seg_used;
  return line;
}

void draw_airspace_edge(lv_obj_t* layer, lv_coord_t x0, lv_coord_t y0, lv_coord_t x1,
                        lv_coord_t y1, uint32_t color, bool dashed) {
  if (!dashed) {
    add_airspace_segment(layer, x0, y0, x1, y1, color);
    return;
  }

  const float dx = static_cast<float>(x1 - x0);
  const float dy = static_cast<float>(y1 - y0);
  const float len = std::sqrt(dx * dx + dy * dy);
  if (len < 0.5f) {
    return;
  }
  const float ux = dx / len;
  const float uy = dy / len;
  const float dash = static_cast<float>(kAirspaceDashWidth);
  const float gap = static_cast<float>(kAirspaceDashGap);
  float pos = 0.0f;
  bool drawing = true;
  while (pos < len) {
    const float segLen = drawing ? dash : gap;
    const float next = pos + segLen;
    if (drawing) {
      const float t0 = pos;
      const float t1 = next < len ? next : len;
      add_airspace_segment(
          layer, static_cast<lv_coord_t>(x0 + ux * t0),
          static_cast<lv_coord_t>(y0 + uy * t0),
          static_cast<lv_coord_t>(x0 + ux * t1),
          static_cast<lv_coord_t>(y0 + uy * t1), color);
    }
    pos = next;
    drawing = !drawing;
  }
}

void build_airspace(lv_obj_t* layer, const desk_display::RadarView& v) {
  if (!layer || !v.airspaceRings || v.airspaceRingCount == 0) {
    return;
  }

  g_airspace_seg_used = 0;
  const float scale = radar_blip_scale(v.rangeMiles, g_plot_radius_px);
  const lv_coord_t cx = g_disc_px / 2;
  const lv_coord_t cy = g_disc_px / 2;

  for (std::size_t r = 0; r < v.airspaceRingCount; ++r) {
    const auto& ring = v.airspaceRings[r];
    if (ring.pointCount < 2) {
      continue;
    }
    const uint32_t color = airspace_color(ring.cls);
    const bool dashed = airspace_dashed(ring.cls);
    for (uint8_t i = 0; i < ring.pointCount; ++i) {
      const uint8_t j = static_cast<uint8_t>((i + 1) % ring.pointCount);
      const lv_coord_t x0 =
          static_cast<lv_coord_t>(cx + ring.offsetXMi[i] * scale);
      const lv_coord_t y0 =
          static_cast<lv_coord_t>(cy - ring.offsetYMi[i] * scale);
      const lv_coord_t x1 =
          static_cast<lv_coord_t>(cx + ring.offsetXMi[j] * scale);
      const lv_coord_t y1 =
          static_cast<lv_coord_t>(cy - ring.offsetYMi[j] * scale);
      draw_airspace_edge(layer, x0, y0, x1, y1, color, dashed);
    }
  }
}

void build_highways(lv_obj_t* layer, const desk_display::RadarView& v) {
  if (!layer || !v.highways || v.highwayCount == 0) {
    return;
  }

  g_highway_seg_used = 0;
  const float scale = radar_blip_scale(v.rangeMiles, g_plot_radius_px);
  const lv_coord_t cx = g_disc_px / 2;
  const lv_coord_t cy = g_disc_px / 2;

  for (std::size_t h = 0; h < v.highwayCount; ++h) {
    const auto& hw = v.highways[h];
    if (hw.pointCount < 2) {
      continue;
    }
    for (uint8_t i = 0; i + 1 < hw.pointCount; ++i) {
      if (g_highway_seg_used >= kMaxHighwaySegs) {
        return;
      }
      const lv_coord_t x0 =
          static_cast<lv_coord_t>(cx + hw.offsetXMi[i] * scale);
      const lv_coord_t y0 =
          static_cast<lv_coord_t>(cy - hw.offsetYMi[i] * scale);
      const lv_coord_t x1 =
          static_cast<lv_coord_t>(cx + hw.offsetXMi[i + 1] * scale);
      const lv_coord_t y1 =
          static_cast<lv_coord_t>(cy - hw.offsetYMi[i + 1] * scale);
      g_highway_seg_pts[g_highway_seg_used][0] = {x0, y0};
      g_highway_seg_pts[g_highway_seg_used][1] = {x1, y1};
      lv_obj_t* line = lv_line_create(layer);
      lv_obj_set_pos(line, 0, 0);
      lv_line_set_points(line, g_highway_seg_pts[g_highway_seg_used], 2);
      lv_obj_set_style_line_width(line, 1, 0);
      lv_obj_set_style_line_color(line, rgb(kHighwayColor), 0);
      lv_obj_set_style_line_rounded(line, false, 0);
      lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
      ++g_highway_seg_used;
    }
  }
}

void build_static_marks(lv_obj_t* layer, const desk_display::RadarView& v) {
  if (!layer || !v.staticMarks || v.staticMarkCount == 0) {
    return;
  }

  const float scale = radar_blip_scale(v.rangeMiles, g_plot_radius_px);
  const lv_coord_t cx = g_disc_px / 2;
  const lv_coord_t cy = g_disc_px / 2;

  for (std::size_t i = 0; i < v.staticMarkCount; ++i) {
    const auto& mark = v.staticMarks[i];
    const lv_coord_t x = static_cast<lv_coord_t>(mark.offsetXMi * scale);
    const lv_coord_t y = static_cast<lv_coord_t>(-mark.offsetYMi * scale);
    const lv_coord_t bx = cx + x;
    const lv_coord_t by = cy + y;
    const bool selected = v.hasStaticSelection && v.selectedStaticIndex == i;

    if (mark.kind == desk_display::RadarStaticMark::Kind::Airport) {
      lv_obj_t* glyph = lv_label_create(layer);
      lv_label_set_text(glyph, "+");
      lv_obj_set_style_text_font(glyph, &lv_font_montserrat_12, 0);
      lv_obj_set_style_text_color(glyph, rgb(kAirportMarkColor), 0);
      lv_obj_clear_flag(glyph, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_clear_flag(glyph, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_align(glyph, LV_ALIGN_CENTER, x, y);
    } else {
      lv_obj_t* glyph = lv_obj_create(layer);
      lv_obj_set_size(glyph, kPoiMarkPx, kPoiMarkPx);
      lv_obj_set_style_radius(glyph, 0, 0);
      lv_obj_set_style_bg_color(glyph, rgb(kPoiMarkColor), 0);
      lv_obj_set_style_bg_opa(glyph, LV_OPA_COVER, 0);
      lv_obj_set_style_border_width(glyph, 0, 0);
      lv_obj_set_style_pad_all(glyph, 0, 0);
      lv_obj_clear_flag(glyph, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_clear_flag(glyph, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_align(glyph, LV_ALIGN_CENTER, x, y);
    }

    // Airports: always show a dim ICAO/code; selected bumps contrast.
    // POIs: label only when selected (names are longer / less chart-like).
    const bool showLabel =
        mark.label[0] != '\0' &&
        (selected || mark.kind == desk_display::RadarStaticMark::Kind::Airport);
    if (showLabel) {
      lv_obj_t* tag = lv_label_create(layer);
      lv_label_set_text(tag, mark.label);
      lv_obj_set_style_text_font(tag, &lv_font_montserrat_10, 0);
      lv_obj_set_style_text_color(tag, rgb(kAirportMarkColor), 0);
      lv_obj_set_style_text_opa(tag, selected ? LV_OPA_COVER : LV_OPA_50, 0);
      lv_obj_set_pos(tag, bx + 8, by - 12);
    }
  }
}

void build_static_overlay(lv_obj_t* layer, const desk_display::RadarView& v) {
  build_highways(layer, v);
  build_airspace(layer, v);
  build_static_marks(layer, v);
  cache_overlay_state(v);
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

void draw_blip_tag(lv_obj_t* layer, std::size_t blipIndex, const char* callsign,
                   const char* tagLine2, const char* tagLine3,
                   uint32_t callsignColor, bool selected, lv_coord_t bx,
                   lv_coord_t by) {
  // Place the tag toward disc center when the blip is near the rim so the
  // round clip doesn't swallow callsign / alt / type lines.
  constexpr lv_coord_t kLeaderLen = 18;
  // Extra margin for the taller selected (3-line) tag block.
  const lv_coord_t kTagMargin = selected ? 48 : 36;
  const lv_coord_t cx = g_disc_px / 2;
  const lv_coord_t cy = g_disc_px / 2;
  const float dx = static_cast<float>(cx - bx);
  const float dy = static_cast<float>(cy - by);
  const float dist = std::sqrt(dx * dx + dy * dy);
  lv_coord_t leaderDx = 16;
  lv_coord_t leaderDy = -16;
  if (dist > 1.0f) {
    const bool nearRim =
        bx < kTagMargin || by < kTagMargin ||
        bx > g_disc_px - kTagMargin || by > g_disc_px - kTagMargin;
    if (nearRim) {
      leaderDx = static_cast<lv_coord_t>((dx / dist) * kLeaderLen);
      leaderDy = static_cast<lv_coord_t>((dy / dist) * kLeaderLen);
    }
  }
  const lv_coord_t tagX = bx + leaderDx;
  const lv_coord_t tagY = by + leaderDy;
  const lv_opa_t opa = selected ? LV_OPA_COVER : kTagDimOpa;
  // Selected: callsign above line2; unselected dense: same. Line3 below line2.
  const lv_coord_t line1Y = tagY - 12;
  const lv_coord_t line2Y = tagY;
  const lv_coord_t line3Y = tagY + 12;

  g_leader_points[blipIndex][0] = {bx, by};
  g_leader_points[blipIndex][1] = {tagX, tagY};

  lv_obj_t* leader = lv_line_create(layer);
  lv_obj_set_pos(leader, 0, 0);
  lv_line_set_points(leader, g_leader_points[blipIndex], 2);
  lv_obj_set_style_line_width(leader, selected ? 2 : 1, 0);
  lv_obj_set_style_line_color(leader, rgb(kLeaderColor), 0);
  lv_obj_set_style_line_opa(leader, opa, 0);
  lv_obj_set_style_line_rounded(leader, false, 0);
  lv_obj_clear_flag(leader, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(leader, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* tag = lv_label_create(layer);
  lv_label_set_text(tag, (callsign && callsign[0]) ? callsign : "?");
  lv_obj_set_style_text_font(tag, &lv_font_montserrat_10, 0);
  lv_obj_set_style_text_color(tag, rgb(callsignColor), 0);
  lv_obj_set_style_text_opa(tag, opa, 0);
  lv_obj_set_pos(tag, tagX, line1Y);

  if (tagLine2 && tagLine2[0] != '\0') {
    lv_obj_t* tag2 = lv_label_create(layer);
    lv_label_set_text(tag2, tagLine2);
    lv_obj_set_style_text_font(tag2, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(tag2, rgb(desk_display::theme::kAccent), 0);
    lv_obj_set_style_text_opa(tag2, opa, 0);
    lv_obj_set_pos(tag2, tagX, line2Y);
  }

  if (selected && tagLine3 && tagLine3[0] != '\0') {
    lv_obj_t* tag3 = lv_label_create(layer);
    lv_label_set_text(tag3, tagLine3);
    lv_obj_set_style_text_font(tag3, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(tag3, rgb(desk_display::theme::kAccent), 0);
    lv_obj_set_style_text_opa(tag3, opa, 0);
    lv_obj_set_pos(tag3, tagX, line3Y);
  }
}

/**
 * Traffic layer: ≤25 mi → stars + vectors + tags on every painted blip;
 * above that → dense dots only (still selectable). Dense tags for unselected;
 * selected gets full-contrast 3-line tag.
 */
void build_traffic(lv_obj_t* layer, const desk_display::RadarView& v) {
  clear_hit_targets();

  lv_area_t layerArea{};
  lv_obj_update_layout(layer);
  lv_obj_get_coords(layer, &layerArea);
  const float originX = static_cast<float>(layerArea.x1);
  const float originY = static_cast<float>(layerArea.y1);

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
    uint32_t markColor = kDotColor;
    if (selected) {
      markColor = kSelectedColor;
    } else {
      switch (b.notable) {
        case desk_display::AircraftNotable::Emergency:
          markColor = desk_display::theme::kAlert;
          break;
        case desk_display::AircraftNotable::Military:
          markColor = desk_display::theme::kMilitary;
          break;
        case desk_display::AircraftNotable::Interesting:
          markColor = desk_display::theme::kAccent;
          break;
        case desk_display::AircraftNotable::None:
        default:
          break;
      }
    }

    BlipHitTarget& hit = g_hit_targets[i];
    hit.valid = true;
    hit.markX = originX + static_cast<float>(bx);
    hit.markY = originY + static_cast<float>(by);
    hit.hasTag = false;

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

      char tagLine2[24];
      char tagLine3[28];
      tagLine2[0] = '\0';
      tagLine3[0] = '\0';
      const auto style = selected ? desk_display::RadarTagStyle::Full
                                  : desk_display::RadarTagStyle::Dense;
      desk_display::formatRadarTagLine2(tagLine2, sizeof(tagLine2), b.aircraft,
                                        style);
      if (selected) {
        desk_display::formatRadarTagLine3(tagLine3, sizeof(tagLine3),
                                          b.aircraft.type, b.aircraft.squawk,
                                          b.notable);
      }

      // Mirror draw_blip_tag placement for hit boxes.
      constexpr lv_coord_t kLeaderLen = 18;
      const lv_coord_t kTagMargin = selected ? 48 : 36;
      const float tdx = static_cast<float>(cx - bx);
      const float tdy = static_cast<float>(cy - by);
      const float tdist = std::sqrt(tdx * tdx + tdy * tdy);
      lv_coord_t leaderDx = 16;
      lv_coord_t leaderDy = -16;
      if (tdist > 1.0f) {
        const bool nearRim =
            bx < kTagMargin || by < kTagMargin ||
            bx > g_disc_px - kTagMargin || by > g_disc_px - kTagMargin;
        if (nearRim) {
          leaderDx = static_cast<lv_coord_t>((tdx / tdist) * kLeaderLen);
          leaderDy = static_cast<lv_coord_t>((tdy / tdist) * kLeaderLen);
        }
      }
      const lv_coord_t tagX = bx + leaderDx;
      const lv_coord_t tagY = by + leaderDy;
      const lv_coord_t line1Y = tagY - 12;
      const lv_coord_t line3Y = selected ? tagY + 12 : tagY;
      hit.hasTag = true;
      hit.tagX0 = originX + static_cast<float>(tagX);
      hit.tagY0 = originY + static_cast<float>(line1Y);
      hit.tagX1 = hit.tagX0 + 72.0f;
      hit.tagY1 = originY + static_cast<float>(line3Y + 12);

      draw_blip_tag(layer, i, b.aircraft.callsign, tagLine2, tagLine3, markColor,
                    selected, bx, by);
    } else {
      const float ageFrac =
          static_cast<float>(b.litAgeMs) /
          static_cast<float>(desk_display::kRadarBlipFadeMs);
      const float alpha = selected ? 1.0f : (1.0f - ageFrac * 0.88f);
      const lv_opa_t opa = static_cast<lv_opa_t>(
          clampf(alpha * 255.0f, selected ? 255.0f : 20.0f, 255.0f));

      lv_coord_t dotPx = kDotPx;
      if (selected) {
        dotPx = kDotPx + 2;
      } else if (b.notable == desk_display::AircraftNotable::Emergency) {
        dotPx = kDotPx + 2;
      }

      lv_obj_t* dot = lv_obj_create(layer);
      lv_obj_set_size(dot, dotPx, dotPx);
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
  }
}

void clear_animate_cache() {
  g_parent = nullptr;
  g_hdr = nullptr;
  g_disc = nullptr;
  g_sweep_line = nullptr;
  g_static_layer = nullptr;
  g_blips_layer = nullptr;
  g_built = false;
  g_disc_px = 0;
  g_plot_radius_px = 0.0f;
  clear_hit_targets();
  g_cached_range = -1.0f;
  g_cached_center_lat = 0.0;
  g_cached_center_lon = 0.0;
  g_cached_static_count = 0;
  g_cached_ring_count = 0;
  g_cached_highway_count = 0;
  g_cached_has_static_sel = false;
  g_cached_selected_static = 0;
  g_airspace_seg_used = 0;
  g_highway_seg_used = 0;
  for (int i = 0; i < kTrailSlices; ++i) {
    g_trail_rays[i] = nullptr;
  }
}

}  // namespace

void radar_lvgl_invalidate() { clear_animate_cache(); }

bool radar_lvgl_hit_static(lv_obj_t* parent, const desk_display::RadarView& v,
                           lv_coord_t absX, lv_coord_t absY, std::size_t* outIndex) {
  if (!outIndex || !v.staticMarks || v.staticMarkCount == 0) {
    return false;
  }

  float cx = 0.0f;
  float cy = 0.0f;
  float scale = 0.0f;
  if (!disc_hit_geometry(parent, v, &cx, &cy, &scale)) {
    return false;
  }

  const float rx = static_cast<float>(absX) - cx;
  const float ry = static_cast<float>(absY) - cy;
  const float hitR2 = kStaticHitRadiusPx * kStaticHitRadiusPx;

  std::size_t nearest = 0;
  float nearestDistSq = -1.0f;
  for (std::size_t i = 0; i < v.staticMarkCount; ++i) {
    const auto& mark = v.staticMarks[i];
    const float dx = mark.offsetXMi * scale - rx;
    const float dy = -mark.offsetYMi * scale - ry;
    const float distSq = dx * dx + dy * dy;
    if (nearestDistSq < 0.0f || distSq < nearestDistSq) {
      nearestDistSq = distSq;
      nearest = i;
    }
  }

  if (nearestDistSq < 0.0f || nearestDistSq > hitR2) {
    return false;
  }
  *outIndex = nearest;
  return true;
}

bool radar_lvgl_hit_blip(lv_obj_t* parent, const desk_display::RadarView& v,
                         lv_coord_t absX, lv_coord_t absY, std::size_t* outIndex) {
  if (!outIndex || !parent || parent != g_parent || !g_built || !v.blips) {
    return false;
  }
  (void)v;

  const float px = static_cast<float>(absX);
  const float py = static_cast<float>(absY);

  constexpr float kMarkR = 20.0f;
  const float markR2 = kMarkR * kMarkR;

  std::size_t bestTag = 0;
  float bestTagDistSq = -1.0f;
  std::size_t bestMark = 0;
  float bestMarkDistSq = -1.0f;

  for (int i = 0; i < kMaxDots; ++i) {
    const BlipHitTarget& hit = g_hit_targets[i];
    if (!hit.valid) {
      continue;
    }

    if (hit.hasTag && px >= hit.tagX0 && px < hit.tagX1 && py >= hit.tagY0 &&
        py < hit.tagY1) {
      const float tcx = (hit.tagX0 + hit.tagX1) * 0.5f;
      const float tcy = (hit.tagY0 + hit.tagY1) * 0.5f;
      const float tdx = tcx - px;
      const float tdy = tcy - py;
      const float tagDistSq = tdx * tdx + tdy * tdy;
      if (bestTagDistSq < 0.0f || tagDistSq < bestTagDistSq) {
        bestTagDistSq = tagDistSq;
        bestTag = static_cast<std::size_t>(i);
      }
    }

    const float mdx = hit.markX - px;
    const float mdy = hit.markY - py;
    const float markDistSq = mdx * mdx + mdy * mdy;
    if (markDistSq <= markR2 &&
        (bestMarkDistSq < 0.0f || markDistSq < bestMarkDistSq)) {
      bestMarkDistSq = markDistSq;
      bestMark = static_cast<std::size_t>(i);
    }
  }

  if (bestTagDistSq >= 0.0f) {
    *outIndex = bestTag;
    return true;
  }
  if (bestMarkDistSq >= 0.0f) {
    *outIndex = bestMark;
    return true;
  }
  return false;
}

bool radar_lvgl_animate_classic(lv_obj_t* parent,
                                const desk_display::RadarView& v) {
  if (!parent || parent != g_parent || !g_built || !g_disc || !g_static_layer ||
      !g_blips_layer || !g_sweep_line) {
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

  if (overlay_needs_rebuild(v)) {
    lv_obj_clean(g_static_layer);
    build_static_overlay(g_static_layer, v);
  }

  lv_obj_clean(g_blips_layer);
  build_traffic(g_blips_layer, v);
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

  g_static_layer = lv_obj_create(g_disc);
  lv_obj_set_size(g_static_layer, g_disc_px, g_disc_px);
  lv_obj_set_pos(g_static_layer, 0, 0);
  lv_obj_set_style_bg_opa(g_static_layer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_static_layer, 0, 0);
  lv_obj_set_style_pad_all(g_static_layer, 0, 0);
  lv_obj_clear_flag(g_static_layer, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(g_static_layer, LV_OBJ_FLAG_CLICKABLE);
  build_static_overlay(g_static_layer, v);

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
}

}  // namespace desk_ui
