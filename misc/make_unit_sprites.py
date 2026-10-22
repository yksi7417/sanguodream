"""Writes the placeholder sprites of the unit classes (docs/ASSETS.md, "Unit sprites").

The script draws one 64 x 64 picture for each render_sprite of
code/render/render.h into data/placeholder/sprites/. Each picture is a
light figure with a dark outline on a transparent background. The renderer
multiplies the picture by the colour of the faction, so the figure takes
that colour on the map. Run it from the root of the repository:

    python -X utf8 misc\\make_unit_sprites.py

The script needs Pillow (pip install pillow). The pictures are project
work, not art of the original game.
"""

import os
import sys

from PIL import Image, ImageDraw, ImageFilter

SIZE = 64
FILL = (235, 235, 235, 255)
SHADE = (170, 170, 170, 255)
OUTLINE = (30, 30, 30, 255)


def new_canvas():
    image = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    return image, ImageDraw.Draw(image)


def thick_line(draw, points, width, fill=FILL):
    draw.line(points, fill=fill, width=width, joint="curve")
    for x, y in points:
        half = width / 2.0
        draw.ellipse((x - half, y - half, x + half, y + half), fill=fill)


def draw_unit(draw):
    """The figure of sprite 0: a head over a body."""
    draw.ellipse((22, 6, 42, 26), fill=FILL)
    draw.ellipse((16, 26, 48, 60), fill=FILL)


def draw_sword(draw):
    draw.polygon([(32, 4), (37, 12), (37, 40), (27, 40), (27, 12)], fill=FILL)
    draw.rectangle((18, 40, 46, 45), fill=SHADE)
    draw.rectangle((29, 45, 35, 58), fill=FILL)
    draw.ellipse((27, 56, 37, 62), fill=SHADE)


def draw_pike(draw):
    thick_line(draw, [(14, 60), (50, 12)], 5)
    draw.polygon([(50, 12), (58, 2), (46, 6)], fill=FILL)
    draw.polygon([(44, 20), (54, 6), (56, 14)], fill=SHADE)


def draw_halberd(draw):
    thick_line(draw, [(26, 62), (26, 6)], 5)
    draw.polygon([(26, 2), (31, 6), (26, 14), (21, 6)], fill=FILL)
    draw.polygon([(29, 10), (50, 14), (52, 30), (29, 34)], fill=FILL)
    draw.polygon([(29, 16), (42, 18), (42, 28), (29, 30)], fill=SHADE)


def draw_crossbow(draw):
    draw.arc((6, 10, 58, 46), start=200, end=340, fill=FILL, width=5)
    thick_line(draw, [(9, 24), (55, 24)], 2, fill=SHADE)
    draw.rectangle((29, 14, 35, 58), fill=FILL)
    draw.polygon([(32, 4), (38, 16), (26, 16)], fill=FILL)
    draw.rectangle((24, 44, 40, 50), fill=SHADE)


def draw_cavalry(draw):
    draw.ellipse((12, 26, 50, 46), fill=FILL)
    draw.polygon([(44, 30), (54, 14), (62, 18), (58, 30), (50, 36)], fill=FILL)
    draw.polygon([(50, 14), (56, 6), (58, 16)], fill=FILL)
    for x in (16, 24, 36, 44):
        draw.rectangle((x, 42, x + 5, 60), fill=FILL)
    draw.polygon([(12, 30), (2, 40), (6, 48), (14, 38)], fill=SHADE)
    draw.polygon([(44, 16), (52, 12), (50, 30)], fill=SHADE)


def draw_siege(draw):
    draw.rectangle((10, 22, 54, 42), fill=FILL)
    draw.polygon([(8, 22), (32, 8), (56, 22)], fill=SHADE)
    thick_line(draw, [(4, 34), (62, 34)], 6, fill=SHADE)
    for x in (16, 48):
        draw.ellipse((x - 8, 42, x + 8, 58), fill=FILL)
        draw.ellipse((x - 3, 47, x + 3, 53), fill=OUTLINE)


def draw_boat(draw):
    draw.polygon([(4, 40), (60, 40), (50, 54), (14, 54)], fill=FILL)
    draw.rectangle((30, 10, 34, 40), fill=SHADE)
    draw.polygon([(34, 12), (52, 36), (34, 36)], fill=FILL)


def draw_ship(draw):
    draw.polygon([(2, 38), (62, 38), (54, 56), (10, 56)], fill=FILL)
    draw.rectangle((22, 22, 42, 38), fill=SHADE)
    draw.rectangle((18, 8, 22, 38), fill=SHADE)
    draw.rectangle((44, 12, 48, 38), fill=SHADE)
    draw.polygon([(22, 10), (36, 26), (22, 26)], fill=FILL)
    draw.polygon([(48, 14), (60, 28), (48, 28)], fill=FILL)


def draw_transport(draw):
    draw.rectangle((8, 26, 50, 44), fill=FILL)
    draw.ellipse((12, 14, 30, 30), fill=SHADE)
    draw.ellipse((26, 12, 46, 30), fill=SHADE)
    for x in (18, 42):
        draw.ellipse((x - 8, 42, x + 8, 58), fill=FILL)
        draw.ellipse((x - 3, 47, x + 3, 53), fill=OUTLINE)
    thick_line(draw, [(50, 36), (62, 32)], 4, fill=SHADE)


SPRITES = {
    "unit": draw_unit,
    "sword": draw_sword,
    "pike": draw_pike,
    "halberd": draw_halberd,
    "crossbow": draw_crossbow,
    "cavalry": draw_cavalry,
    "siege": draw_siege,
    "boat": draw_boat,
    "ship": draw_ship,
    "transport": draw_transport,
}


def outlined(figure):
    """The figure over a dark outline that is two pixels wide."""
    alpha = figure.split()[3]
    grown = alpha.filter(ImageFilter.MaxFilter(5))
    outline = Image.new("RGBA", (SIZE, SIZE), OUTLINE)
    outline.putalpha(grown)
    return Image.alpha_composite(outline, figure)


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    folder = os.path.join(root, "data", "placeholder", "sprites")
    os.makedirs(folder, exist_ok=True)
    for name, painter in SPRITES.items():
        figure, draw = new_canvas()
        painter(draw)
        outlined(figure).save(os.path.join(folder, name + ".png"))
        print("wrote", name + ".png")
    return 0


if __name__ == "__main__":
    sys.exit(main())
