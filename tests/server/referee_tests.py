"""The tests of the referee of the shared games (web/referee.py; docs/MULTIPLAYER.md, MP6).

Run: C:\\tools\\hexdom-venv\\Scripts\\python -I tests\\server\\referee_tests.py

The referee is build\\referee.js ("build_web.bat referee"), and node runs it.
The feature tests of the web build write the fixture ("run_tests.bat --web"):
build\\referee_fixture.bin is the input of the referee for a log of a host,
and build\\referee_fixture.txt has the checksums of that host after each
step. The referee gets the images of the browsers, and the images of the
native build have another layout. Thus the test needs "build_web.bat tests",
"run_tests.bat --web" and "build_web.bat referee" first.
"""
import asyncio
import os
import struct
import sys
import unittest

sys.dont_write_bytecode = True   # no __pycache__ in web/
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
sys.path.insert(0, os.path.join(ROOT, "web"))
import referee   # noqa: E402

SCRIPT = os.path.join(ROOT, "build", "referee.js")
FIXTURE = os.path.join(ROOT, "build", "referee_fixture.bin")
EXPECTED = os.path.join(ROOT, "build", "referee_fixture.txt")


def read_fixture():
    """The parts of the input of the fixture: the pack, the humans, the image and the steps."""
    data = open(FIXTURE, "rb").read()
    at = 4
    version, flags, ledger, first_step, size = struct.unpack_from("<IIIII", data, at)
    at += 20
    pack = data[at:at + size].decode("ascii")
    at += size
    (count,) = struct.unpack_from("<I", data, at)
    humans = list(data[at + 4:at + 4 + count])
    at += 4 + count
    (size,) = struct.unpack_from("<I", data, at)
    image = data[at + 4:at + 4 + size]
    at += 4 + size
    (count,) = struct.unpack_from("<I", data, at)
    at += 4
    steps = []
    for _ in range(count):
        (size,) = struct.unpack_from("<I", data, at)
        steps.append(data[at + 4:at + 4 + size])
        at += 4 + size
    return data, {"pack": pack, "flags": flags, "ledger": ledger, "first_step": first_step,
                  "humans": humans, "image": image, "steps": steps}


class RefereeTests(unittest.TestCase):
    def setUp(self):
        for path, hint in ((SCRIPT, "build_web.bat referee"), (FIXTURE, "run_tests.bat --web"), (EXPECTED, "run_tests.bat --web")):
            self.assertTrue(os.path.isfile(path), f"{path} is missing: run {hint} first")

    def test_the_payload_has_the_format_of_the_referee(self):
        data, parts = read_fixture()
        again = referee.payload(parts["pack"], parts["first_step"], parts["image"], parts["steps"],
                                parts["humans"], parts["flags"], parts["ledger"])
        self.assertEqual(again, data)

    def test_the_referee_replays_a_stored_log_and_gives_the_same_checksum_as_the_engines(self):
        data, parts = read_fixture()
        output = asyncio.run(referee.run(SCRIPT, data))
        self.assertEqual(output.split(), open(EXPECTED, encoding="ascii").read().split())

    def test_the_referee_of_the_rooms_gives_the_checksum_after_each_step(self):
        data, parts = read_fixture()
        record = {"pack": parts["pack"], "seats": [{"force": force, "uid": "a" * 24} for force in parts["humans"]],
                  "referee": {"flags": parts["flags"], "ledger": parts["ledger"]}}
        checksums = asyncio.run(referee.Referee(SCRIPT)(record, 0, parts["image"], parts["steps"]))
        expected = [line.split()[1] for line in open(EXPECTED, encoding="ascii").read().splitlines()[:-1]]
        self.assertEqual(checksums, expected)

    def test_a_bad_input_gives_an_error(self):
        with self.assertRaises(referee.RefereeError):
            asyncio.run(referee.Referee(SCRIPT)({"pack": "placeholder", "seats": []}, 0, b"no image", []))


if __name__ == "__main__":
    unittest.main(verbosity=2)
