#!/usr/bin/env python3
"""Download MLB team logos from the ESPN CDN and resize into assets/mlb/png/.

Build-time only — never fetched on-device. Run this once (or whenever the
team list changes) before `support/gen_mlb_logos.py`.

Usage:
    python3 support/fetch_mlb_logos.py
"""

from __future__ import annotations

import sys
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PNG_DIR = ROOT / "assets" / "mlb" / "png"

# ESPN abbreviations (uppercase, as used by the backend / firmware view-model).
TEAMS = [
    "ARI", "ATL", "BAL", "BOS", "CHC", "CHW", "CIN", "CLE", "COL", "DET",
    "HOU", "KC", "LAA", "LAD", "MIA", "MIL", "MIN", "NYM", "NYY", "OAK",
    "PHI", "PIT", "SD", "SF", "SEA", "STL", "TB", "TEX", "TOR", "WSH",
]

# Known ESPN CDN path casing quirks: abbr -> list of candidate slugs to try,
# in order, before falling back to abbr.lower(). Populated as needed; as of
# writing all 30 teams resolve directly via abbr.lower().
CDN_SLUG_OVERRIDES: dict[str, list[str]] = {}

URL_TEMPLATE = "https://a.espncdn.com/i/teamlogos/mlb/500/{slug}.png"
USER_AGENT = "Mozilla/5.0 (compatible; desk-display-firmware asset build)"
SIZE = 56


def candidate_slugs(abbr: str) -> list[str]:
    slugs = list(CDN_SLUG_OVERRIDES.get(abbr, []))
    lower = abbr.lower()
    if lower not in slugs:
        slugs.append(lower)
    return slugs


def download(abbr: str) -> bytes:
    last_err: Exception | None = None
    for slug in candidate_slugs(abbr):
        url = URL_TEMPLATE.format(slug=slug)
        req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
        try:
            with urllib.request.urlopen(req, timeout=15) as resp:
                print(f"  {abbr}: {url} -> {resp.status}")
                return resp.read()
        except urllib.error.HTTPError as e:
            print(f"  {abbr}: {url} -> {e.code}", file=sys.stderr)
            last_err = e
    assert last_err is not None
    raise last_err


def resize_with_alpha(raw: bytes, size: int) -> bytes:
    import io

    from PIL import Image  # type: ignore

    im = Image.open(io.BytesIO(raw)).convert("RGBA")
    im = im.resize((size, size), Image.LANCZOS)
    out = io.BytesIO()
    im.save(out, format="PNG")
    return out.getvalue()


def main() -> int:
    PNG_DIR.mkdir(parents=True, exist_ok=True)
    print(f"Fetching {len(TEAMS)} MLB logos from ESPN CDN -> {PNG_DIR}")
    for abbr in TEAMS:
        raw = download(abbr)
        resized = resize_with_alpha(raw, SIZE)
        out_png = PNG_DIR / f"{abbr}.png"
        out_png.write_bytes(resized)
        print(f"  wrote {out_png.relative_to(ROOT)} ({SIZE}x{SIZE})")
    print("done")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
