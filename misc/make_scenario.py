"""Checks a scenario of a content pack and writes its Big5 name columns (docs/MAPS.md, W2).

    python misc/make_scenario.py data/medieval_europe/scenarios/y1200

The scenario folder holds scenario.tsv, forces.tsv, officers.tsv and cities.tsv.
You write family_zh and given_zh in officers.tsv. The tool writes family_big5 and
given_big5, the Big5 bytes in hex that the engine reads. A name part holds at most
4 Big5 bytes (two characters), because the record holds 5 bytes with the NUL.
The tool also checks the rows that the engine checks, so that an error shows here
with its row and not only as "a bad row" in the game.

The tool also writes names/en.tsv of the pack: the English names by English key
(docs/EUROPE.md, E2). The rows come from name_en of officers.tsv and forces.tsv of
the scenario, and from name_en of map/source/sites.tsv of the pack. It checks
that each officer has a life in lore/en.tsv of the pack. It checks that each wikipedia
cell is - or a URL of the English Wikipedia (it must start with
https://en.wikipedia.org/wiki/ and have a name, without a space, a control character or one of the
characters in LINK_FORBIDDEN). It writes lore/links.tsv of the pack: the URL by officer.<key>,
and no row for an officer with - (docs/EUROPE.md, E4).

It also writes names/native.tsv of the pack: name_latin of each officer by the same
keys (docs/EUROPE.md, E3). It fails if name_latin is empty or has a letter that is not
Latin.

If the scenario folder has renames.tsv (columns key and name_en), the tool also writes
a force.<key> row for each line. The effect rename of history.tsv uses these names
(docs/EUROPE.md, E7).
"""
import os
import sys

NAME_PART_BYTES = 4
STATUSES = ("ruler", "governor", "officer", "free")
CITY_COUNT = 42


def read(path):
    with open(path, encoding="utf-8") as f:
        lines = [line.rstrip("\r\n") for line in f]
    head = next(i for i, line in enumerate(lines) if line and not line.startswith("#"))
    columns = lines[head].split("\t")
    rows = [dict(zip(columns, line.split("\t"))) for line in lines[head + 1:] if line and not line.startswith("#")]
    return lines, head, columns, rows


def big5(text, where):
    data = text.encode("big5")
    if not data or len(data) > NAME_PART_BYTES: fail("%s: %s has %d Big5 bytes; a name part holds 1 to %d" % (where, text, len(data), NAME_PART_BYTES))
    return data.hex().upper()


def fail(message):
    sys.exit("make_scenario: " + message)


def write_names(folder, officers, forces):
    pack = os.path.dirname(os.path.dirname(os.path.abspath(folder)))
    _, _, _, sites = read(os.path.join(pack, "map", "source", "sites.tsv"))
    lines = ["key\ttext\tnote",
             "# Written by misc/make_scenario.py. The English names of the pack by English key (docs/EUROPE.md, E2).",
             "# officer.<key> and force.<key> come from the scenario %s. city.<key> and base.<key> come from map/source/sites.tsv." % os.path.basename(folder)]
    for row in officers: lines.append("officer.%s\t%s" % (row["key"], row["name_en"]))
    for row in forces:   lines.append("force.%s\t%s" % (row["key"], row["name_en"]))
    renames_path = os.path.join(folder, "renames.tsv")
    if os.path.exists(renames_path):
        for row in read(renames_path)[3]: lines.append("force.%s\t%s" % (row["key"], row["name_en"]))
    for row in sites:   lines.append("%s.%s\t%s" % ("city" if row["kind"] == "city" else "base", row["key"], row["name_en"]))
    os.makedirs(os.path.join(pack, "names"), exist_ok=True)
    with open(os.path.join(pack, "names", "en.tsv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")

    lore_path = os.path.join(pack, "lore", "en.tsv")
    lives = {}
    if os.path.exists(lore_path):
        _, _, _, lore = read(lore_path)
        lives = {row["key"]: row["text"] for row in lore}
    links = []
    for row in officers:
        where = "officers.tsv id %s" % row["id"]
        url = row.get("wikipedia", "")
        if url != "-" and not link_is_allowed(url):
            fail(where + ": wikipedia must be - or a URL that starts with " + LINK_PREFIX + " and has a name")
        if not lives.get("officer." + row["key"]): fail(where + ": lore/en.tsv of the pack has no life for officer." + row["key"])
        if url != "-": links.append("officer.%s\t%s" % (row["key"], url))
    write_links(folder, pack, links)
    write_native_names(folder, pack, officers)
    return len(lines) - 3


# The prefix of each link that the game opens. code/text/text.cpp, text_link_is_allowed, has
# the same rule, and the platform checks it again before it opens a link.
LINK_PREFIX = "https://en.wikipedia.org/wiki/"
LINK_FORBIDDEN = '"<>\\^`{|}'


def link_is_allowed(url):
    if not url.startswith(LINK_PREFIX) or len(url) == len(LINK_PREFIX): return False
    return not any(ord(c) <= 0x20 or ord(c) == 0x7F or c in LINK_FORBIDDEN for c in url)


def write_links(folder, pack, links):
    lines = ["key\turl\tnote",
             "# Written by misc/make_scenario.py. The English Wikipedia article of each officer, by English key (docs/EUROPE.md, E4).",
             "# officer.<key> comes from the wikipedia column of officers.tsv in the scenario %s. An officer with - has no row." % os.path.basename(folder),
             "# The game opens only a URL that starts with " + LINK_PREFIX]
    os.makedirs(os.path.join(pack, "lore"), exist_ok=True)
    with open(os.path.join(pack, "lore", "links.tsv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines + links) + "\n")


# The Latin letters that the fonts of the game show (code/ui/ui_fonts.h): ASCII, Latin-1,
# Latin Extended-A and B, the spacing modifier letters (the ayn of "Muhammad al-Nasir"),
# Latin Extended Additional, and the punctuation block.
LATIN_RANGES = ((0x20, 0x7E), (0xA0, 0x24F), (0x2B0, 0x2FF), (0x1E00, 0x1EFF), (0x2010, 0x205E))


def write_native_names(folder, pack, officers):
    lines = ["key\ttext\tnote",
             "# Written by misc/make_scenario.py. The names in their own language, in the Latin script, by English key (docs/EUROPE.md, E3).",
             "# officer.<key> comes from name_latin of officers.tsv in the scenario %s. The Names setting of the Settings panel shows these rows." % os.path.basename(folder)]
    for row in officers:
        where = "officers.tsv id %s" % row["id"]
        name = row.get("name_latin", "")
        if not name: fail(where + ": name_latin is empty")
        for letter in name:
            if not any(first <= ord(letter) <= last for first, last in LATIN_RANGES):
                fail("%s: name_latin has the character U+%04X, which is not a Latin letter (question Q-E4)" % (where, ord(letter)))
        lines.append("officer.%s\t%s" % (row["key"], name))
    with open(os.path.join(pack, "names", "native.tsv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


def main():
    if len(sys.argv) != 2: sys.exit(__doc__)
    folder = sys.argv[1]
    lines, head, columns, officers = read(os.path.join(folder, "officers.tsv"))
    _, _, _, forces = read(os.path.join(folder, "forces.tsv"))
    _, _, _, cities = read(os.path.join(folder, "cities.tsv"))

    by_id, names = {}, {}
    for row in officers:
        where = "officers.tsv id %s" % row["id"]
        if row["id"] in by_id: fail(where + ": the id is two times in the table")
        by_id[row["id"]] = row
        row["family_big5"] = big5(row["family_zh"], where)
        row["given_big5"] = big5(row["given_zh"], where)
        full = row["family_zh"] + row["given_zh"]
        if full in names: fail("%s: the name %s is also the name of id %s" % (where, full, names[full]))
        names[full] = row["id"]
        if row["status"] not in STATUSES: fail(where + ": the status must be one of " + ", ".join(STATUSES))
        if (row["status"] == "free") != (row["force"] == "-"): fail(where + ": a free officer has the force -, and only a free officer")
        if len(row["aptitudes"]) != 6 or any(c not in "CBAS" for c in row["aptitudes"]): fail(where + ": aptitudes are 6 letters of C, B, A, S")

    for index, force in enumerate(forces):
        ruler = by_id.get(force["ruler"])
        if int(force["id"]) != index: fail("forces.tsv row %d has the id %s" % (index, force["id"]))
        if not ruler or ruler["force"] != force["id"] or ruler["status"] != "ruler": fail("forces.tsv id %s: the ruler %s is not a ruler of the force" % (force["id"], force["ruler"]))
    rulers = {f["ruler"] for f in forces}
    for row in officers:
        if row["status"] == "ruler" and row["id"] not in rulers: fail("officers.tsv id %s: a ruler that forces.tsv does not name" % row["id"])

    if len(cities) != CITY_COUNT: fail("cities.tsv needs %d rows" % CITY_COUNT)
    owned = set()
    for index, city in enumerate(cities):
        where = "cities.tsv id %s" % city["id"]
        if int(city["id"]) != index: fail(where + ": the ids must be 0 to 41 in order")
        if city["owner"] != "-" and int(city["owner"]) >= len(forces): fail(where + ": the owner is not a force")
        owned.add(city["owner"])
        governor = by_id.get(city["governor"])
        if city["governor"] != "-" and (not governor or governor["force"] != city["owner"] or governor["city"] != city["id"]):
            fail(where + ": the governor must be an officer of the owner in this city")
    for row in officers:
        if row["force"] != "-" and cities[int(row["city"])]["owner"] != row["force"]:
            fail("officers.tsv id %s: the officer is in a city of another force" % row["id"])
    for force in forces:
        if force["id"] not in owned: fail("forces.tsv id %s: the force has no city" % force["id"])

    out = lines[:head + 1]
    for line in lines[head + 1:]:
        if not line or line.startswith("#"): out.append(line); continue
        row = by_id[line.split("\t")[0]]
        out.append("\t".join(row[c] for c in columns))
    with open(os.path.join(folder, "officers.tsv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    names = write_names(folder, officers, forces)
    print("%d forces, %d officers (%d free), %d cities (%d without an owner), %d English names" % (
        len(forces), len(officers), sum(r["force"] == "-" for r in officers), len(cities), sum(c["owner"] == "-" for c in cities), names))


main()
