# Handoff — Dial bring-up (next: Radar input → visual pass)

**Date:** 2026-08-03  
**Repo:** `/Users/bruceclingan/Projects/desktop-display-firmware`  
**Branch:** `main` — Weather + Sports + **Radar** ported (shared `weather_lvgl` / `sports_lvgl` / `radar_lvgl` + Dial HTTP). All three are committed on `main`; push to `origin` when ready.

## Device status (confirmed)

- **Board:** Waveshare ESP32-S3-Knob-Touch-LCD-1.8 (“Dial”)
- Wi‑Fi STA + NVS; encoder (iot_knob GPIO **8/7**, not quadrature); CST816 CenterTap
- On-device Nav Focused ↔ Carousel; Serial `nav: …`
- Carousel chrome (title/dots/preview); **Clock** + **Timezones** + **Weather** + **Sports** + **Radar** real (shared `radar_lvgl` + `ScreenRadar`; Classic sweep + live poll)
- SNTP (`pool.ntp.org`) after Wi‑Fi → `ntp: synced`
- Weather: `GET /api/weather` while Weather is active screen → Serial `http: GET … ok` / `weather: bound`
- Sports: `GET /api/scores` while Sports is active screen → Serial `http: GET … ok` / `scores: bound`
- Radar: while Radar is active screen → backend `/api/map-context` + adsb.lol via `dialRadarHttpGet` (≤1 GET/tick; map preferred) → Serial `map: bound` / `adsb: bound`. **Flashed; Serial bind verify recommended** (bind lines not fully monitored post-flash).
- **Carousel reboot fix:** blocking HTTPS was overflowing the default 8KB Arduino loop stack when landing on Weather. Dial now uses `-DARDUINO_LOOP_STACK_SIZE=24576`, heap-allocated `WiFiClientSecure`, and rebuilds UI before poll (`src/net/http.cpp`, `platformio.ini`, `dial_shell.cpp`). **Retested:** carousel both directions without reboot; Weather bind OK.
- **Scores TLS OOM:** a 64KB BSS body in `ScoresPoller` left too little heap for mbedTLS (`SSL - Memory allocation failed`). Body is now 8KB, heap-allocated per poll (`scores_poll.*`).
- **Radar pollers:** ADS-B + map-context bodies are **64KB** heap-allocated per poll (`adsb_poll.*`, `map_context_poll.*`). Scores uses an **8KB** body (TLS OOM fix above).
- **Radar DRAM:** `ScreenRadar` / map-context poller / bind scratch live in PSRAM (`allocLarge` in `dial_shell`), not internal BSS — otherwise LVGL’s ~29KB DMA draw buffer fails and the Dial reboot-loops (~84% RAM before the move).
- Theme `kBg` (green bring-up gone)

## Input model (locked)

- Ring has **no click**; knob click = **center tap**
- See `docs/NAV.md`
- Weather: Focused rotate scrubs hourly; idle settle snaps to now. Alert badge shown; alert detail tap is sim-only (no ring tap on Dial).
- Sports: Focused rotate cycles MLB ↔ Flagstand; detail tap is sim-only (center tap is Nav).
- **Radar (Dial A):** Focused rotate = zoom range; center tap = Nav only. **ClassicSweep** only — no blip select, Detail mode toggle, or settings overlay on Dial (sim-only until CST816 XY / gestures land).

## Deferred (do later, once all screens visible)

- **Focused full-bleed sizing** — Clock face is fixed 200px (carousel-preview-era); empty ring on 360 Focused. Polish sizing across Clock/Weather/Sports/Radar together after ports, not Clock-only now.

## What’s next

1. **Dial Radar input (path to C):** extend CST816 HAL beyond CenterTap to XY + double-tap / long-press; wire Focused hit-test, Classic ↔ Detail toggle, and settings overlay (see `docs/superpowers/specs/2026-08-03-dial-radar-port-design.md`).
2. Then **visual pass**: parent-relative layout / fill the ring in Focused.

Spec for shell/clock pass: `docs/superpowers/specs/2026-08-03-dial-shell-clock-tz-design.md`.

## Flash / tooling

```bash
export PATH="$HOME/.platformio/penv311/bin:$PATH"
cd /Users/bruceclingan/Projects/desktop-display-firmware
pio run -e dial -t upload --upload-port /dev/cu.usbmodem*
# Serial: wifi → display: ready → encoder/touch → ntp: syncing/synced → nav: Focused Clock
# Focus Weather: http: GET …/api/weather ok → weather: bound
# Focus Sports: http: GET …/api/scores ok → scores: bound
# Focus Radar: http: GET … ok → map: bound → adsb: bound (verify after flash)
```

Native: `pio test -e native` · Sim: `pio run -e sim`

## Architecture

- Pure logic: `lib/desk_display/` (`weather_poll`, `scores_poll`, `adsb_poll`, `map_context_poll`, screen VMs)
- Shared LVGL: `src/ui/` (`carousel_lvgl`, `clock_lvgl`, `timezones_lvgl`, `weather_lvgl`, `weather_img` + assets, `sports_lvgl`, `mlb_img` + assets, `radar_lvgl`)
- Dial: `src/hal/dial_shell.*`, `src/net/{wifi,ntp,http}.*`, `src/main.cpp`
- Sim uses shared `sports_lvgl` / `weather_lvgl` / `radar_lvgl` (no inline Sports UI)

## Backend

- `https://desk-display-backend.vercel.app`
- Sibling: `/Users/bruceclingan/Projects/desk-display-backend`
