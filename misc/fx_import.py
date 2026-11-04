"""Makes the effect atlas of a pack from the effect sheets of the Drive folder
(docs/ASSETS.md, "Effects and icons").

Run: python -X utf8 -E misc\\fx_import.py SOURCE_FOLDER OUT.png

OUT.png has 8 columns and one row for each row of SHEETS, with square cells of
CELL pixels. code/ui/ui_effects.cpp has the same rows (ui_fx_row). A sheet on
black gets its alpha from its brightness. A sheet on green loses the green.
Some sheets show a part of a reference picture under the frames, so each black
sheet has a band of rows to use. In a row, all the frames have one scale, and
they stand on the bottom edge of the cell, or they stay in its centre if the
effect spreads from a point (CENTRED). It needs Pillow.
"""
import os
import sys

from PIL import Image

CELL = 128
COLUMNS = 8
ALPHA_FLOOR = 14       # a darker pixel of a black sheet is noise of the JPEG
GREEN_MARGIN = 40      # a pixel with this much more green than red and blue is background
CENTRED = {"fx-sparks.jpg", "fx-dark-vortex.jpg", "fx-calm-ripple.jpg", "fx-drum-rings.jpg"}
SEPARATE_FIGURES = {"fx-projectiles.jpg", "fx-icon-sheet.jpg"}   # these sheets have unequal columns
CUT_WINDOW = 0.2      # a cut between two equal frames can move by this part of a frame
FIGURE_GAP = 8         # empty source columns that separate two figures of a green sheet

# file, frames, top and bottom of the band as a part of the height, "black" or "green"
SHEETS = [
    ("fx-big-fire.jpg", 8, 0.25, 0.75, "black"),
    ("fx-flame-loop.jpg", 8, 0.25, 0.82, "black"),
    ("fx-smoke-puff.jpg", 8, 0.20, 0.72, "black"),
    ("fx-dust-cloud.jpg", 6, 0.30, 0.75, "black"),
    ("fx-impact-debris.jpg", 6, 0.20, 0.80, "green"),
    ("fx-sparks.jpg", 4, 0.20, 0.70, "black"),
    ("fx-water-splash.jpg", 6, 0.25, 0.85, "black"),
    ("fx-thunderbolt.jpg", 4, 0.00, 1.00, "black"),
    ("fx-dark-vortex.jpg", 8, 0.20, 0.60, "black"),
    ("fx-calm-ripple.jpg", 6, 0.20, 0.56, "black"),
    ("fx-drum-rings.jpg", 6, 0.20, 0.62, "black"),
    ("fx-projectiles.jpg", 5, 0.20, 0.80, "green"),
    ("fx-icon-sheet.jpg", 8, 0.20, 0.80, "green"),
]


def black_to_alpha(picture):
    pixels = picture.load()
    for y in range(picture.height):
        for x in range(picture.width):
            r, g, b, _ = pixels[x, y]
            alpha = max(r, g, b)
            if alpha < ALPHA_FLOOR:
                pixels[x, y] = (0, 0, 0, 0)
            else:
                pixels[x, y] = (r * 255 // alpha, g * 255 // alpha, b * 255 // alpha, alpha)


def green_to_alpha(picture):
    pixels = picture.load()
    for y in range(picture.height):
        for x in range(picture.width):
            r, g, b, _ = pixels[x, y]
            if g > r + GREEN_MARGIN and g > b + GREEN_MARGIN:
                pixels[x, y] = (0, 0, 0, 0)
            else:
                pixels[x, y] = (r, min(g, max(r, b) + GREEN_MARGIN // 2), b, 255)


def figure_columns(picture, frames):
    """The x ranges of the figures of a green sheet, from the gaps between them."""
    alpha = picture.getchannel("A")
    filled = [alpha.crop((x, 0, x + 1, alpha.height)).getbbox() is not None for x in range(alpha.width)]
    ranges = []
    start = None
    gap = 0
    for x, full in enumerate(filled):
        if full:
            if start is None:
                start = x
            gap = 0
        elif start is not None:
            gap += 1
            if gap >= FIGURE_GAP:
                ranges.append((start, x - gap + 1))
                start = None
    if start is not None:
        ranges.append((start, len(filled)))
    if len(ranges) != frames:
        sys.exit("a green sheet has %u figures, not %u" % (len(ranges), frames))
    return ranges


def quiet_columns(picture, frames):
    """The x ranges of equal frames. Each cut moves to the emptiest column near it,
    because the tail of a figure can cross the line between two frames."""
    alpha = picture.getchannel("A").resize((picture.width, 1), Image.BOX)
    fill = list(alpha.getdata())
    width = picture.width // frames
    window = int(width * CUT_WINDOW)
    cuts = [0]
    for index in range(1, frames):
        near = range(index * width - window, index * width + window)
        cuts.append(min(near, key=lambda x: fill[x]))
    cuts.append(picture.width)
    return list(zip(cuts, cuts[1:]))


def main(folder, out):
    atlas = Image.new("RGBA", (CELL * COLUMNS, CELL * len(SHEETS)), (0, 0, 0, 0))
    for row, (name, frames, top, bottom, background) in enumerate(SHEETS):
        source = Image.open(os.path.join(folder, name)).convert("RGBA")
        band = source.crop((0, int(source.height * top), source.width, int(source.height * bottom)))
        if background == "black":
            black_to_alpha(band)
        else:
            green_to_alpha(band)
        if name in SEPARATE_FIGURES:
            ranges = figure_columns(band, frames)
        else:
            ranges = quiet_columns(band, frames)
        cuts = [band.crop((left, 0, right, band.height)) for left, right in ranges]
        boxes = [cut.getbbox() or (0, 0, 1, 1) for cut in cuts]
        boxed = [cut.crop(box) for cut, box in zip(cuts, boxes)]
        largest = max(max(part.width, part.height) for part in boxed)
        scale = CELL / largest
        for column, part in enumerate(boxed):
            size = (max(1, round(part.width * scale)), max(1, round(part.height * scale)))
            small = part.resize(size, Image.LANCZOS)
            top_edge = row * CELL + (CELL - size[1]) // 2 if name in CENTRED else (row + 1) * CELL - size[1]
            atlas.alpha_composite(small, (column * CELL + (CELL - size[0]) // 2, top_edge))
    atlas.save(out, "PNG", optimize=True)
    print("%s: %u x %u, %u bytes" % (out, atlas.width, atlas.height, os.path.getsize(out)))


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(2)
    main(sys.argv[1], sys.argv[2])
