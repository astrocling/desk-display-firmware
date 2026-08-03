# Desk Display Firmware

ESP32-S3 firmware for the **Waveshare ESP32-S3-Knob-Touch-LCD-1.8** ("Dial") desk display. The device renders clock, timezone, weather, sports, and ADS-B radar screens and fetches aggregated data from the project backend.

**Backend API base:** `https://desk-display-backend.vercel.app`

## Current status

**Tracks 0–C and the desktop simulator are done** (scaffold, API parsers, nav shell, screen view-models, SDL + LVGL sim). The Dial now has **Wi-Fi STA connect + NVS credentials** and **display HAL solid-color bring-up** — touch, encoder, and on-device nav/UI wire-up are still to come.

### Unit tests

```bash
pio test -e native   # 85 cases — parsers, domain, nav, all screen models
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
2. Copy the example config and set Wi-Fi credentials (2.4 GHz only on the Dial):

   ```bash
   cp include/config.h.example include/config.h
   ```

   Edit `include/config.h` — set `WIFI_SSID` and `WIFI_PASS`.
3. `config.h` is gitignored — do not commit it.

## Flashing notes

1. **Flash the stock Waveshare demo first** to validate the unit, display, touch, and knob before custom firmware.
2. **USB-C orientation matters.** Flip the cable if upload reports "This chip is ESP32, not ESP32-S3". Prefer the native S3 USB port (`usbmodem*` on macOS).
3. Build and upload, then open Serial:

   ```bash
   pio run -e dial -t upload
   pio device monitor -e dial
   ```

4. **Success:** Serial shows `wifi: credentials from NVS` or `wifi: seeded from config.h`, then `wifi: connected ssid=… ip=…`.
5. **Display verify:** after upload, the panel should show a solid dark green (`0x1B5E20`) and Serial should print `display: ready`.
6. **Changing network later:** edit `config.h`, uncomment `-DWIFI_FORCE_CONFIG` under `[env:dial]` in `platformio.ini` for one upload, then comment it out again.

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
