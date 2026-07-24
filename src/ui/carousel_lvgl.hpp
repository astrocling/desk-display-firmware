#pragma once

#include "desk_display/nav.hpp"

#include <lvgl.h>

namespace desk_ui {

/** Title label Y offset from top (px). */
constexpr lv_coord_t kCarouselTitleOffsetY = 14;

/** Dot row Y offset from bottom (px). */
constexpr lv_coord_t kCarouselDotsOffsetY = -16;

/** Inactive carousel dot diameter (px). */
constexpr lv_coord_t kCarouselInactiveDotPx = 6;

/** Active carousel dot diameter (px). */
constexpr lv_coord_t kCarouselActiveDotPx = 8;

/** Circular preview inset diameter (px). */
constexpr lv_coord_t kCarouselPreviewHostPx = 240;

/** Horizontal spacing between dot centers (px). */
constexpr lv_coord_t kCarouselDotSpacingPx = 14;

struct CarouselChrome {
  lv_obj_t* root;          // full 360 host, or nullptr if using parent
  lv_obj_t* title;
  lv_obj_t* dots[5];       // Screen::Count == 5
  lv_obj_t* preview_host;  // circular inset; screens mount here
};

/** Build title + dots + circular preview_host as children of `parent`. */
CarouselChrome carousel_lvgl_build(lv_obj_t* parent);

/** Update title text + which dot is lit for `highlighted` (0..Count-1). */
void carousel_lvgl_set_highlight(CarouselChrome& ui,
                                 desk_display::Screen highlighted);

/** Uppercase short name for title (CLOCK, TIMEZONES, …). */
const char* carousel_screen_title(desk_display::Screen s);

}  // namespace desk_ui
