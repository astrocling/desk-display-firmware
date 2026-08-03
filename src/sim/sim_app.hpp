#pragma once

#include "desk_display/adsb_poll.hpp"
#include "desk_display/map_context_poll.hpp"
#include "desk_display/nav.hpp"
#include "desk_display/scores_poll.hpp"
#include "desk_display/screen_clock.hpp"
#include "desk_display/screen_radar.hpp"
#include "desk_display/screen_sports.hpp"
#include "desk_display/screen_timezones.hpp"
#include "desk_display/screen_weather.hpp"
#include "desk_display/weather_poll.hpp"

#include "../ui/carousel_lvgl.hpp"

#include <lvgl.h>

namespace sim {

class SimApp {
 public:
  bool init();
  void update(uint32_t elapsed_ms);
  void handle_input();
  void shutdown();

 private:
  void load_fixtures();
  void persist_radar_prefs();
  void bind_demo_adsb_fixture();
  void sync_clock_from_wall();
  void rebuild_ui_for_active();
  void refresh_content();
  void settle_focused_screens();
  void on_rotate_focused(int delta);
  void on_tap_focused(int16_t x, int16_t y);
  void on_double_tap_focused();
  void on_long_press_focused();

  desk_display::Nav nav_;
  desk_display::ScreenClock clock_;
  desk_display::ScreenTimezones timezones_;
  desk_display::WeatherScreen weather_;
  desk_display::ScreenSports sports_;
  desk_display::ScreenRadar radar_;
  desk_display::AdsbPoller adsb_poll_;
  desk_display::MapContextPoller map_ctx_poll_;
  desk_display::ScoresPoller scores_poll_;
  desk_display::WeatherPoller weather_poll_;

  lv_obj_t* root_ = nullptr;
  lv_obj_t* carousel_root_ = nullptr;
  desk_ui::CarouselChrome carousel_{};
  lv_obj_t* focused_host_ = nullptr;
  lv_obj_t* body_ = nullptr;  // child of carousel_.preview_host or focused_host_

  desk_display::Screen last_screen_ = desk_display::Screen::Count;
  desk_display::NavMode last_mode_ = desk_display::NavMode::Carousel;
};

}  // namespace sim
