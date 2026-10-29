"""The tests of the Discord Activity (web/auth.py, web/server.py; docs/MULTIPLAYER.md, MP8).

Run: C:\\tools\\hexdom-venv\\Scripts\\python -I tests\\server\\discord_tests.py

A fake Discord api on a local test server takes the place of discord.com.
The tests run the real application of server.py through the test client of
aiohttp. They need no build and no Discord app.
"""
import json
import os
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True   # no __pycache__ in web/
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "web"))
from aiohttp import ClientSession, WSMsgType, web                     # noqa: E402
from aiohttp.test_utils import TestClient, TestServer                 # noqa: E402

CLIENT_ID, CLIENT_SECRET = "1234567890", "discord-secret"
HOUSE = "house-test"
GOOD_CODE, ACCESS_TOKEN = "good-code", "discord-access"
DISCORD_USER = {"id": "8001", "username": "liu", "global_name": "Liu Bei", "email": "liu@example.com", "verified": True}


def fake_discord():
    """The two endpoints of the Discord api that the server uses."""
    async def token(request):
        form = await request.post()
        if form.get("code") != GOOD_CODE or form.get("client_id") != CLIENT_ID or form.get("client_secret") != CLIENT_SECRET:
            return web.json_response({"error": "invalid_grant"}, status=400)
        return web.json_response({"access_token": ACCESS_TOKEN, "token_type": "Bearer", "scope": "identify email"})

    async def me(request):
        if request.headers.get("Authorization") != f"Bearer {ACCESS_TOKEN}":
            return web.json_response({"message": "401: Unauthorized"}, status=401)
        return web.json_response(DISCORD_USER)

    app = web.Application()
    app.add_routes([web.post("/oauth2/token", token), web.get("/users/@me", me)])
    return app


class DiscordTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.fake = TestServer(fake_discord())
        await self.fake.start_server()
        self.folder = tempfile.TemporaryDirectory()
        os.environ.update({
            "SESSION_SECRET": "discord-test-secret", "HOUSE_PASSWORD": HOUSE, "AUTH_DEV_LOGIN": "",
            "DISCORD_CLIENT_ID": CLIENT_ID, "DISCORD_CLIENT_SECRET": CLIENT_SECRET,
            "DISCORD_API_BASE": str(self.fake.make_url("")).rstrip("/"),
            "STORAGE_DIR": os.path.join(self.folder.name, "storage"), "FLY_APP_NAME": "", "BUCKET_NAME": "",
            "HEXDOM_REFEREE": os.path.join(self.folder.name, "no-referee.js"),
        })
        import auth     # noqa: E402  (after the settings of the environment)
        import server   # noqa: E402
        self.auth = auth
        self.client = TestClient(TestServer(server.make_app(self.folder.name)))
        await self.client.start_server()

    async def asyncTearDown(self):
        await self.client.close()
        await self.fake.close()
        self.folder.cleanup()

    async def sign_in(self):
        """The Activity changes the code of the Discord SDK for a session token."""
        response = await self.client.post("/api/discord/token", json={"code": GOOD_CODE})
        self.assertEqual(response.status, 200)
        return await response.json()

    def bearer(self, token):
        return {"Authorization": f"Bearer {token}"}

    async def member(self):
        token = (await self.sign_in())["token"]
        response = await self.client.post("/api/house", json={"password": HOUSE}, headers=self.bearer(token))
        self.assertEqual(response.status, 200)
        return token

    async def make_room(self, token, channel=None):
        body = {"name": "Voice game", "scenario": "", "pack": "placeholder", "mode": "live", "turn_seconds": 300,
                "factions": [{"force": 1, "name": "Liu"}, {"force": 2, "name": "Cao"}]}
        if channel:
            body["channel"] = channel
        return await self.client.post("/api/games", json=body, headers=self.bearer(token))

    async def test_the_provider_discord_with_a_fake_token_endpoint_gives_the_record_of_the_user(self):
        provider = self.auth.Discord()
        self.assertTrue(provider.enabled)
        async with ClientSession() as http:
            identity = await provider.identity(http, {"code": GOOD_CODE}, {}, "https://site.test/auth/discord/callback")
            self.assertEqual(identity, ("8001", "Liu Bei", "liu@example.com", True))
            with self.assertRaises(self.auth.AuthError):
                await provider.identity(http, {"code": "bad-code"}, {}, "https://site.test/auth/discord/callback")
        self.assertIn("discord", self.auth.load_providers())

    async def test_the_activity_changes_the_code_for_a_session_token(self):
        answer = await self.sign_in()
        self.assertEqual(answer["access_token"], ACCESS_TOKEN)   # the page gives it to the SDK (authenticate)
        self.assertFalse(answer["member"])
        token = answer["token"]
        self.assertEqual((await self.client.get("/api/me", headers=self.bearer(token))).status, 401)   # the house password first
        wrong = await self.client.post("/api/house", json={"password": "wrong"}, headers=self.bearer(token))
        self.assertEqual(wrong.status, 403)
        await self.client.post("/api/house", json={"password": HOUSE}, headers=self.bearer(token))
        me = await (await self.client.get("/api/me", headers=self.bearer(token))).json()
        self.assertEqual((me["uid"], me["name"], me["provider"]), (self.auth.user_id("discord", "8001"), "Liu Bei", "discord"))
        bad = await self.client.post("/api/discord/token", json={"code": "bad-code"})
        self.assertEqual(bad.status, 401)
        self.assertEqual((await self.client.get("/api/me", headers=self.bearer("forged.token"))).status, 401)

    async def test_a_hello_with_a_bad_token_gets_401(self):
        token = await self.member()
        room = await (await self.make_room(token)).json()
        socket = await self.client.ws_connect(f"/api/games/{room['gid']}/socket?token=1")
        await socket.send_str(json.dumps({"type": "hello", "token": "bad.token", "last_step": -1}))
        message = await socket.receive()
        self.assertEqual(json.loads(message.data), {"type": "error", "reason": "401"})
        closed = await socket.receive()
        self.assertEqual(closed.type, WSMsgType.CLOSE)
        self.assertEqual(socket.close_code, 4401)

        socket = await self.client.ws_connect(f"/api/games/{room['gid']}/socket?token=1")
        await socket.send_str(json.dumps({"type": "hello", "token": token, "last_step": -1}))
        welcome = json.loads((await socket.receive()).data)
        self.assertEqual(welcome["type"], "welcome")
        self.assertTrue(welcome["host"])
        await socket.close()

    async def test_a_socket_of_the_page_of_the_activity_passes_the_origin_check(self):
        token = await self.member()
        room = await (await self.make_room(token)).json()
        activity = f"https://{CLIENT_ID}.discordsays.com"
        socket = await self.client.ws_connect(f"/api/games/{room['gid']}/socket?token=1", headers={"Origin": activity})
        await socket.send_str(json.dumps({"type": "hello", "token": token, "last_step": -1}))
        self.assertEqual(json.loads((await socket.receive()).data)["type"], "welcome")
        await socket.close()
        with self.assertRaises(Exception):
            await self.client.ws_connect(f"/api/games/{room['gid']}/socket?token=1", headers={"Origin": "https://evil.test"})

    async def test_discord_can_put_the_pages_of_the_site_in_its_frame(self):
        response = await self.client.get("/login?frame_id=test")
        self.assertEqual(response.status, 200)
        self.assertNotIn("X-Frame-Options", response.headers)   # DENY stops the frame of the Activity
        ancestors = response.headers["Content-Security-Policy"]
        self.assertTrue(ancestors.startswith("frame-ancestors 'self' "))
        self.assertIn("https://discord.com", ancestors)
        self.assertIn("https://*.discordsays.com", ancestors)
        self.assertNotIn(" *", ancestors)   # other sites cannot frame the site

    async def test_one_room_for_each_voice_channel(self):
        token = await self.member()
        first = await self.make_room(token, channel="5550001")
        self.assertEqual(first.status, 201)
        gid = (await first.json())["gid"]
        found = await self.client.get("/api/discord/channel/5550001", headers=self.bearer(token))
        self.assertEqual((await found.json())["gid"], gid)
        second = await self.make_room(token, channel="5550001")
        self.assertEqual(second.status, 409)
        self.assertEqual((await self.client.get("/api/discord/channel/5550002", headers=self.bearer(token))).status, 404)
        self.assertEqual((await self.client.get("/api/discord/channel/../x", headers=self.bearer(token))).status, 404)


if __name__ == "__main__":
    unittest.main(verbosity=2)
