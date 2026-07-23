#pragma once

#include <cstdint>

namespace sim {

constexpr int kDispW = 360;
constexpr int kDispH = 360;
constexpr int kZoom = 2;  // window pixels = disp * zoom

/** Create SDL window + LVGL display/mouse. Returns false on failure. */
bool hal_init();

/** Pump SDL events; update mouse; map keys into out flags. Returns false if quit. */
bool hal_poll(bool& quit);

struct KeyEvents {
  int rotate_delta;  // encoder ticks this frame
  bool center_tap;
  bool tap;
  int16_t tap_x;  // mouse position (display px) at time of `tap`
  int16_t tap_y;
  bool double_tap;
  bool long_press;
  bool quit;
};

/** Drain keyboard-mapped input since last call. */
KeyEvents hal_take_keys();

/** Present LVGL frame (tick + handler + SDL present). */
void hal_loop_step(uint32_t elapsed_ms);

void hal_shutdown();

}  // namespace sim
