#pragma once

#include "desk_display/nav.hpp"
#include "desk_display/screen_radar.hpp"

#include <cstdint>

namespace desk_display {

/**
 * Stable create/destroy/show/hide stubs for each screen.
 * Track C replaces bodies with LVGL implementations; signatures stay fixed.
 */
void screen_clock_create();
void screen_clock_destroy();
void screen_clock_show();
void screen_clock_hide();

void screen_timezones_create();
void screen_timezones_destroy();
void screen_timezones_show();
void screen_timezones_hide();

void screen_weather_create();
void screen_weather_destroy();
void screen_weather_show();
void screen_weather_hide();

void screen_sports_create();
void screen_sports_destroy();
void screen_sports_show();
void screen_sports_hide();

void screen_radar_create();
void screen_radar_destroy();
void screen_radar_show();
void screen_radar_hide();

/** Bind the live view-model; dial's strong impl renders from it on show(). */
void screen_radar_bind_model(ScreenRadar* model);
/** Forward elapsed ms to the bound model's sweep animation (no-op if stub). */
void screen_radar_on_tick(uint32_t elapsed_ms);

struct ScreenOps {
  void (*create)();
  void (*destroy)();
  void (*show)();
  void (*hide)();
};

/** Registry indexed by Screen (not Count). */
const ScreenOps &screen_ops(Screen screen);

}  // namespace desk_display
