"""The tests of the room server (web/rooms.py; docs/MULTIPLAYER.md, MP2).

Run: C:\\tools\\hexdom-venv\\Scripts\\python -I tests\\server\\rooms_tests.py

The tests use FolderStorage in a temporary folder and a fake socket. A
scripted client takes the place of the game: it sends the bytes of the
steps that the engine makes, with the faction that acted in byte 3.
"""
import asyncio
import json
import os
import struct
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True   # no __pycache__ in web/
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "web"))
import rooms     # noqa: E402
import storage   # noqa: E402

HOST = {"uid": "a" * 24, "name": "Host"}
GUEST = {"uid": "b" * 24, "name": "Guest"}
OTHER = {"uid": "c" * 24, "name": "Other"}
FORCE_HOST, FORCE_GUEST, FORCE_AI = 2, 5, 6
BUILD = "0123456789abcdef"
SIM_COMMAND_END_TURN, SIM_COMMAND_PATROL = 1, 5


class FakeSocket:
    def __init__(self):
        self.closed = False
        self.texts = []
        self.binaries = []

    async def send_str(self, text):
        self.texts.append(json.loads(text))

    async def send_bytes(self, data):
        self.binaries.append(bytes(data))

    def errors(self):
        return [message["reason"] for message in self.texts if message["type"] == "error"]


def engine_step(acting, kind=rooms.STEP_COMMAND, command=SIM_COMMAND_PATROL):
    """The bytes of a step of the engine: the header, then the kind of the command and its fields."""
    head = bytes([1, kind, 0, acting]) + struct.pack("<I", 0)
    if kind == rooms.STEP_COMMAND:
        return head + struct.pack("<I", command) + bytes(32)
    return head + bytes(11)


def socket_step(number, acting, kind=rooms.STEP_COMMAND, command=SIM_COMMAND_PATROL):
    return struct.pack("<II", number, 0) + engine_step(acting, kind, command)


def server_step(number, kind, force, acting, value=0):
    """A step of the server on the socket (MP4): the header of the engine only."""
    return struct.pack("<II", number, 0) + bytes([1, kind, force, acting]) + struct.pack("<I", value)


class Clock:
    """The clock of the rooms. A test moves it forward."""

    def __init__(self):
        self.now = 1_000_000.0

    def __call__(self):
        return self.now


class RoomsTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.store = storage.FolderStorage(self.folder.name)
        self.clock = Clock()
        self.rooms = rooms.Rooms(self.store, clock=self.clock)

    async def asyncTearDown(self):
        self.folder.cleanup()

    async def make_room(self, image=True):
        factions = [{"force": FORCE_HOST, "name": "Cao"}, {"force": FORCE_GUEST, "name": "Liu"}, {"force": FORCE_AI, "name": "Sun"}]
        room = await self.rooms.create(HOST, "Test game", "SCEN002", "placeholder", BUILD, "live", 300, factions)
        await self.rooms.take_seat(room, HOST, FORCE_HOST)
        await self.rooms.take_seat(room, GUEST, FORCE_GUEST)
        if image:
            await self.rooms.put_start_image(room, HOST, b"start image")
        return room

    async def connect(self, room, user, build=BUILD, last_step=-1, socket=None):
        peer = rooms.Peer(socket or FakeSocket(), user)
        rooms.join(room, peer)
        await rooms.handle_text(self.rooms, room, peer, json.dumps({"type": "hello", "gid": room.gid, "build": build,
                                                                   "last_step": last_step}))
        return peer

    async def send(self, room, peer, data):
        """Send a message as the socket handler of server.py does. Give the error, or None."""
        try:
            if isinstance(data, (bytes, bytearray)):
                await rooms.handle_binary(self.rooms, room, peer, data)
            else:
                await rooms.handle_text(self.rooms, room, peer, json.dumps(data))
        except rooms.RoomError as error:
            return str(error)
        return None

    async def started_room(self, acting=FORCE_HOST):
        """A running room with the host and the guest online. The start report names the faction that acts."""
        room = await self.make_room()
        await self.rooms.start(room, HOST)
        host = await self.connect(room, HOST)
        guest = await self.connect(room, GUEST)
        await self.send(room, host, {"type": "checksum", "step": -1, "checksum": "0" * 16, "acting": acting})
        return room, host, guest

    async def test_a_room_has_an_invite_code_and_free_seats(self):
        room = await self.rooms.create(HOST, "<b>Game</b>", "SCEN002", "placeholder", BUILD, "live", 300,
                                       [{"force": FORCE_HOST, "name": "Cao"}])
        self.assertRegex(room.record["code"], r"^[A-HJKMNP-Z]{6}$")
        self.assertEqual(room.record["state"], "open")
        self.assertIsNone(room.record["seats"][0]["uid"])
        self.assertNotIn("host", room.public())
        self.assertEqual([item["name"] for item in await self.store.list(rooms.member_prefix(HOST["uid"]))], [room.gid])
        with self.assertRaises(rooms.RoomError):
            await self.rooms.create(HOST, "Game", "S", "p", BUILD, "blitz", 300, [{"force": 1}])
        with self.assertRaises(rooms.RoomError):
            await self.rooms.create(HOST, "Game", "S", "p", BUILD, "live", 5, [{"force": 1}])
        with self.assertRaises(rooms.RoomError):
            await self.rooms.create(HOST, "Game", "S", "p", BUILD, "live", 300, [{"force": 1}, {"force": 1}])

    async def test_a_seat_has_one_player(self):
        room = await self.make_room()
        with self.assertRaises(rooms.RoomError):
            await self.rooms.take_seat(room, OTHER, FORCE_GUEST)
        await self.rooms.take_seat(room, GUEST, FORCE_AI)
        self.assertEqual(room.seat_of(GUEST["uid"])["force"], FORCE_AI)
        self.assertTrue(next(s for s in room.record["seats"] if s["force"] == FORCE_GUEST)["ai"])
        await self.rooms.take_seat(room, GUEST, None)
        self.assertIsNone(room.seat_of(GUEST["uid"]))
        with self.assertRaises(rooms.RoomError):
            await self.rooms.start(room, GUEST)
        await self.rooms.start(room, HOST)
        with self.assertRaises(rooms.RoomError):
            await self.rooms.take_seat(room, GUEST, FORCE_GUEST)

    async def test_the_welcome_names_the_seat_and_the_presence_follows(self):
        room, host, guest = await self.started_room()
        welcome = next(m for m in guest.socket.texts if m["type"] == "welcome")
        self.assertEqual(welcome["seat"], FORCE_GUEST)
        self.assertFalse(welcome["host"])
        presence = [m for m in host.socket.texts if m["type"] == "presence"][-1]
        self.assertEqual(presence["seats"], [FORCE_HOST, FORCE_GUEST])

    async def test_the_server_relays_a_step_to_the_other_players(self):
        room, host, guest = await self.started_room()
        self.assertIsNone(await self.send(room, host, socket_step(0, FORCE_HOST)))
        self.assertEqual(guest.socket.binaries, [socket_step(0, FORCE_HOST)])
        self.assertEqual(host.socket.binaries, [])
        self.assertEqual(len(room.steps), 1)

    async def test_a_step_from_the_wrong_seat_gets_an_error(self):
        room, host, guest = await self.started_room()
        self.assertEqual(await self.send(room, guest, socket_step(0, FORCE_HOST)), "not your seat")
        self.assertEqual(await self.send(room, guest, socket_step(0, FORCE_GUEST)), "not your seat")
        self.assertEqual(room.steps, [])
        self.assertEqual(host.socket.binaries, [])

    async def test_a_step_with_an_old_number_does_nothing(self):
        room, host, guest = await self.started_room()
        await self.send(room, host, socket_step(0, FORCE_HOST))
        self.assertEqual(await self.send(room, host, socket_step(0, FORCE_HOST)), "old step")
        self.assertEqual(await self.send(room, host, socket_step(5, FORCE_HOST)), "step out of order")
        self.assertEqual(len(room.steps), 1)
        self.assertEqual(len(guest.socket.binaries), 1)

    async def test_the_checksum_report_passes_the_turn_to_the_next_seat(self):
        room, host, guest = await self.started_room()
        await self.send(room, host, socket_step(0, FORCE_HOST, command=SIM_COMMAND_END_TURN))
        await self.send(room, host, {"type": "checksum", "step": 0, "checksum": "1" * 16, "acting": FORCE_GUEST})
        await self.send(room, guest, {"type": "checksum", "step": 0, "checksum": "1" * 16, "acting": FORCE_GUEST})
        self.assertEqual(room.record["state"], "running")
        self.assertEqual(await self.send(room, host, socket_step(1, FORCE_GUEST)), "not your seat")
        self.assertIsNone(await self.send(room, guest, socket_step(1, FORCE_GUEST)))

    async def test_the_host_is_the_author_for_a_faction_without_a_player(self):
        room, host, guest = await self.started_room(acting=FORCE_AI)
        self.assertEqual(await self.send(room, guest, socket_step(0, FORCE_AI)), "not your seat")
        self.assertIsNone(await self.send(room, host, socket_step(0, FORCE_AI, command=SIM_COMMAND_END_TURN)))

    async def test_only_the_host_changes_the_settings_and_no_player_changes_the_seats(self):
        room, host, guest = await self.started_room(acting=FORCE_GUEST)
        self.assertEqual(await self.send(room, guest, socket_step(0, FORCE_GUEST, kind=rooms.STEP_SETTINGS)), "not your seat")
        self.assertEqual(await self.send(room, guest, socket_step(0, FORCE_GUEST, kind=rooms.STEP_HUMAN)), "not a step of a player")
        self.assertEqual(await self.send(room, guest, socket_step(0, FORCE_GUEST, kind=rooms.STEP_LEDGER_CLEAR)),
                         "not a step of a player")
        self.assertEqual(room.steps, [])

    async def test_two_different_checksums_for_one_step_give_a_desync(self):
        room, host, guest = await self.started_room()
        await self.send(room, host, socket_step(0, FORCE_HOST))
        await self.send(room, host, {"type": "checksum", "step": 0, "checksum": "1" * 16, "acting": FORCE_HOST})
        await self.send(room, guest, {"type": "checksum", "step": 0, "checksum": "2" * 16, "acting": FORCE_HOST})
        self.assertEqual(room.record["state"], "desync")
        for peer in (host, guest):
            self.assertIn({"type": "desync", "step": 0}, peer.socket.texts)
        self.assertEqual(await self.send(room, host, socket_step(1, FORCE_HOST)), "desync")

    async def test_a_client_of_another_build_cannot_send_a_step(self):
        room = await self.make_room()
        await self.rooms.start(room, HOST)
        host = await self.connect(room, HOST, build="ffffffffffffffff")
        self.assertIn("old build", host.socket.errors())
        await self.send(room, host, {"type": "checksum", "step": -1, "checksum": "0" * 16, "acting": FORCE_HOST})
        self.assertEqual(await self.send(room, host, socket_step(0, FORCE_HOST)), "old build")

    async def test_a_hello_after_a_lost_connection_gets_the_missed_steps(self):
        room, host, guest = await self.started_room()
        for number in range(3):
            await self.send(room, host, socket_step(number, FORCE_HOST))
        late = await self.connect(room, GUEST, last_step=0)
        self.assertEqual(late.socket.binaries, [socket_step(1, FORCE_HOST), socket_step(2, FORCE_HOST)])
        welcome = next(m for m in late.socket.texts if m["type"] == "welcome")
        self.assertEqual(welcome["from"], 1)
        self.assertEqual(welcome["room"]["step_count"], 3)

    async def test_more_than_20_steps_in_one_second_close_the_socket(self):
        room, host, guest = await self.started_room()
        results = [await rooms.handle_binary(self.rooms, room, host, socket_step(n, FORCE_HOST))
                   for n in range(rooms.MAX_STEPS_PER_SECOND)]
        self.assertTrue(all(results))
        self.assertFalse(await rooms.handle_binary(self.rooms, room, host, socket_step(rooms.MAX_STEPS_PER_SECOND, FORCE_HOST)))

    async def test_the_chat_is_one_line_of_plain_text(self):
        room, host, guest = await self.started_room()
        await self.send(room, guest, {"type": "chat", "text": "  hello\n  <b>all</b>  " + "x" * 400})
        chat = next(m for m in host.socket.texts if m["type"] == "chat")
        self.assertEqual(chat["name"], "Guest")
        self.assertTrue(chat["text"].startswith("hello <b>all</b> x"))
        self.assertEqual(len(chat["text"]), rooms.MAX_CHAT_CHARS)
        self.assertNotIn("uid", chat)

    async def test_bad_messages_get_an_error(self):
        room, host, guest = await self.started_room()
        self.assertEqual(await self.send(room, host, {"type": "nothing"}), "bad message")
        self.assertEqual(await self.send(room, host, {"type": "checksum", "step": 9, "checksum": "0" * 16, "acting": 1}), "no such step")
        self.assertEqual(await self.send(room, host, {"type": "checksum", "step": -1, "checksum": "xyz", "acting": 1}), "bad checksum")
        self.assertEqual(await self.send(room, host, b"\x00\x01"), "bad step")

    async def test_a_stop_and_a_start_of_the_server_keep_the_room_and_the_log(self):
        room, host, guest = await self.started_room()
        await self.send(room, host, socket_step(0, FORCE_HOST))
        await self.send(room, host, socket_step(1, FORCE_HOST, command=SIM_COMMAND_END_TURN))   # an end_turn writes the log
        self.assertEqual([item["name"] for item in await self.store.list(rooms.log_prefix(room.gid))], ["00000000-00000001.bin"])
        await self.send(room, host, {"type": "checksum", "step": 1, "checksum": "1" * 16, "acting": FORCE_GUEST})
        await self.send(room, guest, socket_step(2, FORCE_GUEST))
        await self.rooms.flush_all()

        again = rooms.Rooms(self.store, clock=self.clock)
        loaded = await again.get(room.gid)
        self.assertIsNot(loaded, room)
        self.assertEqual(loaded.steps, room.steps)
        self.assertEqual(loaded.record["state"], "running")
        self.assertEqual(loaded.record["acting"], FORCE_GUEST)
        self.assertEqual(loaded.record["seats"], room.record["seats"])
        self.assertEqual([r.gid for r in await again.games_of(GUEST["uid"])], [room.gid])
        peer = rooms.Peer(FakeSocket(), GUEST)
        loaded.peers.add(peer)
        await rooms.handle_text(again, loaded, peer, json.dumps({"type": "hello", "build": BUILD, "last_step": -1}))
        self.assertEqual(len(peer.socket.binaries), 3)

    async def test_the_report_of_another_player_does_not_pass_the_turn(self):
        room, host, guest = await self.started_room()
        await self.send(room, host, socket_step(0, FORCE_HOST, command=SIM_COMMAND_END_TURN))
        await self.send(room, guest, {"type": "checksum", "step": 0, "checksum": "1" * 16, "acting": FORCE_GUEST})
        self.assertEqual(room.record["acting"], FORCE_HOST)
        self.assertEqual(await self.send(room, guest, socket_step(1, FORCE_GUEST)), "not your seat")
        await self.send(room, host, {"type": "checksum", "step": 0, "checksum": "1" * 16, "acting": FORCE_GUEST})
        self.assertEqual(room.record["acting"], FORCE_GUEST)
        self.assertIsNone(await self.send(room, guest, socket_step(1, FORCE_GUEST)))

    async def test_only_the_report_of_the_host_names_the_first_faction(self):
        room = await self.make_room()
        host = await self.connect(room, HOST)
        guest = await self.connect(room, GUEST)
        report = {"type": "checksum", "step": -1, "checksum": "0" * 16, "acting": FORCE_GUEST}
        self.assertEqual(await self.send(room, host, report), "the game does not run")
        await self.rooms.start(room, HOST)
        self.assertIsNone(await self.send(room, guest, report))           # a guest reports the start too
        self.assertEqual(room.record["acting"], rooms.NO_FACTION)
        self.assertIsNone(await self.send(room, host, report))
        self.assertEqual(room.record["acting"], FORCE_GUEST)
        other = dict(report, checksum="1" * 16)
        self.assertIsNone(await self.send(room, guest, other))            # another start state of the guest
        self.assertEqual(room.record["state"], "desync")                  # two start states differ

    async def test_a_player_who_joins_again_reports_the_start_state_without_an_error(self):
        room, host, guest = await self.started_room()
        await self.send(room, host, socket_step(0, FORCE_HOST))
        late = await self.connect(room, GUEST)
        await self.report(room, host, 0, FORCE_GUEST)                         # the faction of the guest acts now
        self.assertIsNone(await self.report(room, late, -1, FORCE_HOST, "0" * 16))
        self.assertEqual(room.record["acting"], FORCE_GUEST)                 # a late start report changes no faction
        self.assertEqual(room.record["state"], "running")
        self.assertIsNone(await self.report(room, late, -1, FORCE_HOST, "f" * 16))
        self.assertEqual(room.record["state"], "desync")                     # another start state is a desync

    async def test_a_dead_socket_does_not_stop_the_relay(self):
        room, host, guest = await self.started_room()

        class BrokenSocket(FakeSocket):
            async def send_bytes(self, data):
                raise ConnectionResetError("the peer went away")

        other = await self.connect(room, OTHER, socket=BrokenSocket())
        self.assertIsNone(await self.send(room, host, socket_step(0, FORCE_HOST)))
        self.assertEqual(guest.socket.binaries, [socket_step(0, FORCE_HOST)])
        self.assertTrue(other.dead)
        self.assertNotIn(other, room.peers)
        self.assertIsNone(await self.send(room, host, socket_step(1, FORCE_HOST)))
        self.assertEqual(len(guest.socket.binaries), 2)

    async def test_a_new_socket_of_a_member_replaces_the_old_one(self):
        room, host, guest = await self.started_room()
        again = await self.connect(room, GUEST)
        self.assertTrue(guest.dead)
        self.assertEqual([peer for peer in room.peers if peer.uid == GUEST["uid"]], [again])

    async def test_a_socket_has_one_hello_and_a_limit_of_messages(self):
        room, host, guest = await self.started_room()
        self.assertEqual(await self.send(room, guest, {"type": "hello", "build": BUILD, "last_step": -1}), "one hello for each socket")
        results = [await rooms.handle_text(self.rooms, room, guest, json.dumps({"type": "chat", "text": "hi"}))
                   for _ in range(rooms.MAX_MESSAGES_PER_SECOND)]
        self.assertIn(False, results)

    async def test_the_log_of_a_room_has_a_limit(self):
        room, host, guest = await self.started_room()
        room.step_bytes = rooms.MAX_ROOM_BYTES
        self.assertEqual(await self.send(room, host, socket_step(0, FORCE_HOST)), "the log of the game is full")

    async def test_two_loads_of_one_game_give_one_room(self):
        room = await self.make_room()
        again = rooms.Rooms(self.store, clock=self.clock)
        first, second = await asyncio.gather(again.get(room.gid), again.get(room.gid))
        self.assertIs(first, second)

    async def test_json_true_is_no_number_and_a_list_is_no_mode(self):
        with self.assertRaises(rooms.RoomError):
            await self.rooms.create(HOST, "Game", "S", "p", BUILD, [], 300, [{"force": 1}])
        with self.assertRaises(rooms.RoomError):
            await self.rooms.create(HOST, "Game", "S", "p", BUILD, "live", 300, [{"force": True}])
        room = await self.make_room()
        with self.assertRaises(rooms.RoomError):
            await self.rooms.take_seat(room, OTHER, True)

    async def test_a_player_who_leaves_the_seats_loses_the_game_from_the_list(self):
        room = await self.make_room()
        self.assertEqual([r.gid for r in await self.rooms.games_of(GUEST["uid"])], [room.gid])
        await self.rooms.take_seat(room, GUEST, None)
        self.assertEqual(await self.rooms.games_of(GUEST["uid"]), [])
        await self.rooms.take_seat(room, HOST, None)
        self.assertEqual([r.gid for r in await self.rooms.games_of(HOST["uid"])], [room.gid])

    async def test_the_start_needs_the_start_image_of_the_host(self):
        room = await self.make_room(image=False)
        with self.assertRaises(rooms.RoomError):
            await self.rooms.start(room, HOST)
        with self.assertRaises(rooms.RoomError):
            await self.rooms.put_start_image(room, GUEST, b"image")
        with self.assertRaises(rooms.RoomError):
            await self.rooms.put_start_image(room, HOST, b"")
        await self.rooms.put_start_image(room, HOST, b"image")
        with self.assertRaises(rooms.RoomError):
            await self.rooms.start_image(room, GUEST)   # before the start
        await self.rooms.start(room, HOST)
        self.assertEqual(await self.rooms.start_image(room, GUEST), b"image")
        with self.assertRaises(rooms.RoomError):
            await self.rooms.start_image(room, OTHER)    # no seat
        with self.assertRaises(rooms.RoomError):
            await self.rooms.put_start_image(room, HOST, b"other")

    async def test_the_invite_code_finds_the_room(self):
        room = await self.make_room()
        again = rooms.Rooms(self.store, clock=self.clock)
        self.assertEqual((await again.find_code(room.record["code"])).gid, room.gid)
        self.assertIsNone(await again.find_code("AAAAAA" if room.record["code"] != "AAAAAA" else "BBBBBB"))
        self.assertIsNone(await again.find_code("../x/y"))
        self.assertIsNone(await again.find_code("abcdef"))

    async def test_a_bad_game_id_finds_no_room(self):
        self.assertIsNone(await self.rooms.get("0123456789abcdef\n"))
        self.assertIsNone(await self.rooms.get("../users/x"))
        self.assertIsNone(await self.rooms.get("0123456789abcdef"))

    # ---- MP4: the timers, the absence and the AI takeover ----

    async def report(self, room, peer, step, acting, checksum="1" * 16):
        return await self.send(room, peer, {"type": "checksum", "step": step, "checksum": checksum, "acting": acting})

    def timers(self, peer):
        return [message for message in peer.socket.texts if message["type"] == "timer"]

    async def test_a_room_records_the_pack_as_a_folder_name(self):
        room = await self.rooms.create(HOST, "Game", "S", "medieval_west", BUILD, "live", 300, [{"force": 1}])
        self.assertEqual(room.record["pack"], "medieval_west")
        for pack in ("../saves", "", "a/b", None, "Medieval"):
            with self.assertRaises(rooms.RoomError):
                await self.rooms.create(HOST, "Game", "S", pack, BUILD, "live", 300, [{"force": 1}])

    async def test_the_host_sets_the_options_before_the_start(self):
        room = await self.make_room()
        await self.rooms.set_options(room, HOST, {"mode": "correspondence", "turn_seconds": 24 * 3600, "pause_absent": True})
        self.assertEqual((room.record["mode"], room.record["turn_seconds"], room.record["pause_absent"]),
                         ("correspondence", 24 * 3600, True))
        with self.assertRaises(rooms.RoomError):
            await self.rooms.set_options(room, GUEST, {"mode": "live", "turn_seconds": 300, "pause_absent": False})
        with self.assertRaises(rooms.RoomError):
            await self.rooms.set_options(room, HOST, {"mode": "live", "turn_seconds": 24 * 3600, "pause_absent": False})
        with self.assertRaises(rooms.RoomError):
            await self.rooms.set_options(room, HOST, {"mode": "live", "turn_seconds": 300, "pause_absent": "yes"})
        await self.rooms.start(room, HOST)
        with self.assertRaises(rooms.RoomError):
            await self.rooms.set_options(room, HOST, {"mode": "live", "turn_seconds": 300, "pause_absent": False})

    async def test_the_faction_that_acts_gets_a_deadline_and_each_page_gets_the_timer(self):
        room, host, guest = await self.started_room()
        self.assertEqual(room.record["deadline"], self.clock.now + 300)
        for peer in (host, guest):
            self.assertEqual(self.timers(peer)[-1], {"type": "timer", "acting": FORCE_HOST, "seconds": 300, "paused": False})

    async def test_a_deadline_ends_the_faction_turn_with_a_step_of_the_server(self):
        room, host, guest = await self.started_room()
        await self.rooms.expire(room)
        self.assertEqual(room.steps, [])                                    # the time is not over
        self.clock.now += 301
        await self.rooms.expire(room)
        end_turn = server_step(0, rooms.STEP_END_TURN, FORCE_HOST, FORCE_HOST)
        self.assertEqual(host.socket.binaries, [end_turn])
        self.assertEqual(guest.socket.binaries, [end_turn])
        self.assertEqual(room.record["acting"], rooms.NO_FACTION)            # no seat acts until a report
        self.assertIsNone(room.record["deadline"])
        self.assertEqual(room.record["missed"], {str(FORCE_HOST): 1})
        await self.rooms.expire(room)
        self.assertEqual(len(room.steps), 1)
        self.assertEqual([item["name"] for item in await self.store.list(rooms.log_prefix(room.gid))], ["00000000-00000000.bin"])
        self.assertIsNone(await self.report(room, guest, 0, FORCE_GUEST))   # after a step of the server, each member can report
        self.assertEqual(room.record["acting"], FORCE_GUEST)
        self.assertEqual(room.record["deadline"], self.clock.now + 300)
        self.assertIsNone(await self.send(room, guest, socket_step(1, FORCE_GUEST)))

    async def test_a_deadline_in_the_past_at_the_start_of_the_server_gives_one_end_turn_step_not_two(self):
        room, host, guest = await self.started_room()
        await self.rooms.flush_all()
        self.clock.now += 3600
        again = rooms.Rooms(self.store, clock=self.clock)
        loaded = await again.get(room.gid)
        self.assertEqual(len(loaded.steps), 1)
        await again.get(room.gid)
        await again.expire_all()
        self.assertEqual(len(loaded.steps), 1)
        third = rooms.Rooms(self.store, clock=self.clock)
        self.assertEqual(len((await third.get(room.gid)).steps), 1)

    async def end_guest_turn_by_time(self, room, host, guest):
        """The faction of the guest acts. Its time goes by, then the host acts and ends its turn."""
        self.clock.now += 301
        await self.rooms.expire(room)
        last = len(room.steps) - 1
        await self.report(room, host, last, FORCE_HOST)
        self.assertIsNone(await self.send(room, host, socket_step(last + 1, FORCE_HOST, command=SIM_COMMAND_END_TURN)))
        await self.report(room, host, last + 1, FORCE_GUEST)

    async def test_two_missed_turns_give_the_faction_to_the_ai_and_a_hello_gives_it_back(self):
        room, host, guest = await self.started_room(acting=FORCE_GUEST)
        room.peers.discard(guest)
        await self.end_guest_turn_by_time(room, host, guest)
        seat = room.seat_of(GUEST["uid"])
        self.assertFalse(seat["ai"])
        self.clock.now += 301
        await self.rooms.expire(room)
        # One step: the faction goes to the AI, and the AI finishes the faction turn.
        self.assertEqual(host.socket.binaries[-1], server_step(2, rooms.STEP_AI_TURN, FORCE_GUEST, FORCE_GUEST))
        self.assertEqual(len(room.steps), 3)
        self.assertTrue(seat["ai"])
        self.assertEqual(seat["uid"], GUEST["uid"])                          # the seat stays for the player
        self.assertTrue(room.public()["seats"][1]["ai"])

        back = await self.connect(room, GUEST, last_step=1)
        give_back = server_step(3, rooms.STEP_HUMAN, FORCE_GUEST, rooms.NO_FACTION, 1)
        self.assertEqual(back.socket.binaries[-1], give_back)
        self.assertEqual(host.socket.binaries[-1], give_back)
        self.assertFalse(seat["ai"])
        self.assertEqual(room.record["missed"][str(FORCE_GUEST)], 0)
        again = await self.connect(room, GUEST, last_step=3)
        self.assertEqual(len(room.steps), 4)                                 # one give-back, not two
        self.assertEqual(again.socket.binaries, [])

    async def test_the_ai_does_not_take_the_faction_of_a_player_with_a_connection(self):
        room, host, guest = await self.started_room(acting=FORCE_GUEST)
        await self.end_guest_turn_by_time(room, host, guest)
        self.clock.now += 301
        await self.rooms.expire(room)
        self.assertEqual(room.steps[-1][1], rooms.STEP_END_TURN)             # the guest is online: only an end_turn
        self.assertFalse(room.seat_of(GUEST["uid"])["ai"])
        self.assertEqual(room.record["missed"][str(FORCE_GUEST)], 2)

    async def test_a_step_of_the_player_resets_the_missed_turns(self):
        room, host, guest = await self.started_room(acting=FORCE_GUEST)
        await self.end_guest_turn_by_time(room, host, guest)
        self.assertIsNone(await self.send(room, guest, socket_step(2, FORCE_GUEST)))
        self.assertEqual(room.record["missed"][str(FORCE_GUEST)], 0)
        self.clock.now += 301
        await self.rooms.expire(room)
        self.assertEqual(room.steps[-1][1], rooms.STEP_END_TURN)             # one missed turn: no AI takeover
        self.assertFalse(room.seat_of(GUEST["uid"])["ai"])

    async def test_the_pause_option_stops_the_timer_while_a_player_is_absent(self):
        room = await self.make_room()
        await self.rooms.set_options(room, HOST, {"mode": "live", "turn_seconds": 300, "pause_absent": True})
        await self.rooms.start(room, HOST)
        host = await self.connect(room, HOST)
        guest = await self.connect(room, GUEST)
        await self.report(room, host, -1, FORCE_HOST, "0" * 16)
        await rooms.leave(self.rooms, room, guest)
        self.clock.now += 30
        await self.rooms.expire(room)
        self.assertFalse(room.record["paused"])                              # 60 seconds to connect again
        self.clock.now += 31
        await self.rooms.expire(room)
        self.assertTrue(room.record["paused"])
        self.assertEqual(self.timers(host)[-1], {"type": "timer", "acting": FORCE_HOST, "seconds": None, "paused": True})
        self.clock.now += 3600
        await self.rooms.expire(room)
        self.assertEqual(room.steps, [])
        await self.connect(room, GUEST)
        self.assertFalse(room.record["paused"])
        self.assertEqual(room.record["deadline"], self.clock.now + 300 - 61)

    def test_the_log_packs_and_unpacks(self):
        steps = [b"abc", b"", bytes(range(200))]
        self.assertEqual(rooms.unpack_log(rooms.pack_log(steps)), steps)
        with self.assertRaises(ValueError):
            rooms.unpack_log(rooms.pack_log(steps)[:-1])


if __name__ == "__main__":
    unittest.main(verbosity=2)
