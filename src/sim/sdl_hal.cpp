#include "sdl_hal.hpp"

#include <SDL.h>
#include <lvgl.h>

#include <cstdio>
#include <cstring>

namespace sim {
namespace {

SDL_Window* window_ = nullptr;
SDL_Renderer* renderer_ = nullptr;
SDL_Texture* texture_ = nullptr;

lv_disp_draw_buf_t draw_buf_;
lv_color_t buf1_[kDispW * 40];
lv_disp_drv_t disp_drv_;
lv_indev_drv_t indev_drv_;
lv_indev_t* mouse_indev_ = nullptr;

int mouse_x_ = 0;
int mouse_y_ = 0;
bool mouse_down_ = false;

KeyEvents pending_{};

void flush_cb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* color_p) {
  if (!texture_) {
    lv_disp_flush_ready(disp);
    return;
  }

  const int w = area->x2 - area->x1 + 1;
  const int h = area->y2 - area->y1 + 1;

  SDL_Rect rect{area->x1, area->y1, w, h};
  SDL_UpdateTexture(texture_, &rect, color_p, w * static_cast<int>(sizeof(lv_color_t)));
  lv_disp_flush_ready(disp);
}

void mouse_read_cb(lv_indev_drv_t* /*drv*/, lv_indev_data_t* data) {
  data->point.x = static_cast<lv_coord_t>(mouse_x_);
  data->point.y = static_cast<lv_coord_t>(mouse_y_);
  data->state = mouse_down_ ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void map_key(SDL_Keycode key, bool down) {
  if (!down) {
    return;
  }
  switch (key) {
    case SDLK_LEFT:
    case SDLK_LEFTBRACKET:
    case SDLK_a:
      pending_.rotate_delta -= 1;
      break;
    case SDLK_RIGHT:
    case SDLK_RIGHTBRACKET:
    case SDLK_d:
      pending_.rotate_delta += 1;
      break;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
    case SDLK_SPACE:
      pending_.center_tap = true;
      break;
    case SDLK_t:
      pending_.tap = true;
      break;
    case SDLK_y:
      pending_.double_tap = true;
      break;
    case SDLK_u:
      pending_.long_press = true;
      break;
    case SDLK_ESCAPE:
    case SDLK_q:
      pending_.quit = true;
      break;
    default:
      break;
  }
}

}  // namespace

bool hal_init() {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return false;
  }

  window_ = SDL_CreateWindow(
      "Desk Display Sim (360x360)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
      kDispW * kZoom, kDispH * kZoom, SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN);
  if (!window_) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    return false;
  }

  // macOS (esp. when launched from Terminal/PlatformIO): window can appear
  // without keyboard focus. Drain startup events, then force raise + focus.
  {
    SDL_Event evt;
    while (SDL_PollEvent(&evt)) {
    }
    SDL_RaiseWindow(window_);
    SDL_SetWindowInputFocus(window_);
    while (SDL_PollEvent(&evt)) {
    }
  }

  renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer_) {
    std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    return false;
  }
  SDL_RenderSetLogicalSize(renderer_, kDispW, kDispH);

  texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
                               kDispW, kDispH);
  if (!texture_) {
    std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
    return false;
  }

  lv_init();

  lv_disp_draw_buf_init(&draw_buf_, buf1_, nullptr, kDispW * 40);

  lv_disp_drv_init(&disp_drv_);
  disp_drv_.hor_res = kDispW;
  disp_drv_.ver_res = kDispH;
  disp_drv_.flush_cb = flush_cb;
  disp_drv_.draw_buf = &draw_buf_;
  lv_disp_drv_register(&disp_drv_);

  lv_indev_drv_init(&indev_drv_);
  indev_drv_.type = LV_INDEV_TYPE_POINTER;
  indev_drv_.read_cb = mouse_read_cb;
  mouse_indev_ = lv_indev_drv_register(&indev_drv_);

  std::fprintf(stdout,
               "Desk Display Sim\n"
               "  Click the window first if keys do nothing.\n"
               "  Left/Right or [/] or A/D  rotate\n"
               "  Enter/Space              center tap (knob click)\n"
               "  T                       tap\n"
               "  Y                       double-tap\n"
               "  U                       long-press\n"
               "  Esc/Q                   quit\n");
  std::fflush(stdout);
  return true;
}

bool hal_poll(bool& quit) {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    switch (e.type) {
      case SDL_QUIT:
        quit = true;
        pending_.quit = true;
        break;
      case SDL_KEYDOWN:
        if (!e.key.repeat) {
          map_key(e.key.keysym.sym, true);
        }
        break;
      case SDL_MOUSEBUTTONDOWN:
        if (e.button.button == SDL_BUTTON_LEFT) {
          mouse_down_ = true;
          float lx = 0, ly = 0;
          SDL_RenderWindowToLogical(renderer_, e.button.x, e.button.y, &lx, &ly);
          mouse_x_ = static_cast<int>(lx);
          mouse_y_ = static_cast<int>(ly);
        }
        break;
      case SDL_MOUSEBUTTONUP:
        if (e.button.button == SDL_BUTTON_LEFT) {
          mouse_down_ = false;
          float lx = 0, ly = 0;
          SDL_RenderWindowToLogical(renderer_, e.button.x, e.button.y, &lx, &ly);
          mouse_x_ = static_cast<int>(lx);
          mouse_y_ = static_cast<int>(ly);
        }
        break;
      case SDL_MOUSEMOTION: {
        float lx = 0, ly = 0;
        SDL_RenderWindowToLogical(renderer_, e.motion.x, e.motion.y, &lx, &ly);
        mouse_x_ = static_cast<int>(lx);
        mouse_y_ = static_cast<int>(ly);
        break;
      }
      default:
        break;
    }
  }
  quit = pending_.quit;
  return !quit;
}

KeyEvents hal_take_keys() {
  KeyEvents out = pending_;
  pending_ = KeyEvents{};
  return out;
}

void hal_loop_step(uint32_t elapsed_ms) {
  lv_tick_inc(elapsed_ms);
  lv_timer_handler();

  if (renderer_ && texture_) {
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
  }
}

void hal_shutdown() {
  if (texture_) {
    SDL_DestroyTexture(texture_);
    texture_ = nullptr;
  }
  if (renderer_) {
    SDL_DestroyRenderer(renderer_);
    renderer_ = nullptr;
  }
  if (window_) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
  SDL_Quit();
}

}  // namespace sim
