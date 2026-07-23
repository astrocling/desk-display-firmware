/**
 * LVGL 8.4 config — shared by Dial (hardware) and SDL simulator.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

#define LV_MEM_CUSTOM 0
#ifndef LV_MEM_SIZE
#define LV_MEM_SIZE (96U * 1024U)
#endif

#define LV_DISP_DEF_REFR_PERIOD 16
#define LV_INDEV_DEF_READ_PERIOD 16

#define LV_DPI_DEF 130

#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MEM 1

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_USE_LABEL 1
#define LV_USE_BTN 1
#define LV_USE_IMG 1
#define LV_USE_ARC 1
#define LV_USE_LINE 1
#define LV_USE_KEYBOARD 1
#define LV_USE_TEXTAREA 1
#define LV_USE_ROLLER 1
#define LV_USE_BAR 1
#define LV_USE_SLIDER 0
#define LV_USE_SWITCH 0
#define LV_USE_CHECKBOX 0
#define LV_USE_DROPDOWN 0
#define LV_USE_CHART 0
#define LV_USE_TABLE 0
#define LV_USE_CALENDAR 0
#define LV_USE_MSGBOX 0
#define LV_USE_SPINBOX 0
#define LV_USE_TILEVIEW 0
#define LV_USE_TABVIEW 0
#define LV_USE_WIN 0
#define LV_USE_COLORWHEEL 0
#define LV_USE_LED 0
#define LV_USE_SPINNER 0
#define LV_USE_METER 0
#define LV_USE_MENU 0

#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1

#endif /* LV_CONF_H */
