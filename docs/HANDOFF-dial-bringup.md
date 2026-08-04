# Handoff — Dial bring-up (path C gesture remap shipped)

**Date:** 2026-08-04  
**Repo:** `/Users/bruceclingan/Projects/desktop-display-firmware`  
**Branch:** `main` (ahead of `origin` — push when ready)  
**HEAD:** see `git log -1` on `main`  
**Spec/plan (A done):** `docs/superpowers/specs/2026-08-03-dial-radar-port-design.md`, `docs/superpowers/plans/2026-08-03-dial-radar-port.md`  
**Spec/plan (C shipped):** `docs/superpowers/specs/2026-08-04-dial-gesture-remap-design.md`, `docs/superpowers/plans/2026-08-04-dial-gesture-remap.md`  
**Input target:** Dial firmware only — `src/sim/**` deprecated for input work (leave untouched)

## Done this session

1. **Focused / carousel Radar crash fixed** — `LoadProhibited` + `CORRUPTED` backtrace during blocking `map/context` TLS on 24KB loop stack.
   - `ARDUINO_LOOP_STACK_SIZE` **24576 → 49152**
   - Parse map/ADS-B into `lastGood_` (no ~24KB `MapContext` stack temp)
2. **Sweep hitch fixed** — blocking HTTPS on the Arduino loop froze LVGL ~1s per ADS-B poll.
   - New `src/net/http_async.*` FreeRTOS worker; Dial map/ADS-B use async slots
   - Weather/Scores still sync `desk_net::httpGet` (fine off-Radar)
   - Sweep UI refresh before poll kick; period **33ms**
3. Pollers reuse one `bodyBuf_` (no 64KB alloc every async poll tick)
4. On device: `map: bound` / `adsb: bound`, Focused Radar usable, sweep much smoother

## Device status

- **Board:** Waveshare ESP32-S3-Knob-Touch-LCD-1.8 (“Dial”)
- Wi‑Fi STA + NVS; encoder (iot_knob GPIO **8/7**); CST816 **XY + Tap / DoubleTap / LongPress** (`TouchGestureDetector` → `dialShellOnTouch`)
- Clock / Timezones / Weather / Sports: OK
- Radar **path A + C complete:** Classic sweep, live adsb.lol + `/api/map/context`, Focused rotate = zoom; **double-tap** = Nav toggle; tap = select; long-press = settings + NVS + demo
- Boot: `display: ready` → `nav: Focused Clock` (LVGL heap in PSRAM)

## Residual polish

- Brief hitch when **map overlays rebuild** (first bind / zoom) — airspace/highway `lv_line` storm in `radar_lvgl`; separate from TLS stalls
- Carousel Radar still polls map/ADS-B while highlighted (by design); async so it no longer reboots
- `pio device monitor --filter esp32_exception_decoder` may need `pio run -e dial -t monitor` (filter ships with platform); raw `pio device monitor` often lacks it — PTY/`script` wrapper if agent has no TTY

## Input model (path C shipped)

- Ring has **no click**; knob click = **double-tap** → `Nav::on_center_tap()` (Carousel ↔ Focused)
- Carousel single tap ignored; Focused Radar: tap = hit-test select; long-press = settings; settings close via Done or double-tap
- **Not in this pass:** Classic ↔ Detail toggle; Focused Sports/Weather/TZ tap actions
- Sim (`pio run -e sim`) unchanged — use Dial for input verification

## NEXT — polish / verification

1. **On-device checklist** — `docs/superpowers/plans/2026-08-04-dial-gesture-remap.md` Task 7 (gesture matrix + touch aim)
2. **Optional:** Focused full-bleed visual pass; defer map overlay rebuild across frames to soften zoom hitch

## Flash / tooling

```bash
export PATH="$HOME/.platformio/penv311/bin:$PATH"
cd /Users/bruceclingan/Projects/desktop-display-firmware
pio run -e dial -t upload --upload-port /dev/cu.usbmodem*
# Prefer platform monitor (exception decoder):
pio run -e dial -t monitor
# Agent/no-TTY fallback:
script -q /tmp/pio-monitor.log pio device monitor --port /dev/cu.usbmodem* --baud 115200 --filter time
```

Native: `pio test -e native` · Sim: `pio run -e sim`

## Architecture (Dial Radar)

- Pure logic: `lib/desk_display/` (`adsb_poll`, `map_context_poll`, `screen_radar`, …)
- Shared LVGL: `src/ui/radar_lvgl.*`
- Dial shell: `src/hal/dial_shell.*` — async map/ADS-B via `http_async`
- Sync HTTP still: `src/net/http.*` (weather/scores + worker’s blocking GET)
- PSRAM: radar state (`allocLarge`), LVGL custom alloc (`lv_mem_psram`), async slot bodies
- Recent: `12668f0` (demo fixture), `31132a3` (tap select + settings NVS), `cd1255a` (double-tap Nav), `edba95b` (crash + async sweep)

## Backend

- `https://desk-display-backend.vercel.app`
- Sibling: `/Users/bruceclingan/Projects/desk-display-backend`
