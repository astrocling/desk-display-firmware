# Full-screen shell UX — Carousel component + Focused immersion

**Date:** 2026-07-24  
**Status:** Approved for planning  
**Scope:** Shell-level UX for all screens on the 360 round display — a real Carousel browse component, true full-bleed Focused mode, and idle rules that keep Focused apps sticky. Applies to sim first; same nav + layout contracts for Dial when Track C wires screens.

## Goals

- Make **Carousel feel like a browse mode**, not the same full screen with different inputs.
- Make **Focused feel immersive** — edge-to-edge screen content with no persistent shell mode/title chrome.
- Fix **idle**: Focused stays on the current app; only Carousel idle homes to Focused Clock.
- Keep **center-tap** as the Carousel ↔ Focused switch (no new exit gesture).
- Leave **radar tap-for-details** alone until real hardware; range stays on rotate.

## Non-goals (this pass)

- Redesigning radar selection / detail-tap UX
- DeskRad-style center-tap = range step
- Long-press / double-center exit from Focused
- Per-screen visual redesign beyond fitting overlays on a full-bleed round host
- New product chrome (Wi-Fi, global clock strip, etc.)

## Product decisions (locked)

| Topic | Choice |
|-------|--------|
| Problem | Chrome collision + mode discoverability across **all** screens |
| Carousel | Full shell component: **title + page dots** around a **live inset preview** |
| Focused chrome | **None** — screen-owned HUD overlays only |
| Center-tap | Unchanged: Carousel → enter Focused; Focused → back to Carousel |
| Carousel idle | → **Focused Clock** (existing home) |
| Focused idle | **Stay on app**; settle ephemeral UI; keep intentional settings |
| Radar primary zoom | **Rotate** = range (no change) |
| Radar select | Unchanged this pass |

## Modes

```
Carousel                          Focused
┌─────────────────────┐           ┌─────────────────────┐
│       RADAR         │           │  25 mi · 12         │  ← screen HUD
│   ┌───────────┐     │           │                     │
│   │  live     │     │           │   full-bleed        │
│   │  preview  │     │           │   screen content    │
│   └───────────┘     │           │                     │
│     ● ● ● ● ◉       │           │                     │
└─────────────────────┘           └─────────────────────┘
 title + dots frame                no shell chrome
```

| Mode | Meaning |
|------|---------|
| **Carousel** | Browse screens; shell owns chrome; preview is non-interactive |
| **Focused** | Inside one screen; full-bleed; screen owns rotate/tap |

Boot remains **Focused Clock**. Encoder still has **no push button**; “knob click” = center tap on the touch surface.

## Carousel component

Shell-owned LVGL (or equivalent) layer, visible only in Carousel:

| Element | Placement | Content |
|---------|-----------|---------|
| Title | Top mid | Current highlighted screen name (`CLOCK`, `WEATHER`, `RADAR`, …) |
| Dots | Bottom mid | One per screen in carousel order; highlight = current |
| Preview host | Circular inset between title and dots | Live (or near-live) render of highlighted screen |

### Object tree

```
root_ (360×360 round, clipped)
├── carousel_                 [Carousel only]
│   ├── title_
│   ├── dots_
│   └── preview_host_         circular inset
│       └── screen preview
└── focused_host_             [Focused only]
    └── screen full-bleed
```

### Rules

- Remove the persistent sim debug label (`Carousel · Radar` / `Focused · Clock`) — Carousel frame replaces mode chrome; Focused has none.
- Preview mounts the **same screen renderers** used in Focused, laid out relative to `preview_host_` (not assuming shell margins).
- While Carousel is active: **no in-app gestures** on the preview (taps do not select blips / scrub weather). Rotate only cycles highlight; center-tap focuses.
- Switching highlight rebuilds or swaps the preview for the new screen (same data binding as today’s active-screen refresh).

## Focused full-bleed

- `focused_host_` is **360×360** (full root); no reserved inset for shell chrome.
- Screens draw **overlays on content**, safe for the round clip (prefer top/bottom mid; avoid corners).
- Examples (existing patterns, adapted to full-bleed):
  - **Clock** — face + date overlay
  - **Weather** — H/L top; hourly strip bottom
  - **Sports** — title / detail overlays as today
  - **Radar** — near edge-to-edge disc; compact range/count HUD; selection tag/card overlays (placement may stay top to avoid round clipping)
- Screen code must not depend on a global mode label or fixed content margins from the old sim shell.

## Idle behavior

Timeout remains `kIdleTimeoutMs` (60s), cleared by any input / `idle_reset()`.

| Mode at timeout | Shell action |
|-----------------|--------------|
| **Carousel** | Enter **Focused Clock** (home) |
| **Focused** | **Remain** on current focused screen; call screen settle |

### Focused settle (display-only)

Clear ephemeral UI; **keep** intentional settings:

| Clear / snap | Keep |
|--------------|------|
| Weather scrub → now | — |
| Radar selection / detail overlays | Radar range (zoom) |
| Sports expanded detail (if any) | Radar pinned / temp center policy as today for pin; do not wipe pin on settle |
| Other transient selection chrome | Screen-specific intentional config |

Settle is a **screen API** (e.g. `onIdleSettle()`), invoked by Nav/shell only when Focused times out — not when Carousel homes to Clock.

## Input map

| Event | Carousel | Focused |
|-------|----------|---------|
| Rotate | Cycle highlight + preview | Screen-owned (`onRotate`) |
| Center-tap | Focus highlighted screen | Back to Carousel; Radar still `revertTempCenter()` on exit |
| Tap / Double-tap / Long-press | Idle reset only | Screen-owned |
| Idle timeout | → Focused Clock | Stay + settle |

## Architecture

### Nav (`lib/desk_display` nav)

- Keep `NavMode::{Carousel, Focused}` and center-tap toggle.
- Change idle: if `Focused`, do **not** force Clock; notify shell/screens to settle. If `Carousel`, existing home → Focused Clock.
- Expose enough state for the shell to show/hide `carousel_` vs `focused_host_`.

### Sim shell (`src/sim/sim_app.*`)

- Replace single shared `content_` + top chrome label with `carousel_` / `focused_host_` hosts.
- Bind preview vs full-bleed from `nav_.mode()` and `active_screen()` / highlight.
- Route input as today (Carousel vs Focused ownership), with preview non-interactive.

### Screen LVGL

- Renderers accept a parent and fill it (inset or full-bleed).
- Add settle hooks on view-models where ephemeral state exists (weather, radar, sports as needed).
- Radar disc sizing targets full Focused host; Carousel preview uses the same builder at smaller parent size (relative layout / scale as needed).

### Docs

- Update `docs/NAV.md` (idle rules, Carousel component).
- Update `docs/SIM.md` (chrome removal, Carousel frame).
- Note in `docs/FIRMWARE_PLAN.md` Global Interaction Model: Focused idle stays; Carousel idle → Clock.

## Testing

- **Nav unit tests:** Carousel idle → Focused Clock; Focused idle stays on Weather/Radar/etc.; settle callback or flag as designed.
- **Screen unit tests:** settle clears selection/scrub, preserves range / pin where specified.
- **Sim manual:** browse Carousel (title/dots/preview); focus each screen full-bleed; center-tap back; confirm Focused idle does not jump to Clock; Carousel idle does.

## Success criteria

1. In Carousel, user always sees title + dots + inset preview — unmistakably browsing.
2. In Focused, no shell mode/title label; screen uses the round face edge-to-edge.
3. Center-tap still switches modes both ways.
4. Leaving a Focused app idle no longer returns to Clock; Carousel idle still does.
5. Radar zoom and tap-select behavior unchanged aside from layout host size.
