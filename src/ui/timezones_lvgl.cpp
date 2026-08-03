#include "timezones_lvgl.hpp"

#include "desk_display/theme.hpp"

#include <cstdio>

namespace desk_ui {
namespace {

constexpr lv_coord_t kStatusIconPx = 12;

lv_color_t rgb(uint32_t c) {
  return lv_color_make(static_cast<uint8_t>((c >> 16) & 0xFF),
                       static_cast<uint8_t>((c >> 8) & 0xFF),
                       static_cast<uint8_t>(c & 0xFF));
}

lv_obj_t* make_status_icon(lv_obj_t* parent, desk_display::TzRowStatus st) {
  if (st == desk_display::TzRowStatus::Night) {
    // Crescent: moon disc with a background-colored cutout.
    lv_obj_t* wrap = lv_obj_create(parent);
    lv_obj_set_size(wrap, kStatusIconPx, kStatusIconPx);
    lv_obj_set_style_bg_opa(wrap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(wrap, 0, 0);
    lv_obj_set_style_pad_all(wrap, 0, 0);
    lv_obj_clear_flag(wrap, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* moon = lv_obj_create(wrap);
    lv_obj_set_size(moon, kStatusIconPx, kStatusIconPx);
    lv_obj_set_style_radius(moon, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(moon, rgb(desk_display::theme::kStatusNight), 0);
    lv_obj_set_style_border_width(moon, 0, 0);
    lv_obj_set_style_pad_all(moon, 0, 0);
    lv_obj_align(moon, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t* cut = lv_obj_create(wrap);
    lv_obj_set_size(cut, kStatusIconPx - 2, kStatusIconPx - 2);
    lv_obj_set_style_radius(cut, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(cut, rgb(desk_display::theme::kBg), 0);
    lv_obj_set_style_border_width(cut, 0, 0);
    lv_obj_set_style_pad_all(cut, 0, 0);
    lv_obj_align(cut, LV_ALIGN_CENTER, 3, -1);
    return wrap;
  }

  lv_obj_t* dot = lv_obj_create(parent);
  lv_obj_set_size(dot, kStatusIconPx, kStatusIconPx);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(dot, 0, 0);
  lv_obj_set_style_pad_all(dot, 0, 0);
  const uint32_t color = (st == desk_display::TzRowStatus::Working)
                             ? desk_display::theme::kStatusWorking
                             : desk_display::theme::kStatusAwake;
  lv_obj_set_style_bg_color(dot, rgb(color), 0);
  return dot;
}

}  // namespace

void timezones_lvgl_build(lv_obj_t* parent, const desk_display::TimezoneBoardView& v,
                          lv_coord_t host_h) {
  constexpr lv_coord_t kRowPitch = 28;
  constexpr lv_coord_t kRowCount =
      static_cast<lv_coord_t>(desk_display::kTimezoneBoardRows);
  const lv_coord_t startY = (host_h - kRowCount * kRowPitch) / 2;
  for (std::size_t i = 0; i < desk_display::kTimezoneBoardRows; ++i) {
    const auto& row = v.rows[i];

    lv_obj_t* row_obj = lv_obj_create(parent);
    lv_obj_set_size(row_obj, LV_SIZE_CONTENT, kRowPitch);
    lv_obj_set_style_bg_opa(row_obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row_obj, 0, 0);
    lv_obj_set_style_pad_all(row_obj, 0, 0);
    lv_obj_set_style_pad_column(row_obj, 8, 0);
    lv_obj_clear_flag(row_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row_obj, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_obj, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_align(row_obj, LV_ALIGN_TOP_MID, 0,
                 static_cast<lv_coord_t>(startY + static_cast<lv_coord_t>(i) * kRowPitch));

    make_status_icon(row_obj, row.status);

    char line[80];
    std::snprintf(line, sizeof(line), "%s  %s%s", row.timeText, row.label,
                  row.isAnchor ? " *" : "");
    lv_obj_t* lab = lv_label_create(row_obj);
    lv_label_set_text(lab, line);
    lv_obj_set_style_text_font(lab, &lv_font_montserrat_12, 0);
    uint32_t textColor = desk_display::theme::kDim;
    if (row.status == desk_display::TzRowStatus::Night) {
      textColor = 0x4B5563;  // dimmer for night rows
    } else if (row.isAnchor) {
      textColor = desk_display::theme::kAccent;
    }
    lv_obj_set_style_text_color(lab, rgb(textColor), 0);
  }
}

}  // namespace desk_ui
