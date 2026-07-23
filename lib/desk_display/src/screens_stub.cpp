#include "desk_display/screens_stub.hpp"

namespace desk_display {

void screen_clock_create() {}
void screen_clock_destroy() {}
void screen_clock_show() {}
void screen_clock_hide() {}

void screen_timezones_create() {}
void screen_timezones_destroy() {}
void screen_timezones_show() {}
void screen_timezones_hide() {}

void screen_weather_create() {}
void screen_weather_destroy() {}
void screen_weather_show() {}
void screen_weather_hide() {}

void screen_sports_create() {}
void screen_sports_destroy() {}
void screen_sports_show() {}
void screen_sports_hide() {}

// Radar LVGL renderer lives in src/ui/radar_lvgl.cpp (sim links it; dial
// will use screen_radar_*). These weak no-ops satisfy the ScreenOps registry
// for native tests and any build that doesn't link src/ui/.
__attribute__((weak)) void screen_radar_create() {}
__attribute__((weak)) void screen_radar_destroy() {}
__attribute__((weak)) void screen_radar_show() {}
__attribute__((weak)) void screen_radar_hide() {}
__attribute__((weak)) void screen_radar_bind_model(ScreenRadar*) {}
__attribute__((weak)) void screen_radar_on_tick(uint32_t) {}

namespace {

const ScreenOps kOps[static_cast<uint8_t>(Screen::Count)] = {
    {screen_clock_create, screen_clock_destroy, screen_clock_show,
     screen_clock_hide},
    {screen_timezones_create, screen_timezones_destroy, screen_timezones_show,
     screen_timezones_hide},
    {screen_weather_create, screen_weather_destroy, screen_weather_show,
     screen_weather_hide},
    {screen_sports_create, screen_sports_destroy, screen_sports_show,
     screen_sports_hide},
    {screen_radar_create, screen_radar_destroy, screen_radar_show,
     screen_radar_hide},
};

}  // namespace

const ScreenOps &screen_ops(Screen screen) {
  return kOps[static_cast<uint8_t>(screen)];
}

}  // namespace desk_display
