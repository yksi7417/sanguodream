"""Write a subset of the CJK font for the web build (docs/WEB.md).

Usage: python -I misc/subset_font.py [--output FILE]

The web build downloads its data, and the full Noto Sans CJK TC font is
16 MB. The game can show only the code points that ui_fonts_load puts in
the atlas (code/ui/ui_fonts.h). This script keeps those code points:
- the Latin and Vietnamese ranges of ImGui, and the extra ranges of
  ui_fonts.h (punctuation, kana, the full-width forms)
- each code point of data/unicode/BIG5.TXT outside the user-defined area,
  as in misc/make_big5_table.py
- each character of text/locales.tsv, text/<locale>.tsv and
  names/<locale>.tsv of each pack in data/

The Windows build keeps the full font. Run the script from the root of the
repository after a change to a text file or a names file. It needs
fontTools (pip install fonttools). The default output is
build/web/fonts/NotoSansCJKtc-Regular.otf.
"""
import argparse
import os
import sys

from fontTools import subset
from fontTools.ttLib import TTFont

FONT = os.path.join("data", "fonts", "NotoSansCJKtc-Regular.otf")
BIG5 = os.path.join("data", "unicode", "BIG5.TXT")
DEFAULT_OUTPUT = os.path.join("build", "web", "fonts", "NotoSansCJKtc-Regular.otf")
REPLACEMENT_CHARACTER = 0xFFFD

# GetGlyphRangesDefault and GetGlyphRangesVietnamese of ImGui 1.91, and
# ui_font_extra_ranges of code/ui/ui_fonts.h. Each pair is inclusive.
RANGES = [
    (0x0020, 0x00FF),
    (0x0102, 0x0103), (0x0110, 0x0111), (0x0128, 0x0129), (0x0168, 0x0169),
    (0x01A0, 0x01A1), (0x01AF, 0x01B0), (0x1EA0, 0x1EF9),
    (0x2000, 0x206F),
    (0x3000, 0x30FF),
    (0x31F0, 0x31FF),
    (0xFF00, 0xFFEF),
    (REPLACEMENT_CHARACTER, REPLACEMENT_CHARACTER),   # the fallback glyph of ImGui
]


def big5_code_points(path):
    """The code points of big5_table.h: no user-defined lead byte, no U+FFFD."""
    points = set()
    with open(path, encoding="ascii", errors="replace") as handle:
        for line in handle:
            fields = line.split("#")[0].split()
            if len(fields) < 2:
                continue
            big5, unicode = int(fields[0], 16), int(fields[1], 16)
            lead = big5 >> 8
            if 0x81 <= lead <= 0xA0 or 0xFA <= lead <= 0xFE:
                continue
            if unicode != REPLACEMENT_CHARACTER:
                points.add(unicode)
    return points


def text_code_points(data_dir):
    """Each character of the locale files of each pack."""
    points = set()
    for pack in sorted(os.listdir(data_dir)):
        locales = os.path.join(data_dir, pack, "text", "locales.tsv")
        if not os.path.isfile(locales):
            continue
        files = [locales]
        with open(locales, encoding="utf-8") as handle:
            for line in handle:
                code = line.split("\t")[0].strip()
                if not code or code.startswith("#") or code == "code":
                    continue
                files.append(os.path.join(data_dir, pack, "text", code + ".tsv"))
                files.append(os.path.join(data_dir, pack, "names", code + ".tsv"))
        for path in files:
            if os.path.isfile(path):
                with open(path, encoding="utf-8") as handle:
                    points.update(ord(character) for character in handle.read())
    return points


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--output", default=DEFAULT_OUTPUT)
    args = parser.parse_args(argv)

    wanted = set()
    for first, last in RANGES:
        wanted.update(range(first, last + 1))
    wanted |= big5_code_points(BIG5)
    wanted |= text_code_points("data")

    source_map = TTFont(FONT).getBestCmap()
    kept = sorted(point for point in wanted if point in source_map)

    options = subset.Options()
    options.layout_features = []      # stb_truetype in ImGui reads no OpenType layout
    options.hinting = False           # the atlas has one sample for each pixel
    options.name_IDs = ["*"]          # keep the names and the licence text of the font
    options.notdef_outline = True
    font = subset.load_font(FONT, options)
    subsetter = subset.Subsetter(options)
    subsetter.populate(unicodes=kept)
    subsetter.subset(font)
    os.makedirs(os.path.dirname(args.output) or ".", exist_ok=True)
    subset.save_font(font, args.output, options)

    missing = [point for point in kept if point not in TTFont(args.output).getBestCmap()]
    print(f"{FONT}: {os.path.getsize(FONT)} bytes, {len(source_map)} code points")
    print(f"{args.output}: {os.path.getsize(args.output)} bytes, {len(kept)} code points "
          f"({len(wanted) - len(kept)} wanted code points are not in the font)")
    if missing:
        print(f"FAIL: {len(missing)} code points are not in the subset")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
