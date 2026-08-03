#include "sim_app.hpp"

#include "sdl_hal.hpp"

#include "desk_display/adsb.hpp"
#include "desk_display/airport.hpp"
#include "desk_display/radar_prefs.hpp"
#include "desk_display/radar_settings.hpp"
#include "desk_display/map_context.hpp"
#include "desk_display/scores.hpp"
#include "desk_display/theme.hpp"
#include "desk_display/timezones.hpp"
#include "desk_display/weather.hpp"
#include "sim_http.hpp"

#include "../ui/clock_lvgl.hpp"
#include "../ui/radar_lvgl.hpp"
#include "../ui/sports_lvgl.hpp"
#include "../ui/timezones_lvgl.hpp"
#include "../ui/weather_lvgl.hpp"

#if __has_include("config.h")
#include "config.h"
#endif

#include <cstdio>
#include <ctime>
#include <cstring>

// Reuse test fixture loader
#include "../../test/fixture_loader.hpp"

namespace sim {
namespace {

constexpr const char* kRadarPrefsPath = "radar_prefs.bin";

lv_color_t rgb(uint32_t c) {
  return lv_color_make((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
}

}  // namespace

bool SimApp::init() {
  load_fixtures();
  sync_clock_from_wall();
  adsb_poll_.setHttpGet(&sim::simAdsbHttpGet, nullptr);
  map_ctx_poll_.setHttpGet(&sim::simMapContextHttpGet, nullptr);
  scores_poll_.setHttpGet(&sim::simScoresHttpGet, nullptr);
  weather_poll_.setHttpGet(&sim::simWeatherHttpGet, nullptr);

  root_ = lv_obj_create(lv_scr_act());
  lv_obj_set_size(root_, kDispW, kDispH);
  lv_obj_center(root_);
  lv_obj_set_style_radius(root_, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_clip_corner(root_, true, 0);
  lv_obj_set_style_bg_color(root_, rgb(desk_display::theme::kBg), 0);
  lv_obj_set_style_border_width(root_, 0, 0);
  lv_obj_set_style_pad_all(root_, 0, 0);
  lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

  carousel_root_ = lv_obj_create(root_);
  lv_obj_set_size(carousel_root_, kDispW, kDispH);
  lv_obj_center(carousel_root_);
  lv_obj_set_style_bg_opa(carousel_root_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(carousel_root_, 0, 0);
  lv_obj_set_style_pad_all(carousel_root_, 0, 0);
  lv_obj_clear_flag(carousel_root_, LV_OBJ_FLAG_SCROLLABLE);
  carousel_ = desk_ui::carousel_lvgl_build(carousel_root_);
  desk_ui::carousel_lvgl_set_highlight(carousel_, nav_.highlighted());

  focused_host_ = lv_obj_create(root_);
  lv_obj_set_size(focused_host_, kDispW, kDispH);
  lv_obj_center(focused_host_);
  lv_obj_set_style_bg_opa(focused_host_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(focused_host_, 0, 0);
  lv_obj_set_style_pad_all(focused_host_, 0, 0);
  lv_obj_clear_flag(focused_host_, LV_OBJ_FLAG_SCROLLABLE);

  last_screen_ = desk_display::Screen::Count;
  rebuild_ui_for_active();
  return true;
}

void SimApp::load_fixtures() {
  char buf[256 * 1024];

  desk_display::Weather w{};
  if (loadFixture("weather.json", buf, sizeof(buf)) && desk_display::parseWeather(buf, w)) {
    weather_.bind(w);
  }

  desk_display::Timezones tz{};
  if (loadFixture("timezones.json", buf, sizeof(buf)) &&
      desk_display::parseTimezones(buf, tz)) {
    timezones_.setSunTimes(tz);
  }

  desk_display::Scores scores{};
  if (loadFixture("scores.json", buf, sizeof(buf)) && desk_display::parseScores(buf, scores)) {
    sports_.bind(scores);
  }

  desk_display::MapContext mapCtx{};
  if (loadFixture("map_context_dayton.json", buf, sizeof(buf)) &&
      desk_display::parseMapContext(buf, mapCtx)) {
    radar_.bindMapContext(mapCtx);
  }

#if defined(RADAR_POI_COUNT)
  radar_.setPois(RADAR_POIS, static_cast<std::size_t>(RADAR_POI_COUNT));
#endif

  desk_display::RadarSettings prefs = desk_display::radarSettingsFactoryDefaults();
  desk_display::loadRadarSettingsFromFile(prefs, kRadarPrefsPath);
  radar_.setSettings(prefs);
  if (prefs.demoMode) {
    bind_demo_adsb_fixture();
  }

  // Carousel is the browse path to Timezones — no "→ Timezones" chrome on Clock.
  clock_.setTimezoneBoardHint(false);
  std::fprintf(stdout, "Fixtures loaded (weather/tz/scores/map-context as available).\n");
}

void SimApp::persist_radar_prefs() {
  desk_display::saveRadarSettingsToFile(radar_.settings(), kRadarPrefsPath);
}

void SimApp::bind_demo_adsb_fixture() {
  char buf[256 * 1024];
  desk_display::AircraftList list{};
  if (loadFixture("adsb_sample.json", buf, sizeof(buf)) &&
      desk_display::parseAdsb(buf, list)) {
    radar_.bind(list);
  }
}

void SimApp::sync_clock_from_wall() {
  const std::time_t now = std::time(nullptr);
  clock_.setUnixUtc(static_cast<std::int64_t>(now));
  timezones_.setLiveUnix(static_cast<std::int64_t>(now));
}

void SimApp::rebuild_ui_for_active() {
  using desk_display::NavMode;
  const bool carousel_mode = nav_.mode() == NavMode::Carousel;

  if (carousel_mode) {
    lv_obj_clear_flag(carousel_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(focused_host_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(carousel_root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(focused_host_, LV_OBJ_FLAG_HIDDEN);
  }

  if (body_) {
    lv_obj_del(body_);
    body_ = nullptr;
  }
  desk_ui::radar_lvgl_invalidate();

  lv_obj_t* const body_parent = carousel_mode ? carousel_.preview_host : focused_host_;
  body_ = lv_obj_create(body_parent);
  lv_obj_set_size(body_, lv_pct(100), lv_pct(100));
  lv_obj_set_style_bg_opa(body_, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(body_, 0, 0);
  lv_obj_set_style_pad_all(body_, 0, 0);
  lv_obj_clear_flag(body_, LV_OBJ_FLAG_SCROLLABLE);

  if (carousel_mode) {
    desk_ui::carousel_lvgl_set_highlight(carousel_, nav_.highlighted());
  }

  last_screen_ = nav_.active_screen();
  last_mode_ = nav_.mode();
  refresh_content();
}

void SimApp::settle_focused_screens() {
  weather_.onIdleSettle();
  radar_.onIdleSettle();
  sports_.onIdleSettle();
  timezones_.onIdleSettle();
  // Clock: no ephemeral state.
}

void SimApp::refresh_content() {
  using desk_display::Screen;

  if (!body_) {
    return;
  }

  const Screen scr = nav_.active_screen();

  // Radar: update sweep/traffic in place to avoid a full teardown flash
  // (especially noticeable when the beam wraps through north).
  if (scr == Screen::Radar &&
      desk_ui::radar_lvgl_animate_classic(body_, radar_.view())) {
    return;
  }

  lv_obj_clean(body_);
  desk_ui::radar_lvgl_invalidate();

  // Carousel and Focused share the same default screen views. Mode only
  // changes input: Carousel rotate cycles screens; Focused rotate is in-app.
  switch (scr) {
    case Screen::Clock:
      desk_ui::clock_lvgl_build(body_, clock_.view());
      break;
    case Screen::Timezones:
      desk_ui::timezones_lvgl_build(body_, timezones_.view(),
                                    static_cast<lv_coord_t>(kDispH - 48));
      break;
    case Screen::Weather:
      desk_ui::weather_lvgl_build(body_, weather_.view());
      break;
    case Screen::Sports:
      desk_ui::sports_lvgl_build(body_, sports_.view());
      break;
    case Screen::Radar: {
      desk_ui::radar_lvgl_build(body_, radar_.view());
      break;
    }
    default:
      break;
  }
}

void SimApp::on_rotate_focused(int delta) {
  using desk_display::Screen;
  switch (nav_.focused()) {
    case Screen::Timezones:
      timezones_.onRotate(delta);
      break;
    case Screen::Weather:
      weather_.onRotate(delta);
      break;
    case Screen::Sports:
      sports_.onRotate(delta);
      break;
    case Screen::Radar:
      radar_.onRotate(delta);
      break;
    default:
      break;
  }
}

void SimApp::on_tap_focused(int16_t x, int16_t y) {
  using desk_display::Screen;
  switch (nav_.focused()) {
    case Screen::Sports:
      sports_.onTap();
      break;
    case Screen::Weather:
      if (weather_.view().alertBadge) {
        if (weather_.alertDetailOpen()) {
          weather_.closeAlertDetail();
        } else {
          weather_.openAlertDetail();
        }
      }
      break;
    case Screen::Radar: {
      const auto rv = radar_.view();
      std::size_t nearest = 0;
      // Aircraft wins when both overlap; try blip first, then static marks.
      if (desk_ui::radar_lvgl_hit_blip(body_, rv, x, y, &nearest)) {
        if (radar_.hasSelection() && radar_.selectedIndex() == nearest) {
          radar_.clearSelection();
        } else {
          radar_.selectBlip(nearest);
        }
      } else if (desk_ui::radar_lvgl_hit_static(body_, rv, x, y, &nearest)) {
        if (rv.hasStaticSelection && rv.selectedStaticIndex == nearest) {
          radar_.clearStaticSelection();
        } else {
          radar_.selectStaticMark(nearest);
        }
      } else {
        radar_.clearSelection();
        radar_.clearStaticSelection();
      }
      break;
    }
    case Screen::Timezones:
      // Tap cycles anchor for sim convenience (device uses row tap)
      timezones_.onTapRow((timezones_.anchorIndex() + 1) % desk_display::kTimezoneBoardRows);
      break;
    default:
      break;
  }
}

void SimApp::on_double_tap_focused() {
  using desk_display::Screen;
  switch (nav_.focused()) {
    case Screen::Timezones:
      timezones_.onDoubleTap();
      break;
    case Screen::Weather:
      weather_.snapToNow();
      break;
    case Screen::Radar:
      radar_.clearSelection();
      radar_.clearStaticSelection();
      break;
    default:
      break;
  }
}

void SimApp::on_long_press_focused() {
  using desk_display::Screen;
  switch (nav_.focused()) {
    case Screen::Timezones:
      timezones_.onLongPress();
      break;
    case Screen::Radar:
      radar_.openSettings();
      break;
    default:
      break;
  }
}

void SimApp::handle_input() {
  const KeyEvents keys = hal_take_keys();
  if (keys.quit) {
    return;
  }

  const bool any = keys.rotate_delta != 0 || keys.center_tap || keys.tap ||
                   keys.double_tap || keys.long_press;
  if (!any) {
    return;
  }

  const auto prev_mode = nav_.mode();
  const auto prev_screen = nav_.active_screen();

  const bool radar_settings_ui =
      nav_.mode() == desk_display::NavMode::Focused &&
      nav_.focused() == desk_display::Screen::Radar && radar_.settingsOpen();

  if (keys.rotate_delta != 0 && !radar_settings_ui) {
    if (nav_.mode() == desk_display::NavMode::Carousel) {
      nav_.on_rotate(static_cast<int8_t>(keys.rotate_delta));
    } else {
      on_rotate_focused(keys.rotate_delta);
      nav_.idle_reset();
    }
  }
  if (keys.center_tap) {
    if (radar_settings_ui) {
      radar_.closeSettings();
      nav_.idle_reset();
    } else {
      if (nav_.mode() == desk_display::NavMode::Focused &&
          nav_.focused() == desk_display::Screen::Radar) {
        radar_.revertTempCenter();
      }
      nav_.on_center_tap();
    }
  }
  if (keys.tap) {
    if (radar_settings_ui) {
      const auto prev_settings = radar_.settings();
      const bool prev_demo = prev_settings.demoMode;
      if (desk_ui::radar_lvgl_settings_hit(keys.tap_x, keys.tap_y, radar_)) {
        nav_.idle_reset();
        const auto& cur = radar_.settings();
        const bool values_changed =
            cur.declutter != prev_settings.declutter ||
            cur.showAirports != prev_settings.showAirports ||
            cur.showAirspace != prev_settings.showAirspace ||
            cur.showRoads != prev_settings.showRoads || cur.demoMode != prev_demo;
        if (values_changed) {
          persist_radar_prefs();
          if (cur.demoMode && !prev_demo) {
            bind_demo_adsb_fixture();
          }
        }
      }
    } else {
      if (nav_.mode() == desk_display::NavMode::Focused) {
        on_tap_focused(keys.tap_x, keys.tap_y);
      }
      nav_.on_tap(keys.tap_x, keys.tap_y);
    }
  }
  if (keys.double_tap && !radar_settings_ui) {
    if (nav_.mode() == desk_display::NavMode::Focused) {
      on_double_tap_focused();
    }
    nav_.on_double_tap();
  }
  if (keys.long_press && !radar_settings_ui) {
    if (nav_.mode() == desk_display::NavMode::Focused) {
      on_long_press_focused();
      if (nav_.focused() == desk_display::Screen::Radar) {
        nav_.idle_reset();
      }
    }
    nav_.on_long_press();
  }

  if (nav_.mode() != prev_mode || nav_.active_screen() != prev_screen) {
    rebuild_ui_for_active();
  } else {
    refresh_content();
  }
}

void SimApp::update(uint32_t elapsed_ms) {
  using desk_display::AircraftList;
  using desk_display::Screen;

  static uint32_t clock_accum = 0;
  clock_accum += elapsed_ms;
  if (clock_accum >= 1000) {
    clock_accum = 0;
    sync_clock_from_wall();
    if (nav_.active_screen() == Screen::Clock ||
        nav_.active_screen() == Screen::Timezones) {
      refresh_content();
    }
  }

  const auto idle = nav_.on_tick(elapsed_ms);
  if (idle == desk_display::IdleEvent::SettleFocused) {
    settle_focused_screens();
  }
  // HomeToClock changes mode + active screen, which the checks below already
  // catch and route through rebuild_ui_for_active().

  // Poll adsb.lol only while Radar is the active screen; sweep animation
  // also only advances while Radar is visible.
  const bool radar_active = nav_.active_screen() == Screen::Radar;
  const bool sports_active = nav_.active_screen() == Screen::Sports;
  const bool weather_active = nav_.active_screen() == Screen::Weather;
  const bool demo_mode = radar_.settings().demoMode;
  adsb_poll_.setActive(radar_active && !demo_mode);
  map_ctx_poll_.setActive(radar_active);
  scores_poll_.setActive(sports_active);
  weather_poll_.setActive(weather_active);
  if (radar_active) {
    radar_.onTick(elapsed_ms);
    const double lat = radar_.centerLat();
    const double lon = radar_.centerLon();
    const float range = radar_.rangeMiles();
    adsb_poll_.setCenter(lat, lon, range);
    map_ctx_poll_.setCenter(lat, lon, range);
  }
  adsb_poll_.onTick(elapsed_ms);
  map_ctx_poll_.onTick(elapsed_ms);
  scores_poll_.onTick(elapsed_ms);
  weather_poll_.onTick(elapsed_ms);

  AircraftList fresh{};
  const bool got_fresh = adsb_poll_.takeAircraft(fresh);
  if (got_fresh) {
    radar_.bind(fresh);
  }

  desk_display::MapContext mapCtx{};
  const bool got_map_ctx = map_ctx_poll_.takeContext(mapCtx);
  if (got_map_ctx) {
    radar_.bindMapContext(mapCtx);
  }

  desk_display::Scores freshScores{};
  const bool got_scores = scores_poll_.takeScores(freshScores);
  if (got_scores) {
    sports_.bind(freshScores);
  }

  desk_display::Weather freshWeather{};
  const bool got_weather = weather_poll_.takeWeather(freshWeather);
  if (got_weather) {
    weather_.bind(freshWeather);
  }

  if (nav_.active_screen() != last_screen_ || nav_.mode() != last_mode_) {
    rebuild_ui_for_active();
  } else if (idle == desk_display::IdleEvent::SettleFocused || got_fresh || got_map_ctx ||
             got_scores || got_weather || radar_active) {
    // Refresh on settle (so cleared overlays render), when live data lands,
    // and every tick while Radar is up so the sweep keeps moving.
    refresh_content();
  }
}

void SimApp::shutdown() {}

}  // namespace sim
