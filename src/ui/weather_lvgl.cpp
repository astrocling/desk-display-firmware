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

  // Force layout: parent is often lv_pct(100) and still 0x0 until updated
  // (same pitfall as radar_disc_px_for_parent).
  lv_obj_update_layout(parent);
  lv_coord_t pw = lv_obj_get_content_width(parent);
  lv_coord_t ph = lv_obj_get_content_height(parent);
  if (pw < 64) {
    pw = lv_obj_get_width(parent);
  }
  if (ph < 64) {
    ph = lv_obj_get_height(parent);
  }
  if (pw < 64) {
    pw = 360;
  }
  if (ph < 64) {
    ph = 360;
  }

  // Circular bezel inset ≈ inscribed-square margin: (D - D/√2)/2 ≈ 0.1464*D
  const lv_coord_t side = (pw < ph) ? pw : ph;
  lv_coord_t inset = static_cast<lv_coord_t>((side * 1464) / 10000);
  if (inset < 24) {
    inset = 24;
  }
  const lv_coord_t content_w = side - (inset * 2);
  const lv_coord_t alert_w = (content_w > 40) ? (content_w - 8) : content_w;

  char hl[48];
  std::snprintf(hl, sizeof(hl), "H %.0f  L %.0f", static_cast<double>(v.todayHigh),
                static_cast<double>(v.todayLow));
  lv_obj_t* hl_lab = lv_label_create(parent);
  lv_label_set_text(hl_lab, hl);
  lv_obj_set_style_text_color(hl_lab, rgb(desk_display::theme::kAccent), 0);
  lv_obj_set_style_text_font(hl_lab, &lv_font_montserrat_12, 0);
  lv_obj_align(hl_lab, LV_ALIGN_TOP_MID, 0, inset);

  lv_obj_t* when = lv_label_create(parent);
  lv_label_set_text(when, v.whenLabel);
  lv_obj_set_style_text_color(when, rgb(desk_display::theme::kDim), 0);
  lv_obj_set_style_text_font(when, &lv_font_montserrat_12, 0);
  lv_obj_align(when, LV_ALIGN_CENTER, 0, -56);

  // Icon + temp as direct children (no flex container) so a bad parent measure
  // cannot collapse the hero row to zero width.
  lv_obj_t* icon = lv_img_create(parent);
  lv_img_set_src(icon, weatherIconImg(v.icon));
  lv_obj_align(icon, LV_ALIGN_CENTER, -44, -8);

  char tbuf[32];
  std::snprintf(tbuf, sizeof(tbuf), "%.0f°", static_cast<double>(v.displayTemp));
  lv_obj_t* temp = lv_label_create(parent);
  lv_label_set_text(temp, tbuf);
  lv_obj_set_style_text_font(temp, &lv_font_montserrat_28, 0);
  lv_obj_set_style_text_color(temp, rgb(0xFFFFFF), 0);
  lv_obj_align(temp, LV_ALIGN_CENTER, 36, -8);

  char feels_buf[32];
  std::snprintf(feels_buf, sizeof(feels_buf), "feels %.0f°",
                static_cast<double>(v.feelsLike));
  lv_obj_t* feels = lv_label_create(parent);
  lv_label_set_text(feels, feels_buf);
  lv_obj_set_style_text_color(feels, rgb(desk_display::theme::kDim), 0);
  lv_obj_set_style_text_font(feels, &lv_font_montserrat_12, 0);
  lv_obj_align(feels, LV_ALIGN_CENTER, 0, 36);

  if (v.alertBadge) {
    lv_obj_t* alert = lv_label_create(parent);
    lv_label_set_text(alert,
                      v.alertDetailOpen && v.alertHeadline ? v.alertHeadline : "ALERT");
    lv_obj_set_style_text_color(alert, rgb(desk_display::theme::kAlert), 0);
    lv_obj_set_style_text_font(alert, &lv_font_montserrat_12, 0);
    lv_obj_set_width(alert, alert_w);
    lv_label_set_long_mode(alert, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(alert, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(alert, LV_ALIGN_CENTER, 0, 58);
  }
}

}  // namespace desk_ui
