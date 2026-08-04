# Weather UI Circle Layout Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redesign the weather screen for the circular dial by dropping the five-slot hourly strip and recentering icon+temp (layout B) inside a bezel-safe inset, while keeping encoder scrub.

**Architecture:** Keep scrub/`whenLabel`/alert in `WeatherScreen` (no LVGL). Delete strip window math from the view-model. Rebuild `weather_lvgl_build` as a single shared layout (carousel + focused) with radar-style circular insets, width-clamped alert text, and no strip widgets.

**Tech Stack:** C++17, LVGL 8.4, Unity (`pio test -e native`), PlatformIO `native` / `sim` / `dial`

**Spec:** [`docs/superpowers/specs/2026-08-04-weather-ui-circle-layout-design.md`](../specs/2026-08-04-weather-ui-circle-layout-design.md)

## Global Constraints

- Same layout for carousel and focused (no mode-specific weather build)
- Keep scrub, idle settle, snap, and alert open/close behavior unchanged
- Feels-like remains **current** feels from the API (not per-hour)
- Do not enable new Montserrat sizes — `lv_conf.h` only has up through 28; keep `lv_font_montserrat_28` for temp
- `desk_display` must not depend on LVGL
- Out of scope: radar, new icons, daily forecast, alert severity colors, input remapping
- Do not edit deprecated sim-only docs beyond what this plan touches; `weather_lvgl` is shared by dial + sim

## File map

| File | Responsibility |
|------|----------------|
| `lib/desk_display/include/desk_display/screen_weather.hpp` | Drop strip types/fields from `WeatherScreenView` |
| `lib/desk_display/src/screen_weather.cpp` | Drop `fillStrip` / `fillHourDigit`; stop filling strip in `view()` |
| `test/test_screen_weather/test_main.cpp` | Remove strip-window test; keep scrub/when/alert tests |
| `src/ui/weather_lvgl.cpp` | Icon+temp row layout; circular inset; no strip; alert wrap |
| `src/ui/weather_lvgl.hpp` | Update comment (no hourly strip) |

---

### Task 1: Remove hourly strip from view-model + tests

**Files:**
- Modify: `lib/desk_display/include/desk_display/screen_weather.hpp`
- Modify: `lib/desk_display/src/screen_weather.cpp`
- Modify: `test/test_screen_weather/test_main.cpp`
- Test: `pio test -e native -f test_screen_weather`

**Interfaces:**
- Consumes: existing `WeatherScreen` scrub API (`onRotate`, `snapToNow`, `whenLabel`, `displayTemp`, `icon`)
- Produces: `WeatherScreenView` without `strip`, `stripCount`, `WeatherStripSlot`, or `kWeatherStripSlots`

- [ ] **Step 1: Delete the strip unit test and its `RUN_TEST`**

In `test/test_screen_weather/test_main.cpp`, remove `test_strip_window_now_and_scrub` entirely (lines ~193–219) and remove `RUN_TEST(test_strip_window_now_and_scrub);` from `main`.

Optionally reword the comment in `test_scrub_hourly_updates_center` from “Clamp at end of strip” to “Clamp at end of hourly series”.

- [ ] **Step 2: Strip strip types from the header**

In `lib/desk_display/include/desk_display/screen_weather.hpp`:

1. Delete `kWeatherStripSlots`, `WeatherStripSlot`, and the `strip` / `stripCount` members of `WeatherScreenView`.
2. Update the class comment from “rotate scrubs the hourly strip” to “rotate scrubs hourly forecast (center display)”.

Resulting `WeatherScreenView` fields (keep these):

```cpp
struct WeatherScreenView {
  bool ready;
  bool showingNow;
  int scrubIndex;

  float displayTemp;
  float feelsLike;
  int weatherCode;
  WeatherIconId icon;

  char whenLabel[16];

  float todayHigh;
  float todayLow;

  const WeatherHourly* hourly;
  std::size_t hourlyCount;

  bool alertBadge;
  bool alertDetailOpen;
  const char* alertSeverity;
  const char* alertHeadline;
};
```

- [ ] **Step 3: Remove strip fill from the implementation**

In `lib/desk_display/src/screen_weather.cpp`:

1. Delete `fillHourDigit` and `fillStrip` entirely.
2. Remove `#include <algorithm>` if it becomes unused (it was only used by `fillStrip`).
3. In `WeatherScreen::view()`, delete the `fillStrip(v, weather_, scrubIndex_);` call. Keep `fillWhenLabel`.

- [ ] **Step 4: Run unit tests**

```bash
~/.platformio/penv/bin/pio test -e native -f test_screen_weather
```

Expected: PASS (all remaining tests green; strip test gone).

If the PlatformIO path differs on this machine, use whatever `pio` is on `PATH`.

- [ ] **Step 5: Commit**

```bash
git add lib/desk_display/include/desk_display/screen_weather.hpp \
        lib/desk_display/src/screen_weather.cpp \
        test/test_screen_weather/test_main.cpp
git commit -m "$(cat <<'EOF'
refactor(weather): drop hourly strip from view-model

EOF
)"
```

---

### Task 2: Rebuild `weather_lvgl` for circular safe area (no strip)

**Files:**
- Modify: `src/ui/weather_lvgl.cpp`
- Modify: `src/ui/weather_lvgl.hpp` (comment only)
- Depends on: Task 1 (no `v.strip*` references)

**Interfaces:**
- Consumes: `WeatherScreenView` without strip fields; `weatherIconImg`; theme colors
- Produces: `void weather_lvgl_build(lv_obj_t* parent, const WeatherScreenView& v)` rendering H/L, when, icon+temp, feels, optional alert

- [ ] **Step 1: Update the header comment**

In `src/ui/weather_lvgl.hpp`, change the doc comment to:

```cpp
/** Build the weather screen (temp, icon, feels, alert) under `parent`. */
```

- [ ] **Step 2: Replace `weather_lvgl_build` body**

Rewrite `src/ui/weather_lvgl.cpp` so the ready path matches this structure (keep the existing `rgb` helper and not-ready label):

```cpp
void weather_lvgl_build(lv_obj_t* parent, const desk_display::WeatherScreenView& v) {
  if (!v.ready) {
    lv_obj_t* lab = lv_label_create(parent);
    lv_label_set_text(lab, "Weather not ready");
    lv_obj_center(lab);
    return;
  }

  // Circular bezel inset ≈ inscribed-square margin: (D - D/√2)/2 ≈ 0.1464*D
  // (same idea as radar_lvgl kSettingsRingInset = 52 on a 360 disc).
  const lv_coord_t pw = lv_obj_get_content_width(parent);
  const lv_coord_t ph = lv_obj_get_content_height(parent);
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

  lv_obj_t* row = lv_obj_create(parent);
  lv_obj_set_size(row, content_w > 200 ? 200 : content_w, 48);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, 8, 0);
  lv_obj_align(row, LV_ALIGN_CENTER, 0, -12);

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
  lv_obj_align(feels, LV_ALIGN_CENTER, 0, 28);

  if (v.alertBadge) {
    lv_obj_t* alert = lv_label_create(parent);
    lv_label_set_text(alert,
                      v.alertDetailOpen && v.alertHeadline ? v.alertHeadline : "ALERT");
    lv_obj_set_style_text_color(alert, rgb(desk_display::theme::kAlert), 0);
    lv_obj_set_style_text_font(alert, &lv_font_montserrat_12, 0);
    lv_obj_set_width(alert, alert_w);
    lv_label_set_long_mode(alert, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(alert, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(alert, LV_ALIGN_CENTER, 0, 52);
  }

  // No hourly strip — scrub feedback is whenLabel + center icon/temp.
}
```

Notes for the implementer:

- Do **not** reference `v.strip`, `v.stripCount`, or create a bottom strip container.
- Do **not** add `LV_FONT_MONTSERRAT_36` — stay on 28.
- Center Y offsets (`-56`, `-12`, `28`, `52`) are chosen for a 360 host with ~52px inset; on 240 carousel they compress toward the middle of the smaller disc, which is acceptable for the shared layout.
- If visual check shows H/L or alert still kissing the bezel on Dial, bump `inset` floor or move alert to `LV_ALIGN_BOTTOM_MID` with `y = -inset` instead of center `+52`.

- [ ] **Step 3: Confirm no remaining strip references in UI**

```bash
rg "stripCount|WeatherStrip|fillStrip|kWeatherStrip" src lib test
```

Expected: no matches (docs under `docs/` may still mention the old strip historically — leave those alone unless this change is in-scope).

- [ ] **Step 4: Build Dial (and sim if convenient)**

```bash
~/.platformio/penv/bin/pio run -e dial
```

Expected: SUCCESS compile.

Optional:

```bash
~/.platformio/penv/bin/pio run -e sim
```

- [ ] **Step 5: Commit**

```bash
git add src/ui/weather_lvgl.cpp src/ui/weather_lvgl.hpp
git commit -m "$(cat <<'EOF'
fix(ui): weather layout for circular dial without hourly strip

EOF
)"
```

---

### Task 3: Visual verification on device / sim

**Files:**
- None required (manual check). Touch up `weather_lvgl.cpp` offsets only if clipping remains.

**Interfaces:**
- Consumes: Task 2 layout
- Produces: confirmed no bezel clipping; scrub still readable via when-label

- [ ] **Step 1: Focused Weather**

On Dial (or sim): enter Focused Weather.

Check:

- H/L fully visible inside the circle
- Icon + temp + feels fully visible
- No leftover strip / empty bottom is intentional
- If an alert is present (or force via fixture/API), badge and wrapped headline stay inside the disc

- [ ] **Step 2: Carousel preview**

In Carousel, highlight Weather. Preview should show the same composition scaled into the 240 preview without cut-off corners on the main content.

- [ ] **Step 3: Scrub**

In Focused Weather, rotate: when-label flips `Current` → hour labels; center temp/icon track scrub; idle settle returns to `Current`.

- [ ] **Step 4: Re-run unit tests after any offset tweaks**

```bash
~/.platformio/penv/bin/pio test -e native -f test_screen_weather
```

Expected: PASS.

- [ ] **Step 5: Commit offset tweaks if any**

```bash
git add src/ui/weather_lvgl.cpp
git commit -m "$(cat <<'EOF'
fix(ui): tune weather circular insets after visual check

EOF
)"
```

If no tweaks were needed, skip this commit.

---

## Spec coverage checklist

| Spec requirement | Task |
|------------------|------|
| Drop multi-slot hourly strip from UI | 2 |
| Keep scrub via rotate + when-label | 1 (logic), 3 (verify) |
| Same layout carousel + focused | 2 |
| Circular safe inset for H/L + alert | 2 |
| Alert width clamp / wrap | 2 |
| Remove strip view-model API | 1 |
| Update strip unit tests | 1 |
| Feels = current only | unchanged (verify in 3) |
| No new fonts / no radar / no input changes | Global Constraints |

## Self-review notes

- Spec mentioned optionally bumping temp toward montserrat 36; `lv_conf.h` does not enable it — plan keeps 28 (explicit constraint).
- No placeholders; exact commands and full `weather_lvgl_build` body included.
- Type names match Task 1 → Task 2 (`WeatherScreenView` without strip fields).
