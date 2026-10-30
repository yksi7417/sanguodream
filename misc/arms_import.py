"""Imports the coats of arms into the packs of 1200 (docs/EUROPE.md, E5).

Usage: python -X utf8 -E -P misc\\arms_import.py FOLDER

FOLDER holds one PNG file for each force key, for example france.png, as in
the shared Drive folder of docs/europe/ARMS.md. The tool crops each file to
its shape, puts the shape in the centre of a square and makes a copy of
256 x 256 pixels in data/medieval_europe/arms. Then it writes
arms/arms.tsv of each pack of 1200: one row for each faction of forces.tsv
that has a file. A faction without a file keeps the family name of its
ruler on its flag. The tool needs Pillow.
"""
import glob
import os
import sys

from PIL import Image

SIZE = 256
SOLID_ALPHA = 128   # pixels below this alpha do not count as a part of the shape
MARGIN = 0.04       # the empty border around the shape, as a part of the side
PACKS = ("medieval_west", "medieval_east", "medieval_europe")
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")


def force_keys(pack):
    keys = []
    for line in open(os.path.join(ROOT, "data", pack, "scenarios", "y1200", "forces.tsv"), encoding="utf-8"):
        cells = line.rstrip("\n").split("\t")
        if line.startswith("#") or cells[0] == "id":
            continue
        keys.append(cells[1])
    return keys


def centred(image):
    """Crops the image to its solid shape and puts the shape in the centre of a square.
    Gemini does not always put the shield in the centre, and it can leave faint specks."""
    box = image.getchannel("A").point(lambda value: 255 if value >= SOLID_ALPHA else 0).getbbox()
    if box is None:
        return image
    shape = image.crop(box)
    side = int(max(shape.width, shape.height) * (1.0 + 2.0 * MARGIN))
    square = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    square.paste(shape, ((side - shape.width) // 2, (side - shape.height) // 2))
    return square


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    out = os.path.join(ROOT, "data", "medieval_europe", "arms")
    os.makedirs(out, exist_ok=True)
    for path in sorted(glob.glob(os.path.join(sys.argv[1], "*.png"))):
        key = os.path.basename(path)[:-4]
        image = centred(Image.open(path).convert("RGBA")).resize((SIZE, SIZE), Image.LANCZOS)
        image.save(os.path.join(out, key + ".png"), optimize=True)
    have = {os.path.basename(path)[:-4] for path in glob.glob(os.path.join(out, "*.png"))}

    for pack in PACKS:
        lines = ["key\tfile\tnote",
                 "# Written by misc/arms_import.py. The coat of arms of each faction (docs/EUROPE.md, E5). file: a PNG file under the folder data.",
                 "# The flag of a unit shows the arms. A faction without a row keeps the family name of its ruler. The sources are in data/medieval_europe/arms/sources.tsv."]
        missing = []
        for key in force_keys(pack):
            if key in have:
                lines.append("force.%s\tmedieval_europe/arms/%s.png\t" % (key, key))
            else:
                missing.append(key)
        os.makedirs(os.path.join(ROOT, "data", pack, "arms"), exist_ok=True)
        with open(os.path.join(ROOT, "data", pack, "arms", "arms.tsv"), "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(lines) + "\n")
        print("%s: %d factions without arms: %s" % (pack, len(missing), " ".join(missing) or "-"))


if __name__ == "__main__":
    main()
