# Dial display HAL bring-up (solid color)

**Date:** 2026-08-03  
**Status:** Approved for planning  
**Hardware:** Waveshare ESP32-S3-Knob-Touch-LCD-1.8 (“Dial”)  
**Env:** PlatformIO `dial`

## Goal

Get pixels on the Dial LCD: initialize the ST77916 panel, run LVGL 8.4 with a flush path, and show a **full-screen solid color**. Prove the display HAL we will reuse for real screens later. Keep existing Wi‑Fi STA/NVS boot behavior.

## Non-goals

- Touch (CST816) or encoder input
- Nav shell / carousel / focused modes on device
- Porting Clock or other sim screens
- Captive portal, HTTP clients, NTP
- Changing the no-click input model (already specified elsewhere: “knob click” = center tap)

## Context (input model — no change this pass)

The rotary ring has **no push button**. Existing docs (`docs/NAV.md`, `docs/FIRMWARE_PLAN.md`) already map plan “knob click” to **center tap**. This bring-up does not implement input; it does not reopen that decision.

## Decisions

| Topic | Choice |
|--------|--------|
| Success UI | Solid color fill of the active screen (no widgets required) |
| Stack | LVGL 8.4 + display HAL flush (not a raw throwaway fill-only path) |
| Wi‑Fi | Keep `wifiSetup` / `wifiLoop` as today |
| Pin source | Knob 1.8 map from hardware explorer / Waveshare `08_LVGL_Test` — **not** ESP32-S3-Touch-LCD-1.85B pinouts |

## Architecture

Dial boot order in `src/main.cpp`:

1. Serial begin  
2. Existing `desk_net::wifiSetup()`  
3. Display HAL init (panel reset, QSPI, ST77916 init sequence, backlight on)  
4. LVGL init (tick, draw buffer preferably in PSRAM, display driver + flush callback)  
5. Create default screen; set background to one fixed solid color  
6. `loop`: `lv_timer_handler()` + `desk_net::wifiLoop()` (+ short delay / yield as needed)

| Piece | Responsibility |
|--------|----------------|
| `src/hal/board_pins.hpp` (or equivalent) | GPIO constants for this board only |
| `src/hal/display.hpp` / `display.cpp` | Panel init, backlight, flush (dirty rect → GRAM) |
| `src/hal/lvgl_port.hpp` / `lvgl_port.cpp` (name flexible) | LVGL display registration + tick hookup |
| `src/main.cpp` | Wire Wi‑Fi → display → LVGL → solid screen |

Shared `lib/desk_display` stays free of LVGL and Arduino display drivers.

## Pin map (starting point — verify in implementation)

From [esp32-s3-knob-hardware-explorer](https://github.com/IngoDuesentrieb/esp32-s3-knob-hardware-explorer) for this Knob 1.8:

| Function | GPIO |
|----------|------|
| LCD backlight | 47 |
| QSPI CS | 14 |
| QSPI PCLK | 13 |
| QSPI D0–D3 | 15, 16, 17, 18 |
| LCD RST | 21 |

Implementation **must** cross-check against Waveshare wiki / `08_LVGL_Test` for **ESP32-S3-Knob-Touch-LCD-1.8** before locking pins. Do not copy pins from the 1.85B family (different QSPI/backlight mapping).

## LVGL / buffer

- Resolution: 360×360, color format RGB565 (or LVGL 8 equivalent)  
- Partial buffer(s) sized for reliable flush; prefer PSRAM (`BOARD_HAS_PSRAM`)  
- Flush: copy the invalidated area to the panel via the HAL  
- UI: root screen solid background only — one constant color (exact hex left to implementer; document in Serial or README)

## Error handling

| Condition | Behavior |
|-----------|----------|
| Display init fails | Serial error; do not spin-flood; Wi‑Fi continues |
| Init succeeds | Serial line e.g. `display: ready` |
| Wi‑Fi failure | Unchanged from Wi‑Fi pass; independent of display |

## Verification (manual on hardware)

1. Build/flash `env:dial` with USB-C on native S3 (`usbmodem*`)  
2. Round LCD shows the solid color edge-to-edge (bezel / circular visible area filled)  
3. Serial still shows Wi‑Fi credential source + connect/IP when network is available  
4. Power-cycle: color returns without reflash  

## Follow-ups (explicitly later)

- Touch + encoder → nav events (center tap / rotate)  
- Port sim UI starting with Focused Clock  
- Brightness control / settings screen  

## References

- Waveshare wiki: ESP32-S3-Knob-Touch-LCD-1.8 (`08_LVGL_Test`)  
- Hardware explorer pin map (link above)  
- Existing: `docs/NAV.md`, `docs/superpowers/specs/2026-08-03-wifi-nvs-boot-design.md`
