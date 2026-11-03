"""The tests of the sign-in with an email link (web/email_link.py, web/server.py; docs/WEB.md).

Run: C:\\tools\\hexdom-venv\\Scripts\\python -I tests\\server\\email_link_tests.py

A fake Resend api on a local test server takes the place of api.resend.com.
The tests run the real application of server.py through the test client of
aiohttp. They need no build and no Resend account.
"""
import contextlib
import hashlib
import io
import json
import os
import re
import sys
import tempfile
import time
import unittest
import unittest.mock
import urllib.parse

sys.dont_write_bytecode = True   # no __pycache__ in web/
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "web"))
from aiohttp import web                                               # noqa: E402
from aiohttp.test_utils import TestClient, TestServer, make_mocked_request   # noqa: E402

API_KEY = "re_test_key"
SENDER = "hexdom <sign-in@example.test>"
ADDRESS = "player@example.com"
ADMIN = "boss@example.com"


class EmailLinkTestCase(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.mails = []
        self.resend_status = 200
        self.resend = TestServer(self.fake_resend())
        await self.resend.start_server()
        self.folder = tempfile.TemporaryDirectory()
        self.storage_dir = os.path.join(self.folder.name, "storage")
        os.environ.update({
            "SESSION_SECRET": "email-test-secret", "AUTH_DEV_LOGIN": "", "ADMIN_EMAILS": ADMIN,
            "RESEND_API_KEY": API_KEY, "EMAIL_FROM": SENDER,
            "RESEND_API_BASE": str(self.resend.make_url("")).rstrip("/"),
            "EMAIL_OUTBOX": "", "PUBLIC_URL": "", "DISCORD_CLIENT_ID": "1234", "DISCORD_CLIENT_SECRET": "secret",
            "STORAGE_DIR": self.storage_dir, "FLY_APP_NAME": "", "BUCKET_NAME": "",
            "HEXDOM_REFEREE": os.path.join(self.folder.name, "no-referee.js"),
        })
        os.environ.pop("HOUSE_PASSWORD", None)
        os.environ.pop("SITE_PASSWORD", None)
        import auth       # noqa: E402  (after the settings of the environment)
        import server     # noqa: E402
        import storage    # noqa: E402
        self.auth, self.server, self.storage = auth, server, storage
        self.store = storage.FolderStorage(self.storage_dir)
        self.client = None

    async def asyncTearDown(self):
        if self.client:
            await self.client.close()
        await self.resend.close()
        self.folder.cleanup()

    def fake_resend(self):
        async def send(request):
            if request.headers.get("Authorization") != f"Bearer {API_KEY}":
                return web.json_response({"message": "bad key"}, status=401)
            if self.resend_status != 200:
                return web.json_response({"message": "refused"}, status=self.resend_status)
            self.mails.append(await request.json())
            return web.json_response({"id": "mail-1"})

        app = web.Application()
        app.add_routes([web.post("/emails", send)])
        return app

    async def start(self):
        """Start the site. The seed of the records comes before this call, because the site reads them at the start."""
        self.client = TestClient(TestServer(self.server.make_app(self.folder.name)))
        await self.client.start_server()

    async def request_link(self, address=ADDRESS):
        return await self.client.post("/auth/email/request", data={"email": address},
                                      allow_redirects=False)

    def last_token(self):
        link = urllib.parse.urlsplit(re.search(r"https?://\S+", self.mails[-1]["text"]).group(0))
        self.assertEqual(link.path, "/auth/email/confirm")
        return urllib.parse.parse_qs(link.query)["token"][0]

    async def confirm(self, token):
        return await self.client.post("/auth/email/confirm", data={"token": token}, allow_redirects=False)

    async def sign_in(self, address=ADDRESS):
        self.assertEqual((await self.request_link(address)).status, 200)
        self.assertEqual((await self.confirm(self.last_token())).status, 302)
        return await (await self.client.get("/api/me")).json()

    def token_path(self, token):
        return os.path.join(self.storage_dir, "signin", hashlib.sha256(token.encode("ascii")).hexdigest() + ".json")

    async def seed_user(self, provider, subject, email="", verified=False, member=True, saves=None, last_login=100):
        uid = self.auth.user_id(provider, subject)
        record = {"uid": uid, "provider": provider, "name": subject, "member": member, "joined": 1, "last_login": last_login}
        if email:
            record.update({"email": email, "email_verified": verified})
        await self.storage.write_user(self.store, record)
        for name, data in (saves or {}).items():
            await self.store.put(self.storage.save_key(uid, name), data)
        return uid


class TheHousePasswordIsGone(EmailLinkTestCase):
    async def test_the_site_starts_without_a_house_password(self):
        await self.start()
        self.assertEqual((await self.client.get("/healthz")).status, 200)

    async def test_the_routes_of_the_house_password_do_not_exist(self):
        await self.start()
        self.assertEqual((await self.client.post("/house", data={"password": "x"})).status, 405)
        self.assertEqual((await self.client.post("/api/house", json={"password": "x"})).status, 404)

    async def test_a_user_who_never_typed_the_house_password_can_play(self):
        await self.start()
        await self.seed_user("google", "not-yet", "a@example.com", True, member=False)
        token = self.auth.sign(b"email-test-secret", {"uid": self.auth.user_id("google", "not-yet"), "exp": time.time() + 60})
        me = await self.client.get("/api/me", headers={"Authorization": f"Bearer {token}"})
        self.assertEqual(me.status, 200)


class TheLink(EmailLinkTestCase):
    async def test_the_sign_in_page_has_the_providers_and_the_email_form(self):
        await self.start()
        page = await (await self.client.get("/login")).text()
        self.assertIn('id="login-discord"', page)
        self.assertIn('id="email-address"', page)
        self.assertIn('action="/auth/email/request"', page)

    async def test_the_page_has_no_email_form_without_the_key_of_the_provider(self):
        os.environ["RESEND_API_KEY"] = ""
        await self.start()
        page = await (await self.client.get("/login")).text()
        self.assertNotIn('id="email-address"', page)
        self.assertEqual((await self.request_link()).status, 404)

    async def test_the_email_goes_to_the_address_with_a_link_of_the_site(self):
        os.environ["PUBLIC_URL"] = "https://site.test"
        await self.start()
        self.assertEqual((await self.request_link("  Player@Example.com ")).status, 200)
        mail = self.mails[-1]
        self.assertEqual(mail["to"], [ADDRESS])
        self.assertEqual(mail["from"], SENDER)
        self.assertIn(f"https://site.test/auth/email/confirm?token={self.last_token()}", mail["text"])
        self.assertIn(self.last_token(), mail["html"])

    async def test_a_bad_address_gets_an_error_and_no_email(self):
        await self.start()
        for address in ("", "nobody", "two@@example.com", "a b@example.com", "a@example.com\r\nBcc: x@example.com", "a" * 250 + "@b.test"):
            self.assertEqual((await self.request_link(address)).status, 400, address)
        self.assertEqual(self.mails, [])

    async def test_the_storage_keeps_only_a_hash_of_the_token(self):
        await self.start()
        await self.request_link()
        token = self.last_token()
        self.assertTrue(os.path.isfile(self.token_path(token)))
        for folder, _, names in os.walk(self.storage_dir):
            for name in names:
                self.assertNotIn(token, name)
                with open(os.path.join(folder, name), "rb") as source:
                    self.assertNotIn(token.encode("ascii"), source.read())

    async def test_a_get_of_the_link_does_not_use_the_token(self):
        await self.start()
        await self.request_link()
        token = self.last_token()
        for _ in range(3):   # a mail scanner opens the link
            page = await self.client.get("/auth/email/confirm", params={"token": token})
            self.assertEqual(page.status, 200)
            self.assertNotIn("hx_session", page.cookies)
            self.assertIn('method="post"', await page.text())
        self.assertEqual((await self.client.get("/api/me")).status, 401)
        self.assertEqual((await self.confirm(token)).status, 302)
        self.assertEqual((await self.client.get("/api/me")).status, 200)

    async def test_the_page_of_the_link_keeps_the_token_from_other_sites_and_from_the_cache(self):
        await self.start()
        await self.request_link()
        page = await self.client.get("/auth/email/confirm", params={"token": self.last_token()})
        self.assertEqual(page.headers["Referrer-Policy"], "same-origin")   # no-referrer breaks the check of the origin
        self.assertEqual(page.headers["Cache-Control"], "no-store")

    async def test_the_token_works_one_time_only(self):
        await self.start()
        await self.request_link()
        token = self.last_token()
        self.assertEqual((await self.confirm(token)).status, 302)
        self.assertEqual((await self.confirm(token)).status, 400)
        self.assertEqual((await self.client.get("/auth/email/confirm", params={"token": token})).status, 400)

    async def test_an_old_token_does_not_work(self):
        await self.start()
        await self.request_link()
        token = self.last_token()
        with open(self.token_path(token), encoding="utf-8") as source:
            record = json.load(source)
        self.assertLessEqual(record["exp"] - time.time(), 15 * 60)
        record["exp"] = int(time.time()) - 1
        with open(self.token_path(token), "w", encoding="utf-8") as target:
            json.dump(record, target)
        self.assertEqual((await self.client.get("/auth/email/confirm", params={"token": token})).status, 400)
        self.assertEqual((await self.confirm(token)).status, 400)
        self.assertEqual((await self.client.get("/api/me")).status, 401)

    async def test_a_forged_token_does_not_work(self):
        await self.start()
        self.assertEqual((await self.confirm("forged")).status, 400)
        self.assertEqual((await self.confirm("")).status, 400)

    async def test_the_session_cookie_is_secure_httponly_and_samesite(self):
        os.environ["PUBLIC_URL"] = "https://site.test"
        await self.start()
        await self.request_link()
        answer = await self.confirm(self.last_token())
        cookie = answer.cookies["hx_session"]
        self.assertTrue(cookie["secure"])
        self.assertTrue(cookie["httponly"])
        self.assertEqual(cookie["samesite"], "Lax")

    async def test_a_post_from_another_origin_gets_403(self):
        await self.start()
        answer = await self.client.post("/auth/email/request", data={"email": ADDRESS}, headers={"Origin": "https://evil.test"})
        self.assertEqual(answer.status, 403)
        self.assertEqual(self.mails, [])


class TheLimitsAndTheReplies(EmailLinkTestCase):
    async def test_the_reply_is_the_same_for_a_new_address_and_a_known_address(self):
        await self.seed_user("google", "known", "known@example.com", True)
        await self.start()
        known = await self.request_link("known@example.com")
        new = await self.request_link("new@example.com")
        self.assertEqual((known.status, await known.text()), (new.status, await new.text()))
        self.assertEqual(len(self.mails), 2)

    async def test_too_many_requests_for_an_address_send_no_email_and_get_the_same_reply(self):
        await self.start()
        first = await self.request_link()
        for _ in range(10):
            answer = await self.request_link()
            self.assertEqual((answer.status, await answer.text()), (200, await first.text()))
        self.assertEqual(len(self.mails), 5)
        await self.request_link("other@example.com")
        self.assertEqual(len(self.mails), 6)

    async def test_too_many_requests_from_an_ip_get_429_and_send_no_email(self):
        await self.start()
        for number in range(20):
            self.assertEqual((await self.request_link(f"p{number}@example.com")).status, 200)
        self.assertEqual((await self.request_link("late@example.com")).status, 429)
        self.assertEqual(len(self.mails), 20)

    async def test_the_address_of_a_client_on_fly_comes_from_the_header_of_the_proxy(self):
        import email_link
        request = make_mocked_request("POST", "/auth/email/request", headers={"Fly-Client-IP": "203.0.113.9"})
        os.environ["FLY_APP_NAME"] = "site"
        self.addCleanup(os.environ.update, {"FLY_APP_NAME": ""})
        self.assertEqual(email_link.client_ip(request), "203.0.113.9")
        os.environ["FLY_APP_NAME"] = ""
        self.assertNotEqual(email_link.client_ip(request), "203.0.113.9")   # another computer cannot forge the header

    async def test_a_failure_of_the_provider_gives_the_same_reply(self):
        await self.start()
        good = await self.request_link()
        self.resend_status = 500
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            bad = await self.request_link("other@example.com")
        self.assertEqual((bad.status, await bad.text()), (good.status, await good.text()))
        self.assertIn("500", output.getvalue())

    async def test_the_log_has_no_token_and_no_address(self):
        await self.start()
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            await self.request_link()
            token = self.last_token()
            await self.client.get("/auth/email/confirm", params={"token": token})
            await self.confirm(token)
            await self.confirm(token)
            self.resend_status = 500
            await self.request_link("second@example.com")
        self.assertNotIn(token, output.getvalue())
        self.assertNotIn(ADDRESS, output.getvalue())
        self.assertNotIn("second@example.com", output.getvalue())


class TheAccounts(EmailLinkTestCase):
    def fake_request(self):
        class Request:
            app = self.client.app
        return Request()

    async def test_a_new_address_makes_a_new_user_with_a_name_from_the_address(self):
        await self.start()
        me = await self.sign_in()
        self.assertEqual(me["uid"], self.auth.user_id("email", ADDRESS))
        self.assertEqual(me["name"], "player")
        self.assertEqual(me["provider"], "email")
        again = await self.sign_in()
        self.assertEqual(again["uid"], me["uid"])

    async def test_the_tester_address_signs_in_at_once_with_no_email(self):
        await self.start()
        response = await self.request_link(" Tester@CCCsoftwares.com ")
        self.assertEqual(response.status, 302)
        self.assertEqual(response.headers["Location"], "/")
        self.assertEqual(self.mails, [])
        me = await (await self.client.get("/api/me")).json()
        self.assertEqual(me["uid"], self.auth.user_id("email", "tester@cccsoftwares.com"))
        self.assertEqual(me["name"], "tester")

    async def test_another_address_of_the_tester_domain_still_needs_the_link(self):
        await self.start()
        response = await self.request_link("other@cccsoftwares.com")
        self.assertEqual(response.status, 200)
        self.assertEqual(len(self.mails), 1)
        self.assertEqual((await self.client.get("/api/me")).status, 401)

    async def test_the_tester_address_is_not_an_admin_even_in_admin_emails(self):
        with unittest.mock.patch.dict(os.environ, {"ADMIN_EMAILS": "tester@cccsoftwares.com"}):
            await self.start()
        await self.request_link("tester@cccsoftwares.com")
        self.assertFalse((await (await self.client.get("/api/me")).json())["admin"])
        self.assertNotEqual((await self.client.get("/admin")).status, 200)

    async def test_the_address_in_admin_emails_makes_an_admin(self):
        await self.start()
        self.assertTrue((await self.sign_in(ADMIN))["admin"])
        self.assertEqual((await self.client.get("/admin")).status, 200)

    async def test_the_user_of_a_provider_keeps_the_saves_with_the_same_email(self):
        uid = await self.seed_user("google", "g1", "Player@Example.com", True, saves={"slot1": b"save-one"})
        await self.start()
        me = await self.sign_in()
        self.assertEqual(me["uid"], uid)
        self.assertEqual(await (await self.client.get("/api/saves/slot1")).read(), b"save-one")

    async def test_a_member_of_before_with_an_unverified_email_keeps_the_saves(self):
        uid = await self.seed_user("facebook", "f1", ADDRESS, False, member=True, saves={"slot1": b"old"})
        await self.start()
        self.assertEqual((await self.sign_in())["uid"], uid)

    async def test_an_unverified_email_of_a_new_user_gives_no_account_to_the_address(self):
        # Another person can type any email at Facebook. That person must not share the account of the owner.
        uid = await self.seed_user("facebook", "f2", ADDRESS, False, member=False)
        await self.start()
        self.assertNotEqual((await self.sign_in())["uid"], uid)

    async def test_two_accounts_with_one_email_join_without_a_lost_save(self):
        older = await self.seed_user("discord", "d1", ADDRESS, True, last_login=100, saves={"a": b"older-a", "both": b"older-both"})
        newer = await self.seed_user("google", "g2", ADDRESS, True, last_login=200, saves={"b": b"newer-b", "both": b"newer-both"})
        await self.start()
        me = await self.sign_in()
        self.assertEqual(me["uid"], newer)
        self.assertEqual(await (await self.client.get("/api/saves/a")).read(), b"older-a")
        self.assertEqual(await (await self.client.get("/api/saves/both")).read(), b"newer-both")
        self.assertEqual(await self.store.get(self.storage.save_key(older, "a")), b"older-a")   # the old copy stays

    async def test_an_account_without_an_email_stays_and_the_address_gets_a_new_account(self):
        uid = await self.seed_user("facebook", "f3", saves={"slot1": b"keep"})
        await self.start()
        self.assertNotEqual((await self.sign_in())["uid"], uid)
        self.assertIsNotNone(await self.storage.read_user(self.store, uid))
        self.assertEqual(await self.store.get(self.storage.save_key(uid, "slot1")), b"keep")

    async def test_the_migration_runs_one_time(self):
        await self.seed_user("google", "g3", ADDRESS, True)
        await self.start()
        await self.client.close()
        self.client = None
        uid = await self.seed_user("google", "g4", "late@example.com", True)
        await self.start()
        self.assertNotEqual((await self.sign_in("late@example.com"))["uid"], uid)

    async def test_a_verified_sign_in_with_a_provider_after_the_email_link_finds_the_same_account(self):
        await self.start()
        me = await self.sign_in()
        user = await self.server.sign_in_user(self.fake_request(), self.auth.Discord(), "8002", "Liu", ADDRESS, True)
        self.assertEqual(user["uid"], me["uid"])
        self.assertEqual(user["provider"], "discord")

    async def test_an_unverified_sign_in_with_a_provider_does_not_find_the_account_of_the_address(self):
        await self.start()
        me = await self.sign_in()
        user = await self.server.sign_in_user(self.fake_request(), self.auth.Discord(), "8003", "Other", ADDRESS, False)
        self.assertNotEqual(user["uid"], me["uid"])

    async def test_the_email_link_finds_a_user_who_came_with_a_provider_after_the_migration(self):
        await self.start()
        first = await self.server.sign_in_user(self.fake_request(), self.auth.Discord(), "8004", "Sun", ADDRESS, True)
        self.assertEqual((await self.sign_in())["uid"], first["uid"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
