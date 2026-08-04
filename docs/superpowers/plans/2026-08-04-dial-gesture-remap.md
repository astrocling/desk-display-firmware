# Dial Gesture Remap Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** On Dial, double-tap toggles Carousel ↔ Focused, single-tap selects on Focused Radar, and long-press opens Radar settings (with NVS + demo bind).

**Architecture:** A pure `TouchGestureDetector` in `lib/desk_display` classifies tap / double-tap / long-press from contact+XY+time. Dial `touch` HAL reads CST816 finger + 12-bit XY (remapped for 180° MADCTL), feeds the detector, and `dial_shell` routes events. `Nav::on_center_tap()` stays the mode toggle API; Dial invokes it only from DoubleTap. Web app and `src/sim/**` are untouched.

**Tech Stack:** C++17, PlatformIO `env:dial`, CST816 @ 0x15 over `Wire`, LVGL 8.4, Unity (`pio test -e native`), existing `radar_lvgl` hit-test + `radar_prefs` NVS.

**Spec:** `docs/superpowers/specs/2026-08-04-dial-gesture-remap-design.md`

## Global Constraints

- Dial firmware only — do not edit web clients or `src/sim/**`
- Carousel single tap = ignore; DoubleTap = only Carousel ↔ Focused enter/exit
- Focused Radar: Tap = hit-test select/clear; LongPress = settings; rotate = zoom (frozen while settings open)
- Settings open: chip taps apply + NVS save; Done or DoubleTap closes settings only (stay Focused)
- No Classic ↔ Detail toggle; no Focused Sports/Weather/TZ tap actions this pass
- Timing defaults: max short-hold 400 ms; double gap 350 ms; long-press 600 ms; refractory 80 ms
- Touch coords in LVGL/display space after 180° desk mount (`kLcdWidth`/`kLcdHeight` = 360)
- `lib/desk_display` stays free of Arduino / LVGL / GPIO
- Upload toolchain: `~/.platformio/penv311/bin/pio` when needed

## File map

| File | Responsibility |
|------|----------------|
| `lib/desk_display/include/desk_display/touch_gesture.hpp` | Gesture kinds, event struct, timing constants, `TouchGestureDetector` |
| `lib/desk_display/src/touch_gesture.cpp` | Pure classifier implementation |
| `test/test_input/test_main.cpp` | Native tests for gesture timing |
| `src/hal/touch.hpp` / `touch.cpp` | CST816 XY + feed detector; `touchPoll(TouchGesture&)` |
| `src/hal/dial_shell.hpp` / `dial_shell.cpp` | Route Tap/DoubleTap/LongPress; Radar select/settings/NVS/demo; freeze zoom in settings |
| `src/hal/adsb_demo_fixture.h` + generated `.c` | Embedded `adsb_sample.json` for demo mode |
| `src/main.cpp` | Poll touch gestures into shell (replace `touchPollCenterTap`) |
| `docs/NAV.md` | Document Dial double-tap as knob-click substitute |
| `docs/HANDOFF-dial-bringup.md` | Point path C at this ship |

`CenterTapDetector` remains for existing native tests; Dial touch stops using it.

---

### Task 1: Pure `TouchGestureDetector` (native TDD)

**Files:**
- Create: `lib/desk_display/include/desk_display/touch_gesture.hpp`
- Create: `lib/desk_display/src/touch_gesture.cpp`
- Modify: `test/test_input/test_main.cpp`

**Interfaces:**
- Produces:
  - `enum class TouchGestureKind : uint8_t { None, Tap, DoubleTap, LongPress };`
  - `struct TouchGesture { TouchGestureKind kind; int16_t x; int16_t y; };`
  - `constexpr uint32_t kTouchTapMaxMs = 400;`
  - `constexpr uint32_t kTouchDoubleGapMs = 350;`
  - `constexpr uint32_t kTouchLongPressMs = 600;`
  - `constexpr uint32_t kTouchRefractoryMs = 80;`
  - `class TouchGestureDetector` with:
    - `void reset();`
    - `TouchGesture update(bool down, int16_t x, int16_t y, uint32_t now_ms);`
      - Returns `{None,…}` most calls.
      - Short down→up (held ≤ `kTouchTapMaxMs`): arms a pending Tap at up (x,y); if a second short tap completes before `kTouchDoubleGapMs`, emit DoubleTap (pending cancelled).
      - If pending Tap’s deadline passes on a later `update` call, emit Tap once.
      - While down, if held ≥ `kTouchLongPressMs` and long not yet fired this contact: emit LongPress once at last XY; suppress Tap/DoubleTap for that contact’s up.
      - After any non-None fire: ignore new downs until `now_ms >= refractory_until`.
  - Keep updating pending deadline even when `down` is unchanged (callers poll every loop).

- [ ] **Step 1: Write failing tests** — append to `test/test_input/test_main.cpp` (keep existing CenterTap tests):

```cpp
#include "desk_display/touch_gesture.hpp"

using desk_display::TouchGestureDetector;
using desk_display::TouchGestureKind;
using desk_display::kTouchDoubleGapMs;
using desk_display::kTouchLongPressMs;
using desk_display::kTouchTapMaxMs;

void test_touch_single_tap_after_gap(void) {
  TouchGestureDetector d;
  TEST_ASSERT_EQUAL(TouchGestureKind::None, d.update(true, 10, 20, 1000).kind);
  TEST_ASSERT_EQUAL(TouchGestureKind::None, d.update(false, 10, 20, 1100).kind);
  // Still inside double window — no Tap yet
  TEST_ASSERT_EQUAL(TouchGestureKind::None,
                    d.update(false, 10, 20, 1100 + kTouchDoubleGapMs - 1).kind);
  const auto tap = d.update(false, 10, 20, 1100 + kTouchDoubleGapMs);
  TEST_ASSERT_EQUAL(TouchGestureKind::Tap, tap.kind);
  TEST_ASSERT_EQUAL_INT16(10, tap.x);
  TEST_ASSERT_EQUAL_INT16(20, tap.y);
}

void test_touch_double_tap(void) {
  TouchGestureDetector d;
  d.update(true, 1, 1, 1000);
  d.update(false, 1, 1, 1100);
  d.update(true, 2, 3, 1200);
  const auto dbl = d.update(false, 2, 3, 1300);
  TEST_ASSERT_EQUAL(TouchGestureKind::DoubleTap, dbl.kind);
  TEST_ASSERT_EQUAL_INT16(2, dbl.x);
  TEST_ASSERT_EQUAL_INT16(3, dbl.y);
  // Pending single must not fire later
  TEST_ASSERT_EQUAL(TouchGestureKind::None,
                    d.update(false, 0, 0, 1300 + kTouchDoubleGapMs + 50).kind);
}

void test_touch_long_press(void) {
  TouchGestureDetector d;
  TEST_ASSERT_EQUAL(TouchGestureKind::None, d.update(true, 5, 6, 1000).kind);
  TEST_ASSERT_EQUAL(TouchGestureKind::None,
                    d.update(true, 5, 6, 1000 + kTouchLongPressMs - 1).kind);
  const auto lp = d.update(true, 5, 6, 1000 + kTouchLongPressMs);
  TEST_ASSERT_EQUAL(TouchGestureKind::LongPress, lp.kind);
  TEST_ASSERT_EQUAL_INT16(5, lp.x);
  TEST_ASSERT_EQUAL_INT16(6, lp.y);
  // Up after long must not emit Tap
  TEST_ASSERT_EQUAL(TouchGestureKind::None, d.update(false, 5, 6, 2000).kind);
  TEST_ASSERT_EQUAL(TouchGestureKind::None,
                    d.update(false, 5, 6, 2000 + kTouchDoubleGapMs).kind);
}

void test_touch_hold_too_long_for_tap_without_long(void) {
  // Hold past tap max but release before long → neither tap nor long
  TouchGestureDetector d;
  d.update(true, 0, 0, 1000);
  TEST_ASSERT_EQUAL(TouchGestureKind::None,
                    d.update(false, 0, 0, 1000 + kTouchTapMaxMs + 1).kind);
  TEST_ASSERT_EQUAL(TouchGestureKind::None,
                    d.update(false, 0, 0, 1000 + kTouchTapMaxMs + 1 + kTouchDoubleGapMs)
                        .kind);
}
```

Register the four tests in `main()` with `RUN_TEST(...)`.

- [ ] **Step 2: Run tests — expect fail**

Run: `~/.platformio/penv311/bin/pio test -e native -f test_input`

Expected: compile fail (`touch_gesture.hpp` missing) or link/undefined detector.

- [ ] **Step 3: Implement detector**

`touch_gesture.hpp`:

```cpp
#pragma once

#include <cstdint>

namespace desk_display {

enum class TouchGestureKind : uint8_t { None, Tap, DoubleTap, LongPress };

struct TouchGesture {
  TouchGestureKind kind = TouchGestureKind::None;
  int16_t x = 0;
  int16_t y = 0;
};

constexpr uint32_t kTouchTapMaxMs = 400;
constexpr uint32_t kTouchDoubleGapMs = 350;
constexpr uint32_t kTouchLongPressMs = 600;
constexpr uint32_t kTouchRefractoryMs = 80;

class TouchGestureDetector {
 public:
  void reset();
  TouchGesture update(bool down, int16_t x, int16_t y, uint32_t now_ms);

 private:
  bool down_ = false;
  bool long_fired_ = false;
  bool suppress_up_ = false;
  uint32_t down_at_ms_ = 0;
  int16_t down_x_ = 0;
  int16_t down_y_ = 0;
  bool pending_tap_ = false;
  uint32_t pending_deadline_ms_ = 0;
  int16_t pending_x_ = 0;
  int16_t pending_y_ = 0;
  uint32_t refractory_until_ms_ = 0;
};

}  // namespace desk_display
```

`touch_gesture.cpp` — implement per Interfaces above. Pseudocode for the critical paths:

```cpp
TouchGesture TouchGestureDetector::update(bool down, int16_t x, int16_t y,
                                          uint32_t now_ms) {
  TouchGesture out{};

  // 1) Flush deferred single-tap if deadline passed and not currently starting a 2nd tap
  if (pending_tap_ && now_ms >= pending_deadline_ms_ && !down_) {
    pending_tap_ = false;
    out = {TouchGestureKind::Tap, pending_x_, pending_y_};
    refractory_until_ms_ = now_ms + kTouchRefractoryMs;
    // still process down edge below only if down became true this sample — usually false
  }

  if (down) {
    if (!down_) {
      if (now_ms < refractory_until_ms_) {
        return out;  // ignore bounce; do not arm
      }
      down_ = true;
      long_fired_ = false;
      suppress_up_ = false;
      down_at_ms_ = now_ms;
      down_x_ = x;
      down_y_ = y;
      return out;
    }
    // held
    if (!long_fired_ && (now_ms - down_at_ms_) >= kTouchLongPressMs) {
      long_fired_ = true;
      suppress_up_ = true;
      pending_tap_ = false;  // cancel armed single
      out = {TouchGestureKind::LongPress, x, y};
      refractory_until_ms_ = now_ms + kTouchRefractoryMs;
    }
    return out;
  }

  // up edge
  if (!down_) {
    return out;
  }
  down_ = false;
  if (suppress_up_ || long_fired_) {
    suppress_up_ = false;
    return out;
  }
  const uint32_t held = now_ms - down_at_ms_;
  if (held > kTouchTapMaxMs) {
    return out;
  }
  if (pending_tap_) {
    pending_tap_ = false;
    out = {TouchGestureKind::DoubleTap, x, y};
    refractory_until_ms_ = now_ms + kTouchRefractoryMs;
    return out;
  }
  pending_tap_ = true;
  pending_x_ = x;
  pending_y_ = y;
  pending_deadline_ms_ = now_ms + kTouchDoubleGapMs;
  return out;
}
```

Fix edge cases so `test_touch_single_tap_after_gap` / double / long all pass (adjust flush vs down-edge order if a test fails).

- [ ] **Step 4: Run tests — expect pass**

Run: `~/.platformio/penv311/bin/pio test -e native -f test_input`

Expected: all tests PASS including the four new ones.

- [ ] **Step 5: Commit**

```bash
git add lib/desk_display/include/desk_display/touch_gesture.hpp \
  lib/desk_display/src/touch_gesture.cpp test/test_input/test_main.cpp
git commit -m "$(cat <<'EOF'
feat(input): add TouchGestureDetector for tap/double/long

Pure timing classifier for Dial so Nav can use double-tap as knob-click.
EOF
)"
```

---

### Task 2: CST816 XY + gesture poll in touch HAL

**Files:**
- Modify: `src/hal/touch.hpp`
- Modify: `src/hal/touch.cpp`

**Interfaces:**
- Consumes: `desk_display::TouchGestureDetector`, `TouchGesture`, `kLcdWidth`/`kLcdHeight` from `hal/display.hpp`
- Produces:
  - `bool touchPoll(desk_display::TouchGesture& out);` — returns true when `out.kind != None`
  - Remove Dial use of `touchPollCenterTap` (delete declaration or leave unused — prefer replace)
  - Raw CST816: read 7 bytes from reg `0x00` (or start `0x01`); `finger = data[2] > 0`;  
    `raw_x = ((data[3] & 0x0F) << 8) | data[4]`;  
    `raw_y = ((data[5] & 0x0F) << 8) | data[6]`  
    (registers FingerNum@0x02, X@0x03/04, Y@0x05/06 — if reading from 0x00, index carefully: gesture@1, finger@2, …)
  - Display map for MADCTL 180°:  
    `x = static_cast<int16_t>(kLcdWidth - 1 - raw_x);`  
    `y = static_cast<int16_t>(kLcdHeight - 1 - raw_y);`  
    Guard clamp to `[0, kLcdWidth-1]` / `[0, kLcdHeight-1]`.  
    Keep a local `constexpr bool kTouchMap180 = true;` next to the map with a one-line comment tying it to desk-mount MADCTL.

- [ ] **Step 1: Rewrite `touch.hpp`**

```cpp
#pragma once

#include "desk_display/touch_gesture.hpp"

namespace desk_hal {

bool touchInit();
/** Poll CST816; returns true when a gesture event is ready in `out`. */
bool touchPoll(desk_display::TouchGesture& out);

}  // namespace desk_hal
```

- [ ] **Step 2: Rewrite `touch.cpp` poll path**

Keep `touchInit()` probe. Replace `readFingerDown` with `readTouchSample(bool& down, int16_t& x, int16_t& y)` that fills XY when down (on up, pass last-down XY if chip zeros coords — cache last valid XY while down).

```cpp
static desk_display::TouchGestureDetector s_gest;
// ...
bool touchPoll(desk_display::TouchGesture& out) {
  out = {};
  if (!s_ok) {
    return false;
  }
  bool down = false;
  int16_t x = 0;
  int16_t y = 0;
  if (!readTouchSample(down, x, y)) {
    // Still advance detector time with last known up so pending Tap can flush
    out = s_gest.update(false, 0, 0, millis());
    return out.kind != desk_display::TouchGestureKind::None;
  }
  out = s_gest.update(down, x, y, millis());
  return out.kind != desk_display::TouchGestureKind::None;
}
```

I²C read sketch (from reg 0x00, 7 bytes):

```cpp
// data[0]=? data[1]=GestureID data[2]=FingerNum
// data[3]=XposH data[4]=XposL data[5]=YposH data[6]=YposL
down = data[2] > 0;
const int raw_x = ((data[3] & 0x0F) << 8) | data[4];
const int raw_y = ((data[5] & 0x0F) << 8) | data[6];
```

- [ ] **Step 3: Fix compile of callers temporarily**

`main.cpp` still calls `touchPollCenterTap` — either leave a stub that returns false (not preferred) or update main in the same edit to compile:

```cpp
desk_display::TouchGesture gest{};
if (desk_hal::touchPoll(gest)) {
  // Task 3 wires shell; for now only DoubleTap → old center path so device stays usable mid-rebase:
  if (gest.kind == desk_display::TouchGestureKind::DoubleTap) {
    desk_hal::dialShellOnCenterTap();
  }
}
```

Full routing lands in Task 3; this keep-compile shim is OK for one commit if Task 3 follows immediately — prefer completing Task 3 in the same session before flashing.

- [ ] **Step 4: Commit**

```bash
git add src/hal/touch.hpp src/hal/touch.cpp src/main.cpp
git commit -m "$(cat <<'EOF'
feat(hal): poll CST816 XY through TouchGestureDetector

Map coords for 180° desk mount and expose tap/double/long to the shell.
EOF
)"
```

---

### Task 3: Shell routing — DoubleTap Nav, freeze zoom in settings

**Files:**
- Modify: `src/hal/dial_shell.hpp`
- Modify: `src/hal/dial_shell.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Produces:
  - `void dialShellOnTouch(const desk_display::TouchGesture& g);`
  - Remove or keep `dialShellOnCenterTap` as a thin wrapper calling `g_nav.on_center_tap()` + rebuild — prefer delete from header and route only via `OnTouch`
- Consumes: `Nav::on_center_tap`, `ScreenRadar::settingsOpen`, `revertTempCenter`

- [ ] **Step 1: Update `dial_shell.hpp`**

```cpp
#include "desk_display/touch_gesture.hpp"
// ...
void dialShellOnTouch(const desk_display::TouchGesture& gesture);
// remove dialShellOnCenterTap declaration
```

- [ ] **Step 2: Implement routing in `dial_shell.cpp`**

Replace `dialShellOnCenterTap` with:

```cpp
void dialShellOnTouch(const desk_display::TouchGesture& g) {
  using desk_display::TouchGestureKind;
  using desk_display::NavMode;
  using desk_display::Screen;

  if (g.kind == TouchGestureKind::None) {
    return;
  }

  const bool radar_settings =
      g_nav.mode() == NavMode::Focused &&
      g_nav.focused() == Screen::Radar && g_radar != nullptr &&
      g_radar->settingsOpen();

  if (g.kind == TouchGestureKind::DoubleTap) {
    if (radar_settings) {
      g_radar->closeSettings();
      g_nav.idle_reset();
      refresh_content();
      return;
    }
    if (g_nav.mode() == NavMode::Focused &&
        g_nav.focused() == Screen::Radar && g_radar != nullptr) {
      g_radar->revertTempCenter();
    }
    g_nav.on_center_tap();
    rebuild_ui_for_active();
    return;
  }

  if (g.kind == TouchGestureKind::Tap) {
    if (g_nav.mode() != NavMode::Focused) {
      return;  // Carousel: ignore
    }
    // Task 4 fills Radar tap; for now idle_reset only if non-radar
    g_nav.idle_reset();
    return;
  }

  if (g.kind == TouchGestureKind::LongPress) {
    if (g_nav.mode() != NavMode::Focused) {
      return;
    }
    g_nav.idle_reset();
    // Task 4 opens settings
    return;
  }
}
```

Update `dialShellOnRotate`:

```cpp
void dialShellOnRotate(int8_t delta) {
  if (delta == 0) {
    return;
  }
  if (g_nav.mode() == desk_display::NavMode::Focused && g_radar != nullptr &&
      g_nav.focused() == desk_display::Screen::Radar &&
      g_radar->settingsOpen()) {
    return;  // frozen
  }
  // ... existing body unchanged
}
```

- [ ] **Step 3: Wire `main.cpp`**

```cpp
desk_display::TouchGesture gest{};
if (desk_hal::touchPoll(gest)) {
  desk_hal::dialShellOnTouch(gest);
}
```

Include `desk_display/touch_gesture.hpp` if needed.

- [ ] **Step 4: Build Dial**

Run: `~/.platformio/penv311/bin/pio run -e dial`

Expected: SUCCESS.

- [ ] **Step 5: Commit**

```bash
git add src/hal/dial_shell.hpp src/hal/dial_shell.cpp src/main.cpp
git commit -m "$(cat <<'EOF'
feat(shell): double-tap toggles Carousel and Focused on Dial

Stop treating every short press as Nav; carousel single taps are ignored.
EOF
)"
```

---

### Task 4: Focused Radar tap select + long-press settings + NVS

**Files:**
- Modify: `src/hal/dial_shell.cpp`
- (Reuse) `src/ui/radar_lvgl.hpp` hit helpers — no API change

**Interfaces:**
- Consumes: `radar_lvgl_hit_blip`, `radar_lvgl_hit_static`, `radar_lvgl_settings_hit`, `loadRadarSettingsNvs`, `saveRadarSettingsNvs`, `openSettings` / `closeSettings`, `selectBlip`, `selectStaticMark`, `clearSelection`, `clearStaticSelection`
- Produces: working Focused Radar select + settings on Dial

- [ ] **Step 1: Load NVS prefs at shell init** (after `g_radar` alloc, before first build):

```cpp
#include "desk_display/radar_prefs.hpp"
// ...
desk_display::RadarSettings prefs = desk_display::radarSettingsFactoryDefaults();
desk_display::loadRadarSettingsNvs(prefs);  // assigns defaults on miss
g_radar->setSettings(prefs);
```

- [ ] **Step 2: Helper `persist_radar_prefs()` + tap handlers** in anonymous namespace:

```cpp
void persist_radar_prefs() {
  if (g_radar == nullptr) {
    return;
  }
  desk_display::saveRadarSettingsNvs(g_radar->settings());
}

void on_radar_tap(int16_t x, int16_t y) {
  if (g_radar == nullptr || g_body == nullptr) {
    return;
  }
  if (g_radar->settingsOpen()) {
    const auto prev = g_radar->settings();
    if (desk_ui::radar_lvgl_settings_hit(x, y, *g_radar)) {
      const auto& cur = g_radar->settings();
      if (cur.declutter != prev.declutter ||
          cur.showAirports != prev.showAirports ||
          cur.showAirspace != prev.showAirspace ||
          cur.showRoads != prev.showRoads ||
          cur.demoMode != prev.demoMode) {
        persist_radar_prefs();
        // Demo bind in Task 5 when demoMode rises
      }
      refresh_content();
    }
    return;
  }

  const auto rv = g_radar->view();
  std::size_t nearest = 0;
  if (desk_ui::radar_lvgl_hit_blip(g_body, rv, x, y, &nearest)) {
    if (g_radar->hasSelection() && g_radar->selectedIndex() == nearest) {
      g_radar->clearSelection();
    } else {
      g_radar->selectBlip(nearest);
    }
  } else if (desk_ui::radar_lvgl_hit_static(g_body, rv, x, y, &nearest)) {
    if (rv.hasStaticSelection && rv.selectedStaticIndex == nearest) {
      g_radar->clearStaticSelection();
    } else {
      g_radar->selectStaticMark(nearest);
    }
  } else {
    g_radar->clearSelection();
    g_radar->clearStaticSelection();
  }
  refresh_content();
}
```

- [ ] **Step 3: Fill Tap / LongPress branches in `dialShellOnTouch`**

```cpp
  if (g.kind == TouchGestureKind::Tap) {
    if (g_nav.mode() != NavMode::Focused) {
      return;
    }
    g_nav.idle_reset();
    if (g_nav.focused() == Screen::Radar) {
      on_radar_tap(g.x, g.y);
    }
    return;
  }

  if (g.kind == TouchGestureKind::LongPress) {
    if (g_nav.mode() != NavMode::Focused) {
      return;
    }
    g_nav.idle_reset();
    if (g_nav.focused() == Screen::Radar && g_radar != nullptr &&
        !g_radar->settingsOpen()) {
      g_radar->openSettings();
      refresh_content();
    }
    return;
  }
```

- [ ] **Step 4: Build**

Run: `~/.platformio/penv311/bin/pio run -e dial`

Expected: SUCCESS.

- [ ] **Step 5: Commit**

```bash
git add src/hal/dial_shell.cpp
git commit -m "$(cat <<'EOF'
feat(radar): Dial tap select and long-press settings with NVS

Wire hit-test select on Focused Radar and persist declutter/map/demo prefs.
EOF
)"
```

---

### Task 5: Demo mode fixture bind on Dial

**Files:**
- Create: `src/hal/adsb_demo_fixture.h`
- Create: `src/hal/adsb_demo_fixture.c` (generated)
- Modify: `src/hal/dial_shell.cpp`
- Modify: `platformio.ini` only if a custom script is added (prefer checking in generated `.c`)

**Interfaces:**
- Produces: `const char adsb_demo_json[];` / `unsigned int adsb_demo_json_len;` (xxd style) or `const char* desk_hal::adsbDemoJson();`
- When settings chip sets `demoMode` true: parse JSON into `AircraftList` via existing `parseAdsb*` / bind path used by tests; `g_radar->bind(...)`; `g_adsb_poll->setActive(false)` while demo.
- When demo false: resume `setActive(radar_active)`.

- [ ] **Step 1: Generate embedded fixture**

```bash
xxd -i fixtures/adsb_sample.json | sed 's/adsb_sample_json/adsb_demo_json/g' \
  > src/hal/adsb_demo_fixture.c
```

Create `src/hal/adsb_demo_fixture.h`:

```c
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
extern unsigned char adsb_demo_json[];
extern unsigned int adsb_demo_json_len;
#ifdef __cplusplus
}
#endif
```

Ensure `adsb_demo_json` is null-terminated: either append `0x00` in the generated array or copy into a `std::string`/`char` buffer with `len+1` when parsing.

- [ ] **Step 2: `bind_demo_adsb()` in dial_shell**

```cpp
#include "desk_display/adsb.hpp"
#include "adsb_demo_fixture.h"

void bind_demo_adsb() {
  if (g_radar == nullptr || g_ac_scratch == nullptr) {
    return;
  }
  // xxd arrays are not NUL-terminated — copy + terminate
  char* buf = static_cast<char*>(malloc(adsb_demo_json_len + 1));
  if (buf == nullptr) {
    return;
  }
  memcpy(buf, adsb_demo_json, adsb_demo_json_len);
  buf[adsb_demo_json_len] = '\0';
  if (desk_display::parseAdsb(buf, *g_ac_scratch)) {
    g_radar->bind(*g_ac_scratch);
  }
  free(buf);
}
```

Prefer PSRAM `allocLarge`/`heap_caps_malloc` if `malloc` is tight; fixture is ~10KB.

- [ ] **Step 3: Gate poller + call bind on demo edge**

In tick path:

```cpp
const bool demo = g_radar->settings().demoMode;
g_adsb_poll->setActive(radar_active && !demo);
```

In `on_radar_tap` settings branch after persist, if `cur.demoMode && !prev.demoMode` → `bind_demo_adsb(); refresh_content();`. If demo turns off, do nothing special (next live takeAircraft replaces).

Also after NVS load at init: if `prefs.demoMode` → `bind_demo_adsb()`.

- [ ] **Step 4: Build**

Run: `~/.platformio/penv311/bin/pio run -e dial`

Expected: SUCCESS; flash size still fits app partition.

- [ ] **Step 5: Commit**

```bash
git add src/hal/adsb_demo_fixture.h src/hal/adsb_demo_fixture.c src/hal/dial_shell.cpp
git commit -m "$(cat <<'EOF'
feat(radar): embed ADS-B demo fixture for Dial settings

Pause live poll while demo is on and bind the sample traffic set.
EOF
)"
```

---

### Task 6: Docs

**Files:**
- Modify: `docs/NAV.md`
- Modify: `docs/HANDOFF-dial-bringup.md` (short status note)

- [ ] **Step 1: Update `docs/NAV.md` Input mapping**

Replace CenterTap row / carousel preview note with Dial reality:

| Event | Shell behavior |
|-------|----------------|
| `Rotate(delta)` | Carousel: cycle highlight. Focused: idle reset only (screen owns rotate). |
| `DoubleTap` (Dial) | Knob-click substitute: Carousel → Focused on highlight; Focused → Carousel. (`Nav::on_center_tap()` still implements the toggle.) |
| `Tap` / `LongPress` | Idle reset; Focused Radar owns select / settings (Dial). Carousel Tap ignored. |

Carousel preview sentence: “rotate cycles highlight; **double-tap** focuses”.

- [ ] **Step 2: Handoff** — mark path C gesture remap shipped per this plan; note sim deprecated for input work.

- [ ] **Step 3: Commit**

```bash
git add docs/NAV.md docs/HANDOFF-dial-bringup.md
git commit -m "$(cat <<'EOF'
docs: Dial double-tap is Nav knob-click; update handoff

Reflect gesture remap and deprecate sim as the input target.
EOF
)"
```

---

### Task 7: On-device verification

**Files:** none (manual)

- [ ] **Step 1: Flash**

Run: `~/.platformio/penv311/bin/pio run -e dial -t upload`

- [ ] **Step 2: Manual checklist**

| # | Action | Expected |
|---|--------|----------|
| 1 | Carousel: single tap | No mode change |
| 2 | Carousel: double-tap | Enter Focused on highlighted screen |
| 3 | Focused: double-tap | Back to Carousel |
| 4 | Focused Radar: rotate | Range changes |
| 5 | Focused Radar: tap blip | Selection / tag |
| 6 | Focused Radar: tap empty | Clears selection |
| 7 | Focused Radar: long-press | Settings overlay |
| 8 | Settings: tap chip | Value changes; survives reboot (NVS) |
| 9 | Settings: double-tap | Overlay closes; still Focused |
| 10 | Settings open: rotate | No zoom |
| 11 | Demo on | Sample traffic; live ADS-B paused |
| 12 | Touch aim | Tap lands on intended blip (if mirrored, flip `kTouchMap180` / axis) |

- [ ] **Step 3: If XY is mirrored/wrong**, fix only the map in `touch.cpp`, reflash, recheck row 12. Commit:

```bash
git add src/hal/touch.cpp
git commit -m "fix(touch): correct CST816 to display mapping after desk mount"
```

---

## Spec coverage (self-review)

| Spec requirement | Task |
|------------------|------|
| DoubleTap ↔ Focus | 3 |
| Carousel ignore single tap | 3 |
| Focused Radar hit-test select | 4 |
| Long-press settings | 4 |
| Settings Done/DoubleTap close only | 3–4 |
| Rotate freeze in settings | 3 |
| NVS load/save | 4 |
| Demo bind + pause poll | 5 |
| 180° touch map | 2 / 7 |
| Timing constants | 1 |
| Web/sim untouched | Global constraint |
| Docs | 6 |
| No Detail mode | omitted by design |

## Placeholder / type check

- No TBD steps; CST816 register layout and gesture API named.
- `TouchGesture` / `TouchGestureKind` names consistent across tasks.
- `dialShellOnTouch` is the sole shell entry after Task 3.
