# Live MLB Scorebug Implementation Plan

> **For agentic workers:** Execute task-by-task. Checkboxes track progress.

**Goal:** On-demand rich live MLB blob via `GET /api/scores` (~45s TTL) and sim scorebug UI with logos + situation.

**Architecture:** Backend `fetchMlb` gains live situation fields and always fills abbrs while live. Scores GET refreshes ESPN when cache is stale and a game may be live. Firmware parses new fields and sim renders the LIVE scorebug.

**Tech Stack:** Next.js API + Vitest (backend); C++17 Unity + LVGL sim (firmware)

## Global Constraints

- Single client endpoint: `GET /api/scores`
- Live TTL ~45s using blob `updatedAt`
- Keep legacy `score` string; add `teamRuns`/`opponentRuns`
- Sim-first UI; dial LVGL out of scope
- Do not re-fetch standings on every live refresh

---

### Task 1: Backend types + live mapper

**Files:**
- Modify: `desk-display-backend/src/lib/types/scores.ts`
- Modify: `desk-display-backend/src/lib/fetchers/mlb.ts`
- Modify: `desk-display-backend/src/lib/fetchers/mlb.test.ts`
- Modify: `desk-display-backend/docs/BACKEND_PLAN.md` (mlb shape)

- [ ] Extend `MlbScores` with live fields
- [ ] Map ESPN situation + competitors; populate abbrs while live
- [ ] Tests for live mapping; `npm test`

### Task 2: On-demand refresh in GET /api/scores

**Files:**
- Create: `desk-display-backend/src/lib/scores-refresh.ts` (shouldRefresh + refresh helper)
- Modify: `desk-display-backend/src/app/api/scores/route.ts`
- Test: unit tests for shouldRefresh

- [ ] `shouldRefreshLive(blob, now)` per spec
- [ ] GET path: refresh MLB (reuse flagstand), write Redis, return; on ESPN fail return stale
- [ ] Deploy or local verify against live game

### Task 3: Firmware parse + view helpers

**Files:**
- Modify: `lib/desk_display/include/desk_display/scores.hpp`
- Modify: `lib/desk_display/src/scores.cpp`
- Modify: `lib/desk_display/include/desk_display/screen_sports.hpp`
- Modify: `lib/desk_display/src/screen_sports.cpp`
- Modify: `test/test_parsers/test_main.cpp`, `test/test_screen_sports/test_main.cpp`
- Update: `fixtures/scores.json` optional live sample or inline tests only

- [ ] Parse new fields; format count/bases strings
- [ ] View-model flags for live logo rows
- [ ] `pio test -e native -f test_parsers` and `test_screen_sports`

### Task 4: Sim LIVE scorebug UI

**Files:**
- Modify: `src/sim/sim_app.cpp` live Sports branch
- Modify: `docs/SIM.md` brief note if needed

- [ ] Render LIVE stack with logos + runs + situation
- [ ] `pio run -e sim` link OK
