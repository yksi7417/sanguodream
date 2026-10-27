"""Imports the new 3D models of the shared Drive folder into the pack.

The user makes the models with Meshy and puts each GLB in the Drive
folder "Three Kingdoms Animations — Map Art"/07-3D-Models (docs/ASSETS.md,
"Base models"). The owner shares the folder by link, so the script needs
no sign-in. This script does these steps:

1. It reads the list of the folder from the embedded view of Drive.
2. It finds the model name of each GLB from the start of its file name
   (MODEL_NAMES). It skips a file without a model name.
3. It skips a GLB that provenance.tsv names as the source of a model.
4. It downloads each other GLB into a new, empty folder and counts its
   triangles. The state file keeps the count, so a second run does not
   download the file again. The script only reads a download. It never
   runs a download.
5. A GLB with more than --max-triangles triangles stays out.
6. If a model name already has a model, a new GLB stays out, unless you
   give --replace NAME.
7. For each model name, it imports the GLB with the most triangles under
   the limit (misc/glb_import.py). Then it writes the row of the model in
   provenance.tsv.

Run it from the root of the repository. Do not give -I to Python, because
glb_import.py needs Pillow from the site packages:

    python -X utf8 -E misc\\drive_models.py [--list] [--replace NAME]
        [--max-triangles N] [--pack DIR] [--work DIR]

--list prints the folder and the decision for each GLB, and changes
nothing. The work folder (default art-src/models/drive, out of git) holds
the downloads and the state file. After an import, do these steps:

1. Add the model to the table in docs/ASSETS.md.
2. Run build.bat and run_tests.bat.
3. Look at the model in the game. If its front faces the wrong way, run
   glb_import.py again with --yaw.
"""

import datetime
import html
import os
import re
import sys
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import glb_import  # noqa: E402  (the importer is in the same folder)

FOLDER_ID = "1ET0ml0BmUF8_yvlmf-J5rZhJhx8n2wyG"
FOLDER_NAME = 'Google Drive "Three Kingdoms Animations — Map Art"/07-3D-Models'
LIST_URL = "https://drive.google.com/embeddedfolderview?id={}"
DOWNLOAD_URL = "https://drive.usercontent.google.com/download?id={}&export=download&confirm=t"
MAX_TRIANGLES = 40000   # docs/ASSETS.md: the limit for a new model
MODEL_SOURCE = "Meshy (free plan)"
MODEL_LICENCE = 'CC BY 4.0, credit "Meshy"'

# The start of the file name and the model name. The first match counts.
# The markets of level 2 and 3 have no model name yet: the scene gives
# the renderer no level (follow-up in docs/ASSETS.md).
MODEL_NAMES = [
    ("big-city", "base_big_city"),
    ("city", "base_city"),
    ("gate", "base_gate"),
    ("port", "base_port"),
    ("market-lv1", "facility_market"),
    ("farm-lv1", "facility_farm"),
]

ENTRY = re.compile(r'<div class="flip-entry" id="entry-([^"]+)".*?<div class="flip-entry-title">([^<]*)</div>', re.S)


def model_name_of(title):
    """Returns the model name of a GLB file name, or None."""
    stem = title.lower()
    if not stem.endswith(".glb"):
        return None
    stem = stem[:-4]
    for prefix, name in MODEL_NAMES:
        if stem == prefix or (stem.startswith(prefix) and not stem[len(prefix)].isalnum()):
            return name
    return None


def list_folder():
    """Returns (file id, title) for each file of the Drive folder."""
    with urllib.request.urlopen(LIST_URL.format(FOLDER_ID), timeout=60) as response:
        page = response.read().decode("utf-8", "replace")
    entries = [(match.group(1), html.unescape(match.group(2)).strip()) for match in ENTRY.finditer(page)]
    if not entries:
        raise glb_import.GlbError("the folder list is empty. Share the folder with 'anyone with the link'.")
    return entries


def read_provenance(path):
    """Returns the lines of provenance.tsv, the models and the source GLBs."""
    with open(path, encoding="utf-8") as source:
        lines = source.read().splitlines()
    models, sources = set(), set()
    for line in lines[1:]:
        if not line or line.startswith("#"):
            continue
        columns = line.split("\t")
        models.add(columns[0])
        sources.add(columns[1].split(",")[0].strip())
    return lines, models, sources


def write_provenance(path, lines, name, title, triangles):
    """Replaces the row of the model, or adds a row at the end."""
    row = "\t".join([name, f"{title}, {FOLDER_NAME}", MODEL_SOURCE, datetime.date.today().isoformat(),
                     MODEL_LICENCE, f"{triangles} triangles, the base colour texture at {glb_import.TEXTURE_MAX} pixels"])
    for index, line in enumerate(lines):
        if index > 0 and line.split("\t")[0] == name:
            lines[index] = row
            break
    else:
        lines.append(row)
    with open(path, "w", encoding="utf-8", newline="\n") as target:
        target.write("\n".join(lines) + "\n")


def read_state(path):
    """Returns the triangle count of each GLB that the script downloaded."""
    state = {}
    if os.path.exists(path):
        with open(path, encoding="utf-8") as source:
            for line in source.read().splitlines():
                file_id, title, triangles = line.split("\t")
                state[file_id] = (title, int(triangles))
    return state


def write_state(path, state):
    with open(path, "w", encoding="utf-8", newline="\n") as target:
        for file_id, (title, triangles) in sorted(state.items(), key=lambda item: item[1][0]):
            target.write(f"{file_id}\t{title}\t{triangles}\n")


def download(file_id, title, work):
    """Downloads one GLB into its own new, empty folder. Returns the path."""
    folder = os.path.join(work, file_id)
    if os.path.isdir(folder) and os.listdir(folder):
        raise glb_import.GlbError(f"{folder} is not empty. Remove it and run the script again.")
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, os.path.basename(title))
    with urllib.request.urlopen(DOWNLOAD_URL.format(file_id), timeout=600) as response, open(path, "wb") as target:
        while True:
            block = response.read(1 << 20)
            if not block:
                break
            target.write(block)
    return path


def count_triangles(path):
    """The triangle count of the scene of a GLB, as glb_import.py collects it."""
    document, _ = glb_import.read_glb(path)
    scene = document["scenes"][document.get("scene", 0)] if document.get("scenes") else {"nodes": list(range(len(document.get("nodes", []))))}
    stack, triangles = list(scene["nodes"]), 0
    while stack:
        node = document["nodes"][stack.pop()]
        stack.extend(node.get("children", []))
        if "mesh" not in node:
            continue
        for primitive in document["meshes"][node["mesh"]]["primitives"]:
            if primitive.get("mode", 4) != 4:
                continue
            accessor = primitive["indices"] if "indices" in primitive else primitive["attributes"]["POSITION"]
            triangles += document["accessors"][accessor]["count"] // 3
    return triangles


def option(arguments, flag, default):
    if flag not in arguments:
        return default
    at = arguments.index(flag)
    return arguments[at + 1]


def main(arguments):
    only_list = "--list" in arguments
    replace = {arguments[index + 1] for index, value in enumerate(arguments) if value == "--replace"}
    max_triangles = int(option(arguments, "--max-triangles", MAX_TRIANGLES))
    pack = option(arguments, "--pack", os.path.join("data", "placeholder"))
    work = option(arguments, "--work", os.path.join("art-src", "models", "drive"))
    provenance_path = os.path.join(pack, "models", "provenance.tsv")
    state_path = os.path.join(work, "state.tsv")
    try:
        entries = list_folder()
        lines, models, sources = read_provenance(provenance_path)
    except (OSError, glb_import.GlbError) as error:
        print(f"error: {error}")
        return 1
    os.makedirs(work, exist_ok=True)
    state = read_state(state_path)

    best = {}   # model name: (triangles, file id, title)
    for file_id, title in entries:
        name = model_name_of(title)
        if name is None:
            print(f"skip   {title}: no model name")
            continue
        if title in sources:
            print(f"have   {title}: the source of a model ({name})")
            continue
        if file_id not in state:
            if only_list:
                print(f"new    {title}: {name}, not downloaded yet")
                continue
            try:
                path = download(file_id, title, work)
                state[file_id] = (title, count_triangles(path))
            except (OSError, glb_import.GlbError) as error:
                print(f"error  {title}: {error}")
                continue
            write_state(state_path, state)
        triangles = state[file_id][1]
        if triangles > max_triangles:
            print(f"skip   {title}: {triangles} triangles, more than {max_triangles}")
        elif name in models and name not in replace:
            print(f"skip   {title}: {name} has a model. Give --replace {name} to replace it ({triangles} triangles)")
        else:
            print(f"pick   {title}: {name}, {triangles} triangles")
            if name not in best or triangles > best[name][0]:
                best[name] = (triangles, file_id, title)

    if only_list:
        return 0
    for name, (triangles, file_id, title) in sorted(best.items()):
        path = os.path.join(work, file_id, os.path.basename(title))
        try:
            glb_import.import_model(path, name, pack, 0.0)
        except (OSError, glb_import.GlbError) as error:
            print(f"error  {title}: {error}")
            continue
        write_provenance(provenance_path, lines, name, title, triangles)
        print(f"import {title} -> {pack}/models/{name}.hxm and .png")
    if best:
        print("Next: add each model to docs/ASSETS.md, run build.bat and run_tests.bat, and look at the model in the game.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
