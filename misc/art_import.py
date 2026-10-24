"""Imports the model sheets of the units into the sprite folder of a pack.

A model sheet (docs/art/GEMINI-PROMPTS.md, LAYOUT-SHEET) shows two views
of one unit on a chroma-key green background: the three-quarter front
view at the left, the three-quarter back view at the right. The uniform,
the banners and the shields are a flat red (MASK-RED). This script does
these steps for each sheet (docs/GRAPHICS.md, section 6):

1. It removes the green background and the green fringe of the edges.
2. It finds the red mask and writes it into the alpha channel: a pixel
   with the alpha 160 takes the colour of the faction in the renderer
   (billboard_ps in code/d3d11_renderer.h). A plain pixel has 255.
3. It cuts the front view, puts its feet on the bottom edge of a square
   and scales the square to SPRITE_SIZE pixels.
4. It writes data/<pack>/sprites/art/<class>.png.

Run it from the root of the repository:

    python -X utf8 misc\\art_import.py [art-src\\units] [data\\placeholder]

The script needs Pillow (pip install pillow). The sheets stay in art-src/,
out of git. The provenance of each sheet is in
data/<pack>/sprites/art/provenance.tsv.
"""

import os
import sys

from PIL import Image, ImageChops

SPRITE_SIZE = 256
MASK_ALPHA = 160        # the alpha of a pixel of the red mask
MARGIN = 6              # pixels of air around the figure before the scale

# The file name of a sheet ends with one of these: the class key of unit_types.tsv.
SHEET_CLASSES = {
    "sword": "sword", "pike": "pike", "halberd": "halberd", "crossbow": "crossbow", "cavalry": "cavalry",
    "ram": "ram", "wood-beast": "wood_beast", "tower": "tower", "catapult": "catapult",
    "transport": "transport", "boat": "boat", "tower-ship": "tower_ship", "battle-ship": "battle_ship",
    "banner": "banner",
}


def class_of(file_name):
    stem = os.path.splitext(file_name)[0].lower()
    for suffix, key in SHEET_CLASSES.items():
        if stem.endswith("-" + suffix) or stem == suffix:
            return key
    return None


def scaled(band, factor):
    """The band times a factor, at most 255."""
    return band.point(lambda v: min(255, int(v * factor)))


def green_alpha(rgb):
    """The alpha of the figure against the green: 255 on the figure, 0 on the
    background, a soft edge between. A pixel is background when its green is
    1.5 times above its red and its blue."""
    red, green, blue = rgb.split()
    other = ImageChops.lighter(red, blue)
    # score = green - 1.5 * other, centred on 128: 128 means a pixel on the line
    score = ImageChops.subtract(green, scaled(other, 1.5), scale=1.0, offset=128)
    # background when the score is above 128 + 8; figure when below 128 - 16
    alpha = score.point(lambda v: 255 if v < 112 else (0 if v > 136 else int(255 * (136 - v) / 24)))
    dark = green.point(lambda v: 0 if v > 90 else 255)   # a very dark pixel is never background
    return ImageChops.lighter(alpha, dark)


def despill(rgb):
    """Pulls the green of the edge pixels down to their red or blue."""
    red, green, blue = rgb.split()
    other = ImageChops.lighter(red, blue)
    green = ImageChops.darker(green, scaled(other, 1.1))
    return Image.merge("RGB", (red, green, blue))


def red_mask(rgb):
    """255 where the pixel is the flat red of the mask (#D01818 and its noise)."""
    red, green, blue = rgb.split()
    other = ImageChops.lighter(green, blue)
    score = ImageChops.subtract(red, other, scale=1.0, offset=0)   # red minus the brighter of the others
    strong = score.point(lambda v: 255 if v > 90 else 0)
    bright = red.point(lambda v: 255 if v > 140 else 0)
    return ImageChops.multiply(strong, bright)


def column_sums(alpha):
    """The count of figure pixels in each column, as a list."""
    width, height = alpha.size
    row = alpha.resize((width, 1), Image.BOX)
    return [v * height // 255 for v in row.tobytes()]


def front_view_box(alpha):
    """The box of the left view: the sheet splits at the emptiest column of
    its middle band."""
    width, height = alpha.size
    sums = column_sums(alpha)
    lo, hi = int(width * 0.30), int(width * 0.70)
    split = min(range(lo, hi), key=lambda x: sums[x])
    left = alpha.crop((0, 0, split, height))
    box = left.getbbox()
    if not box:
        return None
    x0, y0, x1, y1 = box
    return (max(0, x0 - MARGIN), max(0, y0 - MARGIN), min(split, x1 + MARGIN), min(height, y1 + MARGIN))


def import_sheet(path, out_dir):
    key = class_of(os.path.basename(path))
    if not key:
        print("  skip %s: no class in the name" % path)
        return None
    sheet = Image.open(path).convert("RGB")
    alpha = green_alpha(sheet)
    rgb = despill(sheet)
    mask = red_mask(sheet)
    box = front_view_box(alpha)
    if not box:
        print("  skip %s: no figure" % path)
        return None
    rgb, alpha, mask = rgb.crop(box), alpha.crop(box), mask.crop(box)
    # The background takes a colour of the figure, so that the scale makes no green fringe.
    fill = Image.new("RGB", rgb.size, (150, 130, 100))
    rgb = Image.composite(rgb, fill, alpha.point(lambda v: 255 if v > 0 else 0))
    # The alpha carries the mask: 160 on the red mask, 255 on the rest of the figure.
    masked = alpha.point(lambda v: v * MASK_ALPHA // 255)
    alpha = Image.composite(masked, alpha, mask)
    figure = Image.merge("RGBA", rgb.split() + (alpha,))
    # A square with the feet on the bottom edge and the figure in the middle.
    width, height = figure.size
    side = max(width, height)
    square = Image.new("RGBA", (side, side), (150, 130, 100, 0))
    square.paste(figure, ((side - width) // 2, side - height))
    sprite = square.resize((SPRITE_SIZE, SPRITE_SIZE), Image.LANCZOS)
    # The scale blurs the alpha: the edge goes back to the two levels, so that the mask stays exact.
    levels = sprite.split()[3].point(lambda v: 0 if v < 100 else (MASK_ALPHA if v < 208 else 255))
    sprite.putalpha(levels)
    out_path = os.path.join(out_dir, key + ".png")
    sprite.save(out_path, optimize=True)
    print("  %s -> %s (%u x %u of the sheet)" % (os.path.basename(path), out_path, box[2] - box[0], box[3] - box[1]))
    return out_path


def main():
    source = sys.argv[1] if len(sys.argv) > 1 else os.path.join("art-src", "units")
    pack = sys.argv[2] if len(sys.argv) > 2 else os.path.join("data", "placeholder")
    out_dir = os.path.join(pack, "sprites", "art")
    os.makedirs(out_dir, exist_ok=True)
    count = 0
    for name in sorted(os.listdir(source)):
        if not name.lower().endswith((".jpg", ".jpeg", ".png")):
            continue
        if import_sheet(os.path.join(source, name), out_dir):
            count += 1
    print("%u sprites written to %s" % (count, out_dir))
    return 0 if count else 1


if __name__ == "__main__":
    sys.exit(main())
