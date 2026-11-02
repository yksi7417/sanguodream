"""The referee of the shared games (docs/MULTIPLAYER.md, MP6).

The referee is a headless engine for node (code/wasm_referee.cpp,
"build_web.bat referee"). The room server starts it only when it must decide
a desync of two players. The referee loads the latest snapshot before the
step of the desync, or the start image, and applies the log after it. Its
checksum after the step then decides which player has the good state. After
the run, the process stops, so the memory of the machine stays small.
"""
import asyncio
import os
import struct

VERSION = 1
PLACEHOLDER_MAP = 1          # the map of the feature tests; the game uses the map of the pack
TIMEOUT_SECONDS = 120
DEFAULT_SCRIPTS = ("/srv/referee/referee.js",
                   os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "referee.js"))


class RefereeError(Exception):
    """The referee did not give the checksums."""


def payload(pack, first_step, image, steps, humans=(), flags=0, ledger=0):
    """The input of the referee on stdin (the format is in code/wasm_referee.cpp)."""
    name = pack.encode("ascii")
    parts = [b"HXRF", struct.pack("<IIII", VERSION, flags, ledger, first_step),
             struct.pack("<I", len(name)), name,
             struct.pack("<I", len(humans)), bytes(humans),
             struct.pack("<I", len(image)), image,
             struct.pack("<I", len(steps))]
    for step in steps:
        parts += [struct.pack("<I", len(step)), step]
    return b"".join(parts)


def parse(text, count):
    """The checksum after each step, from the output of the referee."""
    lines = text.strip().splitlines()
    if not lines or lines[-1] != f"done {count}":
        raise RefereeError(lines[-1] if lines else "no output")
    return [line.split()[1] for line in lines[:-1]]


def find_script():
    """The referee of HEXDOM_REFEREE, of the image of the site, or of the build folder."""
    configured = os.environ.get("HEXDOM_REFEREE")
    if configured:
        return configured if os.path.isfile(configured) else None
    return next((path for path in DEFAULT_SCRIPTS if os.path.isfile(path)), None)


async def run(script, data):
    """Run node on the referee with the input. The current folder is the folder of the
    script: the referee reads the packs from ../data."""
    process = await asyncio.create_subprocess_exec(
        "node", script, cwd=os.path.dirname(os.path.abspath(script)),
        stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
    try:
        output, errors = await asyncio.wait_for(process.communicate(data), TIMEOUT_SECONDS)
    except asyncio.TimeoutError:
        process.kill()
        raise RefereeError("the referee took too long")
    if process.returncode != 0:
        raise RefereeError((output or errors).decode("utf-8", "replace").strip()[-300:])
    return output.decode("utf-8", "replace")


class Referee:
    """The callable of rooms.Rooms: referee(record, first_step, image, steps) gives the
    checksum after each step. The human factions of a start image are the factions with a
    player at the start. A later seat comes with a step of the log (MP9). A room from
    before MP9 has no list, and its seats did not change after the start."""

    def __init__(self, script):
        self.script = script

    async def __call__(self, record, first_step, image, steps):
        humans = record.get("start_humans")
        if humans is None:
            humans = [seat["force"] for seat in record["seats"] if seat.get("uid")]
        options = record.get("referee", {})   # only the tests set it: the map and the ledger of the feature tests
        data = payload(record["pack"], first_step, image, steps, humans,
                       options.get("flags", 0), options.get("ledger", 0))
        return parse(await run(self.script, data), len(steps))
