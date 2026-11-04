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

from PIL import Image

# The terrain ids of terrain.tsv, in order, as the names of the source files.
TERRAINS = ["grass", "earth", "sand", "swamp", "poison-spring", "forest", "stream", "river", "sea", "waste",
            "main-road", "plank-road", "ford", "shallows", "shore", "cliff", "city-ground", "port-ground",
            "gate-ground", "small-path"]
SEASONS = ["spring", "summer", "autumn", "winter"]
WATER = ["stream", "river", "sea", "ford", "shallows"]   # the same order as the terrain shaders
WRAP_PIXELS = 32   # source pixels around a tile, for the filter of the scale


def tile(path, side):
    source = Image.open(path).convert("RGB")
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
    names = ["terrain-%s-%s.jpg" % (name, season) for name in TERRAINS for season in SEASONS]
    names += ["terrain-%s-%s-ripple2.jpg" % (name, season) for name in WATER for season in SEASONS]
    missing = [name for name in names if not os.path.exists(os.path.join(folder, name))]
    if missing:
        sys.exit("missing: " + ", ".join(missing))
    strip = Image.new("RGB", (side, side * len(names)))
    for index, name in enumerate(names):
        strip.paste(tile(os.path.join(folder, name), side), (0, index * side))
    strip.save(out, "JPEG", quality=quality, optimize=True, progressive=False, subsampling=2)
    print("%s: %u layers, %u x %u, %u bytes" % (out, len(names), strip.width, strip.height, os.path.getsize(out)))


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    main(sys.argv[1], sys.argv[2], int(sys.argv[3]) if len(sys.argv) > 3 else 256, int(sys.argv[4]) if len(sys.argv) > 4 else 80)
