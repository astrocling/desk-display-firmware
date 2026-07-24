#include "carousel_lvgl.hpp"

#include "desk_display/theme.hpp"

namespace desk_ui {
namespace {

constexpr int kDotCount = 5;

lv_color_t rgb(uint32_t c) {
  return lv_color_make(static_cast<uint8_t>((c >> 16) & 0xFF),
                       static_cast<uint8_t>((c >> 8) & 0xFF),
                       static_cast<uint8_t>(c & 0xFF));
}

void style_dot(lv_obj_t* dot, bool active) {
  const lv_coord_t px = active ? kCarouselActiveDotPx : kCarouselInactiveDotPx;
  const uint32_t color =
      active ? desk_display::theme::kAccent : desk_display::theme::kDim;

  lv_obj_set_size(dot, px, px);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dot, rgb(color), 0);
  lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(dot, 0, 0);
  lv_obj_set_style_pad_all(dot, 0, 0);
}

}  // namespace

const char* carousel_screen_title(desk_display::Screen s) {
  switch (s) {
    case desk_display::Screen::Clock:
      return "CLOCK";
    case desk_display::Screen::Timezones:
      return "TIMEZONES";
    case desk_display::Screen::Weather:
      return "WEATHER";
    case desk_display::Screen::Sports:
      return "SPORTS";
    case desk_display::Screen::Radar:
      return "RADAR";
    default:
      return "";
  }
}

CarouselChrome carousel_lvgl_build(lv_obj_t* parent) {
  CarouselChrome ui{};
  ui.root = nullptr;

  ui.title = lv_label_create(parent);
  lv_obj_set_style_text_font(ui.title, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(ui.title, rgb(desk_display::theme::kDim), 0);
  lv_obj_align(ui.title, LV_ALIGN_TOP_MID, 0, kCarouselTitleOffsetY);

  for (int i = 0; i < kDotCount; ++i) {
    ui.dots[i] = lv_obj_create(parent);
    style_dot(ui.dots[i], false);
    lv_obj_clear_flag(ui.dots[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(ui.dots[i], LV_OBJ_FLAG_CLICKABLE);
    const lv_coord_t x =
        static_cast<lv_coord_t>((i - (kDotCount - 1) / 2) * kCarouselDotSpacingPx);
    lv_obj_align(ui.dots[i], LV_ALIGN_BOTTOM_MID, x, kCarouselDotsOffsetY);
  }

  ui.preview_host = lv_obj_create(parent);
  lv_obj_set_size(ui.preview_host, kCarouselPreviewHostPx, kCarouselPreviewHostPx);
  lv_obj_center(ui.preview_host);
  lv_obj_set_style_radius(ui.preview_host, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_clip_corner(ui.preview_host, true, 0);
  lv_obj_set_style_bg_opa(ui.preview_host, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ui.preview_host, 0, 0);
  lv_obj_set_style_pad_all(ui.preview_host, 0, 0);
  lv_obj_clear_flag(ui.preview_host, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(ui.preview_host, LV_OBJ_FLAG_CLICKABLE);

  return ui;
}

void carousel_lvgl_set_highlight(CarouselChrome& ui,
                                 desk_display::Screen highlighted) {
  if (!ui.title) {
    return;
  }

  const uint8_t idx = static_cast<uint8_t>(highlighted);
  if (idx >= static_cast<uint8_t>(desk_display::Screen::Count)) {
    return;
  }

  lv_label_set_text(ui.title, carousel_screen_title(highlighted));
  lv_obj_set_style_text_color(ui.title, rgb(desk_display::theme::kAccent), 0);

  for (int i = 0; i < kDotCount; ++i) {
    if (!ui.dots[i]) {
      continue;
    }
    style_dot(ui.dots[i], i == static_cast<int>(idx));
    const lv_coord_t x =
        static_cast<lv_coord_t>((i - (kDotCount - 1) / 2) * kCarouselDotSpacingPx);
    lv_obj_align(ui.dots[i], LV_ALIGN_BOTTOM_MID, x, kCarouselDotsOffsetY);
  }
}

}  // namespace desk_ui
