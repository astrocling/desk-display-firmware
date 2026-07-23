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

Boots in **Focused Clock**. Enter/Space exits to carousel; then Left/Right cycle screens while showing each screen’s normal view (same as Focused). Enter/Space again focuses the highlighted screen so rotate/tap act in-app. On the clock face itself, rotate/tap do nothing visible.

| Key | Maps to |
|-----|---------|
| Left / Right (also `[` `]` `A` `D`) | Encoder rotate |
| Enter / Space | Center tap (“knob click”) |
| T | Tap |
| Y | Double-tap |
| U | Long-press |
| Esc / Q | Quit |

Mouse acts as capacitive touch for LVGL pointer input.

## Data

Loads JSON from `fixtures/` (captured from the live backend + adsb.lol). No Wi-Fi required.
