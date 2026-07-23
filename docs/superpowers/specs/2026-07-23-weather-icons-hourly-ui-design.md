# Weather icons + hourly strip UI

**Date:** 2026-07-23  
**Status:** Approved for planning  
**Scope:** Phase 2 weather screen — center conditions with graphic icons, Current/time label, and hourly forecast strip (sim first; shared view-model for dial later)

## Goals

- Replace text-only condition names with recognizable weather-app icons.
- Show **Current** for live conditions and a short 12-hour time (e.g. `6 PM`) when scrubbing hourly forecast.
- Draw the thin hourly strip from the Phase 2 plan so rotate scrub has a visible target.
- Keep domain logic LVGL-free and unit-tested; sim is the first consumer.

## Non-goals (this pass)

- Live HTTP polling / backend refresh on device
- Night vs day icon variants (day-only assets)
- 5-day / daily forecast expansion
- Alert severity color coding (keep existing single alert style)
- Device dial LVGL layout beyond what shared assets/view-model already enable

## UX / layout

**Center (layout B):**

1. Small when-label: `Current` or `6 PM`
2. Row: condition icon (~40px) beside large temperature
3. Feels-like under the row (always **current** feels-like from API — no per-hour feels)
4. Today H / L near the top (existing)

**Hourly strip (bottom):**

- ~5 visible slots in a window centered on the scrub selection (when on Now, center the window on the start of the hourly series / first upcoming hours; clamp at ends)
- Each slot: mini icon (~14–16px), hour indicator, temp
- Selected slot visually highlighted; center display always matches scrub (or live current when `showingNow`)
- Knob rotate / double-tap snap behavior unchanged (`WeatherScreen` already implements this)
- Alert badge remains when present; place so it does not collide with the strip (above strip or overlay per sim spacing)

## Icons

- **Source:** Bas Milius [Meteocons](https://github.com/basmilius/meteocons), **monochrome**, static SVG
- **License:** MIT — preserve copyright notices in vendored files / `THIRD_PARTY` note
- **Pipeline:** Vendor chosen SVGs → rasterize to LVGL-compatible image descriptors (compiled C arrays or checked-in PNGs converted for LVGL 8.4). No runtime SVG on device/sim.
- **Day-only mapping** from existing `WeatherIconId`:

| WeatherIconId | Meteocons name (approx.) |
|---------------|--------------------------|
| Clear | `clear-day` |
| MostlyClear | `partly-cloudy-day` |
| Cloudy | `cloudy` |
| Fog | `fog` |
| Drizzle | `drizzle` |
| Rain | `rain` |
| Snow | `snow` |
| Showers | `showers` (fallback `rain` if needed) |
| Thunderstorm | `thunderstorms` |
| Unknown | `not-available` (or closest equivalent) |

- Helper such as `weatherIconImg(WeatherIconId)` returns the LVGL descriptor for center + strip.

## View-model & formatting

Extend `WeatherScreen` / `WeatherScreenView` (no LVGL):

- **`whenLabel`:** `"Current"` if `showingNow`; else format hourly `time` (`YYYY-MM-DDTHH:MM` or with `Z`) to 12-hour short form without leading zero (`6 PM`, `12 AM`, `12 PM`).
- **Strip window:** expose either `(startIndex, count)` plus selection, or a small fixed array of slot snapshots (`hourLabel` / digit, `temp`, `icon`, `selected`).
- Feels-like, H/L, alert fields unchanged.
- Time formatting and strip window math live in `desk_display` so native tests cover them.

## Sim UI

Update `src/sim/sim_app.cpp` weather branch to:

- Render layout B using view fields + icon helper
- Draw hourly strip from strip window
- Stop showing condition name as text in the feels line (icon replaces it)

## Testing

- Unit tests for time formatting edge cases (`00:00` → `12 AM`, `12:00` → `12 PM`, `18:00` → `6 PM`)
- Strip window centering and end clamping
- Existing scrub / snap / alert tests remain green
- Manual: sim focused Weather — Current, rotate through hours, strip highlight tracks center

## Architecture sketch

```
fixtures/weather.json
        │
        ▼
  parseWeather() ──► WeatherScreen.bind()
                          │
                          ▼
                   WeatherScreenView
                   (temp, icon, whenLabel,
                    strip slots, feels, H/L, alert)
                          │
          ┌───────────────┼───────────────┐
          ▼               ▼               ▼
   wmoToIcon()    formatWhenLabel()   stripWindow()
          │
          ▼
   Meteocons LVGL imgs
          │
          ▼
   sim_app (LVGL)  →  future dial screen
```

## Open decisions resolved

- Icons: open pack (Meteocons monochrome), not hand-drawn LVGL shapes
- Layout: B (label above; icon beside temp)
- Time: 12-hour short (`6 PM`); live label `Current`
- Day icons only for v1
- Scope: full Phase 2 center + strip (not center-only)
