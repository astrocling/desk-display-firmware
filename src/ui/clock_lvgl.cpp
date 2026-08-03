#include "clock_lvgl.hpp"

#include "desk_display/format_time.hpp"
#include "desk_display/theme.hpp"

namespace desk_ui {
namespace {

lv_color_t rgb(uint32_t c) {
  return lv_color_make(static_cast<uint8_t>((c >> 16) & 0xFF),
                       static_cast<uint8_t>((c >> 8) & 0xFF),
                       static_cast<uint8_t>(c & 0xFF));
}

}  // namespace

void clock_lvgl_build(lv_obj_t* parent, const desk_display::ClockView& v) {
  lv_obj_t* date = lv_label_create(parent);
  lv_label_set_text(date, v.dateText);
  lv_obj_set_style_text_color(date, rgb(desk_display::theme::kDim), 0);
  lv_obj_align(date, LV_ALIGN_CENTER, 0, 40);

  char timebuf[20];
  desk_display::format12HourWithSeconds(timebuf, sizeof(timebuf), v.hour, v.minute, v.second);
  lv_obj_t* time = lv_label_create(parent);
  lv_label_set_text(time, timebuf);
  lv_obj_set_style_text_font(time, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(time, rgb(0xFFFFFF), 0);
  lv_obj_align(time, LV_ALIGN_CENTER, 0, -10);

  // Analog ticks via arcs as stand-in for hands
  lv_obj_t* face = lv_arc_create(parent);
  lv_obj_set_size(face, 200, 200);
  lv_obj_center(face);
  lv_arc_set_bg_angles(face, 0, 360);
  lv_arc_set_value(face, 0);
  lv_obj_remove_style(face, nullptr, LV_PART_KNOB);
  lv_obj_clear_flag(face, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_color(face, rgb(desk_display::theme::kDim), LV_PART_MAIN);
  lv_obj_set_style_arc_width(face, 2, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(face, LV_OPA_40, LV_PART_INDICATOR);

  if (v.timezoneBoardHint) {
    lv_obj_t* hint = lv_label_create(parent);
    lv_label_set_text(hint, "→ Timezones");
    lv_obj_set_style_text_color(hint, rgb(desk_display::theme::kAccent), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -4);
  }
}

}  // namespace desk_ui
