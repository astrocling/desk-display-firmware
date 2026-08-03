# Dial shell + Clock/Timezones port

**Date:** 2026-08-03  
**Status:** Approved (port sim; do not redesign)  
**Env:** PlatformIO `dial` (+ shared `src/ui` used by `sim`)

## Goal

On Dial: replace the debug nav overlay with the **sim carousel chrome** (title/dots/preview), mount **real Clock and Timezones** (same LVGL as sim), drive time via **SNTP** after Wi‑Fi, and stub Weather/Sports/Radar. Input model unchanged (ring no click; center tap = knob click).

## Non-goals

- Weather / Sports / Radar real UI or backend/adsb polling
- Touch gestures beyond CenterTap (row tap, double, long-press)
- `/api/timezones` sun map / TZDB
- Captive portal, redesign of chrome or clock face

## Architecture

- Extract `clock_lvgl` + `timezones_lvgl` (+ small stub helper) from `sim_app` into `src/ui/`; sim and dial share builders.
- Dial shell mirrors sim hosts: `carousel_root_` / `focused_host_` / `body_`; reuse `carousel_lvgl`.
- Root/screen bg → `theme::kBg` (drop solid green bring-up fill).
- Serial `nav: …` on change; remove `nav_overlay`.
- `src/net/ntp.*`: SNTP `pool.ntp.org` when Wi‑Fi connected; feed `ScreenClock::setUnixUtc` + `ScreenTimezones::setLiveUnix`. Until sync, leave default/epoch view.
- Focused Timezones: rotate → `onRotate`; idle settle → `onIdleSettle`. No sun fixture on Dial this pass.

## Success

1. Focused Clock shows live (post-NTP) time matching sim layout.
2. Carousel shows title/dots + Clock or Timezones preview; other screens labeled stubs.
3. Center-tap / rotate / idle behave as `docs/NAV.md`.
4. `pio run -e dial` and `pio run -e sim` build; native nav/clock/tz tests still pass.
