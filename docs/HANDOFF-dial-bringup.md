# Handoff — Dial bring-up (Radar A done → path C input)

**Date:** 2026-08-03  
**Repo:** `/Users/bruceclingan/Projects/desktop-display-firmware`  
**Branch:** `main` (ahead of `origin` by 6 — push when ready)  
**HEAD:** `edba95b` — fix: stop Dial Radar crash and unblock Classic sweep  
**Spec/plan (A done):** `docs/superpowers/specs/2026-08-03-dial-radar-port-design.md`, `docs/superpowers/plans/2026-08-03-dial-radar-port.md`  
**Next track:** Dial Radar input (**path C**) — CST816 XY + gestures → select / Detail / settings

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
- Wi‑Fi STA + NVS; encoder (iot_knob GPIO **8/7**); CST816 **CenterTap only** today
- Clock / Timezones / Weather / Sports: OK
- Radar **path A complete:** Classic sweep, live adsb.lol + `/api/map/context`, Focused rotate = zoom, center tap = Nav only
- Boot: `display: ready` → `nav: Focused Clock` (LVGL heap in PSRAM)

## Known residual (not blocking path C)

- Brief hitch when **map overlays rebuild** (first bind / zoom) — airspace/highway `lv_line` storm in `radar_lvgl`; separate from TLS stalls
- Carousel Radar still polls map/ADS-B while highlighted (by design); async so it no longer reboots
- `pio device monitor --filter esp32_exception_decoder` may need `pio run -e dial -t monitor` (filter ships with platform); raw `pio device monitor` often lacks it — PTY/`script` wrapper if agent has no TTY

## Input model (locked until path C)

- Ring has **no click**; knob click = **center tap**
- Radar Dial A: rotate = zoom; center tap = Nav; **no** select / Detail / settings on Dial yet

## NEXT — Dial Radar input (path C)

**Goal:** Parity with sim Focused Radar gestures on Dial.

1. Extend CST816 HAL beyond finger-down CenterTap → **XY**, double-tap, long-press (`src/hal/touch.*`)
2. Wire Focused Radar only:
   - Tap → `radar_lvgl_hit_blip` / `radar_lvgl_hit_static` → select
   - Mode toggle Classic ↔ Detail (sim semantics)
   - Settings overlay + `radar_lvgl_settings_hit`; center-tap-while-settings as sim
3. Load/save `radar` **NVS** prefs when settings land
4. Keep center-tap **Nav-owned** when settings closed (`dialShellOnCenterTap` → `Nav::on_center_tap`)

**Do not** invent Dial-only Radar APIs — reuse existing `radar_lvgl_hit_*` / settings hooks.

**Optional polish (if path C not started):** Focused full-bleed visual pass across screens; defer map overlay rebuild across frames to soften zoom hitch.

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
- Recent: `edba95b` (crash + async sweep), `8b76ada` (LVGL PSRAM), `d74dcb4` (radar PSRAM)

## Backend

- `https://desk-display-backend.vercel.app`
- Sibling: `/Users/bruceclingan/Projects/desk-display-backend`
