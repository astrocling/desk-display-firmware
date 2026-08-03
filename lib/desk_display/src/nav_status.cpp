#include "desk_display/nav_status.hpp"

#include <cstdio>

namespace desk_display {

namespace {

const char* screenTitleSerial(Screen s) {
  switch (s) {
    case Screen::Clock:
      return "Clock";
    case Screen::Timezones:
      return "Timezones";
    case Screen::Weather:
      return "Weather";
    case Screen::Sports:
      return "Sports";
    case Screen::Radar:
      return "Radar";
    default:
      return "";
  }
}

}  // namespace

const char* screenTitleUpper(Screen s) {
  switch (s) {
    case Screen::Clock:
      return "CLOCK";
    case Screen::Timezones:
      return "TIMEZONES";
    case Screen::Weather:
      return "WEATHER";
    case Screen::Sports:
      return "SPORTS";
    case Screen::Radar:
      return "RADAR";
    default:
      return "";
  }
}

const char* navModeUpper(NavMode m) {
  switch (m) {
    case NavMode::Focused:
      return "FOCUSED";
    case NavMode::Carousel:
      return "CAROUSEL";
    default:
      return "";
  }
}

void formatNavOverlay(NavMode mode, Screen screen, char* buf, size_t buf_len) {
  if (buf_len == 0) {
    return;
  }
  std::snprintf(buf, buf_len, "%s %s", navModeUpper(mode), screenTitleUpper(screen));
}

void formatNavSerial(NavMode mode, Screen screen, char* buf, size_t buf_len) {
  if (buf_len == 0) {
    return;
  }
  const char* mode_title = (mode == NavMode::Focused) ? "Focused" : "Carousel";
  std::snprintf(buf, buf_len, "nav: %s %s", mode_title, screenTitleSerial(screen));
}

}  // namespace desk_display
