#include "hal/nav_overlay.hpp"

#include <lvgl.h>

namespace desk_hal {
namespace {

lv_obj_t* s_label = nullptr;

}  // namespace

bool navOverlayInit() {
  if (lv_disp_get_default() == nullptr) {
    return false;
  }
  if (s_label == nullptr) {
    s_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_color(s_label, lv_color_hex(0xF5F5F5), 0);
    lv_obj_align(s_label, LV_ALIGN_TOP_MID, 0, 24);
  }
  return true;
}

void navOverlaySetText(const char* text) {
  if (s_label == nullptr || text == nullptr) {
    return;
  }
  lv_label_set_text(s_label, text);
}

}  // namespace desk_hal
