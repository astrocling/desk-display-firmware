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
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#if defined(BOARD_HAS_PSRAM)
#include <esp_heap_caps.h>
#endif

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
/**
 * Margin from the square parent edge so content stays inside the circular
 * bezel. For a 360px disc, ~52px approximates the inscribed-square inset
 * ((D - D/√2) / 2) and keeps top/side chips from clipping the ring.
 */
constexpr lv_coord_t kSettingsRingInset = 52;
constexpr lv_coord_t kSettingsChipH = 26;
constexpr lv_coord_t kSettingsChipGap = 6;
constexpr lv_coord_t kSettingsSectionGap = 10;

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
constexpr int kTrailSlices = 4;
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
// lifetime; sweep/trail buffers are disc-global, blip vectors/leaders live in
// per-callsign traffic slots so index compaction cannot retarget another line.
lv_point_t g_sweep_points[2];
lv_point_t g_trail_points[kTrailSlices][2];

/** Absolute display-pixel hit targets written during the last traffic sync. */
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

/**
 * One LVGL subtree per painted callsign. Geometry/text updates happen in
 * place; delete+recreate only when the widget *layout* changes (selection,
 * declutter, vectors↔dots, track line presence).
 */
struct TrafficSlot {
  bool active;
  bool keep;
  char callsign[desk_display::kMaxCallsign];
  uint32_t layout_sig;
  uint32_t content_sig;
  lv_obj_t* root;
  lv_obj_t* mark;
  lv_obj_t* vector;
  lv_obj_t* leader;
  lv_obj_t* tag1;
  lv_obj_t* tag2;
  lv_obj_t* tag3;
  lv_obj_t* tag4;
  lv_point_t vector_pts[2];
  lv_point_t leader_pts[2];
};
TrafficSlot g_traffic_slots[kMaxDots];

/** Blips-layer origin in absolute pixels — disc does not move during Classic. */
bool g_blips_origin_valid = false;
float g_blips_origin_x = 0.0f;
float g_blips_origin_y = 0.0f;

void clear_hit_targets() {
  for (int i = 0; i < kMaxDots; ++i) {
    g_hit_targets[i].valid = false;
  }
}

void forget_traffic_slot(TrafficSlot& slot, bool deleteRoot) {
  if (deleteRoot && slot.root != nullptr) {
    lv_obj_del(slot.root);
  }
  slot.active = false;
  slot.keep = false;
  slot.callsign[0] = '\0';
  slot.layout_sig = 0;
  slot.content_sig = 0;
  slot.root = nullptr;
  slot.mark = nullptr;
  slot.vector = nullptr;
  slot.leader = nullptr;
  slot.tag1 = nullptr;
  slot.tag2 = nullptr;
  slot.tag3 = nullptr;
  slot.tag4 = nullptr;
}

void clear_traffic_slots(bool deleteRoots) {
  for (int i = 0; i < kMaxDots; ++i) {
    forget_traffic_slot(g_traffic_slots[i], deleteRoots);
  }
  g_blips_origin_valid = false;
}

TrafficSlot* find_traffic_slot(const char* callsign) {
  if (callsign == nullptr || callsign[0] == '\0') {
    return nullptr;
  }
  for (int i = 0; i < kMaxDots; ++i) {
    if (g_traffic_slots[i].active &&
        std::strcmp(g_traffic_slots[i].callsign, callsign) == 0) {
      return &g_traffic_slots[i];
    }
  }
  return nullptr;
}

TrafficSlot* alloc_traffic_slot() {
  for (int i = 0; i < kMaxDots; ++i) {
    if (!g_traffic_slots[i].active) {
      return &g_traffic_slots[i];
    }
  }
  return nullptr;
}

bool refresh_blips_origin(lv_obj_t* layer) {
  if (g_blips_origin_valid) {
    return true;
  }
  if (!layer) {
    return false;
  }
  lv_obj_update_layout(layer);
  lv_area_t layerArea{};
  lv_obj_get_coords(layer, &layerArea);
  g_blips_origin_x = static_cast<float>(layerArea.x1);
  g_blips_origin_y = static_cast<float>(layerArea.y1);
  g_blips_origin_valid = true;
  return true;
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

lv_obj_t* make_section_label(lv_obj_t* parent, const char* text, lv_coord_t x,
                             lv_coord_t y) {
  lv_obj_t* lab = lv_label_create(parent);
  lv_label_set_text(lab, text);
  lv_obj_set_style_text_font(lab, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(lab, rgb(desk_display::theme::kDim), 0);
  lv_obj_set_pos(lab, x, y);
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
  // Keep controls inside the circular bezel (inscribed content column).
  const lv_coord_t inset =
      pw < (kSettingsRingInset * 2 + 120) ? (pw / 8) : kSettingsRingInset;
  const lv_coord_t contentX = inset;
  const lv_coord_t contentW = pw - inset * 2;
  const lv_coord_t contentY = inset;

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

  constexpr lv_coord_t kDoneW = 52;
  lv_obj_t* title = lv_label_create(g_settings_root);
  lv_label_set_text(title, "Settings");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(title, rgb(0xFFFFFF), 0);
  lv_obj_set_pos(title, contentX, contentY + 2);
  lv_obj_clear_flag(title, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(title, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* done = lv_obj_create(g_settings_root);
  lv_obj_set_size(done, kDoneW, kSettingsChipH);
  lv_obj_set_pos(done, contentX + contentW - kDoneW, contentY);
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

  lv_coord_t y = contentY + kSettingsChipH + 10;
  make_section_label(g_settings_root, "DECLUTTER", contentX, y);
  y += 18;

  const lv_coord_t chipW =
      (contentW - kSettingsChipGap * 2) / 3;
  lv_coord_t chipX = contentX;

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
  make_section_label(g_settings_root, "MAP CLUTTER", contentX, y);
  y += 18;
  chipX = contentX;

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
  lv_obj_set_pos(demoLabel, contentX, y + 4);
  lv_obj_clear_flag(demoLabel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(demoLabel, LV_OBJ_FLAG_SCROLLABLE);

  const lv_coord_t toggleW = 72;
  lv_obj_t* demoToggle = lv_obj_create(g_settings_root);
  lv_obj_set_size(demoToggle, toggleW, kSettingsChipH);
  lv_obj_set_pos(demoToggle, contentX + contentW - toggleW, y);
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
  lv_obj_align(offLab, LV_ALIGN_LEFT_MID, 8, 0);
  lv_obj_clear_flag(offLab, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* onLab = lv_label_create(demoToggle);
  lv_label_set_text(onLab, "On");
  lv_obj_set_style_text_font(onLab, &lv_font_montserrat_12, 0);
  lv_obj_align(onLab, LV_ALIGN_RIGHT_MID, -8, 0);
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

// Traffic layer: rebuild on paint/bind/selection/range — not every sweep tick.
uint32_t g_cached_traffic_sig = 0;
bool g_traffic_cache_valid = false;
uint32_t g_cached_blip_ages[kMaxDots] = {};
std::size_t g_cached_age_count = 0;

// Flattened map overlay (one img) — avoids restroking ~1000 lv_line segs/frame.
lv_img_dsc_t g_static_snap_dsc{};
void* g_static_snap_buf = nullptr;
uint32_t g_static_snap_cap = 0;
lv_obj_t* g_static_snap_img = nullptr;

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

void mix_u32(uint32_t& h, uint32_t x) {
  h ^= x;
  h *= 16777619u;
}

/** Structural traffic fingerprint — ignores continuous phosphor aging. */
uint32_t traffic_signature(const desk_display::RadarView& v) {
  uint32_t h = 2166136261u;
  mix_u32(h, static_cast<uint32_t>(v.blipCount));
  mix_u32(h, v.hasSelection ? 1u : 0u);
  mix_u32(h, static_cast<uint32_t>(v.selectedIndex));
  mix_u32(h, static_cast<uint32_t>(v.rangeMiles * 10.0f));
  mix_u32(h, static_cast<uint32_t>(v.settings.declutter));
  mix_u32(h, v.settings.demoMode ? 1u : 0u);

  const std::size_t count =
      v.blipCount < static_cast<std::size_t>(kMaxDots) ? v.blipCount
                                                       : kMaxDots;
  for (std::size_t i = 0; i < count; ++i) {
    const desk_display::RadarBlip& b = v.blips[i];
    for (const char* p = b.aircraft.callsign; *p != '\0'; ++p) {
      mix_u32(h, static_cast<uint8_t>(*p));
    }
    mix_u32(h, static_cast<uint32_t>(
                   static_cast<int32_t>(b.offsetXMi * 50.0f) + 500000));
    mix_u32(h, static_cast<uint32_t>(
                   static_cast<int32_t>(b.offsetYMi * 50.0f) + 500000));
    mix_u32(h, static_cast<uint32_t>(b.notable));
    mix_u32(h, b.litAgeMs >= desk_display::kRadarBlipFadeMs ? 1u : 0u);
    if (b.aircraft.hasTrack) {
      mix_u32(h, static_cast<uint32_t>(b.aircraft.trackDeg));
    }
    if (b.aircraft.hasSpeed) {
      mix_u32(h, static_cast<uint32_t>(b.aircraft.speedKt));
    }
    if (b.aircraft.hasAlt) {
      mix_u32(h, static_cast<uint32_t>(b.aircraft.altFt));
    }
  }
  return h;
}

bool traffic_paint_detected(const desk_display::RadarView& v) {
  const std::size_t count =
      v.blipCount < static_cast<std::size_t>(kMaxDots) ? v.blipCount
                                                       : kMaxDots;
  for (std::size_t i = 0; i < count && i < g_cached_age_count; ++i) {
    if (v.blips[i].litAgeMs < g_cached_blip_ages[i]) {
      return true;
    }
  }
  return false;
}

void remember_blip_ages(const desk_display::RadarView& v) {
  const std::size_t count =
      v.blipCount < static_cast<std::size_t>(kMaxDots) ? v.blipCount
                                                       : kMaxDots;
  for (std::size_t i = 0; i < count; ++i) {
    g_cached_blip_ages[i] = v.blips[i].litAgeMs;
  }
  g_cached_age_count = count;
}

void mark_traffic_built(const desk_display::RadarView& v) {
  g_cached_traffic_sig = traffic_signature(v);
  remember_blip_ages(v);
  g_traffic_cache_valid = true;
}

bool traffic_needs_rebuild(const desk_display::RadarView& v) {
  if (!g_traffic_cache_valid) {
    return true;
  }
  if (traffic_signature(v) != g_cached_traffic_sig) {
    return true;
  }
  return traffic_paint_detected(v);
}

void* snap_alloc(std::size_t bytes) {
#if defined(BOARD_HAS_PSRAM)
  void* p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (p == nullptr) {
    p = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
  }
  return p;
#else
  return std::malloc(bytes);
#endif
}

void snap_free(void* p) {
  if (p == nullptr) {
    return;
  }
#if defined(BOARD_HAS_PSRAM)
  heap_caps_free(p);
#else
  std::free(p);
#endif
}

void release_static_snapshot_buf() {
  g_static_snap_img = nullptr;
  if (g_static_snap_buf != nullptr) {
    snap_free(g_static_snap_buf);
    g_static_snap_buf = nullptr;
  }
  g_static_snap_cap = 0;
  std::memset(&g_static_snap_dsc, 0, sizeof(g_static_snap_dsc));
}

/**
 * Replace hundreds of airspace/highway line objects with one image so each
 * Classic sweep invalidate is a blit instead of restroking the map.
 * Falls back to leaving the line children if snapshot fails (sim heap, etc.).
 */
bool flatten_static_overlay() {
  if (!g_static_layer) {
    return false;
  }

  lv_obj_update_layout(g_static_layer);
  const lv_img_cf_t cf = LV_IMG_CF_TRUE_COLOR_ALPHA;
  const uint32_t need = lv_snapshot_buf_size_needed(g_static_layer, cf);
  if (need == 0) {
    return false;
  }

  if (need > g_static_snap_cap || g_static_snap_buf == nullptr) {
    void* fresh = snap_alloc(need);
    if (fresh == nullptr) {
      return false;
    }
    snap_free(g_static_snap_buf);
    g_static_snap_buf = fresh;
    g_static_snap_cap = need;
  }

  if (lv_snapshot_take_to_buf(g_static_layer, cf, &g_static_snap_dsc,
                              g_static_snap_buf, g_static_snap_cap) != LV_RES_OK) {
    return false;
  }

  lv_obj_clean(g_static_layer);
  g_airspace_seg_used = 0;
  g_highway_seg_used = 0;
  g_static_snap_img = nullptr;

  g_static_snap_img = lv_img_create(g_static_layer);
  if (!g_static_snap_img) {
    return false;
  }
  lv_img_set_src(g_static_snap_img, &g_static_snap_dsc);
  lv_obj_set_pos(g_static_snap_img, 0, 0);
  lv_obj_clear_flag(g_static_snap_img, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(g_static_snap_img, LV_OBJ_FLAG_SCROLLABLE);
  return true;
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
    // Live Class D rings often repeat the first vertex as the last (closed
    // GeoJSON). Skip that zero-length wrap edge.
    const bool closedDuplicate =
        ring.pointCount >= 3 &&
        ring.offsetXMi[0] == ring.offsetXMi[ring.pointCount - 1] &&
        ring.offsetYMi[0] == ring.offsetYMi[ring.pointCount - 1];
    const uint8_t edgeCount =
        closedDuplicate ? static_cast<uint8_t>(ring.pointCount - 1)
                        : ring.pointCount;
    for (uint8_t i = 0; i < edgeCount; ++i) {
      const uint8_t j = closedDuplicate
                            ? static_cast<uint8_t>(i + 1)
                            : static_cast<uint8_t>((i + 1) % ring.pointCount);
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

void rebuild_static_overlay(lv_obj_t* layer, const desk_display::RadarView& v) {
  lv_obj_clean(layer);
  g_static_snap_img = nullptr;
  build_static_overlay(layer, v);
  flatten_static_overlay();
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

void tag_leader_offset(lv_coord_t bx, lv_coord_t by, bool selected, bool hasLine4,
                       lv_coord_t* outDx, lv_coord_t* outDy) {
  constexpr lv_coord_t kLeaderLen = 18;
  const lv_coord_t kTagMargin = hasLine4 ? 60 : (selected ? 48 : 36);
  const lv_coord_t cx = g_disc_px / 2;
  const lv_coord_t cy = g_disc_px / 2;
  const float dx = static_cast<float>(cx - bx);
  const float dy = static_cast<float>(cy - by);
  const float dist = std::sqrt(dx * dx + dy * dy);
  lv_coord_t leaderDx = 16;
  lv_coord_t leaderDy = -16;
  if (dist > 1.0f) {
    const bool nearRim = bx < kTagMargin || by < kTagMargin ||
                         bx > g_disc_px - kTagMargin ||
                         by > g_disc_px - kTagMargin;
    if (nearRim) {
      leaderDx = static_cast<lv_coord_t>((dx / dist) * kLeaderLen);
      leaderDy = static_cast<lv_coord_t>((dy / dist) * kLeaderLen);
    }
  }
  *outDx = leaderDx;
  *outDy = leaderDy;
}

void compute_tag_lines(const desk_display::RadarView& v, std::size_t i,
                       bool selected, desk_display::RadarUnselectedLabel label,
                       char* tagLine2, std::size_t tagLine2Len, char* tagLine3,
                       std::size_t tagLine3Len, char* tagLine4,
                       std::size_t tagLine4Len) {
  const desk_display::RadarBlip& b = v.blips[i];
  tagLine2[0] = '\0';
  tagLine3[0] = '\0';
  tagLine4[0] = '\0';
  if (selected) {
    desk_display::formatRadarTagLine2(tagLine2, tagLine2Len, b.aircraft,
                                      desk_display::RadarTagStyle::Full);
    desk_display::formatRadarTagLine3(tagLine3, tagLine3Len, b.aircraft.type,
                                      b.aircraft.squawk, b.notable);
    desk_display::formatRadarTagLine4(tagLine4, tagLine4Len, nullptr);
  } else if (label == desk_display::RadarUnselectedLabel::DenseTag) {
    desk_display::formatRadarTagLine2(tagLine2, tagLine2Len, b.aircraft,
                                      desk_display::RadarTagStyle::Dense);
  }
}

uint32_t blip_mark_color(const desk_display::RadarBlip& b, bool selected) {
  if (selected) {
    return kSelectedColor;
  }
  switch (b.notable) {
    case desk_display::AircraftNotable::Emergency:
      return desk_display::theme::kAlert;
    case desk_display::AircraftNotable::Military:
      return desk_display::theme::kMilitary;
    case desk_display::AircraftNotable::Interesting:
      return desk_display::theme::kAccent;
    case desk_display::AircraftNotable::None:
    default:
      return kDotColor;
  }
}

/**
 * Structural fingerprint — recreate widgets only when this changes.
 * Position / track / alt / speed values are updated in place.
 */
uint32_t blip_layout_sig(const desk_display::RadarView& v, std::size_t i,
                         bool vectors) {
  const desk_display::RadarBlip& b = v.blips[i];
  const bool selected = v.hasSelection && v.selectedIndex == i;
  using desk_display::RadarUnselectedLabel;
  const auto label =
      selected ? RadarUnselectedLabel::DenseTag
               : desk_display::radarUnselectedLabel(v.settings.declutter);
  const bool drawTag =
      vectors && (selected || label != RadarUnselectedLabel::None);
  const bool drawL2 = selected || label == RadarUnselectedLabel::DenseTag;

  uint32_t h = 2166136261u;
  mix_u32(h, selected ? 1u : 0u);
  mix_u32(h, vectors ? 1u : 0u);
  mix_u32(h, static_cast<uint32_t>(v.settings.declutter));
  mix_u32(h, static_cast<uint32_t>(b.notable));
  mix_u32(h, (vectors && b.aircraft.hasTrack) ? 1u : 0u);
  mix_u32(h, drawTag ? 1u : 0u);
  mix_u32(h, drawL2 ? 1u : 0u);
  mix_u32(h, (selected && drawTag) ? 1u : 0u);  // line3 present when selected+tag
  return h;
}

/** Geometry/text fingerprint — cheap in-place update when only this changes. */
uint32_t blip_content_sig(const desk_display::RadarView& v, std::size_t i,
                          bool vectors) {
  const desk_display::RadarBlip& b = v.blips[i];
  const bool selected = v.hasSelection && v.selectedIndex == i;
  uint32_t h = 2166136261u;
  mix_u32(h, static_cast<uint32_t>(
                 static_cast<int32_t>(b.offsetXMi * 50.0f) + 500000));
  mix_u32(h, static_cast<uint32_t>(
                 static_cast<int32_t>(b.offsetYMi * 50.0f) + 500000));
  if (b.aircraft.hasTrack) {
    mix_u32(h, static_cast<uint32_t>(b.aircraft.trackDeg));
  }
  if (b.aircraft.hasSpeed) {
    mix_u32(h, static_cast<uint32_t>(b.aircraft.speedKt));
  }
  if (b.aircraft.hasAlt) {
    mix_u32(h, static_cast<uint32_t>(b.aircraft.altFt));
  }
  if (!vectors && !selected) {
    mix_u32(h, b.litAgeMs / 500u);
  }
  return h;
}

void fill_blip_hit(BlipHitTarget& hit, float originX, float originY,
                   lv_coord_t bx, lv_coord_t by, bool selected, bool drawL2,
                   bool hasLine4) {
  hit.valid = true;
  hit.markX = originX + static_cast<float>(bx);
  hit.markY = originY + static_cast<float>(by);
  hit.hasTag = false;

  if (!drawL2 && !selected) {
    return;
  }

  lv_coord_t leaderDx = 16;
  lv_coord_t leaderDy = -16;
  tag_leader_offset(bx, by, selected, hasLine4, &leaderDx, &leaderDy);
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
}

lv_obj_t* make_blip_root(lv_obj_t* layer) {
  lv_obj_t* root = lv_obj_create(layer);
  if (!root) {
    return nullptr;
  }
  lv_obj_set_size(root, g_disc_px, g_disc_px);
  lv_obj_set_pos(root, 0, 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(root, 0, 0);
  lv_obj_set_style_pad_all(root, 0, 0);
  lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(root, LV_OBJ_FLAG_CLICKABLE);
  return root;
}

void blip_screen_xy(const desk_display::RadarBlip& b, float scale, lv_coord_t* x,
                    lv_coord_t* y, lv_coord_t* bx, lv_coord_t* by) {
  const lv_coord_t cx = g_disc_px / 2;
  const lv_coord_t cy = g_disc_px / 2;
  *x = static_cast<lv_coord_t>(b.offsetXMi * scale);
  *y = static_cast<lv_coord_t>(-b.offsetYMi * scale);
  *bx = cx + *x;
  *by = cy + *y;
}

void write_vector_pts(TrafficSlot& slot, const desk_display::RadarBlip& b,
                      lv_coord_t bx, lv_coord_t by) {
  const float rad = b.aircraft.trackDeg * kPi / 180.0f;
  const float len = b.aircraft.hasSpeed
                         ? clampf(b.aircraft.speedKt * kVectorLenScale,
                                  kVectorLenMinPx, kVectorLenMaxPx)
                         : kVectorLenDefaultPx;
  const float dx = std::sin(rad) * len;
  const float dy = -std::cos(rad) * len;
  slot.vector_pts[0] = {bx, by};
  slot.vector_pts[1] = {static_cast<lv_coord_t>(bx + dx),
                        static_cast<lv_coord_t>(by + dy)};
}

void write_leader_pts(TrafficSlot& slot, lv_coord_t bx, lv_coord_t by,
                      bool selected, bool hasLine4, lv_coord_t* tagX,
                      lv_coord_t* tagY) {
  lv_coord_t leaderDx = 16;
  lv_coord_t leaderDy = -16;
  tag_leader_offset(bx, by, selected, hasLine4, &leaderDx, &leaderDy);
  *tagX = bx + leaderDx;
  *tagY = by + leaderDy;
  slot.leader_pts[0] = {bx, by};
  slot.leader_pts[1] = {*tagX, *tagY};
}

void create_blip_widgets(TrafficSlot& slot, const desk_display::RadarView& v,
                         std::size_t i, float originX, float originY,
                         bool vectors) {
  const desk_display::RadarBlip& b = v.blips[i];
  const bool selected = v.hasSelection && v.selectedIndex == i;
  const float scale = radar_blip_scale(v.rangeMiles, g_plot_radius_px);
  lv_coord_t x = 0;
  lv_coord_t y = 0;
  lv_coord_t bx = 0;
  lv_coord_t by = 0;
  blip_screen_xy(b, scale, &x, &y, &bx, &by);
  const uint32_t markColor = blip_mark_color(b, selected);
  lv_obj_t* layer = slot.root;

  using desk_display::RadarUnselectedLabel;
  const auto label =
      selected ? RadarUnselectedLabel::DenseTag
               : desk_display::radarUnselectedLabel(v.settings.declutter);
  const bool drawL2 = selected || label == RadarUnselectedLabel::DenseTag;
  const bool drawTag =
      vectors && (selected || label != RadarUnselectedLabel::None);

  char tagLine2[24]{};
  char tagLine3[28]{};
  char tagLine4[8]{};
  compute_tag_lines(v, i, selected, label, tagLine2, sizeof(tagLine2), tagLine3,
                    sizeof(tagLine3), tagLine4, sizeof(tagLine4));
  const bool hasLine4 = selected && tagLine4[0] != '\0';
  fill_blip_hit(g_hit_targets[i], originX, originY, bx, by, selected,
                drawTag && (selected || drawL2), hasLine4);
  if (!drawTag) {
    g_hit_targets[i].hasTag = false;
  }

  slot.mark = nullptr;
  slot.vector = nullptr;
  slot.leader = nullptr;
  slot.tag1 = nullptr;
  slot.tag2 = nullptr;
  slot.tag3 = nullptr;
  slot.tag4 = nullptr;

  if (vectors) {
    slot.mark = lv_label_create(layer);
    lv_label_set_text(slot.mark, "*");
    lv_obj_set_style_text_font(slot.mark, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(slot.mark, rgb(markColor), 0);
    lv_obj_clear_flag(slot.mark, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(slot.mark, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(slot.mark, LV_ALIGN_CENTER, x, y);

    if (b.aircraft.hasTrack) {
      write_vector_pts(slot, b, bx, by);
      slot.vector = lv_line_create(layer);
      lv_obj_set_pos(slot.vector, 0, 0);
      lv_line_set_points(slot.vector, slot.vector_pts, 2);
      lv_obj_set_style_line_width(slot.vector, selected ? 2 : 1, 0);
      lv_obj_set_style_line_color(slot.vector, rgb(markColor), 0);
      lv_obj_set_style_line_rounded(slot.vector, false, 0);
      lv_obj_clear_flag(slot.vector, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_clear_flag(slot.vector, LV_OBJ_FLAG_SCROLLABLE);
    }

    if (drawTag) {
      const lv_opa_t opa = selected ? LV_OPA_COVER : kTagDimOpa;
      lv_coord_t tagX = 0;
      lv_coord_t tagY = 0;
      write_leader_pts(slot, bx, by, selected, hasLine4, &tagX, &tagY);

      slot.leader = lv_line_create(layer);
      lv_obj_set_pos(slot.leader, 0, 0);
      lv_line_set_points(slot.leader, slot.leader_pts, 2);
      lv_obj_set_style_line_width(slot.leader, selected ? 2 : 1, 0);
      lv_obj_set_style_line_color(slot.leader, rgb(kLeaderColor), 0);
      lv_obj_set_style_line_opa(slot.leader, opa, 0);
      lv_obj_set_style_line_rounded(slot.leader, false, 0);
      lv_obj_clear_flag(slot.leader, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_clear_flag(slot.leader, LV_OBJ_FLAG_SCROLLABLE);

      slot.tag1 = lv_label_create(layer);
      lv_label_set_text(slot.tag1,
                        b.aircraft.callsign[0] ? b.aircraft.callsign : "?");
      lv_obj_set_style_text_font(slot.tag1, &lv_font_montserrat_10, 0);
      lv_obj_set_style_text_color(slot.tag1, rgb(markColor), 0);
      lv_obj_set_style_text_opa(slot.tag1, opa, 0);
      lv_obj_set_pos(slot.tag1, tagX, tagY - 12);

      if (drawL2 && tagLine2[0] != '\0') {
        slot.tag2 = lv_label_create(layer);
        lv_label_set_text(slot.tag2, tagLine2);
        lv_obj_set_style_text_font(slot.tag2, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(slot.tag2, rgb(desk_display::theme::kAccent),
                                    0);
        lv_obj_set_style_text_opa(slot.tag2, opa, 0);
        lv_obj_set_pos(slot.tag2, tagX, tagY);
      }

      if (selected && tagLine3[0] != '\0') {
        slot.tag3 = lv_label_create(layer);
        lv_label_set_text(slot.tag3, tagLine3);
        lv_obj_set_style_text_font(slot.tag3, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(slot.tag3, rgb(desk_display::theme::kAccent),
                                    0);
        lv_obj_set_style_text_opa(slot.tag3, opa, 0);
        lv_obj_set_pos(slot.tag3, tagX, tagY + 12);
      }

      if (hasLine4) {
        slot.tag4 = lv_label_create(layer);
        lv_label_set_text(slot.tag4, tagLine4);
        lv_obj_set_style_text_font(slot.tag4, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(slot.tag4, rgb(desk_display::theme::kAccent),
                                    0);
        lv_obj_set_style_text_opa(slot.tag4, opa, 0);
        lv_obj_set_pos(slot.tag4, tagX, tagY + 24);
      }
    }
    return;
  }

  const float ageFrac = static_cast<float>(b.litAgeMs) /
                        static_cast<float>(desk_display::kRadarBlipFadeMs);
  const float alpha = selected ? 1.0f : (1.0f - ageFrac * 0.88f);
  const lv_opa_t opa = static_cast<lv_opa_t>(
      clampf(alpha * 255.0f, selected ? 255.0f : 20.0f, 255.0f));

  lv_coord_t dotPx = kDotPx;
  if (selected || b.notable == desk_display::AircraftNotable::Emergency) {
    dotPx = kDotPx + 2;
  }

  slot.mark = lv_obj_create(layer);
  lv_obj_set_size(slot.mark, dotPx, dotPx);
  lv_obj_set_style_radius(slot.mark, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(slot.mark, rgb(markColor), 0);
  lv_obj_set_style_bg_opa(slot.mark, opa, 0);
  lv_obj_set_style_border_width(slot.mark, selected ? 1 : 0, 0);
  lv_obj_set_style_border_color(slot.mark, rgb(kSelectedColor), 0);
  lv_obj_set_style_pad_all(slot.mark, 0, 0);
  lv_obj_clear_flag(slot.mark, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(slot.mark, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_align(slot.mark, LV_ALIGN_CENTER, x, y);
}

/** Move/restyle existing widgets — no create/delete (paint path). */
void update_blip_widgets(TrafficSlot& slot, const desk_display::RadarView& v,
                         std::size_t i, float originX, float originY,
                         bool vectors) {
  const desk_display::RadarBlip& b = v.blips[i];
  const bool selected = v.hasSelection && v.selectedIndex == i;
  const float scale = radar_blip_scale(v.rangeMiles, g_plot_radius_px);
  lv_coord_t x = 0;
  lv_coord_t y = 0;
  lv_coord_t bx = 0;
  lv_coord_t by = 0;
  blip_screen_xy(b, scale, &x, &y, &bx, &by);
  const uint32_t markColor = blip_mark_color(b, selected);

  using desk_display::RadarUnselectedLabel;
  const auto label =
      selected ? RadarUnselectedLabel::DenseTag
               : desk_display::radarUnselectedLabel(v.settings.declutter);
  const bool drawL2 = selected || label == RadarUnselectedLabel::DenseTag;
  const bool drawTag =
      vectors && (selected || label != RadarUnselectedLabel::None);

  char tagLine2[24]{};
  char tagLine3[28]{};
  char tagLine4[8]{};
  compute_tag_lines(v, i, selected, label, tagLine2, sizeof(tagLine2), tagLine3,
                    sizeof(tagLine3), tagLine4, sizeof(tagLine4));
  const bool hasLine4 = selected && tagLine4[0] != '\0';
  fill_blip_hit(g_hit_targets[i], originX, originY, bx, by, selected,
                drawTag && (selected || drawL2), hasLine4);
  if (!drawTag) {
    g_hit_targets[i].hasTag = false;
  }

  if (vectors) {
    if (slot.mark) {
      lv_obj_set_style_text_color(slot.mark, rgb(markColor), 0);
      lv_obj_align(slot.mark, LV_ALIGN_CENTER, x, y);
    }
    if (slot.vector && b.aircraft.hasTrack) {
      write_vector_pts(slot, b, bx, by);
      lv_line_set_points(slot.vector, slot.vector_pts, 2);
      lv_obj_set_style_line_width(slot.vector, selected ? 2 : 1, 0);
      lv_obj_set_style_line_color(slot.vector, rgb(markColor), 0);
      lv_obj_invalidate(slot.vector);
    }
    if (drawTag && slot.leader) {
      const lv_opa_t opa = selected ? LV_OPA_COVER : kTagDimOpa;
      lv_coord_t tagX = 0;
      lv_coord_t tagY = 0;
      write_leader_pts(slot, bx, by, selected, hasLine4, &tagX, &tagY);
      lv_line_set_points(slot.leader, slot.leader_pts, 2);
      lv_obj_set_style_line_width(slot.leader, selected ? 2 : 1, 0);
      lv_obj_set_style_line_opa(slot.leader, opa, 0);
      lv_obj_invalidate(slot.leader);

      if (slot.tag1) {
        lv_label_set_text(slot.tag1,
                          b.aircraft.callsign[0] ? b.aircraft.callsign : "?");
        lv_obj_set_style_text_color(slot.tag1, rgb(markColor), 0);
        lv_obj_set_style_text_opa(slot.tag1, opa, 0);
        lv_obj_set_pos(slot.tag1, tagX, tagY - 12);
      }
      if (slot.tag2) {
        if (tagLine2[0] != '\0') {
          lv_label_set_text(slot.tag2, tagLine2);
        }
        lv_obj_set_style_text_opa(slot.tag2, opa, 0);
        lv_obj_set_pos(slot.tag2, tagX, tagY);
      }
      if (slot.tag3) {
        if (tagLine3[0] != '\0') {
          lv_label_set_text(slot.tag3, tagLine3);
        }
        lv_obj_set_style_text_opa(slot.tag3, opa, 0);
        lv_obj_set_pos(slot.tag3, tagX, tagY + 12);
      }
      if (slot.tag4) {
        if (tagLine4[0] != '\0') {
          lv_label_set_text(slot.tag4, tagLine4);
        }
        lv_obj_set_style_text_opa(slot.tag4, opa, 0);
        lv_obj_set_pos(slot.tag4, tagX, tagY + 24);
      }
    }
    return;
  }

  if (!slot.mark) {
    return;
  }
  const float ageFrac = static_cast<float>(b.litAgeMs) /
                        static_cast<float>(desk_display::kRadarBlipFadeMs);
  const float alpha = selected ? 1.0f : (1.0f - ageFrac * 0.88f);
  const lv_opa_t opa = static_cast<lv_opa_t>(
      clampf(alpha * 255.0f, selected ? 255.0f : 20.0f, 255.0f));
  lv_coord_t dotPx = kDotPx;
  if (selected || b.notable == desk_display::AircraftNotable::Emergency) {
    dotPx = kDotPx + 2;
  }
  lv_obj_set_size(slot.mark, dotPx, dotPx);
  lv_obj_set_style_bg_color(slot.mark, rgb(markColor), 0);
  lv_obj_set_style_bg_opa(slot.mark, opa, 0);
  lv_obj_set_style_border_width(slot.mark, selected ? 1 : 0, 0);
  lv_obj_align(slot.mark, LV_ALIGN_CENTER, x, y);
}

/**
 * Traffic layer: ≤25 mi → stars + vectors + declutter-aware tags;
 * above that → dense dots only (still selectable).
 *
 * Paint path updates existing widgets in place. Create/delete only when a
 * callsign appears/vanishes or its widget layout changes.
 */
void sync_traffic(lv_obj_t* layer, const desk_display::RadarView& v) {
  clear_hit_targets();
  for (int i = 0; i < kMaxDots; ++i) {
    g_traffic_slots[i].keep = false;
  }

  if (!refresh_blips_origin(layer)) {
    return;
  }
  const float originX = g_blips_origin_x;
  const float originY = g_blips_origin_y;

  const bool vectors = show_vectors(v.rangeMiles);
  const std::size_t count =
      v.blipCount < static_cast<std::size_t>(kMaxDots) ? v.blipCount : kMaxDots;

  for (std::size_t i = 0; i < count; ++i) {
    const auto& b = v.blips[i];
    const bool selected = v.hasSelection && v.selectedIndex == i;
    if (!selected && b.litAgeMs >= desk_display::kRadarBlipFadeMs) {
      continue;
    }

    const char* cs = b.aircraft.callsign[0] != '\0' ? b.aircraft.callsign : "?";
    const uint32_t layout = blip_layout_sig(v, i, vectors);
    const uint32_t content = blip_content_sig(v, i, vectors);
    TrafficSlot* slot = find_traffic_slot(cs);

    if (slot != nullptr && slot->root != nullptr && slot->layout_sig == layout) {
      slot->keep = true;
      if (slot->content_sig != content) {
        update_blip_widgets(*slot, v, i, originX, originY, vectors);
        slot->content_sig = content;
      } else {
        // Unchanged on-screen blip — refresh hit boxes only.
        const float scale = radar_blip_scale(v.rangeMiles, g_plot_radius_px);
        lv_coord_t x = 0;
        lv_coord_t y = 0;
        lv_coord_t bx = 0;
        lv_coord_t by = 0;
        blip_screen_xy(b, scale, &x, &y, &bx, &by);
        using desk_display::RadarUnselectedLabel;
        const auto label =
            selected ? RadarUnselectedLabel::DenseTag
                     : desk_display::radarUnselectedLabel(v.settings.declutter);
        const bool drawTag =
            vectors && (selected || label != RadarUnselectedLabel::None);
        const bool drawL2 =
            selected || label == RadarUnselectedLabel::DenseTag;
        fill_blip_hit(g_hit_targets[i], originX, originY, bx, by, selected,
                      drawTag && (selected || drawL2), /*hasLine4=*/false);
        if (!drawTag) {
          g_hit_targets[i].hasTag = false;
        }
      }
      continue;
    }

    if (slot == nullptr) {
      slot = alloc_traffic_slot();
      if (slot == nullptr) {
        continue;
      }
    }

    if (slot->root != nullptr) {
      lv_obj_del(slot->root);
      slot->root = nullptr;
      slot->mark = nullptr;
      slot->vector = nullptr;
      slot->leader = nullptr;
      slot->tag1 = nullptr;
      slot->tag2 = nullptr;
      slot->tag3 = nullptr;
      slot->tag4 = nullptr;
    }

    std::snprintf(slot->callsign, sizeof(slot->callsign), "%s", cs);
    slot->layout_sig = layout;
    slot->content_sig = content;
    slot->active = true;
    slot->keep = true;
    slot->root = make_blip_root(layer);
    if (slot->root == nullptr) {
      forget_traffic_slot(*slot, false);
      continue;
    }
    create_blip_widgets(*slot, v, i, originX, originY, vectors);
  }

  for (int i = 0; i < kMaxDots; ++i) {
    if (g_traffic_slots[i].active && !g_traffic_slots[i].keep) {
      forget_traffic_slot(g_traffic_slots[i], true);
    }
  }
}

void build_traffic(lv_obj_t* layer, const desk_display::RadarView& v) {
  // Full rebuild path (screen enter / disc recreate).
  clear_traffic_slots(true);
  sync_traffic(layer, v);
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
  // Roots die with the disc/parent clean — only drop slot metadata here.
  clear_traffic_slots(false);
  g_cached_range = -1.0f;
  g_cached_center_lat = 0.0;
  g_cached_center_lon = 0.0;
  g_cached_static_count = 0;
  g_cached_ring_count = 0;
  g_cached_highway_count = 0;
  g_cached_has_static_sel = false;
  g_cached_selected_static = 0;
  g_cached_traffic_sig = 0;
  g_traffic_cache_valid = false;
  g_cached_age_count = 0;
  release_static_snapshot_buf();
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

  // Sweep geometry only — keep this path cheap so Dial hits ~30 Hz.
  apply_trail_geometry(v.sweepAngleDeg);

  if (overlay_needs_rebuild(v)) {
    rebuild_static_overlay(g_static_layer, v);
  }

  if (traffic_needs_rebuild(v)) {
    if (g_hdr) {
      char hdr[48];
      format_header(hdr, sizeof(hdr), v);
      if (std::strcmp(lv_label_get_text(g_hdr), hdr) != 0) {
        lv_label_set_text(g_hdr, hdr);
      }
    }
    // Incremental per-callsign sync — a sweep paint must not tear down every
    // other target's LVGL objects (that hitch is what stalled the beam).
    sync_traffic(g_blips_layer, v);
    mark_traffic_built(v);
  }

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

  // Map under sweep (design paint order): flatten to one image so Classic
  // sweep invalidates don't restroke hundreds of airspace/highway lines.
  g_static_layer = lv_obj_create(g_disc);
  lv_obj_set_size(g_static_layer, g_disc_px, g_disc_px);
  lv_obj_set_pos(g_static_layer, 0, 0);
  lv_obj_set_style_bg_opa(g_static_layer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_static_layer, 0, 0);
  lv_obj_set_style_pad_all(g_static_layer, 0, 0);
  lv_obj_clear_flag(g_static_layer, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(g_static_layer, LV_OBJ_FLAG_CLICKABLE);
  rebuild_static_overlay(g_static_layer, v);

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
  mark_traffic_built(v);
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
