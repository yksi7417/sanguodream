"""The rooms of the shared games (docs/MULTIPLAYER.md, MP2).

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

The keys in the storage (storage.py):
- games/<gid>/room.json: the room.
- games/<gid>/log/<first>-<last>.bin: a part of the log of steps. Each step
  is a u32 size, little-endian, then the bytes of the engine.
- games/<gid>/start.hxs: the start image of the host (MP3), in the format of a
  save slot. The start of the game needs it.
- members/<uid>/games/<gid>: an empty file for each game of a member.
- codes/<code>: the gid of the room of the invite code, as text.

The timers (MP4). The faction that acts has a deadline in room.json. When
the deadline is over, the server is the author of a step:
- end_turn (STEP_END_TURN) ends the faction turn. The commands of the
  faction in that turn stay.
- After the second missed faction turn in a row of a player without a
  socket, the server sends STEP_AI_TURN. In one step, the engine makes the
  faction a faction of the AI, and the AI finishes that faction turn. The
  hello of the player gives the faction back with set_faction_human true
  (STEP_HUMAN). A player with a socket keeps the faction.
After a step of the server, no faction acts in the room until the first
report of a member names it. Thus a deadline gives one end_turn, not two.
With the option pause_absent, the timer stops while a player of a seat is
absent for more than ABSENT_SECONDS.
"""
import asyncio
import json
import os
import re
import secrets
import struct
import time

STEP_HEADER = 8
ENGINE_HEADER = 8
MAX_STEP_BYTES = 64 * 1024
MAX_STEPS_PER_SECOND = 20
MAX_CHAT_CHARS = 300
FLUSH_SECONDS = 30
PING_SECONDS = 20         # the proxy of fly.io closes an idle connection after 60 seconds
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
TIMER_SECONDS = 2         # the loop of the deadlines
SERVER = "~server"        # the author of a step of the server; no uid has a "~"
PACK = re.compile(r"[a-z0-9_]{1,63}")   # the folder of a content pack under data/
KEEP_SNAPSHOTS = 5        # MP5: the server keeps the last five snapshots of a game

# The kinds of the steps of the engine (engine_step_kind in code/engine/engine.h).
STEP_COMMAND, STEP_SETTINGS, STEP_HUMAN, STEP_LEDGER_CLEAR, STEP_END_TURN, STEP_AI_TURN = range(6)
# The first 4 bytes of a command are its kind (code/sim/sim_wire.cpp). SIM_COMMAND_END_TURN is 1.
END_TURN_KIND = struct.pack("<I", 1)

MAX_MESSAGES_PER_SECOND = 40      # all the messages of one socket
MAX_ROOM_STEPS = 200_000
MAX_ROOM_BYTES = 64 * 1024 * 1024
SEND_SECONDS = 5                  # a socket that does not take a message in this time leaves the room

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
        self.reports = {}               # step -> (checksum, acting) of the first report
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

    def author_of(self, acting):
        """The uid that can send a step when the faction acts: its player, else the host."""
        for seat in self.record["seats"]:
            if seat["force"] == acting and seat.get("uid") and not seat.get("ai"):
                return seat["uid"]
        return self.record["host"]

    def player_seat(self, force):
        """The seat of the faction if a player has it, else None."""
        return next((seat for seat in self.record["seats"] if seat["force"] == force and seat.get("uid")), None)

    def public(self):
        """The room for the pages. The uids of the players stay on the server."""
        record = {key: value for key, value in self.record.items() if key not in ("host", "deadline")}
        record["seats"] = [{key: seat.get(key) for key in ("force", "name", "user", "ai")} for seat in self.record["seats"]]
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
        report of the host names the first faction that acts."""
        if self.record["state"] != "running":
            raise RoomError("the game does not run")
        if not is_int(step) or not START_STEP <= step < len(self.steps):
            raise RoomError("no such step")
        if not isinstance(checksum, str) or not CHECKSUM.fullmatch(checksum):
            raise RoomError("bad checksum")
        if not is_int(acting) or not 0 <= acting <= NO_FACTION:
            raise RoomError("bad checksum")
        first = self.reports.setdefault(step, (checksum, acting))
        if first != (checksum, acting):
            self.record["state"] = "desync"
            self.record["desync_step"] = step
            return "desync"
        last = step == len(self.steps) - 1   # a late report of the start state (a page that joins again) is not the last
        authors = (self.record["host"],) if step == START_STEP else (self.last_author, self.record["host"])
        if self.last_author == SERVER and step >= 0:
            authors = (uid,)   # a step of the server: the first report of a member names the faction
        if last and acting != self.record["acting"] and uid in authors:
            self.record["acting"] = acting
            return "acting"
        return None


class Rooms:
    """The rooms of the server. A room loads from the storage at its first use."""

    def __init__(self, store, clock=time.time, build=""):
        self.store = store
        self.clock = clock
        self.build = build   # the build of the game files of this server; "" when the server has none
        self.rooms = {}
        self.loading = {}   # gid -> the lock of its first load: two requests make one Room

    async def create(self, host, name, scenario, pack, build, mode, turn_seconds, factions):
        check_options(mode, turn_seconds)
        if not isinstance(pack, str) or not PACK.fullmatch(pack):
            raise RoomError("bad pack")   # each player starts the engine of this folder of data/
        if not isinstance(build, str) or not BUILD.fullmatch(build):
            raise RoomError("bad build")   # the build id of game_files.py; the server gives it, not the client
        if not isinstance(factions, list) or not 1 <= len(factions) < NO_FACTION:
            raise RoomError("bad factions")
        seats, forces = [], set()
        for faction in factions:
            force = faction.get("force") if isinstance(faction, dict) else None
            if not is_int(force) or not 0 <= force < NO_FACTION or force in forces:
                raise RoomError("bad factions")
            forces.add(force)
            seats.append({"force": force, "name": clean_text(faction.get("name"), MAX_NAME_CHARS), "uid": None, "user": None, "ai": True})
        gid = secrets.token_hex(8)
        record = {
            "gid": gid,
            "code": await self.free_code(),
            "name": clean_text(name, MAX_NAME_CHARS) or "Game",
            "host": host["uid"],
            "host_name": clean_text(host["name"], MAX_NAME_CHARS),
            "scenario": clean_text(scenario, MAX_NAME_CHARS),
            "pack": pack,
            "build": build,
            "mode": mode,
            "turn_seconds": turn_seconds,
            "pause_absent": False,
            "deadline": None,
            "paused": False,
            "missed": {},
            "seats": seats,
            "state": "open",
            "acting": NO_FACTION,
            "seed": secrets.randbits(63),
            "step_count": 0,
            "created": int(time.time()),
        }
        room = Room(record, self.clock())
        self.rooms[gid] = room
        await self.save(room)
        await self.store.put(member_prefix(host["uid"]) + gid, b"")
        await self.store.put(code_key(record["code"]), gid.encode("ascii"))
        return room

    async def free_code(self):
        for _ in range(16):
            code = "".join(secrets.choice(CODE_LETTERS) for _ in range(CODE_LENGTH))
            if await self.store.get(code_key(code)) is None:
                return code
        raise RoomError("no free invite code")

    async def find_code(self, code):
        """The room of an invite code, or None."""
        if not isinstance(code, str) or not CODE.fullmatch(code):
            return None
        gid = await self.store.get(code_key(code))
        return await self.get(gid.decode("ascii", "replace")) if gid else None

    async def put_start_image(self, room, user, data):
        """The host sends the start image before the start."""
        if user["uid"] != room.record["host"]:
            raise RoomError("only the host sends the start image")
        if room.record["state"] != "open":
            raise RoomError("the game started")
        if not data or len(data) > MAX_START_IMAGE_BYTES:
            raise RoomError("bad start image")
        await self.store.put(start_key(room.gid), data)
        room.record["start_image"] = len(data)
        await self.save(room)

    async def start_image(self, room, user):
        """The start image for the host and the players of a running game."""
        if not room.is_member(user["uid"]):
            raise RoomError("no seat in this game")
        if room.record["state"] == "open" or not room.record.get("start_image"):
            raise RoomError("the game did not start")
        return await self.store.get(start_key(room.gid))

    async def get(self, gid):
        if not isinstance(gid, str) or not GID.fullmatch(gid):
            return None
        room = self.rooms.get(gid)
        if room:
            return room
        async with self.loading.setdefault(gid, asyncio.Lock()):
            room = self.rooms.get(gid)
            if room:
                return room
            data = await self.store.get(room_key(gid))
            if not data:
                return None
            room = Room(json.loads(data), self.clock())
            for part in sorted(item["name"] for item in await self.store.list(log_prefix(gid))):
                room.steps += unpack_log(await self.store.get(log_prefix(gid) + part) or b"")
            room.step_bytes = sum(len(step) for step in room.steps)
            room.flushed = len(room.steps)
            room.record["step_count"] = len(room.steps)
            self.rooms[gid] = room
            await self.move_to_build(room)
        async with room.lock:
            await self.expire(room)   # a deadline that ended while the machine stopped
        return room

    async def move_to_build(self, room):
        """A deploy gave the server a new build (MP5). A running game goes on in the new build:
        each page loads the latest snapshot and applies the log after it. The log before the
        snapshot does not run again. A page of the old build gets "old build"."""
        record = room.record
        if not self.build or record["state"] != "running" or record["build"] == self.build:
            return
        record.setdefault("old_builds", []).append(record["build"])
        record["build"] = self.build
        await self.save(room)

    async def put_snapshot(self, room, user, step, checksum, data):
        """A snapshot: the image of the state after the step. Its checksum must be the checksum
        of the reports of that step."""
        if not room.is_member(user["uid"]):
            raise RoomError("no seat in this game")
        if room.record["state"] != "running":
            raise RoomError("the game does not run")
        if not is_int(step) or not 0 <= step < len(room.steps):
            raise RoomError("no such step")
        if not isinstance(checksum, str) or not CHECKSUM.fullmatch(checksum):
            raise RoomError("bad checksum")
        if not data or len(data) > MAX_START_IMAGE_BYTES:
            raise RoomError("bad snapshot")
        report = room.reports.get(step)
        if report is None:
            raise RoomError("no report of the step")
        if report[0] != checksum:
            raise RoomError("the checksum of the snapshot differs")
        await self.store.put(snapshot_key(room.gid, step), data)
        room.record["snapshot"] = max(step, room.record.get("snapshot", -1))
        names = sorted(item["name"] for item in await self.store.list(snapshot_prefix(room.gid)))
        for name in names[:-KEEP_SNAPSHOTS]:
            await self.store.delete(snapshot_prefix(room.gid) + name)
        await self.save(room)

    async def latest_snapshot(self, room, user):
        """The step and the bytes of the latest snapshot, or None."""
        if not room.is_member(user["uid"]):
            raise RoomError("no seat in this game")
        step = room.record.get("snapshot")
        if step is None:
            return None
        data = await self.store.get(snapshot_key(room.gid, step))
        return (step, data) if data else None

    async def save(self, room):
        await self.store.put(room_key(room.gid), json.dumps(room.record, ensure_ascii=False).encode("utf-8"))

    async def flush(self, room):
        """Write the steps after the last part of the log, then the room."""
        if room.flushed < len(room.steps):
            first, last = room.flushed, len(room.steps) - 1
            await self.store.put(f"{log_prefix(room.gid)}{first:08d}-{last:08d}.bin", pack_log(room.steps[first:]))
            room.flushed = len(room.steps)
        await self.save(room)

    async def flush_all(self):
        for room in list(self.rooms.values()):
            if room.flushed < len(room.steps):
                async with room.lock:
                    await self.flush(room)

    async def games_of(self, uid):
        rooms = []
        for item in await self.store.list(member_prefix(uid)):
            room = await self.get(item["name"])
            if room:
                rooms.append(room)
        return rooms

    # ---- the seats and the start ----

    async def take_seat(self, room, user, force):
        """The user takes the seat of the faction, or leaves the seats with force None."""
        if room.record["state"] != "open":
            raise RoomError("the game started")
        seat = None
        if force is not None:
            if not is_int(force):
                raise RoomError("no such seat")
            seat = next((s for s in room.record["seats"] if s["force"] == force), None)
            if seat is None:
                raise RoomError("no such seat")
            if seat.get("uid") not in (None, user["uid"]):
                raise RoomError("the seat is taken")
            players = sum(1 for s in room.record["seats"] if s.get("uid"))
            if players >= MAX_SEATS and not room.seat_of(user["uid"]):
                raise RoomError("the room is full")
        for other in room.record["seats"]:
            if other.get("uid") == user["uid"]:
                other.update(uid=None, user=None, ai=True)
        if seat is not None:
            seat.update(uid=user["uid"], user=clean_text(user["name"], MAX_NAME_CHARS), ai=False)
            await self.store.put(member_prefix(user["uid"]) + room.gid, b"")
        elif user["uid"] != room.record["host"]:
            await self.store.delete(member_prefix(user["uid"]) + room.gid)   # the game leaves the list of the member
        await self.save(room)

    async def set_options(self, room, user, options):
        """The host sets the mode, the time of a turn and the pause option before the start."""
        if user["uid"] != room.record["host"]:
            raise RoomError("only the host sets the options")
        if room.record["state"] != "open":
            raise RoomError("the game started")
        mode, turn_seconds, pause = options.get("mode"), options.get("turn_seconds"), options.get("pause_absent")
        check_options(mode, turn_seconds)
        if not isinstance(pause, bool):
            raise RoomError("bad pause option")
        room.record.update(mode=mode, turn_seconds=turn_seconds, pause_absent=pause)
        await self.save(room)

    # ---- the timers (MP4) ----

    def timer_message(self, room):
        record = room.record
        deadline = record.get("deadline")
        seconds = max(0, round(deadline - self.clock())) if deadline is not None else None
        return {"type": "timer", "acting": record["acting"], "seconds": seconds, "paused": bool(record.get("paused"))}

    async def start_deadline(self, room):
        """The faction that acts changed: its turn gets the full time."""
        room.record["deadline"] = self.clock() + room.record["turn_seconds"]
        room.record["paused"] = False
        room.record.pop("remaining", None)
        if room.record.get("pause_absent") and self.absent_player(room):
            await self.pause(room)
            return
        await self.save(room)
        await broadcast_json(room, self.timer_message(room))

    def absent_player(self, room):
        """A player of a seat without a socket for more than ABSENT_SECONDS."""
        now, online = self.clock(), {peer.uid for peer in room.peers}
        for seat in room.record["seats"]:
            uid = seat.get("uid")
            if uid and not seat.get("ai") and uid not in online and now - room.absent.get(uid, room.loaded) > ABSENT_SECONDS:
                return True
        return False

    async def pause(self, room):
        record = room.record
        if record.get("deadline") is not None:
            record["remaining"] = max(0, record["deadline"] - self.clock())
        record["deadline"] = None
        record["paused"] = True
        await self.save(room)
        await broadcast_json(room, self.timer_message(room))

    async def server_step(self, room, kind, force, value=0):
        """A step of the server: the header of the engine only. Each player gets it."""
        engine_bytes = bytes([1, kind, force, room.record["acting"]]) + struct.pack("<I", value)
        room.add_step(engine_bytes, SERVER)
        message = step_message(len(room.steps) - 1, engine_bytes)
        for peer in list(room.peers):
            await peer.send_bytes(message)
        drop_dead(room)

    async def expire(self, room):
        """Apply the deadline of the faction turn. The caller holds the lock of the room."""
        record = room.record
        if record["state"] != "running":
            return
        if record.get("pause_absent"):
            absent = self.absent_player(room)
            if absent and not record.get("paused") and record.get("deadline") is not None:
                await self.pause(room)
            elif not absent and record.get("paused"):
                record["paused"] = False
                record["deadline"] = self.clock() + record.pop("remaining", record["turn_seconds"])
                await self.save(room)
                await broadcast_json(room, self.timer_message(room))
        deadline = record.get("deadline")
        if deadline is None or self.clock() < deadline:
            return
        acting = record["acting"]
        missed = record.setdefault("missed", {})
        missed[str(acting)] = missed.get(str(acting), 0) + 1
        seat = room.player_seat(acting)
        online = seat and any(peer.uid == seat["uid"] for peer in room.peers)
        if seat and not online and missed[str(acting)] >= MISSED_TURNS_FOR_AI:
            # Only an absent player loses the faction: the hello of the player gives it back.
            await self.server_step(room, STEP_AI_TURN, acting)
            seat["ai"] = True
        else:
            await self.server_step(room, STEP_END_TURN, acting)
        record["acting"] = NO_FACTION
        record["deadline"] = None
        await self.flush(room)
        await broadcast_json(room, self.timer_message(room))

    async def give_back(self, room, uid):
        """The player of a seat of the AI connects again: the faction goes back to the player."""
        seat = room.seat_of(uid)
        if room.record["state"] != "running" or not seat or not seat.get("ai"):
            return
        await self.server_step(room, STEP_HUMAN, seat["force"], 1)
        seat["ai"] = False
        room.record.setdefault("missed", {})[str(seat["force"])] = 0
        await self.flush(room)

    async def expire_all(self):
        for room in list(self.rooms.values()):
            async with room.lock:
                await self.expire(room)

    async def start(self, room, user):
        if user["uid"] != room.record["host"]:
            raise RoomError("only the host starts the game")
        if room.record["state"] != "open":
            raise RoomError("the game started")
        if not room.record.get("start_image"):
            raise RoomError("no start image")
        if not any(seat.get("uid") for seat in room.record["seats"]):
            raise RoomError("no player has a seat")
        room.record["state"] = "running"
        room.record["started"] = int(time.time())
        await self.save(room)


# ---- the socket --------------------------------------------------------------

class Peer:
    """One socket of a room."""

    def __init__(self, socket, user):
        self.socket = socket
        self.user = user
        self.uid = user["uid"]
        self.welcomed = False
        self.build_ok = False
        self.dead = False
        self.steps = []      # the times of the last steps, for the limits
        self.messages = []   # the times of the last messages of each kind

    async def send(self, send, payload):
        """A socket that fails, or does not take the message in SEND_SECONDS, is dead.
        The room then stops sending to it, and the relay goes on for the others."""
        if self.dead or self.socket.closed:
            return
        try:
            await asyncio.wait_for(send(payload), SEND_SECONDS)
        except (ConnectionError, RuntimeError, asyncio.TimeoutError):
            self.dead = True

    async def send_json(self, message):
        await self.send(self.socket.send_str, json.dumps(message, ensure_ascii=False))

    async def send_bytes(self, data):
        await self.send(self.socket.send_bytes, data)

    def over_limit(self, now, step):
        self.messages = [moment for moment in self.messages if now - moment < 1.0] + [now]
        if step:
            self.steps = [moment for moment in self.steps if now - moment < 1.0] + [now]
        return len(self.messages) > MAX_MESSAGES_PER_SECOND or len(self.steps) > MAX_STEPS_PER_SECOND


def drop_dead(room):
    room.peers = {peer for peer in room.peers if not peer.dead}


def join(room, peer):
    """One socket for each member in a room. Give the old socket of the member, or None."""
    old = next((other for other in room.peers if other.uid == peer.uid), None)
    if old:
        old.dead = True
        room.peers.discard(old)
    room.peers.add(peer)
    return old


async def broadcast_json(room, message, skip=None):
    for peer in list(room.peers):
        if peer is not skip:
            await peer.send_json(message)
    drop_dead(room)


async def send_presence(room):
    """The factions of the seats with a socket, and the host."""
    online = {peer.uid for peer in room.peers}
    seats = sorted(seat["force"] for seat in room.record["seats"] if seat.get("uid") in online)
    await broadcast_json(room, {"type": "presence", "seats": seats, "host": room.record["host"] in online})


async def on_hello(rooms, room, peer, message):
    if peer.welcomed:
        raise RoomError("one hello for each socket")
    last = message.get("last_step", START_STEP)
    if not is_int(last) or last < START_STEP:
        raise RoomError("bad hello")
    peer.welcomed = True
    peer.build_ok = message.get("build") == room.record["build"]
    seat = room.seat_of(peer.uid)
    room.absent.pop(peer.uid, None)
    await peer.send_json({"type": "welcome", "seat": seat["force"] if seat else None,
                          "host": peer.uid == room.record["host"], "room": room.public(), "from": last + 1})
    for number in range(last + 1, len(room.steps)):
        await peer.send_bytes(step_message(number, room.steps[number]))
    if not peer.build_ok:
        await peer.send_json({"type": "error", "reason": "old build"})
    else:
        await rooms.give_back(room, peer.uid)
    await send_presence(room)
    await rooms.expire(room)   # the pause can end
    if room.record["state"] == "running":
        await peer.send_json(rooms.timer_message(room))


async def on_step(rooms, room, peer, data):
    if not peer.welcomed or not peer.build_ok:
        raise RoomError("old build" if peer.welcomed else "hello first")
    number, engine_bytes = room.check_step(peer.uid, data)
    room.add_step(engine_bytes, peer.uid)
    message = step_message(number, engine_bytes)
    for other in list(room.peers):
        if other is not peer:
            await other.send_bytes(message)
    drop_dead(room)
    if is_end_turn(engine_bytes):
        await rooms.flush(room)


async def on_checksum(rooms, room, peer, message):
    if not peer.welcomed:
        raise RoomError("hello first")
    result = room.report(peer.uid, message.get("step"), message.get("checksum"), message.get("acting"))
    if result:
        await rooms.save(room)
    if result == "acting":
        await rooms.start_deadline(room)
    if result == "desync":
        await broadcast_json(room, {"type": "desync", "step": room.record["desync_step"]})


async def on_chat(rooms, room, peer, message):
    text = clean_text(message.get("text"), MAX_CHAT_CHARS)
    if text:
        await broadcast_json(room, {"type": "chat", "name": clean_text(peer.user["name"], MAX_NAME_CHARS), "text": text})


HANDLERS = {"hello": on_hello, "checksum": on_checksum, "chat": on_chat}


async def handle_text(rooms, room, peer, text):
    """Give False if the socket went past the limit of the messages."""
    if peer.over_limit(time.monotonic(), False):
        return False
    try:
        message = json.loads(text)
    except ValueError:
        raise RoomError("bad message")
    handler = HANDLERS.get(message.get("type")) if isinstance(message, dict) else None
    if handler is None:
        raise RoomError("bad message")
    await handler(rooms, room, peer, message)
    return True


async def handle_binary(rooms, room, peer, data):
    """Give False if the socket went past the limit of the steps."""
    if peer.over_limit(time.monotonic(), True):
        return False
    await on_step(rooms, room, peer, data)
    return True


async def leave(rooms, room, peer):
    """The socket of the peer closed. The caller holds the lock of the room."""
    room.peers.discard(peer)
    if not any(other.uid == peer.uid for other in room.peers):
        room.absent[peer.uid] = rooms.clock()
    await send_presence(room)


async def timer_loop(rooms):
    """Apply the deadlines of the rooms in memory each TIMER_SECONDS."""
    while True:
        await asyncio.sleep(TIMER_SECONDS)
        try:
            await rooms.expire_all()
        except Exception as error:   # the loop must go on: the next pass tries again
            print(f"rooms: the timers failed: {error}", flush=True)


async def flush_loop(rooms):
    """Write the new steps of each room to the storage each FLUSH_SECONDS."""
    while True:
        await asyncio.sleep(FLUSH_SECONDS)
        try:
            await rooms.flush_all()
        except Exception as error:   # the loop must go on: the next flush tries again
            print(f"rooms: the flush failed: {error}", flush=True)


def flush_context(rooms):
    """An aiohttp cleanup context: the flush loop runs, and the stop writes each room."""
    async def context(app):
        tasks = [asyncio.create_task(flush_loop(rooms)), asyncio.create_task(timer_loop(rooms))]
        yield
        for task in tasks:
            task.cancel()
        await rooms.flush_all()
    return context
