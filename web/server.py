"""The server of the web game (docs/WEB.md, "The fly.io site").

Usage: python -u web/server.py [FOLDER]

The server gives the files of FOLDER (default /srv/web) to a member. A
member is a user who signed in (auth.py) and typed the house password one
time. The server also keeps the saves of each member (storage.py) and, as
an option, relays a YouTube broadcast (broadcast.py).

The environment variables:
- SESSION_SECRET: the key of the cookies. The server does not start
  without it.
- HOUSE_PASSWORD: the shared password of the house. SITE_PASSWORD is the
  old name, and the server also reads it. The server does not start
  without one of them.
- PUBLIC_URL: the address of the site for the providers, for example
  https://sanguodream.fly.dev. On fly.io the default comes from
  FLY_APP_NAME. Locally, the default is the address of the request.
- The secrets of the providers (auth.py), of the bucket (storage.py) and
  of YouTube (broadcast.py).
- ADMIN_EMAILS: the email addresses of the admins, with commas between
  them. An admin sees the members page, /admin. The provider must verify
  the email.
- PORT: the port. The default is 8080.

If the browser accepts gzip and the file FILE.gz exists, the server sends
FILE.gz. The Dockerfile makes these files.
"""
import asyncio
import hmac
import json
import mimetypes
import os
import sys
import time
import urllib.parse

import aiohttp
from aiohttp import web

import auth
import broadcast
import game_files
import pages
import storage

DEFAULT_FOLDER = "/srv/web"
DEFAULT_PORT = 8080
FAILED_PASSWORD_DELAY_SECONDS = 1.0
MAX_FAILED_PASSWORDS = 10
FAILED_PASSWORD_WINDOW_SECONDS = 3600
GAME_FILES = ("index.html", "index.js", "index.wasm", "index.data")

mimetypes.add_type("application/wasm", ".wasm")
mimetypes.add_type("text/javascript", ".js")

KEY_SECRET = web.AppKey("secret", bytes)
KEY_HOUSE = web.AppKey("house", bytes)
KEY_ROOT = web.AppKey("root", str)
KEY_PROVIDERS = web.AppKey("providers", dict)
KEY_STORAGE = web.AppKey("storage", object)
KEY_HTTP = web.AppKey("http", aiohttp.ClientSession)
KEY_BROADCAST = web.AppKey("broadcast", broadcast.Broadcast)
KEY_FAILURES = web.AppKey("failures", dict)
KEY_ADMINS = web.AppKey("admins", frozenset)
KEY_GAME_FILES = web.AppKey("game_files", game_files.GameFiles)


# ---- helpers ---------------------------------------------------------------

def public_url(request):
    configured = os.environ.get("PUBLIC_URL", "").rstrip("/")
    if configured:
        return configured
    fly_app = os.environ.get("FLY_APP_NAME", "")
    if fly_app:
        return f"https://{fly_app}.fly.dev"
    return f"{request.scheme}://{request.host}"


def redirect(location):
    return web.Response(status=302, headers={"Location": location})


def secure_cookies(request):
    return public_url(request).startswith("https://")


def set_cookie(response, request, name, value, max_age, same_site="Lax"):
    secure = secure_cookies(request)
    if same_site == "None" and not secure:
        same_site = "Lax"   # A browser refuses SameSite=None without Secure.
    response.set_cookie(name, value, max_age=max_age, path="/", httponly=True, secure=secure, samesite=same_site)


async def current_user(request):
    """Give the record of the signed-in user, or None."""
    session = auth.verify(request.app[KEY_SECRET], request.cookies.get(auth.SESSION_COOKIE, ""))
    if not session:
        return None
    try:
        return await storage.read_user(request.app[KEY_STORAGE], session.get("uid", ""))
    except ValueError:
        return None


async def require_member(request):
    user = await current_user(request)
    if user and user.get("member"):
        return user
    if request.path.startswith("/api/"):
        raise web.HTTPUnauthorized()
    raise web.HTTPFound("/house" if user else "/login")


def same_origin(request):
    """A form post or an upload must come from a page of this site."""
    origin = request.headers.get("Origin")
    if origin is None:
        return True
    return urllib.parse.urlsplit(origin).netloc == request.host


@web.middleware
async def security_headers(request, handler):
    if request.method not in ("GET", "HEAD") and not request.path.endswith("/callback") and not same_origin(request):
        raise web.HTTPForbidden(text="cross-origin request")
    response = await handler(request)
    response.headers.setdefault("X-Content-Type-Options", "nosniff")
    response.headers.setdefault("X-Frame-Options", "DENY")
    response.headers.setdefault("Referrer-Policy", "same-origin")
    return response


# ---- sign-in ---------------------------------------------------------------

async def login(request):
    user = await current_user(request)
    if user and user.get("member"):
        raise web.HTTPFound("/")
    error = request.query.get("error", "")
    return web.Response(text=pages.login_page(request.app[KEY_PROVIDERS], error), content_type="text/html")


async def auth_start(request):
    provider = request.app[KEY_PROVIDERS].get(request.match_info["provider"])
    if not provider:
        raise web.HTTPNotFound()
    state = auth.new_state(provider.name, "/", request.query.get("name", ""))
    redirect_uri = f"{public_url(request)}/auth/{provider.name}/callback"
    response = redirect(provider.authorize_url(redirect_uri, state))
    # Apple posts the callback from its own site, so the cookie needs SameSite=None.
    set_cookie(response, request, auth.STATE_COOKIE, auth.sign(request.app[KEY_SECRET], state), auth.STATE_SECONDS, "None")
    return response


async def auth_callback(request):
    provider = request.app[KEY_PROVIDERS].get(request.match_info["provider"])
    if not provider:
        raise web.HTTPNotFound()
    params = dict(request.query)
    if request.method == "POST":
        params.update(await request.post())
    state = auth.verify(request.app[KEY_SECRET], request.cookies.get(auth.STATE_COOKIE, ""))
    returned = str(params.get("state", ""))
    if not state or state.get("provider") != provider.name or not hmac.compare_digest(returned, state.get("state", "")):
        raise web.HTTPFound("/login?error=" + urllib.parse.quote("The sign-in expired. Try again."))
    if params.get("error"):
        raise web.HTTPFound("/login?error=" + urllib.parse.quote("The sign-in was cancelled."))
    redirect_uri = f"{public_url(request)}/auth/{provider.name}/callback"
    try:
        subject, name, email, verified = await provider.identity(request.app[KEY_HTTP], params, state, redirect_uri)
    except (auth.AuthError, aiohttp.ClientError, asyncio.TimeoutError) as error:
        print(f"auth: {provider.name} sign-in failed: {error}", flush=True)
        raise web.HTTPFound("/login?error=" + urllib.parse.quote(f"The sign-in with {provider.label} failed."))

    store = request.app[KEY_STORAGE]
    uid = auth.user_id(provider.name, subject)
    user = await storage.read_user(store, uid) or {"uid": uid, "provider": provider.name, "member": False,
                                                   "joined": int(time.time())}
    # Apple gives the name only at the first sign-in, so an empty name keeps the old one.
    user["name"] = name or user.get("name") or auth.clean_name(email, f"{provider.label} user")
    if email:
        user["email"] = email
        user["email_verified"] = verified
    user["last_login"] = int(time.time())
    await storage.write_user(store, user)
    print(f"auth: {user['name']} signed in with {provider.name}", flush=True)

    response = redirect("/" if user.get("member") else "/house")
    session = {"uid": uid, "exp": int(time.time()) + auth.SESSION_SECONDS}
    set_cookie(response, request, auth.SESSION_COOKIE, auth.sign(request.app[KEY_SECRET], session), auth.SESSION_SECONDS)
    response.del_cookie(auth.STATE_COOKIE, path="/")
    return response


async def house_form(request):
    user = await current_user(request)
    if not user:
        raise web.HTTPFound("/login")
    if user.get("member"):
        raise web.HTTPFound("/")
    return web.Response(text=pages.house_page(user["name"]), content_type="text/html")


async def house_submit(request):
    user = await current_user(request)
    if not user:
        raise web.HTTPFound("/login")
    failures = request.app[KEY_FAILURES]
    now = time.time()
    recent = [moment for moment in failures.get(user["uid"], []) if now - moment < FAILED_PASSWORD_WINDOW_SECONDS]
    if len(recent) >= MAX_FAILED_PASSWORDS:
        page = pages.house_page(user["name"], "Too many wrong passwords. Try again in one hour.")
        return web.Response(text=page, content_type="text/html", status=429)
    form = await request.post()
    typed = str(form.get("password", "")).encode("utf-8")
    if not hmac.compare_digest(typed, request.app[KEY_HOUSE]):
        failures[user["uid"]] = recent + [now]
        await asyncio.sleep(FAILED_PASSWORD_DELAY_SECONDS)
        page = pages.house_page(user["name"], "That is not the house password.")
        return web.Response(text=page, content_type="text/html", status=403)
    failures.pop(user["uid"], None)
    user["member"] = True
    await storage.write_user(request.app[KEY_STORAGE], user)
    print(f"auth: {user['name']} joined the house", flush=True)
    raise web.HTTPFound("/")


async def logout(request):
    response = redirect("/login")
    response.del_cookie(auth.SESSION_COOKIE, path="/")
    return response


# ---- the api ---------------------------------------------------------------

async def api_me(request):
    user = await require_member(request)
    return web.json_response({"uid": user["uid"], "name": user["name"], "provider": user["provider"],
                              "admin": auth.is_admin(user, request.app[KEY_ADMINS]),
                              "can_broadcast": request.app[KEY_BROADCAST].enabled})


async def api_saves(request):
    user = await require_member(request)
    items = await request.app[KEY_STORAGE].list(storage.save_prefix(user["uid"]))
    return web.json_response({"saves": items})


async def api_save_get(request):
    user = await require_member(request)
    name = request.match_info["name"]
    if not storage.valid_save_name(name):
        raise web.HTTPNotFound()
    data = await request.app[KEY_STORAGE].get(storage.save_key(user["uid"], name))
    if data is None:
        raise web.HTTPNotFound()
    return web.Response(body=data, content_type="application/octet-stream", headers={"Cache-Control": "no-store"})


async def api_save_put(request):
    user = await require_member(request)
    name = request.match_info["name"]
    if not storage.valid_save_name(name):
        raise web.HTTPBadRequest(text="bad save name")
    if request.content_length is not None and request.content_length > storage.MAX_SAVE_BYTES:
        raise web.HTTPRequestEntityTooLarge(max_size=storage.MAX_SAVE_BYTES, actual_size=request.content_length)
    data = await request.read()
    await request.app[KEY_STORAGE].put(storage.save_key(user["uid"], name), data)
    return web.json_response({"name": name, "size": len(data), "modified": int(time.time())})


async def admin_members(request):
    user = await require_member(request)
    if not auth.is_admin(user, request.app[KEY_ADMINS]):
        raise web.HTTPForbidden(text=pages.forbidden_page(), content_type="text/html")
    store = request.app[KEY_STORAGE]
    members = []
    for row in await store.list("users/"):
        try:
            record = json.loads(await store.get("users/" + row["name"]) or b"null")
        except ValueError:
            continue
        if not isinstance(record, dict) or "uid" not in record:
            continue
        record["saves"] = len(await store.list(storage.save_prefix(record["uid"])))
        record["admin"] = auth.is_admin(record, request.app[KEY_ADMINS])
        members.append(record)
    members.sort(key=lambda record: record.get("last_login", 0), reverse=True)
    return web.Response(text=pages.members_page(members), content_type="text/html",
                        headers={"Cache-Control": "no-store"})


CLIENT_ERROR_MAX_CHARS = 300


async def api_client_error(request):
    """The page sends the reason when the game stops in the browser (web/shell.html)."""
    user = await require_member(request)
    try:
        report = await request.json()
    except ValueError:
        raise web.HTTPBadRequest()
    message = " ".join(str(report.get("message", "") if isinstance(report, dict) else "").split())
    agent = " ".join(request.headers.get("User-Agent", "").split())
    print(f"client: the game stopped for {user['name']}: {message[:CLIENT_ERROR_MAX_CHARS]} "
          f"[{agent[:CLIENT_ERROR_MAX_CHARS]}]", flush=True)
    return web.json_response({"logged": True})


async def api_broadcast(request):
    user = await require_member(request)
    if not same_origin(request):
        raise web.HTTPForbidden(text="cross-origin request")
    return await request.app[KEY_BROADCAST].handle(request, user)


# ---- the game files ----------------------------------------------------------

async def game_file(request):
    await require_member(request)
    name = request.match_info.get("name") or "index.html"
    if name not in GAME_FILES:
        raise web.HTTPNotFound()
    path = os.path.join(request.app[KEY_ROOT], name)
    if not os.path.isfile(path):
        raise web.HTTPNotFound()
    # FileResponse sends FILE.gz to a browser that accepts gzip, and answers
    # If-Modified-Since. The file names do not change between builds, so the
    # browser asks again each time.
    return web.FileResponse(path, headers={"Cache-Control": "no-cache"})


async def api_game_files(request):
    """The addresses of index.wasm and index.data for a member (game_files.py)."""
    await require_member(request)
    files = request.app[KEY_GAME_FILES]
    if not files.build:
        raise web.HTTPNotFound()
    return web.json_response(files.describe(request.app[KEY_STORAGE]), headers={"Cache-Control": "no-store"})


async def health(request):
    return web.Response(text="ok")


# ---- start -----------------------------------------------------------------

async def open_http(app):
    app[KEY_HTTP] = aiohttp.ClientSession(timeout=aiohttp.ClientTimeout(total=15))
    yield
    await app[KEY_HTTP].close()


SETTING_NAMES = ("SESSION_SECRET", "HOUSE_PASSWORD", "SITE_PASSWORD", "PUBLIC_URL", "GOOGLE_CLIENT_ID",
                 "FACEBOOK_APP_ID", "APPLE_CLIENT_ID", "ADMIN_EMAILS", "BUCKET_NAME", "AWS_ENDPOINT_URL_S3", "YOUTUBE_STREAM_KEY")


def make_app(root):
    # Only the names of the settings go to the log, never the values.
    present = [name for name in SETTING_NAMES if os.environ.get(name)]
    print(f"server.py: settings with a value: {', '.join(present) or 'none'}", flush=True)
    secret = os.environ.get("SESSION_SECRET", "")
    house = os.environ.get("HOUSE_PASSWORD", "") or os.environ.get("SITE_PASSWORD", "")
    if not secret or not house:
        raise SystemExit("server.py: set SESSION_SECRET and HOUSE_PASSWORD (fly secrets set)")
    app = web.Application(middlewares=[security_headers], client_max_size=storage.MAX_SAVE_BYTES + 1024)
    app[KEY_SECRET] = secret.encode("utf-8")
    app[KEY_HOUSE] = house.encode("utf-8")
    app[KEY_ROOT] = os.path.realpath(root)
    app[KEY_PROVIDERS] = auth.load_providers()
    app[KEY_STORAGE] = storage.open_storage()
    app[KEY_BROADCAST] = broadcast.Broadcast()
    app[KEY_FAILURES] = {}
    app[KEY_ADMINS] = auth.admin_emails()
    app[KEY_GAME_FILES] = game_files.GameFiles(app[KEY_ROOT])
    app.cleanup_ctx.append(open_http)
    app.cleanup_ctx.append(game_files.publish_context(app[KEY_GAME_FILES], app[KEY_STORAGE]))
    app.add_routes([
        web.get("/healthz", health),
        web.get("/login", login),
        web.get("/auth/{provider}/start", auth_start),
        web.get("/auth/{provider}/callback", auth_callback),
        web.post("/auth/{provider}/callback", auth_callback),
        web.get("/house", house_form),
        web.post("/house", house_submit),
        web.post("/logout", logout),
        web.get("/admin", admin_members),
        web.get("/api/me", api_me),
        web.get("/api/saves", api_saves),
        web.get("/api/saves/{name}", api_save_get),
        web.put("/api/saves/{name}", api_save_put),
        web.post("/api/client-error", api_client_error),
        web.get("/api/broadcast", api_broadcast),
        web.get("/api/game-files", api_game_files),
        web.get("/", game_file),
        web.get("/{name}", game_file),
    ])
    print(f"server.py: serving {app[KEY_ROOT]}", flush=True)
    print(f"server.py: providers {', '.join(app[KEY_PROVIDERS]) or 'none'}", flush=True)
    print(f"server.py: storage {app[KEY_STORAGE].description}", flush=True)
    print(f"server.py: broadcast {'on' if app[KEY_BROADCAST].enabled else 'off'}", flush=True)
    print(f"server.py: {len(app[KEY_ADMINS])} admin emails", flush=True)
    print(f"server.py: game build {app[KEY_GAME_FILES].build or 'none'}", flush=True)
    return app


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_FOLDER
    port = int(os.environ.get("PORT", DEFAULT_PORT))
    web.run_app(make_app(root), host="0.0.0.0", port=port, print=None, access_log=None)
    return 0


if __name__ == "__main__":
    sys.exit(main())
