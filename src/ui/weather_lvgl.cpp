#include "weather_lvgl.hpp"

#include "desk_display/theme.hpp"
#include "weather_img.hpp"

#include <cstdio>

namespace desk_ui {
namespace {

lv_color_t rgb(uint32_t c) {
  return lv_color_make(static_cast<uint8_t>((c >> 16) & 0xFF),
                       static_cast<uint8_t>((c >> 8) & 0xFF),
                       static_cast<uint8_t>(c & 0xFF));
}

}  // namespace

void weather_lvgl_build(lv_obj_t* parent, const desk_display::WeatherScreenView& v) {
  if (!v.ready) {
    lv_obj_t* lab = lv_label_create(parent);
    lv_label_set_text(lab, "Weather not ready");
    lv_obj_center(lab);
    return;
  }

  char hl[48];
  std::snprintf(hl, sizeof(hl), "H %.0f  L %.0f", static_cast<double>(v.todayHigh),
                static_cast<double>(v.todayLow));
  lv_obj_t* hl_lab = lv_label_create(parent);
  lv_label_set_text(hl_lab, hl);
  lv_obj_set_style_text_color(hl_lab, rgb(desk_display::theme::kAccent), 0);
  lv_obj_set_style_text_font(hl_lab, &lv_font_montserrat_12, 0);
  lv_obj_align(hl_lab, LV_ALIGN_TOP_MID, 0, 4);

  lv_obj_t* when = lv_label_create(parent);
  lv_label_set_text(when, v.whenLabel);
  lv_obj_set_style_text_color(when, rgb(desk_display::theme::kDim), 0);
  lv_obj_set_style_text_font(when, &lv_font_montserrat_12, 0);
  lv_obj_align(when, LV_ALIGN_CENTER, 0, -70);

  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_set_size(row, 200, 48);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, -28);

  lv_obj_t* icon = lv_img_create(row);
  lv_img_set_src(icon, weatherIconImg(v.icon));

  char tbuf[32];
  std::snprintf(tbuf, sizeof(tbuf), "%.0f°", static_cast<double>(v.displayTemp));
  lv_obj_t* temp = lv_label_create(row);
  lv_label_set_text(temp, tbuf);
  lv_obj_set_style_text_font(temp, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(temp, rgb(0xFFFFFF), 0);

  char feels_buf[32];
  std::snprintf(feels_buf, sizeof(feels_buf), "feels %.0f°",
                static_cast<double>(v.feelsLike));
  lv_obj_t* feels = lv_label_create(parent);
  lv_label_set_text(feels, feels_buf);
  lv_obj_set_style_text_color(feels, rgb(desk_display::theme::kDim), 0);
  lv_obj_set_style_text_font(feels, &lv_font_montserrat_12, 0);
  lv_obj_align(feels, LV_ALIGN_CENTER, 0, 10);

  if (v.alertBadge) {
    lv_obj_t* alert = lv_label_create(parent);
    lv_label_set_text(alert,
                      v.alertDetailOpen && v.alertHeadline ? v.alertHeadline : "ALERT");
    lv_obj_set_style_text_color(alert, rgb(desk_display::theme::kAlert), 0);
    lv_obj_set_style_text_font(alert, &lv_font_montserrat_12, 0);
    lv_obj_align(alert, LV_ALIGN_CENTER, 0, 36);
  }

  lv_obj_t* strip = lv_obj_create(parent);
  lv_obj_set_size(strip, 280, 70);
  lv_obj_set_style_bg_opa(strip, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(strip, 0, 0);
  lv_obj_set_style_pad_all(strip, 0, 0);
  lv_obj_clear_flag(strip, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(strip, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_align(strip, LV_ALIGN_BOTTOM_MID, 0, -4);

  for (std::size_t i = 0; i < v.stripCount; ++i) {
    const auto& slot = v.strip[i];
    if (!slot.valid) {
      continue;
    }
    lv_obj_t* cell = lv_obj_create(strip);
    lv_obj_set_size(cell, 48, 66);
    lv_obj_set_style_radius(cell, 6, 0);
    lv_obj_set_style_border_width(cell, 0, 0);
    lv_obj_set_style_pad_all(cell, 2, 0);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    if (slot.selected) {
      lv_obj_set_style_bg_color(cell, rgb(desk_display::theme::kAccent), 0);
      lv_obj_set_style_bg_opa(cell, LV_OPA_30, 0);
    } else {
      lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, 0);
    }
    lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cell, 2, 0);

    lv_obj_t* simg = lv_img_create(cell);
    lv_img_set_src(simg, weatherIconImg(slot.icon));
    lv_img_set_zoom(simg, 102);  // ~16px from 40px (256 = 100%)

    lv_obj_t* hour_lab = lv_label_create(cell);
    lv_label_set_text(hour_lab, slot.hourDigit);
    lv_obj_set_style_text_font(hour_lab, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hour_lab, rgb(desk_display::theme::kDim), 0);

    char st[16];
    std::snprintf(st, sizeof(st), "%.0f°", static_cast<double>(slot.temp));
    lv_obj_t* stemp = lv_label_create(cell);
    lv_label_set_text(stemp, st);
    lv_obj_set_style_text_font(stemp, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(stemp, rgb(0xFFFFFF), 0);
  }
}

}  // namespace desk_ui
