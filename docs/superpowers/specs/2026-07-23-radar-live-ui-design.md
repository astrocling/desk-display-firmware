# Radar live UI — sweep, Detail ATC-lite, adsb.lol poll

**Date:** 2026-07-23  
**Status:** Approved for planning  
**Scope:** Phase 4 radar — Classic Sweep animation, Detail mode with ATC-lite symbology, live adsb.lol polling when Radar is the selected carousel screen, shared view-model + LVGL for sim and `screen_radar_*` (ready for Dial when hardware arrives).

## Goals

- Make radar **feel live**: rotating sweep, moving traffic from real ADS-B, Detail mode that reads like a simplified ATC tag.
- Display **only real adsb.lol aircraft fields** — never invent cleared altitudes, destinations, or missing kinematics.
- Keep domain logic LVGL-free and unit-tested; sim validates UX; `screen_radar_*` stubs become real LVGL so Track C / device can hook the same screen.

## Non-goals (this pass)

- Arrival / origin airport via routeset or any second API
- History trails / coasting dots
- ICAO airport keyboard recenter UI
- Precipitation radar, airport markers, vibration alerts
- Device Wi-Fi / display HAL beyond compiling and wiring the radar screen module
- Changing carousel / nav shell behavior except when to poll

## Product decisions (locked)

| Topic | Choice |
|-------|--------|
| Detail density | ATC-lite: star + velocity vector for all; full 2-line tag **only for selected** aircraft |
| Altitude display | `F###` at ≥ 18,000 ft (flight level, hundreds of feet); `A###` below |
| Data source | Live adsb.lol ~10s when Radar is the **selected carousel screen**; fixture / last-good fallback |
| Architecture | Sim + device LVGL (`screen_radar_*`) in the same pass |
| Arrival airport | Out — not in `ac[]`; separate lookup later if ever |

## UX / layout

### Classic Sweep

- Dim concentric range rings + outer ring
- Cosmetic **rotating sweep** (UI tick; independent of poll)
- Aircraft as small green **dots** only (no vectors / tags)
- Header: `Sweep · {range} mi · {count}`

### Detail

- Same range rings; **hide sweep** so tags stay readable
- Every aircraft: **star** + **velocity vector** (`track` direction; length scaled by `gs` when present)
- **Selected only:** short leader line + 2-line ATC-lite tag  
  - Line 1: callsign  
  - Line 2: altitude (`F`/`A`) + optional ↑/↓ from vertical rate + `G###` ground speed  
  - Omit any piece whose source field is missing — never fake it
- Bottom **detail card** when selected: callsign + altitude + speed (same real fields; richer than callsign-only)
- Header: `Detail · {range} mi · {count}`

### Interaction

Unchanged ownership; wire hit-testing where practical in LVGL:

| Input | Behavior |
|-------|----------|
| Rotate | Zoom range ±5 mi, clamped 5–50 |
| Double-tap / empty-area mode toggle | ClassicSweep ↔ Detail; clears selection |
| Tap blip | Select → tag + card |
| Tap selected / empty (with blips) | Clear selection (keep existing empty/no-blip toggle behavior) |
| Long-press | `pinCenter()` (existing); no ICAO keyboard this pass |
| Center tap | Nav back; `revertTempCenter()` as today |

**Clutter:** draw cap remains ~40 blips. If selection leaves the list after zoom/poll/rebuild, clear selection.

## Data honesty — field map

| UI element | Source field(s) | Notes |
|------------|-----------------|-------|
| Position / star | `lat`, `lon` | Drop if no position |
| Velocity vector | `track` (else `calc_track`), length from `gs` | Omit vector if no track |
| Callsign | `flight` → else `r` → else `hex` | Trim trailing spaces |
| Altitude text | `alt_baro` else `alt_geom` | `F` if ≥ 18000 ft else `A`; hundreds of feet |
| Speed text | `gs` | `G` + knots, rounded |
| Climb/descend | `baro_rate` else `geom_rate` | Arrow only if \|rate\| above small deadband |
| Fixture `dst` | — | **Distance from receiver (nm/mi), not destination** — do not display as airport |

**Explicitly not shown:** cleared altitude, squawk-as-tag-third-line, route/destination.

## Polling

- **Endpoint:** adsb.lol v2 geographic query, e.g.  
  `GET https://api.adsb.lol/v2/lat/{lat}/lon/{lon}/dist/{radius}`  
  with `radius` in **nautical miles** (convert from UI statute miles; clamp to API max ~250 nm).
- **When:** only while Radar is the **selected carousel screen** (the screen currently shown — carousel highlight or focused Radar). Do not poll on Clock / Weather / etc.
- **Cadence:** ~10 seconds.
- **On success:** `parseAdsb` → `ScreenRadar::bind`.
- **On failure:** keep last good bind; if never successfully bound, load `fixtures/adsb_sample.json` once (sim boot / offline).
- **Transport:** shared poller interface; sim uses host HTTP; device later uses ESP HTTP client behind the same interface.

## View-model & formatting

Extend `Aircraft` / parser (optional flags):

- `trackDeg`, `hasTrack`
- `baroRateFpm`, `hasBaroRate`

Pure helpers (unit-tested), e.g.:

- `formatRadarAltitude(altFt)` → `F330` / `A045`
- Tag line builders that omit missing segments
- Trend glyph from rate + deadband

`ScreenRadar`:

- `onTick(elapsed_ms)` advances `sweepAngleDeg` (Classic)
- `bind` rebuilds blips; invalidates selection if index gone
- `RadarView` exposes mode, range, blips (with track/rate), sweep angle, selection + detail/tag fields

Domain stays **LVGL-free**.

## LVGL UI

Shared radar render driven by `RadarView`:

1. Sim (`src/sim/sim_app.cpp` or extracted helper used by sim) — primary validation surface.
2. Replace empty `screen_radar_create/destroy/show/hide` with real LVGL that renders/updates from the same view (and accepts bind/tick from the app shell when wired).

Visual tokens: existing `theme.hpp` greens / dim / accent; phosphor-green blips/tags; white/dim rings.

Sweep: draw a radial line (optional short fade wedge) rotated by `sweepAngleDeg` — not an unstyled `lv_arc` indicator leftover.

## Architecture sketch

```
adsb.lol ──(10s, Radar selected)──► AdsbPoller
                                        │
fixtures/adsb_sample.json ─(fallback)───┤
                                        ▼
                                 parseAdsb()
                                        │
                                        ▼
                                 ScreenRadar.bind / onTick
                                        │
                                        ▼
                                    RadarView
                                        │
                    ┌───────────────────┴───────────────────┐
                    ▼                                       ▼
              sim LVGL                              screen_radar_* LVGL
```

## Testing

**Unit (`pio test -e native`):**

- Parse `track` / `calc_track` / `baro_rate` / `geom_rate`; flags when absent
- `F`/`A` boundary at 17999 / 18000
- Tag string omits missing alt, speed, or trend
- Sweep angle wraps; selection cleared when blip leaves range after bind
- Poller: success rebinds; failure preserves prior list (mock HTTP or inject JSON)

**Manual (sim):**

- Sweep rotates in Classic; hidden in Detail
- Detail: vectors on all; tag + card only when selected
- With network: traffic updates ~10s while Radar selected; stops when leaving Radar
- Without network: fixture / last-good; no crash, no invented fields

## Open implementation notes (not product TBD)

- Exact nm conversion helper and API radius clamp live in poller/radar math.
- Vector length scale constant chosen for ~240px disc readability in implementation.
- Baro-rate deadband (e.g. 64–128 fpm) chosen in implementation and covered by tests.
