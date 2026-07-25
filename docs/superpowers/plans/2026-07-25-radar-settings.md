# Radar Settings Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a long-press radar settings overlay with persistent declutter modes, map-layer toggles, and demo mode, plus a reserved full-tag line 4 for future arrival ICAO.

**Architecture:** Domain owns `RadarSettings` on `ScreenRadar` (declutter + layer flags + demo). A portable prefs store persists to a file on native/sim and ESP32 NVS on dial. LVGL reads settings from `RadarView`, gates tags/overlays, and hosts a full-screen sectioned settings overlay. Sim wires long-press / center-tap / frozen zoom / demo fixture bind.

**Tech Stack:** C++17, Unity (`pio test -e native`), LVGL 8.4 + SDL sim, ArduinoJson 7 (unchanged), ESP32 Preferences (dial only)

**Spec:** `docs/superpowers/specs/2026-07-25-radar-settings-design.md`

## Global Constraints

- Long-press on focused Radar opens settings (replaces `pinCenter()` this pass)
- Live apply + persist on each change; Done or center-tap closes overlay
- Zoom frozen while settings open
- Declutter factory default: `TargetTag`; map layers all on; demo off
- Every mode: selecting a target shows the **full** tag
- Outside ≤25 mi vector range: unselected remain dots-only (zoom honesty unchanged)
- Arrival line 4: layout/format hook only — never invent airports
- `desk_display` must not depend on LVGL
- Backend unchanged this plan

## File map

| File | Responsibility |
|------|----------------|
| `lib/desk_display/include/desk_display/radar_settings.hpp` | `RadarDeclutterMode`, `RadarSettings`, defaults, unselected-label helper |
| `lib/desk_display/src/radar_settings.cpp` | Helper implementations |
| `lib/desk_display/include/desk_display/radar_prefs.hpp` + `src/radar_prefs.cpp` | Load/save settings (file path API; ESP NVS behind `#ifdef ARDUINO`) |
| `lib/desk_display/include/desk_display/radar_format.hpp` + `src/radar_format.cpp` | `formatRadarTagLine4` |
| `lib/desk_display/include/desk_display/screen_radar.hpp` + `src/screen_radar.cpp` | Hold settings; gate overlays; expose on `RadarView`; settings-open idle settle |
| `src/ui/radar_lvgl.hpp` + `src/ui/radar_lvgl.cpp` | Declutter drawing, line 4, Demo header, settings overlay |
| `src/sim/sim_app.cpp` / `.hpp` | Long-press → settings; demo bind; prefs path; gesture routing |
| `test/test_domain/test_main.cpp` | Format line4 + declutter helper tests |
| `test/test_screen_radar/test_main.cpp` | Settings, overlay gates, prefs round-trip, idle |
| `docs/SIM.md` | U = settings; demo note |
| `.gitignore` | Sim prefs file path if under project root |

---

### Task 1: Declutter types + unselected label helper

**Files:**
- Create: `lib/desk_display/include/desk_display/radar_settings.hpp`
- Create: `lib/desk_display/src/radar_settings.cpp`
- Modify: `test/test_domain/test_main.cpp`

**Interfaces:**
- Produces:
  - `enum class RadarDeclutterMode : uint8_t { TargetOnly = 0, TargetCallsign = 1, TargetTag = 2 };`
  - `enum class RadarUnselectedLabel : uint8_t { None = 0, Callsign = 1, DenseTag = 2 };`
  - `struct RadarSettings { RadarDeclutterMode declutter; bool showAirports; bool showAirspace; bool showRoads; bool demoMode; };`
  - `RadarSettings radarSettingsFactoryDefaults();` → Tag, all layers true, demo false
  - `RadarUnselectedLabel radarUnselectedLabel(RadarDeclutterMode mode);`

- [ ] **Step 1: Write failing tests** in `test/test_domain/test_main.cpp`:

```cpp
#include "desk_display/radar_settings.hpp"

void test_radar_settings_defaults() {
  const auto s = desk_display::radarSettingsFactoryDefaults();
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(desk_display::RadarDeclutterMode::TargetTag),
      static_cast<uint8_t>(s.declutter));
  TEST_ASSERT_TRUE(s.showAirports);
  TEST_ASSERT_TRUE(s.showAirspace);
  TEST_ASSERT_TRUE(s.showRoads);
  TEST_ASSERT_FALSE(s.demoMode);
}

void test_radar_unselected_label_modes() {
  using desk_display::RadarDeclutterMode;
  using desk_display::RadarUnselectedLabel;
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RadarUnselectedLabel::None),
      static_cast<uint8_t>(
          desk_display::radarUnselectedLabel(RadarDeclutterMode::TargetOnly)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RadarUnselectedLabel::Callsign),
      static_cast<uint8_t>(desk_display::radarUnselectedLabel(
          RadarDeclutterMode::TargetCallsign)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(RadarUnselectedLabel::DenseTag),
      static_cast<uint8_t>(
          desk_display::radarUnselectedLabel(RadarDeclutterMode::TargetTag)));
}
```

Register both in `main()` / `UNITY_BEGIN` runner the same way other tests in that file are registered.

- [ ] **Step 2: Run to verify fail**

```bash
/Users/bruceclingan/.platformio/penv/bin/pio test -e native -f test_domain
```

Expected: FAIL (missing header / undefined symbols)

- [ ] **Step 3: Implement**

`radar_settings.hpp`:

```cpp
#pragma once
#include <cstdint>

namespace desk_display {

enum class RadarDeclutterMode : uint8_t {
  TargetOnly = 0,
  TargetCallsign = 1,
  TargetTag = 2,
};

enum class RadarUnselectedLabel : uint8_t {
  None = 0,
  Callsign = 1,
  DenseTag = 2,
};

struct RadarSettings {
  RadarDeclutterMode declutter;
  bool showAirports;
  bool showAirspace;
  bool showRoads;
  bool demoMode;
};

RadarSettings radarSettingsFactoryDefaults();
RadarUnselectedLabel radarUnselectedLabel(RadarDeclutterMode mode);

}  // namespace desk_display
```

`radar_settings.cpp`:

```cpp
#include "desk_display/radar_settings.hpp"

namespace desk_display {

RadarSettings radarSettingsFactoryDefaults() {
  return RadarSettings{
      RadarDeclutterMode::TargetTag,
      true,
      true,
      true,
      false,
  };
}

RadarUnselectedLabel radarUnselectedLabel(RadarDeclutterMode mode) {
  switch (mode) {
    case RadarDeclutterMode::TargetOnly:
      return RadarUnselectedLabel::None;
    case RadarDeclutterMode::TargetCallsign:
      return RadarUnselectedLabel::Callsign;
    case RadarDeclutterMode::TargetTag:
    default:
      return RadarUnselectedLabel::DenseTag;
  }
}

}  // namespace desk_display
```

- [ ] **Step 4: Re-run** `pio test -e native -f test_domain` — expect PASS

- [ ] **Step 5: Commit**

```bash
git add lib/desk_display/include/desk_display/radar_settings.hpp \
  lib/desk_display/src/radar_settings.cpp test/test_domain/test_main.cpp
git commit -m "$(cat <<'EOF'
feat: add radar declutter settings types

Introduce RadarSettings defaults and unselected-label policy helpers.
EOF
)"
```

---

### Task 2: ScreenRadar settings + overlay gating

**Files:**
- Modify: `lib/desk_display/include/desk_display/screen_radar.hpp`
- Modify: `lib/desk_display/src/screen_radar.cpp`
- Modify: `test/test_screen_radar/test_main.cpp`

**Interfaces:**
- Consumes: `RadarSettings`, `radarSettingsFactoryDefaults()`
- Produces on `ScreenRadar`:
  - `const RadarSettings& settings() const;`
  - `void setSettings(const RadarSettings& s);` — clamps/normalizes; calls `reprojectOverlays()`
  - `void setDeclutterMode(RadarDeclutterMode m);`
  - `void setShowAirports(bool);` / `setShowAirspace(bool);` / `setShowRoads(bool);`
  - `void setDemoMode(bool);`
  - `bool settingsOpen() const;` / `void openSettings();` / `void closeSettings();`
  - `void onIdleSettle();` — if settings open: `closeSettings()` then `clearSelection()`; else clear selection only
- Produces on `RadarView`:
  - `RadarSettings settings;`
  - `bool settingsOpen;`

- [ ] **Step 1: Write failing tests** in `test/test_screen_radar/test_main.cpp` (reuse existing map-context bind fixtures):

```cpp
void test_radar_settings_gate_airports() {
  desk_display::ScreenRadar screen;
  desk_display::MapContext ctx{};
  // bind fixture map_context_dayton.json as existing tests do
  TEST_ASSERT_TRUE(screen.view().staticMarkCount > 0);

  desk_display::RadarSettings s = screen.settings();
  s.showAirports = false;
  screen.setSettings(s);
  // POIs may still appear if configured; airports from map context must be gone.
  // Prefer asserting airspace/highways still present when those flags true:
  s.showAirports = false;
  s.showAirspace = true;
  s.showRoads = true;
  screen.setSettings(s);
  auto v = screen.view();
  for (std::size_t i = 0; i < v.staticMarkCount; ++i) {
    TEST_ASSERT_NOT_EQUAL(desk_display::RadarStaticMark::Kind::Airport,
                          v.staticMarks[i].kind);
  }
  TEST_ASSERT_TRUE(v.airspaceRingCount > 0 || v.highwayCount > 0);
}

void test_radar_settings_gate_airspace_and_roads() {
  desk_display::ScreenRadar screen;
  // bind map context with rings + highways
  desk_display::RadarSettings s = screen.settings();
  s.showAirspace = false;
  s.showRoads = false;
  screen.setSettings(s);
  auto v = screen.view();
  TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)v.airspaceRingCount);
  TEST_ASSERT_EQUAL_UINT32(0, (uint32_t)v.highwayCount);
}

void test_radar_idle_settle_closes_settings() {
  desk_display::ScreenRadar screen;
  screen.openSettings();
  TEST_ASSERT_TRUE(screen.settingsOpen());
  screen.onIdleSettle();
  TEST_ASSERT_FALSE(screen.settingsOpen());
}
```

Fill map bind using the same `loadFixture("map_context_dayton.json", ...)` pattern already in this file.

- [ ] **Step 2: Run** `pio test -e native -f test_screen_radar` — expect FAIL

- [ ] **Step 3: Implement**

In `screen_radar.hpp` add `#include "desk_display/radar_settings.hpp"`, members `RadarSettings settings_{radarSettingsFactoryDefaults()};`, `bool settingsOpen_{false};`, public accessors above, and extend `RadarView` with `settings` + `settingsOpen`.

In `reprojectOverlays()`:
- Skip airport projection loop when `!settings_.showAirports` (still project POIs when airports off — spec: “POIs follow the same gate”; **gate POIs with `showAirports` as well**).
- Skip airspace ring projection when `!settings_.showAirspace`.
- Skip highway projection when `!settings_.showRoads`.

`view()` copies `settings_` and `settingsOpen_`.

`onIdleSettle()`:

```cpp
void ScreenRadar::onIdleSettle() {
  if (settingsOpen_) {
    closeSettings();
  }
  clearSelection();
}
```

Keep `pinCenter()` / `clearPin()` APIs for later; nothing calls pin from long-press after Task 7.

- [ ] **Step 4: Re-run** `pio test -e native -f test_screen_radar` — PASS

- [ ] **Step 5: Commit**

```bash
git add lib/desk_display/include/desk_display/screen_radar.hpp \
  lib/desk_display/src/screen_radar.cpp test/test_screen_radar/test_main.cpp
git commit -m "$(cat <<'EOF'
feat: gate radar overlays from settings flags

ScreenRadar holds RadarSettings and skips airports/POIs, airspace, and
roads in reproject when toggled off.
EOF
)"
```

---

### Task 3: Prefs load/save

**Files:**
- Create: `lib/desk_display/include/desk_display/radar_prefs.hpp`
- Create: `lib/desk_display/src/radar_prefs.cpp`
- Modify: `test/test_screen_radar/test_main.cpp`
- Modify: `.gitignore` — add `radar_prefs.bin` (sim default path under project root)

**Interfaces:**
- Produces:
  - `constexpr const char* kRadarPrefsNvsNamespace = "radar";`
  - `bool saveRadarSettingsToFile(const RadarSettings& s, const char* path);`
  - `bool loadRadarSettingsFromFile(RadarSettings& out, const char* path);`
    - Missing/corrupt file → `out = radarSettingsFactoryDefaults()`, return false
    - Valid → fill `out`, return true
  - File format (versioned binary, little-endian, portable):
    - `uint32_t magic = 0x52445253;` (`'RDRS'`)
    - `uint16_t version = 1;`
    - `uint8_t declutter;`
    - `uint8_t showAirports, showAirspace, showRoads, demoMode;` (0/1)
    - `uint8_t reserved[3] = {0,0,0};`
  - On dial (`#if defined(ARDUINO)`): also
    - `bool saveRadarSettingsNvs(const RadarSettings& s);`
    - `bool loadRadarSettingsNvs(RadarSettings& out);`
    using `Preferences` namespace `"radar"`, keys: `dcl`, `ap`, `as`, `rd`, `dm`

Native/sim tests use **file API only** (no Preferences on host).

- [ ] **Step 1: Failing tests**

```cpp
void test_radar_prefs_round_trip() {
  desk_display::RadarSettings in = desk_display::radarSettingsFactoryDefaults();
  in.declutter = desk_display::RadarDeclutterMode::TargetOnly;
  in.showRoads = false;
  in.demoMode = true;
  const char* path = "radar_prefs_test.bin";
  TEST_ASSERT_TRUE(desk_display::saveRadarSettingsToFile(in, path));
  desk_display::RadarSettings out{};
  TEST_ASSERT_TRUE(desk_display::loadRadarSettingsFromFile(out, path));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(in.declutter),
                          static_cast<uint8_t>(out.declutter));
  TEST_ASSERT_FALSE(out.showRoads);
  TEST_ASSERT_TRUE(out.demoMode);
  std::remove(path);
}

void test_radar_prefs_corrupt_uses_defaults() {
  const char* path = "radar_prefs_bad.bin";
  FILE* f = std::fopen(path, "wb");
  TEST_ASSERT_NOT_NULL(f);
  const char junk[] = "nope";
  std::fwrite(junk, 1, sizeof(junk), f);
  std::fclose(f);
  desk_display::RadarSettings out{};
  TEST_ASSERT_FALSE(desk_display::loadRadarSettingsFromFile(out, path));
  const auto d = desk_display::radarSettingsFactoryDefaults();
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(d.declutter),
                          static_cast<uint8_t>(out.declutter));
  std::remove(path);
}
```

- [ ] **Step 2: Run** — expect FAIL

- [ ] **Step 3: Implement** `radar_prefs.cpp` with `fopen`/`fread`/`fwrite`. Validate declutter ≤ 2; any invalid bool byte → treat as corrupt. On corrupt/missing, assign factory defaults before returning false.

- [ ] **Step 4: Re-run** — PASS

- [ ] **Step 5: Commit**

```bash
git add lib/desk_display/include/desk_display/radar_prefs.hpp \
  lib/desk_display/src/radar_prefs.cpp test/test_screen_radar/test_main.cpp \
  .gitignore
git commit -m "$(cat <<'EOF'
feat: persist radar settings to versioned prefs file

Add portable load/save with factory defaults on corrupt data; NVS hooks
for dial behind ARDUINO.
EOF
)"
```

---

### Task 4: Full-tag line 4 (arrival stub)

**Files:**
- Modify: `lib/desk_display/include/desk_display/radar_format.hpp`
- Modify: `lib/desk_display/src/radar_format.cpp`
- Modify: `lib/desk_display/include/desk_display/screen_radar.hpp` (`RadarDetailCard::tagLine4[8]`)
- Modify: `lib/desk_display/src/screen_radar.cpp` (`detailCard`)
- Modify: `test/test_domain/test_main.cpp`

**Interfaces:**
- Produces: `bool formatRadarTagLine4(char* buf, std::size_t bufLen, const char* arrivalIcao);`
  - Returns false and writes `""` if `arrivalIcao` null/empty
  - Otherwise copies ICAO (truncate to fit); return true if non-empty result

- [ ] **Step 1: Failing tests**

```cpp
void test_format_radar_tag_line4() {
  char buf[8];
  TEST_ASSERT_FALSE(desk_display::formatRadarTagLine4(buf, sizeof(buf), nullptr));
  TEST_ASSERT_EQUAL_STRING("", buf);
  TEST_ASSERT_FALSE(desk_display::formatRadarTagLine4(buf, sizeof(buf), ""));
  TEST_ASSERT_TRUE(desk_display::formatRadarTagLine4(buf, sizeof(buf), "KORD"));
  TEST_ASSERT_EQUAL_STRING("KORD", buf);
}
```

- [ ] **Step 2: Run** `pio test -e native -f test_domain` — FAIL

- [ ] **Step 3: Implement** with `snprintf` / guarded copy. In `detailCard()`, always set `tagLine4[0] = '\0'` and call `formatRadarTagLine4(card.tagLine4, sizeof(card.tagLine4), nullptr)` until Aircraft gains arrival later.

- [ ] **Step 4: Re-run** domain + screen_radar — PASS

- [ ] **Step 5: Commit**

```bash
git add lib/desk_display/include/desk_display/radar_format.hpp \
  lib/desk_display/src/radar_format.cpp \
  lib/desk_display/include/desk_display/screen_radar.hpp \
  lib/desk_display/src/screen_radar.cpp test/test_domain/test_main.cpp
git commit -m "$(cat <<'EOF'
feat: reserve radar full-tag line 4 for arrival ICAO

Add formatRadarTagLine4; detail card field stays empty until routeset data exists.
EOF
)"
```

---

### Task 5: LVGL traffic tags honor declutter + line 4 + Demo header

**Files:**
- Modify: `src/ui/radar_lvgl.cpp`
- Modify: `src/ui/radar_lvgl.hpp` (only if new public helpers needed — prefer keep internal)

**Interfaces:**
- Consumes: `v.settings.declutter`, `radarUnselectedLabel`, `v.detail.tagLine4` / format line4 when selected
- Header: append ` - DEMO` when `v.settings.demoMode`

- [ ] **Step 1: Update `format_header`**

```cpp
void format_header(char* hdr, std::size_t hdrLen, const desk_display::RadarView& v) {
  std::snprintf(hdr, hdrLen, "%.0f mi - %zu%s%s",
                static_cast<double>(v.rangeMiles), v.blipCount,
                show_vectors(v.rangeMiles) ? " - vec" : "",
                v.settings.demoMode ? " - DEMO" : "");
}
```

- [ ] **Step 2: Change `draw_blip_tag` signature** to accept `tagLine4` and optional flags for which lines to draw:

```cpp
void draw_blip_tag(lv_obj_t* layer, std::size_t blipIndex, const char* callsign,
                   const char* tagLine2, const char* tagLine3, const char* tagLine4,
                   uint32_t callsignColor, bool selected, bool drawCallsign,
                   bool drawLine2, lv_coord_t bx, lv_coord_t by);
```

- Selected: always `drawCallsign=true`, Full line2, line3, line4 if non-empty; bump `kTagMargin` to `60` when line4 present / selected.
- Unselected + vectors:
  - `None` → draw symbol only (skip `draw_blip_tag` entirely; hit box = mark only)
  - `Callsign` → callsign only (`drawLine2=false`)
  - `DenseTag` → callsign + dense line2 (today)

Hit-box height must include line4 when selected (`tagY + 24 + 12`).

- [ ] **Step 3: In `build_traffic`**, replace unconditional tag draw:

```cpp
using desk_display::RadarUnselectedLabel;
const auto label = selected
    ? RadarUnselectedLabel::DenseTag  // unused when selected
    : desk_display::radarUnselectedLabel(v.settings.declutter);

char tagLine2[24]{};
char tagLine3[28]{};
char tagLine4[8]{};
if (selected) {
  desk_display::formatRadarTagLine2(tagLine2, sizeof(tagLine2), b.aircraft,
                                    desk_display::RadarTagStyle::Full);
  desk_display::formatRadarTagLine3(tagLine3, sizeof(tagLine3), b.aircraft.type,
                                    b.aircraft.squawk, b.notable);
  desk_display::formatRadarTagLine4(tagLine4, sizeof(tagLine4), nullptr);
} else if (label == RadarUnselectedLabel::DenseTag) {
  desk_display::formatRadarTagLine2(tagLine2, sizeof(tagLine2), b.aircraft,
                                    desk_display::RadarTagStyle::Dense);
}

if (selected || label != RadarUnselectedLabel::None) {
  const bool drawCs = true;
  const bool drawL2 = selected || label == RadarUnselectedLabel::DenseTag;
  // ... update hit box ...
  draw_blip_tag(..., tagLine2, tagLine3, tagLine4, markColor, selected, drawCs,
                drawL2, bx, by);
}
```

For `Callsign` mode, still draw callsign via `draw_blip_tag` with empty line2.

- [ ] **Step 4: Build sim** to confirm compile

```bash
/Users/bruceclingan/.platformio/penv/bin/pio run -e sim
```

Expected: SUCCESS

- [ ] **Step 5: Commit**

```bash
git add src/ui/radar_lvgl.cpp src/ui/radar_lvgl.hpp
git commit -m "$(cat <<'EOF'
feat: apply declutter modes and demo header on radar PPI

Unselected tags follow TargetOnly/Callsign/Tag; selected stays full;
header shows DEMO when demo mode is on.
EOF
)"
```

---

### Task 6: LVGL settings overlay (layout A)

**Files:**
- Modify: `src/ui/radar_lvgl.hpp`
- Modify: `src/ui/radar_lvgl.cpp`

**Interfaces:**
- Produces:
  - `void radar_lvgl_set_settings_open(bool open);` — or drive purely from `v.settingsOpen` inside `radar_lvgl_build` / animate
  - Prefer: when `v.settingsOpen`, build/show overlay children on top of disc; when false, delete overlay
  - `bool radar_lvgl_settings_hit(lv_coord_t absX, lv_coord_t absY, desk_display::ScreenRadar& radar);`
    - Returns true if tap consumed by overlay (chip / Done)
    - Mutates `radar` via setters (`setDeclutterMode`, layer toggles, `setDemoMode`, `closeSettings`)
    - Caller persists after true return (Task 7)

Overlay layout (dark full-bleed panel over parent):

```
Radar Settings                         Done
DECLUTTER
[ Target ] [ Callsign ] [ Tag ]     // exclusive; selected filled
MAP CLUTTER
[ Airports ] [ Airspace ] [ Roads ] // multi toggles
Demo Mode                              Off|On
```

Use LVGL buttons/labels with montserrat_12/14; exclusive declutter highlights active mode; map chips toggle green when on. Done calls `radar.closeSettings()`.

Do **not** rebuild the entire disc on every chip tap if avoidable — update chip styles + let next `refresh_content` rebuild from new `view()`. Simplest acceptable approach: caller `refresh_content()` after every settings mutation (sim already refreshes).

- [ ] **Step 1: Extend `radar_lvgl_build` / `radar_lvgl_animate_classic`**

At end of build (and when settingsOpen flips), if `v.settingsOpen`, create overlay rooted on `parent` (not clipped by disc). Store static/global `g_settings_root` similar to other caches; clear in `radar_lvgl_invalidate`.

When `!v.settingsOpen` and overlay exists, `lv_obj_del` it.

- [ ] **Step 2: Implement hit-test**

Map absolute coords to chip rectangles stored during last overlay build (same pattern as blip hit targets). Order: Done → declutter chips → map chips → demo toggle.

- [ ] **Step 3: `pio run -e sim`** — SUCCESS

- [ ] **Step 4: Manual smoke** (document in commit body): run sim, focus radar, press `U`, toggle chips, `Done` / center — visual check.

- [ ] **Step 5: Commit**

```bash
git add src/ui/radar_lvgl.hpp src/ui/radar_lvgl.cpp
git commit -m "$(cat <<'EOF'
feat: add radar settings overlay UI

Sectioned Declutter / Map Clutter / Demo controls with hit-testing.
EOF
)"
```

---

### Task 7: Sim wiring — gestures, prefs, demo

**Files:**
- Modify: `src/sim/sim_app.hpp`
- Modify: `src/sim/sim_app.cpp`
- Modify: `docs/SIM.md`
- Modify: `.gitignore` if using `radar_prefs.bin` at repo root

**Interfaces:**
- Sim prefs path: `radar_prefs.bin` in CWD (gitignored)
- On boot after fixtures: `loadRadarSettingsFromFile` → `radar_.setSettings(s)`
- After each settings mutation: `saveRadarSettingsToFile(radar_.settings(), "radar_prefs.bin")`
- Demo: when `settings().demoMode`:
  - `adsb_poll_.setActive(false)` even if radar active
  - Once on enter demo (or each toggle on): `loadFixture("adsb_sample.json")` + `parseAdsb` + `radar_.bind`
  - When leaving demo: re-enable poller; do not clear blips until next live poll arrives
- Long-press Radar: `radar_.openSettings()` (not `pinCenter`)
- While `radar_.settingsOpen()`:
  - Rotate: **ignore** (do not call `onRotate`)
  - Center-tap: `radar_.closeSettings()`; **do not** `nav_.on_center_tap()` / do not `revertTempCenter`
  - Tap: `radar_lvgl_settings_hit(...)`; on consume, persist; skip blip hit-test
  - Double-tap / long-press: no-op
- Idle settle already calls `radar_.onIdleSettle()` which closes settings

- [ ] **Step 1: Boot load prefs** in `SimApp` init / fixture load path:

```cpp
desk_display::RadarSettings prefs = desk_display::radarSettingsFactoryDefaults();
desk_display::loadRadarSettingsFromFile(prefs, "radar_prefs.bin");
radar_.setSettings(prefs);
if (prefs.demoMode) {
  // bind adsb_sample.json immediately
}
```

- [ ] **Step 2: Replace long-press**

```cpp
case Screen::Radar:
  radar_.openSettings();
  break;
```

- [ ] **Step 3: Gate `handle_input`** for settings-open radar (center / rotate / tap) as specified.

- [ ] **Step 4: Gate `update()` ADS-B poll** with `!radar_.settings().demoMode`.

- [ ] **Step 5: Update `docs/SIM.md`** — `U` opens radar settings; demo mode note; prefs file `radar_prefs.bin`.

- [ ] **Step 6: Run tests + sim build**

```bash
/Users/bruceclingan/.platformio/penv/bin/pio test -e native -f test_domain -f test_screen_radar
/Users/bruceclingan/.platformio/penv/bin/pio run -e sim
```

Expected: PASS / SUCCESS

- [ ] **Step 7: Commit**

```bash
git add src/sim/sim_app.hpp src/sim/sim_app.cpp docs/SIM.md .gitignore
git commit -m "$(cat <<'EOF'
feat: wire radar settings gestures and demo mode in sim

Long-press opens overlay; prefs persist; demo binds adsb_sample and
pauses live ADS-B polling.
EOF
)"
```

---

### Task 8: Dial NVS hook + docs cross-link

**Files:**
- Modify: `lib/desk_display/src/radar_prefs.cpp` (complete NVS path if not finished in Task 3)
- Modify: `docs/SIM.md` / `docs/FIRMWARE_PLAN.md` one-line note that radar settings use NVS namespace `radar`
- Modify: `docs/superpowers/specs/2026-07-25-radar-settings-design.md` status line only if needed (keep Approved)

Dial full input shell may still be stubby — ensure compile for dial:

```bash
/Users/bruceclingan/.platformio/penv/bin/pio run -e dial
```

If dial has no Preferences include path issues, wrap NVS functions with `#if defined(ARDUINO) && defined(ESP32)`.

- [ ] **Step 1: Ensure dial build compiles** with prefs NVS stubs or real Preferences

- [ ] **Step 2: Short FIRMWARE_PLAN / SIM note**

- [ ] **Step 3: Commit**

```bash
git add lib/desk_display/src/radar_prefs.cpp docs/SIM.md docs/FIRMWARE_PLAN.md
git commit -m "$(cat <<'EOF'
feat: enable ESP32 NVS persistence for radar settings

Dial build uses Preferences namespace radar; docs note the keys.
EOF
)"
```

---

## Spec coverage checklist

| Spec requirement | Task |
|------------------|------|
| Long-press → settings | 7 |
| Layout A overlay | 6 |
| Live apply + persist | 6, 3, 7 |
| Done / center-tap close | 6, 7 |
| Zoom frozen while open | 7 |
| Declutter 3 modes + defaults | 1, 2, 5 |
| Select → full tag all modes | 5 |
| Map clutter toggles + defaults | 2, 6 |
| Demo on/off + fixture + indicator | 5, 7 |
| NVS / file persistence | 3, 8 |
| Idle closes overlay, keeps prefs | 2, 7 |
| Line 4 stub | 4 |
| No backend / no pin UI | — out of scope |

## Placeholder / consistency notes

- `pinCenter` remains in API but is unused by long-press after Task 7.
- POIs gated with `showAirports` (same chip).
- Prefs path for sim: `radar_prefs.bin` CWD; tests use disposable `*_test.bin` names.
