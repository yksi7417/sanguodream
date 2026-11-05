"""The state of one room of a shared game (docs/MULTIPLAYER.md, MP2).

The server keeps the one order of the steps of each game and relays each
step to the other players. The server has no sim. The engine of the player
who acts is the author of the step (code/engine/engine_multiplayer.cpp).

A step on the socket is a binary message:
     0  u32  the number of the step, little-endian
     4  u32  0
     8  ...  the bytes of the engine. Byte 1 of them is the kind of the step,
             and byte 3 is the faction that acted before the step.

The server takes a step only if:
- the number is the next number of the room;
- the faction that acted is the faction that acts in the room, and the
  author has the seat of that faction. When no seat holds that faction,
  only the host is the author;
- a step of the settings comes from the host. A change of the human
  factions and a clear of the ledger never come from a player.

Each engine sends the checksum of its state after each step, and the
faction that acts after it. The report of step -1 is the start state. Two
different reports for one step stop the game with a desync. The faction
of the first report of the last step is the faction that acts next.

rooms.py keeps the rooms, and room_socket.py relays the messages of their sockets.
"""
import asyncio
import os
import re
import struct

STEP_HEADER = 8
ENGINE_HEADER = 8
MAX_STEP_BYTES = 64 * 1024
MAX_WATCHERS = 32         # MP7: the spectators of one room
NO_FACTION = 0xFF
START_STEP = -1
CODE_LETTERS = "ABCDEFGHJKMNPQRSTUVWXYZ"   # no I, L or O: they look like 1 and 0
CODE_LENGTH = 6
MAX_SEATS = 8
MAX_NAME_CHARS = 64
TURN_SECONDS = {"live": (120, 600), "correspondence": (12 * 3600, 72 * 3600)}
# The browser tests set a short turn. The site never sets this variable.
TEST_MIN_TURN_SECONDS = int(os.environ.get("HEXDOM_TEST_MIN_TURN_SECONDS") or 0)
MISSED_TURNS_FOR_AI = 2   # the AI takes the faction after this number of missed faction turns in a row
ABSENT_SECONDS = 60       # a player who lost the connection has this time to connect again
SERVER = "~server"        # the author of a step of the server; no uid has a "~"
PACK = re.compile(r"[a-z0-9_]{1,63}")   # the folder of a content pack under data/
KEEP_SNAPSHOTS = 5        # MP5: the server keeps the last five snapshots of a game
SNAPSHOT_URL_SECONDS = 120   # a signed address of a save in the bucket stops after this time

# The kinds of the steps of the engine (engine_step_kind in code/engine/engine.h).
STEP_COMMAND, STEP_SETTINGS, STEP_HUMAN, STEP_LEDGER_CLEAR, STEP_END_TURN, STEP_AI_TURN = range(6)
# The first 4 bytes of a command are its kind (code/sim/sim_wire.cpp). SIM_COMMAND_END_TURN is 1.
END_TURN_KIND = struct.pack("<I", 1)

MAX_ROOM_STEPS = 200_000
MAX_ROOM_BYTES = 64 * 1024 * 1024

GID = re.compile(r"[0-9a-f]{16}")
CHECKSUM = re.compile(r"[0-9a-f]{16}")
BUILD = re.compile(r"[0-9a-f]{0,64}")


class RoomError(Exception):
    """A request that the room refuses. The text goes to the client."""


def is_int(value):
    """An int of JSON. JSON true and false are no numbers."""
    return isinstance(value, int) and not isinstance(value, bool)


def room_key(gid):
    return f"games/{gid}/room.json"


def log_prefix(gid):
    return f"games/{gid}/log/"


def member_prefix(uid):
    return f"members/{uid}/games/"


def start_key(gid):
    return f"games/{gid}/start.hxs"


def snapshot_prefix(gid):
    return f"games/{gid}/snap/"


def snapshot_key(gid, step):
    return f"{snapshot_prefix(gid)}{step:08d}.hxs"


def desync_key(gid, step, uid):
    return f"games/{gid}/desync/{step:08d}-{uid}.hxs"


CHANNEL = re.compile(r"[0-9]{1,20}")   # MP8: the id of a voice channel of Discord (a snowflake)


def channel_key(channel):
    return f"discord/channels/{channel}"


def code_key(code):
    return f"codes/{code}"


CODE = re.compile(f"[{CODE_LETTERS}]{{{CODE_LENGTH}}}")
MAX_START_IMAGE_BYTES = 16 * 1024 * 1024   # the limit of a save (storage.MAX_SAVE_BYTES)


def clean_text(text, limit):
    """One line of plain text, at most limit characters. The page shows it as text, never as HTML."""
    return " ".join(str(text or "").split())[:limit]


def pack_log(steps):
    return b"".join(struct.pack("<I", len(step)) + step for step in steps)


def unpack_log(data):
    steps, at = [], 0
    while at + 4 <= len(data):
        (size,) = struct.unpack_from("<I", data, at)
        at += 4
        if at + size > len(data):
            raise ValueError("a short part of the log")
        steps.append(bytes(data[at:at + size]))
        at += size
    return steps


def step_message(number, engine_bytes):
    return struct.pack("<II", number, 0) + engine_bytes


def is_end_turn(engine_bytes):
    if engine_bytes[1] in (STEP_END_TURN, STEP_AI_TURN):
        return True
    return engine_bytes[1] == STEP_COMMAND and engine_bytes[ENGINE_HEADER:ENGINE_HEADER + 4] == END_TURN_KIND


def turn_limits(mode):
    low, high = TURN_SECONDS[mode]
    return (min(low, TEST_MIN_TURN_SECONDS) if TEST_MIN_TURN_SECONDS > 0 else low), high


def check_options(mode, turn_seconds):
    if not isinstance(mode, str) or mode not in TURN_SECONDS:
        raise RoomError("bad mode")
    low, high = turn_limits(mode)
    if not is_int(turn_seconds) or not low <= turn_seconds <= high:
        raise RoomError("bad time of a turn")


class Room:
    """One shared game: the record of room.json, the log of steps and the sockets."""

    def __init__(self, record, now=0.0):
        self.record = record
        self.steps = []                 # the bytes of the engine of each step, by number
        self.step_bytes = 0
        self.flushed = 0                # the steps that the storage has
        self.reports = {}               # step -> {uid: (checksum, acting)} in the order of the reports
        self.last_author = None         # the uid of the author of the last step, or SERVER
        self.peers = set()
        self.lock = asyncio.Lock()
        self.loaded = now               # a player without a socket since the load is absent since then
        self.absent = {}                # uid -> the time of the loss of the connection

    @property
    def gid(self):
        return self.record["gid"]

    def seat_of(self, uid):
        for seat in self.record["seats"]:
            if seat.get("uid") == uid:
                return seat
        return None

    def is_member(self, uid):
        return uid == self.record["host"] or self.seat_of(uid) is not None

    def is_viewer(self, uid):
        """A member, or a spectator (MP7). A spectator gets the steps and gives no command."""
        return self.is_member(uid) or uid in self.record.get("watchers", [])

    def author_of(self, acting):
        """The uid that can send a step when the faction acts: its player, else the host."""
        for seat in self.record["seats"]:
            if seat["force"] == acting and seat.get("uid") and not seat.get("ai"):
                return seat["uid"]
        return self.record["host"]

    def player_seat(self, force):
        """The seat of the faction if a player has it, else None."""
        return next((seat for seat in self.record["seats"] if seat["force"] == force and seat.get("uid")), None)

    def no_steps_of_the_ai(self):
        """True if the host cannot have steps for the AI on the way. After a faction with a
        player ends its turn, the host sends the steps of the factions of the AI in a burst."""
        if not any(peer.uid == self.record["host"] for peer in self.peers):
            return True
        acting = self.record["acting"]
        if acting == NO_FACTION:
            if self.last_author == SERVER:
                return True   # the server refuses each step until a report names a faction
            # A game of the AI alone: after its report, the host waits before the next step.
            return self.record["host"] in self.reports.get(len(self.steps) - 1, {})
        seat = self.player_seat(acting)
        return bool(seat and not seat.get("ai"))

    def end_vote_forces(self):
        """The factions of the players who voted to end the game. The uids stay on the server."""
        votes = self.record.get("end_votes", [])
        return sorted(seat["force"] for seat in self.record["seats"] if seat.get("uid") in votes)

    def end_players(self):
        """The uids of the players who must agree before a vote ends the game."""
        return {seat["uid"] for seat in self.record["seats"] if seat.get("uid")}

    def public(self):
        """The room for the pages. The uids of the players stay on the server."""
        record = {key: value for key, value in self.record.items() if key not in ("host", "deadline", "end_votes", "watchers")}
        record["watcher_count"] = len(self.record.get("watchers", []))
        record["seats"] =[{key: seat.get(key) for key in ("force", "name", "user", "ai", "auto")} for seat in self.record["seats"]]
        record["step_count"] = len(self.steps)
        return record

    def summary(self, uid):
        """The room for the list "My games" of the member (MP5): the room and "Your turn"."""
        record = self.public()
        seat = self.seat_of(uid)
        record["your_turn"] = bool(self.record["state"] == "running" and seat and not seat.get("ai")
                                   and seat["force"] == self.record["acting"])
        return record

    # ---- the steps ----

    def check_step(self, uid, message):
        """Give the number and the bytes of the engine, or raise RoomError."""
        if self.record["state"] == "desync":
            raise RoomError("desync")
        if self.record["state"] != "running":
            raise RoomError("the game does not run")
        if len(message) < STEP_HEADER + ENGINE_HEADER or len(message) > MAX_STEP_BYTES:
            raise RoomError("bad step")
        number, _ = struct.unpack_from("<II", message, 0)
        engine_bytes = bytes(message[STEP_HEADER:])
        kind, acting = engine_bytes[1], engine_bytes[3]
        if number != len(self.steps):
            raise RoomError("old step" if number < len(self.steps) else "step out of order")
        if len(self.steps) >= MAX_ROOM_STEPS or self.step_bytes + len(engine_bytes) > MAX_ROOM_BYTES:
            raise RoomError("the log of the game is full")
        if kind in (STEP_HUMAN, STEP_LEDGER_CLEAR) or kind > STEP_LEDGER_CLEAR:
            raise RoomError("not a step of a player")
        if kind == STEP_SETTINGS and uid != self.record["host"]:
            raise RoomError("not your seat")
        if acting != self.record["acting"] or self.author_of(acting) != uid:
            raise RoomError("not your seat")
        return number, engine_bytes

    def add_step(self, engine_bytes, author=None):
        self.steps.append(engine_bytes)
        self.step_bytes += len(engine_bytes)
        self.last_author = author
        self.record["step_count"] = len(self.steps)
        if author != SERVER:
            self.record.setdefault("missed", {})[str(engine_bytes[3])] = 0   # the player acted in the faction turn

    def report(self, uid, step, checksum, acting):
        """A checksum after a step. Give "desync", "acting" when the faction that acts changes, or None.

        Each player can report, and two different reports give a desync. Only
        the author of the last step and the host can change the faction that
        acts. Each player reports the start state (step -1), and only the
        report of the host names the first faction that acts.

        In a desync (MP6), the reports of the step of the desync go on. Give
        "judge" after each of them: the room can then decide."""
        desync = self.record["state"] == "desync"
        if self.record["state"] != "running" and not (desync and step == self.record.get("desync_step")):
            raise RoomError("the game does not run")
        if not is_int(step) or not START_STEP <= step < len(self.steps):
            raise RoomError("no such step")
        if not isinstance(checksum, str) or not CHECKSUM.fullmatch(checksum):
            raise RoomError("bad checksum")
        if not is_int(acting) or not 0 <= acting <= NO_FACTION:
            raise RoomError("bad checksum")
        entries = self.reports.setdefault(step, {})
        first = self.first_report(step) or (checksum, acting)
        entries.setdefault(uid, (checksum, acting))
        if desync:
            return "judge"
        if first != (checksum, acting):
            self.record["state"] = "desync"
            self.record["desync_step"] = step
            self.record["last_desync"] = step
            return "desync"
        last = step == len(self.steps) - 1   # a late report of the start state (a page that joins again) is not the last
        authors = (self.record["host"],) if step == START_STEP else (self.last_author, self.record["host"])
        if self.last_author == SERVER and step >= 0:
            authors = (uid,)   # a step of the server: the first report of a member names the faction
        if last and acting != self.record["acting"] and uid in authors:
            self.record["acting"] = acting
            return "acting"
        return None

    def first_report(self, step):
        """The (checksum, acting) of the first report of the step, or None."""
        entries = self.reports.get(step)
        return next(iter(entries.values())) if entries else None
