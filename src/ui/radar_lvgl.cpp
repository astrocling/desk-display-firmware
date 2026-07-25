#include "radar_lvgl.hpp"

#include "desk_display/aircraft_notable.hpp"
#include "desk_display/map_context.hpp"
#include "desk_display/radar.hpp"
#include "desk_display/radar_format.hpp"
#include "desk_display/radar_settings.hpp"
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

constexpr uint32_t kSettingsPanelBg = 0x0B0F14;
constexpr uint32_t kSettingsChipBorder = 0x006900;
constexpr uint32_t kSettingsChipOn = 0x00FF00;
constexpr lv_coord_t kSettingsPad = 16;
constexpr lv_coord_t kSettingsChipH = 28;
constexpr lv_coord_t kSettingsChipGap = 8;
constexpr lv_coord_t kSettingsSectionGap = 12;

enum class SettingsHitKind : uint8_t {
  Done,
  DeclutterTarget,
  DeclutterCallsign,
  DeclutterTag,
  MapAirports,
  MapAirspace,
  MapRoads,
  DemoToggle,
};

struct SettingsHitRect {
  bool valid;
  float x0;
  float y0;
  float x1;
  float y1;
  SettingsHitKind kind;
};

struct SettingsHitPending {
  lv_obj_t* obj;
  SettingsHitKind kind;
};

constexpr int kMaxSettingsHits = 12;
SettingsHitRect g_settings_hits[kMaxSettingsHits];
int g_settings_hit_count = 0;
SettingsHitPending g_settings_hit_pending[kMaxSettingsHits];
int g_settings_hit_pending_count = 0;

lv_obj_t* g_settings_root = nullptr;
desk_display::RadarDeclutterMode g_cached_declutter =
    desk_display::RadarDeclutterMode::TargetTag;
bool g_cached_show_airports = true;
bool g_cached_show_airspace = true;
bool g_cached_show_roads = true;
bool g_cached_demo_mode = false;
bool g_cached_settings_open = false;

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

void clear_settings_hits() {
  g_settings_hit_count = 0;
  for (int i = 0; i < kMaxSettingsHits; ++i) {
    g_settings_hits[i].valid = false;
  }
}

void register_settings_hit(lv_obj_t* obj, SettingsHitKind kind) {
  if (!obj || g_settings_hit_pending_count >= kMaxSettingsHits) {
    return;
  }
  g_settings_hit_pending[g_settings_hit_pending_count++] = {obj, kind};
}

void resolve_settings_hits() {
  clear_settings_hits();
  for (int i = 0; i < g_settings_hit_pending_count; ++i) {
    const SettingsHitPending& pending = g_settings_hit_pending[i];
    if (!pending.obj || g_settings_hit_count >= kMaxSettingsHits) {
      continue;
    }
    lv_area_t area{};
    lv_obj_get_coords(pending.obj, &area);
    const desk_display::RadarSettingsHitRect rect =
        desk_display::radarSettingsHitRectFromArea(area.x1, area.y1, area.x2,
                                                   area.y2);
    SettingsHitRect& hit = g_settings_hits[g_settings_hit_count++];
    hit.valid = true;
    hit.x0 = rect.x0;
    hit.y0 = rect.y0;
    hit.x1 = rect.x1;
    hit.y1 = rect.y1;
    hit.kind = pending.kind;
  }
  g_settings_hit_pending_count = 0;
}

bool point_in_settings_hit(float px, float py, const SettingsHitRect& hit) {
  return hit.valid && px >= hit.x0 && px < hit.x1 && py >= hit.y0 && py < hit.y1;
}

void cache_settings_overlay_state(const desk_display::RadarView& v) {
  g_cached_settings_open = v.settingsOpen;
  g_cached_declutter = v.settings.declutter;
  g_cached_show_airports = v.settings.showAirports;
  g_cached_show_airspace = v.settings.showAirspace;
  g_cached_show_roads = v.settings.showRoads;
  g_cached_demo_mode = v.settings.demoMode;
}

bool settings_overlay_needs_rebuild(const desk_display::RadarView& v) {
  return v.settings.declutter != g_cached_declutter ||
         v.settings.showAirports != g_cached_show_airports ||
         v.settings.showAirspace != g_cached_show_airspace ||
         v.settings.showRoads != g_cached_show_roads ||
         v.settings.demoMode != g_cached_demo_mode;
}

lv_obj_t* make_section_label(lv_obj_t* parent, const char* text, lv_coord_t y) {
  lv_obj_t* lab = lv_label_create(parent);
  lv_label_set_text(lab, text);
  lv_obj_set_style_text_font(lab, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(lab, rgb(desk_display::theme::kDim), 0);
  lv_obj_set_pos(lab, kSettingsPad, y);
  lv_obj_clear_flag(lab, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(lab, LV_OBJ_FLAG_SCROLLABLE);
  return lab;
}

lv_obj_t* make_chip(lv_obj_t* parent, const char* text, lv_coord_t x, lv_coord_t y,
                    lv_coord_t w, bool active, bool greenActive) {
  lv_obj_t* chip = lv_obj_create(parent);
  lv_obj_set_size(chip, w, kSettingsChipH);
  lv_obj_set_pos(chip, x, y);
  lv_obj_set_style_radius(chip, 4, 0);
  lv_obj_set_style_pad_all(chip, 0, 0);
  lv_obj_set_style_border_width(chip, 1, 0);
  lv_obj_clear_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(chip, LV_OBJ_FLAG_CLICKABLE);

  const uint32_t onColor = greenActive ? kSettingsChipOn : kSettingsChipBorder;
  if (active) {
    lv_obj_set_style_bg_color(chip, rgb(onColor), 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(chip, rgb(onColor), 0);
  } else {
    lv_obj_set_style_bg_opa(chip, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(chip, rgb(desk_display::theme::kDim), 0);
  }

  lv_obj_t* lab = lv_label_create(chip);
  lv_label_set_text(lab, text);
  lv_obj_set_style_text_font(lab, &lv_font_montserrat_12, 0);
  if (active) {
    lv_obj_set_style_text_color(lab, rgb(0x0B0F14), 0);
  } else {
    lv_obj_set_style_text_color(lab, rgb(desk_display::theme::kDim), 0);
  }
  lv_obj_center(lab);
  lv_obj_clear_flag(lab, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(lab, LV_OBJ_FLAG_SCROLLABLE);
  return chip;
}

void drop_settings_overlay_cache() {
  g_settings_root = nullptr;
  g_settings_hit_pending_count = 0;
  clear_settings_hits();
  g_cached_settings_open = false;
}

void destroy_settings_overlay() {
  if (g_settings_root) {
    lv_obj_del(g_settings_root);
  }
  drop_settings_overlay_cache();
}

void build_settings_overlay(lv_obj_t* parent, const desk_display::RadarView& v) {
  g_settings_hit_pending_count = 0;
  clear_settings_hits();

  const lv_coord_t pw = lv_obj_get_content_width(parent);
  const lv_coord_t ph = lv_obj_get_content_height(parent);

  g_settings_root = lv_obj_create(parent);
  lv_obj_set_size(g_settings_root, pw, ph);
  lv_obj_set_pos(g_settings_root, 0, 0);
  lv_obj_set_style_bg_color(g_settings_root, rgb(kSettingsPanelBg), 0);
  lv_obj_set_style_bg_opa(g_settings_root, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_settings_root, 0, 0);
  lv_obj_set_style_pad_all(g_settings_root, 0, 0);
  lv_obj_clear_flag(g_settings_root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(g_settings_root, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_move_foreground(g_settings_root);

  lv_obj_t* title = lv_label_create(g_settings_root);
  lv_label_set_text(title, "Radar Settings");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(title, rgb(0xFFFFFF), 0);
  lv_obj_set_pos(title, kSettingsPad, kSettingsPad);
  lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(title, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* done = lv_obj_create(g_settings_root);
  lv_obj_set_size(done, 56, kSettingsChipH);
  lv_obj_align(done, LV_ALIGN_TOP_RIGHT, -kSettingsPad, kSettingsPad);
  lv_obj_set_style_radius(done, 4, 0);
  lv_obj_set_style_bg_opa(done, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(done, 1, 0);
  lv_obj_set_style_border_color(done, rgb(kSettingsChipBorder), 0);
  lv_obj_set_style_pad_all(done, 0, 0);
  lv_obj_clear_flag(done, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(done, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_t* doneLab = lv_label_create(done);
  lv_label_set_text(doneLab, "Done");
  lv_obj_set_style_text_font(doneLab, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(doneLab, rgb(kSettingsChipOn), 0);
  lv_obj_center(doneLab);
  lv_obj_clear_flag(doneLab, LV_OBJ_FLAG_CLICKABLE);
  register_settings_hit(done, SettingsHitKind::Done);

  lv_coord_t y = kSettingsPad + 36;
  make_section_label(g_settings_root, "DECLUTTER", y);
  y += 20;

  const lv_coord_t chipW =
      (pw - kSettingsPad * 2 - kSettingsChipGap * 2) / 3;
  lv_coord_t chipX = kSettingsPad;

  const bool declutterTarget =
      v.settings.declutter == desk_display::RadarDeclutterMode::TargetOnly;
  const bool declutterCallsign =
      v.settings.declutter == desk_display::RadarDeclutterMode::TargetCallsign;
  const bool declutterTag =
      v.settings.declutter == desk_display::RadarDeclutterMode::TargetTag;

  lv_obj_t* chipTarget = make_chip(g_settings_root, "Target", chipX, y, chipW,
                                   declutterTarget, false);
  register_settings_hit(chipTarget, SettingsHitKind::DeclutterTarget);
  chipX += chipW + kSettingsChipGap;

  lv_obj_t* chipCallsign = make_chip(g_settings_root, "Callsign", chipX, y, chipW,
                                     declutterCallsign, false);
  register_settings_hit(chipCallsign, SettingsHitKind::DeclutterCallsign);
  chipX += chipW + kSettingsChipGap;

  lv_obj_t* chipTag = make_chip(g_settings_root, "Tag", chipX, y, chipW,
                                declutterTag, false);
  register_settings_hit(chipTag, SettingsHitKind::DeclutterTag);

  y += kSettingsChipH + kSettingsSectionGap;
  make_section_label(g_settings_root, "MAP CLUTTER", y);
  y += 20;
  chipX = kSettingsPad;

  lv_obj_t* chipAirports =
      make_chip(g_settings_root, "Airports", chipX, y, chipW,
                v.settings.showAirports, true);
  register_settings_hit(chipAirports, SettingsHitKind::MapAirports);
  chipX += chipW + kSettingsChipGap;

  lv_obj_t* chipAirspace =
      make_chip(g_settings_root, "Airspace", chipX, y, chipW,
                v.settings.showAirspace, true);
  register_settings_hit(chipAirspace, SettingsHitKind::MapAirspace);
  chipX += chipW + kSettingsChipGap;

  lv_obj_t* chipRoads = make_chip(g_settings_root, "Roads", chipX, y, chipW,
                                  v.settings.showRoads, true);
  register_settings_hit(chipRoads, SettingsHitKind::MapRoads);

  y += kSettingsChipH + kSettingsSectionGap;
  lv_obj_t* demoLabel = lv_label_create(g_settings_root);
  lv_label_set_text(demoLabel, "Demo Mode");
  lv_obj_set_style_text_font(demoLabel, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(demoLabel, rgb(0xFFFFFF), 0);
  lv_obj_set_pos(demoLabel, kSettingsPad, y + 6);
  lv_obj_clear_flag(demoLabel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(demoLabel, LV_OBJ_FLAG_SCROLLABLE);

  const lv_coord_t toggleW = 88;
  lv_obj_t* demoToggle = lv_obj_create(g_settings_root);
  lv_obj_set_size(demoToggle, toggleW, kSettingsChipH);
  lv_obj_align(demoToggle, LV_ALIGN_TOP_RIGHT, -kSettingsPad, y);
  lv_obj_set_style_radius(demoToggle, 4, 0);
  lv_obj_set_style_bg_opa(demoToggle, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(demoToggle, 1, 0);
  lv_obj_set_style_border_color(demoToggle, rgb(desk_display::theme::kDim), 0);
  lv_obj_set_style_pad_all(demoToggle, 0, 0);
  lv_obj_clear_flag(demoToggle, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(demoToggle, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* offLab = lv_label_create(demoToggle);
  lv_label_set_text(offLab, "Off");
  lv_obj_set_style_text_font(offLab, &lv_font_montserrat_12, 0);
  lv_obj_align(offLab, LV_ALIGN_LEFT_MID, 10, 0);
  lv_obj_clear_flag(offLab, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* onLab = lv_label_create(demoToggle);
  lv_label_set_text(onLab, "On");
  lv_obj_set_style_text_font(onLab, &lv_font_montserrat_12, 0);
  lv_obj_align(onLab, LV_ALIGN_RIGHT_MID, -12, 0);
  lv_obj_clear_flag(onLab, LV_OBJ_FLAG_CLICKABLE);

  if (v.settings.demoMode) {
    lv_obj_set_style_text_color(offLab, rgb(desk_display::theme::kDim), 0);
    lv_obj_set_style_text_color(onLab, rgb(kSettingsChipOn), 0);
    lv_obj_set_style_bg_color(demoToggle, rgb(kSettingsChipBorder), 0);
    lv_obj_set_style_bg_opa(demoToggle, LV_OPA_40, 0);
  } else {
    lv_obj_set_style_text_color(offLab, rgb(kSettingsChipOn), 0);
    lv_obj_set_style_text_color(onLab, rgb(desk_display::theme::kDim), 0);
  }
  register_settings_hit(demoToggle, SettingsHitKind::DemoToggle);

  lv_obj_update_layout(g_settings_root);
  resolve_settings_hits();
  cache_settings_overlay_state(v);
}

void sync_settings_overlay(lv_obj_t* parent, const desk_display::RadarView& v) {
  if (!v.settingsOpen) {
    if (g_settings_root) {
      destroy_settings_overlay();
    }
    return;
  }

  if (!g_settings_root) {
    build_settings_overlay(parent, v);
    return;
  }

  if (settings_overlay_needs_rebuild(v)) {
    lv_obj_del(g_settings_root);
    g_settings_root = nullptr;
    build_settings_overlay(parent, v);
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
  std::snprintf(hdr, hdrLen, "%.0f mi - %zu%s%s",
                static_cast<double>(v.rangeMiles), v.blipCount,
                show_vectors(v.rangeMiles) ? " - vec" : "",
                v.settings.demoMode ? " - DEMO" : "");
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
                               lv_coord_t x1, lv_coord_t y1, uint32_t color,
                               bool dashed) {
  if (!layer || g_airspace_seg_used >= kMaxAirspaceSegs) {
    return nullptr;
  }
  g_airspace_seg_pts[g_airspace_seg_used][0] = {x0, y0};
  g_airspace_seg_pts[g_airspace_seg_used][1] = {x1, y1};
  lv_obj_t* line = lv_line_create(layer);
  if (!line) {
    // LVGL heap exhausted — stop creating segments rather than deref null.
    return nullptr;
  }
  lv_obj_set_pos(line, 0, 0);
  lv_line_set_points(line, g_airspace_seg_pts[g_airspace_seg_used], 2);
  lv_obj_set_style_line_width(line, 1, 0);
  lv_obj_set_style_line_color(line, rgb(color), 0);
  lv_obj_set_style_line_rounded(line, false, 0);
  // One LVGL line per edge (with style dash). Manual dash sub-segments used to
  // spawn hundreds of objects per ring and SIGSEGV inside lv_line_create.
  if (dashed) {
    lv_obj_set_style_line_dash_width(line, kAirspaceDashWidth, 0);
    lv_obj_set_style_line_dash_gap(line, kAirspaceDashGap, 0);
  }
  lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
  ++g_airspace_seg_used;
  return line;
}

void draw_airspace_edge(lv_obj_t* layer, lv_coord_t x0, lv_coord_t y0, lv_coord_t x1,
                        lv_coord_t y1, uint32_t color, bool dashed) {
  add_airspace_segment(layer, x0, y0, x1, y1, color, dashed);
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
      if (!line) {
        return;
      }
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
                   const char* tagLine2, const char* tagLine3, const char* tagLine4,
                   uint32_t callsignColor, bool selected, bool drawCallsign,
                   bool drawLine2, lv_coord_t bx, lv_coord_t by) {
  // Place the tag toward disc center when the blip is near the rim so the
  // round clip doesn't swallow callsign / alt / type lines.
  constexpr lv_coord_t kLeaderLen = 18;
  const bool hasLine4 = selected && tagLine4 && tagLine4[0] != '\0';
  const lv_coord_t kTagMargin = hasLine4 ? 60 : (selected ? 48 : 36);
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
  const lv_coord_t line4Y = tagY + 24;

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

  if (drawCallsign) {
    lv_obj_t* tag = lv_label_create(layer);
    lv_label_set_text(tag, (callsign && callsign[0]) ? callsign : "?");
    lv_obj_set_style_text_font(tag, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(tag, rgb(callsignColor), 0);
    lv_obj_set_style_text_opa(tag, opa, 0);
    lv_obj_set_pos(tag, tagX, line1Y);
  }

  if (drawLine2 && tagLine2 && tagLine2[0] != '\0') {
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

  if (hasLine4) {
    lv_obj_t* tag4 = lv_label_create(layer);
    lv_label_set_text(tag4, tagLine4);
    lv_obj_set_style_text_font(tag4, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(tag4, rgb(desk_display::theme::kAccent), 0);
    lv_obj_set_style_text_opa(tag4, opa, 0);
    lv_obj_set_pos(tag4, tagX, line4Y);
  }
}

/**
 * Traffic layer: ≤25 mi → stars + vectors + declutter-aware tags;
 * above that → dense dots only (still selectable). Unselected label density
 * follows settings; selected always gets the full tag (incl. line4 when set).
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

      using desk_display::RadarUnselectedLabel;
      const auto label =
          selected ? RadarUnselectedLabel::DenseTag
                   : desk_display::radarUnselectedLabel(v.settings.declutter);

      char tagLine2[24]{};
      char tagLine3[28]{};
      char tagLine4[8]{};
      if (selected) {
        desk_display::formatRadarTagLine2(tagLine2, sizeof(tagLine2), b.aircraft,
                                          desk_display::RadarTagStyle::Full);
        desk_display::formatRadarTagLine3(tagLine3, sizeof(tagLine3),
                                          b.aircraft.type, b.aircraft.squawk,
                                          b.notable);
        desk_display::formatRadarTagLine4(tagLine4, sizeof(tagLine4), nullptr);
      } else if (label == RadarUnselectedLabel::DenseTag) {
        desk_display::formatRadarTagLine2(tagLine2, sizeof(tagLine2), b.aircraft,
                                          desk_display::RadarTagStyle::Dense);
      }

      if (selected || label != RadarUnselectedLabel::None) {
        const bool drawCs = true;
        const bool drawL2 =
            selected || label == RadarUnselectedLabel::DenseTag;
        const bool hasLine4 = selected && tagLine4[0] != '\0';

        // Mirror draw_blip_tag placement for hit boxes.
        constexpr lv_coord_t kLeaderLen = 18;
        const lv_coord_t kTagMargin = hasLine4 ? 60 : (selected ? 48 : 36);
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
        lv_coord_t tagBottomY = tagY;
        if (selected) {
          tagBottomY = hasLine4 ? static_cast<lv_coord_t>(tagY + 24 + 12)
                                : static_cast<lv_coord_t>(tagY + 12 + 12);
        } else if (drawL2) {
          tagBottomY = tagY + 12;
        }
        hit.hasTag = true;
        hit.tagX0 = originX + static_cast<float>(tagX);
        hit.tagY0 = originY + static_cast<float>(line1Y);
        hit.tagX1 = hit.tagX0 + 72.0f;
        hit.tagY1 = originY + static_cast<float>(tagBottomY);

        draw_blip_tag(layer, i, b.aircraft.callsign, tagLine2, tagLine3, tagLine4,
                      markColor, selected, drawCs, drawL2, bx, by);
      }
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
  drop_settings_overlay_cache();
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
  sync_settings_overlay(parent, v);
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
  sync_settings_overlay(parent, v);
}

bool radar_lvgl_settings_hit(lv_coord_t absX, lv_coord_t absY,
                             desk_display::ScreenRadar& radar) {
  if (!g_settings_root || !g_built) {
    return false;
  }

  const float px = static_cast<float>(absX);
  const float py = static_cast<float>(absY);

  for (int i = 0; i < g_settings_hit_count; ++i) {
    const SettingsHitRect& hit = g_settings_hits[i];
    if (!point_in_settings_hit(px, py, hit)) {
      continue;
    }
    switch (hit.kind) {
      case SettingsHitKind::Done:
        radar.closeSettings();
        return true;
      case SettingsHitKind::DeclutterTarget:
        radar.setDeclutterMode(desk_display::RadarDeclutterMode::TargetOnly);
        return true;
      case SettingsHitKind::DeclutterCallsign:
        radar.setDeclutterMode(desk_display::RadarDeclutterMode::TargetCallsign);
        return true;
      case SettingsHitKind::DeclutterTag:
        radar.setDeclutterMode(desk_display::RadarDeclutterMode::TargetTag);
        return true;
      case SettingsHitKind::MapAirports:
        radar.setShowAirports(!radar.settings().showAirports);
        return true;
      case SettingsHitKind::MapAirspace:
        radar.setShowAirspace(!radar.settings().showAirspace);
        return true;
      case SettingsHitKind::MapRoads:
        radar.setShowRoads(!radar.settings().showRoads);
        return true;
      case SettingsHitKind::DemoToggle:
        radar.setDemoMode(!radar.settings().demoMode);
        return true;
    }
  }
  return false;
}

}  // namespace desk_ui
