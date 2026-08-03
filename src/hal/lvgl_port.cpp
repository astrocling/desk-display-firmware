#include "hal/lvgl_port.hpp"

#include "hal/display.hpp"

#include "desk_display/theme.hpp"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <lvgl.h>

namespace desk_hal {
namespace {

constexpr size_t kDrawBufferPixels =
    static_cast<size_t>(kLcdWidth) * 40;
constexpr uint32_t kWarningIntervalMs = 1000;

lv_color_t* s_drawBufferMemory = nullptr;
lv_disp_draw_buf_t s_drawBuffer;
lv_disp_drv_t s_displayDriver;
esp_timer_handle_t s_tickTimer = nullptr;
bool s_initialized = false;
uint32_t s_lastFlushFailureWarningMs = 0;
bool s_flushFailureWarningLogged = false;

void flushDisplay(lv_disp_drv_t* driver, const lv_area_t* area,
                  lv_color_t* colors) {
  if (!displayFlush(area->x1, area->y1, area->x2, area->y2,
                    reinterpret_cast<const uint16_t*>(colors))) {
    const uint32_t now = millis();
    if (!s_flushFailureWarningLogged ||
        now - s_lastFlushFailureWarningMs >= kWarningIntervalMs) {
      Serial.println("lvgl: display flush failed");
      s_lastFlushFailureWarningMs = now;
      s_flushFailureWarningLogged = true;
    }
  }
  lv_disp_flush_ready(driver);
}

void incrementTick(void*) { lv_tick_inc(1); }

}  // namespace

bool lvglPortInit() {
  if (s_initialized) {
    return true;
  }
  if (!displayInit()) {
    return false;
  }

  lv_init();

  const size_t drawBytes = kDrawBufferPixels * sizeof(lv_color_t);
  s_drawBufferMemory = static_cast<lv_color_t*>(heap_caps_malloc(
      drawBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  if (s_drawBufferMemory == nullptr) {
    Serial.printf(
        "lvgl: draw buf alloc failed (%u B); free internal=%u largest=%u\n",
        static_cast<unsigned>(drawBytes),
        static_cast<unsigned>(
            heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
        static_cast<unsigned>(
            heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)));
    return false;
  }

  lv_disp_draw_buf_init(&s_drawBuffer, s_drawBufferMemory, nullptr,
                        kDrawBufferPixels);
  lv_disp_drv_init(&s_displayDriver);
  s_displayDriver.hor_res = kLcdWidth;
  s_displayDriver.ver_res = kLcdHeight;
  s_displayDriver.flush_cb = flushDisplay;
  s_displayDriver.draw_buf = &s_drawBuffer;
  if (lv_disp_drv_register(&s_displayDriver) == nullptr) {
    heap_caps_free(s_drawBufferMemory);
    s_drawBufferMemory = nullptr;
    return false;
  }

  const esp_timer_create_args_t tickTimerArgs = {
      .callback = incrementTick,
      .arg = nullptr,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "lvgl_tick",
      .skip_unhandled_events = true,
  };
  if (esp_timer_create(&tickTimerArgs, &s_tickTimer) != ESP_OK ||
      esp_timer_start_periodic(s_tickTimer, 1000) != ESP_OK) {
    return false;
  }

  lv_obj_set_style_bg_color(lv_scr_act(),
                            lv_color_hex(desk_display::theme::kBg), 0);
  lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);
  lv_obj_invalidate(lv_scr_act());

  s_initialized = true;
  return true;
}

void lvglPortHandler() {
  if (s_initialized) {
    lv_timer_handler();
  }
}

}  // namespace desk_hal
