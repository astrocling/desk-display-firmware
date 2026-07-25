# Live MLB batter / pitcher stats

**Date:** 2026-07-25  
**Status:** Ready for review  
**Repos:** `desk-display-backend` (ESPN enrichment + scores contract) and `desktop-display-firmware` (parser, format helpers, sim LIVE card)

## Goals

- Enrich the live scorebug batter/pitcher lines with:
  - **Batter:** season batting average + game-line summary
  - **Pitcher:** season ERA + game-line summary
- Keep a single client endpoint (`GET /api/scores`); no ESPN calls from sim/dial.
- Fail soft: missing AVG/ERA/summary must not blank names or the rest of the scorebug.

## Non-goals (this pass)

- Pitch count / PC-ST on the card (available in boxscore later if wanted)
- Full boxscore, OBP/SLG/WHIP, or other season stats
- Headshots or player cards
- Changing not-live next-game card
- Dial-specific LVGL layout beyond what sim already mirrors via shared view-model strings

## Decisions

| Topic | Choice |
|-------|--------|
| Data path | Scoreboard (existing) + **one** ESPN game summary when live |
| Season AVG / ERA | From summary `boxscore.players[].statistics` by `playerId` |
| Game lines | From scoreboard `situation.batter.summary` / `situation.pitcher.summary` |
| Display | Two lines: `AB: …` and `P: …` (already labeled) |
| Missing pieces | Omit that fragment; still show name when present |
| Cache | Same ~45s live TTL; summary fetch only on live ESPN refresh |

## Backend data path

On live ESPN refresh (existing `shouldRefreshLive` / scores GET path):

```
fetch ESPN scoreboard
  → find configured team's live competition + situation
  → names + game summaries from situation.batter / situation.pitcher
  → if live event id known:
        fetch ESPN summary?event={id}
          → boxscore batting row for batter playerId → AVG
          → boxscore pitching row for pitcher playerId → ERA
  → if summary fails: keep names + game summaries; AVG/ERA null
```

Do **not** add per-athlete statistics URL calls in this pass.

## Contract additions (`mlb` while live)

Existing `batterName` / `pitcherName` remain. New nullable fields:

| Field | Type | Notes |
|-------|------|--------|
| `batterAvg` | string \| null | Season AVG display, e.g. `".222"` (ESPN `displayValue`) |
| `batterSummary` | string \| null | Game line, e.g. `"1-3, BB"` |
| `pitcherEra` | string \| null | Season ERA display, e.g. `"1.93"` |
| `pitcherSummary` | string \| null | Game line, e.g. `"0.2 IP, 0 ER, 0 H, K, BB"` |

When not live, all four are `null`.

Example fragment:

```json
{
  "batterName": "A. Kirk",
  "batterAvg": ".222",
  "batterSummary": "1-3, BB",
  "pitcherName": "A. Chapman",
  "pitcherEra": "1.93",
  "pitcherSummary": "0.2 IP, 0 ER, 0 H, K, BB"
}
```

## Display (sim LIVE card)

Replace the current name-only batter/pitcher formatter with compact labeled lines:

```text
AB: A. Kirk .222 · 1-3, BB
P: A. Chapman 1.93 · 0.2 IP, 0 ER, 0 H, K, BB
```

Assembly rules (name required to emit a role line):

1. Start with `AB: {name}` / `P: {name}`
2. If AVG / ERA present, append ` {avg|era}`
3. If summary present, append ` · {summary}`
4. Two roles joined with `\n` when both exist (same as today’s labeled layout)
5. Truncation: prefer keeping name + season stat; allow summary to shorten via existing label wrap width (280) — no hard client-side ellipsis in this pass unless lines overflow the round bezel in manual check

Side-by-side logos + diamond layout unchanged.

## Firmware

- Extend `MlbScores` / `parseScores` / `SportsMlbView` with the four fields
- Update `formatMlbBatterPitcherLine` (unit-tested) to the assembly rules above
- Sim already renders `batterPitcherLine`; no separate LVGL stat widgets

## Testing

**Backend:** Vitest — map scoreboard situation summaries; map boxscore AVG/ERA by player id; summary fetch failure leaves AVG/ERA null without dropping names/summaries.

**Firmware:** Unity — parser fields; format helper cases (full, name+avg only, name+summary only, pitcher-only, both roles).

**Manual:** Sim LIVE during a game — lines show AVG/ERA + game lines within a live poll cycle; kill/summary error path still shows names.

## Success criteria

1. Live `/api/scores` returns the four new fields when ESPN provides them.
2. Sim shows labeled AB/P lines with season stat + game summary when present.
3. Summary enrichment failure does not clear names or the rest of the scorebug.
4. Not-live card and non-live `mlb` fields unchanged.
