# Handoff — Dial bring-up (Wi‑Fi + display + input/Nav)

**Date:** 2026-08-03  
**Repo:** `/Users/bruceclingan/Projects/desktop-display-firmware`  
**Branch:** `main` (local) — `feat/dial-input-nav` merged in; do **not** push until user asks. Encoder fix landed as `1d8a368`.

## Device

- **Board:** Waveshare ESP32-S3-Knob-Touch-LCD-1.8 (“Dial”)
- **Working on device:** Wi‑Fi STA + NVS; solid dark green LVGL background; tap toggles Focused ↔ Carousel; rotate in Carousel cycles screen names on the LVGL overlay; Serial status matches overlay.
- **User confirmed:** green screen; input/nav path above works end-to-end.

## What’s done

1. **Wi‑Fi STA + NVS** — `config.h` (gitignored) seeds NVS namespace `wifi`; NVS wins after first boot; optional `-DWIFI_FORCE_CONFIG`. Spec/plan: `docs/superpowers/specs/2026-08-03-wifi-nvs-boot-design.md`, `docs/superpowers/plans/2026-08-03-wifi-nvs-boot.md`. Code: `src/net/wifi.*`, `lib/desk_display` `wifi_policy.*`.
2. **Display HAL + LVGL** — Solid dark green (`0x1B5E20`); Serial `display: ready`. Spec/plan: `docs/superpowers/specs/2026-08-03-dial-display-hal-design.md`, `docs/superpowers/plans/2026-08-03-dial-display-hal.md`. Code: `src/hal/board_pins.hpp`, `display.*`, `lvgl_port.*`; `main` wires Wi‑Fi then LVGL.
3. **Encoder** — Waveshare **iot_knob** / `bidi_switch_knob` (GPIO **8/7**). Hardware is **bidirectional pulse**, **not** classic quadrature. Code: `src/hal/encoder.*`, `src/hal/bidi_switch_knob.*`; decode helpers in `lib/desk_display` `encoder_decode.*`.
4. **Touch** — CST816 → **CenterTap** (any short press). Code: `src/hal/touch.*`, `lib/desk_display` `center_tap.*`.
5. **On-device Nav** — Focused ↔ Carousel wired; Serial + LVGL overlay status (`nav_overlay`, `nav_status`). Boot: `nav: Focused Clock`. Pure nav in `lib/desk_display/nav.*`.

## Input model (locked — do not rethink)

- Rotary ring has **no click / no push button**.
- “Knob click” = **short center tap** on the touchscreen.
- See `docs/NAV.md` and encoder note at top of `docs/FIRMWARE_PLAN.md`.
- Sim already maps Enter → center tap.

## What’s next (suggested order)

1. **On-device carousel chrome** — title / dots / preview (nav logic is live; UI is still overlay text only).
2. **Port screens** starting with **Focused Clock**, then weather/sports/radar from sim (`src/sim/`, `src/ui/`).

## Flash / tooling (macOS)

- USB-C **orientation matters**: need `/dev/cu.usbmodem*` (ESP32-S3). If esptool says “ESP32, not ESP32-S3”, flip the cable (`usbserial*` is the companion chip).
- PlatformIO for **dial**: use Python 3.10+ — on this machine `~/.platformio/penv311/bin/pio` (default `penv` is 3.9 and breaks espressif32 55.x).
- Secrets: `cp include/config.h.example include/config.h` — **never commit** `include/config.h` (SSID already set locally).
- Build/upload:
  ```bash
  export PATH="$HOME/.platformio/penv311/bin:$PATH"
  cd /Users/bruceclingan/Projects/desktop-display-firmware
  pio run -e dial -t upload --upload-port /dev/cu.usbmodem*
  ```
- Verify Serial: `wifi: …`, `display: ready`, `encoder: ready`, `touch: ready` (or not found), boot `nav: Focused Clock`; screen dark green; tap/rotate as above.
- Native smoke: `pio test -e native -f test_input -f test_nav` (full: `pio test -e native`).

## Pin map (Knob 1.8 — locked)

| Function | GPIO |
|----------|------|
| LCD QSPI CS / PCLK / D0–D3 / RST | 14 / 13 / 15–18 / 21 |
| Backlight | 47 |
| Encoder A / B | 8 / 7 |
| Touch I²C SDA / SCL | 11 / 12 (CST816 @ 0x15) |

## Architecture notes

- Shared logic: `lib/desk_display/` (pure — no LVGL).
- Dial-only HAL: `src/hal/`, `src/net/`, `src/main.cpp`.
- Encoder driver: Waveshare iot_knob / `bidi_switch_knob` (not a generic quadrature ISR).
- Sim: `src/sim/` + `src/ui/` (SDL + LVGL) — reference for on-device UI.
- Dial build flag: `-DLV_COLOR_16_SWAP=1` (panel needs byte swap; sim stays 0).
- Flush waits are **bounded** (100 ms) so a stuck DMA can’t hang Wi‑Fi (`76bb8ca`).

## Backend

- API base: `https://desk-display-backend.vercel.app`
- Sibling repo: `/Users/bruceclingan/Projects/desk-display-backend`

## Open follow-ups (non-blocking)

- Interactive idle **60s** verify on device (if not already confirmed).
- `kEncoderInvert` if rotation direction feels wrong.
- Illegal-jump decoder deferrals from review remain **optional**.
- Visually confirm green vs wrong-endian (user said green looks good).
- Init command table from community ST77916 mirror; optional cross-check vs Waveshare `08_LVGL_Test`.
- Push to `origin` when user is ready (“push it all in the end”).
