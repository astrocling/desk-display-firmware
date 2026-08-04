# Weather UI circle layout

**Date:** 2026-08-04  
**Status:** Approved for planning  
**Scope:** Rethink the weather screen layout for the circular dial so focused mode (and carousel) no longer clip content; drop the multi-slot hourly strip while keeping encoder scrub.

## Problem

Weather uses one fixed LVGL layout for carousel and focused ([`src/ui/weather_lvgl.cpp`](../../../src/ui/weather_lvgl.cpp)). Positions assume a square host. On the 360×360 circular dial, the bottom **280×70** five-slot hourly strip sits against the bezel and clips; H/L and long alert headlines can also run into the curved edge. Carousel preview (240⌀) mounts the same layout and can look acceptable as a small preview, but focused makes the cutoff obvious.

## Goals

- Prioritize a clear **current-conditions** read (icon + temperature) inside the circular safe area.
- Remove the multi-slot hourly strip from the UI.
- Keep hourly **scrub**: rotate still updates center temp/icon and the when-label (`Current` / `6 PM`).
- Use the **same layout** in carousel and focused (no mode-specific weather build).
- Clear H/L and alert from the bezel via circular-safe insets.

## Non-goals

- Radar screen changes
- New weather icons or night variants
- Daily / multi-day forecast
- Alert severity color coding
- Changing scrub, idle settle, or alert open/close input behavior
- Mode-aware weather layouts

## UX / layout (icon + temp row)

Vertical stack, content constrained to a safe box inset ~48–56px from the 360 disc edge (same idea as radar bezel clearance):

1. **Today H / L** — top center, accent, montserrat 12; more top inset than today’s `y=4`
2. **When label** — `Current` or short hour (`6 PM`), dim; this is the primary scrub feedback
3. **Icon + temp row** — ~40px Meteocons icon beside large temperature (montserrat 28, bump toward 36 only if the row still fits the safe width)
4. **Feels like** — always **current** feels from the API (`feels xx°`), dim; not per-hour
5. **Alert** — when present: `ALERT` or headline when detail open; place above the bottom rim; clamp width and allow wrap so long headlines do not clip the circle
6. **No hourly strip** — bottom of the disc stays empty

```
        H 84  L 68
         Current
      [icon]  72°
        feels 74°
          ALERT
```

## Behavior (unchanged)

- `WeatherScreen::onRotate` still walks Now → hour 0 → … → end
- Idle settle / snap returns to Now and closes alert detail
- Alert badge / detail open-close behavior unchanged
- Carousel vs focused still differs only in host size and shell chrome, not weather composition

## View-model & code

| Area | Change |
|------|--------|
| [`src/ui/weather_lvgl.cpp`](../../../src/ui/weather_lvgl.cpp) | Remove strip rendering; recenter/respace remaining widgets; alert width clamp/wrap |
| [`screen_weather.hpp`](../../../lib/desk_display/include/desk_display/screen_weather.hpp) / [`screen_weather.cpp`](../../../lib/desk_display/src/screen_weather.cpp) | Remove `WeatherStripSlot`, `kWeatherStripSlots`, `strip[]` / `stripCount`, and `fillStrip` |
| [`test/test_screen_weather/`](../../../test/test_screen_weather/) | Drop strip-window assertions; keep scrub, `whenLabel`, alert, and snap tests |
| Dial shell / nav | No change required for mounting weather |

Scrub index, `whenLabel`, `displayTemp`, `icon`, feels, H/L, and alert fields remain on `WeatherScreenView`.

## Testing

- Unit: existing scrub / when-label / alert / idle settle tests still pass after strip removal
- Visual: sim carousel (240 preview) and focused (360) — no bezel clipping of H/L, center row, feels, or alert
- Confirm rotate still updates when-label + center conditions without a strip

## Success criteria

- Focused weather content sits inside the circular safe area with no cut-off corners
- Carousel preview shows the same composition without a clipped strip
- Hourly scrub remains usable via rotate + when-label
- Strip API and strip unit tests are gone (no dead view-model surface)
