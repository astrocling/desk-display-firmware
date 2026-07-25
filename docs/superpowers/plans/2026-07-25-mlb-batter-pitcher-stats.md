# MLB Batter / Pitcher Stats Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enrich live scorebug AB/P lines with season AVG + game summary (batter) and season ERA + game summary (pitcher).

**Architecture:** On live ESPN refresh, keep the existing scoreboard fetch for names and game-line `summary` strings; add one ESPN game-summary request and pull season AVG/ERA from boxscore rows keyed by `playerId`. Firmware parses four new nullable string fields and extends `formatMlbBatterPitcherLine` so the sim keeps rendering a single multiline label.

**Tech Stack:** Next.js + Vitest (`desk-display-backend`); C++17 Unity + LVGL sim (`desktop-display-firmware`)

**Spec:** `docs/superpowers/specs/2026-07-25-mlb-batter-pitcher-stats-design.md`

## Global Constraints

- Single client endpoint: `GET /api/scores` (no ESPN from sim/dial)
- Live TTL ~45s unchanged; summary fetch only during live ESPN refresh
- Soft-fail: summary errors leave AVG/ERA null without clearing names/game summaries
- Display: `AB: {name} {avg} · {summary}` / `P: {name} {era} · {summary}` (omit missing fragments)
- No pitch count / OBP / WHIP in this pass
- Not-live card unchanged

## File map

| File | Responsibility |
|------|----------------|
| `desk-display-backend/src/lib/types/scores.ts` | Contract fields |
| `desk-display-backend/src/lib/fetchers/mlb.ts` | Scoreboard summaries + summary boxscore AVG/ERA |
| `desk-display-backend/src/lib/fetchers/mlb.test.ts` | Vitest coverage |
| `desk-display-backend/docs/BACKEND_PLAN.md` | Document new `mlb.*` fields |
| `desktop-display-firmware/lib/desk_display/include/desk_display/scores.hpp` | C++ `MlbScores` fields + sizes |
| `desktop-display-firmware/lib/desk_display/src/scores.cpp` | JSON parse |
| `desktop-display-firmware/lib/desk_display/include/desk_display/mlb_live_format.hpp` | Buffer size + docs |
| `desktop-display-firmware/lib/desk_display/src/mlb_live_format.cpp` | Line assembly |
| `desktop-display-firmware/test/test_parsers/test_main.cpp` | Parser assertions |
| `desktop-display-firmware/test/test_domain/test_main.cpp` | Format helper cases |
| `desktop-display-firmware/test/test_screen_sports/test_main.cpp` | View-model line assertion |
| `desktop-display-firmware/docs/SIM.md` | One-line note on AB/P stats |

---

### Task 1: Backend contract + scoreboard game summaries

**Files:**
- Modify: `desk-display-backend/src/lib/types/scores.ts`
- Modify: `desk-display-backend/src/lib/fetchers/mlb.ts`
- Modify: `desk-display-backend/src/lib/fetchers/mlb.test.ts`
- Modify: `desk-display-backend/docs/BACKEND_PLAN.md`
- Modify: `desk-display-backend/src/lib/scores-refresh.test.ts` (stub blob fields if required for typecheck)

**Interfaces:**
- Consumes: existing `EspnSituation` / `fetchMlb`
- Produces: `MlbScores.batterSummary` / `pitcherSummary` (string \| null); `batterAvg` / `pitcherEra` always `null` until Task 2

- [ ] **Step 1: Extend `MlbScores` in `src/lib/types/scores.ts`**

After `pitcherName`, add:

```ts
  /** Season AVG display while live, e.g. ".222". */
  batterAvg: string | null;
  /** Game line from ESPN situation, e.g. "1-3, BB". */
  batterSummary: string | null;
  /** Season ERA display while live, e.g. "1.93". */
  pitcherEra: string | null;
  /** Game line from ESPN situation, e.g. "0.2 IP, 0 ER, 0 H, K, BB". */
  pitcherSummary: string | null;
```

- [ ] **Step 2: Write failing Vitest expectations for live summaries**

In `mlb.test.ts`, extend `buildEspnPayload` situation options with optional `batterSummary` / `pitcherSummary` / `batterId` / `pitcherId`, and map them onto ESPN shape:

```ts
batter: event.situation.batterShort
  ? {
      playerId: event.situation.batterId ?? 1,
      summary: event.situation.batterSummary,
      athlete: { shortName: event.situation.batterShort },
    }
  : undefined,
```

Update the live test case situation to include:

```ts
batterShort: "A. Judge",
batterSummary: "1-3, BB",
pitcherShort: "F. Valdez",
pitcherSummary: "5.0 IP, 2 ER, 4 H, 6 K",
```

Expect on `fetchMlb("HOU")` result:

```ts
batterName: "A. Judge",
batterAvg: null, // Task 2 fills this
batterSummary: "1-3, BB",
pitcherName: "F. Valdez",
pitcherEra: null,
pitcherSummary: "5.0 IP, 2 ER, 4 H, 6 K",
```

Also add the four keys as `null` to every existing `toEqual` mlb expectation in this file and in `scores-refresh.test.ts` fixture blobs so TypeScript/tests stay consistent.

- [ ] **Step 3: Run test — expect FAIL**

Run (in `desk-display-backend`):

```bash
npm test -- src/lib/fetchers/mlb.test.ts
```

Expected: FAIL — result missing new fields / summaries not mapped.

- [ ] **Step 4: Implement scoreboard summary mapping (AVG/ERA still null)**

In `mlb.ts`:

1. Extend `EspnSituationPlayer`:

```ts
interface EspnSituationPlayer {
  playerId?: number | string;
  summary?: string;
  athlete?: EspnSituationAthlete;
}
```

2. Add helper:

```ts
function situationSummary(player: EspnSituationPlayer | undefined): string | null {
  const s = player?.summary?.trim();
  return s ? s : null;
}
```

3. Extend `emptyLiveSituation` / `buildScoreFields` Pick unions and all live/post/pre returns with:

```ts
batterAvg: null,
batterSummary: null, // live: situationSummary(sit?.batter)
pitcherEra: null,
pitcherSummary: null, // live: situationSummary(sit?.pitcher)
```

In the `state === "in"` branch set summaries from `situationSummary`; keep `batterAvg` / `pitcherEra` as `null` for this task.

- [ ] **Step 5: Run tests — expect PASS for summaries; AVG/ERA null**

```bash
npm test -- src/lib/fetchers/mlb.test.ts src/lib/scores-refresh.test.ts
```

Expected: PASS.

- [ ] **Step 6: Document fields in `docs/BACKEND_PLAN.md`**

Add four rows next to `batterName` / `pitcherName` in the scores response table.

- [ ] **Step 7: Commit (backend repo)**

```bash
git add src/lib/types/scores.ts src/lib/fetchers/mlb.ts src/lib/fetchers/mlb.test.ts src/lib/scores-refresh.test.ts docs/BACKEND_PLAN.md
git commit -m "$(cat <<'EOF'
feat: expose live batter/pitcher game summaries on scores

EOF
)"
```

---

### Task 2: Backend ESPN summary fetch for AVG / ERA

**Files:**
- Modify: `desk-display-backend/src/lib/fetchers/mlb.ts`
- Modify: `desk-display-backend/src/lib/fetchers/mlb.test.ts`

**Interfaces:**
- Consumes: live event `id`, situation `playerId`s from Task 1
- Produces: `batterAvg` / `pitcherEra` populated from boxscore when summary OK

- [ ] **Step 1: Write failing test for boxscore AVG/ERA enrichment**

Extend `mockEspnFetch` to accept optional `summary?: (url: string) => unknown` and route URLs containing `/summary` (and `event=`) to it.

Add test:

```ts
it("enriches live batter AVG and pitcher ERA from game summary boxscore", async () => {
  const payload = buildEspnPayload([
    {
      date: "2026-07-23T23:00:00Z",
      away: { abbr: "HOU", nick: "Astros", score: "4" },
      home: { abbr: "NYY", nick: "Yankees", score: "2" },
      state: "in",
      detail: "Top 7th",
      situation: {
        batterShort: "A. Judge",
        batterId: 33192,
        batterSummary: "1-3, BB",
        pitcherShort: "F. Valdez",
        pitcherId: 42501,
        pitcherSummary: "5.0 IP, 2 ER",
      },
    },
  ]);

  mockEspnFetch({
    scoreboard: () => payload,
    summary: () => ({
      boxscore: {
        players: [
          {
            statistics: [
              {
                type: "batting",
                labels: ["H-AB", "AVG"],
                athletes: [
                  { athlete: { id: "33192" }, stats: ["1-3", ".311"] },
                ],
              },
            ],
          },
          {
            statistics: [
              {
                type: "pitching",
                labels: ["IP", "ERA"],
                athletes: [
                  { athlete: { id: "42501" }, stats: ["5.0", "2.85"] },
                ],
              },
            ],
          },
        ],
      },
    }),
  });

  const result = await fetchMlb("HOU");
  expect(result.batterAvg).toBe(".311");
  expect(result.pitcherEra).toBe("2.85");
  expect(result.batterSummary).toBe("1-3, BB");
  expect(result.pitcherSummary).toBe("5.0 IP, 2 ER");
});
```

Add a second test: summary returns `ok: false` → names + summaries still set, `batterAvg` / `pitcherEra` null.

Ensure `buildEspnPayload` sets `events[].id` (already `String(index + 1)`).

- [ ] **Step 2: Run test — expect FAIL**

```bash
npm test -- src/lib/fetchers/mlb.test.ts
```

Expected: FAIL — `batterAvg` still null / summary URL 404.

- [ ] **Step 3: Implement summary fetch + boxscore lookup**

In `mlb.ts`:

1. Add constant:

```ts
const ESPN_SUMMARY_URL =
  "https://site.api.espn.com/apis/site/v2/sports/baseball/mlb/summary";
```

2. Add `id?: string` to `EspnEvent`.

3. Add minimal summary types + helpers:

```ts
function situationPlayerId(player: EspnSituationPlayer | undefined): string | null {
  if (player?.playerId === undefined || player.playerId === null) return null;
  return String(player.playerId);
}

function boxscoreStat(
  summary: {
    boxscore?: {
      players?: Array<{
        statistics?: Array<{
          type?: string;
          labels?: string[];
          athletes?: Array<{ athlete?: { id?: string }; stats?: string[] }>;
        }>;
      }>;
    };
  },
  playerId: string,
  statType: "batting" | "pitching",
  label: string,
): string | null {
  for (const group of summary.boxscore?.players ?? []) {
    for (const block of group.statistics ?? []) {
      if (block.type !== statType) continue;
      const idx = (block.labels ?? []).indexOf(label);
      if (idx < 0) continue;
      for (const row of block.athletes ?? []) {
        if (String(row.athlete?.id ?? "") !== playerId) continue;
        const v = row.stats?.[idx]?.trim();
        if (v) return v;
      }
    }
  }
  return null;
}

async function fetchGameSummary(eventId: string): Promise<unknown | null> {
  try {
    const response = await fetch(
      `${ESPN_SUMMARY_URL}?event=${encodeURIComponent(eventId)}`,
    );
    if (!response.ok) return null;
    return await response.json();
  } catch {
    return null;
  }
}
```

4. In `fetchMlb`, when `state === "in"`:

```ts
const scoreFields = buildScoreFields(team, todayGame.competition, null);
const eventId = todayGame.event.id;
const sit = todayGame.competition.situation;
let batterAvg: string | null = null;
let pitcherEra: string | null = null;
if (eventId) {
  const summary = await fetchGameSummary(String(eventId));
  if (summary) {
    const batterId = situationPlayerId(sit?.batter);
    const pitcherId = situationPlayerId(sit?.pitcher);
    if (batterId) {
      batterAvg = boxscoreStat(
        summary as {
          boxscore?: {
            players?: Array<{
              statistics?: Array<{
                type?: string;
                labels?: string[];
                athletes?: Array<{ athlete?: { id?: string }; stats?: string[] }>;
              }>;
            }>;
          };
        },
        batterId,
        "batting",
        "AVG",
      );
    }
    if (pitcherId) {
      pitcherEra = boxscoreStat(
        summary as {
          boxscore?: {
            players?: Array<{
              statistics?: Array<{
                type?: string;
                labels?: string[];
                athletes?: Array<{ athlete?: { id?: string }; stats?: string[] }>;
              }>;
            }>;
          };
        },
        pitcherId,
        "pitching",
        "ERA",
      );
    }
  }
}
const standing = await standingPromise;
return withDisplayFields(
  { ...scoreFields, batterAvg, pitcherEra },
  todayGame.competition,
  team,
  standing,
);
```

Keep `buildScoreFields` setting `batterAvg`/`pitcherEra` to null; overwrite only in this live path. Prefer a named `EspnGameSummary` interface instead of inline casts if cleaner.

- [ ] **Step 4: Run tests — expect PASS**

```bash
npm test -- src/lib/fetchers/mlb.test.ts
```

Expected: PASS (enrichment + soft-fail cases).

- [ ] **Step 5: Commit (backend repo)**

```bash
git add src/lib/fetchers/mlb.ts src/lib/fetchers/mlb.test.ts
git commit -m "$(cat <<'EOF'
feat: enrich live batter AVG and pitcher ERA from ESPN summary

EOF
)"
```

---

### Task 3: Firmware parse + format helper

**Files:**
- Modify: `desktop-display-firmware/lib/desk_display/include/desk_display/scores.hpp`
- Modify: `desktop-display-firmware/lib/desk_display/src/scores.cpp`
- Modify: `desktop-display-firmware/lib/desk_display/include/desk_display/mlb_live_format.hpp`
- Modify: `desktop-display-firmware/lib/desk_display/src/mlb_live_format.cpp`
- Modify: `desktop-display-firmware/test/test_parsers/test_main.cpp`
- Modify: `desktop-display-firmware/test/test_domain/test_main.cpp`
- Modify: `desktop-display-firmware/test/test_screen_sports/test_main.cpp`

**Interfaces:**
- Consumes: JSON fields from Task 1–2
- Produces: parsed `MlbScores` stats; `formatMlbBatterPitcherLine` output per spec

- [ ] **Step 1: Write failing parser + format tests**

In `test_parsers/test_main.cpp` live JSON fixture fragment, add:

```json
"batterAvg":".222","batterSummary":"1-3, BB","pitcherEra":"1.93","pitcherSummary":"0.2 IP, 0 ER"
```

Assert `hasBatterAvg`, string `.222`, and the other three fields.

In `test_domain/test_main.cpp` replace batter/pitcher assertions with:

```cpp
m.hasBatterName = true;
std::strncpy(m.batterName, "A. Judge", sizeof(m.batterName) - 1);
m.hasBatterAvg = true;
std::strncpy(m.batterAvg, ".311", sizeof(m.batterAvg) - 1);
m.hasBatterSummary = true;
std::strncpy(m.batterSummary, "1-3, BB", sizeof(m.batterSummary) - 1);
m.hasPitcherName = true;
std::strncpy(m.pitcherName, "F. Valdez", sizeof(m.pitcherName) - 1);
m.hasPitcherEra = true;
std::strncpy(m.pitcherEra, "2.85", sizeof(m.pitcherEra) - 1);
m.hasPitcherSummary = true;
std::strncpy(m.pitcherSummary, "5.0 IP, 2 ER", sizeof(m.pitcherSummary) - 1);
char names[128];
formatMlbBatterPitcherLine(names, sizeof(names), m);
TEST_ASSERT_EQUAL_STRING(
    "AB: A. Judge .311 · 1-3, BB\nP: F. Valdez 2.85 · 5.0 IP, 2 ER", names);
```

Also cover: name+avg only; name+summary only; pitcher-only with era+summary.

In `test_screen_sports/test_main.cpp`, set the new fields on the live scorebug fixture and expect the assembled multiline string from `view().mlb.batterPitcherLine`.

- [ ] **Step 2: Run tests — expect FAIL**

```bash
/Users/bruceclingan/.platformio/penv/bin/pio test -e native -f test_parsers -f test_domain -f test_screen_sports
```

Expected: FAIL (missing fields / old format string).

- [ ] **Step 3: Implement parse + format**

`scores.hpp` — add sizes and fields:

```cpp
constexpr std::size_t kMaxMlbStat = 8;
constexpr std::size_t kMaxMlbPlayerSummary = 48;

bool hasBatterAvg;
char batterAvg[kMaxMlbStat];
bool hasBatterSummary;
char batterSummary[kMaxMlbPlayerSummary];
bool hasPitcherEra;
char pitcherEra[kMaxMlbStat];
bool hasPitcherSummary;
char pitcherSummary[kMaxMlbPlayerSummary];
```

`mlb_live_format.hpp` — bump:

```cpp
constexpr std::size_t kMlbPitchersLineLen = 128;
```

Update comment to the new example line.

`scores.cpp` — parse the four optional strings like `batterName` (null → `has*=false`).

`mlb_live_format.cpp` — assemble with a small local helper:

```cpp
void appendRoleLine(char* dest, std::size_t destLen, const char* prefix,
                    const char* name, const char* seasonStat,
                    const char* summary) {
  // "AB: Name" + optional " .311" + optional " · 1-3, BB"
}
```

Rules: emit role only if `name`; append season stat with leading space; append summary with ` · `.

Join batter and pitcher with `\n` when both present.

- [ ] **Step 4: Run tests — expect PASS**

```bash
/Users/bruceclingan/.platformio/penv/bin/pio test -e native -f test_parsers -f test_domain -f test_screen_sports
```

Expected: PASS.

- [ ] **Step 5: Build sim**

```bash
/Users/bruceclingan/.platformio/penv/bin/pio run -e sim
```

Expected: SUCCESS (sim already renders `batterPitcherLine`).

- [ ] **Step 6: Note in `docs/SIM.md`**

One sentence under Sports: live AB/P lines include season AVG/ERA and game summaries when the API provides them.

- [ ] **Step 7: Commit (firmware repo)**

```bash
git add lib/desk_display test/test_parsers test/test_domain test/test_screen_sports docs/SIM.md
git commit -m "$(cat <<'EOF'
feat: show live batter AVG and pitcher ERA with game lines

EOF
)"
```

---

### Task 4: Manual verify

**Files:** none (manual)

- [ ] **Step 1: Run backend locally or use deployed API with a live game**

Confirm `GET /api/scores` mlb fragment includes non-null `batterAvg` / `pitcherEra` / summaries when ESPN has them.

- [ ] **Step 2: Run sim Sports while live**

Confirm lines like:

```text
AB: A. Kirk .222 · 1-3, BB
P: A. Chapman 1.93 · 0.2 IP, 0 ER, 0 H, K, BB
```

fit without clipping on the round display. If summary wrap clips the bezel, shorten pitcher summary display in format helper only (keep API full string) — document the tweak in the commit message if needed.

- [ ] **Step 3: Optional follow-up commit only if manual check required a format tweak**

---

## Spec coverage check

| Spec requirement | Task |
|------------------|------|
| Scoreboard game summaries | 1 |
| Summary boxscore AVG/ERA | 2 |
| Soft-fail summary errors | 2 |
| Four new API fields | 1–2 |
| Firmware parse + format assembly | 3 |
| Sim uses existing label | 3 |
| Not-live unchanged | 1 (nulls) |
| Manual LIVE check | 4 |

## Placeholder / consistency check

- Field names match spec: `batterAvg`, `batterSummary`, `pitcherEra`, `pitcherSummary`
- Display separator is middle dot ` · ` per spec
- Buffer bumped to 128 for multiline AB/P output
