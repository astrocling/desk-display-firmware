/**
 * Desk Display — SDL / LVGL desktop simulator entry point.
 *
 * Build & run:
 *   pio run -e sim -t upload
 */

#include "sdl_hal.hpp"
#include "sim_app.hpp"

#include <SDL.h>
#include <cstdio>

int main(int /*argc*/, char** /*argv*/) {
  if (!sim::hal_init()) {
    return 1;
  }

  sim::SimApp app;
  if (!app.init()) {
    sim::hal_shutdown();
    return 1;
  }

  bool quit = false;
  uint32_t last = SDL_GetTicks();
  while (!quit) {
    const uint32_t now = SDL_GetTicks();
    uint32_t elapsed = now - last;
    if (elapsed < 1) {
      elapsed = 1;
    }
    last = now;

    if (!sim::hal_poll(quit)) {
      break;
    }
    app.handle_input();
    app.update(elapsed);
    sim::hal_loop_step(elapsed);
  }

  app.shutdown();
  sim::hal_shutdown();
  return 0;
}
