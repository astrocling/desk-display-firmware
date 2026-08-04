#include "hal/dial_shell.hpp"

#include "desk_display/adsb_poll.hpp"
#include "desk_display/map_context_poll.hpp"
#include "desk_display/nav.hpp"
#include "desk_display/nav_status.hpp"
#include "desk_display/scores_poll.hpp"
#include "desk_display/screen_clock.hpp"
#include "desk_display/screen_radar.hpp"
#include "desk_display/screen_sports.hpp"
#include "desk_display/screen_timezones.hpp"
#include "desk_display/screen_weather.hpp"
#include "desk_display/theme.hpp"
#include "desk_display/touch_gesture.hpp"
#include "desk_display/weather_poll.hpp"

#include "hal/display.hpp"
#include "net/http.hpp"
#include "net/http_async.hpp"
#include "net/ntp.hpp"
#include "../ui/carousel_lvgl.hpp"
#include "../ui/clock_lvgl.hpp"
#include "../ui/radar_lvgl.hpp"
#include "../ui/sports_lvgl.hpp"
#include "../ui/timezones_lvgl.hpp"
#include "../ui/weather_lvgl.hpp"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <new>

namespace desk_hal {
namespace {

/**
 * Large radar objects (~55KB ScreenRadar, ~24KB MapContext) must not live in
 * internal BSS: LVGL needs ~29KB contiguous DMA DRAM for the draw buffer.
 * Prefer PSRAM; fall back to internal heap only if SPIRAM is unavailable.
 */
template <typename T>
T* allocLarge() {
  void* mem = heap_caps_malloc(sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (mem == nullptr) {
    mem = heap_caps_malloc(sizeof(T), MALLOC_CAP_8BIT);
  }
  if (mem == nullptr) {
    return nullptr;
  }
  return new (mem) T();
}

desk_display::Nav g_nav;
desk_display::ScreenClock g_clock;
desk_display::ScreenTimezones g_timezones;
desk_display::WeatherScreen g_weather;
desk_display::ScreenSports g_sports;
desk_display::WeatherPoller g_weather_poll;
desk_display::ScoresPoller g_scores_poll;

// Heap/PSRAM — not BSS (see allocLarge).
desk_display::ScreenRadar* g_radar = nullptr;
desk_display::AdsbPoller* g_adsb_poll = nullptr;
desk_display::MapContextPoller* g_map_ctx_poll = nullptr;
desk_display::MapContext* g_map_scratch = nullptr;
desk_display::AircraftList* g_ac_scratch = nullptr;
/** Cap Classic sweep UI updates — traffic rebuild every tick OOMs/starves LVGL. */
uint32_t g_radar_ui_accum_ms = 0;
constexpr uint32_t kRadarUiPeriodMs = 33;

bool dialMapHttpGet(const char* url, char* body, std::size_t bodyCap,
                    std::size_t& bodyLen, void* user) {
  return desk_net::httpGetAsync(desk_net::HttpAsyncChannel::RadarMap, url, body,
                                bodyCap, bodyLen, user);
}

bool dialAdsbHttpGet(const char* url, char* body, std::size_t bodyCap,
                     std::size_t& bodyLen, void* user) {
  return desk_net::httpGetAsync(desk_net::HttpAsyncChannel::RadarAdsb, url, body,
                                bodyCap, bodyLen, user);
}

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

  if (g_radar != nullptr && g_nav.active_screen() == Screen::Radar &&
      desk_ui::radar_lvgl_animate_classic(g_body, g_radar->view())) {
    return;
  }

  lv_obj_clean(g_body);
  desk_ui::radar_lvgl_invalidate();

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
      desk_ui::weather_lvgl_build(g_body, g_weather.view());
      break;
    case Screen::Sports:
      desk_ui::sports_lvgl_build(g_body, g_sports.view());
      break;
    case Screen::Radar:
      if (g_radar != nullptr) {
        desk_ui::radar_lvgl_build(g_body, g_radar->view());
      }
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
    desk_ui::radar_lvgl_invalidate();
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

  if (carousel_mode) {
    desk_ui::carousel_lvgl_set_highlight(g_carousel, g_nav.highlighted());
  }

  g_last_screen = g_nav.active_screen();
  g_last_mode = g_nav.mode();
  if (g_last_screen == desk_display::Screen::Radar) {
    Serial.printf("shell: build Radar mode=%s\n",
                  carousel_mode ? "carousel" : "focused");
  }
  refresh_content();
  publishNavSerial();
}

void settle_focused_screens() {
  g_timezones.onIdleSettle();
  g_weather.onIdleSettle();
  g_sports.onIdleSettle();
  if (g_radar != nullptr) {
    g_radar->onIdleSettle();
  }
}

void on_rotate_focused(int8_t delta) {
  switch (g_nav.focused()) {
    case desk_display::Screen::Timezones:
      g_timezones.onRotate(delta);
      break;
    case desk_display::Screen::Weather:
      g_weather.onRotate(delta);
      break;
    case desk_display::Screen::Sports:
      g_sports.onRotate(delta);
      break;
    case desk_display::Screen::Radar:
      if (g_radar != nullptr) {
        g_radar->onRotate(delta);
      }
      break;
    default:
      break;
  }
}

}  // namespace

bool dialShellInit() {
  if (lv_disp_get_default() == nullptr) {
    return false;
  }

  g_radar = allocLarge<desk_display::ScreenRadar>();
  g_adsb_poll = allocLarge<desk_display::AdsbPoller>();
  g_map_ctx_poll = allocLarge<desk_display::MapContextPoller>();
  g_map_scratch = allocLarge<desk_display::MapContext>();
  g_ac_scratch = allocLarge<desk_display::AircraftList>();
  if (g_radar == nullptr || g_adsb_poll == nullptr ||
      g_map_ctx_poll == nullptr || g_map_scratch == nullptr ||
      g_ac_scratch == nullptr) {
    Serial.println("shell: radar alloc failed");
    return false;
  }

  g_weather_poll.setHttpGet(&desk_net::httpGet, nullptr);
  g_scores_poll.setHttpGet(&desk_net::httpGet, nullptr);
  if (!desk_net::httpAsyncInit()) {
    Serial.println("shell: http-async init failed");
    return false;
  }
  g_adsb_poll->setHttpGet(&dialAdsbHttpGet, nullptr);
  g_map_ctx_poll->setHttpGet(&dialMapHttpGet, nullptr);

#if defined(RADAR_POI_COUNT)
  g_radar->setPois(RADAR_POIS, static_cast<std::size_t>(RADAR_POI_COUNT));
#endif

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
  if (g_nav.mode() == desk_display::NavMode::Focused && g_radar != nullptr &&
      g_nav.focused() == desk_display::Screen::Radar &&
      g_radar->settingsOpen()) {
    return;  // frozen
  }
  if (g_nav.mode() == desk_display::NavMode::Carousel) {
    g_nav.on_rotate(delta);
  } else {
    on_rotate_focused(delta);
    g_nav.idle_reset();
    if (g_nav.focused() == desk_display::Screen::Timezones ||
        g_nav.focused() == desk_display::Screen::Weather ||
        g_nav.focused() == desk_display::Screen::Sports ||
        g_nav.focused() == desk_display::Screen::Radar) {
      refresh_content();
    }
  }
}

void dialShellOnTouch(const desk_display::TouchGesture& g) {
  using desk_display::NavMode;
  using desk_display::Screen;
  using desk_display::TouchGestureKind;

  if (g.kind == TouchGestureKind::None) {
    return;
  }

  const bool radar_settings =
      g_nav.mode() == NavMode::Focused &&
      g_nav.focused() == Screen::Radar && g_radar != nullptr &&
      g_radar->settingsOpen();

  if (g.kind == TouchGestureKind::DoubleTap) {
    if (radar_settings) {
      g_radar->closeSettings();
      g_nav.idle_reset();
      refresh_content();
      return;
    }
    if (g_nav.mode() == NavMode::Focused &&
        g_nav.focused() == Screen::Radar && g_radar != nullptr) {
      g_radar->revertTempCenter();
    }
    g_nav.on_center_tap();
    rebuild_ui_for_active();
    return;
  }

  if (g.kind == TouchGestureKind::Tap) {
    if (g_nav.mode() != NavMode::Focused) {
      return;  // Carousel: ignore
    }
    // Task 4 fills Radar tap; for now idle_reset only if non-radar
    g_nav.idle_reset();
    return;
  }

  if (g.kind == TouchGestureKind::LongPress) {
    if (g_nav.mode() != NavMode::Focused) {
      return;
    }
    g_nav.idle_reset();
    // Task 4 opens settings
    return;
  }
}

void dialShellOnTick(uint32_t elapsed_ms) {
  using desk_display::Screen;

  if (g_radar == nullptr || g_adsb_poll == nullptr ||
      g_map_ctx_poll == nullptr) {
    return;
  }

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

  // Rebuild UI before any blocking HTTP so carousel stays responsive and TLS
  // does not run on a half-torn-down body.
  const auto idle = g_nav.on_tick(elapsed_ms);
  if (idle == desk_display::IdleEvent::SettleFocused) {
    settle_focused_screens();
  }

  if (g_nav.active_screen() != g_last_screen || g_nav.mode() != g_last_mode) {
    rebuild_ui_for_active();
  } else if (idle == desk_display::IdleEvent::SettleFocused) {
    refresh_content();
  }

  const bool weather_active = g_nav.active_screen() == Screen::Weather;
  g_weather_poll.setActive(weather_active);
  g_weather_poll.onTick(elapsed_ms);

  desk_display::Weather fresh_weather{};
  const bool got_weather = g_weather_poll.takeWeather(fresh_weather);
  if (got_weather) {
    g_weather.bind(fresh_weather);
    Serial.println("weather: bound");
    refresh_content();
  }

  const bool sports_active = g_nav.active_screen() == Screen::Sports;
  g_scores_poll.setActive(sports_active);
  g_scores_poll.onTick(elapsed_ms);

  desk_display::Scores fresh_scores{};
  const bool got_scores = g_scores_poll.takeScores(fresh_scores);
  if (got_scores) {
    g_sports.bind(fresh_scores);
    Serial.println("scores: bound");
    refresh_content();
  }

  const bool radar_active = g_nav.active_screen() == Screen::Radar;
  g_adsb_poll->setActive(radar_active);
  g_map_ctx_poll->setActive(radar_active);

  if (radar_active) {
    g_radar->onTick(elapsed_ms);
    g_adsb_poll->setCenter(g_radar->centerLat(), g_radar->centerLon(),
                           g_radar->rangeMiles());
    g_map_ctx_poll->setCenter(g_radar->centerLat(), g_radar->centerLon(),
                              g_radar->rangeMiles());
  }

  // Advance Classic sweep on the UI before kicking/polling HTTP so a slow
  // bind never starves the phosphor arm (GETs run on http_async worker).
  bool radar_dirty = false;
  if (radar_active) {
    g_radar_ui_accum_ms += elapsed_ms;
    if (g_radar_ui_accum_ms >= kRadarUiPeriodMs) {
      g_radar_ui_accum_ms = 0;
      radar_dirty = true;
    }
  } else {
    g_radar_ui_accum_ms = 0;
  }
  if (radar_dirty) {
    refresh_content();
    radar_dirty = false;
  }

  g_map_ctx_poll->onTick(elapsed_ms);
  g_adsb_poll->onTick(elapsed_ms);

  // Scratch lives in PSRAM — MapContext (~24KB) must not sit on the loop stack.
  if (g_map_ctx_poll->takeContext(*g_map_scratch)) {
    g_radar->bindMapContext(*g_map_scratch);
    Serial.printf("map: bound airports=%u rings=%u",
                  static_cast<unsigned>(g_map_scratch->airportCount),
                  static_cast<unsigned>(g_map_scratch->ringCount));
    for (std::size_t i = 0; i < g_map_scratch->ringCount && i < 4; ++i) {
      Serial.printf(" %s", g_map_scratch->rings[i].id);
    }
    Serial.println();
    radar_dirty = true;
  }

  if (g_adsb_poll->takeAircraft(*g_ac_scratch)) {
    g_radar->bind(*g_ac_scratch);
    Serial.println("adsb: bound");
    radar_dirty = true;
  }

  if (radar_dirty) {
    refresh_content();
  }
}

}  // namespace desk_hal
