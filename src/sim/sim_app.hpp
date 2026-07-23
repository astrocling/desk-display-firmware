#pragma once

#include "desk_display/adsb_poll.hpp"
#include "desk_display/nav.hpp"
#include "desk_display/screen_clock.hpp"
#include "desk_display/screen_radar.hpp"
#include "desk_display/screen_sports.hpp"
#include "desk_display/screen_timezones.hpp"
#include "desk_display/screen_weather.hpp"

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
  void sync_clock_from_wall();
  void rebuild_ui_for_active();
  void refresh_content();
  void on_rotate_focused(int delta);
  void on_tap_focused();
  void on_double_tap_focused();
  void on_long_press_focused();

  desk_display::Nav nav_;
  desk_display::ScreenClock clock_;
  desk_display::ScreenTimezones timezones_;
  desk_display::WeatherScreen weather_;
  desk_display::ScreenSports sports_;
  desk_display::ScreenRadar radar_;
  desk_display::AdsbPoller adsb_poll_;

  lv_obj_t* root_ = nullptr;
  lv_obj_t* content_ = nullptr;
  lv_obj_t* chrome_ = nullptr;  // mode / screen label
  lv_obj_t* body_ = nullptr;

  desk_display::Screen last_screen_ = desk_display::Screen::Count;
  desk_display::NavMode last_mode_ = desk_display::NavMode::Carousel;
};

}  // namespace sim
