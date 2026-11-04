"""The rooms of the shared games (docs/MULTIPLAYER.md, MP2).

The server keeps the one order of the steps of each game and relays each
step to the other players. room_state.py has the format of a step and the
checks of a room. room_socket.py has the messages of a socket.

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
import secrets
import struct
import time

# The callers use rooms.X for each name of the two modules.
from room_state import *    # noqa: F401,F403  the format of a step, the limits and Room
from room_socket import *   # noqa: F401,F403  Peer, the handlers of the messages and the loops


class Rooms:
    """The rooms of the server. A room loads from the storage at its first use."""

    def __init__(self, store, clock=time.time, build="", referee=None):
        self.store = store
        self.clock = clock
        # MP6: async referee(record, first_step, image, steps) gives the checksum after each step (referee.py)
        self.referee = referee
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
            "spectators": True,
            "late_join": True,
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

    async def room_of_channel(self, channel):
        """MP8: the room of a voice channel of Discord, or None. A bad id gives a RoomError."""
        if not isinstance(channel, str) or not CHANNEL.fullmatch(channel):
            raise RoomError("bad voice channel")
        gid = await self.store.get(channel_key(channel))
        return await self.get(gid.decode("ascii", "replace")) if gid else None

    async def link_channel(self, room, channel):
        """MP8: the Activity of the voice channel opens this room."""
        if not isinstance(channel, str) or not CHANNEL.fullmatch(channel):
            raise RoomError("bad voice channel")
        room.record["channel"] = channel
        await self.store.put(channel_key(channel), room.gid.encode("ascii"))
        await self.save(room)

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
        if not room.is_viewer(user["uid"]):
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

    def check_snapshot(self, room, user, step, checksum):
        """Only the host saves. The checksum must be the checksum of the reports of that step."""
        if not room.is_member(user["uid"]):
            raise RoomError("no seat in this game")
        if user["uid"] != room.record["host"]:
            raise RoomError("only the host saves the game")
        if room.record["state"] != "running":
            raise RoomError("the game does not run")
        if not is_int(step) or not 0 <= step < len(room.steps):
            raise RoomError("no such step")
        if not isinstance(checksum, str) or not CHECKSUM.fullmatch(checksum):
            raise RoomError("bad checksum")
        report = room.first_report(step)
        if report is None:
            raise RoomError("no report of the step")
        if report[0] != checksum:
            raise RoomError("the checksum of the snapshot differs")

    async def put_snapshot(self, room, user, step, checksum, data):
        """A snapshot through the server: the image of the state after the step. A page uses
        this way if it cannot reach the bucket, for example in the frame of a Discord Activity."""
        self.check_snapshot(room, user, step, checksum)
        if not data or len(data) > MAX_START_IMAGE_BYTES:
            raise RoomError("bad snapshot")
        await self.store.put(snapshot_key(room.gid, step), data)
        await self.record_snapshot(room, step, checksum)

    async def snapshot_upload_url(self, room, user, step, checksum):
        """A signed address for a direct upload of the host to the bucket, or None without a bucket."""
        self.check_snapshot(room, user, step, checksum)
        if not hasattr(self.store, "presigned_put"):
            return None
        return self.store.presigned_put(snapshot_key(room.gid, step), SNAPSHOT_URL_SECONDS)

    async def finish_snapshot(self, room, user, step, checksum):
        """The host uploaded the file. The signed address has no limit of size, so the server checks it here."""
        self.check_snapshot(room, user, step, checksum)
        key = snapshot_key(room.gid, step)
        size = await self.store.size(key)
        if not size:
            raise RoomError("no file of the snapshot")
        if size > MAX_START_IMAGE_BYTES:
            await self.store.delete(key)
            raise RoomError("bad snapshot")
        await self.record_snapshot(room, step, checksum)

    async def record_snapshot(self, room, step, checksum):
        snapshots = room.record.setdefault("snapshots", {})   # the step -> the checksum of each kept snapshot
        snapshots[str(step)] = checksum
        for old in sorted(snapshots, key=int)[:-KEEP_SNAPSHOTS]:
            await self.store.delete(snapshot_key(room.gid, int(old)))
            del snapshots[old]
        room.record["snapshot"] = max(int(kept) for kept in snapshots)
        await self.save(room)

    async def drop_snapshots(self, room, step, good):
        """After a desync: a snapshot of the step or later with another checksum than the good one goes away."""
        snapshots = room.record.get("snapshots", {})
        for kept in [kept for kept in snapshots if int(kept) >= step and snapshots[kept] != good]:
            await self.store.delete(snapshot_key(room.gid, int(kept)))
            del snapshots[kept]
        room.record["snapshot"] = max((int(kept) for kept in snapshots), default=None)

    async def base_before(self, room, step):
        """The latest snapshot before the step, or the start image: (the first step after it, the image)."""
        snapshots = [int(kept) for kept in room.record.get("snapshots", {}) if int(kept) < step]
        for kept in sorted(snapshots, reverse=True):
            data = await self.store.get(snapshot_key(room.gid, kept))
            if data:
                return kept + 1, data
        return 0, await self.store.get(start_key(room.gid))

    # ---- the recovery from a desync (MP6) ----

    async def put_desync_image(self, room, user, step, data):
        """The image of the state of a player after the step of a desync, for the developer.
        The room can decide before the image comes, so the last desync of the room stays valid."""
        if not room.is_member(user["uid"]):
            raise RoomError("no seat in this game")
        if step != room.record.get("last_desync"):
            raise RoomError("no desync at this step")
        if not data or len(data) > MAX_START_IMAGE_BYTES:
            raise RoomError("bad state image")
        await self.store.put(desync_key(room.gid, step, user["uid"]), data)

    async def judge(self, room):
        """Decide a desync after the reports of all players with a socket. With three
        reports or more, a strict majority wins. Else the referee replays the log from the
        latest snapshot before the step and gives the good checksum. The caller holds the
        lock of the room."""
        step = room.record["desync_step"]
        reports = room.reports.get(step, {})
        online = {peer.uid for peer in room.peers if room.is_member(peer.uid)}
        if not online <= set(reports):
            return
        counts = {}
        for value in reports.values():
            counts[value] = counts.get(value, 0) + 1
        best = max(counts, key=counts.get)
        if not (len(reports) >= 3 and 2 * counts[best] > len(reports)):
            best = await self.ask_referee(room, step, counts)
            if best is None:
                return
        await self.drop_snapshots(room, step, best[0])
        record = room.record
        record["state"] = "running"
        record.pop("desync_step", None)
        record["acting"] = best[1]
        room.reports[step] = {uid: value for uid, value in reports.items() if value == best}
        for peer in list(room.peers):
            if peer.uid in reports and reports[peer.uid] != best:
                await peer.send_json({"type": "reload", "step": step})   # the page loads the good state again
        await broadcast_json(room, {"type": "desync_end", "step": step})
        await self.start_deadline(room)

    async def ask_referee(self, room, step, counts):
        """The report that the referee agrees with, or None. Each player gets the reason of a None."""
        if not self.referee:
            await broadcast_json(room, {"type": "error", "reason": "no referee"})
            return None
        first_step, image = await self.base_before(room, step)
        try:
            checksums = await self.referee(room.record, first_step, image, room.steps[first_step:step + 1])
        except Exception as error:   # the referee is another process: its failure must not stop the server
            print(f"rooms: the referee failed: {error}", flush=True)
            checksums = []
        good = checksums[-1] if checksums else None
        best = next((value for value in counts if value[0] == good), None)
        if best is None:
            await broadcast_json(room, {"type": "error", "reason": "the referee agrees with no player"})
        return best

    async def latest_snapshot_step(self, room, user):
        """The step of the latest snapshot, or None."""
        if not room.is_viewer(user["uid"]):
            raise RoomError("no seat in this game")
        return room.record.get("snapshot")

    async def latest_snapshot(self, room, user):
        """The step and the bytes of the latest snapshot, or None."""
        step = await self.latest_snapshot_step(room, user)
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
            await self.take_late_seat(room, user, force)
            return
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

    async def take_late_seat(self, room, user, force):
        """MP9: a member takes a free seat of a game that runs. The AI keeps the faction
        until the hello of the player (give_factions_back)."""
        record = room.record
        if record["state"] == "desync":
            raise RoomError("desync")
        if record["state"] != "running" or force is None or not record.get("late_join"):
            raise RoomError("the game started")
        seat = next((s for s in record["seats"] if s["force"] == force), None) if is_int(force) else None
        if seat is None:
            raise RoomError("no such seat")
        if room.seat_of(user["uid"]):
            raise RoomError("you have a seat")
        if seat.get("uid"):
            raise RoomError("the seat is taken")
        if sum(1 for s in record["seats"] if s.get("uid")) >= MAX_SEATS:
            raise RoomError("the room is full")
        seat.update(uid=user["uid"], user=clean_text(user["name"], MAX_NAME_CHARS), ai=True)
        if user["uid"] in record.get("watchers", []):
            record["watchers"].remove(user["uid"])
        await self.store.put(member_prefix(user["uid"]) + room.gid, b"")
        await self.save(room)

    async def set_options(self, room, user, options):
        """The host sets the mode, the time of a turn and the pause option before the start."""
        if user["uid"] != room.record["host"]:
            raise RoomError("only the host sets the options")
        if room.record["state"] != "open":
            raise RoomError("the game started")
        mode, turn_seconds, pause = options.get("mode"), options.get("turn_seconds"), options.get("pause_absent")
        spectators = options.get("spectators", room.record.get("spectators", True))
        late_join = options.get("late_join", room.record.get("late_join", True))
        check_options(mode, turn_seconds)
        if not isinstance(pause, bool) or not isinstance(spectators, bool) or not isinstance(late_join, bool):
            raise RoomError("bad option")
        room.record.update(mode=mode, turn_seconds=turn_seconds, pause_absent=pause, spectators=spectators, late_join=late_join)
        await self.save(room)

    async def watch(self, room, user):
        """MP7: a member of the house watches the game. The host decides if spectators can (Q-11)."""
        if room.is_viewer(user["uid"]):
            return
        if not room.record.get("spectators", True):
            raise RoomError("the host allows no spectators")
        if room.record["state"] == "open":
            raise RoomError("the game did not start")
        watchers = room.record.setdefault("watchers", [])
        if len(watchers) >= MAX_WATCHERS:
            raise RoomError("the room has no place for a spectator")
        watchers.append(user["uid"])
        await self.save(room)

    # ---- the timers (MP4) ----

    def timer_message(self, room):
        record = room.record
        deadline = record.get("deadline")
        seconds = max(0, round(deadline - self.clock())) if deadline is not None else None
        return {"type": "timer", "acting": record["acting"], "seconds": seconds, "paused": bool(record.get("paused"))}

    async def start_deadline(self, room):
        """The faction that acts changed: its turn gets the full time. No faction acts in a game
        of the AI alone: the host ends the game turn, and the room has no deadline."""
        if room.record["acting"] == NO_FACTION and any(peer.uid == room.record["host"] for peer in room.peers):
            room.record["deadline"] = None
            await self.save(room)
            return
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

    async def give_factions_back(self, room):
        """Each seat of the AI with its player online goes to the player. A server step takes the
        next number, so it waits while the host can have steps for the AI on the way."""
        if room.record["state"] != "running" or not room.no_steps_of_the_ai():
            return
        online = {peer.uid for peer in room.peers if peer.welcomed and peer.build_ok}
        seats = [seat for seat in room.record["seats"] if seat.get("ai") and not seat.get("auto") and seat.get("uid") in online]
        for seat in seats:
            await self.server_step(room, STEP_HUMAN, seat["force"], 1)
            seat["ai"] = False
            room.record.setdefault("missed", {})[str(seat["force"])] = 0
        if seats:
            if room.record["acting"] == NO_FACTION and not any(peer.uid == room.record["host"] for peer in room.peers):
                await self.server_step(room, STEP_END_TURN, NO_FACTION)   # no host ends the game turn of the AI
            await self.flush(room)

    async def hand_factions_to_ai(self, room):
        """Each seat with the choice of its player goes to the AI (the button of the banner). If the
        faction acts, the AI finishes its turn. The player stays in the room and watches."""
        if room.record["state"] != "running" or not room.no_steps_of_the_ai():
            return
        acting = room.record["acting"]
        seats = [seat for seat in room.record["seats"] if seat.get("auto") and seat.get("uid") and not seat.get("ai")]
        for seat in seats:
            if seat["force"] != acting:
                await self.server_step(room, STEP_HUMAN, seat["force"], 0)
                seat["ai"] = True
        for seat in seats:
            if seat["force"] == acting:
                await self.server_step(room, STEP_AI_TURN, acting)
                seat["ai"] = True
                room.record["acting"] = NO_FACTION
                room.record["deadline"] = None
                await broadcast_json(room, self.timer_message(room))
        if seats:
            await self.flush(room)

    async def expire_all(self):
        for room in list(self.rooms.values()):
            async with room.lock:
                await self.expire(room)

    # ---- the early end of a game ----

    async def vote_to_end(self, room, user, on):
        """The host ends the game at once. Another player votes, and the game ends after
        each player with a seat agrees. An absent player must also agree: a correspondence
        game does not end while its players are away. Give True if the game ended."""
        record = room.record
        if record["state"] not in ("running", "desync"):
            raise RoomError("the game does not run")
        if on and user["uid"] == record["host"]:
            await self.end_game(room, clean_text(user["name"], MAX_NAME_CHARS))
            return True
        if room.seat_of(user["uid"]) is None:
            raise RoomError("no seat in this game")
        votes = record.setdefault("end_votes", [])
        if on and user["uid"] not in votes:
            votes.append(user["uid"])
        if not on and user["uid"] in votes:
            votes.remove(user["uid"])
        if room.end_players() <= set(votes):
            await self.end_game(room, None)
            return True
        await self.save(room)
        return False

    async def end_game(self, room, by):
        """The game ends for each page. The log, the snapshots and the invite code stay, so
        that a member can open the last state. The voice channel can take a new room."""
        record = room.record
        record.update(state="ended", ended=int(time.time()), ended_by=by, deadline=None, paused=False)
        for key in ("desync_step", "end_votes", "remaining"):
            record.pop(key, None)
        channel = record.pop("channel", None)
        if channel:
            await self.store.delete(channel_key(channel))
        await self.flush(room)
        await broadcast_json(room, {"type": "ended", "by": by})
        print(f"rooms: the game {room.gid} ended", flush=True)

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
        # MP9: the start image makes these factions human. A later seat comes with a step of the log.
        room.record["start_humans"] = [seat["force"] for seat in room.record["seats"] if seat.get("uid")]
        await self.save(room)
