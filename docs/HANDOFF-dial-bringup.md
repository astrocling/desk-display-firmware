# Handoff — Dial bring-up (next: Weather / Sports / Radar)

**Date:** 2026-08-03  
**Repo:** `/Users/bruceclingan/Projects/desktop-display-firmware`  
**Branch:** `main` — shell + Clock/Timezones landed; push when this handoff was written.

## Device status (confirmed)

- **Board:** Waveshare ESP32-S3-Knob-Touch-LCD-1.8 (“Dial”)
- Wi‑Fi STA + NVS; encoder (iot_knob GPIO **8/7**, not quadrature); CST816 CenterTap
- On-device Nav Focused ↔ Carousel; Serial `nav: …`
- Carousel chrome (title/dots/preview); **Clock** + **Timezones** real; Weather/Sports/Radar **stubs**
- SNTP (`pool.ntp.org`) after Wi‑Fi → `ntp: synced`
- Theme `kBg` (green bring-up gone)

## Input model (locked)

- Ring has **no click**; knob click = **center tap**
- See `docs/NAV.md`

## Deferred (do later, once all screens visible)

- **Focused full-bleed sizing** — Clock face is fixed 200px (carousel-preview-era); empty ring on 360 Focused. Polish sizing across Clock/Weather/Sports/Radar together after ports, not Clock-only now.

## What’s next

1. **Port Weather** from sim (`src/sim/sim_app.cpp` weather case + `lib/desk_display` weather VM; extract `src/ui/weather_lvgl.*` like clock/tz; Dial HTTP to backend).
2. **Port Sports** (same pattern; scores poll).
3. **Port Radar** (`src/ui/radar_lvgl.*` already shared; wire Dial shell + adsb.lol / map context; settings later as needed).
4. Then **visual pass**: parent-relative layout / fill the ring in Focused.

Spec for last pass: `docs/superpowers/specs/2026-08-03-dial-shell-clock-tz-design.md`.

## Flash / tooling

```bash
export PATH="$HOME/.platformio/penv311/bin:$PATH"
cd /Users/bruceclingan/Projects/desktop-display-firmware
pio run -e dial -t upload --upload-port /dev/cu.usbmodem*
# Serial: wifi → display: ready → encoder/touch → ntp: syncing/synced → nav: Focused Clock
```

Native: `pio test -e native` · Sim: `pio run -e sim`

## Architecture

- Pure logic: `lib/desk_display/`
- Shared LVGL: `src/ui/` (`carousel_lvgl`, `clock_lvgl`, `timezones_lvgl`, `screen_stub_lvgl`, `radar_lvgl`)
- Dial: `src/hal/dial_shell.*`, `src/net/{wifi,ntp}.*`, `src/main.cpp`
- Sim remains reference for Weather/Sports/Radar UI still inline in `sim_app.cpp`

## Backend

- `https://desk-display-backend.vercel.app`
- Sibling: `/Users/bruceclingan/Projects/desk-display-backend`
