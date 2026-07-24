> **Live contract note:** The authoritative backend plan and API shapes live in the sibling repo at `/Users/bruceclingan/Projects/desk-display-backend/docs/BACKEND_PLAN.md`. **Live Vercel responses win** over any suggested shapes in this local copy when they differ.

---

# Desk Display — Backend Implementation Plan

This repo is the data aggregation backend only. A separate firmware repo
(`desk-display-firmware`) is the ESP32-S3 device that consumes this backend's
endpoints — see **API Contract** at the bottom of this doc for what the device
expects. If this repo changes response shapes, update that section so the firmware
repo can be kept in sync.

---

## Stack

- **Compute:** Next.js API routes on Vercel (existing Vercel Pro account)
- **Scheduling:** Vercel Cron for all periodic data fetches — chosen over Trigger.dev
  because required frequencies (15–30 min for weather/alerts, once/day for
  sunrise-sunset, 1–2 min only during live game windows for scores) are well within
  Vercel Cron's included capability
- **Storage:** Upstash Redis via the **Vercel Marketplace** integration. Note: native
  "Vercel KV" was deprecated/sunset — Upstash Redis through Marketplace is the
  current replacement (same idea, unified billing, `@vercel/kv`-compatible-ish API).
  Store one small JSON blob per data domain (weather, alerts, scores,
  sunrise-sunset per city, airport lookup table).
- **Cost expectation:** this workload is a rounding error against the existing $20
  Vercel Pro credit and comfortably inside Upstash's free tier (10,000 commands/day
  via Marketplace). No new paid infrastructure should be needed.

## Explicit Non-Goals

- **Do not use Railway** — nothing here needs an always-on process.
- **Do not use Trigger.dev** — Vercel Cron covers every scheduling need at the
  frequencies this project requires.
- **Do not use Neon/Postgres** — there's no relational data; small JSON blobs in
  Upstash Redis are sufficient for everything here.
- **ADS-B is not this backend's job** — the firmware device polls adsb.lol directly
  due to its ~10s refresh need. This backend does not proxy or cache ADS-B data.

---

## Data Domains & Cron Jobs

### Weather + Alerts
- **Weather source:** Open-Meteo (free, no API key) — current conditions +
  hourly/daily forecast in one call
- **Alerts source:** National Weather Service API, `api.weather.gov` (free, no key)
  — active alerts by lat/long, severity + headline
- **Cron cadence:** every 15–30 min, single location (home/Eastern) for v1
- Write both into a single `weather` blob in Redis

### Timezone Sunrise/Sunset
- **Source:** `sunrise-sunset.org` API (free, no key)
- **Cron cadence:** once per day per city
- **Cities (fixed list for v1):** Eastern (home), Chicago, Las Vegas, GMT, Rome
  (Italy), Kyiv (Ukraine), Chișinău (Moldova)
- Write `sunrise`/`sunset` timestamps per city into a `timezones` blob — the device
  uses these to compute its own 3-state working-hours indicator, no solar math
  needed server-side beyond the API call itself

### Sports Scores
- **MLB team:** ESPN's public scoreboard API (free, no key) — live score/inning or
  next game date/time
- **Flagstand/SSR Hub:** first-party — query this project's own API/database
  directly for latest league (iRacing) result + next scheduled race
- **Cron cadence:** 15–30 min normally; consider a tighter 1–2 min cadence only
  during known live-game windows if feasible, otherwise standard cadence is fine for
  v1
- Write into a `scores` blob

### Airport Lookup
- **Source:** OurAirports' open, free CSV dataset (no key, no scraping) — maps ICAO
  codes to lat/long
- **Not a cron job** — this is closer to a static dataset. Recommend importing it
  once into Redis (or another simple lookup store) as part of setup/deploy, with a
  simple API route: given an ICAO code, return lat/long
- Consider filtering to relevant airports (US + wherever the device's coverage
  extends) rather than the full worldwide dataset if size becomes a concern, though
  at this scale it's unlikely to matter

---

## Future Features (backend-relevant, not in v1)

- **Dirt track motorsport scores** (World of Outlaws, High Limit Sprint Cars, USAC
  Sprint/Midget/Silver Crown) — no clean public API exists for any of these; would
  require scraping. MyRacePass looks like the best first target since it appears to
  standardize results across series in one format. Build as an isolated module (own
  cron job, own Redis key) so a broken scraper doesn't affect other data domains.
  Once available, add to the `scores` blob and the firmware repo can add a slot for
  it.
- **Precipitation radar map** — RainViewer API for radar tiles. This backend would
  do the image processing: fetch tile, crop to area of interest, downsample to
  ~200×200px, convert to a raw pixel format the device can draw directly (RGB565 or
  small indexed palette), store as a small binary blob (Redis or Vercel Blob).
  Must stay precomputed/static-tile style — no live GIS/rasterize in the request path.
- **Map context national airspace refresh** — expand committed `data/map/airspace-rings.json`
  beyond the Dayton-area sample using FAA NASR-derived Class B/C/D geometry
  (`npm run build:map-context` + larger fixture). Towered airports already come from
  OurAirports TWR join.
- **Multi-location weather** — additional cron fetches per location if ever wanted
  beyond the single home location.
- **Editable timezone/contact list** — a small web or phone config interface to
  change the fixed city list without a firmware redeploy.

---

## API Contract (what the firmware device expects from this backend)

Endpoints should be simple JSON GETs, fast enough for a battery/USB-powered embedded
client to poll casually. Suggested shape (adjust field names as implemented, then
update the firmware repo's copy of this section to match):

**`GET /api/weather`**
```json
{
  "current": { "temp": 72, "feelsLike": 70, "code": 3 },
  "todayHigh": 78,
  "todayLow": 61,
  "hourly": [ { "time": "2026-07-23T18:00:00Z", "temp": 74, "code": 2 }, ... ],
  "alert": { "severity": "moderate", "headline": "..." } // omitted/null if none
}
```

**`GET /api/timezones`**
```json
{
  "America/New_York": { "sunrise": "...", "sunset": "..." },
  "America/Chicago": { "sunrise": "...", "sunset": "..." },
  "America/Los_Angeles": { "sunrise": "...", "sunset": "..." },
  "Etc/GMT": { "sunrise": "...", "sunset": "..." },
  "Europe/Rome": { "sunrise": "...", "sunset": "..." },
  "Europe/Kyiv": { "sunrise": "...", "sunset": "..." },
  "Europe/Chisinau": { "sunrise": "...", "sunset": "..." }
}
```

**`GET /api/scores`**
```json
{
  "mlb": { "live": true, "score": "4-2", "inning": "Top 7", "nextGame": null },
  "flagstand": { "lastResult": { "...": "..." }, "nextRace": { "...": "..." } }
}
```

**`GET /api/airport?code=KXXX`**
```json
{ "lat": 0.0, "lon": 0.0 }
```

**`GET /api/map/context?lat=&lon=&radiusMi=`**
```json
{
  "airports": [
    { "icao": "KDAY", "name": "James M Cox Dayton Intl", "lat": 39.9024, "lon": -84.2194 }
  ],
  "rings": [
    { "class": "D", "id": "KDAY_D", "points": [[39.92, -84.25], [39.93, -84.20]] }
  ]
}
```

Cost rules: served from committed `data/map/*.json` and/or Redis after
`/api/cron/seed-map-context`; long `Cache-Control`; no per-request GIS.

adsb.lol is called directly by the device and is out of scope for this backend.
