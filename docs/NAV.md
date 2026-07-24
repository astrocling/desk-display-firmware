# Navigation shell

Pure C++ app-shell navigation (no LVGL / no Dial hardware required). Source:
`lib/desk_display/include/desk_display/nav.hpp` + `src/nav.cpp`. Unit tests:
`test/test_nav/` via `pio test -e native`.

## Modes

| Mode | Meaning |
|------|---------|
| **Carousel** | Browse screens; shell shows **title + page dots + live inset preview** of the highlighted screen (preview is non-interactive) |
| **Focused** | Inside one screen; **full-bleed** content with **no shell chrome**; rotate/tap are screen-owned |

Boot / `reset()` starts in **Focused Clock**.

## Input mapping

The encoder has **no push button**. Shell events:

| Event | Shell behavior |
|-------|----------------|
| `Rotate(delta)` | Carousel: cycle highlight. Focused: idle reset only (screen owns rotate). |
| `CenterTap` | Plan’s “knob click” (unchanged): Carousel → enter Focused on highlight; Focused → back to Carousel |
| `Tap` / `DoubleTap` / `LongPress` | Reset idle timer (detail actions are screen-owned) |

## Idle / home

- Timeout: `kIdleTimeoutMs` (60s), driven by `on_tick(elapsed_ms)` (returns `IdleEvent`) or cleared with `idle_reset()` / any input.
- Focused Clock is already home — idle does not re-fire there.

| Mode at timeout | Nav outcome |
|-----------------|-------------|
| **Carousel** | `IdleEvent::HomeToClock` → **Focused Clock** (home) |
| **Focused** | `IdleEvent::SettleFocused` → **stay** on current screen; shell calls each screen’s `onIdleSettle()` to clear ephemeral UI (scrub, selection, detail overlays) while keeping intentional settings (e.g. radar range) |

## Carousel shell (sim / LVGL)

In Carousel only, the shell owns a browse frame around a circular **preview host**:

| Element | Content |
|---------|---------|
| Title (top) | Highlighted screen name (`CLOCK`, `WEATHER`, `RADAR`, …) |
| Dots (bottom) | One per screen in carousel order; filled dot = highlight |
| Preview (inset) | Live render of the highlighted screen; **non-interactive** (rotate cycles highlight; center-tap focuses) |

Focused mode hides the carousel frame and mounts the same screen renderers full-bleed on `focused_host_` (360×360).

## Screens

Carousel order: Clock → Timezones → Weather → Sports → Radar → (wrap).

Track C fills LVGL behind the create/destroy/show/hide stubs in
`screens_stub.hpp` / `screens_stub.cpp` and theme tokens in `theme.hpp`.
