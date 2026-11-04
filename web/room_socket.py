"""The socket of a room of a shared game (docs/MULTIPLAYER.md, MP2).

Each member of a room has one socket. The socket carries the steps as binary
messages and the hello, the checksums and the chat as JSON. The handlers get
the Rooms of rooms.py as an argument, so this module does not import it.
"""
import asyncio
import json
import time

from room_state import (MAX_NAME_CHARS, START_STEP, RoomError, clean_text, is_end_turn, is_int,
                        step_message)

MAX_STEPS_PER_SECOND = 20
MAX_CHAT_CHARS = 300
CHAT_HISTORY = 50         # MP7: the room keeps the last lines of the chat
FLUSH_SECONDS = 30
PING_SECONDS = 20         # the proxy of fly.io closes an idle connection after 60 seconds
TIMER_SECONDS = 2         # the loop of the deadlines
MAX_MESSAGES_PER_SECOND = 40      # all the messages of one socket
SEND_SECONDS = 5                  # a socket that does not take a message in this time leaves the room


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
    players = [{"force": seat["force"], "user": seat["user"]} for seat in room.record["seats"] if seat.get("user")]
    auto = [seat["force"] for seat in room.record["seats"] if seat.get("auto")]
    await broadcast_json(room, {"type": "presence", "seats": seats, "host": room.record["host"] in online, "players": players, "auto": auto})


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
        await rooms.give_factions_back(room)
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
    if not room.is_member(peer.uid):
        return   # MP7: the report of a spectator counts for nothing
    result = room.report(peer.uid, message.get("step"), message.get("checksum"), message.get("acting"))
    if result:
        await rooms.save(room)
    if result == "acting":
        await rooms.start_deadline(room)
    if result == "desync":
        await broadcast_json(room, {"type": "desync", "step": room.record["desync_step"]})
    if result in ("desync", "judge"):
        await rooms.judge(room)   # MP6: the majority or the referee decides after the last report
    await rooms.hand_factions_to_ai(room)
    await rooms.give_factions_back(room)   # a report can name a faction with a player: the burst is over


async def on_chat(rooms, room, peer, message):
    text = clean_text(message.get("text"), MAX_CHAT_CHARS)
    if text:
        line = {"name": clean_text(peer.user["name"], MAX_NAME_CHARS), "text": text}
        history = room.record.setdefault("chat", [])   # MP7: a page that joins later shows the last lines
        history.append(line)
        del history[:-CHAT_HISTORY]
        await broadcast_json(room, dict(line, type="chat"))


async def on_auto(rooms, room, peer, message):
    """The button of the banner: the AI plays the faction of the player, until the next click."""
    if not peer.welcomed:
        raise RoomError("hello first")
    seat = room.seat_of(peer.uid)
    if seat is None:
        raise RoomError("no seat in this game")
    if not isinstance(message.get("on"), bool):
        raise RoomError("bad message")
    seat["auto"] = message["on"]
    await rooms.save(room)
    await rooms.hand_factions_to_ai(room)
    await rooms.give_factions_back(room)
    await send_presence(room)


HANDLERS = {"hello": on_hello, "checksum": on_checksum, "chat": on_chat, "auto": on_auto}


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
