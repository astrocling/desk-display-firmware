# Desk Display Firmware

ESP32-S3 firmware for the **Waveshare ESP32-S3-Knob-Touch-LCD-1.8** ("Dial") desk display. The device renders clock, timezone, weather, sports, and ADS-B radar screens and fetches aggregated data from the project backend.

**Backend API base:** `https://desk-display-backend.vercel.app`

## Current status

No Dial hardware yet. **Tracks 0–C are done** on the host (scaffold, API parsers, nav shell, screen view-models). An **SDL + LVGL desktop simulator** is available. Tracks D–E (display/touch/encoder HAL + on-device wire-up) wait for the Dial.

### Unit tests

```bash
pio test -e native   # 65 cases — parsers, domain, nav, all screen models
```

### Desktop simulator

Requires SDL2 (`brew install sdl2` — this machine uses `sdl2-compat`).

```bash
pio run -e sim -t upload
```

Keyboard (encoder has no click — Enter = center tap):

| Key | Action |
|-----|--------|
| `←` / `→` or `[` / `]` or `A` / `D` | Rotate |
| `Enter` / `Space` | Center tap (enter screen / back to carousel) |
| `T` | Tap (screen action) |
| `Y` | Double-tap |
| `U` | Long-press |
| `Esc` / `Q` | Quit |

Boots on **Focused Clock** with fixtures from `fixtures/`. Shared logic lives in `lib/desk_display/`; sim UI is under `src/sim/`.

## Setup

1. Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation.html) (`pio`).
2. Copy the example config and fill in Wi-Fi credentials:

   ```bash
   cp include/config.h.example include/config.h
   ```

   Edit `include/config.h` — set `WIFI_SSID` and `WIFI_PASS`. `config.h` is git-ignored and must not be committed.

## Flashing notes (when hardware arrives)

1. **Flash the stock Waveshare demo first** to confirm the unit, display, touch, and knob work before any custom firmware.
2. **USB-C orientation matters.** The Dial has a USB hub behind the Type-C port; orientation selects either the native ESP32-S3 USB or the secondary ESP32 UART path. If upload fails with something like "This chip is ESP32, not ESP32-S3", flip the cable and retry.
3. Prefer the native S3 USB port (`usbmodem*` on macOS) over the UART serial device when uploading with PlatformIO.

```bash
pio run -e dial -t upload
pio device monitor -e dial
```

## Project layout

| Path | Purpose |
|------|---------|
| `lib/desk_display/` | Shared firmware logic (nav shell, theme tokens, screen stubs) |
| `src/app/` | Application / screen flow |
| `src/net/` | HTTP client, Wi-Fi, API calls |
| `src/models/` | Parsed API / domain models |
| `src/screens/` | LVGL screen implementations |
| `src/hal/` | Hardware abstraction (display, encoder, touch) |
| `test/` | Unit tests (`pio test -e native`) |
| `fixtures/` | Sample JSON for parser tests |
| `docs/` | Firmware and backend plans |

## Docs

- [Firmware plan](docs/FIRMWARE_PLAN.md)
- [Navigation shell](docs/NAV.md) — carousel/focus modes and center-tap mapping
- [Backend plan (local copy)](docs/BACKEND_PLAN.md) — live contract lives in the sibling backend repo
