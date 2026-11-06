"""The summary of a play test (docs/PLAYTEST.md).

The script reads the logs of tests/web/playtest/match.js and writes summary.json:
the series of each faction, the eliminations, the pace and the kinds of the captions.
Usage: python -X utf8 -I tests/web/playtest/analyze.py build/playtest/match1
"""
import collections
import json
import pathlib
import sys

# The names of the rulers in the list of the 194 scenario, by force id.
RULERS = {9: "Zhang Lu", 15: "Zhang Yang", 8: "Yuan Shu", 2: "Sun Ce", 10: "Lü Bu", 11: "Gongsun Zan",
          18: "Yan Baihu", 3: "Ma Teng", 1: "Liu Bei", 7: "Yuan Shao", 4: "Liu Zhang", 6: "Liu Biao"}
HUMANS = {1: "Liu Bei (Claude)", 10: "Lü Bu (Rival)"}


def rows(folder, name):
    path = folder / name
    if not path.exists():
        return []
    return [json.loads(line) for line in path.read_text(encoding="utf8").splitlines() if line.strip()]


def caption_kind(what):
    text = what.split("：")[-1]
    for key, kind in (("行進", "march"), ("出城", "sortie"), ("士兵 +", "soldiers gained"), ("士兵 -", "soldiers lost"),
                      ("受損", "facility damaged"), ("攻", "attack"), ("佔領", "capture"), ("俘", "captive"), ("計", "plot")):
        if key in text:
            return kind
    return "other"


def main():
    folder = pathlib.Path(sys.argv[1])
    turns = rows(folder, "turns.jsonl")
    captions = rows(folder, "captions.jsonl")
    driver = rows(folder, "driver.jsonl")
    by_turn = {}
    for row in turns:
        by_turn[row["turn"]] = row   # the last row of each turn
    ordered = [by_turn[key] for key in sorted(by_turn)]

    series = collections.defaultdict(list)
    for row in ordered:
        for faction in row["factions"]:
            series[faction["force"]].append({"turn": row["turn"], "date": row["date"], "cities": faction["cities"],
                                             "troops": faction["troops"], "gold": faction["gold"], "food": faction["food"],
                                             "officers": faction["officers"], "units": faction["units"], "field": faction["field"]})
    eliminations = []
    for force, points in series.items():
        alive = [point for point in points if point["cities"] > 0]
        if alive and alive[-1] is not points[-1]:
            gone = next(point for point in points if point["turn"] > alive[-1]["turn"])
            eliminations.append({"force": force, "turn": gone["turn"], "date": gone["date"]})
    # A faction that leaves the list of the engine is also gone.
    last_forces = {faction["force"] for faction in ordered[-1]["factions"]} if ordered else set()
    for force, points in series.items():
        if force not in last_forces and not any(item["force"] == force for item in eliminations):
            eliminations.append({"force": force, "turn": points[-1]["turn"] + 1, "date": "after " + points[-1]["date"]})

    desyncs = [row for row in turns if row["checksum"] != row["guest_checksum"] and row["steps"] == row["guest_steps"]]
    errors = [row for row in turns if row.get("net_error")]
    pace = []
    for before, after in zip(ordered, ordered[1:]):
        pace.append({"turn": after["turn"], "seconds": round((after["t"] - before["t"]) / 1000 / max(1, after["turn"] - before["turn"]), 2)})
    summary = {
        "names": {str(force): HUMANS.get(force, RULERS.get(force, f"force {force}")) for force in series},
        "humans": list(HUMANS),
        "first": ordered[0]["date"] if ordered else "", "last": ordered[-1]["date"] if ordered else "",
        "turns": len(ordered), "steps": ordered[-1]["steps"] if ordered else 0,
        "wall_minutes": round((ordered[-1]["t"] - ordered[0]["t"]) / 60000, 1) if ordered else 0,
        "desync_rows": len(desyncs), "net_errors": len(errors),
        "series": {str(force): points for force, points in series.items()},
        "eliminations": sorted(eliminations, key=lambda item: item["turn"]),
        "pace": pace,
        "caption_kinds": collections.Counter(caption_kind(row["what"]) for row in captions).most_common(),
        "caption_count": len(captions), "caption_last_turn": captions[-1]["turn"] if captions else 0,
        "driver": [row["text"] for row in driver],
    }
    (folder / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=1), encoding="utf8")
    print(f"{summary['turns']} turns, {summary['first']} to {summary['last']}, {summary['wall_minutes']} min, "
          f"desync rows {summary['desync_rows']}, eliminations {len(summary['eliminations'])}")
    for force, points in sorted(series.items(), key=lambda item: -item[1][-1]["cities"]):
        print(f"  {summary['names'][str(force)]:>18}: cities {points[0]['cities']:>2} -> {points[-1]['cities']:>2}, "
              f"troops {points[0]['troops']:>7} -> {points[-1]['troops']:>7}")


if __name__ == "__main__":
    main()
