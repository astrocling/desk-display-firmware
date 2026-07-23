# Navigation shell

Pure C++ app-shell navigation (no LVGL / no Dial hardware required). Source:
`lib/desk_display/include/desk_display/nav.hpp` + `src/nav.cpp`. Unit tests:
`test/test_nav/` via `pio test -e native`.

## Modes

| Mode | Meaning |
|------|---------|
| **Carousel** | Browse screens; highlight moves with the encoder |
| **Focused** | Inside one screen; rotate is reserved for that screen’s actions |

## Input mapping

The encoder has **no push button**. Shell events:

| Event | Shell behavior |
|-------|----------------|
| `Rotate(delta)` | Carousel: cycle highlight. Focused: idle reset only (screen owns rotate). |
| `CenterTap` | Plan’s “knob click”: Carousel → enter Focused on highlight; Focused → back to Carousel |
| `Tap` / `DoubleTap` / `LongPress` | Reset idle timer (detail actions are screen-owned) |

## Idle / home

- Timeout: `kIdleTimeoutMs` (60s), driven by `on_tick(elapsed_ms)` or cleared with `idle_reset()` / any input.
- **Idle home choice:** **Focused on Clock** (not Carousel highlighting Clock). Matches the firmware plan’s “falls back to the Clock screen (home)” — the user sees the clock face.

Boot / `reset()` also starts in **Focused Clock**.

## Screens

Carousel order: Clock → Timezones → Weather → Sports → Radar → (wrap).

Track C fills LVGL behind the create/destroy/show/hide stubs in
`screens_stub.hpp` / `screens_stub.cpp` and theme tokens in `theme.hpp`.
