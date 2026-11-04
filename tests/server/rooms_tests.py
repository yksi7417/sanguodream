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
        room.record["late_join"] = False
        with self.assertRaises(rooms.RoomError):
            await self.rooms.take_seat(room, GUEST, FORCE_GUEST)             # the host allows no late player

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

    # ---- the button of the banner: the AI plays the faction of a player ----

    async def test_the_button_gives_a_faction_that_waits_to_the_ai_and_the_next_click_takes_it_back(self):
        room, host, guest = await self.started_room(acting=FORCE_HOST)
        seat = room.seat_of(GUEST["uid"])
        self.assertIsNone(await self.send(room, guest, {"type": "auto", "on": True}))
        self.assertEqual(host.socket.binaries[-1], server_step(0, rooms.STEP_HUMAN, FORCE_GUEST, FORCE_HOST, 0))
        self.assertTrue(seat["ai"] and seat["auto"])
        self.assertEqual(seat["uid"], GUEST["uid"])
        self.assertEqual(guest.socket.texts[-1]["auto"], [FORCE_GUEST])
        again = await self.connect(room, GUEST, last_step=0)                 # a new page keeps the choice
        self.assertEqual(len(room.steps), 1)
        self.assertIsNone(await self.send(room, again, {"type": "auto", "on": False}))
        self.assertEqual(host.socket.binaries[-1], server_step(1, rooms.STEP_HUMAN, FORCE_GUEST, FORCE_HOST, 1))
        self.assertFalse(seat["ai"] or seat["auto"])

    def last_message(self, peer, kind):
        return next((text for text in reversed(peer.socket.texts) if text.get("type") == kind), None)

    async def test_the_host_ends_the_game_at_once(self):
        room, host, guest = await self.started_room()
        self.assertIsNone(await self.send(room, host, {"type": "end_game", "on": True}))
        self.assertEqual(room.record["state"], "ended")
        self.assertIsNone(room.record["deadline"])
        for peer in (host, guest):
            self.assertEqual(self.last_message(peer, "ended")["by"], "Host")

    async def test_a_guest_vote_ends_the_game_only_after_each_player_agrees(self):
        room, host, guest = await self.started_room()
        self.assertIsNone(await self.send(room, guest, {"type": "end_game", "on": True}))
        self.assertEqual(room.record["state"], "running")
        presence = self.last_message(host, "presence")
        self.assertEqual((presence["end_votes"], presence["end_players"]), ([FORCE_GUEST], 2))
        self.assertIsNone(await self.send(room, guest, {"type": "end_game", "on": False}))
        self.assertEqual(self.last_message(host, "presence")["end_votes"], [])
        self.assertIsNone(await self.send(room, guest, {"type": "end_game", "on": True}))
        self.assertEqual(room.record["state"], "running")
        room.record["host"] = OTHER["uid"]                                   # the player of the first seat is now only a guest
        self.assertIsNone(await self.send(room, host, {"type": "end_game", "on": True}))
        self.assertEqual(room.record["state"], "ended")
        self.assertIsNone(self.last_message(guest, "ended")["by"])

    async def test_a_person_without_a_seat_cannot_vote_and_the_vote_hides_the_uids(self):
        room, host, guest = await self.started_room()
        await self.rooms.watch(room, OTHER)
        watcher = await self.connect(room, OTHER)
        self.assertEqual(await self.send(room, watcher, {"type": "end_game", "on": True}), "no seat in this game")
        self.assertEqual(await self.send(room, guest, {"type": "end_game", "on": "yes"}), "bad message")
        await self.send(room, guest, {"type": "end_game", "on": True})
        self.assertNotIn("end_votes", room.public())
        self.assertNotIn(GUEST["uid"], json.dumps(self.last_message(host, "presence")))

    async def test_an_ended_game_takes_no_step_and_no_report_and_a_new_page_learns_the_end(self):
        room, host, guest = await self.started_room()
        await self.send(room, host, {"type": "end_game", "on": True})
        self.assertIsNone(await self.send(room, host, {"type": "checksum", "step": -1, "checksum": "0" * 16, "acting": FORCE_HOST}))
        self.assertEqual(await self.send(room, host, {"type": "end_game", "on": True}), "the game does not run")
        step = struct.pack("<II", 0, 0) + bytes([1, rooms.STEP_END_TURN, FORCE_HOST, FORCE_HOST]) + bytes(4)
        self.assertEqual(await self.send(room, host, step), "the game does not run")
        again = await self.connect(room, GUEST, build="old")
        self.assertEqual(self.last_message(again, "ended")["by"], "Host")
        self.assertEqual(again.socket.errors(), [])
        self.assertEqual(self.last_message(again, "welcome")["room"]["state"], "ended")
        reloaded = rooms.Rooms(self.store, clock=self.clock)
        self.assertEqual((await reloaded.get(room.gid)).record["state"], "ended")

    async def test_the_end_of_a_game_frees_its_voice_channel_and_can_stop_a_desync(self):
        room, host, guest = await self.started_room()
        await self.rooms.link_channel(room, "123")
        room.record["state"] = "desync"
        room.record["desync_step"] = 0
        self.assertIsNone(await self.send(room, host, {"type": "end_game", "on": True}))
        self.assertEqual(room.record["state"], "ended")
        self.assertNotIn("desync_step", room.record)
        self.assertIsNone(await self.rooms.room_of_channel("123"))

    async def test_the_button_of_the_faction_that_acts_lets_the_ai_finish_the_turn(self):
        room, host, guest = await self.started_room(acting=FORCE_GUEST)
        self.assertIsNone(await self.send(room, guest, {"type": "auto", "on": True}))
        self.assertEqual(host.socket.binaries[-1], server_step(0, rooms.STEP_AI_TURN, FORCE_GUEST, FORCE_GUEST))
        self.assertEqual(room.record["acting"], rooms.NO_FACTION)
        self.assertIsNone(room.record["deadline"])

    async def test_a_game_of_the_ai_alone_gives_a_faction_back_only_after_the_report_of_the_host(self):
        room, host, guest = await self.started_room(acting=FORCE_GUEST)
        await self.send(room, guest, {"type": "auto", "on": True})
        await self.send(room, host, {"type": "auto", "on": True})
        self.assertTrue(room.seat_of(HOST["uid"])["ai"])
        count = len(room.steps)
        self.assertIsNone(await self.send(room, host, socket_step(count, rooms.NO_FACTION, command=SIM_COMMAND_END_TURN)))
        await self.send(room, guest, {"type": "auto", "on": False})
        self.assertEqual(len(room.steps), count + 1)                         # no step of the server yet
        await self.report(room, host, count, rooms.NO_FACTION)
        self.assertEqual(guest.socket.binaries[-1], server_step(count + 1, rooms.STEP_HUMAN, FORCE_GUEST, rooms.NO_FACTION, 1))
        self.assertFalse(room.seat_of(GUEST["uid"])["ai"])

    async def test_a_spectator_has_no_button(self):
        room, host, guest = await self.started_room()
        await self.rooms.watch(room, OTHER)
        watcher = await self.connect(room, OTHER)
        self.assertEqual(await self.send(room, watcher, {"type": "auto", "on": True}), "no seat in this game")

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

    async def test_a_hello_does_not_take_the_number_of_a_step_of_the_host_for_the_ai(self):
        room, host, guest = await self.started_room(acting=FORCE_AI)
        room.seat_of(GUEST["uid"])["ai"] = True
        await self.connect(room, GUEST)
        self.assertEqual(len(room.steps), 0)                                 # the host still sends steps for the AI
        self.assertIsNone(await self.send(room, host, socket_step(0, FORCE_AI, command=SIM_COMMAND_END_TURN)))
        self.assertTrue(room.seat_of(GUEST["uid"])["ai"])
        await self.report(room, host, 0, FORCE_HOST, "1" * 16)               # a faction with a player acts: no burst comes now
        self.assertEqual(room.steps[1][1], rooms.STEP_HUMAN)
        self.assertFalse(room.seat_of(GUEST["uid"])["ai"])
        self.assertIsNone(await self.send(room, host, socket_step(2, FORCE_HOST)))

    async def test_the_faction_goes_to_a_player_if_the_host_has_no_socket(self):
        room, host, guest = await self.started_room(acting=FORCE_AI)
        room.seat_of(GUEST["uid"])["ai"] = True
        room.peers.discard(host)
        await self.connect(room, GUEST)
        self.assertEqual(room.steps[0][1], rooms.STEP_HUMAN)                 # nobody can send a step for the AI

    # ---- MP9: a player joins a game that runs ----

    async def test_a_member_takes_a_free_seat_after_the_start(self):
        room, host, guest = await self.started_room()
        self.assertEqual(room.record["start_humans"], [FORCE_HOST, FORCE_GUEST])
        await self.rooms.take_seat(room, OTHER, FORCE_AI)
        seat = room.seat_of(OTHER["uid"])
        self.assertEqual((seat["force"], seat["user"], seat["ai"]), (FORCE_AI, "Other", True))
        self.assertEqual(len(room.steps), 0)                                 # the faction changes after the hello of the player
        self.assertEqual(room.record["start_humans"], [FORCE_HOST, FORCE_GUEST])
        self.assertEqual([item["name"] for item in await self.store.list(rooms.member_prefix(OTHER["uid"]))], [room.gid])
        other = await self.connect(room, OTHER)
        self.assertEqual(room.steps[0], bytes([1, rooms.STEP_HUMAN, FORCE_AI, FORCE_HOST]) + struct.pack("<I", 1))
        self.assertFalse(seat["ai"])
        self.assertEqual(self.welcome(other)["seat"], FORCE_AI)
        presence = [m for m in host.socket.texts if m["type"] == "presence"][-1]
        self.assertIn({"force": FORCE_AI, "user": "Other"}, presence["players"])

    def welcome(self, peer):
        return next(m for m in peer.socket.texts if m["type"] == "welcome")

    async def test_a_late_seat_has_limits(self):
        room, host, guest = await self.started_room()
        stranger = {"uid": "d" * 24, "name": "Stranger"}
        with self.assertRaisesRegex(rooms.RoomError, "the seat is taken"):
            await self.rooms.take_seat(room, OTHER, FORCE_GUEST)
        with self.assertRaisesRegex(rooms.RoomError, "you have a seat"):
            await self.rooms.take_seat(room, GUEST, FORCE_AI)
        with self.assertRaisesRegex(rooms.RoomError, "the game started"):
            await self.rooms.take_seat(room, GUEST, None)                    # a player cannot leave the seat in a game
        room.record["state"] = "desync"
        with self.assertRaisesRegex(rooms.RoomError, "desync"):
            await self.rooms.take_seat(room, OTHER, FORCE_AI)
        room.record["state"] = "running"
        room.record["late_join"] = False
        with self.assertRaisesRegex(rooms.RoomError, "the game started"):
            await self.rooms.take_seat(room, stranger, FORCE_AI)

    async def test_the_host_sets_the_option_of_a_late_join(self):
        room = await self.make_room()
        self.assertTrue(room.record["late_join"])
        await self.rooms.set_options(room, HOST, {"mode": "live", "turn_seconds": 300, "pause_absent": False, "late_join": False})
        self.assertFalse(room.record["late_join"])
        with self.assertRaises(rooms.RoomError):
            await self.rooms.set_options(room, HOST, {"mode": "live", "turn_seconds": 300, "pause_absent": False, "late_join": 1})

    async def test_a_spectator_takes_a_free_seat(self):
        room, host, guest = await self.started_room()
        await self.rooms.watch(room, OTHER)
        await self.rooms.take_seat(room, OTHER, FORCE_AI)
        self.assertNotIn(OTHER["uid"], room.record["watchers"])
        self.assertTrue(room.is_member(OTHER["uid"]))

    async def test_an_old_room_without_the_option_keeps_the_seats(self):
        room, host, guest = await self.started_room()
        del room.record["late_join"]
        with self.assertRaisesRegex(rooms.RoomError, "the game started"):
            await self.rooms.take_seat(room, OTHER, FORCE_AI)

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

    # ---- MP5: the snapshots, the list of games and a new build ----

    async def steps_with_reports(self, room, host, count, checksum="1" * 16):
        for number in range(len(room.steps), len(room.steps) + count):
            await self.send(room, host, socket_step(number, FORCE_HOST))
            await self.report(room, host, number, FORCE_HOST, checksum)

    async def test_a_snapshot_with_a_wrong_checksum_gets_an_error(self):
        room, host, guest = await self.started_room()
        await self.steps_with_reports(room, host, 1)
        with self.assertRaises(rooms.RoomError):
            await self.rooms.put_snapshot(room, HOST, 0, "2" * 16, b"image")
        with self.assertRaises(rooms.RoomError):
            await self.rooms.put_snapshot(room, OTHER, 0, "1" * 16, b"image")   # no seat
        with self.assertRaises(rooms.RoomError):
            await self.rooms.put_snapshot(room, HOST, 1, "1" * 16, b"image")    # no such step
        with self.assertRaises(rooms.RoomError):
            await self.rooms.put_snapshot(room, HOST, 0, "1" * 16, b"")
        self.assertIsNone(await self.rooms.latest_snapshot(room, GUEST))
        with self.assertRaises(rooms.RoomError) as caught:
            await self.rooms.put_snapshot(room, GUEST, 0, "1" * 16, b"image")   # only the host saves
        self.assertEqual(str(caught.exception), "only the host saves the game")
        await self.rooms.put_snapshot(room, HOST, 0, "1" * 16, b"image")
        self.assertEqual(await self.rooms.latest_snapshot(room, GUEST), (0, b"image"))
        with self.assertRaises(rooms.RoomError):
            await self.rooms.latest_snapshot(room, OTHER)

    async def test_a_snapshot_of_a_step_without_a_report_waits_for_the_report(self):
        room, host, guest = await self.started_room()
        await self.send(room, host, socket_step(0, FORCE_HOST))
        with self.assertRaises(rooms.RoomError) as caught:
            await self.rooms.put_snapshot(room, HOST, 0, "1" * 16, b"image")
        self.assertEqual(str(caught.exception), "no report of the step")      # the page tries again

    def bucket_links(self):
        """The folder of the tests acts as a bucket: it gives signed addresses and sizes."""
        self.store.presigned_put = lambda key, seconds: f"https://bucket.test/{key}?put={seconds}"
        self.store.presigned_get = lambda key, seconds: f"https://bucket.test/{key}?get={seconds}"

        async def size(key):
            data = await self.store.get(key)
            return None if data is None else len(data)
        self.store.size = size

    async def test_the_host_uploads_a_snapshot_to_the_bucket_and_the_server_records_it(self):
        self.bucket_links()
        room, host, guest = await self.started_room()
        await self.steps_with_reports(room, host, 1)
        key = rooms.snapshot_key(room.gid, 0)
        self.assertEqual(await self.rooms.snapshot_upload_url(room, HOST, 0, "1" * 16), f"https://bucket.test/{key}?put=120")
        for user, checksum, reason in ((GUEST, "1" * 16, "only the host saves the game"),
                                       (HOST, "2" * 16, "the checksum of the snapshot differs")):
            with self.assertRaises(rooms.RoomError) as caught:
                await self.rooms.snapshot_upload_url(room, user, 0, checksum)
            self.assertEqual(str(caught.exception), reason)
        with self.assertRaises(rooms.RoomError) as caught:
            await self.rooms.finish_snapshot(room, HOST, 0, "1" * 16)        # the upload did not arrive
        self.assertEqual(str(caught.exception), "no file of the snapshot")
        self.assertIsNone(room.record.get("snapshot"))
        await self.store.put(key, b"image")                                  # the page uploads the file
        await self.rooms.finish_snapshot(room, HOST, 0, "1" * 16)
        self.assertEqual(room.record["snapshot"], 0)
        self.assertEqual(await self.rooms.latest_snapshot_step(room, GUEST), 0)
        self.assertEqual(self.store.presigned_get(key, rooms.SNAPSHOT_URL_SECONDS), f"https://bucket.test/{key}?get=120")

    async def test_a_snapshot_over_the_limit_in_the_bucket_goes_away(self):
        self.bucket_links()
        room, host, guest = await self.started_room()
        await self.steps_with_reports(room, host, 1)
        key = rooms.snapshot_key(room.gid, 0)
        await self.store.put(key, b"x" * (rooms.MAX_START_IMAGE_BYTES + 1))
        with self.assertRaises(rooms.RoomError) as caught:
            await self.rooms.finish_snapshot(room, HOST, 0, "1" * 16)
        self.assertEqual(str(caught.exception), "bad snapshot")
        self.assertIsNone(await self.store.get(key))
        self.assertIsNone(room.record.get("snapshot"))

    async def test_a_storage_without_signed_addresses_gives_no_upload_address(self):
        room, host, guest = await self.started_room()
        await self.steps_with_reports(room, host, 1)
        self.assertIsNone(await self.rooms.snapshot_upload_url(room, HOST, 0, "1" * 16))

    async def test_a_person_without_a_seat_gets_no_step_of_the_latest_snapshot(self):
        room, host, guest = await self.started_room()
        with self.assertRaises(rooms.RoomError):
            await self.rooms.latest_snapshot_step(room, OTHER)

    async def test_the_server_keeps_five_snapshots(self):
        room, host, guest = await self.started_room()
        await self.steps_with_reports(room, host, 7)
        for step in range(7):
            await self.rooms.put_snapshot(room, HOST, step, "1" * 16, bytes([step]))
        names = [item["name"] for item in await self.store.list(rooms.snapshot_prefix(room.gid))]
        self.assertEqual(names, [f"{step:08d}.hxs" for step in range(2, 7)])
        self.assertEqual(await self.rooms.latest_snapshot(room, HOST), (6, bytes([6])))
        again = rooms.Rooms(self.store, clock=self.clock)
        self.assertEqual(await again.latest_snapshot(await again.get(room.gid), GUEST), (6, bytes([6])))

    async def test_the_list_of_a_member_shows_your_turn(self):
        room, host, guest = await self.started_room(acting=FORCE_GUEST)
        self.assertTrue(room.summary(GUEST["uid"])["your_turn"])
        self.assertFalse(room.summary(HOST["uid"])["your_turn"])
        self.assertNotIn("host", room.summary(HOST["uid"]))

    async def test_a_new_build_moves_a_running_room_to_the_new_build(self):
        room, host, guest = await self.started_room()
        await self.steps_with_reports(room, host, 2)
        await self.rooms.put_snapshot(room, HOST, 1, "1" * 16, b"image")
        await self.rooms.flush_all()
        new_build = "fedcba9876543210"
        again = rooms.Rooms(self.store, clock=self.clock, build=new_build)
        loaded = await again.get(room.gid)
        self.assertEqual(loaded.record["build"], new_build)
        self.assertEqual(loaded.record["old_builds"], [BUILD])
        peer = rooms.Peer(FakeSocket(), HOST)
        rooms.join(loaded, peer)
        await rooms.handle_text(again, loaded, peer, json.dumps({"type": "hello", "build": new_build, "last_step": 1}))
        self.assertNotIn("old build", peer.socket.errors())
        open_room = await again.get((await again.create(HOST, "G", "S", "placeholder", BUILD, "live", 300, [{"force": 1}])).gid)
        self.assertEqual(open_room.record["build"], BUILD)                  # the server gives the build at the creation

    # ---- MP6: the recovery from a desync and the referee ----

    async def three_players(self, referee=None):
        self.rooms = rooms.Rooms(self.store, clock=self.clock, referee=referee)
        room = await self.make_room()
        await self.rooms.take_seat(room, OTHER, FORCE_AI)
        await self.rooms.start(room, HOST)
        peers = [await self.connect(room, user) for user in (HOST, GUEST, OTHER)]
        await self.report(room, peers[0], -1, FORCE_HOST, "0" * 16)
        return room, peers

    async def test_a_desync_with_three_players_gives_the_majority_state_to_the_third_player(self):
        room, (host, guest, other) = await self.three_players()
        await self.send(room, host, socket_step(0, FORCE_HOST))
        await self.report(room, host, 0, FORCE_HOST, "1" * 16)
        await self.report(room, other, 0, FORCE_HOST, "2" * 16)
        self.assertEqual(room.record["state"], "desync")                     # the game stops for all players
        self.assertIn({"type": "desync", "step": 0}, guest.socket.texts)
        self.assertEqual(await self.send(room, host, socket_step(1, FORCE_HOST)), "desync")
        await self.report(room, guest, 0, FORCE_HOST, "1" * 16)              # the last report: the majority decides
        self.assertEqual(room.record["state"], "running")
        self.assertIn({"type": "reload", "step": 0}, other.socket.texts)     # the third player loads the good state
        self.assertNotIn({"type": "reload", "step": 0}, host.socket.texts)
        self.assertIn({"type": "desync_end", "step": 0}, host.socket.texts)
        self.assertIsNone(await self.send(room, host, socket_step(1, FORCE_HOST)))

    async def test_a_desync_of_two_players_asks_the_referee(self):
        calls = []

        async def referee(record, first_step, image, steps):
            calls.append((record["pack"], first_step, image, len(steps)))
            return ["2" * 16 for _ in steps]

        self.rooms = rooms.Rooms(self.store, clock=self.clock, referee=referee)
        room, host, guest = await self.started_room()
        await self.steps_with_reports(room, host, 1)
        await self.rooms.put_snapshot(room, HOST, 0, "1" * 16, b"snapshot")
        await self.send(room, host, socket_step(1, FORCE_HOST))
        await self.report(room, host, 1, FORCE_HOST, "3" * 16)
        await self.report(room, guest, 1, FORCE_HOST, "2" * 16)
        self.assertEqual(calls, [("placeholder", 1, b"snapshot", 1)])        # the latest snapshot before the step, and the log after it
        self.assertEqual(room.record["state"], "running")
        self.assertIn({"type": "reload", "step": 1}, host.socket.texts)      # the referee agrees with the guest

    async def test_without_a_referee_a_desync_of_two_players_stays(self):
        room, host, guest = await self.started_room()
        await self.steps_with_reports(room, host, 1)
        await self.report(room, guest, 0, FORCE_HOST, "2" * 16)
        self.assertEqual(room.record["state"], "desync")
        self.assertIn("no referee", guest.socket.errors())

    async def test_the_state_images_of_a_desync_go_to_the_storage(self):
        room, host, guest = await self.started_room()
        await self.steps_with_reports(room, host, 1)
        with self.assertRaises(rooms.RoomError):
            await self.rooms.put_desync_image(room, GUEST, 0, b"image")      # no desync yet
        await self.report(room, guest, 0, FORCE_HOST, "2" * 16)
        await self.rooms.put_desync_image(room, GUEST, 0, b"image")
        self.assertEqual(await self.store.get(f"games/{room.gid}/desync/00000000-{GUEST['uid']}.hxs"), b"image")
        with self.assertRaises(rooms.RoomError):
            await self.rooms.put_desync_image(room, OTHER, 0, b"image")

    async def test_a_state_image_can_come_after_the_room_decided(self):
        async def referee(record, first_step, image, steps):
            return ["1" * 16 for _ in steps]

        self.rooms = rooms.Rooms(self.store, clock=self.clock, referee=referee)
        room, host, guest = await self.started_room()
        await self.steps_with_reports(room, host, 1)
        await self.report(room, guest, 0, FORCE_HOST, "2" * 16)
        self.assertEqual(room.record["state"], "running")                    # the referee decided before the images came
        await self.rooms.put_desync_image(room, HOST, 0, b"good")
        await self.rooms.put_desync_image(room, GUEST, 0, b"bad")
        self.assertEqual(await self.store.get(f"games/{room.gid}/desync/00000000-{HOST['uid']}.hxs"), b"good")
        self.assertEqual(await self.store.get(f"games/{room.gid}/desync/00000000-{GUEST['uid']}.hxs"), b"bad")
        with self.assertRaises(rooms.RoomError):
            await self.rooms.put_desync_image(room, GUEST, 1, b"image")      # no desync at this step
        with self.assertRaises(rooms.RoomError):
            await self.rooms.put_desync_image(room, OTHER, 0, b"image")      # no seat in this game

    async def test_a_snapshot_of_the_bad_state_goes_away(self):
        room, (host, guest, other) = await self.three_players()
        await self.steps_with_reports(room, host, 1)
        await self.rooms.put_snapshot(room, HOST, 0, "1" * 16, b"good")
        await self.send(room, host, socket_step(1, FORCE_HOST))
        await self.report(room, host, 1, FORCE_HOST, "3" * 16)              # the host reports first: its snapshot is taken
        await self.rooms.put_snapshot(room, HOST, 1, "3" * 16, b"bad")
        await self.report(room, guest, 1, FORCE_HOST, "2" * 16)
        await self.report(room, other, 1, FORCE_HOST, "2" * 16)
        self.assertEqual(room.record["state"], "running")
        self.assertEqual(await self.rooms.latest_snapshot(room, HOST), (0, b"good"))
        self.assertIn({"type": "reload", "step": 1}, host.socket.texts)

    # ---- MP7: the spectators and the chat ----

    async def test_a_spectator_gets_the_steps_and_gives_no_command(self):
        room, host, guest = await self.started_room()
        await self.rooms.watch(room, OTHER)
        self.assertTrue(room.is_viewer(OTHER["uid"]))
        self.assertFalse(room.is_member(OTHER["uid"]))
        watcher = await self.connect(room, OTHER)
        welcome = next(m for m in watcher.socket.texts if m["type"] == "welcome")
        self.assertIsNone(welcome["seat"])
        await self.send(room, host, socket_step(0, FORCE_HOST))
        self.assertEqual(watcher.socket.binaries, [socket_step(0, FORCE_HOST)])
        self.assertEqual(await self.send(room, watcher, socket_step(1, FORCE_AI)), "not your seat")
        await self.report(room, host, 0, FORCE_HOST, "1" * 16)
        self.assertIsNone(await self.report(room, watcher, 0, FORCE_HOST, "2" * 16))   # a spectator gives no report
        self.assertEqual(room.record["state"], "running")
        self.assertEqual(await self.rooms.games_of(OTHER["uid"]), [])         # the game is not in the list of the spectator

    async def test_the_host_decides_if_spectators_can_watch(self):
        room = await self.make_room()
        await self.rooms.set_options(room, HOST, {"mode": "live", "turn_seconds": 300, "pause_absent": False,
                                                  "spectators": False})
        await self.rooms.start(room, HOST)
        with self.assertRaises(rooms.RoomError):
            await self.rooms.watch(room, OTHER)
        self.assertFalse(room.is_viewer(OTHER["uid"]))

    async def test_the_room_keeps_the_last_lines_of_the_chat(self):
        room, host, guest = await self.started_room()
        for number in range(rooms.CHAT_HISTORY + 5):
            await self.send(room, guest, {"type": "chat", "text": f"line {number}"})
            self.clock.now += 1
            guest.messages = []                                               # no limit of the messages in this test
        history = room.public()["chat"]
        self.assertEqual(len(history), rooms.CHAT_HISTORY)
        self.assertEqual(history[-1], {"name": "Guest", "text": f"line {rooms.CHAT_HISTORY + 4}"})

    def test_the_log_packs_and_unpacks(self):
        steps = [b"abc", b"", bytes(range(200))]
        self.assertEqual(rooms.unpack_log(rooms.pack_log(steps)), steps)
        with self.assertRaises(ValueError):
            rooms.unpack_log(rooms.pack_log(steps)[:-1])


if __name__ == "__main__":
    unittest.main(verbosity=2)
