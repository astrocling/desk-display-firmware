#!/usr/bin/env python3
"""Convert vendored MLB logo PNGs into LVGL 8 TRUE_COLOR_ALPHA C arrays.

Reads `assets/mlb/png/{ABBR}.png` (see `support/fetch_mlb_logos.py`) and
emits one `.c` file per team under `src/sim/assets/mlb/`, plus a
`mlb_logos_assets.h` header declaring `extern const lv_img_dsc_t` for each.
"""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PNG_DIR = ROOT / "assets" / "mlb" / "png"
OUT_DIR = ROOT / "src" / "sim" / "assets" / "mlb"

# ESPN abbreviations, matching support/fetch_mlb_logos.py and the firmware
# view-model's teamAbbr/opponentAbbr values.
TEAMS = [
    "ARI", "ATL", "BAL", "BOS", "CHC", "CHW", "CIN", "CLE", "COL", "DET",
    "HOU", "KC", "LAA", "LAD", "MIA", "MIL", "MIN", "NYM", "NYY", "OAK",
    "PHI", "PIT", "SD", "SF", "SEA", "STL", "TB", "TEX", "TOR", "WSH",
]


def rgb888_to_rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def png_to_lvgl_c(png: Path, symbol: str, out_c: Path) -> tuple[int, int]:
    from PIL import Image  # type: ignore

    im = Image.open(png).convert("RGBA")
    w, h = im.size
    pixels = list(im.getdata())
    data = bytearray()
    for r, g, b, a in pixels:
        c = rgb888_to_rgb565(r, g, b)
        # LVGL 16-bit TRUE_COLOR_ALPHA: color bytes little-endian then alpha
        data.append(c & 0xFF)
        data.append((c >> 8) & 0xFF)
        data.append(a)

    lines = [
        f"/* Auto-generated from {png.name} — do not edit by hand. */",
        '#include "lvgl.h"',
        "",
        f"#ifndef LV_ATTRIBUTE_MEM_ALIGN",
        f"#define LV_ATTRIBUTE_MEM_ALIGN",
        f"#endif",
        "",
        f"const LV_ATTRIBUTE_MEM_ALIGN uint8_t {symbol}_map[] = {{",
    ]
    for i in range(0, len(data), 24):
        chunk = data[i : i + 24]
        lines.append("  " + ", ".join(f"0x{b:02X}" for b in chunk) + ",")
    lines += [
        "};",
        "",
        f"const lv_img_dsc_t {symbol} = {{",
        "  .header.cf = LV_IMG_CF_TRUE_COLOR_ALPHA,",
        "  .header.always_zero = 0,",
        "  .header.reserved = 0,",
        f"  .header.w = {w},",
        f"  .header.h = {h},",
        f"  .data_size = {len(data)},",
        f"  .data = {symbol}_map,",
        "};",
        "",
    ]
    out_c.write_text("\n".join(lines))
    return w, h


def main() -> int:
    if not PNG_DIR.exists():
        print(f"missing {PNG_DIR} — run support/fetch_mlb_logos.py first", file=sys.stderr)
        return 1
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    decls = [
        "#pragma once",
        "",
        '#include "lvgl.h"',
        "",
        "/* Vendored MLB team logos (ESPN CDN, build-time only). */",
        "/* See assets/mlb/THIRD_PARTY.md for usage terms. */",
        "",
    ]

    for abbr in TEAMS:
        png = PNG_DIR / f"{abbr}.png"
        if not png.exists():
            print(f"missing {png} — run support/fetch_mlb_logos.py first", file=sys.stderr)
            return 1
        symbol = f"mlb_logo_{abbr.lower()}"
        out_c = OUT_DIR / f"{symbol}.c"
        w, h = png_to_lvgl_c(png, symbol, out_c)
        print(f"wrote {out_c.relative_to(ROOT)} ({w}x{h})")
        decls.append(f"extern const lv_img_dsc_t {symbol};")

    decls.append("")
    (OUT_DIR / "mlb_logos_assets.h").write_text("\n".join(decls) + "\n")
    print("wrote mlb_logos_assets.h")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
