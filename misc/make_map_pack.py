"""Writes the map tables of the placeholder pack from spec-src/spec/fixtures/map.

Run it from any folder after the spec-src submodule changes:
    python misc/make_map_pack.py
hexes.tsv and bases.tsv go to data/placeholder/map/ as they are.
movement_costs.tsv gets the form of the pack: one column for each terrain
key of data/placeholder/terrain.tsv, in the order of the terrain ids.
The feature tests compare the loaded pack with the spec fixtures.
"""
import os
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPEC = os.path.join(ROOT, "spec-src", "spec", "fixtures", "map")
PACK = os.path.join(ROOT, "data", "placeholder")

# The keys of the 12 armaments, in the order of movement-costs.tsv.
KEYS = ["sword", "pike", "halberd", "crossbow", "horse", "ram", "tower", "catapult",
        "wood_beast", "boat", "tower_ship", "battle_ship"]


def read_tsv(path):
    with open(path, encoding="utf-8") as f:
        rows = [line.rstrip("\r\n").split("\t") for line in f if line.strip() and not line.startswith("#")]
    return rows[0], rows[1:]


def main():
    os.makedirs(os.path.join(PACK, "map"), exist_ok=True)
    for name in ("hexes.tsv", "bases.tsv"):
        shutil.copyfile(os.path.join(SPEC, name), os.path.join(PACK, "map", name))

    t_head, t_rows = read_tsv(os.path.join(PACK, "terrain.tsv"))
    terrain_keys = [row[t_head.index("key")] for row in t_rows]
    terrain_names = [row[t_head.index("name_zh")] for row in t_rows]

    m_head, m_rows = read_tsv(os.path.join(SPEC, "movement-costs.tsv"))
    if len(m_rows) != len(KEYS):
        sys.exit("movement-costs.tsv: %d rows, expected %d" % (len(m_rows), len(KEYS)))
    out = ["\t".join(["id", "key", "name_zh", "stat_a", "stat_b", "move"] + terrain_keys + ["status"]),
           "# The 12 armaments of fixtures/map/movement-costs.tsv (code): the movement allowance (stat_c) and the cost to enter each terrain of terrain.tsv. 255 = the armament cannot enter.",
           "# stat_a and stat_b: probably attack and defence (Q-199); the engine does not read them. misc/make_map_pack.py writes this file from the spec fixture."]
    for index, row in enumerate(m_rows):
        if int(row[m_head.index("armament")]) != index:
            sys.exit("movement-costs.tsv: row %d has armament %s" % (index, row[0]))
        costs = []
        for name in terrain_names:
            if name not in m_head:
                sys.exit("movement-costs.tsv has no column for the terrain %s" % name)
            costs.append(row[m_head.index(name)])
        out.append("\t".join([str(index), KEYS[index], row[m_head.index("name")], row[m_head.index("stat_a")],
                              row[m_head.index("stat_b")], row[m_head.index("stat_c")]] + costs + ["code"]))
    with open(os.path.join(PACK, "movement_costs.tsv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    print("wrote movement_costs.tsv: %d armaments, %d terrains; copied hexes.tsv and bases.tsv" % (len(m_rows), len(terrain_keys)))


main()
