# Wi-Fi boot connect + NVS credentials (A+)

**Date:** 2026-08-03  
**Status:** Approved for planning  
**Hardware:** Waveshare ESP32-S3-Knob-Touch-LCD-1.8 (“Dial”)  
**Env:** PlatformIO `dial`

## Goal

Get the Dial on a 2.4 GHz Wi-Fi network after flash, with credentials seeded from a gitignored `config.h` and persisted in NVS so a future settings UI can change them without redesigning the connect path. Document how to flash and verify over Serial.

## Non-goals (this pass)

- Captive portal / SoftAP first-run setup
- On-device settings UI
- Display, touch, or encoder HAL
- HTTP / backend health check / NTP
- Automatic Waveshare stock-demo flashing

## Decisions

| Topic | Choice |
|--------|--------|
| Credential source (v1) | Local `include/config.h` (gitignored); example file committed |
| Persistence | ESP32 NVS namespace `wifi`, keys `ssid` / `pass` |
| Precedence | NVS wins after first seed; optional `WIFI_FORCE_CONFIG` re-seeds from `config.h` at boot |
| UX | Serial only |
| Scope | Connect + retry + docs |

## Architecture

On dial boot:

1. Init Serial (115200).
2. Load Wi-Fi credentials from NVS (`wifi` / `ssid`, `pass`).
3. If NVS is empty **or** `WIFI_FORCE_CONFIG` is defined → copy from `WIFI_SSID` / `WIFI_PASS` in `config.h`, write NVS.
4. If SSID is still empty or still the example placeholder (`your-ssid`) → log a clear “set config.h” message and do not call `WiFi.begin`.
5. Otherwise `WiFi.mode(WIFI_STA)` + `WiFi.begin(ssid, pass)`, wait up to ~20s with Serial progress.
6. On success: log SSID (never password), IP, RSSI.
7. On failure: log status; leave STA enabled for later reconnect.

Runtime (`loop`):

- If disconnected, retry with exponential backoff (start ~5s, cap ~30s).
- Log only on state changes (connected ↔ disconnected), not every loop.

### Modules

| Piece | Role |
|--------|------|
| `include/config.h` (gitignored) | Compile-time seed credentials + existing app constants |
| `include/config.h.example` | Placeholders; documents `WIFI_FORCE_CONFIG` usage in comments or README |
| `-DWIFI_FORCE_CONFIG` | Optional build flag: overwrite NVS from `config.h` once at boot |
| `src/net/wifi.hpp` / `wifi.cpp` | NVS load/save, connect, status, retry helpers |
| `src/main.cpp` | Call connect in `setup`; poll/retry in `loop` |
| `README.md` | Flashing and local setup steps |

Secrets must never be committed. `.gitignore` already lists `include/config.h`.

## NVS schema

- Namespace: `wifi`
- `ssid` — string
- `pass` — string
- Empty `ssid` means “no stored credentials”

Radar settings remain in a separate NVS namespace (`radar`); do not mix.

## Error handling

| Condition | Behavior |
|-----------|----------|
| Missing `config.h` | Dial build fails at compile (include required) — same as today once net code includes it |
| Placeholder SSID and empty NVS | Serial warning; skip `begin` |
| Connect timeout | Serial failure; keep STA; retry per backoff |
| Disconnect after success | Backoff reconnect; Serial on transition |
| Wrong USB-C orientation | Upload/monitor fails (wrong chip); documented flip-cable fix — not a firmware concern |

## Flashing & verification docs

README updates (tighten existing “Flashing notes”):

1. Prereqs: PlatformIO Core, USB-C cable, **2.4 GHz** network (Dial has no 5 GHz).
2. Optional but recommended: flash Waveshare stock demo first to validate display/touch/knob.
3. `cp include/config.h.example include/config.h` and set real `WIFI_SSID` / `WIFI_PASS`.
4. USB-C orientation: if upload reports ESP32 instead of ESP32-S3, flip the cable; prefer `usbmodem*` on macOS.
5. `pio run -e dial -t upload` then `pio device monitor -e dial`.
6. Success: Serial shows credential source (NVS vs config seed), then connected + IP.
7. Changing Wi-Fi later: edit `config.h`, flash once with `-DWIFI_FORCE_CONFIG`, then remove the flag for subsequent builds.

## Testing

- **Host:** no hardware Wi-Fi unit tests required this pass (Arduino/WiFi APIs are device-only). Prefer small pure helpers (e.g. “is placeholder SSID”) tested under `native` if extracted without Arduino deps.
- **Device:** manual — flash dial, confirm Serial connect/IP on known 2.4 GHz AP; power-cycle and confirm NVS path (no re-seed log unless `WIFI_FORCE_CONFIG`); force-config flash and confirm re-seed.

## Follow-ups (explicitly later)

- Captive-portal / on-device Wi-Fi settings UI reading/writing the same NVS keys
- Backend health ping and NTP after connect is proven
- HAL + LVGL screens on device
