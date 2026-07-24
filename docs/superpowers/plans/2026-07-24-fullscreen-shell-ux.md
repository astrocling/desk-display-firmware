# Full-Screen Shell UX Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Carousel a real browse frame (title + dots + live inset preview) and Focused a chrome-free full-bleed host, with Focused idle staying on the current app and settling ephemeral UI.

**Architecture:** Pure `Nav` signals idle outcomes (`HomeToClock` vs `SettleFocused`); screen view-models expose `onIdleSettle()`; sim shell owns `carousel_` vs `focused_host_` and mounts the same screen renderers into either the inset preview or the full round face. Center-tap mode switching and radar tap-select stay as today.

**Tech Stack:** C++17, Unity (`pio test -e native`), LVGL 8.4 + SDL sim

**Spec:** `docs/superpowers/specs/2026-07-24-fullscreen-shell-ux-design.md`

## Global Constraints

- Center-tap remains Carousel ↔ Focused (no new exit gesture)
- Carousel idle → Focused Clock; Focused idle → stay on app + settle
- Settle clears ephemeral UI only; keep radar range / pin; do not wipe pin on settle
- Radar rotate = zoom; tap-for-details unchanged this pass
- Remove persistent `Carousel · Screen` / `Focused · Screen` chrome label
- `desk_display` must not depend on LVGL
- Screens lay out relative to their parent (inset or full-bleed)

## File map

| File | Responsibility |
|------|----------------|
| `lib/desk_display/include/desk_display/nav.hpp` + `src/nav.cpp` | `IdleEvent` from `on_tick`; Focused idle stays |
| `test/test_nav/test_main.cpp` | Idle stay / home / event tests |
| `lib/desk_display/.../screen_weather.*` | `onIdleSettle()` → snap + close alert |
| `lib/desk_display/.../screen_radar.*` | `onIdleSettle()` → clearSelection only |
| `lib/desk_display/.../screen_sports.*` | `onIdleSettle()` → exitDetail |
| `lib/desk_display/.../screen_timezones.*` | `onIdleSettle()` → reset scrub/anchor |
| `test/test_screen_{weather,radar,sports,timezones}/` | Settle unit tests |
| `src/ui/carousel_lvgl.hpp` + `carousel_lvgl.cpp` | Title + dots frame around `preview_host_` |
| `src/sim/sim_app.hpp` + `sim_app.cpp` | Dual hosts; wire settle; drop chrome label |
| `src/ui/radar_lvgl.*` | Disc size from parent (full-bleed vs inset) |
| `docs/NAV.md`, `docs/SIM.md`, `docs/FIRMWARE_PLAN.md` | Document new shell + idle rules |

---

### Task 1: Nav idle — Focused stays, signal settle

**Files:**
- Modify: `lib/desk_display/include/desk_display/nav.hpp`
- Modify: `lib/desk_display/src/nav.cpp`
- Modify: `test/test_nav/test_main.cpp`

**Interfaces:**
- Produces:
  ```cpp
  enum class IdleEvent : uint8_t {
    None = 0,
    HomeToClock,     // was Carousel → now Focused Clock
    SettleFocused,   // was Focused non-home → stay; shell must settle screens
  };
  IdleEvent on_tick(uint32_t elapsed_ms);  // was void
  ```
- Consumes: existing `NavMode`, `Screen`, `kIdleTimeoutMs`

- [ ] **Step 1: Write failing tests** — replace / add in `test/test_nav/test_main.cpp`:

```cpp
void test_idle_timeout_from_carousel_homes_to_clock(void) {
  nav.on_center_tap();  // Carousel
  nav.on_rotate(2);     // Weather highlighted
  const desk_display::IdleEvent ev = nav.on_tick(kIdleTimeoutMs);
  TEST_ASSERT_EQUAL(static_cast<int>(desk_display::IdleEvent::HomeToClock),
                    static_cast<int>(ev));
  TEST_ASSERT_EQUAL(static_cast<int>(NavMode::Focused),
                    static_cast<int>(nav.mode()));
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Clock),
                    static_cast<int>(nav.focused()));
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Clock),
                    static_cast<int>(nav.highlighted()));
  TEST_ASSERT_EQUAL_UINT32(0, nav.idle_elapsed_ms());
}

void test_idle_timeout_focused_stays_and_settles(void) {
  nav.on_center_tap();
  nav.on_rotate(4);  // Radar
  nav.on_center_tap();
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Radar),
                    static_cast<int>(nav.focused()));

  const desk_display::IdleEvent ev = nav.on_tick(kIdleTimeoutMs);
  TEST_ASSERT_EQUAL(static_cast<int>(desk_display::IdleEvent::SettleFocused),
                    static_cast<int>(ev));
  TEST_ASSERT_EQUAL(static_cast<int>(NavMode::Focused),
                    static_cast<int>(nav.mode()));
  TEST_ASSERT_EQUAL(static_cast<int>(Screen::Radar),
                    static_cast<int>(nav.focused()));
  TEST_ASSERT_EQUAL_UINT32(0, nav.idle_elapsed_ms());
}

void test_idle_tick_none_before_timeout(void) {
  nav.on_center_tap();
  const desk_display::IdleEvent ev = nav.on_tick(kIdleTimeoutMs / 2);
  TEST_ASSERT_EQUAL(static_cast<int>(desk_display::IdleEvent::None),
                    static_cast<int>(ev));
  TEST_ASSERT_EQUAL(static_cast<int>(NavMode::Carousel),
                    static_cast<int>(nav.mode()));
}
```

Rename/remove `test_idle_timeout_returns_focused_clock` (old Focused→Clock behavior). Update `test_idle_timeout_from_carousel` to assert `HomeToClock` return (or delete if replaced). Update any `on_tick` call sites in this file that ignore the return value (C++ allows discarding).

Register new tests with `RUN_TEST(...)`.

- [ ] **Step 2: Run** `pio test -e native -f test_nav` — expect FAIL (missing `IdleEvent` / wrong Focused idle)

- [ ] **Step 3: Implement**

In `nav.hpp`, add `IdleEvent` and change `void on_tick` → `IdleEvent on_tick`.

In `nav.cpp`:

```cpp
IdleEvent Nav::on_tick(uint32_t elapsed_ms) {
  if (elapsed_ms == 0) {
    return IdleEvent::None;
  }

  // Focused Clock home: do not accumulate / re-fire.
  if (mode_ == NavMode::Focused && focused_ == Screen::Clock &&
      highlighted_ == Screen::Clock) {
    idle_elapsed_ms_ = 0;
    return IdleEvent::None;
  }

  const uint32_t room = kIdleTimeoutMs - idle_elapsed_ms_;
  if (elapsed_ms >= room) {
    idle_elapsed_ms_ = 0;
    if (mode_ == NavMode::Carousel) {
      apply_idle_home();
      return IdleEvent::HomeToClock;
    }
    // Focused: stay on current app; shell settles ephemeral UI.
    return IdleEvent::SettleFocused;
  }
  idle_elapsed_ms_ += elapsed_ms;
  return IdleEvent::None;
}
```

Keep `apply_idle_home()` as today (Focused Clock). Do **not** call it on Focused timeout.

- [ ] **Step 4: Run** `pio test -e native -f test_nav` — expect PASS

- [ ] **Step 5: Commit**

```bash
git add lib/desk_display/include/desk_display/nav.hpp lib/desk_display/src/nav.cpp test/test_nav/test_main.cpp
git commit -m "$(cat <<'EOF'
feat: keep Focused app on idle; signal settle vs home

EOF
)"
```

---

### Task 2: Screen `onIdleSettle()` APIs

**Files:**
- Modify: `lib/desk_display/include/desk_display/screen_weather.hpp` + `src/screen_weather.cpp`
- Modify: `lib/desk_display/include/desk_display/screen_radar.hpp` + `src/screen_radar.cpp`
- Modify: `lib/desk_display/include/desk_display/screen_sports.hpp` + `src/screen_sports.cpp`
- Modify: `lib/desk_display/include/desk_display/screen_timezones.hpp` + `src/screen_timezones.cpp`
- Modify: `test/test_screen_weather/test_main.cpp`
- Modify: `test/test_screen_radar/test_main.cpp`
- Modify: `test/test_screen_sports/test_main.cpp`
- Modify: `test/test_screen_timezones/test_main.cpp`

**Interfaces:**
- Produces (each screen):
  ```cpp
  void onIdleSettle();
  ```
- Weather: `snapToNow()` + `closeAlertDetail()`
- Radar: `clearSelection()` only (keep `rangeMiles_`, pin, temp center)
- Sports: `exitDetail()`
- Timezones: same as double-tap reset (`scrubSteps_ = 0`, Eastern anchor) — call existing `onDoubleTap()` body or shared private reset

- [ ] **Step 1: Write failing tests**

Weather (`test/test_screen_weather/test_main.cpp`) — after binding fixture weather and scrubbing + opening alert if available:

```cpp
void test_idle_settle_snaps_and_closes_alert(void) {
  // assume g_screen bound in setUp with hourly data
  g_screen.onRotate(1);
  TEST_ASSERT_FALSE(g_screen.view().showingNow);
  (void)g_screen.openAlertDetail();  // ok if false when no alert
  g_screen.onIdleSettle();
  TEST_ASSERT_TRUE(g_screen.view().showingNow);
  TEST_ASSERT_EQUAL(desk_display::kWeatherScrubNow, g_screen.scrubIndex());
  TEST_ASSERT_FALSE(g_screen.alertDetailOpen());
}
```

Radar:

```cpp
void test_idle_settle_clears_selection_keeps_range(void) {
  ScreenRadar screen;
  // bind sample, select blip 0, zoom out once
  screen.onRotate(1);
  const float rangeAfterZoom = screen.rangeMiles();
  TEST_ASSERT_TRUE(screen.selectBlip(0));
  TEST_ASSERT_TRUE(screen.hasSelection());
  screen.onIdleSettle();
  TEST_ASSERT_FALSE(screen.hasSelection());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, rangeAfterZoom, screen.rangeMiles());
}
```

Sports: enter detail, `onIdleSettle()`, assert `!view().detail`.

Timezones: scrub, `onIdleSettle()`, assert `scrubSteps() == 0`.

- [ ] **Step 2: Run** relevant `pio test -e native -f test_screen_*` — expect FAIL (missing `onIdleSettle`)

- [ ] **Step 3: Implement** the four `onIdleSettle()` methods as specified above

- [ ] **Step 4: Re-run** those test filters — expect PASS

- [ ] **Step 5: Commit**

```bash
git add lib/desk_display/include/desk_display/screen_weather.hpp \
  lib/desk_display/src/screen_weather.cpp \
  lib/desk_display/include/desk_display/screen_radar.hpp \
  lib/desk_display/src/screen_radar.cpp \
  lib/desk_display/include/desk_display/screen_sports.hpp \
  lib/desk_display/src/screen_sports.cpp \
  lib/desk_display/include/desk_display/screen_timezones.hpp \
  lib/desk_display/src/screen_timezones.cpp \
  test/test_screen_weather/test_main.cpp \
  test/test_screen_radar/test_main.cpp \
  test/test_screen_sports/test_main.cpp \
  test/test_screen_timezones/test_main.cpp
git commit -m "$(cat <<'EOF'
feat: add onIdleSettle to clear ephemeral screen UI

EOF
)"
```

---

### Task 3: Carousel LVGL frame component

**Files:**
- Create: `src/ui/carousel_lvgl.hpp`
- Create: `src/ui/carousel_lvgl.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace desk_ui {
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
  }
  ```
- Layout constants (lock in header):
  - Title: `TOP_MID`, y ≈ 14, montserrat 12, theme dim/accent
  - Dots: `BOTTOM_MID`, y ≈ −16; inactive dim 6px; active accent 8px
  - `preview_host`: size **240×240**, centered, `LV_RADIUS_CIRCLE`, `clip_corner`, transparent bg, no border/pad

- [ ] **Step 1: Implement** `carousel_lvgl.hpp` / `.cpp` with the API above. Map titles:

```cpp
case Screen::Clock: return "CLOCK";
case Screen::Timezones: return "TIMEZONES";
case Screen::Weather: return "WEATHER";
case Screen::Sports: return "SPORTS";
case Screen::Radar: return "RADAR";
```

- [ ] **Step 2: Ensure sim build lists new cpp** — if PlatformIO auto-globs `src/`, no change; otherwise add to env. Check with `pio run -e sim` compile (may fail until Task 4 wires it — if so, skip run until Task 4). Prefer implementing and committing the component alone if `src/**/*.cpp` is already globbed.

- [ ] **Step 3: Commit**

```bash
git add src/ui/carousel_lvgl.hpp src/ui/carousel_lvgl.cpp
git commit -m "$(cat <<'EOF'
feat: add Carousel title/dots/preview LVGL frame

EOF
)"
```

---

### Task 4: Sim shell — dual hosts + settle wiring

**Files:**
- Modify: `src/sim/sim_app.hpp`
- Modify: `src/sim/sim_app.cpp`

**Interfaces:**
- Consumes: `IdleEvent` from Task 1; `onIdleSettle()` from Task 2; `carousel_lvgl_*` from Task 3
- Replace `content_` + `chrome_` + single `body_` with:
  ```cpp
  lv_obj_t* carousel_root_ = nullptr;
  desk_ui::CarouselChrome carousel_{};
  lv_obj_t* focused_host_ = nullptr;
  lv_obj_t* body_ = nullptr;  // child of preview_host_ OR focused_host_
  ```

- [ ] **Step 1: Restructure `init()`**

```cpp
// Remove chrome_ label entirely.

carousel_root_ = lv_obj_create(root_);
lv_obj_set_size(carousel_root_, kDispW, kDispH);
// transparent, no pad/border, full size, clear scroll
carousel_ = desk_ui::carousel_lvgl_build(carousel_root_);

focused_host_ = lv_obj_create(root_);
lv_obj_set_size(focused_host_, kDispW, kDispH);
lv_obj_center(focused_host_);
// transparent, no pad/border, clear scroll — true full-bleed
```

- [ ] **Step 2: Mode visibility + body parent**

In `rebuild_ui_for_active()` / a new `sync_shell_hosts()`:

- If `nav_.mode() == Carousel`: show `carousel_root_`, hide `focused_host_`; parent `body_` under `carousel_.preview_host`; call `carousel_lvgl_set_highlight(carousel_, nav_.highlighted())`.
- If Focused: hide `carousel_root_`, show `focused_host_`; parent `body_` under `focused_host_`.

Always delete previous `body_` before recreate; call `radar_lvgl_invalidate()` when tearing down radar.

Remove all `snprintf` / `lv_label_set_text` for the old chrome string in `refresh_content()`.

- [ ] **Step 3: Wire idle in `update()`**

Wherever `nav_.on_tick(elapsed_ms)` is called:

```cpp
const auto idle = nav_.on_tick(elapsed_ms);
if (idle == desk_display::IdleEvent::SettleFocused) {
  settle_focused_screens();
} else if (idle == desk_display::IdleEvent::HomeToClock) {
  // optional: no settle required; rebuild will show Clock
}
if (idle != desk_display::IdleEvent::None) {
  rebuild_ui_for_active();  // or refresh if screen unchanged on settle
}
```

```cpp
void SimApp::settle_focused_screens() {
  weather_.onIdleSettle();
  radar_.onIdleSettle();
  sports_.onIdleSettle();
  timezones_.onIdleSettle();
  // Clock: no ephemeral state
}
```

On Focused settle, screen is unchanged — still call `refresh_content()` so overlays clear.

- [ ] **Step 4: Keep input routing** — Carousel taps still must **not** call `on_tap_focused` (already true). Center-tap / rotate unchanged.

- [ ] **Step 5: Build** `pio run -e sim` — expect success

- [ ] **Step 6: Manual smoke** (`pio run -e sim -t upload`):

1. Boot Focused Clock — no top “Focused · Clock” label; clock full-bleed
2. Center-tap → Carousel: see **CLOCK** title + dots + inset preview
3. Rotate → titles/dots/preview update; taps on radar preview do nothing useful
4. Center-tap on Weather → full-bleed weather, no shell title
5. Focused idle (or temporarily lower timeout in a debug build / advance mentally): stay on Weather after settle
6. From Carousel idle → Focused Clock

- [ ] **Step 7: Commit**

```bash
git add src/sim/sim_app.hpp src/sim/sim_app.cpp
git commit -m "$(cat <<'EOF'
feat: Carousel frame + Focused full-bleed sim shell

EOF
)"
```

---

### Task 5: Radar disc scales to parent

**Files:**
- Modify: `src/ui/radar_lvgl.hpp`
- Modify: `src/ui/radar_lvgl.cpp`
- Modify: `src/ui/screen_radar_lvgl.cpp` (only if it assumes 340px)
- Modify: hit-test math in `src/sim/sim_app.cpp` if it hardcodes disc geometry / `kRadarContentOffsetY`

**Interfaces:**
- Change sizing so disc diameter = `min(lv_obj_get_content_width(parent), lv_obj_get_content_height(parent))` clamped, with a small inset (e.g. 4px) so rings stay inside the round clip.
- Plot radius ≈ `disc/2 - 10` (keep rings inside).
- Deprecate or stop using fixed `kRadarDiscPx = 340` as the sole size; keep helpers that take `disc_px` / `plot_radius`.
- Remove `kRadarContentOffsetY` shell nudge if content is truly centered full-bleed; update hit-tests to use disc center = parent center.

- [ ] **Step 1: Refactor** `radar_lvgl_build` / animate path to measure parent and size `g_disc` accordingly; scale blip projection with the computed plot radius.

- [ ] **Step 2: Fix sim Focused radar tap hit-testing** to match new geometry (center of `focused_host_` / `body_`, plot radius from same formula).

- [ ] **Step 3: Build** `pio run -e sim` + `pio test -e native -f test_screen_radar` (domain tests unchanged)

- [ ] **Step 4: Manual** — Focused radar nearly edge-to-edge; Carousel radar preview fits inside 240 inset without clipping title/dots

- [ ] **Step 5: Commit**

```bash
git add src/ui/radar_lvgl.hpp src/ui/radar_lvgl.cpp src/ui/screen_radar_lvgl.cpp src/sim/sim_app.cpp
git commit -m "$(cat <<'EOF'
feat: size radar disc from parent for inset and full-bleed

EOF
)"
```

---

### Task 6: Docs

**Files:**
- Modify: `docs/NAV.md`
- Modify: `docs/SIM.md`
- Modify: `docs/FIRMWARE_PLAN.md` (Global Interaction Model idle bullets only)

- [ ] **Step 1: Update NAV.md**

- Document Carousel as title + dots + inset preview (non-interactive)
- Document Focused as full-bleed, no shell chrome
- Idle table: Carousel → Focused Clock; Focused → stay + settle
- Center-tap unchanged

- [ ] **Step 2: Update SIM.md**

- Remove references to mode·screen chrome label
- Describe Carousel frame controls still map the same keys
- Note Focused idle stay behavior

- [ ] **Step 3: Patch FIRMWARE_PLAN.md** interaction model so idle matches spec (Focused does not fall back to Clock)

- [ ] **Step 4: Commit**

```bash
git add docs/NAV.md docs/SIM.md docs/FIRMWARE_PLAN.md
git commit -m "$(cat <<'EOF'
docs: document full-screen Carousel/Focused shell UX

EOF
)"
```

---

## Self-review (spec coverage)

| Spec requirement | Task |
|------------------|------|
| Carousel title + dots + live inset | 3, 4 |
| Focused no shell chrome / full-bleed | 4, 5 |
| Center-tap unchanged | 4 (no nav change to toggle) |
| Carousel idle → Clock | 1 |
| Focused idle stay + settle | 1, 2, 4 |
| Radar zoom/select unchanged | 2 (settle clears selection only), 5 (layout only) |
| Docs | 6 |
| Nav + screen tests | 1, 2 |

No placeholders remain. `IdleEvent` / `onIdleSettle` / `carousel_lvgl_*` names are consistent across tasks.
