"""Makes the art of the packs of 1200 with the local model server.

The script reads docs/europe/ART-JOBS.tsv and does each job one time. It
writes the state of each job to art-src/europe/generated/state.tsv. A new run
continues from that state, so a restart of this PC or of the server loses at
most one job:

- A 3D job runs on the server, detached from the SSH connection. If this PC
  stops, the job continues, and the next run collects the result.
- If the server stops, the job has no exit file and no process. The next
  check starts the job again.
- A lock file stops a second run. The operating system releases the lock when
  the process stops.

The Windows task HexdomEuropeArt starts this script every 10 minutes. When all
jobs are done or failed, the script removes the task.

Each model goes to data/medieval_europe/models, and each unit sheet goes to
data/medieval_europe/sprites/art. The terrain and the effect pictures stay in
art-src/europe/generated/2d for review. The script commits nothing.

Run it from the root of the repository:

    python -X utf8 -E -P misc\\local_art.py
"""

import os
import site
import sys
import traceback

# Python removes __file__ before it calls the hook, so keep the path here.
CRASH_LOG = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "art-src", "europe", "generated", "crash.log")


def write_crash(kind, value, trace):
    """pythonw has no console, so an uncaught error goes to this file."""
    with open(CRASH_LOG, "a", encoding="utf-8") as crash_file:
        traceback.print_exception(kind, value, trace, file=crash_file)


sys.excepthook = write_crash

# The Claude desktop app is a packaged app, and Windows redirects its AppData writes.
# A pip install from the app is not visible to a scheduled task.
# So this folder on D: holds a copy of Pillow for the task.
site.addsitedir(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "art-src", "europe", "pylib"))

import csv  # noqa: E402
import ctypes  # noqa: E402
import json  # noqa: E402
import msvcrt  # noqa: E402
import shlex  # noqa: E402
import subprocess  # noqa: E402
import time  # noqa: E402
import urllib.parse  # noqa: E402
import urllib.request  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy  # noqa: E402  (in the user site packages)
from PIL import Image  # noqa: E402
import art_import  # noqa: E402  (in the same folder)
import glb_import  # noqa: E402  (in the same folder)

HOST = "yksi7417@100.114.42.13"
COMFY = "http://100.114.42.13:8188"
GIT_USR = r"C:\Program Files\Git\usr\bin"
USER_HOME = "/c/Users/anthony.ai.assistant"
SSH_OPTIONS = ["-i", USER_HOME + "/.ssh/id_ed25519", "-o", "UserKnownHostsFile=" + USER_HOME + "/.ssh/known_hosts",
               "-o", "BatchMode=yes", "-o", "ConnectTimeout=20"]
JOBS_FILE = os.path.join("docs", "europe", "ART-JOBS.tsv")
WORK = os.path.join("art-src", "europe", "generated")
PACK = os.path.join("data", "medieval_europe")
STATE_FILE = os.path.join(WORK, "state.tsv")
TASK_NAME = "HexdomEuropeArt"
REMOTE_REPO = "~/ai/text-to-3D-skill"
REMOTE_OUT = "out/hexdom_eu"
WORKFLOW_2D = "Qwen-Image-2512-BF16-4-Step-LoRA.json"
WORKFLOW_EDIT = "Qwen-Image-Edit-2511-BF16-4-Step-LoRA.json"
SAMPLE_FOLDER = os.path.join("art-src", "europe", "drive")
SHEET_SAMPLES = {"eu-boat": "EU-MS-13-cog.jpg", "eu-wood-beast": "EU-MS-09-battering-ram.jpg"}
EDIT_SHEET = ("Keep exactly the same two-view layout, camera, painting style, line work, palette, flat pure green "
              "background and flat red uniform and shield colour as this image. Keep one view on the left and one on "
              "the right. Replace the subject in both views with: %s. Do not add water, ground, a shadow, a divider, "
              "or text.")
TARGET_FACES = 16000
MESH_RESOLUTION = 1024
TEXTURE_VOLUME = 1024
SMALL_MODEL_TEXTURE = 512
JPEG_QUALITY = 85
MODEL_LICENCE = "Apache-2.0 (FLUX.2 klein 4B), MIT (TRELLIS.2)"
LUMINANCE_WEIGHTS = (0.2126, 0.7152, 0.0722)
BRIGHTNESS_PERCENTILE = 75
STRONGEST_BRIGHTNESS_EXPONENT = 0.5
SUBJECT_DIFFERENCE = 45  # a source pixel with this sum of differences from the background is part of the subject
MAX_ATTEMPTS = 3
JOB_TIMEOUT_SECONDS = 45 * 60
IMAGE_TIMEOUT_SECONDS = 15 * 60
STACK_TIMEOUT_SECONDS = 15 * 60
RETRY_SECONDS = 60
POLL_SECONDS = 20
BASE_SEED = 1200

STYLE_2D = ("Game art asset for a turn-based strategy game set in Europe around 1200 AD. Hand-painted look: gouache "
            "with ink linework on warm paper tones, crisp readable silhouettes, slightly stylised proportions, muted "
            "earth palette of ochre, umber, slate blue and pine green. Soft light from the upper left. ")
UNIT_CAMERA = ("Camera: three-quarter top-down view at about 50 degrees, orthographic. Historically grounded high "
               "medieval equipment: mail hauberks, surcoats, kite shields, nasal helmets. Original design. ")
MASK_RED = ("Paint the whole uniform of every soldier, and every banner, pennant, helmet plume and shield face, in one "
            "flat saturated red, hex #D01818, with no shading inside those areas. Use that red nowhere else. ")
BG_GREEN = ("Background: one flat pure chroma-key green, hex #00FF00, edge to edge. No gradient, no floor, no cast "
            "shadow. Nothing in the subject uses green close to #00FF00. ")
BG_BLACK = "Background: pure black, hex #000000, edge to edge, no gradient. The effect is the only bright thing. "
LAYOUT_SHEET = ("Layout: two views of the same subject side by side, same size, one shared baseline, a wide empty gap "
                "between them, nothing touching the image edge. Left view: three-quarter front. Right view: "
                "three-quarter back. ")
LAYOUT_STRIP = ("Layout: one single horizontal row of frames of the same subject, evenly spaced, each frame the same "
                "size, a wide empty gap between frames, nothing touching the image edge. ")
TILE = ("A seamless tileable ground texture seen from straight above, square, flat even light, no perspective, the "
        "left edge matches the right edge and the top edge matches the bottom edge. Low contrast. No objects larger "
        "than a bush. Ground: ")
NEGATIVE = " Do not include: text, letters, numbers, logos, signatures, frames, borders, photographic realism, 3D render look, anime style, modern objects, gunpowder weapons."
STYLE_3D = (", high medieval Europe around 1200, bright warm sunny colours, brand new, pristine, intact, well "
            "maintained, no damage, no moss, no weathering, miniature game board piece on a small flat patch of green "
            "ground, no people, game asset")


class ServerDown(Exception):
    """The server does not answer. The main loop waits and tries again."""


def log(message):
    line = time.strftime("%Y-%m-%d %H:%M:%S ") + message
    with open(os.path.join(WORK, "run.log"), "a", encoding="utf-8") as log_file:
        log_file.write(line + "\n")


def run_tool(arguments, timeout):
    """Runs a tool without a console window. It returns the exit code and the text."""
    try:
        done = subprocess.run(arguments, capture_output=True, text=True, encoding="utf-8", errors="replace",
                              timeout=timeout, creationflags=subprocess.CREATE_NO_WINDOW)
    except subprocess.TimeoutExpired:
        return 124, ""
    return done.returncode, done.stdout + done.stderr


def ssh(command, timeout=60):
    code, text = run_tool([os.path.join(GIT_USR, "ssh.exe")] + SSH_OPTIONS + [HOST, command], timeout)
    if code == 255 or code == 124:
        raise ServerDown(text.strip()[:200] or "no answer")
    return code, text


def scp_from_server(remote_path, local_path):
    # A relative local path, because scp reads the "D:" of a full path as a host name.
    arguments = [os.path.join(GIT_USR, "scp.exe"), "-q", "-S", os.path.join(GIT_USR, "ssh.exe")] + SSH_OPTIONS
    code, text = run_tool(arguments + [HOST + ":" + remote_path, local_path.replace("\\", "/")], 300)
    if code != 0:
        raise ServerDown("scp: " + text.strip()[:200])


def read_jobs():
    jobs = []
    with open(JOBS_FILE, encoding="utf-8", newline="") as jobs_file:
        for row in csv.reader(jobs_file, delimiter="\t", quoting=csv.QUOTE_NONE):
            if row and not row[0].startswith("#") and row[0] != "name":
                jobs.append({"name": row[0], "kind": row[1], "prompt": row[2], "index": len(jobs)})
    return jobs


def read_state():
    state = {}
    if os.path.isfile(STATE_FILE):
        with open(STATE_FILE, encoding="utf-8", newline="") as state_file:
            for row in csv.reader(state_file, delimiter="\t", quoting=csv.QUOTE_NONE):
                if len(row) == 4 and row[0] != "name":
                    state[row[0]] = {"status": row[1], "attempts": int(row[2]), "note": row[3]}
    return state


def write_state(state):
    temporary = STATE_FILE + ".tmp"
    with open(temporary, "w", encoding="utf-8", newline="") as state_file:
        state_file.write("name\tstatus\tattempts\tnote\n")
        for name, entry in state.items():
            state_file.write("%s\t%s\t%d\t%s\n" % (name, entry["status"], entry["attempts"], entry["note"]))
    os.replace(temporary, STATE_FILE)


def set_state(state, name, status, note=""):
    entry = state.setdefault(name, {"status": "pending", "attempts": 0, "note": ""})
    entry["status"], entry["note"] = status, note
    write_state(state)
    log("%s: %s %s" % (name, status, note))


def stack_is(status_text, mode):
    if mode == "2d":
        return "comfy-2d.service : active" in status_text
    return "t2m-engine(Up" in status_text and "comfy-2d.service : active" not in status_text


def stack_answers(mode):
    urls = ["http://127.0.0.1:8188/system_stats"] + (["http://127.0.0.1:8189/health"] if mode == "3d" else [])
    test = " && ".join("curl -s -f -m 8 -o /dev/null " + url for url in urls)
    return ssh(test)[0] == 0


def ensure_stack(mode):
    """Puts the server on one stack, through genai only (REMOTE.md)."""
    _, status_text = ssh("genai status")
    if stack_is(status_text, mode) and stack_answers(mode):
        return
    if not stack_is(status_text, mode):
        log("server: genai " + mode)
        ssh("genai " + mode, timeout=STACK_TIMEOUT_SECONDS)
    deadline = time.time() + STACK_TIMEOUT_SECONDS
    while time.time() < deadline:
        if stack_answers(mode):
            return
        time.sleep(POLL_SECONDS)
    raise ServerDown("the stack %s does not answer" % mode)


def image_prompt(job):
    if job["kind"] == "sheet":
        return STYLE_2D + UNIT_CAMERA + "Subject: " + job["prompt"] + ". " + MASK_RED + BG_GREEN + LAYOUT_SHEET + NEGATIVE
    if job["kind"] == "terrain":
        return STYLE_2D + TILE + job["prompt"] + "." + NEGATIVE
    if job["name"] == "fx-impact-debris" or job["name"] == "fx-projectiles":
        return STYLE_2D + "Subject: " + job["prompt"] + ". " + BG_GREEN + LAYOUT_STRIP + NEGATIVE
    return STYLE_2D + "Subject: " + job["prompt"] + ". " + BG_BLACK + LAYOUT_STRIP + NEGATIVE


def comfy_json(path, data=None):
    body = json.dumps(data).encode("utf-8") if data is not None else None
    request = urllib.request.Request(COMFY + path, data=body, headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(request, timeout=60) as answer:
            return json.load(answer)
    except OSError as error:
        raise ServerDown(str(error)) from error


def load_workflow(file_name):
    workflow_path = os.path.join(WORK, file_name)
    if not os.path.isfile(workflow_path):
        scp_from_server("~/ai/workflows/" + file_name, workflow_path)
    with open(workflow_path, encoding="utf-8") as workflow_file:
        return json.load(workflow_file)


def upload_image(path):
    """Sends a sample sheet to ComfyUI. It returns the name on the server."""
    boundary = "hexdom-local-art-boundary"
    with open(path, "rb") as image_file:
        data = image_file.read()
    head = ('--%s\r\nContent-Disposition: form-data; name="image"; filename="%s"\r\n'
            "Content-Type: application/octet-stream\r\n\r\n") % (boundary, os.path.basename(path))
    body = head.encode("utf-8") + data + ("\r\n--%s--\r\n" % boundary).encode("utf-8")
    request = urllib.request.Request(COMFY + "/upload/image", data=body,
                                     headers={"Content-Type": "multipart/form-data; boundary=" + boundary})
    try:
        with urllib.request.urlopen(request, timeout=120) as answer:
            return json.load(answer)["name"]
    except OSError as error:
        raise ServerDown(str(error)) from error


def make_image(job, seed, out_path):
    if job["kind"] == "sheet":
        # The sample sheet of Drive gives the layout, the style, the green and the red.
        workflow = load_workflow(WORKFLOW_EDIT)
        workflow["41"]["inputs"]["image"] = upload_image(os.path.join(SAMPLE_FOLDER, SHEET_SAMPLES[job["name"]]))
        workflow["68"]["inputs"]["prompt"] = EDIT_SHEET % job["prompt"]
        workflow["65"]["inputs"]["seed"] = seed
    else:
        workflow = load_workflow(WORKFLOW_2D)
        wide = job["kind"] == "effect"
        workflow["6"]["inputs"]["text"] = image_prompt(job)
        workflow["58"]["inputs"]["width"] = 1536 if wide else 1024
        workflow["58"]["inputs"]["height"] = 768 if wide else 1024
        workflow["3"]["inputs"]["seed"] = seed
    prompt_id = comfy_json("/prompt", {"prompt": workflow})["prompt_id"]
    deadline = time.time() + IMAGE_TIMEOUT_SECONDS
    while time.time() < deadline:
        history = comfy_json("/history/" + prompt_id)
        if prompt_id in history:
            outputs = history[prompt_id].get("outputs", {})
            images = [image for node in outputs.values() for image in node.get("images", [])]
            if not images:
                return False
            query = urllib.parse.urlencode({key: images[0][key] for key in ("filename", "subfolder", "type")})
            try:
                with urllib.request.urlopen(COMFY + "/view?" + query, timeout=120) as answer:
                    data = answer.read()
            except OSError as error:
                raise ServerDown(str(error)) from error
            with open(out_path + ".tmp", "wb") as image_file:
                image_file.write(data)
            os.replace(out_path + ".tmp", out_path)
            return True
        time.sleep(5)
    return False


def do_image_job(job, state):
    entry = state.setdefault(job["name"], {"status": "pending", "attempts": 0, "note": ""})
    folder = os.path.join(WORK, "2d")
    os.makedirs(folder, exist_ok=True)
    out_path = os.path.join(folder, job["name"] + ".png")
    if not os.path.isfile(out_path):
        ensure_stack("2d")
        entry["attempts"] += 1
        write_state(state)
        if not make_image(job, BASE_SEED + job["index"] * 10 + entry["attempts"], out_path):
            if entry["attempts"] >= MAX_ATTEMPTS:
                set_state(state, job["name"], "failed", "no image")
            return
    note = "review only"
    if job["kind"] == "sheet":
        art_import.import_sheet(out_path, os.path.join(PACK, "sprites", "art"))
        note = "in the pack"
    set_state(state, job["name"], "done", note)


def start_remote_job(job, seed):
    folder = REMOTE_OUT + "/" + job["name"]
    atlas = 1024 if job["name"].startswith("base_") else 512
    pipeline = ("python3 layers/pipeline/src/pipeline.py --prompt %s --target-faces %d --res %d --tex-res %d --atlas %d "
                "--framing object --seed %d --out-dir %s --runner server") % (
                    shlex.quote(job["prompt"] + STYLE_3D), TARGET_FACES, MESH_RESOLUTION, TEXTURE_VOLUME, atlas, seed, folder)
    inner = "%s > %s/run.log 2>&1; echo $? > %s/exit" % (pipeline, folder, folder)
    command = ("cd %s && mkdir -p %s && rm -f %s/exit && date +%%s > %s/started && "
               "(nohup setsid bash -c %s </dev/null >/dev/null 2>&1 & echo $! > %s/pid)") % (
                   REMOTE_REPO, folder, folder, folder, shlex.quote(inner), folder)
    code, text = ssh(command)
    if code != 0:
        raise ServerDown("start: " + text[:200])


def remote_job_state(job):
    """Returns ("exit", code), ("running", seconds) or ("none", 0)."""
    folder = REMOTE_OUT + "/" + job["name"]
    command = ("cd %s; if [ -f %s/exit ]; then echo exit $(cat %s/exit); "
               "elif [ -f %s/pid ] && kill -0 $(cat %s/pid) 2>/dev/null; then echo running $(( $(date +%%s) - $(cat %s/started) )); "
               "else echo none 0; fi") % (REMOTE_REPO, folder, folder, folder, folder, folder)
    _, text = ssh(command)
    words = text.split()
    return words[0], int(words[1])


def write_provenance(name, triangles):
    path = os.path.join(PACK, "models", "provenance.tsv")
    rows = {}
    if os.path.isfile(path):
        with open(path, encoding="utf-8", newline="") as provenance_file:
            for row in csv.reader(provenance_file, delimiter="\t", quoting=csv.QUOTE_NONE):
                if row and not row[0].startswith("#") and row[0] != "file":
                    rows[row[0]] = row
    rows[name] = [name, "misc/local_art.py, docs/europe/ART-JOBS.tsv", "FLUX.2 klein 4B and TRELLIS.2 on the local server",
                  time.strftime("%Y-%m-%d"), MODEL_LICENCE, "%d triangles" % triangles]
    with open(path + ".tmp", "w", encoding="utf-8", newline="") as provenance_file:
        provenance_file.write("file\tsource\tmodel\tdate\tlicence\tnote\n")
        provenance_file.write("# The provenance of each model of this folder (docs/ASSETS.md, \"Base models\").\n")
        for key in sorted(rows):
            provenance_file.write("\t".join(rows[key]) + "\n")
    os.replace(path + ".tmp", path)


def shrink_small_model(file_name):
    """Cuts the texture of a facility or a building to SMALL_MODEL_TEXTURE. The bases keep their size.

    The server ignores --atlas, so the import must do this. A second call changes nothing.
    """
    if file_name.startswith("base_"):
        return
    path = os.path.join(PACK, "models", file_name)
    with Image.open(path) as image:
        if image.width <= SMALL_MODEL_TEXTURE:
            return
        small = image.convert("RGB").resize((SMALL_MODEL_TEXTURE, SMALL_MODEL_TEXTURE), Image.LANCZOS)
    small.save(path + ".tmp.png", optimize=True)
    os.replace(path + ".tmp.png", path)


def brighten_toward_source(texture_path, source_path):
    """Brightens a texture with one curve on the luminance. The colour ratios do not change.

    An atlas has large dark blocks. These blocks give false colours if each channel
    has its own curve. The 75th percentile is not sensitive to the blocks.
    """
    texture = Image.open(texture_path).convert("RGB")
    pixels = numpy.asarray(texture).astype(float) / 255.0
    source = numpy.asarray(Image.open(source_path).convert("RGB")).astype(float) / 255.0
    background = source[:16, :16].reshape(-1, 3).mean(0)
    subject = numpy.abs(source - background).sum(2) * 255.0 > SUBJECT_DIFFERENCE
    luminance = numpy.maximum((pixels * LUMINANCE_WEIGHTS).sum(2), 1.0 / 255.0)
    texture_level = numpy.percentile(luminance, BRIGHTNESS_PERCENTILE)
    source_level = numpy.percentile((source * LUMINANCE_WEIGHTS).sum(2)[subject], BRIGHTNESS_PERCENTILE)
    exponent = min(1.0, max(STRONGEST_BRIGHTNESS_EXPONENT, numpy.log(source_level) / numpy.log(texture_level)))
    gain = numpy.power(luminance, exponent) / luminance
    result = numpy.clip(pixels * gain[..., None], 0.0, 1.0)
    Image.fromarray((result * 255.0 + 0.5).astype("uint8")).save(texture_path, optimize=True)


def import_with_colour(name, glb_path):
    """Imports one GLB into the pack and sets its colours and its texture size."""
    glb_import.import_model(glb_path, name, PACK, 0.0)
    texture_path = os.path.join(PACK, "models", name + ".png")
    source_path = os.path.join(WORK, "images", name + ".png")
    brighten_toward_source(texture_path, source_path)
    shrink_small_model(name + ".png")
    write_jpeg_texture(texture_path)


def write_jpeg_texture(texture_path):
    """Writes the texture again as a JPEG file and removes the PNG file.

    The decoder of the game refuses a progressive file, so this file is baseline.
    No subsampling of the colour stops colour at the edges of the atlas pieces from mixing.
    """
    with Image.open(texture_path) as image:
        image.convert("RGB").save(texture_path[:-4] + ".jpg.tmp", "JPEG", quality=JPEG_QUALITY, subsampling=0, optimize=True, progressive=False)
    os.replace(texture_path[:-4] + ".jpg.tmp", texture_path[:-4] + ".jpg")
    os.remove(texture_path)


def collect_model(job, state):
    folder = REMOTE_OUT + "/" + job["name"]
    _, listing = ssh("cd %s/%s && ls -t *.glb *.png" % (REMOTE_REPO, folder))
    names = listing.split()
    glbs = [name for name in names if name.endswith(".glb")]
    if not glbs:
        return False
    code, report = ssh("podman run --rm -e NODE_PATH=/usr/local/lib/node_modules -v ~/ai/text-to-3D-skill:/t:ro,z "
                       "localhost/gltf-validator:latest node /t/scripts/validate-glb.mjs /t/%s/%s" % (folder, glbs[0]), timeout=180)
    if " 0 errors" not in report:
        log("%s: the validator reports: %s" % (job["name"], report.strip()[:300]))
        return False
    glb_path = os.path.join(WORK, "glb", job["name"] + ".glb")
    os.makedirs(os.path.dirname(glb_path), exist_ok=True)
    os.makedirs(os.path.join(WORK, "images"), exist_ok=True)
    scp_from_server("%s/%s/%s" % (REMOTE_REPO, folder, glbs[0]), glb_path)
    for name in names:
        if name.endswith(".png"):
            scp_from_server("%s/%s/%s" % (REMOTE_REPO, folder, name), os.path.join(WORK, "images", job["name"] + ".png"))
            break
    ssh("cd %s/out && cp hexdom_eu/%s/%s ." % (REMOTE_REPO, job["name"], glbs[0]))
    import_with_colour(job["name"], glb_path)
    document, _ = glb_import.read_glb(glb_path)
    triangles = sum(document["accessors"][primitive["indices"]]["count"] // 3
                    for mesh in document["meshes"] for primitive in mesh["primitives"])
    write_provenance(job["name"], triangles)
    models = os.path.join(PACK, "models", job["name"])
    size = os.path.getsize(models + ".hxm") + os.path.getsize(models + ".jpg")
    set_state(state, job["name"], "done", "%d triangles, %d KB" % (triangles, size // 1024))
    return True


def do_model_job(job, state):
    entry = state.setdefault(job["name"], {"status": "pending", "attempts": 0, "note": ""})
    ensure_stack("3d")
    while True:
        kind, value = remote_job_state(job)
        if kind == "running":
            if value > JOB_TIMEOUT_SECONDS:
                log("%s: timeout, stop the remote job" % job["name"])
                ssh("cd %s/%s && kill -- -$(cat pid); rm -f pid" % (REMOTE_REPO, REMOTE_OUT + "/" + job["name"]))
                continue
            time.sleep(POLL_SECONDS)
            continue
        if kind == "exit" and value == 0 and collect_model(job, state):
            return
        if entry["attempts"] >= MAX_ATTEMPTS:
            set_state(state, job["name"], "failed", "no valid model after %d attempts" % entry["attempts"])
            return
        entry["attempts"] += 1
        write_state(state)
        ensure_stack("3d")
        log("%s: start, attempt %d" % (job["name"], entry["attempts"]))
        start_remote_job(job, BASE_SEED + job["index"] * 10 + entry["attempts"])
        time.sleep(POLL_SECONDS)


def main():
    os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))    # a task can start the script from any folder
    os.makedirs(WORK, exist_ok=True)
    os.makedirs(os.path.join(PACK, "models"), exist_ok=True)
    os.makedirs(os.path.join(PACK, "sprites", "art"), exist_ok=True)
    lock_file = open(os.path.join(WORK, "run.lock"), "a+")
    try:
        msvcrt.locking(lock_file.fileno(), msvcrt.LK_NBLCK, 1)
    except OSError:
        return 0                # another run has the lock
    ES_CONTINUOUS, ES_SYSTEM_REQUIRED = 0x80000000, 0x00000001
    ctypes.windll.kernel32.SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED)   # no sleep while this runs
    log("run: start, pid %d" % os.getpid())
    jobs = read_jobs()
    state = read_state()
    while True:
        pending = [job for job in jobs if state.get(job["name"], {}).get("status") not in ("done", "failed")]
        if not pending:
            break
        job = pending[0]
        try:
            if job["kind"] == "model":
                do_model_job(job, state)
            else:
                do_image_job(job, state)
        except ServerDown as error:
            log("server: %s. Wait %d s." % (error, RETRY_SECONDS))
            time.sleep(RETRY_SECONDS)
        except Exception as error:     # one bad job must not stop the night
            entry = state.setdefault(job["name"], {"status": "pending", "attempts": 0, "note": ""})
            log("%s: error %r" % (job["name"], error))
            entry["attempts"] += 1
            write_state(state)
            if entry["attempts"] >= MAX_ATTEMPTS:
                set_state(state, job["name"], "failed", repr(error)[:150].replace("\t", " "))
            time.sleep(RETRY_SECONDS)
    done = sum(1 for entry in state.values() if entry["status"] == "done")
    log("run: finished, %d done, %d failed" % (done, len(jobs) - done))
    run_tool(["schtasks", "/delete", "/tn", TASK_NAME, "/f"], 60)
    return 0


def import_models_again():
    """Imports the finished models again from their GLBs, with no new generation.

    A running batch does not write to a finished model, so this can run beside it.
    """
    os.chdir(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    state = read_state()
    for job in read_jobs():
        name = job["name"]
        if job["kind"] == "model" and state.get(name, {}).get("status") == "done":
            import_with_colour(name, os.path.join(WORK, "glb", name + ".glb"))
    return 0


if __name__ == "__main__":
    sys.exit(import_models_again() if "--import-again" in sys.argv else main())
