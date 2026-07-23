#!/usr/bin/env python3
"""Rasterize weather SVGs and emit LVGL 8 TRUE_COLOR_ALPHA C arrays (RGB565 + A)."""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SVG_DIR = ROOT / "assets" / "weather" / "svg"
PNG_DIR = ROOT / "assets" / "weather" / "png"
OUT_DIR = ROOT / "src" / "sim" / "assets" / "weather"

# Logical name → svg stem
ICONS = [
    ("clear", "clear-day"),
    ("mostly_clear", "partly-cloudy-day"),
    ("cloudy", "cloudy"),
    ("fog", "fog"),
    ("drizzle", "drizzle"),
    ("rain", "rain"),
    ("snow", "snow"),
    ("showers", "partly-cloudy-day-rain"),
    ("thunderstorm", "thunderstorms"),
    ("unknown", "not-available"),
]


def rgb888_to_rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def rasterize_with_resvg(svg: Path, png: Path, size: int) -> None:
    tools = ROOT / "support" / "weather_icon_tools"
    script = f"""
const {{ Resvg }} = require('@resvg/resvg-js');
const fs = require('fs');
const svg = fs.readFileSync({str(svg)!r});
const resvg = new Resvg(svg, {{
  fitTo: {{ mode: 'width', value: {size} }},
  background: 'rgba(0,0,0,0)',
}});
const pngData = resvg.render().asPng();
fs.writeFileSync({str(png)!r}, pngData);
"""
    subprocess.run(
        ["node", "-e", script],
        check=True,
        cwd=str(tools),
    )


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
    PNG_DIR.mkdir(parents=True, exist_ok=True)
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    decls = [
        "#pragma once",
        "",
        '#include "lvgl.h"',
        "",
        "/* Meteocons monochrome weather icons (MIT — Bas Milius). */",
        "",
    ]

    size = 40
    for sym_base, stem in ICONS:
        svg = SVG_DIR / f"{stem}.svg"
        if not svg.exists():
            print(f"missing {svg}", file=sys.stderr)
            return 1
        png = PNG_DIR / f"{stem}_{size}.png"
        symbol = f"weather_icon_{sym_base}"
        print(f"rasterize {stem} → {png.name}")
        rasterize_with_resvg(svg, png, size)
        out_c = OUT_DIR / f"{symbol}.c"
        w, h = png_to_lvgl_c(png, symbol, out_c)
        print(f"  wrote {out_c.name} ({w}x{h})")
        decls.append(f"extern const lv_img_dsc_t {symbol};")

    decls.append("")
    (OUT_DIR / "weather_icons_assets.h").write_text("\n".join(decls) + "\n")
    print("wrote weather_icons_assets.h")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
