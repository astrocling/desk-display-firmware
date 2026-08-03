# Dial Radar port (Classic + live ADS-B / map-context)

**Date:** 2026-08-03  
**Status:** Approved for planning  
**Env:** PlatformIO `dial` (+ shared `lib/desk_display` pollers and `src/ui/radar_lvgl`)  
**Goal end-state:** Full sim parity on Dial (select / Detail / settings) after a follow-up Dial-touch pass. **This pass is A only.**

## Goal

Replace the Dial Radar stub with the shared `radar_lvgl` + `ScreenRadar` path already used by sim: Classic sweep, live [adsb.lol](https://api.adsb.lol/) traffic, and backend `/api/map-context` overlays while Radar is the active screen. Focused rotate zooms range. Keep selection, Classic ↔ Detail, and settings sim-only until CST816 XY / gestures exist.

## Non-goals (this pass)

- Blip / static-mark hit-testing on Dial
- Classic ↔ Detail mode toggle on Dial
- Radar settings overlay, demo mode UI, NVS prefs load/save on Dial
- Pin / temp recenter from Dial gestures
- Background HTTP thread (sim prefetch thread)
- Focused full-bleed sizing polish across screens
- Boot fixtures on Dial (sim-only offline path)

## Product decisions

| Topic | Choice |
|-------|--------|
| Dial v1 interaction | Rotate = zoom; center tap = Nav only |
| Mode | ClassicSweep only on Dial (sim may still use Detail) |
| Data | Live adsb.lol ~10s + `/api/map-context` while Radar active |
| Architecture | Mirror Weather/Sports in `dial_shell`; real `ScreenRadar` / `radar_lvgl` (no throwaway stub) |
| Poller buffers | Heap per attempt; **64KB** cap each; no 256KB BSS |
| Concurrent GETs | At most one blocking Radar-related GET per shell tick (serialize map vs ADS-B) |
| Center | `kRadarHomeLat` / `kRadarHomeLon` (Dayton); factory `RadarSettings` |
| Failure | Keep last-good; empty traffic/map until first success is OK |
| Follow-up (C) | Dial touch XY + double-tap / long-press → select / Detail / settings |

## Architecture

### Shell (`src/hal/dial_shell.*`)

- Own `desk_display::ScreenRadar`, `AdsbPoller`, `MapContextPoller`.
- `setHttpGet(&desk_net::httpGet, nullptr)` for both pollers (same transport as Weather/Scores).
- `refresh_content` Radar branch: prefer `radar_lvgl_animate_classic`; on miss, clean + `radar_lvgl_build`. Call `radar_lvgl_invalidate` when tearing down the body.
- Focused rotate → `ScreenRadar::onRotate` + refresh when focused on Radar.
- Idle settle → include `radar.onIdleSettle()`.
- Tick order matches Weather: Nav / rebuild UI **before** blocking HTTP.

### Poll loop (while Radar is active screen)

1. `radar.onTick(elapsed_ms)`
2. `adsb.setCenter` / `map.setCenter` from radar center + range
3. `adsb.setActive(radar_active)` / `map.setActive(radar_active)`
4. Pollers `onTick` — **at most one** blocking GET this shell tick (if both due, prefer map-context, defer ADS-B to a later tick)
5. `takeAircraft` → `radar.bind`; `takeContext` → `radar.bindMapContext`
6. Refresh UI on successful bind **or** every tick while Radar is visible (sweep)

Leaving Radar stops polling; last bind remains until the next visit.

### Shared domain changes

- `AdsbPoller` and `MapContextPoller`: remove `char body_[256*1024]`; allocate `std::unique_ptr<char[]>` of `kBodyCap = 64 * 1024` per attempt (Scores pattern). Oversized / alloc fail → soft fail, keep last-good.
- Native tests updated if they assume BSS capacity or layout; behavior otherwise unchanged.
- Optional: if `RADAR_POI_*` build flags are defined for Dial, call `setPois` like sim. No requirement to add new POI config this pass.

### C-shaped hooks (do not invent Dial-only Radar APIs)

- Dial never calls `radar_lvgl_hit_*`, `radar_lvgl_settings_hit`, mode toggle, or `openSettings` until the Dial-touch pass.
- Center tap remains Nav-owned (`dialShellOnCenterTap` → `Nav::on_center_tap` only).

## Error handling

| Failure | Behavior |
|---------|----------|
| Wi‑Fi down / HTTP non-2xx / timeout | Keep last-good; retry next interval |
| Body exceeds 64KB or alloc fails | Soft fail; Serial warn; keep last-good |
| Parse fail | Soft fail; keep last-good |
| Never successfully bound | Rings + sweep still draw; no aircraft / sparse map |

## Success criteria

1. `pio test -e native` passes (including poller / radar tests).
2. `pio run -e dial` and `pio run -e sim` build.
3. On device: Focus Radar → Serial shows successful GETs and `adsb: bound` / `map: bound` (or equivalent); Classic sweep moves; rotate updates range in the header.
4. Carousel both directions without reboot; leave and re-enter Radar without crash.
5. Selection / settings / Detail remain unavailable on Dial (no regressions requiring new gestures).

## Follow-up (explicitly next)

**Dial Radar input (path to C):** extend CST816 HAL beyond finger-down CenterTap to XY + double-tap / long-press; wire Focused Radar hit-test, mode toggle, and settings overlay (center-tap-while-settings semantics as sim). Load/save `radar` NVS prefs when settings land.
