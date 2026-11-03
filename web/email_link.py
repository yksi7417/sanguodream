"""The sign-in with an email link (docs/WEB.md, "The sign-in with an email link").

The flow:
1. The user types an address. The server makes a token, keeps only its hash in
   the storage, and sends a link with the token.
2. The link opens a page with a button. A GET does not use the token, because
   a mail scanner opens links.
3. The button posts the token. The server deletes the token and gives the
   session cookie.

The tokens stay in the storage, because the machine on fly.io stops when nobody
uses it. The keys:
- signin/<hash of the token>.json: the address and the end time.
- emails/<hash of the address>.json: the uid of the account of the address.
- emails/index-done.json: the mark of the one-time migration of the old records.
"""
import asyncio
import hashlib
import json
import os
import re
import secrets
import time

import aiohttp

import auth
import storage

TOKEN_SECONDS = 15 * 60
MAX_REQUESTS_FOR_ADDRESS = 5
MAX_REQUESTS_FROM_IP = 20
LIMIT_WINDOW_SECONDS = 3600
MAX_ADDRESS_LENGTH = 254
ADDRESS = re.compile(r"[^@\s]+@[^@\s]+\.[^@\s]+")
INDEX_DONE_KEY = "emails/index-done.json"
# The account for the testers. This address signs in with no link, so nobody proves it.
TESTER_ADDRESS = "tester@cccsoftwares.com"


class EmailError(Exception):
    pass


def clean_address(text):
    """Give the address in lower case, or "" if it is not an address."""
    address = str(text or "").strip().lower()
    return address if len(address) <= MAX_ADDRESS_LENGTH and ADDRESS.fullmatch(address) else ""


def token_key(token):
    return "signin/" + hashlib.sha256(str(token).encode("utf-8")).hexdigest() + ".json"


def address_key(address):
    return "emails/" + hashlib.sha256(address.encode("utf-8")).hexdigest() + ".json"


def client_ip(request):
    """The proxy of fly.io sets Fly-Client-IP. Elsewhere another computer can forge the header."""
    if os.environ.get("FLY_APP_NAME"):
        return request.headers.get("Fly-Client-IP") or str(request.remote)
    return str(request.remote)


def compose(link):
    minutes = TOKEN_SECONDS // 60
    text = (f"Open this link to sign in to hexdom:\n\n{link}\n\n"
            f"The link works one time, for {minutes} minutes.\n"
            "If you did not ask for it, you can ignore this email.\n")
    page = (f'<p><a href="{link}">Sign in to hexdom</a></p>'
            f"<p>The link works one time, for {minutes} minutes.</p>"
            "<p>If you did not ask for it, you can ignore this email.</p>")
    return "Your sign-in link for hexdom", text, page


class Resend:
    """The email provider. RESEND_API_BASE changes the address of the api for the tests."""

    def __init__(self):
        self.api_key = os.environ.get("RESEND_API_KEY", "")
        self.sender = os.environ.get("EMAIL_FROM", "")
        self.api = os.environ.get("RESEND_API_BASE", "") or "https://api.resend.com"
        self.enabled = bool(self.api_key and self.sender)

    async def send(self, http, address, link):
        subject, text, page = compose(link)
        body = {"from": self.sender, "to": [address], "subject": subject, "text": text, "html": page}
        headers = {"Authorization": f"Bearer {self.api_key}"}
        async with http.post(self.api + "/emails", json=body, headers=headers) as response:
            if response.status != 200:
                # The log gets only the status, because the answer can name the address.
                raise EmailError(f"the provider answered {response.status}")


class Outbox:
    """It writes the link to the file EMAIL_OUTBOX, for the browser tests on a local computer."""

    def __init__(self):
        self.path = os.environ.get("EMAIL_OUTBOX", "")
        self.enabled = bool(self.path) and not os.environ.get("FLY_APP_NAME")

    async def send(self, http, address, link):
        with open(self.path, "w", encoding="utf-8") as target:
            target.write(link)


def load_sender():
    """Give the sender of the emails, or None. Without a sender, the page has no email form."""
    for sender in (Resend(), Outbox()):
        if sender.enabled:
            return sender
    return None


class RateLimit:
    def __init__(self, limit):
        self.limit = limit
        self.moments = {}

    def allow(self, name):
        now = time.time()
        recent = [moment for moment in self.moments.get(name, []) if now - moment < LIMIT_WINDOW_SECONDS]
        if len(recent) >= self.limit:
            self.moments[name] = recent
            return False
        self.moments[name] = recent + [now]
        return True


async def read_json(store, key):
    data = await store.get(key)
    return json.loads(data) if data else None


class EmailLinks:
    def __init__(self, store, sender):
        self.store = store
        self.sender = sender
        self.for_address = RateLimit(MAX_REQUESTS_FOR_ADDRESS)
        self.from_ip = RateLimit(MAX_REQUESTS_FROM_IP)
        self.consume_lock = asyncio.Lock()   # one machine runs the site, so this lock makes a token single-use
        self.last_sweep = 0

    # ---- the tokens ----

    async def request(self, http, address, ip, confirm_url):
        """Send a link. Give False only if the IP sent too many requests. An address over its
        limit gets no email, and the reply does not change."""
        if not self.from_ip.allow(ip):
            return False
        if not self.for_address.allow(address):
            return True
        await self.sweep()
        token = secrets.token_urlsafe(32)
        record = {"email": address, "exp": int(time.time()) + TOKEN_SECONDS}
        await self.store.put(token_key(token), json.dumps(record).encode("utf-8"))
        try:
            await self.sender.send(http, address, f"{confirm_url}?token={token}")
            print("email: the server sent a sign-in link", flush=True)
        except (EmailError, aiohttp.ClientError, asyncio.TimeoutError) as error:
            print(f"email: the server sent no sign-in link: {error}", flush=True)
        return True

    async def peek(self, token):
        """Give the address of a good token, and do not use the token."""
        record = await read_json(self.store, token_key(token))
        return record["email"] if record and record["exp"] >= time.time() else None

    async def consume(self, token):
        """Give the address of a good token, and delete the token."""
        key = token_key(token)
        async with self.consume_lock:
            record = await read_json(self.store, key)
            if not record:
                return None
            await self.store.delete(key)
        return record["email"] if record["exp"] >= time.time() else None

    async def sweep(self):
        """Delete the tokens that nobody used. Do this at most one time for each window."""
        now = time.time()
        if now - self.last_sweep < LIMIT_WINDOW_SECONDS:
            return
        self.last_sweep = now
        for item in await self.store.list("signin/"):
            if now - item["modified"] > TOKEN_SECONDS:
                await self.store.delete("signin/" + item["name"])

    # ---- the accounts ----

    async def account_of(self, address):
        """Give the record of the account of an address, or None."""
        index = await read_json(self.store, address_key(address))
        return await storage.read_user(self.store, index["uid"]) if index else None

    async def remember(self, address, uid):
        """Note the first account of an address. A later account does not take its place."""
        key = address_key(address)
        if await self.store.get(key) is None:
            await self.store.put(key, json.dumps({"uid": uid}).encode("utf-8"))

    async def sign_in(self, address, verified=True):
        """Give the record of the account of an address. The user proved it with the link, or verified is False."""
        user = await self.account_of(address)
        if not user:
            uid = auth.user_id("email", address)
            user = {"uid": uid, "name": auth.clean_name(address.split("@")[0], "Player"), "joined": int(time.time())}
        user.update({"provider": "email", "email": address, "email_verified": verified, "last_login": int(time.time())})
        await storage.write_user(self.store, user)
        await self.remember(address, user["uid"])
        return user

    async def migrate(self):
        """One time: put the old records into the index, so that their users keep their saves.

        An old record counts if the provider verified the email, or if the user typed the old
        house password. A new record of Facebook has an email that nobody verified, so it
        does not count: another person can type any email at Facebook."""
        if await self.store.get(INDEX_DONE_KEY) is not None:
            return
        groups = {}
        for user in await storage.all_users(self.store):
            address = clean_address(user.get("email"))
            if address and (user.get("email_verified") is True or user.get("member")):
                groups.setdefault(address, []).append(user)
        for address, users in groups.items():
            users.sort(key=lambda user: user.get("last_login", 0), reverse=True)
            for other in users[1:]:
                await self.copy_saves(other["uid"], users[0]["uid"])
            await self.remember(address, users[0]["uid"])
        await self.store.put(INDEX_DONE_KEY, b"{}")
        print(f"email: the index has {len(groups)} old addresses", flush=True)

    async def copy_saves(self, source, target):
        """Copy the saves to the account of the same user. A save of the target stays, and the old copy stays."""
        present = {item["name"] for item in await self.store.list(storage.save_prefix(target))}
        for item in await self.store.list(storage.save_prefix(source)):
            if item["name"] not in present:
                data = await self.store.get(storage.save_key(source, item["name"]))
                await self.store.put(storage.save_key(target, item["name"]), data)


def migrate_context(links):
    async def context(app):
        await links.migrate()
        yield
    return context
