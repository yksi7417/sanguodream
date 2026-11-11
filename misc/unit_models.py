"""Makes the 3D unit models of the Three Kingdoms art on the local model server.

The source of each model is a Gemini model sheet of the owner (Drive folder
01-Units, MS-01 to MS-14, docs/art/GEMINI-PROMPTS.md). The script does these
steps for each class of docs/units/UNIT-JOBS.tsv:

1. It cuts the front view of the sheet at full size, with the art_import
   functions. The green becomes transparent. The red faction paint stays
   opaque red, so that the texture keeps it for the faction colour.
2. It copies the cut picture to the server and starts TRELLIS.2 as
   image-to-3D (mesh.py), detached from the SSH connection.
3. It collects the GLB and imports it with glb_import.py into
   data/placeholder/models/unit_<class>.hxm and .jpg, at 512 pixels.
4. It brightens the texture toward the cut picture. Then unit_mask.py paints
   the red faction paint of the cut back onto the texture.

The script writes the state of each job to art-src/units/state.tsv. A new run
continues from that state, as misc/local_art.py does. Only "genai 3d" starts
the stack (~/ai/REMOTE.md on the server). The script commits nothing.

Run it from the root of the repository:

    python -X utf8 -E -P misc\\unit_models.py [--cut-only] [--import-again]

With --cut-only, it writes only the cut pictures to art-src/units/cut.
With --import-again, it imports the finished GLBs again, with no new model.
"""

import csv
import os
import shlex
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import local_art                      # noqa: E402  the server helpers: ssh, state, stack
import art_import                     # noqa: E402  the cut of a model sheet
import glb_import                     # noqa: E402
import unit_mask                      # noqa: E402  the red faction paint back on the texture

from PIL import Image                 # noqa: E402

JOBS_FILE = os.path.join("docs", "units", "UNIT-JOBS.tsv")
WORK = os.path.join("art-src", "units")
SHEETS = os.path.join(WORK, "drive")
CUTS = os.path.join(WORK, "cut")
PACK = os.path.join("data", "placeholder")
REMOTE_OUT = "out/hexdom_units"
INPUT_SIZE = 1024                      # pixels; the side of the square picture for TRELLIS.2
INPUT_MARGIN = 0.08                    # the air around the figure, as a part of the side
MESH_RESOLUTION = 1024
MODEL_TEXTURE = 512
BASE_SEED = 1960
MODEL_LICENCE = "MIT (TRELLIS.2); the source picture: Gemini sheet of the owner"


def read_jobs():
    jobs = []
    with open(JOBS_FILE, encoding="utf-8", newline="") as jobs_file:
        for row in csv.reader(jobs_file, delimiter="\t", quoting=csv.QUOTE_NONE):
            if row and not row[0].startswith("#") and row[0] != "name":
                jobs.append({"name": row[0], "sheet": row[1], "fit": row[2], "faces": int(row[3]), "index": len(jobs)})
    return jobs


def use_unit_folders():
    """Points the helpers of local_art.py to the folders of this script."""
    local_art.WORK = WORK
    local_art.CRASH_LOG = os.path.abspath(os.path.join(WORK, "crash.log"))
    local_art.STATE_FILE = os.path.join(WORK, "state.tsv")
    local_art.REMOTE_OUT = REMOTE_OUT


def cut_front_view(sheet_path, out_path):
    """Writes the front view of a sheet as a square RGBA picture on a transparent background."""
    sheet = Image.open(sheet_path).convert("RGB")
    alpha = art_import.green_alpha(sheet)
    box = art_import.front_view_box(alpha)
    if not box:
        raise RuntimeError("no figure in " + sheet_path)
    rgb = art_import.despill(sheet).crop(box)
    alpha = alpha.crop(box)
    figure = Image.merge("RGBA", rgb.split() + (alpha,))
    width, height = figure.size
    side = int(max(width, height) / (1.0 - 2.0 * INPUT_MARGIN))
    square = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    square.paste(figure, ((side - width) // 2, (side - height) // 2), figure)
    square = square.resize((INPUT_SIZE, INPUT_SIZE), Image.LANCZOS)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    square.save(out_path, optimize=True)
    return out_path


def scp_to_server(local_path, remote_path):
    # A relative local path, because scp reads the "D:" of a full path as a host name.
    arguments = [os.path.join(local_art.GIT_USR, "scp.exe"), "-q", "-S", os.path.join(local_art.GIT_USR, "ssh.exe")]
    arguments += local_art.SSH_OPTIONS + [local_path.replace("\\", "/"), local_art.HOST + ":" + remote_path]
    code, text = local_art.run_tool(arguments, 300)
    if code != 0:
        raise local_art.ServerDown("scp: " + text.strip()[:200])


def start_remote_job(job, seed):
    folder = REMOTE_OUT + "/" + job["name"]
    local_art.ssh("cd %s && mkdir -p %s" % (local_art.REMOTE_REPO, folder))
    scp_to_server(os.path.join(CUTS, job["name"] + ".png"), "%s/%s/input.png" % (local_art.REMOTE_REPO, folder))
    mesh = ("python3 layers/image2mesh/src/mesh.py --image %s/input.png --runner server --res %d --target-faces %d "
            "--seed %d --out-dir %s") % (folder, MESH_RESOLUTION, job["faces"], seed, folder)
    inner = "%s > %s/run.log 2>&1; echo $? > %s/exit" % (mesh, folder, folder)
    command = ("cd %s && rm -f %s/exit && date +%%s > %s/started && "
               "(nohup setsid bash -c %s </dev/null >/dev/null 2>&1 & echo $! > %s/pid)") % (
                   local_art.REMOTE_REPO, folder, folder, shlex.quote(inner), folder)
    code, text = local_art.ssh(command)
    if code != 0:
        raise local_art.ServerDown("start: " + text[:200])


def write_provenance(job, triangles):
    path = os.path.join(PACK, "models", "provenance.tsv")
    with open(path, encoding="utf-8", newline="") as provenance_file:
        lines = [line for line in provenance_file.read().splitlines() if not line.startswith(job["name"] + "\t")]
    row = [job["name"], "misc/unit_models.py, Google Drive 01-Units/" + job["sheet"], "TRELLIS.2 on the local server",
           time.strftime("%Y-%m-%d"), MODEL_LICENCE, "%d triangles, the front view of the sheet" % triangles]
    lines.append("\t".join(row))
    header, body = lines[:2], sorted(lines[2:])
    with open(path + ".tmp", "w", encoding="utf-8", newline="") as provenance_file:
        provenance_file.write("\n".join(header + body) + "\n")
    os.replace(path + ".tmp", path)


def import_unit(job, glb_path):
    glb_import.import_model(glb_path, job["name"], PACK, 0.0, job["fit"])
    texture_path = os.path.join(PACK, "models", job["name"] + ".png")
    with Image.open(texture_path) as image:
        small = image.convert("RGB")
        if small.width > MODEL_TEXTURE:
            small = small.resize((MODEL_TEXTURE, MODEL_TEXTURE), Image.LANCZOS)
    small.save(texture_path, optimize=True)
    cut_path = os.path.join(CUTS, job["name"] + ".png")
    local_art.brighten_toward_source(texture_path, cut_path)
    camera, match, red_texels = unit_mask.paint_faction_mask(
        os.path.join(PACK, "models", job["name"] + ".hxm"), texture_path, cut_path)
    print("%s: camera yaw %d pitch %d mirror %s, overlap %.2f, %d red texels"
          % (job["name"], camera[0], camera[1], camera[2], match, red_texels))
    local_art.write_jpeg_texture(texture_path)


def collect_model(job, state):
    folder = REMOTE_OUT + "/" + job["name"]
    _, listing = local_art.ssh("cd %s/%s && ls -t *.glb" % (local_art.REMOTE_REPO, folder))
    glbs = [name for name in listing.split() if name.endswith(".glb")]
    if not glbs:
        return False
    glb_path = os.path.join(WORK, "glb", job["name"] + ".glb")
    os.makedirs(os.path.dirname(glb_path), exist_ok=True)
    local_art.scp_from_server("%s/%s/%s" % (local_art.REMOTE_REPO, folder, glbs[0]), glb_path)
    import_unit(job, glb_path)
    document, _ = glb_import.read_glb(glb_path)
    triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                    for mesh in document["meshes"] for primitive in mesh["primitives"])
    write_provenance(job, triangles)
    model = os.path.join(PACK, "models", job["name"])
    size = os.path.getsize(model + ".hxm") + os.path.getsize(model + ".jpg")
    local_art.set_state(state, job["name"], "done", "%d triangles, %d KB" % (triangles, size // 1024))
    return True


def do_job(job, state):
    entry = state.setdefault(job["name"], {"status": "pending", "attempts": 0, "note": ""})
    local_art.ensure_stack("3d")
    while True:
        kind, value = local_art.remote_job_state(job)
        if kind == "running":
            if value > local_art.JOB_TIMEOUT_SECONDS:
                local_art.log("%s: timeout, stop the remote job" % job["name"])
                local_art.ssh("cd %s/%s/%s && kill -- -$(cat pid); rm -f pid" % (local_art.REMOTE_REPO, REMOTE_OUT, job["name"]))
                continue
            time.sleep(local_art.POLL_SECONDS)
            continue
        if kind == "exit" and value == 0 and collect_model(job, state):
            return
        if entry["attempts"] >= local_art.MAX_ATTEMPTS:
            local_art.set_state(state, job["name"], "failed", "no model after %d attempts" % entry["attempts"])
            return
        entry["attempts"] += 1
        local_art.write_state(state)
        local_art.ensure_stack("3d")
        local_art.log("%s: start, attempt %d" % (job["name"], entry["attempts"]))
        start_remote_job(job, BASE_SEED + job["index"] * 10 + entry["attempts"])
        time.sleep(local_art.POLL_SECONDS)


def cut_all(jobs):
    for job in jobs:
        out_path = cut_front_view(os.path.join(SHEETS, job["sheet"]), os.path.join(CUTS, job["name"] + ".png"))
        print("%s -> %s" % (job["sheet"], out_path))


def main(arguments):
    os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    use_unit_folders()
    jobs = read_jobs()
    if "--cut-only" in arguments:
        cut_all(jobs)
        return 0
    state = local_art.read_state()
    if "--import-again" in arguments:
        for job in jobs:
            if state.get(job["name"], {}).get("status") == "done":
                import_unit(job, os.path.join(WORK, "glb", job["name"] + ".glb"))
        return 0
    cut_all(jobs)
    local_art.log("units: start, pid %d" % os.getpid())
    for job in jobs:
        while state.get(job["name"], {}).get("status") not in ("done", "failed"):
            try:
                do_job(job, state)
            except local_art.ServerDown as error:
                local_art.log("server: %s. Wait %d s." % (error, local_art.RETRY_SECONDS))
                time.sleep(local_art.RETRY_SECONDS)
    done = sum(1 for job in jobs if state.get(job["name"], {}).get("status") == "done")
    local_art.log("units: finished, %d of %d done" % (done, len(jobs)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
