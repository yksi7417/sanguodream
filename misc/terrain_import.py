"""Makes the ground tiles of a pack from the terrain drop of the Drive folder
(docs/ASSETS.md, "Terrain tiles").

Run: python -X utf8 -E misc\\terrain_import.py SOURCE_FOLDER OUT.jpg [SIDE] [QUALITY]

SOURCE_FOLDER holds terrain-<name>-<season>.jpg, and for each water terrain
terrain-<name>-<season>-ripple2.jpg. OUT.jpg is one baseline JPEG, SIDE pixels
wide, with one square tile below the other:

- layers 0..79: terrain id x 4 + season (spring, summer, autumn, winter)
- layers 80..99: the second frame of the water terrains, in the order of
  WATER, 4 for each terrain

SIDE is a multiple of 16, so that a JPEG block never crosses two tiles. The
scale wraps at the edges, so that a tile stays seamless. It needs Pillow.
"""
import os
import sys

from PIL import Image, ImageChops, ImageOps

# The terrain ids of terrain.tsv, in order, as the names of the source files.
TERRAINS = ["grass", "earth", "sand", "swamp", "poison-spring", "forest", "stream", "river", "sea", "waste",
            "main-road", "plank-road", "ford", "shallows", "shore", "cliff", "city-ground", "port-ground",
            "gate-ground", "small-path"]
SEASONS = ["spring", "summer", "autumn", "winter"]
WATER = ["stream", "river", "sea", "ford", "shallows"]   # the same order as the terrain shaders
WRAP_PIXELS = 32   # source pixels around a tile, for the filter of the scale

# The drop paints the river olive and brown, and a wide river then looks like land.
# The dark, middle and light colours of the water of a season.
RIVER_BLUES = {
    "spring": ((24, 62, 96), (58, 124, 168), (178, 222, 238)),
    "summer": ((16, 50, 82), (44, 104, 148), (140, 198, 222)),
    "autumn": ((18, 46, 74), (48, 96, 134), (150, 192, 212)),
    "winter": ((40, 72, 100), (92, 138, 170), (248, 251, 252)),
}
# The drop paints the earth brown and orange. Earth is most of the land of the map, and the old flat colour is green.
# The tool changes the spring and summer tiles. Orange autumn and snow in winter are correct for the season.
EARTH_GREENS = {
    "spring": ((58, 92, 42), (104, 150, 66), (176, 204, 118)),
    "summer": ((44, 78, 36), (82, 124, 54), (150, 178, 92)),
}
RAMP_MIDPOINT = 110           # the grey value of the middle colour
RAMP_CUTOFF_PERCENT = 1       # the darkest and lightest pixels that the contrast ignores
RIVER_LEAF_HUE = 30           # below this hue (0..255) a strong colour is a leaf, not water
RIVER_LEAF_SATURATION = 150   # above this saturation (0..255) a colour can be a leaf


def colour_ramp(source, ramp):
    """Keeps the light and dark of a tile, and gives it the dark, middle and light colours of the ramp."""
    dark, middle, light = ramp
    grey = ImageOps.autocontrast(source.convert("L"), cutoff=RAMP_CUTOFF_PERCENT)
    return ImageOps.colorize(grey, dark, light, middle, midpoint=RAMP_MIDPOINT)


def blue_river(source, season):
    """Keeps the waves of a river tile, and changes the colour of the water to blue."""
    water = colour_ramp(source, RIVER_BLUES[season])
    hue, saturation, _ = source.convert("HSV").split()
    is_leaf = ImageChops.multiply(hue.point(lambda h: 255 if h < RIVER_LEAF_HUE else 0),
                                  saturation.point(lambda s: 255 if s > RIVER_LEAF_SATURATION else 0))
    return Image.composite(source, water, is_leaf)


def tile(path, side, terrain, season):
    source = Image.open(path).convert("RGB")
    if terrain == "river":
        source = blue_river(source, season)
    elif terrain == "earth" and season in EARTH_GREENS:
        source = colour_ramp(source, EARTH_GREENS[season])
    width, height = source.size
    wrapped = Image.new("RGB", (width + 2 * WRAP_PIXELS, height + 2 * WRAP_PIXELS))
    for x in (WRAP_PIXELS - width, WRAP_PIXELS, WRAP_PIXELS + width):
        for y in (WRAP_PIXELS - height, WRAP_PIXELS, WRAP_PIXELS + height):
            wrapped.paste(source, (x, y))
    box = (WRAP_PIXELS, WRAP_PIXELS, WRAP_PIXELS + width, WRAP_PIXELS + height)
    return wrapped.resize((side, side), Image.LANCZOS, box=box)


def main(folder, out, side, quality):
    if side % 16:
        sys.exit("SIDE must be a multiple of 16")
    layers = [("terrain-%s-%s.jpg" % (name, season), name, season) for name in TERRAINS for season in SEASONS]
    layers += [("terrain-%s-%s-ripple2.jpg" % (name, season), name, season) for name in WATER for season in SEASONS]
    missing = [layer[0] for layer in layers if not os.path.exists(os.path.join(folder, layer[0]))]
    if missing:
        sys.exit("missing: " + ", ".join(missing))
    strip = Image.new("RGB", (side, side * len(layers)))
    for index, (file_name, terrain, season) in enumerate(layers):
        strip.paste(tile(os.path.join(folder, file_name), side, terrain, season), (0, index * side))
    strip.save(out, "JPEG", quality=quality, optimize=True, progressive=False, subsampling=2)
    print("%s: %u layers, %u x %u, %u bytes" % (out, len(layers), strip.width, strip.height, os.path.getsize(out)))


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    main(sys.argv[1], sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 256, int(sys.argv[4]) if len(sys.argv) > 4 else 80)
