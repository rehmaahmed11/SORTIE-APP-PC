#!/usr/bin/env python3
"""Generates the raster brand assets that Qt and the installers need.

Everything is drawn from scratch so the repo does not depend on a design tool.
Run from the repository root:  python3 tools/make_assets.py
Outputs:
  assets/icons/app.png            256x256 window / About icon
  assets/icons/app.ico            16..256 multi-size Windows icon
  assets/installer/banner.bmp     493x58  MSI banner
  assets/installer/dialog.bmp     493x312 MSI dialog art
  assets/installer/nsis_header.bmp 150x57 NSIS header
"""
from __future__ import annotations

import math
import os

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT_BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
FONT_REG = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"

INK = (8, 12, 15)
PANEL = (18, 25, 30)
MINT = (0, 229, 176)
MINT_DEEP = (0, 168, 130)
TEXT = (232, 237, 242)
DIM = (147, 163, 177)
PETALS = [
    (0, 229, 176), (255, 176, 32), (79, 168, 255), (255, 122, 198),
    (167, 139, 250), (91, 227, 125), (255, 138, 92), (56, 217, 217),
]


def font(path: str, size: int) -> ImageFont.FreeTypeFont:
    try:
        return ImageFont.truetype(path, size)
    except OSError:
        return ImageFont.load_default()


def radial(size: int, inner, outer, center=(0.5, 0.32), radius=0.78) -> Image.Image:
    img = Image.new("RGB", (size, size), outer)
    px = img.load()
    cx, cy = center[0] * size, center[1] * size
    max_r = radius * size
    for y in range(size):
        for x in range(size):
            d = math.hypot(x - cx, y - cy) / max_r
            d = min(1.0, d)
            px[x, y] = tuple(int(inner[i] + (outer[i] - inner[i]) * d) for i in range(3))
    return img


def draw_orb(img: Image.Image, box: tuple[int, int, int, int]) -> None:
    """Draws the dial artwork (rings + petal dots + AMS) into a square box."""
    x0, y0, x1, y1 = box
    size = x1 - x0
    cx, cy = x0 + size / 2, y0 + size / 2
    r = size / 2

    orb = radial(size, (36, 51, 61), INK, center=(0.5, 0.30), radius=0.80)
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, size - 1, size - 1), fill=255)
    img.paste(orb, (x0, y0), mask)

    draw = ImageDraw.Draw(img)
    lw = max(1, int(size * 0.026))
    draw.ellipse((cx - r * 0.92, cy - r * 0.92, cx + r * 0.92, cy + r * 0.92),
                 outline=MINT_DEEP, width=lw)
    draw.ellipse((cx - r * 0.80, cy - r * 0.80, cx + r * 0.80, cy + r * 0.80),
                 outline=(30, 52, 58), width=max(1, lw // 2))

    dot = max(1.5, r * 0.070)
    for i, color in enumerate(PETALS):
        angle = math.radians(-90 + i * 45)
        px = cx + r * 0.78 * math.cos(angle)
        py = cy + r * 0.78 * math.sin(angle)
        draw.ellipse((px - dot, py - dot, px + dot, py + dot), fill=color)

    if size >= 120:
        ams = font(FONT_BOLD, int(size * 0.255))
        draw.text((cx, cy - size * 0.025), "AMS", font=ams, fill=TEXT, anchor="mm")
        sub = font(FONT_BOLD, max(5, int(size * 0.080)))
        draw.text((cx, cy + size * 0.155), "SORTIE", font=sub, fill=MINT, anchor="mm")
        bar = max(1, int(size * 0.012))
        draw.line((cx - size * 0.10, cy + size * 0.085, cx + size * 0.10, cy + size * 0.085),
                  fill=(0, 120, 96), width=bar)


def make_icon_master() -> Image.Image:
    size = 1024
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw_orb(img, (0, 0, size, size))
    return img


def build_icon(master: Image.Image) -> None:
    out_png = os.path.join(ROOT, "assets", "icons", "app.png")
    master.resize((256, 256), Image.LANCZOS).save(out_png)

    sizes = [16, 24, 32, 48, 64, 128, 256]
    frames = [master.resize((s, s), Image.LANCZOS) for s in sizes]
    out_ico = os.path.join(ROOT, "assets", "icons", "app.ico")
    frames[-1].save(out_ico, format="ICO", sizes=[(s, s) for s in sizes])
    # Pillow only writes the sizes it is given; rebuild explicitly to be safe.
    frames[-1].save(out_ico, format="ICO", append_images=frames[:-1],
                    sizes=[(s, s) for s in sizes])


def gradient_bg(width: int, height: int, top=PANEL, bottom=INK) -> Image.Image:
    img = Image.new("RGB", (width, height), bottom)
    draw = ImageDraw.Draw(img)
    for y in range(height):
        t = y / max(1, height - 1)
        draw.line((0, y, width, y),
                  fill=tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)))
    return img


def make_banner(master: Image.Image) -> None:
    width, height = 493, 58
    img = gradient_bg(width, height)
    draw = ImageDraw.Draw(img)
    draw.rectangle((0, height - 3, width, height), fill=MINT)

    orb = master.resize((42, 42), Image.LANCZOS)
    img.paste(orb, (12, 8), orb)

    draw.text((66, 12), "AMS SORTIE", font=font(FONT_BOLD, 20), fill=TEXT)
    draw.text((67, 35), "Drop it. Sorted.", font=font(FONT_REG, 11), fill=DIM)

    draw.text((width - 14, 14), "+92 306 0706989", font=font(FONT_BOLD, 13), fill=MINT,
              anchor="ra")
    draw.text((width - 14, 32), "Rehman Ahmed", font=font(FONT_REG, 11), fill=DIM, anchor="ra")

    os.makedirs(os.path.join(ROOT, "assets", "installer"), exist_ok=True)
    img.save(os.path.join(ROOT, "assets", "installer", "banner.bmp"), format="BMP")

    # NSIS header art (150x57 max)
    header = img.crop((0, 0, 150, height)).resize((150, 57), Image.LANCZOS)
    header.save(os.path.join(ROOT, "assets", "installer", "nsis_header.bmp"), format="BMP")


def make_dialog(master: Image.Image) -> None:
    width, height = 493, 312
    img = gradient_bg(width, height, top=(24, 33, 39), bottom=(6, 9, 11))
    draw = ImageDraw.Draw(img)

    # decorative rings
    for i, alpha in enumerate((26, 18, 12)):
        r = 150 + i * 34
        draw.ellipse((width / 2 - r, height / 2 - r, width / 2 + r, height / 2 + r),
                     outline=(0, 90, 76), width=1)

    orb = master.resize((132, 132), Image.LANCZOS)
    img.paste(orb, (int(width / 2 - 66), 28), orb)

    draw.text((width / 2, 176), "AMS SORTIE", font=font(FONT_BOLD, 26), fill=TEXT, anchor="mm")
    draw.text((width / 2, 202), "Drop it. Sorted.", font=font(FONT_REG, 13), fill=DIM, anchor="mm")

    draw.line((60, 222, width - 60, 222), fill=(40, 52, 61), width=1)

    draw.text((width / 2, 242), "Rehman Ahmed", font=font(FONT_BOLD, 15), fill=MINT, anchor="mm")
    draw.text((width / 2, 262), "+92 306 0706989   -   WhatsApp available",
              font=font(FONT_REG, 12), fill=DIM, anchor="mm")
    draw.text((width / 2, 288), "AMS", font=font(FONT_BOLD, 11), fill=(70, 88, 98), anchor="mm")

    img.save(os.path.join(ROOT, "assets", "installer", "dialog.bmp"), format="BMP")


def main() -> None:
    master = make_icon_master()
    build_icon(master)
    make_banner(master)
    make_dialog(master)
    print("assets written")


if __name__ == "__main__":
    main()
