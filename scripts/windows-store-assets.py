#!/usr/bin/env python3
"""Draws the MSIX logos in packaging/windows/Assets from the app icon.

Run it again after the icon changes and commit the PNGs. Needs cairosvg and Pillow.

  scripts/windows-store-assets.py
"""
import io
import pathlib

import cairosvg
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent.parent
ICON = ROOT / "app/data/icons/hicolor/scalable/apps/com.ktechpit.pixelfx.svg"
OUT = ROOT / "packaging/windows/Assets"


def icon(size):
    png = cairosvg.svg2png(url=str(ICON), output_width=size, output_height=size)
    return Image.open(io.BytesIO(png)).convert("RGBA")


def tile(name, width, height, share):
    """The icon centred on a transparent tile, as tall as SHARE of the tile."""
    canvas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    side = round(min(width, height) * share)
    canvas.alpha_composite(icon(side), ((width - side) // 2, (height - side) // 2))
    canvas.save(OUT / name)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for scale in (100, 200):
        f = scale / 100
        tile(f"StoreLogo.scale-{scale}.png", round(50 * f), round(50 * f), 1.0)
        tile(f"Square44x44Logo.scale-{scale}.png", round(44 * f), round(44 * f), 1.0)
        tile(f"Square150x150Logo.scale-{scale}.png", round(150 * f), round(150 * f), 0.66)
        tile(f"Wide310x150Logo.scale-{scale}.png", round(310 * f), round(150 * f), 0.66)
    # The taskbar, Start and the file list use these, without a plate behind them.
    for size in (16, 24, 32, 48, 256):
        tile(f"Square44x44Logo.targetsize-{size}.png", size, size, 1.0)
        tile(f"Square44x44Logo.targetsize-{size}_altform-unplated.png", size, size, 1.0)
        tile(f"Square44x44Logo.targetsize-{size}_altform-lightunplated.png", size, size, 1.0)


if __name__ == "__main__":
    main()
