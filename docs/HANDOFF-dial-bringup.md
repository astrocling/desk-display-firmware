# Handoff — Dial bring-up (BLOCKED: Focused Radar crash)

**Date:** 2026-08-03  
**Repo:** `/Users/bruceclingan/Projects/desktop-display-firmware`  
**Branch:** `main` (ahead of `origin` by several commits — push when ready)  
**Spec/plan:** `docs/superpowers/specs/2026-08-03-dial-radar-port-design.md`, `docs/superpowers/plans/2026-08-03-dial-radar-port.md`

## BLOCKED — Focused Radar immediate reboot

**Symptom:** Carousel → highlight Radar → center-tap to Focus → **immediate crash/reboot** back to boot `nav: Focused Clock`. Carousel Radar preview may be OK; Focused is not.

**Tried (still crashes):**
1. Move `ScreenRadar` / `MapContextPoller` / bind scratch to **PSRAM** (`allocLarge` in `dial_shell`) — fixed LVGL boot OOM (RAM was ~84%).
2. LVGL **custom allocator in PSRAM** (`-DLV_MEM_CUSTOM=1`, `include/lv_mem_psram.h`) — suspected 96KB internal LVGL pool vs 1000+ airspace `lv_line` objs.
3. Throttle Radar UI refresh to 50ms; Serial breadcrumb `shell: build Radar mode=…`.

**Not captured yet:** Guru Meditation / backtrace on the Focus crash (Serial monitor had no lines during one 90s capture — need reproduce with monitor already open).

**Prime suspects for next chat:**
1. **Stack overflow** on Focus: `rebuild_ui` (heavy `radar_lvgl_build` + airspace) and/or **blocking TLS** (`map-context` / adsb) on 24KB `ARDUINO_LOOP_STACK_SIZE` in the same tick.
2. **Airspace/highway LVGL object storm** in `src/ui/radar_lvgl.cpp` (`kMaxAirspaceSegs=1280`) even with PSRAM LVGL heap — assert/`LoadProhibited` mid-build.
3. Confirm `shell: build Radar mode=focused` prints before crash — if not, failure is earlier (Nav/rebuild).

**Bisect ideas:**
- Temporarily stub Focused Radar to `screen_stub_lvgl_build` / rings-only (no airspace/highways/HTTP) — if stable, re-enable pieces.
- Poll map/adsb **only in Focused** (not carousel highlight) to avoid TLS while previewing.
- Raise loop stack further **or** defer HTTP one tick after focus rebuild.
- `pio device monitor` + exception decoder while reproducing.

## Device status (otherwise)

- **Board:** Waveshare ESP32-S3-Knob-Touch-LCD-1.8 (“Dial”)
- Wi‑Fi STA + NVS; encoder (iot_knob GPIO **8/7**); CST816 CenterTap
- Clock / Timezones / Weather / Sports: OK on device (Weather/Sports bind confirmed earlier)
- Radar: wired in `dial_shell` (Classic + pollers) but **Focused unusable** until crash fixed
- Boot OK after PSRAM moves: `display: ready` → `nav: Focused Clock` (RAM ~27% with LVGL-in-PSRAM)

## Input model (locked)

- Ring has **no click**; knob click = **center tap**
- Radar Dial A: rotate = zoom; center tap = Nav; no select/Detail/settings on Dial

## What’s next (after Focused Radar works)

1. Verify Serial `map: bound` / `adsb: bound` on Focused Radar; carousel both ways without reboot
2. Dial Radar input (path to C): CST816 XY + gestures
3. Focused full-bleed visual pass

## Flash / tooling

```bash
export PATH="$HOME/.platformio/penv311/bin:$PATH"
cd /Users/bruceclingan/Projects/desktop-display-firmware
pio run -e dial -t upload --upload-port /dev/cu.usbmodem*
# Then open monitor BEFORE reproducing Focused Radar crash:
pio device monitor --port /dev/cu.usbmodem* --filter esp32_exception_decoder
```

Native: `pio test -e native` · Sim: `pio run -e sim`

## Architecture

- Pure logic: `lib/desk_display/` (`adsb_poll`, `map_context_poll`, `screen_radar`, …)
- Shared LVGL: `src/ui/radar_lvgl.*` (heavy overlays)
- Dial: `src/hal/dial_shell.*`, `src/hal/lv_mem_psram.*`, `src/net/http.*`
- Recent fixes: `d74dcb4` (radar PSRAM state), `8b76ada` (LVGL PSRAM + UI throttle)

## Backend

- `https://desk-display-backend.vercel.app`
- Sibling: `/Users/bruceclingan/Projects/desk-display-backend`
