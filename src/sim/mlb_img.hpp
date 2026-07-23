#pragma once

#include "lvgl.h"

/**
 * LVGL image descriptor for a vendored MLB team logo, keyed by ESPN
 * abbreviation (case-insensitive, e.g. "hou" or "HOU").
 * Returns nullptr when abbr is null, empty, or not one of the 30 bundled
 * teams.
 */
const lv_img_dsc_t* mlbTeamLogoImg(const char* abbr);
