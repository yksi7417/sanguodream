"""The tests of the public files of the site and of the fast start (web/server.py, web/game_files.py; docs/WEB.md).

Run: C:\\tools\\hexdom-venv\\Scripts\\python -I tests\\server\\site_files_tests.py

The tests run the real application of server.py through the test client of
aiohttp. They need no build.
"""
import contextlib
import io
import os
import sys
import tempfile
import unittest
from unittest import mock

sys.dont_write_bytecode = True   # no __pycache__ in web/
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "web"))
from aiohttp.test_utils import TestClient, TestServer   # noqa: E402


class SiteFilesTestCase(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.folder = tempfile.TemporaryDirectory()
        os.environ.update({
            "SESSION_SECRET": "site-files-secret", "AUTH_DEV_LOGIN": "", "ADMIN_EMAILS": "",
            "RESEND_API_KEY": "", "EMAIL_FROM": "", "EMAIL_OUTBOX": "", "PUBLIC_URL": "",
            "STORAGE_DIR": os.path.join(self.folder.name, "storage"), "FLY_APP_NAME": "", "BUCKET_NAME": "",
            "HEXDOM_REFEREE": os.path.join(self.folder.name, "no-referee.js"),
        })
        import server     # noqa: E402  (after the settings of the environment)
        with contextlib.redirect_stdout(io.StringIO()):
            self.client = TestClient(TestServer(server.make_app(self.folder.name)))
            await self.client.start_server()

    async def asyncTearDown(self):
        await self.client.close()
        self.folder.cleanup()

    async def test_a_person_without_a_session_gets_the_public_files(self):
        for path, media_type in (("/splash.jpg", "image/jpeg"), ("/boot.html", "text/html"),
                                 ("/sw.js", "text/javascript")):
            response = await self.client.get(path, allow_redirects=False)
            self.assertEqual(response.status, 200, path)
            self.assertEqual(response.content_type, media_type, path)
            self.assertGreater(len(await response.read()), 0, path)

    async def test_the_browser_keeps_the_splash_and_asks_again_for_the_script(self):
        splash = await self.client.get("/splash.jpg")
        self.assertIn("max-age=86400", splash.headers["Cache-Control"])
        worker = await self.client.get("/sw.js")
        self.assertEqual(worker.headers["Cache-Control"], "no-cache")

    async def test_the_game_files_stay_for_the_members(self):
        for path in ("/", "/index.html", "/index.data", "/server.py"):
            response = await self.client.get(path, allow_redirects=False)
            self.assertEqual(response.status, 302, path)
            self.assertEqual(response.headers["Location"], "/login", path)

    async def test_the_sign_in_page_has_the_art_and_starts_the_service_worker(self):
        page = await (await self.client.get("/login")).text()
        self.assertIn('class="art"', page)
        self.assertIn("/splash.jpg", page)
        self.assertIn('serviceWorker.register("/sw.js")', page)


class BuildIdTestCase(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        for name in ("index.wasm", "index.data"):
            with open(os.path.join(self.folder.name, name), "wb") as target:
                target.write(name.encode("ascii"))

    def test_the_saved_build_id_is_the_id_of_the_hash(self):
        import game_files
        hashed = game_files.build_id_of(self.folder.name)
        game_files.save_build_id(self.folder.name)
        with mock.patch.object(game_files, "build_id_of", side_effect=AssertionError("the server read the big files")):
            files = game_files.GameFiles(self.folder.name)
        self.assertEqual(files.build, hashed)
        self.assertEqual(files.sizes["index.data"], len("index.data"))

    def test_without_a_saved_id_the_server_computes_the_hash(self):
        import game_files
        self.assertEqual(game_files.GameFiles(self.folder.name).build, game_files.build_id_of(self.folder.name))

    def test_without_the_big_files_the_saved_id_does_not_count(self):
        import game_files
        game_files.save_build_id(self.folder.name)
        os.remove(os.path.join(self.folder.name, "index.data"))
        self.assertIsNone(game_files.GameFiles(self.folder.name).build)


if __name__ == "__main__":
    result = unittest.main(exit=False, verbosity=1).result
    print("RESULT " + ("PASS" if result.wasSuccessful() else "FAIL"))
    sys.exit(0 if result.wasSuccessful() else 1)
