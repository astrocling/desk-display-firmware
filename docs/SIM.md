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

Boots offline: loads JSON from `fixtures/` (captured from the live backend) so Weather, Timezones, and Sports have data before any network call happens. Radar loads `fixtures/map_context_dayton.json` for airport marks and Class D sample rings, but does **not** bind `fixtures/adsb_sample.json` at boot — sample aircraft confused the first Classic sweep before live traffic arrived. That fixture remains for unit tests and a future settings demo mode.

Once booted, screens poll the network while they are the active screen (carousel-highlighted or focused):

**Radar** — polls live [adsb.lol](https://api.adsb.lol/) every ~10s via libcurl (`src/sim/sim_http.*`), centered on the radar's current lat/lon and range. Fetches start ~2.5s early on a **background thread** so the Classic sweep never stalls when the beam wraps through north. Leaving Radar stops polling; the last-fetched aircraft remain bound until you return. Until the first successful poll, the radar shows map overlays only (no aircraft). A failed/slow request (timeout ~8s, or HTTP 429) keeps the previous live data when available. There is no mid-session fixture reload.

Map overlays: a debounced `MapContextPoller` GETs `{API_BASE_URL}/api/map/context` after center/range settles (~400 ms), on its **own** async HTTP slot (separate from ADS-B so the two pollers cannot clobber each other). On failure it keeps last-good overlays (fixture at boot). Optional `RADAR_POIS` in `config.h` add curated POI marks.

**Sports** — polls `{API_BASE_URL}/api/scores` every ~30s on a third async HTTP slot. The first fetch runs as soon as Sports becomes active (fixture until then). While a game is live, the backend may refresh ESPN on read (~45s TTL) and return logos-ready fields (`teamRuns`/`opponentRuns`, count, bases, batter/pitcher). The sim LIVE card shows logos + runs + situation; live AB/P lines include season AVG/ERA and game summaries when the API provides them. Failures keep last-good.

Classic Sweep runs continuously (including while a target is selected): **10 s/rev**, a green phosphor trail, and blips that only move when the sweep crosses them. **Click a visible blip** (Focused mode) for callsign / alt / speed (tag + card); empty click clears selection. Static airport/POI taps show a short label (aircraft hit wins overlaps). **Zoom out** (≥20 mi) uses dense dots; **zoom in** (≤15 mi) uses velocity vectors.
