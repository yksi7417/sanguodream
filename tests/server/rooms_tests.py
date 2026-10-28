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


class RoomsTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.store = storage.FolderStorage(self.folder.name)
        self.rooms = rooms.Rooms(self.store)

    async def asyncTearDown(self):
        self.folder.cleanup()

    async def make_room(self):
        factions = [{"force": FORCE_HOST, "name": "Cao"}, {"force": FORCE_GUEST, "name": "Liu"}, {"force": FORCE_AI, "name": "Sun"}]
        room = await self.rooms.create(HOST, "Test game", "SCEN002", "placeholder", BUILD, "live", 300, factions)
        await self.rooms.take_seat(room, HOST, FORCE_HOST)
        await self.rooms.take_seat(room, GUEST, FORCE_GUEST)
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

        again = rooms.Rooms(self.store)
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

    async def test_only_the_host_reports_the_start_and_only_in_a_running_game(self):
        room = await self.make_room()
        host = await self.connect(room, HOST)
        guest = await self.connect(room, GUEST)
        report = {"type": "checksum", "step": -1, "checksum": "0" * 16, "acting": FORCE_GUEST}
        self.assertEqual(await self.send(room, host, report), "the game does not run")
        await self.rooms.start(room, HOST)
        self.assertEqual(await self.send(room, guest, report), "no such step")
        self.assertEqual(room.record["acting"], rooms.NO_FACTION)
        self.assertIsNone(await self.send(room, host, report))
        self.assertEqual(room.record["acting"], FORCE_GUEST)

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
        again = rooms.Rooms(self.store)
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

    async def test_a_bad_game_id_finds_no_room(self):
        self.assertIsNone(await self.rooms.get("0123456789abcdef\n"))
        self.assertIsNone(await self.rooms.get("../users/x"))
        self.assertIsNone(await self.rooms.get("0123456789abcdef"))

    def test_the_log_packs_and_unpacks(self):
        steps = [b"abc", b"", bytes(range(200))]
        self.assertEqual(rooms.unpack_log(rooms.pack_log(steps)), steps)
        with self.assertRaises(ValueError):
            rooms.unpack_log(rooms.pack_log(steps)[:-1])


if __name__ == "__main__":
    unittest.main(verbosity=2)
