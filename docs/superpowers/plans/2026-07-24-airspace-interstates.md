# Real airspace shelves + interstate highways

**Date:** 2026-07-24  
**Spec:** [2026-07-24-radar-map-overlays-design.md](../specs/2026-07-24-radar-map-overlays-design.md)

## Goal

Replace fixture airspace with real FAA Class B/C/D **shelves** (every lateral footprint) and add dim **US interstate** polylines in the same `/api/map/context` response. Hydro stays out.

## Backend (`desk-display-backend`)

1. Ingest `@squawk/airspace-data` (NASR-derived); emit every Polygon / MultiPolygon exterior as its own ring with ids like `DAY_C_0`.
2. Download National Transportation Atlas interstates; normalize `I10` → `I-75`; write `data/map/highways.json`.
3. Extend `filterMapContext` + Redis seed + route to return `highways`.
4. Rebuild with `npm run build:map-context`.

## Firmware (`desktop-display-firmware`)

1. Raise parse rings to 24, view rings to 16, LVGL airspace segs to 1280.
2. Parse `highways[]`; project into `RadarHighwayView`; draw under airspace (dim gray, no hit targets).
3. Update Dayton fixture + native tests.

## Verify

- Backend: `npm test -- src/lib/fetchers/map_context.test.ts`
- Firmware: `pio test -e native`, `pio run -e sim`
- Sim around Dayton: multiple Class C/D shelves, faint I-70/I-75
