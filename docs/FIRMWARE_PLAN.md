> **API base URL:** `https://desk-display-backend.vercel.app`
>
> **Encoder note:** The physical rotary knob has **no push button**. In this plan, every “knob click” means a **short center tap** on the touch screen (not an encoder press).

---

# Desk Display — Firmware Implementation Plan

This repo is the ESP32-S3 firmware only. A separate backend repo
(`desk-display-backend`) provides the data this device consumes over HTTP — see
**API Contract** at the bottom of this doc for the exact shapes to expect. If the
backend repo changes those shapes, this doc's contract section should be updated to
match.

---

## Hardware

**Board:** Waveshare ESP32-S3-Knob-Touch-LCD-1.8 ("Dial")
- Dual MCU: ESP32-S3R8 (8MB PSRAM, 240MHz) + ESP32-U4WDH (4MB flash, classic BT)
- 16MB onboard flash
- 1.8" round IPS LCD, 360×360, 262K color, QSPI interface, ST77916 driver IC
- Capacitive touch, CST816 touch IC
- Physical rotary knob encoder (dual encoder — one per MCU)
- Onboard mic, vibration motor (DRV2605 driver), TF card slot, 3.5mm audio jack
- 2.4GHz Wi-Fi (802.11 b/g/n) only — **no 5GHz support**, onboard antenna
- Bluetooth 5 (LE + Classic)
- USB-C (orientation determines ESP32-S3 USB vs. ESP32 UART mode)
- Wiki: waveshare.com/wiki/ESP32-S3-Knob-Touch-LCD-1.8

**First step before any custom firmware:** flash the stock Waveshare demo firmware to
confirm the unit and touch/knob are working before writing anything custom.

---

## Stack

- **Framework:** PlatformIO + Arduino core for ESP32 (or ESP-IDF directly)
- **UI:** LVGL for all screen rendering, widgets, and animation
- **Simulator:** LVGL's desktop simulator runs in a normal window on a Mac/PC — use
  this to build and iterate on every screen's layout before hardware exists
- **Time:** NTP sync on boot (`pool.ntp.org`) via SNTP. Store IANA timezone names /
  POSIX TZ strings per zone (not fixed UTC offsets) so DST transitions are automatic.
- **Wi-Fi config (v1):** credentials hardcoded in a local `config.h` (git-ignored,
  not committed). Captive-portal first-run setup is a future improvement, not v1.
- **Network — two kinds of calls:**
  1. Direct to a public API: **adsb.lol only** (free, no key) — polled directly by
     the device due to its ~10s refresh need, bypassing the backend entirely
  2. To the project's backend (everything else) — one lightweight JSON GET per
     screen/refresh cycle, per the API Contract below

---

## What Can Be Built Before Hardware Arrives

- **[Simulator-safe]** All screen layout and rendering in the LVGL simulator: clock
  face, timezone board, weather screen, sports screen, radar sweep/detail rendering,
  detail-card layouts, on-screen keyboard widget for airport codes
- **[Simulator-safe]** Data-parsing logic against real sample responses: adsb.lol
  JSON, and whatever shape the backend returns per the contract below
- **[Simulator-safe]** Timezone/DST conversion math, scrub-offset logic
- **[Simulator-safe]** Project scaffolding — PlatformIO project setup, folder
  structure, config file layout
- **[Hardware-required]** Real Wi-Fi connection behavior and NTP sync timing, knob
  rotation/click wiring, touch input calibration, on-device legibility/contrast at
  true size, vibration motor, TF card, power/USB behavior

---

## Global Interaction Model

Two nested knob modes, consistent across every screen:

- **Carousel mode** (top-level browse): rotate = cycle between screens;
  knob click = enter the highlighted screen
- **Focused mode** (inside a screen): rotate = context-specific action (defined per
  screen below); knob click = back out to carousel
- **Touch, consistent everywhere:** tap = select/drill into detail; double-tap or
  long-press = reset to the default/live state for that screen
- **Idle timeout (60s):** from **Carousel**, enter **Focused Clock** (home); from **Focused**, **stay** on the current screen and settle ephemeral UI (scrub, selection, detail overlays) — do not force Clock

---

## Screens

### Phase 1 — Clock + Timezone Board

**Clock (home screen)**
- Analog face: hour/minute/second hands, tick marks around the rim
- Small date readout below center (e.g. "Thu, Jul 23")
- Small indicator hinting at the timezone board; tap or knob-click to enter it

**Timezone board**
- Fixed list of 7 rows, in order: Eastern (home, live anchor by default), Chicago,
  Las Vegas, GMT, Italy (Rome), Ukraine (Kyiv), Moldova (Chișinău)
- Store each as IANA tz name: `America/New_York`, `America/Chicago`,
  `America/Los_Angeles`, `Etc/GMT`, `Europe/Rome`, `Europe/Kyiv`, `Europe/Chisinau`
- Fixed in firmware config for v1, not an editable UI
- **Per-row 3-state status icon**, computed by comparing current time against the
  `sunrise`/`sunset` timestamps the backend provides per city (see API Contract) —
  no solar math on-device:
  - Working hours (9am–5pm local) → neutral/green icon
  - Off-hours but awake (5–9pm, 7–9am local) → caution/amber icon
  - Night (9pm–7am local) → moon icon, row dimmed

**Knob interaction:**
- Rotate = scrub the anchor's time forward/back (1-hour steps); all rows update in
  sync relative to the anchor
- Tap a row = make that row the new anchor
- Double-tap / long-press = reset instantly to live time, anchor back to Eastern
- Knob click = back to carousel

---

### Phase 2 — Weather + Alerts

- Current temp (large, center) + condition icon — map the backend's WMO weather code
  to a small icon set
- "Feels like" temp, smaller, below
- Thin hourly strip along the bottom (temp + icon per upcoming hour)
- High/low for today near the top
- Alert badge, color-coded by severity, rendered only when the backend reports an
  active alert; tap to see headline text
- **Knob:** rotate = scrub forward through the hourly forecast (updates center temp
  live); tap/idle-timeout snaps back to "now"; knob click = back to carousel
- Not in v1: 5-day daily view

---

### Phase 3 — Sports Scores

- Rotate through active items (MLB, Flagstand/SSR Hub — both populated by backend);
  tap for more detail (inning-by-inning box score, or fuller race result) where the
  backend provides it
- World of Outlaws / High Limit / USAC are **not** in this phase — screen should
  simply not render a slot for them until the backend adds that data (see backend
  repo's future-features list)

---

### Phase 4 — Radar / ADS-B (core feature)

- **Data source:** adsb.lol — device polls this **directly**, not through the
  backend
- **Two view modes:**
  - **Classic sweep:** rotating sweep-line animation (cosmetic), aircraft as small
    dots at relative position/distance from center
  - **Detail mode:** aircraft with small labels (callsign, altitude, speed); tap a
    blip expands to a full detail card
- **Knob:** rotate = zoom range (~5mi–50mi); tap empty area = toggle sweep/detail
  mode; tap a blip = detail card; knob click = back to carousel
- **Config (stored on-device, changeable):** center point (default: home lat/long),
  default zoom range on boot
- **Airport code search/center (v1):**
  - On-device UI: LVGL's built-in keyboard widget for typing a 4-letter ICAO code
  - Device sends the code to the backend's lookup endpoint (see API Contract),
    receives lat/long back, re-centers
  - Re-centering is **temporary by default** (reverts on carousel exit or
    power-cycle); long-press or a small "pin" toggle makes it the new permanent
    default center

---

## Future Features (firmware-relevant, not in v1)

- **Radar map overlays (in progress on `feat/radar-map-overlays`)** — see
  [docs/superpowers/specs/2026-07-24-radar-map-overlays-design.md](superpowers/specs/2026-07-24-radar-map-overlays-design.md):
  towered airports (OurAirports TWR join), config POIs, Class B/C/D rings (D dashed),
  debounced `GET /api/map/context`. Hydrography underlay deferred (static tiles only).
- **Precipitation radar map** — device would render a small raw-pixel image (RGB565
  or indexed palette) provided by the backend; reuses image-handling groundwork when
  hydro/precip tiles ship.
- **Vibration alerts** — buzz the onboard motor when a notable aircraft appears
  (altitude threshold or military squawk match).
- **5-day weather view** — tap the hourly strip to expand into a daily forecast.
- **Captive-portal Wi-Fi setup** — replace hardcoded `config.h` credentials with a
  first-run AP + web form flow.
- **Editable timezone/contact list** — currently fixed in firmware config.
- **Settings/utility screen** — Wi-Fi status, location config, brightness; comes
  after the four core data screens, plus a general knob-interaction polish pass.

---

## API Contract (what this device expects from the backend)

The device polls one or more JSON endpoints on the backend on a simple interval per
screen (exact polling interval is a firmware concern, not fixed here — should be
generous, e.g. every few minutes, since the backend itself only refreshes data every
15–30 min except during live game windows).

Expected data per domain (backend repo is the source of truth for exact field names —
confirm against that repo's `API_CONTRACT.md` before finalizing parsing code):

- **Weather:** current temp, feels-like, WMO weather code, today's high/low, hourly
  array (time + temp + code), active alert (severity + headline) if any
- **Timezone/sunrise-sunset:** per city — `sunrise` and `sunset` timestamps (used for
  the 3-state row indicator)
- **Sports:** MLB (live score/inning or next game time), Flagstand/SSR Hub (latest
  result + next scheduled race)
- **Airport lookup:** given an ICAO code, returns lat/long
- **Map context:** `GET /api/map/context?lat=&lon=&radiusMi=` — nearby towered
  airports + Class B/C/D rings (see radar map-overlays design spec)

adsb.lol is the one exception — the device calls it directly using its own public API
shape, not via the backend.
