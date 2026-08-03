# Dial input + Nav bring-up (encoder + CST816)

**Date:** 2026-08-03  
**Status:** Approved for planning  
**Hardware:** Waveshare ESP32-S3-Knob-Touch-LCD-1.8 (“Dial”)  
**Env:** PlatformIO `dial`

## Goal

On the Dial, read the rotary encoder (GPIO 8/7) and CST816 touch, emit **`Rotate`** and **`CenterTap`**, drive the existing `desk_display::Nav` shell, and show mode/screen on **Serial** plus a **simple LVGL text overlay**. Keep Wi‑Fi STA/NVS and the solid dark green background from the display bring-up.

## Non-goals

- `Tap` / `DoubleTap` / `LongPress` (and position-based hit testing)
- Center-region gating for CenterTap (any short press counts)
- Full carousel chrome (title, page dots, live preview)
- Porting Clock / Weather / Sports / Radar (or other sim screens)
- LVGL pointer or encoder `indev` drivers
- Interrupt/queue-heavy input (FreeRTOS queues, PCNT-first design)
- Vibration motor, brightness UI, captive portal, HTTP, NTP

## Context (input model — no change)

The rotary ring has **no push button**. Plan “knob click” = **short center tap** on the touch surface (`docs/NAV.md`, `docs/FIRMWARE_PLAN.md`). This pass implements that as: any short press/release on CST816 → `Nav::on_center_tap()`.

Sim already maps Enter/Space → `center_tap` and arrows/`A`/`D` → rotate; dial should match that event subset.

## Decisions

| Topic | Choice |
|--------|--------|
| Done look | Input HAL + wire `Nav`; Serial + LVGL label; no real screens |
| Gestures this pass | `Rotate` + `CenterTap` only |
| CenterTap geometry | Any short press/release anywhere (no center circle) |
| Feedback | Serial on change + on-screen text overlay on green background |
| Input architecture | Dial HAL poll → small event bag → `Nav` (not LVGL indevs) |
| Encoder direction | Positive delta = next carousel screen (same as sim Right / `D`) |
| Idle | Call `Nav::on_tick(elapsed_ms)`; 60s behavior unchanged |

## Architecture

Dial boot order in `src/main.cpp`:

1. Serial begin  
2. Existing `desk_net::wifiSetup()`  
3. Existing display + LVGL init (solid green root screen)  
4. Encoder init (GPIO A=8, B=7) + Touch init (I²C SDA=11, SCL=12, CST816 @ `0x15`)  
5. Construct `desk_display::Nav` (existing `reset()` → Focused Clock)  
6. Create one LVGL label for mode + screen name  

Each `loop` tick:

1. `wifiLoop` + `lvglPortHandler`  
2. Poll encoder → `rotate_delta` since last drain  
3. Poll touch → short press/release → `center_tap`  
4. If events: `nav.on_rotate` / `nav.on_center_tap`  
5. `nav.on_tick(elapsed_ms)` for idle home / settle  
6. On nav state change: Serial log + refresh label text  

| Piece | Responsibility |
|--------|----------------|
| `src/hal/board_pins.hpp` | Add encoder A/B + touch I²C pins (and address if useful) |
| `src/hal/encoder.hpp` / `encoder.cpp` | Quadrature decode → signed tick delta since last poll |
| `src/hal/touch.hpp` / `touch.cpp` | CST816 bring-up; short down→up → `center_tap` |
| Dial-local input event struct | `{ rotate_delta, center_tap }` — sim-shaped subset |
| Dial-local nav status helper | Format Serial + label from `Nav` mode / screen |
| `lib/desk_display` `Nav` | Unchanged API; used as-is |

Shared `lib/desk_display` stays free of Arduino / LVGL / Dial GPIO.

## Input details

### Encoder (GPIO 8 / 7)

- Poll quadrature in `loop`, or use a cheap ISR that only updates a counter with decode in poll.  
- Drain integer ticks each loop; clamp per drain (e.g. ±8) so a fast spin does not jump the whole carousel.  
- If A/B appear inverted on hardware, fix with one invert flag or pin swap — do not change nav’s positive = next mapping.

### Touch (CST816 @ 0x15)

- Init I²C; probe chip.  
- Contact tracking: finger down → up within a short window (e.g. ≤ ~400 ms) → one `center_tap`.  
- Longer holds: ignore (no LongPress this pass).  
- After firing, short refractory (e.g. ~50–100 ms) to avoid double edges.

### Overlay + Serial

- Label examples: `FOCUSED  CLOCK`, `CAROUSEL WEATHER`.  
- Serial on change only, e.g. `nav: Focused Clock` / `nav: Carousel Weather`.  
- Keep solid green background; label high-contrast (light text), centered or near top.

### Idle

- Drive `on_tick` every loop with elapsed ms so Carousel idle still returns to Focused Clock; update label/Serial when idle changes mode.
- In **Focused** with no real screens yet, rotate still calls `on_rotate` (resets idle) but does not change mode/screen or the overlay — same as `Nav` today.

## Pin map (Knob 1.8 — locked for this board)

| Function | GPIO |
|----------|------|
| Encoder A / B | 8 / 7 |
| Touch I²C SDA / SCL | 11 / 12 |
| Touch address | CST816 @ `0x15` |

(LCD pins unchanged from display bring-up.)

## Error handling

| Condition | Behavior |
|-----------|----------|
| Touch I²C / CST816 probe fails | Serial `touch: not found` (or equivalent); rotate-only nav still works |
| Encoder miswired / inverted | Correct via invert/pin swap after manual check; no fatal halt |
| Display / LVGL already failed | Skip overlay; Serial nav logs still OK if `Nav` runs |
| Wi‑Fi failure | Unchanged; independent of input |

## Verification (manual on hardware)

1. Build/flash `env:dial`; Serial shows Wi‑Fi, `display: ready`, `encoder: ready`, and `touch: ready` (or touch not found).  
2. Boot: overlay + Serial show **Focused Clock**.  
3. Short tap anywhere → **Carousel** (Clock highlighted); rotate cycles highlight; Serial + label update.  
4. Short tap → **Focused** on highlighted screen.  
5. Idle ~60s in Carousel → **Focused Clock**.  
6. Wi‑Fi still operates; green background remains behind text.

## Tests

- Existing `pio test -e native` nav tests must remain green.  
- Optional: host unit tests for pure encoder state machine / tap timing if logic is extracted without Arduino — not required for done.

## Follow-ups (explicitly later)

- Position `Tap` (+ DoubleTap / LongPress)  
- On-device carousel chrome and porting Focused Clock first  
- LVGL indevs when interactive widgets need them  

## References

- `docs/NAV.md`, `docs/HANDOFF-dial-bringup.md`  
- `docs/superpowers/specs/2026-08-03-dial-display-hal-design.md`  
- Sim input: `src/sim/sdl_hal.hpp` (`KeyEvents`), `src/sim/sim_app.cpp` (`handle_input`)  
- `lib/desk_display/include/desk_display/nav.hpp`
