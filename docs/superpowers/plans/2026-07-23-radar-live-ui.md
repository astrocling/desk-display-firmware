# Radar Live UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the radar screen feel live — rotating Classic Sweep, Detail ATC-lite symbology from real ADS-B fields only, and ~10s adsb.lol polling while Radar is the active carousel screen — with shared view-model + LVGL for sim and `screen_radar_*`.

**Architecture:** Keep parsing, formatting, sweep math, and poller state in `desk_display` (no LVGL). Sim owns host HTTP + UI loop; a shared `src/ui/radar_lvgl.*` renderer draws from `RadarView` and backs both sim and `screen_radar_*` for dial readiness. Poll only when `Nav::active_screen() == Screen::Radar`.

**Tech Stack:** C++17, Unity (`pio test -e native`), LVGL 8.4 + SDL sim, ArduinoJson 7, host HTTP via libcurl (sim) / injectable fetch fn (tests + future ESP)

**Spec:** `docs/superpowers/specs/2026-07-23-radar-live-ui-design.md`

## Global Constraints

- Display **only** real adsb.lol `ac[]` fields — never invent destination, cleared altitude, or missing kinematics
- Altitude text: `F###` if alt ≥ 18000 ft, else `A###` (hundreds of feet)
- Detail: star + vector for all; full 2-line tag **only when selected**
- Classic Sweep: dots + rotating sweep; Detail: hide sweep
- Poll ~10s only when `active_screen() == Radar`; fixture / last-good on failure
- Fixture field `dst` is distance-from-receiver — **not** destination
- `desk_display` must not depend on LVGL
- `bind`/rebuild must **preserve selection by callsign** across polls when the aircraft remains in range; clear only when gone

## File map

| File | Responsibility |
|------|----------------|
| `lib/desk_display/include/desk_display/adsb.hpp` + `src/adsb.cpp` | Parse `track`/`calc_track`, `baro_rate`/`geom_rate` |
| `lib/desk_display/include/desk_display/radar_format.hpp` + `src/radar_format.cpp` | `F`/`A`, tag lines, trend |
| `lib/desk_display/include/desk_display/adsb_poll.hpp` + `src/adsb_poll.cpp` | URL builder, nm conversion, poll timer + inject HTTP |
| `lib/desk_display/include/desk_display/screen_radar.hpp` + `src/screen_radar.cpp` | Sweep tick, selection preserve, richer `RadarView` |
| `src/ui/radar_lvgl.hpp` + `src/ui/radar_lvgl.cpp` | Shared LVGL draw from `RadarView` |
| `src/ui/screen_radar_lvgl.cpp` | Real `screen_radar_*` using shared draw |
| `lib/desk_display/src/screens_stub.cpp` | Weak no-op `screen_radar_*` so dial/sim can override |
| `src/sim/sim_app.cpp` / `.hpp` | Poll loop, call shared LVGL, tick radar |
| `src/sim/sim_http.cpp` / `.hpp` | Host HTTP GET (libcurl) for poller |
| `test/test_domain/test_main.cpp` | Format + URL + nm tests |
| `test/test_screen_radar/test_main.cpp` | Parse extensions, sweep, selection preserve, poller |
| `docs/SIM.md` | Note live ADS-B when Radar active |

---

### Task 1: Parse track and vertical rate

**Files:**
- Modify: `lib/desk_display/include/desk_display/adsb.hpp`
- Modify: `lib/desk_display/src/adsb.cpp`
- Modify: `test/test_screen_radar/test_main.cpp`

**Interfaces:**
- Extends `Aircraft`:
  - `float trackDeg; bool hasTrack;`
  - `float baroRateFpm; bool hasBaroRate;`
- `parseAdsb`: set `hasTrack` from numeric `track`, else numeric `calc_track`; set `hasBaroRate` from numeric `baro_rate`, else numeric `geom_rate`. Leave flags false when absent.

- [ ] **Step 1: Write failing tests** in `test/test_screen_radar/test_main.cpp`:

```cpp
void test_parse_adsb_track_and_rate(void) {
  const char* json =
      "{\"ac\":[{\"hex\":\"abc\",\"flight\":\"TST1  \",\"lat\":40.0,\"lon\":-84.0,"
      "\"alt_baro\":5200,\"gs\":106.2,\"track\":75.83,\"baro_rate\":-192}]}";
  AircraftList list{};
  TEST_ASSERT_TRUE(parseAdsb(json, list));
  TEST_ASSERT_EQUAL(1, list.count);
  TEST_ASSERT_TRUE(list.items[0].hasTrack);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 75.83f, list.items[0].trackDeg);
  TEST_ASSERT_TRUE(list.items[0].hasBaroRate);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, -192.0f, list.items[0].baroRateFpm);
}

void test_parse_adsb_calc_track_and_geom_rate_fallback(void) {
  const char* json =
      "{\"ac\":[{\"hex\":\"def\",\"lat\":40.0,\"lon\":-84.0,"
      "\"calc_track\":309,\"geom_rate\":448}]}";
  AircraftList list{};
  TEST_ASSERT_TRUE(parseAdsb(json, list));
  TEST_ASSERT_TRUE(list.items[0].hasTrack);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 309.0f, list.items[0].trackDeg);
  TEST_ASSERT_TRUE(list.items[0].hasBaroRate);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 448.0f, list.items[0].baroRateFpm);
}

void test_parse_adsb_missing_track_rate(void) {
  const char* json =
      "{\"ac\":[{\"hex\":\"ghi\",\"lat\":40.0,\"lon\":-84.0,\"alt_baro\":1000}]}";
  AircraftList list{};
  TEST_ASSERT_TRUE(parseAdsb(json, list));
  TEST_ASSERT_FALSE(list.items[0].hasTrack);
  TEST_ASSERT_FALSE(list.items[0].hasBaroRate);
}
```

Register with `RUN_TEST(...)`.

- [ ] **Step 2: Run** `pio test -e native -f test_screen_radar` — expect FAIL (missing fields / compile error)

- [ ] **Step 3: Implement** fields on `Aircraft` and parser branches in `adsb.cpp` (mirror existing numeric checks for `alt_baro` / `gs`)

- [ ] **Step 4: Re-run** `pio test -e native -f test_screen_radar` — expect PASS

- [ ] **Step 5: Commit**

```bash
git add lib/desk_display/include/desk_display/adsb.hpp lib/desk_display/src/adsb.cpp test/test_screen_radar/test_main.cpp
git commit -m "$(cat <<'EOF'
feat: parse ADS-B track and vertical rate

EOF
)"
```

---

### Task 2: Radar altitude / tag formatting

**Files:**
- Create: `lib/desk_display/include/desk_display/radar_format.hpp`
- Create: `lib/desk_display/src/radar_format.cpp`
- Modify: `test/test_domain/test_main.cpp`

**Interfaces:**
- `constexpr float kRadarFlightLevelMinFt = 18000.0f;`
- `constexpr float kRadarBaroRateDeadbandFpm = 100.0f;`
- `enum class RadarTrend : uint8_t { None = 0, Climb = 1, Descend = 2 };`
- `RadarTrend radarTrendFromRate(float baroRateFpm, bool hasBaroRate);`
- `bool formatRadarAltitude(char* buf, std::size_t bufLen, float altFt);`  
  Rounding: `hundreds = (int)lroundf(altFt / 100.0f)`; if `altFt >= 18000` print `F%03d` else `A%03d`.  
  Examples: 33000 → `F330`; 5200 → `A052`; 18000 → `F180`; 17999 → `A180`.
- `bool formatRadarSpeed(char* buf, std::size_t bufLen, float speedKt);` → `G475` (round knots)
- `bool formatRadarTagLine2(char* buf, std::size_t bufLen, const Aircraft& ac);`  
  Builds e.g. `F330 G475` or `A045 ^ G106` using ASCII `^`/`v` for trend. Omits missing segments. Returns false if nothing to write.

- [ ] **Step 1: Write failing tests** in `test/test_domain/test_main.cpp`:

```cpp
void test_format_radar_altitude_examples(void) {
  char buf[8];
  TEST_ASSERT_TRUE(formatRadarAltitude(buf, sizeof(buf), 33000.0f));
  TEST_ASSERT_EQUAL_STRING("F330", buf);
  TEST_ASSERT_TRUE(formatRadarAltitude(buf, sizeof(buf), 5200.0f));
  TEST_ASSERT_EQUAL_STRING("A052", buf);
  TEST_ASSERT_TRUE(formatRadarAltitude(buf, sizeof(buf), 18000.0f));
  TEST_ASSERT_EQUAL_STRING("F180", buf);
  TEST_ASSERT_TRUE(formatRadarAltitude(buf, sizeof(buf), 17999.0f));
  TEST_ASSERT_EQUAL_STRING("A180", buf);
}

void test_radar_trend_deadband(void) {
  TEST_ASSERT_EQUAL(static_cast<int>(RadarTrend::None),
                    static_cast<int>(radarTrendFromRate(50.0f, true)));
  TEST_ASSERT_EQUAL(static_cast<int>(RadarTrend::Climb),
                    static_cast<int>(radarTrendFromRate(128.0f, true)));
  TEST_ASSERT_EQUAL(static_cast<int>(RadarTrend::Descend),
                    static_cast<int>(radarTrendFromRate(-192.0f, true)));
  TEST_ASSERT_EQUAL(static_cast<int>(RadarTrend::None),
                    static_cast<int>(radarTrendFromRate(999.0f, false)));
}

void test_format_radar_tag_line2_omits_missing(void) {
  Aircraft ac{};
  ac.hasAlt = true;
  ac.altFt = 33000.0f;
  ac.hasSpeed = true;
  ac.speedKt = 474.5f;
  ac.hasBaroRate = false;
  char buf[32];
  TEST_ASSERT_TRUE(formatRadarTagLine2(buf, sizeof(buf), ac));
  TEST_ASSERT_EQUAL_STRING("F330 G475", buf);

  Aircraft onlyAlt{};
  onlyAlt.hasAlt = true;
  onlyAlt.altFt = 4500.0f;
  TEST_ASSERT_TRUE(formatRadarTagLine2(buf, sizeof(buf), onlyAlt));
  TEST_ASSERT_EQUAL_STRING("A045", buf);

  Aircraft empty{};
  TEST_ASSERT_FALSE(formatRadarTagLine2(buf, sizeof(buf), empty));
}
```

Include `desk_display/radar_format.hpp` and `desk_display/adsb.hpp`. Register `RUN_TEST`.

- [ ] **Step 2: Run** `pio test -e native -f test_domain` — expect FAIL

- [ ] **Step 3: Implement** `radar_format.hpp` / `.cpp`

- [ ] **Step 4: Re-run** — PASS

- [ ] **Step 5: Commit**

```bash
git add lib/desk_display/include/desk_display/radar_format.hpp lib/desk_display/src/radar_format.cpp test/test_domain/test_main.cpp
git commit -m "$(cat <<'EOF'
feat: add radar F/A altitude and tag formatting

EOF
)"
```

---

### Task 3: ScreenRadar sweep tick + selection preserve + view fields

**Files:**
- Modify: `lib/desk_display/include/desk_display/screen_radar.hpp`
- Modify: `lib/desk_display/src/screen_radar.cpp`
- Modify: `test/test_screen_radar/test_main.cpp`

**Interfaces:**
- `constexpr float kRadarSweepDegPerSec = 150.0f;`
- `void ScreenRadar::onTick(uint32_t elapsedMs);` — advances `sweepAngleDeg_` by `elapsedMs * kRadarSweepDegPerSec / 1000`, wraps `[0, 360)`. `reset()` sets sweep to `0`.
- Change `bind` selection behavior:
  - Before rebuild, if `hasSelection_`, copy selected callsign.
  - After rebuild, reselect matching callsign if found; else `clearSelection()`.
- `RadarView` adds: `float sweepAngleDeg;`
- `RadarDetailCard` adds:
  - `char tagLine2[24];`
  - `char altLabel[8];`
  - `char speedLabel[8];`  
  Filled via `radar_format` when selection present; empty strings when missing.

- [ ] **Step 1: Write failing tests**

```cpp
void test_radar_sweep_advances_and_wraps(void) {
  ScreenRadar screen;
  screen.bind(loadAdsbFixture());
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, screen.view().sweepAngleDeg);
  screen.onTick(1000);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 150.0f, screen.view().sweepAngleDeg);
  screen.onTick(2000);  // +300 → 450 → 90
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 90.0f, screen.view().sweepAngleDeg);
}

void test_radar_bind_preserves_selection_by_callsign(void) {
  ScreenRadar screen;
  AircraftList list = loadAdsbFixture();
  screen.bind(list);
  std::size_t idx = 0;
  bool found = false;
  for (std::size_t i = 0; i < screen.blipCount(); ++i) {
    if (std::strcmp(screen.blip(i).aircraft.callsign, "UAL2402") == 0) {
      idx = i;
      found = true;
      break;
    }
  }
  TEST_ASSERT_TRUE(found);
  TEST_ASSERT_TRUE(screen.selectBlip(idx));
  screen.bind(list);
  TEST_ASSERT_TRUE(screen.hasSelection());
  TEST_ASSERT_EQUAL_STRING("UAL2402", screen.detailCard().callsign);
}

void test_radar_bind_clears_selection_when_gone(void) {
  ScreenRadar screen;
  AircraftList list = loadAdsbFixture();
  screen.bind(list);
  TEST_ASSERT_TRUE(screen.selectBlip(0));
  AircraftList other{};
  other.count = 1;
  std::snprintf(other.items[0].callsign, sizeof(other.items[0].callsign), "ZZZZZZ");
  other.items[0].hasPosition = true;
  other.items[0].lat = kRadarHomeLat;
  other.items[0].lon = kRadarHomeLon;
  screen.bind(other);
  TEST_ASSERT_FALSE(screen.hasSelection());
}
```

Update any existing test that assumed `bind` always clears selection.

- [ ] **Step 2: Run** `pio test -e native -f test_screen_radar` — FAIL

- [ ] **Step 3: Implement** `onTick`, selection preserve, view/detail fields

- [ ] **Step 4: Re-run** — PASS

- [ ] **Step 5: Commit**

```bash
git add lib/desk_display/include/desk_display/screen_radar.hpp lib/desk_display/src/screen_radar.cpp test/test_screen_radar/test_main.cpp
git commit -m "$(cat <<'EOF'
feat: radar sweep tick and selection-preserving bind

EOF
)"
```

---

### Task 4: AdsbPoller (URL + timer + injectable HTTP)

**Files:**
- Create: `lib/desk_display/include/desk_display/adsb_poll.hpp`
- Create: `lib/desk_display/src/adsb_poll.cpp`
- Modify: `lib/desk_display/include/desk_display/radar.hpp`
- Modify: `lib/desk_display/src/radar.cpp`
- Modify: `test/test_domain/test_main.cpp`
- Modify: `test/test_screen_radar/test_main.cpp`

**Interfaces:**

```cpp
// radar.hpp
float statuteMilesToNauticalMiles(float statuteMi);  // statuteMi * 0.868976f
float clampAdsbQueryRadiusNm(float nm);              // clamp to [1, 250]

// adsb_poll.hpp
constexpr uint32_t kAdsbPollIntervalMs = 10000;

using AdsbHttpGetFn = bool (*)(const char* url, char* body, std::size_t bodyCap,
                               std::size_t& bodyLen, void* user);

bool buildAdsbLolUrl(char* buf, std::size_t bufLen, double lat, double lon,
                     float rangeStatuteMi);
// https://api.adsb.lol/v2/lat/{lat}/lon/{lon}/dist/{radiusNm}

class AdsbPoller {
 public:
  void setHttpGet(AdsbHttpGetFn fn, void* user);
  void setActive(bool radarIsActiveScreen);
  void setCenter(double lat, double lon, float rangeStatuteMi);
  void onTick(uint32_t elapsedMs);
  bool takeAircraft(AircraftList& out);  // true once per success until consumed
  bool hasLastGood() const;
};
```

Behavior: no fetch when inactive; when active, fetch every 10s; success → parse → `takeAircraft`; failure leaves last-good alone.

- [ ] **Step 1: Failing tests**

```cpp
void test_statute_to_nm_and_url(void) {
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 21.7244f, statuteMilesToNauticalMiles(25.0f));
  char url[160];
  TEST_ASSERT_TRUE(buildAdsbLolUrl(url, sizeof(url), 40.03353, -84.19588, 25.0f));
  TEST_ASSERT_NOT_NULL(std::strstr(url, "api.adsb.lol/v2/lat/"));
  TEST_ASSERT_NOT_NULL(std::strstr(url, "/lon/"));
  TEST_ASSERT_NOT_NULL(std::strstr(url, "/dist/"));
}

static int g_http_calls;
static bool fake_http(const char* url, char* body, std::size_t cap,
                      std::size_t& len, void*) {
  (void)url;
  ++g_http_calls;
  const char* json =
      "{\"ac\":[{\"hex\":\"a\",\"flight\":\"P1\",\"lat\":40.03,\"lon\":-84.19,"
      "\"alt_baro\":1000,\"gs\":100,\"track\":90}]}";
  len = std::strlen(json);
  if (len + 1 > cap) return false;
  std::memcpy(body, json, len + 1);
  return true;
}

void test_adsb_poller_only_when_active(void) {
  g_http_calls = 0;
  AdsbPoller poll;
  poll.setHttpGet(fake_http, nullptr);
  poll.setCenter(40.03353, -84.19588, 25.0f);
  poll.setActive(false);
  poll.onTick(15000);
  TEST_ASSERT_EQUAL(0, g_http_calls);
  poll.setActive(true);
  poll.onTick(10000);
  TEST_ASSERT_EQUAL(1, g_http_calls);
  AircraftList list{};
  TEST_ASSERT_TRUE(poll.takeAircraft(list));
  TEST_ASSERT_EQUAL(1, list.count);
  TEST_ASSERT_FALSE(poll.takeAircraft(list));
}
```

- [ ] **Step 2: Run tests — FAIL**

- [ ] **Step 3: Implement** helpers + `AdsbPoller` (internal body buffer ≥ 256 KiB or reuse caller buffer via setHttpGet writing into poller-owned buffer)

- [ ] **Step 4: PASS**

- [ ] **Step 5: Commit**

```bash
git add lib/desk_display/include/desk_display/adsb_poll.hpp lib/desk_display/src/adsb_poll.cpp lib/desk_display/include/desk_display/radar.hpp lib/desk_display/src/radar.cpp test/test_domain/test_main.cpp test/test_screen_radar/test_main.cpp
git commit -m "$(cat <<'EOF'
feat: add AdsbPoller with injectable HTTP

EOF
)"
```

---

### Task 5: Sim HTTP + wire poller to Radar active screen

**Files:**
- Create: `src/sim/sim_http.hpp`
- Create: `src/sim/sim_http.cpp`
- Modify: `src/sim/sim_app.hpp`
- Modify: `src/sim/sim_app.cpp`
- Modify: `platformio.ini` (`[env:sim]` add `-lcurl`)
- Modify: `docs/SIM.md`

**Interfaces:**
- `bool simHttpGet(const char* url, char* body, std::size_t bodyCap, std::size_t& bodyLen);` — libcurl, ~8s timeout, HTTPS
- Trampoline matching `AdsbHttpGetFn`
- `SimApp`: `AdsbPoller adsb_poll_;`
- `update()`:
  - `radar_.onTick(elapsed_ms)` when `active_screen() == Radar`
  - `adsb_poll_.setActive(nav_.active_screen() == Screen::Radar)`
  - `adsb_poll_.setCenter(radar_.centerLat(), radar_.centerLon(), radar_.rangeMiles())` when active
  - `adsb_poll_.onTick(elapsed_ms)`
  - If `takeAircraft(list)` → `radar_.bind(list)` + refresh if Radar visible
  - While Radar + ClassicSweep, refresh content often enough for smooth sweep (each `update` or ≥15 Hz)

`load_fixtures` still binds fixture first so offline boot works.

- [ ] **Step 1: Implement** `sim_http` with libcurl

- [ ] **Step 2: Wire** poller in `SimApp`

- [ ] **Step 3: Build** `pio run -e sim` — success

- [ ] **Step 4: Manual smoke** — Radar active fetches; leave Radar stops fetches (optional log)

- [ ] **Step 5: Update** `docs/SIM.md` Data section for live adsb.lol + fixture fallback

- [ ] **Step 6: Commit**

```bash
git add src/sim/sim_http.hpp src/sim/sim_http.cpp src/sim/sim_app.hpp src/sim/sim_app.cpp platformio.ini docs/SIM.md
git commit -m "$(cat <<'EOF'
feat: poll adsb.lol from sim when Radar is active

EOF
)"
```

---

### Task 6: Shared LVGL radar renderer — Classic Sweep

**Files:**
- Create: `src/ui/radar_lvgl.hpp`
- Create: `src/ui/radar_lvgl.cpp`
- Modify: `src/sim/sim_app.cpp`
- Modify: `platformio.ini` — `[env:sim]` `build_src_filter` add `+<ui/>`

**Interfaces:**

```cpp
namespace desk_ui {
constexpr lv_coord_t kRadarDiscPx = 240;
void radar_lvgl_build(lv_obj_t* parent, const desk_display::RadarView& v);
}
```

Classic path:
- Concentric dim rings (no unstyled `lv_arc` indicator leftover)
- Radial sweep line at `sweepAngleDeg` (0° = north / up, clockwise)
- Aircraft as 6px green dots; scale `110 / rangeMiles`
- Header: `Sweep · %.0f mi · %zu`

Sim Radar case becomes `desk_ui::radar_lvgl_build(body_, radar_.view())`.

- [ ] **Step 1: Implement** Classic path in `radar_lvgl_build`

- [ ] **Step 2: Switch sim** to shared renderer; remove old blue arc

- [ ] **Step 3: Build** `pio run -e sim`

- [ ] **Step 4: Manual** — sweep rotates; dots only; no blue leftover arc

- [ ] **Step 5: Commit**

```bash
git add src/ui/radar_lvgl.hpp src/ui/radar_lvgl.cpp src/sim/sim_app.cpp platformio.ini
git commit -m "$(cat <<'EOF'
feat: shared LVGL classic radar sweep renderer

EOF
)"
```

---

### Task 7: Detail mode ATC-lite in shared LVGL

**Files:**
- Modify: `src/ui/radar_lvgl.cpp`
- Modify: `src/sim/sim_app.cpp` (nearest-blip tap)

**Detail path:**
- Rings; **no sweep**
- Per blip (cap 40): star mark + velocity vector if `hasTrack`  
  Track 0° = north = up: `dx = sin(track)*len`, `dy = -cos(track)*len`  
  `len = clamp(speedKt * 0.04f, 8, 28)` (if no speed, use mid length 16)
- Selected: leader + callsign + `tagLine2`
- Bottom card: callsign + `altLabel` + `speedLabel`

Sim tap: nearest blip within ~20px → `selectBlip` / clear; double-tap → `toggleMode()`.

- [ ] **Step 1: Implement** Detail drawing

- [ ] **Step 2: Nearest-blip tap selection in sim

- [ ] **Step 3: Build + manual** — vectors; tag+card on select; Sweep hides vectors

- [ ] **Step 4: Commit**

```bash
git add src/ui/radar_lvgl.cpp src/sim/sim_app.cpp
git commit -m "$(cat <<'EOF'
feat: Detail mode ATC-lite tags and vectors

EOF
)"
```

---

### Task 8: Real `screen_radar_*` + weak stubs

**Files:**
- Create: `src/ui/screen_radar_lvgl.cpp`
- Modify: `lib/desk_display/src/screens_stub.cpp`
- Modify: `lib/desk_display/include/desk_display/screens_stub.hpp`

**Add to header:**

```cpp
void screen_radar_bind_model(desk_display::ScreenRadar* model);
void screen_radar_on_tick(uint32_t elapsed_ms);
```

Existing `create/destroy/show/hide` kept. Stub radar symbols marked `__attribute__((weak))` no-ops. Strong symbols in `src/ui/screen_radar_lvgl.cpp` call shared `radar_lvgl_build` when shown. Sim may keep calling `radar_lvgl_build` directly (avoid double ownership).

- [ ] **Step 1: Weak stubs** + new no-op bind/tick in `screens_stub`

- [ ] **Step 2: Strong** `screen_radar_*` in `src/ui/screen_radar_lvgl.cpp`

- [ ] **Step 3: Build** `pio run -e sim` and `pio run -e dial` — both compile

- [ ] **Step 4: Commit**

```bash
git add src/ui/screen_radar_lvgl.cpp lib/desk_display/src/screens_stub.cpp lib/desk_display/include/desk_display/screens_stub.hpp
git commit -m "$(cat <<'EOF'
feat: implement screen_radar LVGL entry points for dial

EOF
)"
```

---

### Task 9: Regression + docs polish

- [ ] **Step 1: Run** `pio test -e native` — all PASS

- [ ] **Step 2: Manual checklist** (sim)
  - [ ] Sweep rotates in Classic; hidden in Detail
  - [ ] Detail vectors; tag+card only when selected
  - [ ] Live updates ~10s while Radar active; stops when leaving
  - [ ] Offline / failed fetch keeps fixture or last-good
  - [ ] No invented destination fields

- [ ] **Step 3: Commit** any remaining doc nits if needed

---

## Spec coverage check

| Spec requirement | Task |
|------------------|------|
| Classic rotating sweep | 3, 6 |
| Detail star + vector | 7 |
| Selected-only ATC-lite tag | 2, 3, 7 |
| F/A altitude ≥18000 | 2 |
| Real fields only / omit missing | 1, 2, 7 |
| Poll ~10s when Radar selected | 4, 5 |
| Fixture / last-good fallback | 4, 5 |
| Selection preserve / clear when gone | 3 |
| Shared sim + `screen_radar_*` LVGL | 6, 7, 8 |
| No arrival / trails / ICAO UI | non-goals |
| Unit tests | 1–4, 9 |

## Locked implementation constants

- `kRadarSweepDegPerSec = 150`
- `kRadarBaroRateDeadbandFpm = 100`
- Trend in domain strings: ASCII `^` / `v`
- `bind` preserves selection by callsign
