"""Writes officers.tsv and the lives of a pack scenario from a table of tiers.

    python -X utf8 misc/make_tier_officers.py data/han_196/scenarios/y196

The scenario folder holds source/officers.tsv. Each row gives the facts of one
historical officer and five tier letters for the five abilities. This tool turns
the letters into numbers with the rubric below. Thus each number of the pack comes
from a written rule and from the history, and not from the table of another game.

The rubric:
- tiers: the letters S, A, B, C, D, E and F for leadership, war, intelligence,
  politics and charm. TIER_VALUES gives the base of each letter.
- spread: each ability gets a fixed offset from -3 to +3. The offset comes from
  the CRC32 of the key and the ability. Two officers of one tier thus differ.
- aptitudes: the base letter comes from the best of leadership and war. The
  region of the officer adds one step to its arm. An officer with intelligence
  S or A adds one step to siege and to crossbow. Only an officer with war S or
  leadership S can get the letter S.
- loyalty: ruler and kin 100, core 99, new 98, free 0. A guest has 65, because the history gives a conflict with the ruler.
- merit: a ruler has 15000. The others have 8000, 6000, 4000 or 2000 for the best
  tier S, A, B or lower. A free officer has 0.
- compatibility: the province of the home of the officer gives a base on the
  scale 0 to 149. Neighbour provinces have near values. The spread adds -3 to +3.

The tool writes officers.tsv of the scenario and lore/en.tsv of the pack. Then run
misc/make_scenario.py on the same folder. It writes the Big5 names and checks the rows.
"""
import os
import sys
import zlib

TIER_VALUES = {"S": 95, "A": 86, "B": 76, "C": 65, "D": 52, "E": 38, "F": 25}
ABILITIES = ("leadership", "war", "intelligence", "politics", "charm")
ARMS = ("pike", "halberd", "crossbow", "cavalry", "siege", "navy")
APTITUDE_LETTERS = "CBAS"
REGION_ARM = {"n": "cavalry", "w": "cavalry", "c": "pike", "s": "navy", "y": "crossbow"}
LOYALTY = {"ruler": 100, "kin": 100, "core": 99, "new": 98, "guest": 65, "free": 0}
MERIT = {"S": 8000, "A": 6000, "B": 4000}
HOME_COMPATIBILITY = {
    "you": 8, "ji": 18, "bing": 28, "qing": 38, "yan": 48, "yu": 58, "xu": 68,
    "yang": 80, "jing": 92, "jiao": 102, "yi": 112, "liang": 124, "si": 136,
}
OUT_COLUMNS = ("id", "key", "family_zh", "given_zh", "name_en", "native_lang", "name_native", "name_latin",
               "wikipedia", "family_big5", "given_big5", "female", "birth", "death", "appear", "force", "city",
               "status", "leadership", "war", "intelligence", "politics", "charm", "aptitudes", "skill",
               "loyalty", "merit", "compatibility")


def fail(message):
    sys.exit("make_tier_officers: " + message)


def spread(key, part):
    return zlib.crc32(("%s:%s" % (key, part)).encode("utf-8")) % 7 - 3


def read_rows(path):
    with open(path, encoding="utf-8") as f:
        lines = [line.rstrip("\r\n") for line in f]
    body = [line for line in lines if line and not line.startswith("#")]
    columns = body[0].split("\t")
    return [dict(zip(columns, line.split("\t"))) for line in body[1:]]


def best_tier(tiers):
    return min(tiers, key=lambda letter: -TIER_VALUES[letter])


def aptitudes(row, tiers):
    martial = max(TIER_VALUES[tiers[0]], TIER_VALUES[tiers[1]])
    base = 2 if martial >= TIER_VALUES["S"] else 1 if martial >= TIER_VALUES["B"] else 0
    steps = dict.fromkeys(ARMS, base)
    region_arm = REGION_ARM[row["region"]]
    steps[region_arm] += 1
    if tiers[2] in "SA":
        steps["siege"] += 1
        steps["crossbow"] += 1
    top = 3 if "S" in tiers[:2] else 2
    return "".join(APTITUDE_LETTERS[min(steps[arm], top)] for arm in ARMS)


def officer_row(index, row):
    tiers = row["tiers"]
    if len(tiers) != 5 or any(letter not in TIER_VALUES for letter in tiers):
        fail("%s: tiers must be five letters of SABCDEF" % row["key"])
    if row["region"] not in REGION_ARM: fail("%s: the region %s is not known" % (row["key"], row["region"]))
    if row["home"] not in HOME_COMPATIBILITY: fail("%s: the home %s is not known" % (row["key"], row["home"]))
    if row["bond"] not in LOYALTY: fail("%s: the bond %s is not known" % (row["key"], row["bond"]))
    values = [max(1, min(100, TIER_VALUES[letter] + spread(row["key"], ability))) for letter, ability in zip(tiers, ABILITIES)]
    status = row["status"]
    bond = "free" if status == "free" else "ruler" if status == "ruler" else row["bond"]
    merit = 15000 if status == "ruler" else 0 if status == "free" else MERIT.get(best_tier(tiers), 2000)
    compatibility = max(0, min(149, HOME_COMPATIBILITY[row["home"]] + spread(row["key"], "home")))
    wiki = row["wiki"]
    link = "-" if wiki == "-" else "https://en.wikipedia.org/wiki/" + wiki
    native = row["family_zh"] + row["given_zh"]
    fields = [index, row["key"], row["family_zh"], row["given_zh"], row["name_en"], "zh", native,
              row["name_en"].replace("'", ""), link, "-", "-", row["female"], row["birth"], row["death"],
              row["appear"], row["force"], row["city"], status] + values + [
              aptitudes(row, tiers), row["skill"], LOYALTY[bond], merit, compatibility]
    return "\t".join(str(field) for field in fields)


def write(path, header, comments, rows):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join([header] + comments + rows) + "\n")


def main():
    if len(sys.argv) != 2: sys.exit(__doc__)
    folder = sys.argv[1]
    rows = read_rows(os.path.join(folder, "source", "officers.tsv"))
    officers = [officer_row(index, row) for index, row in enumerate(rows)]
    write(os.path.join(folder, "officers.tsv"), "\t".join(OUT_COLUMNS), [
        "# Written by misc/make_tier_officers.py from source/officers.tsv. Do not edit this file. Edit the source and run the tool again.",
        "# The names, the years, the factions and the cities follow the history. The abilities come from the tiers of the source and the rubric of the tool.",
        "# force: the id of forces.tsv, or - for a free officer. city: the city id of the map (0 to 41). status: ruler, officer or free.",
        "# aptitudes: pike, halberd, crossbow, cavalry, siege, navy; C, B, A or S. skill: a key of skills.tsv, or -. compatibility: 0 to 149.",
    ], officers)
    pack = os.path.dirname(os.path.dirname(os.path.abspath(folder)))
    os.makedirs(os.path.join(pack, "lore"), exist_ok=True)
    lives = ["officer.%s\t%s\t-" % (row["key"], row["life"]) for row in rows]
    write(os.path.join(pack, "lore", "en.tsv"), "key\ttext\tnote", [
        "# The life of each officer of the scenario, keyed by officer.<key> (docs/EUROPE.md, E4).",
        "# Written by misc/make_tier_officers.py from the column life of source/officers.tsv. The pack wrote each life in its own words.",
    ], lives)
    print("make_tier_officers: %d officers" % len(officers))


if __name__ == "__main__":
    main()
