# Sim Scores Live Poll Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Poll `GET /api/scores` in the sim while Sports is active so live MLB data replaces the boot fixture.

**Architecture:** Add `ScoresPoller` mirroring `AdsbPoller` (interval + async HTTP GET + parse + take). Dedicated `simScoresHttpGet` slot so Radar and scores never share in-flight state. Wire in `SimApp::update`.

**Tech Stack:** C++17, Unity native tests, libcurl async slots, ArduinoJson `parseScores`

## Global Constraints

- Poll only while Sports is the active screen (~30s interval)
- Keep last-good on failure (fixture or previous live)
- Do not share HTTP async slots with ADS-B or map-context
- URL uses `API_BASE_URL` when defined, else production default

---

### Task 1: ScoresPoller + tests

**Files:**
- Create: `lib/desk_display/include/desk_display/scores_poll.hpp`
- Create: `lib/desk_display/src/scores_poll.cpp`
- Modify: `test/test_domain/test_main.cpp`

- [x] Add failing tests: inactive does nothing; after interval + successful GET, `takeScores` once; URL builder
- [x] Implement poller (`kScoresPollIntervalMs = 30000`, reuse `kAdsbFetchMaxWaitMs` for in-flight budget)
- [x] Run `pio test -e native -f test_domain`

### Task 2: Sim HTTP slot + SimApp wire + docs

**Files:**
- Modify: `src/sim/sim_http.hpp`, `src/sim/sim_http.cpp`
- Modify: `src/sim/sim_app.hpp`, `src/sim/sim_app.cpp`
- Modify: `docs/SIM.md`

- [x] Add `simScoresHttpGet` + dedicated `AsyncSlot`
- [x] Wire poller: active when Sports, bind + refresh on `takeScores`
- [x] Document scores poll + Radar fixture-until-first-live note in SIM.md
- [x] Build sim (`pio run -e sim`) to confirm link
