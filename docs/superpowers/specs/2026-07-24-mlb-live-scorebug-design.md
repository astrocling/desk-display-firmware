# Live MLB scorebug (API on-demand + sim UI)

**Date:** 2026-07-24  
**Status:** Ready for review  
**Repos:** `desk-display-backend` (data path + contract) and `desktop-display-firmware` (parser, view-model, sim UI)

## Goals

- Show a rich **live** MLB card: logos + per-team runs, inning, count/outs, bases, batter/pitcher.
- Fix live identity: always populate `teamAbbr` / `opponentAbbr` / `homeAway` while the game is in progress (today those are stripped).
- Keep a **single client endpoint** (`GET /api/scores`); refresh ESPN on read during live with a short shared TTL (~45s).
- Sim is the first UI consumer; shared C++ view-model stays LVGL-free for dial later.

## Non-goals (this pass)

- Drawn baseball diamond (text bases only)
- Hits/errors, full linescore, win probability
- Dial LVGL live layout (parser + view-model only)
- Hitting ESPN from the sim/dial
- Changing not-live next-game card
- 1-minute cron for all scores (cron stays ~15 min for cold / not-live)

## Decisions

| Topic | Choice |
|-------|--------|
| Richness | Logos, split runs, inning, balls/strikes/outs, bases, batter + pitcher names |
| Layout | Scorebug stack (centered column) |
| Data path | On-demand ESPN via `GET /api/scores` when live + ~45s cache |
| Bases | Compact text (`Empty` / `1st` / `1st & 2nd` / `Loaded`, etc.) |
| Title | `LIVE` (accent) |
| Team order | Configured `MLB_TEAM` row first, opponent second |

## Backend data path

```
Client GET /api/scores
        │
        ├─ Read Redis `scores`
        │
        ├─ If shouldRefreshLive(blob):
        │     (last ESPN check older than ~45s) AND
        │     (blob.mlb.live OR blob suggests a game may have started —
        │      e.g. nextGame ISO ≤ now, or same-calendar-day scheduled game)
        │         → fetch ESPN scoreboard → rebuild mlb (rich) + keep flagstand
        │            → write Redis (update updatedAt) → return
        │
        └─ Else → return cached blob (503 only if Redis empty)
```

**TTL clock:** use blob `updatedAt` as “last successful ESPN-backed write” for live refreshes. Cron writes also bump `updatedAt`; that is fine — the 45s gate still caps ESPN QPS.

**Cron `/api/cron/scores`:** unchanged cadence; continues to seed not-live next-game + standings and may set `live: true` with partial fields. Live richness and sub-minute freshness are owned by the read path. Prefer teaching the cron fetcher the same rich live mapper so cron and on-demand stay consistent.

**Concurrency:** the 45s TTL is the dedupe for one dial; in-flight promise coalescing is optional.

**Failure:** if ESPN refresh fails, return last good Redis blob. Do not 503 if any scores blob exists.

**Standings:** do not re-fetch standings on every live refresh; reuse `record` / `standingLine` from the existing blob when present.

## Live `mlb` contract

Existing fields remain. While `live: true`:

| Field | Type | Notes |
|-------|------|--------|
| `teamAbbr` | string | Always configured team |
| `opponentAbbr` | string \| null | Opponent; required for logo rows when available |
| `homeAway` | `"home"` \| `"away"` \| null | Configured team’s side |
| `score` | string \| null | Keep `"team-opponent"` fallback for old clients |
| `inning` | string \| null | Short form (`Bot 2`) |
| `teamRuns` | number \| null | Configured team runs |
| `opponentRuns` | number \| null | Opponent runs |
| `balls` | number \| null | 0–3 |
| `strikes` | number \| null | 0–2 |
| `outs` | number \| null | 0–2 |
| `onFirst` / `onSecond` / `onThird` | boolean \| null | Base occupancy |
| `batterName` | string \| null | Prefer ESPN `shortName` |
| `pitcherName` | string \| null | Prefer ESPN `shortName` |
| `nextGame` / `matchup` / `whenEt` | null while live | Same as today |

When not live, new live-only fields are `null` / omitted-as-null; next-game fields unchanged.

Example (live):

```json
{
  "mlb": {
    "live": true,
    "score": "0-3",
    "inning": "Bot 2",
    "nextGame": null,
    "matchup": null,
    "whenEt": null,
    "record": "50-54",
    "standingLine": "3rd AL West · 2 GB",
    "teamAbbr": "HOU",
    "opponentAbbr": "CHW",
    "homeAway": "away",
    "teamRuns": 0,
    "opponentRuns": 3,
    "balls": 2,
    "strikes": 1,
    "outs": 1,
    "onFirst": false,
    "onSecond": false,
    "onThird": false,
    "batterName": "M. Murakami",
    "pitcherName": "A. Blubaugh"
  },
  "flagstand": {},
  "updatedAt": "2026-07-25T00:20:00.000Z"
}
```

## Live card UX (sim)

Vertically centered column (~280×220 content, same family as next-game):

1. Title: `LIVE`
2. Row: configured-team logo + large `teamRuns`
3. Row: opponent logo + large `opponentRuns`
4. Inning (dim)
5. Count/outs line when any present, e.g. `2-1 · 1 out` / `2 outs`
6. Bases text when flags present, e.g. `Empty`, `1st`, `1st & 2nd`, `Loaded`
7. Batter · pitcher (dim, smaller), omit missing names

**Fallbacks**

- Missing logo asset → abbr text in that slot  
- Missing `opponentAbbr` / runs → fall back to legacy centered `score` + `inning`  
- Missing situation → still show logos + runs + inning  

## Firmware

- Extend `MlbScores` / `parseScores` / `SportsMlbView` with the new fields  
- Live logo rows when `live && hasTeamAbbr && hasOpponentAbbr && hasTeamRuns && hasOpponentRuns` (or equivalent)  
- Format helpers for count line and bases phrase (unit-tested, no LVGL)  
- Sim: replace current live score/inning-only branch with the scorebug stack; reuse bundled MLB logos  
- Keep ~30s Sports poll; backend TTL ~45s means many polls are cache hits  

## Testing

**Backend:** Vitest for live field mapping from a fixture ESPN competition/situation; TTL/cache behavior with a fake clock or injected “now”; not-live path unchanged.

**Firmware:** Unity parser tests for live JSON; view-model / format helpers for count + bases strings; existing next-game tests stay green.

**Manual:** Sim on Sports during a live HOU game — logos, runs, inning, count, bases, names update within ~30–45s of ESPN.

## Success criteria

1. Live API returns abbrs + split runs + situation fields with `updatedAt` advancing at least every ~45s while Sports is polling.  
2. Sim live card shows logos next to each team’s runs (no ambiguous `0-3` alone).  
3. Not-live next-game card unchanged.  
4. ESPN blip does not blank the card if Redis still has a prior blob.
