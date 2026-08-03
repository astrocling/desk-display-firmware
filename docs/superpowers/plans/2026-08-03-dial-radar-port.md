# Dial Radar Port Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** On Dial, replace the Radar stub with shared Classic `radar_lvgl` + live adsb.lol traffic and `/api/map-context` overlays, matching Weather/Sports shell wiring, without Dial selection/Detail/settings.

**Architecture:** Heap-allocate `AdsbPoller` / `MapContextPoller` HTTP bodies (64KB, Scores pattern) so Dial TLS has headroom. Wire `ScreenRadar` + both pollers in `dial_shell` with a one-GET-per-tick wrapper (map-context preferred). Prefer in-place Classic animate; rotate zooms; center tap stays Nav-only.

**Tech Stack:** C++17, PlatformIO (`native` / `sim` / `dial`), Unity tests, LVGL 8.4, Arduino `WiFiClientSecure` via `desk_net::httpGet`

**Spec:** `docs/superpowers/specs/2026-08-03-dial-radar-port-design.md`

## Global Constraints

- Dial A only: rotate = zoom; center tap = Nav; no hit-test / Detail toggle / settings on Dial
- Mode on Dial remains `RadarMode::ClassicSweep` (constructor default); do not call mode toggle
- Poll only while `Nav::active_screen() == Screen::Radar`
- Poller body: heap per attempt, `kBodyCap = 64 * 1024`, no 256KB BSS member
- At most one blocking Radar-related GET per Dial shell tick (map before ADS-B)
- Reuse `desk_net::httpGet`; no background HTTP thread this pass
- No Dial boot fixtures; keep last-good on failure
- `desk_display` must not depend on LVGL
- Do not invent Dial-only Radar APIs — leave `radar_lvgl_hit_*` / settings unused on Dial
- Prerequisite: Weather/Sports Dial port already present in the working tree (shared `weather_lvgl` / `sports_lvgl` + `http.cpp`); build Dial on top of that, do not revert it

## File map

| File | Responsibility |
|------|----------------|
| `lib/desk_display/include/desk_display/adsb_poll.hpp` | Drop BSS body; expose 64KB `kBodyCap` |
| `lib/desk_display/src/adsb_poll.cpp` | Heap buffer in `tryPollOnce` |
| `lib/desk_display/include/desk_display/map_context_poll.hpp` | Same as ADS-B poller header |
| `lib/desk_display/src/map_context_poll.cpp` | Heap buffer in `tryPollOnce` |
| `test/test_domain/test_main.cpp` | ADS-B poller tests still pass; add oversized-cap soft-fail if useful |
| `test/test_screen_radar/test_main.cpp` | Map-context poller tests still pass |
| `src/hal/dial_shell.cpp` | ScreenRadar + pollers + LVGL + serialize HTTP + Serial |
| `docs/HANDOFF-dial-bringup.md` | Radar no longer stub; note C follow-up |

---

### Task 1: Heap HTTP bodies for AdsbPoller + MapContextPoller

**Files:**
- Modify: `lib/desk_display/include/desk_display/adsb_poll.hpp`
- Modify: `lib/desk_display/src/adsb_poll.cpp`
- Modify: `lib/desk_display/include/desk_display/map_context_poll.hpp`
- Modify: `lib/desk_display/src/map_context_poll.cpp`
- Modify: `test/test_domain/test_main.cpp` (add oversized-body soft-fail test)
- Test: `pio test -e native`

**Interfaces:**
- Consumes: existing `AdsbHttpGetFn`, `parseAdsb`, `parseMapContext`
- Produces: pollers with `static constexpr std::size_t kBodyCap = 64 * 1024` and no `body_` member; `tryPollOnce` heap-allocates each attempt

- [ ] **Step 1: Write the failing oversized-body test** in `test/test_domain/test_main.cpp` (near existing ADS-B poller tests):

```cpp
bool fake_http_oversized(const char* /*url*/, char* body, std::size_t cap,
                         std::size_t& len, void* /*user*/) {
  ++g_http_calls;
  // Claim a body that fills the entire capacity — poller must reject (>= cap).
  len = cap;
  if (cap > 0) {
    body[0] = '{';
  }
  return true;
}

void test_adsb_poller_rejects_oversized_body(void) {
  g_http_calls = 0;
  AdsbPoller poll;
  poll.setHttpGet(fake_http_oversized, nullptr);
  poll.setCenter(40.03353, -84.19588, 25.0f);
  poll.setActive(true);
  poll.onTick(kAdsbPollIntervalMs);
  TEST_ASSERT_EQUAL(1, g_http_calls);
  AircraftList list{};
  TEST_ASSERT_FALSE(poll.takeAircraft(list));
  TEST_ASSERT_FALSE(poll.hasLastGood());
}
```

Register it with the other ADS-B poller tests:

```cpp
RUN_TEST(test_adsb_poller_rejects_oversized_body);
```

- [ ] **Step 2: Run test to verify it fails or passes against old BSS behavior**

Run: `export PATH="$HOME/.platformio/penv311/bin:$PATH" && pio test -e native --filter test_domain`

Expected: existing tests pass; new test may pass even on BSS if `len >= cap` already fails — that is fine. If the suite fails to **link/compile** after header changes in later steps, fix forward. Primary goal of this step is the test exists before shrinking capacity.

If the new test fails unexpectedly before code changes, stop and inspect `tryPollOnce` length checks.

- [ ] **Step 3: Update `adsb_poll.hpp` — remove BSS body, set 64KB cap**

Replace the private buffer section with:

```cpp
  AircraftList lastGood_{};
  /** Response buffer size; heap-allocated per poll (Dial TLS needs free heap). */
  static constexpr std::size_t kBodyCap = 64 * 1024;
};
```

(Delete `char body_[kBodyCap]{};` and the old `256 * 1024` constant.)

- [ ] **Step 4: Update `adsb_poll.cpp` `tryPollOnce` to heap-allocate**

Add includes:

```cpp
#include <memory>
#include <new>
```

Replace the HTTP + parse section of `tryPollOnce` with:

```cpp
bool AdsbPoller::tryPollOnce() {
  char url[160];
  if (!buildAdsbLolUrl(url, sizeof(url), centerLat_, centerLon_,
                       rangeStatuteMi_)) {
    return false;
  }

  auto body = std::unique_ptr<char[]>(new (std::nothrow) char[kBodyCap]);
  if (!body) {
    return false;
  }

  std::size_t bodyLen = 0;
  if (!httpGet_(url, body.get(), kBodyCap, bodyLen, httpUser_)) {
    return false;
  }
  if (bodyLen >= kBodyCap) {
    return false;
  }
  body[bodyLen] = '\0';

  AircraftList parsed{};
  if (!parseAdsb(body.get(), parsed)) {
    return false;
  }

  lastGood_ = parsed;
  hasLastGood_ = true;
  hasPending_ = true;
  return true;
}
```

- [ ] **Step 5: Update `map_context_poll.hpp` the same way**

```cpp
  MapContext lastGood_{};
  /** Response buffer size; heap-allocated per poll (Dial TLS needs free heap). */
  static constexpr std::size_t kBodyCap = 64 * 1024;
};
```

(Delete `char body_[kBodyCap]{};`.)

- [ ] **Step 6: Update `map_context_poll.cpp` `tryPollOnce`**

Add:

```cpp
#include <memory>
#include <new>
```

Replace body usage:

```cpp
MapContextPoller::PollAttemptResult MapContextPoller::tryPollOnce() {
  char url[256];
  if (!buildMapContextUrl(url, sizeof(url), centerLat_, centerLon_, rangeMi_)) {
    return PollAttemptResult::HardFail;
  }

  auto body = std::unique_ptr<char[]>(new (std::nothrow) char[kBodyCap]);
  if (!body) {
    return PollAttemptResult::Retry;
  }

  std::size_t bodyLen = 0;
  if (!httpGet_(url, body.get(), kBodyCap, bodyLen, httpUser_)) {
    return PollAttemptResult::Retry;
  }
  if (bodyLen == 0 || bodyLen >= kBodyCap) {
    return PollAttemptResult::HardFail;
  }
  body[bodyLen] = '\0';

  MapContext parsed{};
  if (!parseMapContext(body.get(), parsed)) {
    return PollAttemptResult::HardFail;
  }

  lastGood_ = parsed;
  hasLastGood_ = true;
  hasPending_ = true;
  return PollAttemptResult::Success;
}
```

- [ ] **Step 7: Run native tests**

Run: `export PATH="$HOME/.platformio/penv311/bin:$PATH" && pio test -e native`

Expected: all green, including `test_adsb_poller_*` and `test_map_context_poller_*`.

- [ ] **Step 8: Commit**

```bash
git add lib/desk_display/include/desk_display/adsb_poll.hpp \
  lib/desk_display/src/adsb_poll.cpp \
  lib/desk_display/include/desk_display/map_context_poll.hpp \
  lib/desk_display/src/map_context_poll.cpp \
  test/test_domain/test_main.cpp
git commit -m "$(cat <<'EOF'
fix: heap-allocate ADS-B and map-context poll bodies

Drop 256KB BSS buffers so Dial TLS has heap headroom; cap at 64KB.
EOF
)"
```

---

### Task 2: Wire Radar LVGL + rotate/settle in dial_shell

**Files:**
- Modify: `src/hal/dial_shell.cpp`
- Test: `pio run -e dial` (compile); `pio run -e sim` if touching shared UI (should not)

**Interfaces:**
- Consumes: `desk_ui::radar_lvgl_build`, `radar_lvgl_animate_classic`, `radar_lvgl_invalidate`; `desk_display::ScreenRadar`
- Produces: Dial shows Classic radar disc (empty traffic OK until Task 3); Focused rotate zooms

- [ ] **Step 1: Add includes and globals** near the Weather/Sports globals in `src/hal/dial_shell.cpp`:

```cpp
#include "desk_display/screen_radar.hpp"
#include "../ui/radar_lvgl.hpp"
```

```cpp
desk_display::ScreenRadar g_radar;
```

(Do not add pollers yet — Task 3.)

- [ ] **Step 2: Replace Radar stub in `refresh_content`**

Change the Radar branch so Classic can animate in place (mirror sim). Restructure `refresh_content` like:

```cpp
void refresh_content() {
  using desk_display::Screen;

  if (g_body == nullptr) {
    return;
  }

  if (g_nav.active_screen() == Screen::Radar &&
      desk_ui::radar_lvgl_animate_classic(g_body, g_radar.view())) {
    return;
  }

  lv_obj_clean(g_body);
  desk_ui::radar_lvgl_invalidate();

  const bool carousel_mode = g_nav.mode() == desk_display::NavMode::Carousel;
  const lv_coord_t host_h =
      carousel_mode ? desk_ui::kCarouselPreviewHostPx
                    : static_cast<lv_coord_t>(kLcdHeight - 48);

  switch (g_nav.active_screen()) {
    case Screen::Clock:
      desk_ui::clock_lvgl_build(g_body, g_clock.view());
      break;
    case Screen::Timezones:
      desk_ui::timezones_lvgl_build(g_body, g_timezones.view(), host_h);
      break;
    case Screen::Weather:
      desk_ui::weather_lvgl_build(g_body, g_weather.view());
      break;
    case Screen::Sports:
      desk_ui::sports_lvgl_build(g_body, g_sports.view());
      break;
    case Screen::Radar:
      desk_ui::radar_lvgl_build(g_body, g_radar.view());
      break;
    default:
      break;
  }
}
```

Remove `#include "../ui/screen_stub_lvgl.hpp"` only if no other screen still uses the stub.

- [ ] **Step 3: Invalidate radar caches on body teardown**

In `rebuild_ui_for_active`, after deleting `g_body` (and setting it null), call:

```cpp
  desk_ui::radar_lvgl_invalidate();
```

- [ ] **Step 4: Idle settle + Focused rotate**

```cpp
void settle_focused_screens() {
  g_timezones.onIdleSettle();
  g_weather.onIdleSettle();
  g_sports.onIdleSettle();
  g_radar.onIdleSettle();
}

void on_rotate_focused(int8_t delta) {
  switch (g_nav.focused()) {
    case desk_display::Screen::Timezones:
      g_timezones.onRotate(delta);
      break;
    case desk_display::Screen::Weather:
      g_weather.onRotate(delta);
      break;
    case desk_display::Screen::Sports:
      g_sports.onRotate(delta);
      break;
    case desk_display::Screen::Radar:
      g_radar.onRotate(delta);
      break;
    default:
      break;
  }
}
```

In `dialShellOnRotate`, include Radar in the Focused refresh list:

```cpp
    if (g_nav.focused() == desk_display::Screen::Timezones ||
        g_nav.focused() == desk_display::Screen::Weather ||
        g_nav.focused() == desk_display::Screen::Sports ||
        g_nav.focused() == desk_display::Screen::Radar) {
      refresh_content();
    }
```

- [ ] **Step 5: Advance sweep while Radar is visible**

At the end of `dialShellOnTick` (before or after weather/scores blocks; Task 3 will expand), add:

```cpp
  const bool radar_active = g_nav.active_screen() == Screen::Radar;
  if (radar_active) {
    g_radar.onTick(elapsed_ms);
    refresh_content();
  }
```

(Task 3 will fold poll + conditional refresh so we do not double-refresh awkwardly; for this task, sweep-only refresh is enough.)

- [ ] **Step 6: Build Dial**

Run: `export PATH="$HOME/.platformio/penv311/bin:$PATH" && pio run -e dial`

Expected: success. Flash optional: Focus Radar shows rings + sweep with no aircraft yet.

- [ ] **Step 7: Commit**

```bash
git add src/hal/dial_shell.cpp
git commit -m "$(cat <<'EOF'
feat: mount shared Classic radar LVGL on Dial

Replace Radar stub; Focused rotate zooms; sweep advances while visible.
EOF
)"
```

---

### Task 3: Live ADS-B + map-context polling on Dial

**Files:**
- Modify: `src/hal/dial_shell.cpp`
- Test: `pio run -e dial`; on-device Serial verification

**Interfaces:**
- Consumes: `AdsbPoller`, `MapContextPoller`, `desk_net::httpGet`
- Produces: binds while Radar active; Serial `adsb: bound` / `map: bound`; ≤1 Radar GET per tick

- [ ] **Step 1: Add poller globals + one-GET-per-tick wrapper**

```cpp
#include "desk_display/adsb_poll.hpp"
#include "desk_display/map_context_poll.hpp"
```

```cpp
desk_display::AdsbPoller g_adsb_poll;
desk_display::MapContextPoller g_map_ctx_poll;
bool g_radar_http_used = false;

bool dialRadarHttpGet(const char* url, char* body, std::size_t bodyCap,
                      std::size_t& bodyLen, void* user) {
  if (g_radar_http_used) {
    return false;  // Defer — poller retries next tick.
  }
  g_radar_http_used = true;
  return desk_net::httpGet(url, body, bodyCap, bodyLen, user);
}
```

- [ ] **Step 2: Init pollers in `dialShellInit`**

Alongside weather/scores:

```cpp
  g_adsb_poll.setHttpGet(&dialRadarHttpGet, nullptr);
  g_map_ctx_poll.setHttpGet(&dialRadarHttpGet, nullptr);
```

Optional POIs (only if build already defines them — do not invent config):

```cpp
#if defined(RADAR_POI_COUNT)
  g_radar.setPois(RADAR_POIS, static_cast<std::size_t>(RADAR_POI_COUNT));
#endif
```

- [ ] **Step 3: Replace Task 2’s radar-only tick block with full poll loop**

Inside `dialShellOnTick`, after weather/scores handling (or integrated cleanly), use:

```cpp
  const bool radar_active = g_nav.active_screen() == Screen::Radar;
  g_radar_http_used = false;
  g_adsb_poll.setActive(radar_active);
  g_map_ctx_poll.setActive(radar_active);

  if (radar_active) {
    g_radar.onTick(elapsed_ms);
    g_adsb_poll.setCenter(g_radar.centerLat(), g_radar.centerLon(),
                          g_radar.rangeMiles());
    g_map_ctx_poll.setCenter(g_radar.centerLat(), g_radar.centerLon(),
                             g_radar.rangeMiles());
  }

  // Map first so when both are due, ADS-B defers via dialRadarHttpGet.
  g_map_ctx_poll.onTick(elapsed_ms);
  g_adsb_poll.onTick(elapsed_ms);

  bool radar_dirty = radar_active;

  desk_display::MapContext fresh_map{};
  if (g_map_ctx_poll.takeContext(fresh_map)) {
    g_radar.bindMapContext(fresh_map);
    Serial.println("map: bound");
    radar_dirty = true;
  }

  desk_display::AircraftList fresh_ac{};
  if (g_adsb_poll.takeAircraft(fresh_ac)) {
    g_radar.bind(fresh_ac);
    Serial.println("adsb: bound");
    radar_dirty = true;
  }

  if (radar_dirty) {
    refresh_content();
  }
```

Remove the earlier Task 2 duplicate `if (radar_active) { onTick; refresh; }` so sweep refresh happens once via `radar_dirty`.

Keep Nav rebuild **before** this HTTP block (existing Weather comment / order).

- [ ] **Step 4: Build Dial + sim**

Run:

```bash
export PATH="$HOME/.platformio/penv311/bin:$PATH"
pio test -e native
pio run -e dial
pio run -e sim
```

Expected: all succeed.

- [ ] **Step 5: On-device check (manual)**

```bash
pio run -e dial -t upload --upload-port /dev/cu.usbmodem*
```

Serial expectations when focusing Radar:

- `http: GET … ok` (map-context and/or adsb.lol)
- `map: bound` and `adsb: bound`
- Sweep moves; rotate changes range in header
- Carousel both directions without reboot

If TLS OOM: confirm pollers no longer have 256KB BSS (`sizeof` / map file); ensure Weather/Scores still heap-sized; do not raise Radar caps without evidence.

- [ ] **Step 6: Commit**

```bash
git add src/hal/dial_shell.cpp
git commit -m "$(cat <<'EOF'
feat: poll adsb.lol and map-context on Dial Radar

Serialize Radar HTTPS to one GET per tick; bind traffic and overlays while active.
EOF
)"
```

---

### Task 4: Handoff doc

**Files:**
- Modify: `docs/HANDOFF-dial-bringup.md`

- [ ] **Step 1: Update device status + what’s next**

In `docs/HANDOFF-dial-bringup.md`:

- Carousel line: Clock + Timezones + Weather + Sports + **Radar real** (not stub)
- Add Radar Serial notes under flash/tooling: Focus Radar → `adsb: bound` / `map: bound`
- Note Dial A limits: no blip select / Detail / settings (sim-only until CST816 XY pass)
- **What’s next:** Dial Radar input (path to C) → then Focused full-bleed visual pass
- Architecture line already lists `radar_lvgl` — keep; remove stub implication for Radar

- [ ] **Step 2: Commit**

```bash
git add docs/HANDOFF-dial-bringup.md
git commit -m "$(cat <<'EOF'
docs: handoff after Dial Radar Classic + live poll

EOF
)"
```

---

## Spec coverage (self-review)

| Spec requirement | Task |
|------------------|------|
| Replace stub with `radar_lvgl` + `ScreenRadar` | 2 |
| Live adsb.lol + `/api/map-context` while Radar active | 3 |
| Focused rotate = zoom | 2 |
| ClassicSweep only; no Dial hit-test/settings | 2–3 (never wire) |
| Heap 64KB poller bodies | 1 |
| ≤1 Radar GET per tick; map preferred | 3 (`dialRadarHttpGet`) |
| Rebuild UI before blocking HTTP | 3 (preserve order) |
| Serial bind lines; no reboot carousel | 3–4 |
| Native + dial + sim build | 1, 3 |
| Handoff / C follow-up | 4 |
| NVS settings / fixtures / background thread | Out of scope — omitted |

No placeholders remain; poller APIs match existing headers; Dial wrapper name `dialRadarHttpGet` is consistent across Task 3 steps.
