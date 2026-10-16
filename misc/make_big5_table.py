"""Generate code/text/big5_table.h from the Unicode BIG5.TXT mapping.

Usage: python -I misc/make_big5_table.py [--input FILE] [--output FILE] [--download]

The script reads data/unicode/BIG5.TXT. With --download, it first gets the
file from unicode.org if the file is not there. It writes a two-level table:

- big5_row_of_lead[lead - 0x81]: 0 if BIG5.TXT has no row for the lead
  byte, else the row number + 1.
- big5_rows[row][trail index]: the code point, or 0 if BIG5.TXT gives no
  code point (or gives U+FFFD) for the pair.

The trail index is 0..62 for the trail bytes 0x40..0x7E and 63..156 for
the trail bytes 0xA1..0xFE. The user-defined area (lead bytes 0x81..0xA0
and 0xFA..0xFE) is not in the table. code/text/text.cpp maps it to the
Private Use Area with a formula.

Run it from the root of the repository. The output does not change if the
input does not change.
"""

import argparse
import os
import sys
import urllib.request

BIG5_URL = "https://www.unicode.org/Public/MAPPINGS/OBSOLETE/EASTASIA/OTHER/BIG5.TXT"
LEAD_FIRST = 0x81
LEAD_LAST = 0xFE
TRAIL_COUNT = 157
REPLACEMENT_CHARACTER = 0xFFFD


def trail_index(trail):
    if 0x40 <= trail <= 0x7E:
        return trail - 0x40
    if 0xA1 <= trail <= 0xFE:
        return 63 + trail - 0xA1
    return None


def read_mapping(path):
    """Returns {big5 pair: code point} from the two first columns of BIG5.TXT."""
    mapping = {}
    with open(path, "r", encoding="utf-8") as source:
        for line_number, line in enumerate(source, 1):
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
            columns = line.split()
            if len(columns) < 2:
                sys.exit("%s:%d: expected two columns" % (path, line_number))
            pair = int(columns[0], 16)
            code_point = int(columns[1], 16)
            if pair in mapping:
                sys.exit("%s:%d: pair 0x%04X appears two times" % (path, line_number, pair))
            mapping[pair] = code_point
    return mapping


def build_rows(mapping):
    """Returns (row_of_lead list, rows list) for the leads that have pairs."""
    rows_by_lead = {}
    for pair, code_point in mapping.items():
        lead, trail = pair >> 8, pair & 0xFF
        index = trail_index(trail)
        if not (LEAD_FIRST <= lead <= LEAD_LAST) or index is None:
            sys.exit("pair 0x%04X is not a Big5 pair" % pair)
        if code_point > 0xFFFF:
            sys.exit("pair 0x%04X maps outside the BMP; the table holds u16" % pair)
        if code_point == REPLACEMENT_CHARACTER:
            continue
        row = rows_by_lead.setdefault(lead, [0] * TRAIL_COUNT)
        row[index] = code_point
    row_of_lead = []
    rows = []
    for lead in range(LEAD_FIRST, LEAD_LAST + 1):
        if lead in rows_by_lead:
            rows.append(rows_by_lead[lead])
            row_of_lead.append(len(rows))
        else:
            row_of_lead.append(0)
    return row_of_lead, rows


def write_header(path, row_of_lead, rows, pair_count):
    lines = []
    lines.append("#ifndef BIG5_TABLE_H")
    lines.append("#define BIG5_TABLE_H")
    lines.append("/* GENERATED FILE. Do not edit. misc/make_big5_table.py writes it from")
    lines.append("   data/unicode/BIG5.TXT (Unicode License v3, see data/unicode/README.md).")
    lines.append("")
    lines.append("   big5_row_of_lead[lead - BIG5_LEAD_FIRST] is 0 if the table has no row")
    lines.append("   for the lead byte, else the row number + 1. big5_rows[row][trail index]")
    lines.append("   is the code point, or 0 if BIG5.TXT has no code point for the pair.")
    lines.append("   The trail index is 0..62 for the trail bytes 0x40..0x7E and 63..156 for")
    lines.append("   the trail bytes 0xA1..0xFE. */")
    lines.append("")
    lines.append("#define BIG5_LEAD_FIRST  0x%02X" % LEAD_FIRST)
    lines.append("#define BIG5_LEAD_LAST   0x%02X" % LEAD_LAST)
    lines.append("#define BIG5_TRAIL_COUNT %d" % TRAIL_COUNT)
    lines.append("#define BIG5_ROW_COUNT   %d" % len(rows))
    lines.append("#define BIG5_PAIR_COUNT  %d   /* pairs with a code point */" % pair_count)
    lines.append("")
    lines.append("global_variable const u8 big5_row_of_lead[BIG5_LEAD_LAST - BIG5_LEAD_FIRST + 1] =")
    lines.append("{")
    for start in range(0, len(row_of_lead), 16):
        chunk = row_of_lead[start:start + 16]
        lines.append("    " + ", ".join("%3d" % value for value in chunk) + ",")
    lines.append("};")
    lines.append("")
    lines.append("global_variable const u16 big5_rows[BIG5_ROW_COUNT][BIG5_TRAIL_COUNT] =")
    lines.append("{")
    lead_of_row = [LEAD_FIRST + offset for offset, row in enumerate(row_of_lead) if row]
    for lead, row in zip(lead_of_row, rows):
        lines.append("    { /* lead 0x%02X */" % lead)
        for start in range(0, TRAIL_COUNT, 12):
            chunk = row[start:start + 12]
            lines.append("        " + ", ".join("0x%04X" % value for value in chunk) + ",")
        lines.append("    },")
    lines.append("};")
    lines.append("")
    lines.append("#endif /* BIG5_TABLE_H */")
    with open(path, "w", encoding="ascii", newline="\n") as target:
        target.write("\n".join(lines) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--input", default=os.path.join("data", "unicode", "BIG5.TXT"))
    parser.add_argument("--output", default=os.path.join("code", "text", "big5_table.h"))
    parser.add_argument("--download", action="store_true",
                        help="get BIG5.TXT from unicode.org if the input file is not there")
    arguments = parser.parse_args()

    if not os.path.exists(arguments.input):
        if not arguments.download:
            sys.exit("%s not found; use --download" % arguments.input)
        os.makedirs(os.path.dirname(arguments.input), exist_ok=True)
        urllib.request.urlretrieve(BIG5_URL, arguments.input)

    mapping = read_mapping(arguments.input)
    row_of_lead, rows = build_rows(mapping)
    pair_count = sum(1 for row in rows for value in row if value)
    write_header(arguments.output, row_of_lead, rows, pair_count)
    print("%s: %d rows, %d pairs with a code point" % (arguments.output, len(rows), pair_count))


if __name__ == "__main__":
    main()
