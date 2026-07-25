# Sim live scores poll (+ Radar HTTPS note)

**Date:** 2026-07-24  
**Repo:** `desktop-display-firmware`  
**Status:** draft for review

## Problem

1. **MLB / Sports:** Sim binds `fixtures/scores.json` once at boot and never refreshes. A live game on the backend still shows the next-game card.
2. **Radar (related note):** Live HTTPS *is* wired (`simAdsbHttpGet` → adsb.lol). Terminal logs show timeouts and **HTTP 429**. On failure the poller keeps last-good — which is the boot fixture — so Radar *looks* stuck on sample data. Fixing rate-limit/backoff is out of scope for this change unless it blocks verification; call out in SIM.md.

## Goal

While developing live MLB UI, the sim Sports screen should show the same `/api/scores` blob the dial will eventually poll, updating through a live game without restarting.

## Non-goals

- Dial / ESP32 HTTP client for scores
- Backend cron cadence changes (still ~15 min; sim may poll faster and see stale Redis until cron runs)
- Redesigning the live MLB card layout (separate follow-up once we can see live data)
- Fixing adsb.lol 429 / timeout behavior (track separately)

## Design

### ScoresPoller (shared lib, sim-wired)

Mirror `AdsbPoller` / map-context pattern:

| Piece | Behavior |
|-------|----------|
| URL | `{API_BASE_URL}/api/scores` (`API_BASE_URL` from `config.h`, else production default like map-context) |
| Transport | New dedicated async slot `simScoresHttpGet` (must not share ADS-B or map-context slots) |
| Active when | Sports is the active screen (carousel-highlighted or focused) — same rule as Radar |
| Interval | ~30 s between successful binds |
| Parse | Existing `parseScores` |
| Success | `sports_.bind(scores)` + UI refresh |
| Failure / in-flight | Keep last-good (fixture or previous live); no mid-session fixture reload |

Boot path unchanged: fixture primes Sports; first successful poll overwrites.

### Sim wiring

- `SimApp`: own a `ScoresPoller`, `setHttpGet(&simScoresHttpGet)`, `setActive(sports_active)`, `onTick`, `takeScores` in `update()`
- When `takeScores` returns true and Sports is visible → `refresh_content()` (or rebuild body) so live → score card flips without leaving the screen
- Document in `docs/SIM.md` next to the Radar live-network paragraph

### Radar note (docs only in this change)

Document that failed/429 ADS-B GETs retain boot fixture traffic; successful polls replace it. Optional follow-up: longer interval / backoff on 429, or a small on-screen “live / stale” hint.

## Files (expected)

- `lib/desk_display/include/desk_display/scores_poll.hpp` (new)
- `lib/desk_display/src/scores_poll.cpp` (new)
- `src/sim/sim_http.{hpp,cpp}` — `simScoresHttpGet` + async slot
- `src/sim/sim_app.{hpp,cpp}` — wire poller
- `docs/SIM.md` — scores poll + Radar 429/stale note
- Tests: unit-test URL builder + poller timer/parse success path under `test/` if cheap (Unity native); otherwise parser already covered

## Success criteria

1. With Sports focused and network up, sim shows `live: true` score/inning from production API within ~30s of a successful cron cache.
2. Killing network keeps the last bound scores (fixture or last good).
3. Radar and scores can poll without clobbering each other’s in-flight HTTP.
4. SIM.md describes both behaviors.
