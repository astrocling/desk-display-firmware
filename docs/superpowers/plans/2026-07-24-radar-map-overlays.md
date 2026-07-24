# Radar Map Overlays Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Draw towered-airport markers, config POIs, and Class B/C/D airspace rings under live ADS-B traffic on the radar PPI, using a cost-light cached `/api/map/context` backend.

**Architecture:** Parse map-context JSON in `desk_display` (no LVGL). `ScreenRadar` holds projected static marks + rings in `RadarView`. Shared `radar_lvgl` paints hydro-less layers under traffic. Backend seeds pre-joined TWR airports and pre-simplified B/C/D rings into Redis once; a thin GET filters by lat/lon/radius with long `Cache-Control`. Hydrography is **out of this plan** (follow-up: static tiles only).

**Tech Stack:** C++17, Unity (`pio test -e native`), LVGL 8.4, ArduinoJson 7; backend Next.js + Upstash Redis + Vitest (sibling repo `desk-display-backend`)

**Spec:** `docs/superpowers/specs/2026-07-24-radar-map-overlays-design.md`

## Global Constraints

- Towered = OurAirports airports ⨝ frequencies where type includes **TWR** (no size proxy)
- Airspace classes **B / C / D** only; D drawn **dashed**; B solid blue; C solid magenta (muted for dark PPI)
- No fill, no ceiling labels, no Class E / SUA / STARS routes
- Backend: **no live GIS/rasterize** in the request path; build-time seed + Redis read + haversine filter + long CDN cache
- Prefer **one** `GET /api/map/context` (not separate airports/airspace routes in production)
- Device draw caps: ≤20 airports, ≤10 POIs, ≤8 rings (thin D first)
- Paint order: airspace → airports/POIs → aircraft → sweep/HUD
- `desk_display` must not depend on LVGL
- POIs are on-device config only (zero backend)
- Hydrography deferred — do not implement in this plan

## File map

### Firmware (`desktop-display-firmware`)

| File | Responsibility |
|------|----------------|
| `lib/desk_display/include/desk_display/map_context.hpp` + `src/map_context.cpp` | Types + `parseMapContext` |
| `lib/desk_display/include/desk_display/map_context_poll.hpp` + `src/map_context_poll.cpp` | Debounced HTTP poller for map context |
| `lib/desk_display/include/desk_display/radar_poi.hpp` | Compile-time POI struct + apply helper |
| `lib/desk_display/include/desk_display/screen_radar.hpp` + `src/screen_radar.cpp` | Bind overlays, project offsets, static selection, extend `RadarView` |
| `src/ui/radar_lvgl.hpp` + `src/ui/radar_lvgl.cpp` | Draw rings/markers; static hit-test |
| `include/config.h.example` | `RADAR_POIS[]` sample entries |
| `fixtures/map_context_dayton.json` | Offline map-context fixture |
| `test/test_parsers/test_main.cpp` or `test/test_screen_radar/test_main.cpp` | Parse + view-model tests |
| `src/sim/sim_app.cpp` | Wire poller + fixture fallback |
| `docs/FIRMWARE_PLAN.md` / `docs/BACKEND_PLAN.md` | Contract sync |

### Backend (`../desk-display-backend`)

| File | Responsibility |
|------|----------------|
| `scripts/build-map-context-data.ts` | Offline TWR join + airspace simplify → JSON artifacts |
| `src/lib/fetchers/map_context.ts` | Seed Redis + in-memory filter helpers |
| `src/lib/fetchers/map_context.test.ts` | Vitest for filter / TWR join fixtures |
| `src/app/api/map/context/route.ts` | Cheap GET + Cache-Control |
| `src/app/api/cron/seed-map-context/route.ts` | Optional cron/manual re-seed |
| `src/lib/config.ts` | `REDIS_KEYS.mapTowered`, `mapAirspace` |

---

### Task 1: Parse map-context JSON (firmware)

**Files:**
- Create: `lib/desk_display/include/desk_display/map_context.hpp`
- Create: `lib/desk_display/src/map_context.cpp`
- Create: `fixtures/map_context_dayton.json`
- Modify: `test/test_parsers/test_main.cpp` (or add cases to `test/test_screen_radar/test_main.cpp` if parsers env lacks ArduinoJson linkage — match existing airport tests)

**Interfaces:**
- Produces:
  - `enum class AirspaceClass : uint8_t { B, C, D };`
  - `struct MapAirport { char icao[8]; char name[48]; double lat; double lon; };`
  - `struct MapAirspaceRing { AirspaceClass cls; char id[24]; float pointsLat[64]; float pointsLon[64]; uint8_t pointCount; };`
  - `struct MapContext { MapAirport airports[40]; std::size_t airportCount; MapAirspaceRing rings[16]; std::size_t ringCount; };`
  - `bool parseMapContext(const char* json, MapContext& out);`

- [ ] **Step 1: Add fixture** `fixtures/map_context_dayton.json`:

```json
{
  "airports": [
    {
      "icao": "KDAY",
      "name": "James M Cox Dayton Intl",
      "lat": 39.902401,
      "lon": -84.219398
    },
    {
      "icao": "KFFO",
      "name": "Wright-Patterson AFB",
      "lat": 39.826111,
      "lon": -84.048332
    }
  ],
  "rings": [
    {
      "class": "D",
      "id": "KDAY_D",
      "points": [
        [39.92, -84.25],
        [39.93, -84.20],
        [39.88, -84.18],
        [39.87, -84.24]
      ]
    }
  ]
}
```

- [ ] **Step 2: Write failing tests**

```cpp
void test_parse_map_context_dayton(void) {
  // load fixtures/map_context_dayton.json into g_buf (same helper as airport fixture)
  MapContext ctx{};
  TEST_ASSERT_TRUE(parseMapContext(g_buf, ctx));
  TEST_ASSERT_EQUAL(2, ctx.airportCount);
  TEST_ASSERT_EQUAL_STRING("KDAY", ctx.airports[0].icao);
  TEST_ASSERT_EQUAL(1, ctx.ringCount);
  TEST_ASSERT_TRUE(ctx.rings[0].cls == AirspaceClass::D);
  TEST_ASSERT_TRUE(ctx.rings[0].pointCount >= 4);
}

void test_parse_map_context_rejects_bad_class(void) {
  MapContext ctx{};
  TEST_ASSERT_TRUE(parseMapContext(
      "{\"airports\":[],\"rings\":[{\"class\":\"E\",\"id\":\"x\",\"points\":[[1,2],[3,4],[5,6]]}]}",
      ctx));
  TEST_ASSERT_EQUAL(0, ctx.ringCount);  // skip unknown classes
}
```

- [ ] **Step 3: Run tests — expect FAIL** (symbol missing)

```bash
pio test -e native -f test_parsers
# or: pio test -e native -f test_screen_radar
```

- [ ] **Step 4: Implement** `map_context.hpp` / `map_context.cpp` with ArduinoJson: require `lat`/`lon`/`icao` for airports; parse `class` as `"B"|"C"|"D"`; clamp points to 64; ignore malformed rings.

- [ ] **Step 5: Run tests — expect PASS**

- [ ] **Step 6: Commit**

```bash
git add lib/desk_display/include/desk_display/map_context.hpp \
  lib/desk_display/src/map_context.cpp fixtures/map_context_dayton.json \
  test/test_parsers/test_main.cpp
git commit -m "$(cat <<'EOF'
feat: parse radar map-context JSON for airports and airspace rings

EOF
)"
```

---

### Task 2: ScreenRadar overlay bind + projection + static selection

**Files:**
- Modify: `lib/desk_display/include/desk_display/screen_radar.hpp`
- Modify: `lib/desk_display/src/screen_radar.cpp`
- Modify: `test/test_screen_radar/test_main.cpp`

**Interfaces:**
- Consumes: `MapContext`, `aircraftOffsetMiles`, `distanceMiles`
- Produces on `RadarView`:
  - `struct RadarStaticMark { enum Kind : uint8_t { Airport, Poi } kind; char label[48]; float offsetXMi; float offsetYMi; };`
  - `struct RadarAirspaceRingView { AirspaceClass cls; float offsetXMi[64]; float offsetYMi[64]; uint8_t pointCount; };`
  - `const RadarStaticMark* staticMarks; std::size_t staticMarkCount;`
  - `const RadarAirspaceRingView* airspaceRings; std::size_t airspaceRingCount;`
  - `bool hasStaticSelection; std::size_t selectedStaticIndex;`
- Methods:
  - `void bindMapContext(const MapContext& ctx);` — keep raw lat/lon; reproject on center/range change
  - `void setPois(const RadarPoi* pois, std::size_t count);` — max 10
  - `bool selectStaticMark(std::size_t index);` / `void clearStaticSelection();`
  - Selecting a blip clears static selection and vice versa
- Caps when projecting: nearest ≤20 airports in range; ≤8 rings (drop `D` first, then farthest by centroid distance)

- [ ] **Step 1: Write failing tests**

```cpp
void test_radar_bind_map_context_projects_airport(void) {
  ScreenRadar r;
  MapContext ctx{};
  // one airport 10 mi due east of home — set lat/lon via known offset
  ctx.airportCount = 1;
  std::snprintf(ctx.airports[0].icao, sizeof(ctx.airports[0].icao), "KTEST");
  ctx.airports[0].lat = kRadarHomeLat;
  ctx.airports[0].lon = kRadarHomeLon + 10.0 / (69.0 * std::cos(kRadarHomeLat * 0.017453292519943295));
  r.bindMapContext(ctx);
  auto v = r.view();
  TEST_ASSERT_EQUAL(1, v.staticMarkCount);
  TEST_ASSERT_FLOAT_WITHIN(1.5f, 10.0f, v.staticMarks[0].offsetXMi);
  TEST_ASSERT_FLOAT_WITHIN(1.5f, 0.0f, v.staticMarks[0].offsetYMi);
}

void test_radar_static_select_clears_blip_select(void) {
  // bind fixture aircraft + map context; selectBlip(0); selectStaticMark(0);
  // TEST_ASSERT_FALSE(r.hasSelection());
  // TEST_ASSERT_TRUE(r.view().hasStaticSelection);
}
```

- [ ] **Step 2: Run — expect FAIL**

```bash
pio test -e native -f test_screen_radar
```

- [ ] **Step 3: Implement** bind/store/reproject/`RadarView` fields; call reproject from `applyRange`, `setActiveCenter`, and `bindMapContext`.

- [ ] **Step 4: Run — expect PASS**

- [ ] **Step 5: Commit**

```bash
git commit -m "$(cat <<'EOF'
feat: bind map-context overlays into ScreenRadar view-model

EOF
)"
```

---

### Task 3: Config POIs

**Files:**
- Create: `lib/desk_display/include/desk_display/radar_poi.hpp`
- Modify: `include/config.h.example`
- Modify: `test/test_screen_radar/test_main.cpp`
- Modify: `src/sim/sim_app.cpp` (call `setPois` from config when available)

**Interfaces:**
- Produces: `struct RadarPoi { const char* name; double lat; double lon; };`
- `config.h.example`:

```cpp
static const desk_display::RadarPoi RADAR_POIS[] = {
    {"Home", HOME_LAT, HOME_LON},
};
static const int RADAR_POI_COUNT =
    (int)(sizeof(RADAR_POIS) / sizeof(RADAR_POIS[0]));
```

- [ ] **Step 1: Failing test** — `setPois` adds a mark with `Kind::Poi` and label `"Home"`.

- [ ] **Step 2: Implement header + wire sim; document in `config.h.example`.**

- [ ] **Step 3: Tests PASS + commit**

```bash
git commit -m "$(cat <<'EOF'
feat: support curated radar POIs from device config

EOF
)"
```

---

### Task 4: LVGL draw airspace + static marks + hit-test

**Files:**
- Modify: `src/ui/radar_lvgl.hpp`
- Modify: `src/ui/radar_lvgl.cpp`
- Modify: sim / dial touch handlers that call `radar_lvgl_hit_blip` (extend to try static marks first or second per spec: aircraft vs static — **aircraft tap wins if both hit**; try blip first then static)

**Interfaces:**
- Colors (module constants):
  - Class B: `0x3A6AA8` solid
  - Class C: `0xA83A7A` solid
  - Class D: `0x3A6AA8` dashed (`line_dash_width` 4, `line_dash_gap` 3)
  - Airport mark: `0xFFFFFF`
  - POI mark: `0xAAAAAA`
- Draw airspace on a layer under `g_blips_layer`; marks on same static layer under traffic
- Selected static: small `lv_label` with mark label near the glyph
- Produces: `bool radar_lvgl_hit_static(lv_obj_t* parent, const RadarView& v, lv_coord_t absX, lv_coord_t absY, std::size_t* outIndex);`

- [ ] **Step 1: Extend `radar_lvgl_build` / refresh path** to call `build_airspace` then `build_static_marks` before `build_traffic`.

- [ ] **Step 2: Implement dashed D via LVGL line dash styles; solid B/C.**

- [ ] **Step 3: Wire sim tap:** if `radar_lvgl_hit_blip` → aircraft select; else if `radar_lvgl_hit_static` → `selectStaticMark`.

- [ ] **Step 4: Manual check in sim** (`pio run -e sim`): fixture map context shows KDAY mark + dashed D ring under traffic.

- [ ] **Step 5: Commit**

```bash
git commit -m "$(cat <<'EOF'
feat: draw airspace rings and airport/POI marks on radar LVGL

EOF
)"
```

---

### Task 5: Map-context poller + sim fixture fallback

**Files:**
- Create: `lib/desk_display/include/desk_display/map_context_poll.hpp`
- Create: `lib/desk_display/src/map_context_poll.cpp`
- Modify: `src/sim/sim_app.cpp` / `.hpp`
- Modify: `test/test_screen_radar/test_main.cpp`

**Interfaces:**
- Consumes: same `AdsbHttpGetFn`-shaped callback (or shared HTTP typedef)
- `class MapContextPoller`:
  - `void setHttpGet(AdsbHttpGetFn fn, void* user);`
  - `void setActive(bool);`
  - `void setCenter(double lat, double lon, float rangeMi);` — starts **400 ms** debounce; only GET after settle
  - `void onTick(uint32_t elapsedMs);`
  - `bool takeContext(MapContext& out);`
- URL: `{API_BASE}/api/map/context?lat=..&lon=..&radiusMi=..` (use existing backend base from config)
- On failure: keep last good; sim boots from `fixtures/map_context_dayton.json` if never succeeded

- [ ] **Step 1: Failing test** — rapid `setCenter` calls do not invoke HTTP until 400 ms quiet; then one GET.

- [ ] **Step 2: Implement poller.**

- [ ] **Step 3: Wire in `SimApp` alongside `AdsbPoller`; on take → `radar_.bindMapContext`.**

- [ ] **Step 4: Tests PASS + commit**

```bash
git commit -m "$(cat <<'EOF'
feat: debounce and poll /api/map/context for radar overlays

EOF
)"
```

---

### Task 6: Backend — offline TWR + airspace data build

**Repo:** `desk-display-backend` (sibling)

**Files:**
- Create: `scripts/build-map-context-data.ts`
- Create: `data/map/towered-airports.json` (generated, committed or artifact — prefer commit small US/global TWR list)
- Create: `data/map/airspace-rings.json` (generated B/C/D simplified rings)
- Create: `src/lib/fetchers/map_context.ts`
- Create: `src/lib/fetchers/map_context.test.ts`
- Modify: `src/lib/config.ts` — add Redis keys
- Modify: `package.json` — script `build:map-context`

**Interfaces:**
- `buildToweredAirports()`: download/cached OurAirports `airports.csv` + `airport-frequencies.csv`; keep rows whose airport appears with a frequency `type` containing `TWR`; emit `{icao,name,lat,lon}[]`
- `buildAirspaceRings()`: ingest FAA/open Class B/C/D polygons (document exact source URL in script header); Douglas–Peucker simplify to ≤60 verts; emit `{class,id,points: [lat,lon][]}[]`
- `seedMapContextToRedis()`: `SET` two JSON blobs under `REDIS_KEYS.mapTowered` / `mapAirspace`
- `filterMapContext(lat, lon, radiusMi, towered, rings): MapContextResponse` — haversine; sort airports by distance; include ring if any vertex or centroid within radius (or bbox intersects)

- [ ] **Step 1: Vitest** — fixture CSVs with one TWR and one non-TWR airport; assert only TWR emitted; filter returns airport inside radius only.

- [ ] **Step 2: Implement join + filter (no network in unit test — use string fixtures).**

- [ ] **Step 3: Implement build script; run once; commit generated JSON if size reasonable (< ~2 MB). If larger, store gzipped or Redis-only with script documented in README.**

- [ ] **Step 4: Commit in backend repo**

```bash
git commit -m "$(cat <<'EOF'
feat: build and seed towered airports and B/C/D airspace rings

EOF
)"
```

---

### Task 7: Backend — cheap `/api/map/context` route

**Repo:** `desk-display-backend`

**Files:**
- Create: `src/app/api/map/context/route.ts`
- Create: `src/app/api/cron/seed-map-context/route.ts` (cron secret gated like other crons)
- Update: backend API docs / README

**Interfaces:**
- `GET /api/map/context?lat=&lon=&radiusMi=`
  - Validate params; default/clamp radius 5–50
  - Load blobs from Redis (module-level memory cache after first read in warm isolate)
  - `filterMapContext`
  - Response JSON per spec
  - Headers: `Cache-Control: public, s-maxage=86400, max-age=3600, stale-while-revalidate=86400`
- **Must not** download OurAirports or run simplify in this handler

- [ ] **Step 1: Route test or manual curl against local `next dev` with seeded Redis.**

- [ ] **Step 2: Implement route + seed cron.**

- [ ] **Step 3: Seed prod/dev Redis once via script or cron.**

- [ ] **Step 4: Commit**

```bash
git commit -m "$(cat <<'EOF'
feat: add cached /api/map/context endpoint

EOF
)"
```

---

### Task 8: Docs sync (firmware)

**Files:**
- Modify: `docs/FIRMWARE_PLAN.md` — move nearby markers from “future” to this design; note B/C/D + TWR + cost rules; link spec
- Modify: `docs/BACKEND_PLAN.md` — document `/api/map/context`; note static seed; hydro still future/static-only
- Modify: `docs/SIM.md` — map overlays + fixture fallback

- [ ] **Step 1: Update the three docs to match the spec (no contradictions with precipitation still future).**

- [ ] **Step 2: Commit**

```bash
git commit -m "$(cat <<'EOF'
docs: sync plans for radar map overlays and map-context API

EOF
)"
```

---

## Out of scope (follow-up plan)

- Hydrography underlay (precomputed static tiles/Blob only — never live rasterize)
- Precipitation image path
- Class E / airways / STARS procedures
- Editable POI UI
- Airspace ceiling labels

## Spec coverage checklist

| Spec item | Task |
|-----------|------|
| TWR join towered airports | 6, 1–2, 4–5 |
| Config POIs | 3 |
| Class B/C/D rings + D dashed | 1–2, 4, 6–7 |
| Combined `/api/map/context` + cache | 5, 7 |
| Cost-light (no live GIS) | 6–7 |
| Caps / thin D first | 2 |
| Selection / labels | 2, 4 |
| Hydro | Explicitly deferred |
| Paint order | 4 |

## Self-review notes

- No hydro tasks in this plan (cost).
- Types `MapContext` / `AirspaceClass` / `RadarStaticMark` used consistently across tasks 1–5.
- Backend paths assume sibling `desk-display-backend`; run those commits in that repo.
