# MLB Next-Game Centered Logos Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Center the not-live MLB next-game card and show both team logos (logos-only hero) using backend abbreviations + bundled LVGL assets.

**Architecture:** Backend adds `teamAbbr` / `opponentAbbr` / `homeAway` to the scores blob. Firmware parses those into `SportsMlbView` (no LVGL). Sim bundles 30 MLB logos as LVGL C arrays keyed by ESPN abbreviation and renders a vertically centered logos-only hero with `@`/`vs` connector.

**Tech Stack:** TypeScript/Vitest (backend), C++17 + Unity/PlatformIO (firmware), LVGL 8.4 + SDL sim, PNG→LVGL generator (reuse weather icon pipeline)

**Spec:** `docs/superpowers/specs/2026-07-23-mlb-next-game-logos-design.md`

## Global Constraints

- Logos-as-hero, logos only (no matchup nickname line when logos render)
- Both teams; connector `@` when away, `vs` when home
- Missing logo → abbreviation text in that slot
- Missing abbrs → centered text-only fallback (existing fields)
- Live MLB + Flagstand unchanged this pass
- No on-device logo download/decode
- `desk_display` must not depend on LVGL
- MLB marks are trademarked — vendored assets are for personal dial use; document in `THIRD_PARTY.md`

## File map

| Path | Responsibility |
|------|----------------|
| `desk-display-backend/src/lib/types/scores.ts` | API types for new mlb fields |
| `desk-display-backend/src/lib/fetchers/mlb.ts` | Populate abbrs + homeAway |
| `desk-display-backend/src/lib/fetchers/mlb.test.ts` | Backend unit tests |
| `lib/desk_display/include/desk_display/scores.hpp` | `MlbScores` fields |
| `lib/desk_display/src/scores.cpp` | Parse new JSON fields |
| `lib/desk_display/include/desk_display/screen_sports.hpp` | `SportsMlbView` fields |
| `lib/desk_display/src/screen_sports.cpp` | Copy into view + connector |
| `fixtures/scores.json` | Sim/test fixture with abbrs |
| `test/test_parsers/test_main.cpp` | Parser assertions |
| `test/test_screen_sports/test_main.cpp` | View-model assertions |
| `assets/mlb/THIRD_PARTY.md` | Trademark / personal-use note |
| `assets/mlb/png/*.png` | Source logos (~56px) |
| `support/gen_mlb_logos.py` | PNG → LVGL C arrays |
| `src/sim/assets/mlb/*.c` + header | Generated LVGL descriptors |
| `src/sim/mlb_img.hpp` / `.cpp` | `mlbTeamLogoImg(abbr)` |
| `src/sim/sim_app.cpp` | Centered logos-only next-game UI |

---

### Task 1: Backend — mlb abbr + homeAway fields

**Repo:** `/Users/bruceclingan/Projects/desk-display-backend`

**Files:**
- Modify: `src/lib/types/scores.ts`
- Modify: `src/lib/fetchers/mlb.ts`
- Modify: `src/lib/fetchers/mlb.test.ts`
- Modify: `docs/BACKEND_PLAN.md` (field table row only)

**Interfaces:**
- Produces on `MlbScores`:
  - `teamAbbr: string | null`
  - `opponentAbbr: string | null`
  - `homeAway: "home" | "away" | null`
- Consumes: existing `EspnCompetition` competitors (`abbreviation`, `homeAway`)

- [ ] **Step 1: Extend `MlbScores` type**

```ts
  /** Configured MLB_TEAM abbreviation, e.g. "HOU". */
  teamAbbr: string | null;
  /** Opponent abbreviation for the described non-live game; null when live / no game. */
  opponentAbbr: string | null;
  /** Configured team's home/away for that game; null when live / no game. */
  homeAway: "home" | "away" | null;
```

- [ ] **Step 2: Write failing tests in `mlb.test.ts`**

For the existing not-live next-game case (e.g. HOU away at opponent), assert:

```ts
expect(result.teamAbbr).toBe("HOU");
expect(result.opponentAbbr).toBe("SEA"); // or whatever abbr the mock uses
expect(result.homeAway).toBe("away");
```

For a live-game case, assert:

```ts
expect(result.teamAbbr).toBe("HOU");
expect(result.opponentAbbr).toBeNull();
expect(result.homeAway).toBeNull();
```

For a home next-game mock, assert `homeAway === "home"` and `matchup` still uses `vs.`.

- [ ] **Step 3: Run tests — expect fail**

```bash
cd /Users/bruceclingan/Projects/desk-display-backend && npm test -- src/lib/fetchers/mlb.test.ts
```

Expected: FAIL on missing fields / undefined property

- [ ] **Step 4: Implement in `withDisplayFields`**

Add helper (near `formatMatchup`):

```ts
function matchupParticipants(
  teamAbbr: string,
  competition: EspnCompetition,
): { teamAbbr: string; opponentAbbr: string; homeAway: "home" | "away" } | null {
  const teamCompetitor = competition.competitors?.find(
    (c) => c.team.abbreviation.toUpperCase() === teamAbbr,
  );
  const opponent = competition.competitors?.find((c) => c !== teamCompetitor);
  if (!teamCompetitor || !opponent) return null;
  return {
    teamAbbr: teamCompetitor.team.abbreviation.toUpperCase(),
    opponentAbbr: opponent.team.abbreviation.toUpperCase(),
    homeAway: teamCompetitor.homeAway,
  };
}
```

In `withDisplayFields`, when `!scoreFields.live && matchupCompetition`:

```ts
const parts = matchupParticipants(teamAbbr, matchupCompetition);
return {
  ...scoreFields,
  matchup,
  whenEt,
  record: standing?.record ?? null,
  standingLine: standing?.standingLine ?? null,
  teamAbbr: teamAbbr.toUpperCase(),
  opponentAbbr: parts?.opponentAbbr ?? null,
  homeAway: parts?.homeAway ?? null,
};
```

When live (or no matchup competition):

```ts
teamAbbr: teamAbbr.toUpperCase(),
opponentAbbr: null,
homeAway: null,
```

- [ ] **Step 5: Re-run tests — pass**

- [ ] **Step 6: Update `docs/BACKEND_PLAN.md` field table** with the three new rows

- [ ] **Step 7: Commit (backend repo)**

```bash
git add src/lib/types/scores.ts src/lib/fetchers/mlb.ts src/lib/fetchers/mlb.test.ts docs/BACKEND_PLAN.md
git commit -m "$(cat <<'EOF'
feat: add mlb teamAbbr, opponentAbbr, and homeAway to scores

EOF
)"
```

---

### Task 2: Firmware — parse new mlb fields

**Repo:** `desktop-display-firmware`

**Files:**
- Modify: `lib/desk_display/include/desk_display/scores.hpp`
- Modify: `lib/desk_display/src/scores.cpp`
- Modify: `fixtures/scores.json`
- Modify: `test/test_parsers/test_main.cpp`

**Interfaces:**
- Produces on `MlbScores`:
  - `constexpr std::size_t kMaxMlbAbbr = 8;`
  - `bool hasTeamAbbr; char teamAbbr[kMaxMlbAbbr];`
  - `bool hasOpponentAbbr; char opponentAbbr[kMaxMlbAbbr];`
  - `enum class MlbHomeAway : uint8_t { Unknown = 0, Home, Away };`
  - `MlbHomeAway homeAway;` (Unknown when null/omitted)

- [ ] **Step 1: Write failing parser tests**

In `test_parse_scores_fixture`, after standingLine asserts:

```cpp
  TEST_ASSERT_TRUE(s.mlb.hasTeamAbbr);
  TEST_ASSERT_EQUAL_STRING("HOU", s.mlb.teamAbbr);
  TEST_ASSERT_TRUE(s.mlb.hasOpponentAbbr);
  TEST_ASSERT_EQUAL_STRING("CHW", s.mlb.opponentAbbr);
  TEST_ASSERT_EQUAL(static_cast<int>(MlbHomeAway::Away),
                    static_cast<int>(s.mlb.homeAway));
```

Add `test_parse_scores_home_away_home` with inline JSON `homeAway":"home"` → `MlbHomeAway::Home`.

Add `test_parse_scores_abbrs_optional` with live JSON lacking the new fields → `hasTeamAbbr` false, `homeAway == Unknown`.

- [ ] **Step 2: Run — expect fail**

```bash
pio test -e native -f test_parsers
```

- [ ] **Step 3: Extend `scores.hpp` `MlbScores`** with the fields above (`#include <cstdint>` already available via other headers or add it)

- [ ] **Step 4: Parse in `scores.cpp` after standingLine**

```cpp
  out.mlb.hasTeamAbbr = false;
  if (!mlb["teamAbbr"].isNull() && mlb["teamAbbr"].is<const char*>()) {
    out.mlb.hasTeamAbbr = true;
    copyStr(out.mlb.teamAbbr, sizeof(out.mlb.teamAbbr),
            mlb["teamAbbr"].as<const char*>());
  }

  out.mlb.hasOpponentAbbr = false;
  if (!mlb["opponentAbbr"].isNull() && mlb["opponentAbbr"].is<const char*>()) {
    out.mlb.hasOpponentAbbr = true;
    copyStr(out.mlb.opponentAbbr, sizeof(out.mlb.opponentAbbr),
            mlb["opponentAbbr"].as<const char*>());
  }

  out.mlb.homeAway = MlbHomeAway::Unknown;
  if (!mlb["homeAway"].isNull() && mlb["homeAway"].is<const char*>()) {
    const char* ha = mlb["homeAway"].as<const char*>();
    if (std::strcmp(ha, "home") == 0) {
      out.mlb.homeAway = MlbHomeAway::Home;
    } else if (std::strcmp(ha, "away") == 0) {
      out.mlb.homeAway = MlbHomeAway::Away;
    }
  }
```

- [ ] **Step 5: Update `fixtures/scores.json` mlb object**

```json
"teamAbbr":"HOU","opponentAbbr":"CHW","homeAway":"away"
```

(Use ESPN White Sox abbr `CHW`, not nickname-based `CWS`.)

- [ ] **Step 6: Re-run `pio test -e native -f test_parsers` — pass**

- [ ] **Step 7: Commit**

```bash
git add lib/desk_display/include/desk_display/scores.hpp lib/desk_display/src/scores.cpp fixtures/scores.json test/test_parsers/test_main.cpp
git commit -m "$(cat <<'EOF'
feat: parse mlb teamAbbr, opponentAbbr, and homeAway

EOF
)"
```

---

### Task 3: Firmware — SportsMlbView pass-through + connector

**Files:**
- Modify: `lib/desk_display/include/desk_display/screen_sports.hpp`
- Modify: `lib/desk_display/src/screen_sports.cpp`
- Modify: `test/test_screen_sports/test_main.cpp`

**Interfaces:**
- Produces on `SportsMlbView`:
  - `bool hasTeamAbbr; char teamAbbr[kMaxMlbAbbr];`
  - `bool hasOpponentAbbr; char opponentAbbr[kMaxMlbAbbr];`
  - `MlbHomeAway homeAway;`
  - `bool hasConnector; char connector[4];` — `"@"` or `"vs"` when homeAway known; empty otherwise
  - `bool showLogoHero;` — true when `!live && hasTeamAbbr && hasOpponentAbbr`

- [ ] **Step 1: Failing tests in `test_bind_fixture_ready_mlb_next_game`**

```cpp
  TEST_ASSERT_TRUE(v.mlb.hasTeamAbbr);
  TEST_ASSERT_EQUAL_STRING("HOU", v.mlb.teamAbbr);
  TEST_ASSERT_TRUE(v.mlb.hasOpponentAbbr);
  TEST_ASSERT_EQUAL_STRING("CHW", v.mlb.opponentAbbr);
  TEST_ASSERT_EQUAL(static_cast<int>(MlbHomeAway::Away),
                    static_cast<int>(v.mlb.homeAway));
  TEST_ASSERT_TRUE(v.mlb.hasConnector);
  TEST_ASSERT_EQUAL_STRING("@", v.mlb.connector);
  TEST_ASSERT_TRUE(v.mlb.showLogoHero);
```

In `test_mlb_live_score_and_inning`, assert `showLogoHero == false`.

- [ ] **Step 2: Run — expect fail**

```bash
pio test -e native -f test_screen_sports
```

- [ ] **Step 3: Extend `SportsMlbView` and `fillMlbView`**

```cpp
  out.hasTeamAbbr = m.hasTeamAbbr;
  copyStr(out.teamAbbr, sizeof(out.teamAbbr), m.teamAbbr);
  out.hasOpponentAbbr = m.hasOpponentAbbr;
  copyStr(out.opponentAbbr, sizeof(out.opponentAbbr), m.opponentAbbr);
  out.homeAway = m.homeAway;
  out.hasConnector = false;
  out.connector[0] = '\0';
  if (m.homeAway == MlbHomeAway::Away) {
    out.hasConnector = true;
    copyStr(out.connector, sizeof(out.connector), "@");
  } else if (m.homeAway == MlbHomeAway::Home) {
    out.hasConnector = true;
    copyStr(out.connector, sizeof(out.connector), "vs");
  }
  out.showLogoHero = !m.live && m.hasTeamAbbr && m.hasOpponentAbbr;
```

Keep existing primary/secondary / matchup copy behavior unchanged.

- [ ] **Step 4: Tests pass**

- [ ] **Step 5: Commit**

```bash
git add lib/desk_display/include/desk_display/screen_sports.hpp lib/desk_display/src/screen_sports.cpp test/test_screen_sports/test_main.cpp
git commit -m "$(cat <<'EOF'
feat: expose mlb logo hero fields on SportsMlbView

EOF
)"
```

---

### Task 4: Vendor MLB logos → LVGL assets + lookup

**Files:**
- Create: `assets/mlb/THIRD_PARTY.md`
- Create: `assets/mlb/png/{ABBR}.png` (30 teams, ~56×56)
- Create: `support/gen_mlb_logos.py`
- Create: `src/sim/assets/mlb/mlb_logo_*.c` + `mlb_logos_assets.h`
- Create: `src/sim/mlb_img.hpp`
- Create: `src/sim/mlb_img.cpp`

**Interfaces:**
- Produces: `const lv_img_dsc_t* mlbTeamLogoImg(const char* abbr);` — case-insensitive lookup; returns `nullptr` if unknown/null/empty

**Team list (ESPN abbreviations):**  
`ARI ATL BAL BOS CHC CHW CIN CLE COL DET HOU KC LAA LAD MIA MIL MIN NYM NYY OAK PHI PIT SD SF SEA STL TB TEX TOR WSH`

**Asset source (build-time only):** ESPN CDN scoreboard logos, e.g.  
`https://a.espncdn.com/i/teamlogos/mlb/500/{abbr_lower}.png`  
Resize to 56px with transparency preserved. Do **not** fetch at device runtime.

- [ ] **Step 1: Write `assets/mlb/THIRD_PARTY.md`**

State: team marks are property of MLB / clubs; assets vendored for personal non-commercial dial use only; sourced from ESPN CDN at build time; no affiliation with MLB/ESPN.

- [ ] **Step 2: Download + resize PNGs into `assets/mlb/png/`**

Script sketch (`support/fetch_mlb_logos.py` or inline in gen):

```python
# for each abbr: urllib request ESPN URL → PIL resize 56 → assets/mlb/png/{ABBR}.png
```

- [ ] **Step 3: Create `support/gen_mlb_logos.py`** (mirror `support/gen_weather_icons.py` `png_to_lvgl_c`)

Emit `src/sim/assets/mlb/mlb_logo_hou.c` etc. and `mlb_logos_assets.h` declaring `extern const lv_img_dsc_t mlb_logo_hou;` for each team.

- [ ] **Step 4: Implement lookup**

`mlb_img.hpp`:

```cpp
#pragma once
#include "lvgl.h"
const lv_img_dsc_t* mlbTeamLogoImg(const char* abbr);
```

`mlb_img.cpp`: strcmp against uppercase table of 30 abbrs → descriptor; else `nullptr`.

- [ ] **Step 5: Build sim to confirm assets compile**

```bash
pio run -e sim
```

Expected: success (link includes new `.c` under `src/sim/`)

- [ ] **Step 6: Commit**

```bash
git add assets/mlb support/gen_mlb_logos.py support/fetch_mlb_logos.py src/sim/assets/mlb src/sim/mlb_img.hpp src/sim/mlb_img.cpp
git commit -m "$(cat <<'EOF'
feat: vendor MLB team logos as LVGL assets

EOF
)"
```

---

### Task 5: Sim — centered logos-only next-game card

**Files:**
- Modify: `src/sim/sim_app.cpp` (Sports / MLB branch)
- Include: `mlb_img.hpp`

**Interfaces:**
- Consumes: `SportsMlbView::showLogoHero`, `teamAbbr`, `opponentAbbr`, `connector`, `whenEt`, `record`, `standingLine`
- Consumes: `mlbTeamLogoImg`

- [ ] **Step 1: Replace not-live MLB top-stacked layout**

When `v.mlb.showLogoHero`:

1. Create a column container on `body_`, size ~280×220, `lv_obj_center(col)`, flex column, center align, pad row ~6.
2. Title `Next Game` (accent, montserrat_20).
3. Row: team img (or abbr label if `mlbTeamLogoImg` null) · connector label · opponent img/abbr. Logo zoom if needed so ~56px.
4. `whenEt` (dim), `record` (white), `standingLine` (dim) — skip empty strings.
5. Do **not** create the matchup nickname label.

When `!showLogoHero` (text fallback): same centered column, but lines are matchup / whenEt / record / standingLine (still centered — no `y = 48` top stack).

Live path: leave score/inning center align as today.

- [ ] **Step 2: Build + run sim**

```bash
pio run -e sim
# run sim binary / existing sim launch command from docs/SIM.md
```

Expected: Next Game card vertically centered; HOU + CHW logos (or abbrs); `@`; time/record/standing below; no “Astros @ White Sox” line.

- [ ] **Step 3: Commit**

```bash
git add src/sim/sim_app.cpp
git commit -m "$(cat <<'EOF'
feat: center MLB next-game card with logos-only hero

EOF
)"
```

---

### Task 6: Spec checklist + manual pass

- [ ] **Step 1: Walk `docs/superpowers/specs/2026-07-23-mlb-next-game-logos-design.md` Goals / Non-goals / UX** — confirm each item covered by Tasks 1–5

- [ ] **Step 2: Manual sim checklist**
  - [ ] Next-game content centered (not glued to top)
  - [ ] Both logos visible for fixture
  - [ ] No matchup nickname when logos show
  - [ ] Live card still works (rotate not required; bind live JSON or temporary fixture swap)
  - [ ] Flagstand still rotates

- [ ] **Step 3: If spacing clips on round bezel, nudge pad/font only in `sim_app.cpp` — commit** `fix: tune MLB next-game logo card spacing`

---

## Self-review (plan vs spec)

| Spec requirement | Task |
|------------------|------|
| Vertically center next-game card | 5 |
| Both team logos, logos-only hero | 5 (+ 4 assets) |
| `@` / `vs` from homeAway | 1, 3, 5 |
| Abbr fallback if asset missing | 5 |
| Text fallback if abbrs missing | 5 |
| Backend teamAbbr/opponentAbbr/homeAway | 1 |
| Firmware parse + view-model | 2, 3 |
| Bundled assets, no runtime download | 4 |
| Live / Flagstand unchanged | 5 (explicit) |
| Unit tests parser + view | 2, 3 |
| Fixture updated | 2 |
