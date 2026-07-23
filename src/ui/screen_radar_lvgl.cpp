/**
 * Dial's strong `screen_radar_*` LVGL entry points. Renders via the shared
 * `desk_ui::radar_lvgl_build` — same renderer the sim uses directly.
 *
 * The sim does NOT go through these (it calls `radar_lvgl_build` itself
 * from `SimApp::refresh_content`), so this container/model pair only owns
 * the dial's screen lifetime; no double ownership of the LVGL disc.
 */
#include "desk_display/screens_stub.hpp"

#include "radar_lvgl.hpp"

#include <lvgl.h>

namespace desk_display {

namespace {

lv_obj_t* g_container = nullptr;
ScreenRadar* g_model = nullptr;

}  // namespace

void screen_radar_bind_model(ScreenRadar* model) { g_model = model; }

void screen_radar_on_tick(uint32_t elapsed_ms) {
  if (g_model) {
    g_model->onTick(elapsed_ms);
  }
}

void screen_radar_create() {
  if (g_container) {
    return;
  }
  g_container = lv_obj_create(lv_scr_act());
  lv_obj_set_size(g_container, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(g_container, 0, 0);
  lv_obj_add_flag(g_container, LV_OBJ_FLAG_HIDDEN);
}

void screen_radar_destroy() {
  if (!g_container) {
    return;
  }
  lv_obj_del(g_container);
  g_container = nullptr;
}

void screen_radar_show() {
  if (!g_container) {
    screen_radar_create();
  }
  lv_obj_clean(g_container);
  if (g_model) {
    desk_ui::radar_lvgl_build(g_container, g_model->view());
  }
  lv_obj_clear_flag(g_container, LV_OBJ_FLAG_HIDDEN);
}

void screen_radar_hide() {
  if (g_container) {
    lv_obj_add_flag(g_container, LV_OBJ_FLAG_HIDDEN);
  }
}

}  // namespace desk_display
