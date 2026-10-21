"""Writes data/placeholder/pairs.tsv from spec-src/spec/fixtures/saves/pairs.

Run it from any folder after the spec-src submodule changes:
    python -X utf8 misc/make_pair_index.py
Each pair folder <rule>/<n> gives one row: the command of pair.json in the
ids of the pack. The engine reads the table (M13, tutorial 26). A command
that the engine cannot replay gets the command "-". The feature test then
fails for that pair.

pair.json names the city and the facility, but not the officers of the
command. The script takes the officers whose acted flag goes from 0 to 1
in diff.json, in the order of their ids (assumed; see the M13 status).
"""
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PAIRS = os.path.join(ROOT, "spec-src", "spec", "fixtures", "saves", "pairs")
PACK = os.path.join(ROOT, "data", "placeholder")
EXE_TO_PAIRS = "..\\spec-src\\spec\\fixtures\\saves\\pairs"

# The city commands of the 都市 menu that the engine can replay.
CITY_COMMANDS = {"徵兵": "conscript", "巡察": "patrol", "訓練": "train", "商人": "merchant",
                 "conscript": "conscript", "patrol": "patrol", "train": "train", "merchant": "merchant"}
OFFICER_PLACES = 3
ACTED = re.compile(r"^officers\[(\d+)\]\.acted$")
CITY_FIELD = re.compile(r"^cities\[(\d+)\]\.")
MERCHANT_SELL, MERCHANT_BUY = 1, 0   # sim_merchant_mode


def diff_city(diff):
    """The city of a command: the first city whose fields change in diff.json.
    The pairs of 2026-10-20 name the city with an ASCII key, not with its name."""
    for change in diff.get("changes", []):
        match = CITY_FIELD.match(change.get("path", ""))
        if match:
            return match.group(1)
    return None


def merchant_trade(diff):
    """(mode, amount) of a 商人 trade from the grain and gold of the city."""
    for change in diff.get("changes", []):
        path = change.get("path", "")
        if CITY_FIELD.match(path) and path.endswith(".food") and change["after"] < change["before"]:
            return MERCHANT_SELL, change["before"] - change["after"]
        if CITY_FIELD.match(path) and path.endswith(".gold") and change["after"] < change["before"]:
            return MERCHANT_BUY, change["before"] - change["after"]
    return None, None


def read_tsv(path):
    with open(path, encoding="utf-8") as f:
        rows = [line.rstrip("\r\n").split("\t") for line in f if line.strip() and not line.startswith("#")]
    return rows[0], rows[1:]


def column(head, rows, key, value):
    return {row[head.index(key)]: row[head.index(value)] for row in rows}


def acted_officers(diff):
    officers = []
    for change in diff.get("changes", []):
        match = ACTED.match(change.get("path", ""))
        if match and change.get("before") == 0 and change.get("after") == 1:
            officers.append(int(match.group(1)))
    return sorted(officers)[:OFFICER_PLACES]


def command_of(pair, diff, city_ids, facility_ids):
    """Returns (command, city, facility, officers, mode, amount) in the ids of the pack.
    The arguments name the city and the facility in Chinese (pairs of 2026-10-18) or
    with ASCII keys (pairs of 2026-10-20). An ASCII city key takes the city of diff.json."""
    kind, arguments = pair.get("command", ""), pair.get("arguments", [])
    city = city_ids.get(arguments[0]) if arguments else None
    if city is None:
        city = diff_city(diff)
    if kind == "end-turn":
        return "end_turn", "-", "-", [], "-", "-"
    if kind == "develop" and len(arguments) >= 2 and city is not None and arguments[1] in facility_ids:
        return "develop", city, facility_ids[arguments[1]], acted_officers(diff), "-", "-"
    if kind == "city-cmd" and len(arguments) >= 3 and city is not None and arguments[2] in CITY_COMMANDS:
        command = CITY_COMMANDS[arguments[2]]
        if command == "merchant":
            mode, amount = merchant_trade(diff)
            if mode is None:
                return "-", "-", "-", [], "-", "-"
            return command, city, "-", acted_officers(diff), str(mode), str(amount)
        return command, city, "-", acted_officers(diff), "-", "-"
    return "-", "-", "-", [], "-", "-"


def main():
    b_head, b_rows = read_tsv(os.path.join(PACK, "map", "bases.tsv"))
    city_ids = {row[b_head.index("name")]: row[b_head.index("id")] for row in b_rows if row[b_head.index("kind")] == "city"}
    f_head, f_rows = read_tsv(os.path.join(PACK, "facilities.tsv"))
    facility_ids = column(f_head, f_rows, "name_zh", "id")
    facility_ids.update(column(f_head, f_rows, "key", "id"))

    out = ["\t".join(["id", "rule", "number", "command", "city", "facility", "officers", "mode", "amount", "q_ids", "folder"]),
           "# The save pairs of spec/fixtures/saves/pairs (M13). misc/make_pair_index.py writes this file from pair.json and diff.json.",
           "# command: end_turn, develop, conscript, patrol, train or merchant; - if the engine has no handler. city and facility: the ids of bases.tsv and facilities.tsv.",
           "# officers: the officers whose acted flag rises in diff.json (assumed: pair.json does not name them). folder: relative to the executable.",
           "# mode and amount: the merchant trade from diff.json (mode 1 sells grain, 0 spends gold); - for the other commands."]
    row_id = 0
    for rule in sorted(os.listdir(PAIRS)):
        rule_folder = os.path.join(PAIRS, rule)
        if not os.path.isdir(rule_folder):
            continue
        for number in sorted(os.listdir(rule_folder)):
            folder = os.path.join(rule_folder, number)
            if not os.path.isfile(os.path.join(folder, "pair.json")):
                continue
            with open(os.path.join(folder, "pair.json"), encoding="utf-8") as f:
                pair = json.load(f)
            with open(os.path.join(folder, "diff.json"), encoding="utf-8") as f:
                diff = json.load(f)
            command, city, facility, officers, mode, amount = command_of(pair, diff, city_ids, facility_ids)
            out.append("\t".join([str(row_id), rule, str(int(number)), command, city, facility,
                                  ",".join(str(o) for o in officers) or "-", mode, amount,
                                  ",".join(pair.get("q_ids", [])) or "-",
                                  EXE_TO_PAIRS + "\\" + rule + "\\" + number]))
            row_id += 1
    with open(os.path.join(PACK, "pairs.tsv"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out) + "\n")
    print("wrote pairs.tsv: %d pairs" % row_id)


if __name__ == "__main__":
    sys.exit(main())
