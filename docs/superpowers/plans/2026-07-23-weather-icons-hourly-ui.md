# Weather Icons + Hourly Strip Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add Meteocons weather icons, `Current`/`6 PM` when-labels, and a 5-slot hourly strip to the weather screen (view-model + sim UI).

**Architecture:** Keep formatting and strip window math in `desk_display` (no LVGL). Vendor Meteocons monochrome SVGs as LVGL image C arrays under `src/sim/assets/weather/` (sim-only for now). Sim renders layout B + strip from `WeatherScreenView`.

**Tech Stack:** C++17, Unity tests (PlatformIO native), LVGL 8.4 + SDL sim, Bas Milius Meteocons (MIT)

## Global Constraints

- Day-only Meteocons monochrome icons (no night variants)
- When-label: `Current` or 12h short without minutes (`6 PM`, `12 AM`, `12 PM`)
- Feels-like always from current conditions
- Strip: 5 slots, centered on scrub (or index 0 when Now), clamp at ends
- No HTTP polling, no 5-day view, no severity colors
- `desk_display` must not depend on LVGL

---

### Task 1: Hourly time short format

**Files:**
- Modify: `lib/desk_display/include/desk_display/format_time.hpp`
- Modify: `lib/desk_display/src/format_time.cpp`
- Modify: `test/test_domain/test_main.cpp` (or add cases there / extend existing format tests)

**Interfaces:**
- Produces: `std::size_t format12HourShort(char* buf, std::size_t bufLen, int hour24);` → `"6 PM"`, `"12 AM"`, `"12 PM"`
- Produces: `bool parseHourlyIsoHour(const char* iso, int& hour24);` — extracts hour from `YYYY-MM-DDTHH:MM` or `...THH:MM:SS` / trailing `Z`

- [ ] **Step 1: Write failing tests** for `format12HourShort(0/12/18)` and `parseHourlyIsoHour("2026-07-23T18:00")`

- [ ] **Step 2: Run** `pio test -e native -f test_domain` (or the suite that owns format_time) — expect fail

- [ ] **Step 3: Implement** `format12HourShort` + `parseHourlyIsoHour`

- [ ] **Step 4: Re-run tests — pass**

- [ ] **Step 5: Commit** `feat: add short 12h weather hour formatting`

---

### Task 2: WeatherScreen whenLabel + strip slots

**Files:**
- Modify: `lib/desk_display/include/desk_display/screen_weather.hpp`
- Modify: `lib/desk_display/src/screen_weather.cpp`
- Modify: `test/test_screen_weather/test_main.cpp`

**Interfaces:**
- Produces on `WeatherScreenView`:
  - `char whenLabel[16];` — `"Current"` or short hour
  - `static constexpr std::size_t kWeatherStripSlots = 5;`
  - `struct WeatherStripSlot { bool valid; bool selected; float temp; WeatherIconId icon; char hourDigit[8]; };`
  - `WeatherStripSlot strip[kWeatherStripSlots];`
  - `std::size_t stripCount;`
- Strip window: focus = `0` if `showingNow` else `scrubIndex`; `start = clamp(focus - 2, 0, max(0, count-5))`; fill up to 5 slots; `selected` true only when slot index == scrubIndex (never when showingNow)
- `hourDigit`: 12h hour number only (`"6"`, `"12"`) for strip compactness

- [ ] **Step 1: Failing tests** — bind fixture; assert `whenLabel=="Current"`; rotate once; assert whenLabel from first hourly; assert stripCount==5, first selected after rotate(1), strip centered/clamped at end

- [ ] **Step 2: Run** `pio test -e native -f test_screen_weather` — fail

- [ ] **Step 3: Implement view fields + fill logic**

- [ ] **Step 4: Tests pass**

- [ ] **Step 5: Commit** `feat: weather whenLabel and hourly strip window`

---

### Task 3: Vendor Meteocons → LVGL assets

**Files:**
- Create: `assets/weather/THIRD_PARTY.md` (MIT notice, Bas Milius Meteocons)
- Create: `assets/weather/svg/*.svg` (10 icons)
- Create: `support/gen_weather_icons.py` (SVG→PNG→LVGL C)
- Create: `src/sim/assets/weather/*.c` + `src/sim/assets/weather/weather_img.h`
- Create: `src/sim/weather_img.cpp` + `src/sim/weather_img.hpp` — `const lv_img_dsc_t* weatherIconImg(WeatherIconId)`

**Mapping:**
| Id | File |
|----|------|
| Clear | clear-day |
| MostlyClear | partly-cloudy-day |
| Cloudy | cloudy |
| Fog | fog |
| Drizzle | drizzle |
| Rain | rain |
| Snow | snow |
| Showers | partly-cloudy-day-rain |
| Thunderstorm | thunderstorms |
| Unknown | not-available |

Rasterize at 40px (center) and 16px (strip) OR one 40px size and let LVGL zoom for strip.

- [ ] **Step 1: Copy SVGs from `@meteocons/svg-static` monochrome + THIRD_PARTY.md**

- [ ] **Step 2: Generate LVGL C arrays** (TRUE_COLOR_ALPHA, LV_COLOR_DEPTH 16)

- [ ] **Step 3: Implement `weatherIconImg` lookup**

- [ ] **Step 4: Ensure `env:sim` compiles assets** (`build_src_filter` already includes `sim/`)

- [ ] **Step 5: Commit** `feat: vendor Meteocons weather icons for LVGL`

---

### Task 4: Sim weather UI (layout B + strip)

**Files:**
- Modify: `src/sim/sim_app.cpp` weather case in rebuild body

**Layout:**
- H/L top
- `whenLabel` above center
- `lv_img` + large temp in a row
- `feels %.0f°` below (no condition text name)
- 5 strip slots near bottom; selected slot accent bg
- Alert above strip if present

- [ ] **Step 1: Wire icons + labels + strip in sim**

- [ ] **Step 2: Build** `pio run -e sim` — success

- [ ] **Step 3: Manual smoke** (or note for user): Focused Weather, rotate, snap

- [ ] **Step 4: Commit** `feat: render weather icons and hourly strip in sim`

---

## Spec coverage

| Spec item | Task |
|-----------|------|
| Layout B | 4 |
| Current / 6 PM | 1, 2, 4 |
| Hourly strip ~5 | 2, 4 |
| Meteocons MIT | 3 |
| Day-only mapping | 3 |
| View-model tested | 1, 2 |
| No LVGL in desk_display | 1–2 vs 3–4 split |
| Non-goals | omitted |
