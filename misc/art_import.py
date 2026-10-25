"""Imports the model sheets of the units and the concept sheets of the bases
into the sprite folder of a pack.

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

A concept sheet of a base (docs/art/GEMINI-PROMPTS.md, section 5) shows
three views of one base. The three-quarter top-down view is at the left.
The front elevation is in the middle, and the plan is at the right. The
name of the file starts with "base-". The script cuts the left view for
each sheet:

1. It removes the green background. It pulls the green fringe down only
   at the edge, so that the trees and the hills of the base keep their
   green.
2. It finds the red mask, as for a unit.
3. It keeps the figure that touches the leftmost figure pixel. The views
   of a sheet can share a column, so a cut at a column is not sufficient.
4. It scales the view to BASE_WIDTH pixels wide and keeps its aspect.
   The front corner of the walls is on the bottom edge.
5. It writes data/<pack>/sprites/art/base_<kind>.png.

Run it from the root of the repository:

    python -X utf8 misc\\art_import.py [source folder] [data\\placeholder]

Without a source folder, the script imports art-src\\units and
art-src\\bases. The script needs Pillow (pip install pillow). The sheets
stay in art-src/, out of git. The provenance of each sheet is in
data/<pack>/sprites/art/provenance.tsv.
"""

import os
import sys

from PIL import Image, ImageChops, ImageFilter

SPRITE_SIZE = 256
BASE_WIDTH = 384        # pixels; the width of the picture of a base
BASE_STEP = 4           # the scale down of the sheet for the search of the left view
EDGE_BAND = 9           # pixels; the band at the edge of a base where the green fringe goes
MASK_ALPHA = 160        # the alpha of a pixel of the red mask
MARGIN = 6              # pixels of air around the figure before the scale

# The file name of a sheet ends with one of these: the class key of unit_types.tsv.
SHEET_CLASSES = {
    "sword": "sword", "pike": "pike", "halberd": "halberd", "crossbow": "crossbow", "cavalry": "cavalry",
    "ram": "ram", "wood-beast": "wood_beast", "tower": "tower", "catapult": "catapult",
    "transport": "transport", "boat": "boat", "tower-ship": "tower_ship", "battle-ship": "battle_ship",
    "banner": "banner",
}


# The file name of a base sheet ends with one of these: the base kind. The
# longer name comes first, because "base-big-city" also ends with "-city".
SHEET_BASES = {"big-city": "big_city", "city": "city", "gate": "gate", "port": "port"}


def base_of(file_name):
    stem = os.path.splitext(file_name)[0].lower()
    if not stem.startswith("base-"):
        return None
    for suffix, key in SHEET_BASES.items():
        if stem.endswith("-" + suffix):
            return key
    return None


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


def left_view_mask(alpha):
    """The mask of the left view of a base sheet, at the size of the sheet.
    The search runs on a copy BASE_STEP times smaller: the figure that
    touches the leftmost figure pixel, with its 8 neighbours."""
    width, height = alpha.size
    small = alpha.resize((width // BASE_STEP, height // BASE_STEP), Image.BOX)
    small_width, small_height = small.size
    solid = [v > 128 for v in small.tobytes()]
    start = None
    for x in range(small_width):
        for y in range(small_height):
            if solid[y * small_width + x]:
                start = (x, y)
                break
        if start:
            break
    if not start:
        return None
    seen = bytearray(small_width * small_height)

    def figure_at(start):
        """The pixels of the figure that holds start, and its right edge."""
        seen[start[1] * small_width + start[0]] = 1
        stack, pixels, right = [start], [], start[0]
        while stack:
            x, y = stack.pop()
            pixels.append(y * small_width + x)
            right = max(right, x)
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    nx, ny = x + dx, y + dy
                    if nx < 0 or ny < 0 or nx >= small_width or ny >= small_height:
                        continue
                    index = ny * small_width + nx
                    if solid[index] and not seen[index]:
                        seen[index] = 1
                        stack.append((nx, ny))
        return pixels, right

    keep = bytearray(small_width * small_height)
    main_pixels, main_right = figure_at(start)
    for index in main_pixels:
        keep[index] = 255
    # A small part apart from the view, for example a boat on the water,
    # stays when it is fully left of the right edge of the view.
    for x in range(start[0], main_right + 1):
        for y in range(small_height):
            index = y * small_width + x
            if solid[index] and not seen[index]:
                pixels, right = figure_at((x, y))
                if right <= main_right and len(pixels) >= 8:
                    for part in pixels:
                        keep[part] = 255
    mask = Image.frombytes("L", (small_width, small_height), bytes(keep))
    # Two small pixels of air keep the soft edge of the figure in the cut.
    mask = mask.filter(ImageFilter.MaxFilter(5))
    return mask.resize((width, height), Image.NEAREST)


def import_base_sheet(path, out_dir):
    key = base_of(os.path.basename(path))
    if not key:
        print("  skip %s: no base kind in the name" % path)
        return None
    sheet = Image.open(path).convert("RGB")
    alpha = green_alpha(sheet)
    view = left_view_mask(alpha)
    if not view:
        print("  skip %s: no figure" % path)
        return None
    alpha = ImageChops.multiply(alpha, view)
    box = alpha.getbbox()
    width, height = alpha.size
    if box[2] > width // 2:
        print("  warning %s: the left view reaches the middle of the sheet" % path)
    # The despill only in the band along the background: the trees keep their green.
    background = alpha.point(lambda v: 255 if v < 128 else 0)
    band = ImageChops.multiply(background.filter(ImageFilter.MaxFilter(EDGE_BAND)), alpha.point(lambda v: 255 if v > 0 else 0))
    rgb = Image.composite(despill(sheet), sheet, band)
    mask = red_mask(sheet)
    x0, y0, x1, y1 = box
    box = (max(0, x0 - MARGIN), max(0, y0 - MARGIN), min(width, x1 + MARGIN), min(height, y1 + MARGIN))
    rgb, alpha, mask = rgb.crop(box), alpha.crop(box), mask.crop(box)
    fill = Image.new("RGB", rgb.size, (150, 130, 100))
    rgb = Image.composite(rgb, fill, alpha.point(lambda v: 255 if v > 0 else 0))
    masked = alpha.point(lambda v: v * MASK_ALPHA // 255)
    alpha = Image.composite(masked, alpha, mask)
    figure = Image.merge("RGBA", rgb.split() + (alpha,))
    out_height = max(1, round(BASE_WIDTH * figure.size[1] / figure.size[0]))
    picture = figure.resize((BASE_WIDTH, out_height), Image.LANCZOS)
    levels = picture.split()[3].point(lambda v: 0 if v < 100 else (MASK_ALPHA if v < 208 else 255))
    picture.putalpha(levels)
    out_path = os.path.join(out_dir, "base_" + key + ".png")
    picture.save(out_path, optimize=True)
    print("  %s -> %s (%u x %u of the sheet, %u x %u out)" % (os.path.basename(path), out_path, box[2] - box[0], box[3] - box[1], BASE_WIDTH, out_height))
    return out_path


def main():
    pack = sys.argv[2] if len(sys.argv) > 2 else os.path.join("data", "placeholder")
    if len(sys.argv) > 1:
        sources = [sys.argv[1]]
    else:
        sources = [os.path.join("art-src", "units"), os.path.join("art-src", "bases")]
    out_dir = os.path.join(pack, "sprites", "art")
    os.makedirs(out_dir, exist_ok=True)
    count = 0
    for source in sources:
        if not os.path.isdir(source):
            print("  skip %s: no folder" % source)
            continue
        for name in sorted(os.listdir(source)):
            if not name.lower().endswith((".jpg", ".jpeg", ".png")):
                continue
            path = os.path.join(source, name)
            done = import_base_sheet(path, out_dir) if base_of(name) else import_sheet(path, out_dir)
            if done:
                count += 1
    print("%u sprites written to %s" % (count, out_dir))
    return 0 if count else 1


if __name__ == "__main__":
    sys.exit(main())
