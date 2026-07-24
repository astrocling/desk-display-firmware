# Desktop simulator

LVGL 8.4 + SDL2 window (360×360 logical, 2× zoom) for iterating UI before Dial hardware arrives.

## Prerequisites

```bash
brew install sdl2   # or sdl2-compat on newer Homebrew
```

## Run

```bash
pio run -e sim -t upload
```

(`upload` is overridden to execute the host binary — see `support/sdl2_build_extra.py`.)

## Controls

Click the sim window so it has keyboard focus (macOS often leaves focus on the terminal when launched via `pio`).

Boots in **Focused Clock** (full-bleed, no shell chrome). Enter/Space backs out to **Carousel**: title + page dots + a live inset preview of the highlighted screen (preview taps are ignored). Left/Right cycle the highlight and preview. Enter/Space again focuses the highlighted screen full-bleed so rotate/tap act in-app. On the clock face itself, rotate/tap do nothing visible.

**Idle:** Carousel idle → Focused Clock. Focused idle → stay on the current app and settle (e.g. clear radar selection, snap weather scrub) without jumping back to Clock.

The old persistent `Carousel · Screen` / `Focused · Screen` debug label is removed; Carousel frame chrome replaces it in browse mode.

| Key | Maps to |
|-----|---------|
| Left / Right (also `[` `]` `A` `D`) | Encoder rotate |
| Enter / Space | Center tap (“knob click”) |
| Mouse click / T | Tap |
| Y | Double-tap |
| U | Long-press |
| Esc / Q | Quit |

Mouse acts as capacitive touch for LVGL pointer input.

## Data

Boots offline: loads JSON from `fixtures/` (captured from the live backend + adsb.lol) so every screen has data before any network call happens.

Once booted, Radar is the exception — while it's the active screen (carousel-highlighted or focused), the sim polls the live [adsb.lol](https://api.adsb.lol/) API every ~10s via libcurl (`src/sim/sim_http.*`), centered on the radar's current lat/lon and range. Fetches start ~2.5s early on a **background thread** so the Classic sweep never stalls when the beam wraps through north. Leaving Radar (switching to another screen) stops polling; the last-fetched aircraft remain bound until you return. A failed/slow request (timeout ~8s) just keeps the previous data — no fixture fallback mid-session, since the fixture already primed `radar_` at boot.

Classic Sweep runs continuously (including while a target is selected): **10 s/rev**, a green phosphor trail, and blips that only move when the sweep crosses them. **Click a visible blip** (Focused mode) for callsign / alt / speed (tag + card); empty click clears selection. **Zoom out** (≥20 mi) uses dense dots; **zoom in** (≤15 mi) uses velocity vectors.
