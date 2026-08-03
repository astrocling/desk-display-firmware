#include "hal/dial_shell.hpp"

#include "desk_display/nav.hpp"
#include "desk_display/nav_status.hpp"
#include "desk_display/screen_clock.hpp"
#include "desk_display/screen_timezones.hpp"
#include "desk_display/theme.hpp"

#include "hal/display.hpp"
#include "net/ntp.hpp"
#include "../ui/carousel_lvgl.hpp"
#include "../ui/clock_lvgl.hpp"
#include "../ui/screen_stub_lvgl.hpp"
#include "../ui/timezones_lvgl.hpp"

#include <Arduino.h>
#include <lvgl.h>

namespace desk_hal {
namespace {

desk_display::Nav g_nav;
desk_display::ScreenClock g_clock;
desk_display::ScreenTimezones g_timezones;

lv_obj_t* g_root = nullptr;
lv_obj_t* g_carousel_root = nullptr;
desk_ui::CarouselChrome g_carousel{};
lv_obj_t* g_focused_host = nullptr;
lv_obj_t* g_body = nullptr;

desk_display::Screen g_last_screen = desk_display::Screen::Count;
desk_display::NavMode g_last_mode = desk_display::NavMode::Carousel;
uint32_t g_clock_accum_ms = 0;
std::int64_t g_last_unix = 0;

lv_color_t rgb(uint32_t c) {
  return lv_color_make(static_cast<uint8_t>((c >> 16) & 0xFF),
                       static_cast<uint8_t>((c >> 8) & 0xFF),
                       static_cast<uint8_t>((c >> 0) & 0xFF));
}

void publishNavSerial() {
  char serial[40];
  desk_display::formatNavSerial(g_nav.mode(), g_nav.active_screen(), serial,
                                sizeof(serial));
  Serial.println(serial);
}

void sync_clock_from_ntp() {
  if (!desk_net::ntpIsSynced()) {
    return;
  }
  const std::int64_t unix = desk_net::ntpUnixUtc();
  if (unix <= 0) {
    return;
  }
  g_clock.setUnixUtc(unix);
  g_timezones.setLiveUnix(unix);
  g_last_unix = unix;
}

void refresh_content() {
  using desk_display::Screen;

  if (g_body == nullptr) {
    return;
  }

  lv_obj_clean(g_body);

  const bool carousel_mode = g_nav.mode() == desk_display::NavMode::Carousel;
  const lv_coord_t host_h =
      carousel_mode ? desk_ui::kCarouselPreviewHostPx
                    : static_cast<lv_coord_t>(kLcdHeight - 48);

  switch (g_nav.active_screen()) {
    case Screen::Clock:
      desk_ui::clock_lvgl_build(g_body, g_clock.view());
      break;
    case Screen::Timezones:
      desk_ui::timezones_lvgl_build(g_body, g_timezones.view(), host_h);
      break;
    case Screen::Weather:
      desk_ui::screen_stub_lvgl_build(g_body, "Weather");
      break;
    case Screen::Sports:
      desk_ui::screen_stub_lvgl_build(g_body, "Sports");
      break;
    case Screen::Radar:
      desk_ui::screen_stub_lvgl_build(g_body, "Radar");
      break;
    default:
      break;
  }
}

void rebuild_ui_for_active() {
  using desk_display::NavMode;
  const bool carousel_mode = g_nav.mode() == NavMode::Carousel;

  if (carousel_mode) {
    lv_obj_clear_flag(g_carousel_root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_focused_host, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(g_carousel_root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(g_focused_host, LV_OBJ_FLAG_HIDDEN);
  }

  if (g_body != nullptr) {
    lv_obj_del(g_body);
    g_body = nullptr;
  }

  lv_obj_t* const body_parent =
      carousel_mode ? g_carousel.preview_host : g_focused_host;
  g_body = lv_obj_create(body_parent);
  lv_obj_set_size(g_body, lv_pct(100), lv_pct(100));
  lv_obj_set_style_bg_opa(g_body, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_body, 0, 0);
  lv_obj_set_style_pad_all(g_body, 0, 0);
  lv_obj_clear_flag(g_body, LV_OBJ_FLAG_SCROLLABLE);

  if (carousel_mode) {
    desk_ui::carousel_lvgl_set_highlight(g_carousel, g_nav.highlighted());
  }

  g_last_screen = g_nav.active_screen();
  g_last_mode = g_nav.mode();
  refresh_content();
  publishNavSerial();
}

void settle_focused_screens() {
  g_timezones.onIdleSettle();
}

void on_rotate_focused(int8_t delta) {
  if (g_nav.focused() == desk_display::Screen::Timezones) {
    g_timezones.onRotate(delta);
  }
}

}  // namespace

bool dialShellInit() {
  if (lv_disp_get_default() == nullptr) {
    return false;
  }

  g_root = lv_obj_create(lv_scr_act());
  lv_obj_set_size(g_root, kLcdWidth, kLcdHeight);
  lv_obj_center(g_root);
  lv_obj_set_style_radius(g_root, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_clip_corner(g_root, true, 0);
  lv_obj_set_style_bg_color(g_root, rgb(desk_display::theme::kBg), 0);
  lv_obj_set_style_bg_opa(g_root, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(g_root, 0, 0);
  lv_obj_set_style_pad_all(g_root, 0, 0);
  lv_obj_clear_flag(g_root, LV_OBJ_FLAG_SCROLLABLE);

  g_carousel_root = lv_obj_create(g_root);
  lv_obj_set_size(g_carousel_root, kLcdWidth, kLcdHeight);
  lv_obj_center(g_carousel_root);
  lv_obj_set_style_bg_opa(g_carousel_root, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_carousel_root, 0, 0);
  lv_obj_set_style_pad_all(g_carousel_root, 0, 0);
  lv_obj_clear_flag(g_carousel_root, LV_OBJ_FLAG_SCROLLABLE);
  g_carousel = desk_ui::carousel_lvgl_build(g_carousel_root);

  g_focused_host = lv_obj_create(g_root);
  lv_obj_set_size(g_focused_host, kLcdWidth, kLcdHeight);
  lv_obj_center(g_focused_host);
  lv_obj_set_style_bg_opa(g_focused_host, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(g_focused_host, 0, 0);
  lv_obj_set_style_pad_all(g_focused_host, 0, 0);
  lv_obj_clear_flag(g_focused_host, LV_OBJ_FLAG_SCROLLABLE);

  g_nav.reset();
  sync_clock_from_ntp();
  rebuild_ui_for_active();
  return true;
}

void dialShellOnRotate(int8_t delta) {
  if (delta == 0) {
    return;
  }
  if (g_nav.mode() == desk_display::NavMode::Carousel) {
    g_nav.on_rotate(delta);
  } else {
    on_rotate_focused(delta);
    g_nav.idle_reset();
    if (g_nav.focused() == desk_display::Screen::Timezones) {
      refresh_content();
    }
  }
}

void dialShellOnCenterTap() { g_nav.on_center_tap(); }

void dialShellOnTick(uint32_t elapsed_ms) {
  using desk_display::Screen;

  g_clock_accum_ms += elapsed_ms;
  if (g_clock_accum_ms >= 1000) {
    g_clock_accum_ms = 0;
    const std::int64_t prev = g_last_unix;
    sync_clock_from_ntp();
    if (g_nav.active_screen() == Screen::Clock ||
        g_nav.active_screen() == Screen::Timezones) {
      if (g_last_unix != prev || desk_net::ntpIsSynced()) {
        refresh_content();
      }
    }
  }

  const auto idle = g_nav.on_tick(elapsed_ms);
  if (idle == desk_display::IdleEvent::SettleFocused) {
    settle_focused_screens();
  }

  if (g_nav.active_screen() != g_last_screen || g_nav.mode() != g_last_mode) {
    rebuild_ui_for_active();
  } else if (idle == desk_display::IdleEvent::SettleFocused) {
    refresh_content();
  }
}

}  // namespace desk_hal
