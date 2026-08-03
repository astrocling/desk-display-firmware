#include "screen_stub_lvgl.hpp"

#include "desk_display/theme.hpp"

#include <cctype>
#include <cstring>

namespace desk_ui {
namespace {

constexpr lv_coord_t kStubDiscPx = 200;

lv_color_t rgb(uint32_t c) {
  return lv_color_make(static_cast<uint8_t>((c >> 16) & 0xFF),
                       static_cast<uint8_t>((c >> 8) & 0xFF),
                       static_cast<uint8_t>(c & 0xFF));
}

void copy_upper(char* dst, std::size_t dstLen, const char* src) {
  if (dstLen == 0) {
    return;
  }
  std::size_t i = 0;
  for (; src[i] != '\0' && i + 1 < dstLen; ++i) {
    dst[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(src[i])));
  }
  dst[i] = '\0';
}

}  // namespace

void screen_stub_lvgl_build(lv_obj_t* parent, const char* title) {
  lv_obj_t* disc = lv_obj_create(parent);
  lv_obj_set_size(disc, kStubDiscPx, kStubDiscPx);
  lv_obj_center(disc);
  lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(disc, rgb(desk_display::theme::kDim), 0);
  lv_obj_set_style_bg_opa(disc, LV_OPA_20, 0);
  lv_obj_set_style_border_width(disc, 0, 0);
  lv_obj_set_style_pad_all(disc, 0, 0);
  lv_obj_clear_flag(disc, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(disc, LV_OBJ_FLAG_CLICKABLE);

  char upper[32];
  copy_upper(upper, sizeof(upper), title != nullptr ? title : "");

  lv_obj_t* lab = lv_label_create(disc);
  lv_label_set_text(lab, upper);
  lv_obj_set_style_text_font(lab, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(lab, rgb(desk_display::theme::kAccent), 0);
  lv_obj_center(lab);
}

}  // namespace desk_ui
