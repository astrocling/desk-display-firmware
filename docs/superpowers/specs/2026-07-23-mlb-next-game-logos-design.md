# MLB next-game centered logos

**Date:** 2026-07-23  
**Status:** Approved for planning  
**Scope:** Center the not-live MLB next-game card and show both team logos (sim first; shared view-model + parser for dial later). Companion backend fields in `desk-display-backend`.

## Goals

- Fix the next-game MLB card sitting too high on the dial by vertically centering the content block.
- Show **both** team logos as the visual hero of the matchup (configured team + opponent).
- Avoid redundant matchup nickname text when logos are present (`logos only`).
- Prefer hardware-safe **bundled** logo assets (weather-icon pattern) — no on-device logo downloads.
- Keep domain logic LVGL-free and unit-tested; sim is the first consumer.

## Non-goals (this pass)

- Live-game score card redesign (leave score/inning centered as today)
- Flagstand logos or layout changes beyond any incidental centering consistency
- Runtime HTTP fetch/decode/cache of logo images on the dial
- Night/alternate logo variants
- Device dial LVGL screen beyond what shared assets/view-model already enable
- Replacing or removing existing `matchup` string from the API (kept for fallback / tests)

## UX / layout

**Not-live MLB next-game card (logos-as-hero, logos only):**

1. Small title: `Next Game` (accent)
2. Hero row, centered: team logo · connector · opponent logo  
   - Connector: `@` when configured team is away, `vs` when home (from `homeAway`)
   - Logo size ~52–58px on the 360×360 dial
3. Below hero: `whenEt`, then `record`, then `standingLine` (dim / white hierarchy as today)
4. Entire block vertically centered in the content area (not top-stacked from `y ≈ 48`)

**Missing asset:** If a logo for an abbreviation is not bundled, show that team's abbreviation text in the logo slot (same size footprint).

**Missing opponent / abbrs:** If `teamAbbr` / `opponentAbbr` are absent, fall back to the current text stack (matchup + when + record + standing), but still **vertically centered**.

**Live MLB:** Unchanged this pass (score + inning centered).

## Data contract (backend)

Extend `GET /api/scores` → `mlb` (in `desk-display-backend`) with:

| Field | Type | Meaning |
|-------|------|---------|
| `teamAbbr` | string \| null | Configured `MLB_TEAM` abbreviation (e.g. `HOU`) |
| `opponentAbbr` | string \| null | Opponent abbreviation for the described next/current non-live game context |
| `homeAway` | `"home"` \| `"away"` \| null | Configured team's home/away for that game; drives `@` vs `vs` |

Population rules:

- When not live and a next/upcoming game is described: populate all three from the same competition used for `matchup` / `whenEt`.
- When live or no game context: `opponentAbbr` / `homeAway` may be null; `teamAbbr` may still be the configured team for future use.
- Existing fields (`live`, `score`, `inning`, `nextGame`, `matchup`, `whenEt`, `record`, `standingLine`) unchanged.

Example (not live):

```json
{
  "mlb": {
    "live": false,
    "score": null,
    "inning": null,
    "nextGame": "2026-07-24T23:40:00Z",
    "matchup": "Astros @ White Sox",
    "whenEt": "Fri 7/24 7:40 PM",
    "record": "50-54",
    "standingLine": "3rd AL West · 2 GB",
    "teamAbbr": "HOU",
    "opponentAbbr": "CWS",
    "homeAway": "away"
  }
}
```

## Assets

- Bundle all 30 MLB team logos as LVGL image descriptors (compiled C arrays), keyed by uppercase abbreviation.
- Same pipeline family as weather icons: source images → rasterize small (~56px) → `TRUE_COLOR_ALPHA` / project color depth → `src/sim/assets/mlb/` (sim-first).
- Helper such as `mlbTeamLogoImg(const char* abbr)` returns the descriptor or null when unknown.
- Prefer a clearly licensed / redistributable logo pack or simple mark set suitable for a personal dial; document source + license in vendored notes. Do not scrape ESPN PNG URLs onto the device at runtime.
- Flash cost is acceptable on N16R8 (~tens of KB per small logo × 30); keep assets small and single size (zoom if needed).

## View-model & parsing (firmware)

Extend `MlbScores` / `parseScores` and `SportsMlbView`:

- Parse optional `teamAbbr`, `opponentAbbr`, `homeAway`.
- Expose them on the view for the UI layer.
- Expose a simple connector hint (`@` / `vs`) or raw `homeAway` for the sim to render.
- Do not put LVGL in `desk_display`.

Fixture `fixtures/scores.json` updated to include the new fields for sim/manual runs.

## Sim UI

Update `src/sim/sim_app.cpp` Sports → MLB not-live branch:

- Build a vertically centered column (flex or center-aligned group) for the next-game card.
- Render logos-only hero when both abbrs resolve (or one resolves with abbr fallback in the other slot).
- Omit the matchup nickname label when showing the logo hero.
- Keep Flagstand and live MLB paths otherwise as today.

## Testing

- Parser: new fields present / null / omitted.
- Screen sports view: abbrs and homeAway pass through; summary primary/secondary behavior unchanged for live.
- Manual sim: next-game card is vertically centered; logos (or abbr fallbacks) appear for fixture teams.

## Architecture

```text
Backend mlb fetcher  →  scores JSON (+ abbrs, homeAway)
                              ↓
              parseScores / ScreenSports (no LVGL)
                              ↓
         sim_app LVGL  →  future dial sports screen
                              ↑
         bundled mlb logo assets (abbr → img)
```

## Implementation order

1. Backend: add `teamAbbr` / `opponentAbbr` / `homeAway` to scores blob + tests.
2. Firmware parser + view-model + fixture + unit tests.
3. Vendor logo assets + lookup helper.
4. Sim next-game layout: center + logos-only hero.
5. Manual sim check; adjust spacing if the round bezel clips.

## Open decisions resolved

- Logos now (not deferred): yes, via bundled assets.
- Both teams: yes.
- Layout: logos as hero (option B).
- Names with logos: no — logos only; abbr fallback if asset missing.
- No on-device logo download.
