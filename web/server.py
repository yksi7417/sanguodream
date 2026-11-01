"""The server of the web game (docs/WEB.md, "The fly.io site").

Usage: python -u web/server.py [FOLDER]

The server gives the files of FOLDER (default /srv/web) to a member. A
member is a user who signed in with a provider (auth.py) or with an email
link (email_link.py). The server also keeps the saves of each member (storage.py), relays
the steps of the shared games (games_api.py, rooms.py) and, as an option, relays a
YouTube broadcast (broadcast.py).

The environment variables:
- SESSION_SECRET: the key of the cookies. The server does not start
  without it.
- RESEND_API_KEY, EMAIL_FROM: the key of Resend and the sender of the
  emails. Without them, the sign-in page has no email form.
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
import mimetypes
import os
import sys
import time
import urllib.parse

import aiohttp
from aiohttp import web

import auth
import broadcast
import email_link
import game_files
import games_api
import pages
import referee
import rooms
import storage
from http_common import (KEY_ADMINS, KEY_BROADCAST, KEY_EMAIL, KEY_GAME_FILES, KEY_HTTP,
                         KEY_PROVIDERS, KEY_ROOMS, KEY_ROOT, KEY_SECRET, KEY_STORAGE, current_user,
                         public_url, read_object, redirect, require_member, same_origin, session_token,
                         set_cookie)

DEFAULT_FOLDER = "/srv/web"
DEFAULT_PORT = 8080
GAME_FILES = ("index.html", "index.js", "index.wasm", "index.data", "shared_game.js")
# Only the site and Discord can put a page of the site in a frame. The Discord
# Activity is a frame on discord.com, and its proxy is <client id>.discordsays.com.
# X-Frame-Options has no list of sites, so the site does not send it.
FRAME_ANCESTORS = "frame-ancestors 'self' https://discord.com https://*.discord.com https://*.discordsays.com"

mimetypes.add_type("application/wasm", ".wasm")
mimetypes.add_type("text/javascript", ".js")


# ---- the headers of each response --------------------------------------------

@web.middleware
async def security_headers(request, handler):
    if request.method not in ("GET", "HEAD") and not request.path.endswith("/callback") and not same_origin(request):
        raise web.HTTPForbidden(text="cross-origin request")
    response = await handler(request)
    response.headers.setdefault("X-Content-Type-Options", "nosniff")
    response.headers.setdefault("Content-Security-Policy", FRAME_ANCESTORS)
    response.headers.setdefault("Referrer-Policy", "same-origin")
    return response


# ---- sign-in ---------------------------------------------------------------

async def login(request):
    user = await current_user(request)
    if user:
        raise web.HTTPFound("/")
    error = request.query.get("error", "")
    page = pages.login_page(request.app[KEY_PROVIDERS], bool(request.app[KEY_EMAIL].sender), error)
    return web.Response(text=page, content_type="text/html")


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

    user = await sign_in_user(request, provider, subject, name, email, verified)
    response = redirect("/")
    set_cookie(response, request, auth.SESSION_COOKIE, session_token(request, user["uid"]), auth.SESSION_SECONDS)
    response.del_cookie(auth.STATE_COOKIE, path="/")
    return response


async def sign_in_user(request, provider, subject, name, email, verified):
    """Read or make the record of the user of a provider, and note the sign-in."""
    store = request.app[KEY_STORAGE]
    links = request.app[KEY_EMAIL]
    uid = auth.user_id(provider.name, subject)
    address = email_link.clean_address(email) if verified else ""
    # A user who came with an email link first has one account, if the provider verified the same address.
    user = await storage.read_user(store, uid) or (await links.account_of(address) if address else None) or {
        "uid": uid, "joined": int(time.time())}
    user["provider"] = provider.name
    # Apple gives the name only at the first sign-in, so an empty name keeps the old one.
    user["name"] = name or user.get("name") or auth.clean_name(email, f"{provider.label} user")
    if email:
        user["email"] = email
        user["email_verified"] = verified
    user["last_login"] = int(time.time())
    await storage.write_user(store, user)
    if address:
        await links.remember(address, user["uid"])
    print(f"auth: {user['name']} signed in with {provider.name}", flush=True)
    return user


async def email_request(request):
    """The form of the sign-in page. The reply is the same for each good address."""
    links = request.app[KEY_EMAIL]
    if not links.sender:
        raise web.HTTPNotFound()
    form = await request.post()
    address = email_link.clean_address(form.get("email"))
    if not address:
        page = pages.login_page(request.app[KEY_PROVIDERS], True, "Type a good email address.")
        return web.Response(text=page, content_type="text/html", status=400)
    confirm_url = f"{public_url(request)}/auth/email/confirm"
    if not await links.request(request.app[KEY_HTTP], address, email_link.client_ip(request), confirm_url):
        return web.Response(text=pages.email_limit_page(), content_type="text/html", status=429)
    return web.Response(text=pages.email_sent_page(), content_type="text/html")


def email_link_page(text, status=200):
    # The address of the link has the token. The default policy same-origin keeps it from other sites.
    # The policy no-referrer does not work here: the browser then posts the form with Origin: null.
    return web.Response(text=text, content_type="text/html", status=status,
                        headers={"Cache-Control": "no-store"})


async def email_confirm_form(request):
    """A mail scanner opens the link, so this GET does not use the token."""
    token = request.query.get("token", "")
    address = await request.app[KEY_EMAIL].peek(token)
    if not address:
        return email_link_page(pages.email_bad_link_page(), 400)
    return email_link_page(pages.email_confirm_page(token, address))


async def email_confirm(request):
    links = request.app[KEY_EMAIL]
    form = await request.post()
    address = await links.consume(form.get("token", ""))
    if not address:
        return email_link_page(pages.email_bad_link_page(), 400)
    user = await links.sign_in(address)
    response = redirect("/")
    set_cookie(response, request, auth.SESSION_COOKIE, session_token(request, user["uid"]), auth.SESSION_SECONDS)
    print(f"auth: {user['name']} signed in with an email link", flush=True)
    return response


async def api_discord_token(request):
    """MP8: the page of the Discord Activity changes the code of the Embedded App SDK for
    the Discord access token (the SDK needs it) and a session token of the site."""
    provider = request.app[KEY_PROVIDERS].get("discord")
    if not provider:
        raise web.HTTPNotFound()
    body = await read_object(request)
    try:
        access = await provider.access_token(request.app[KEY_HTTP], body.get("code"))
        subject, name, email, verified = await provider.user(request.app[KEY_HTTP], access)
    except (auth.AuthError, aiohttp.ClientError, asyncio.TimeoutError) as error:
        print(f"auth: discord activity sign-in failed: {error}", flush=True)
        raise web.HTTPUnauthorized(text="the Discord sign-in failed")
    user = await sign_in_user(request, provider, subject, name, email, verified)
    return web.json_response({"access_token": access, "token": session_token(request, user["uid"]),
                              "name": user["name"]},
                             headers={"Cache-Control": "no-store"})


DISCORD_SDK_URL = "/.proxy/jsdelivr/npm/@discord/embedded-app-sdk@2/+esm"   # the URL mapping /jsdelivr -> cdn.jsdelivr.net


async def api_discord_config(request):
    """MP8: the client id of the Discord app and the address of the Embedded App SDK, for
    the page of the Activity before its sign-in. Neither is a secret."""
    provider = request.app[KEY_PROVIDERS].get("discord")
    if not provider:
        raise web.HTTPNotFound()
    return web.json_response({"client_id": provider.client_id, "sdk": os.environ.get("DISCORD_SDK_URL") or DISCORD_SDK_URL})


async def api_session_token(request):
    """MP8: the session token of the member, for a page without the cookie (the
    Activity, and the browser test of the token). Another site cannot read the answer."""
    user = await require_member(request)
    return web.json_response({"token": session_token(request, user["uid"])}, headers={"Cache-Control": "no-store"})


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
    for record in await storage.all_users(store):
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

PUBLIC_GAME_FILES = ("index.js", "shared_game.js")   # MP8: the code of the page, without data; a page without the cookie loads it


def activity_page(request):
    """MP8: the page of the Discord Activity has no cookie. The page and its code have no
    data of the game, so they load without a session. The page then signs in with a token,
    and the game files need that token (prefetchGameFiles in shell.html)."""
    name = request.match_info.get("name") or "index.html"
    if name in PUBLIC_GAME_FILES:
        return True
    return name == "index.html" and ("frame_id" in request.query or request.query.get("activity") == "1")


async def game_file(request):
    if not activity_page(request):
        await require_member(request)
    following = request.cookies.get(games_api.NEXT_COOKIE, "")
    if request.path == "/" and games_api.INVITE_PATH.fullmatch(following):
        return redirect(following)   # an invite link from before the sign-in
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


SETTING_NAMES = ("SESSION_SECRET", "RESEND_API_KEY", "EMAIL_FROM", "PUBLIC_URL", "GOOGLE_CLIENT_ID",
                 "FACEBOOK_APP_ID", "APPLE_CLIENT_ID", "ADMIN_EMAILS", "BUCKET_NAME", "AWS_ENDPOINT_URL_S3", "YOUTUBE_STREAM_KEY")


def make_app(root):
    # Only the names of the settings go to the log, never the values.
    present = [name for name in SETTING_NAMES if os.environ.get(name)]
    print(f"server.py: settings with a value: {', '.join(present) or 'none'}", flush=True)
    secret = os.environ.get("SESSION_SECRET", "")
    if not secret:
        raise SystemExit("server.py: set SESSION_SECRET (fly secrets set)")
    app = web.Application(middlewares=[security_headers], client_max_size=storage.MAX_SAVE_BYTES + 1024)
    app[KEY_SECRET] = secret.encode("utf-8")
    app[KEY_ROOT] = os.path.realpath(root)
    app[KEY_PROVIDERS] = auth.load_providers()
    app[KEY_STORAGE] = storage.open_storage()
    app[KEY_BROADCAST] = broadcast.Broadcast()
    app[KEY_EMAIL] = email_link.EmailLinks(app[KEY_STORAGE], email_link.load_sender())
    app[KEY_ADMINS] = auth.admin_emails()
    app[KEY_GAME_FILES] = game_files.GameFiles(app[KEY_ROOT])
    script = referee.find_script()   # MP6: the referee decides a desync of two players
    app[KEY_ROOMS] = rooms.Rooms(app[KEY_STORAGE], build=app[KEY_GAME_FILES].build or "",
                                 referee=referee.Referee(script) if script else None)
    app.cleanup_ctx.append(open_http)
    app.cleanup_ctx.append(email_link.migrate_context(app[KEY_EMAIL]))
    app.cleanup_ctx.append(game_files.publish_context(app[KEY_GAME_FILES], app[KEY_STORAGE]))
    app.cleanup_ctx.append(rooms.flush_context(app[KEY_ROOMS]))
    app.add_routes([
        web.get("/healthz", health),
        web.get("/login", login),
        web.get("/auth/{provider}/start", auth_start),
        web.get("/auth/{provider}/callback", auth_callback),
        web.post("/auth/{provider}/callback", auth_callback),
        web.post("/auth/email/request", email_request),
        web.get("/auth/email/confirm", email_confirm_form),
        web.post("/auth/email/confirm", email_confirm),
        web.post("/logout", logout),
        web.get("/admin", admin_members),
        web.get("/api/me", api_me),
        web.get("/api/saves", api_saves),
        web.get("/api/saves/{name}", api_save_get),
        web.put("/api/saves/{name}", api_save_put),
        web.post("/api/client-error", api_client_error),
        web.get("/api/broadcast", api_broadcast),
        web.get("/api/game-files", api_game_files),
        web.get("/api/discord/config", api_discord_config),
        web.post("/api/discord/token", api_discord_token),
        web.get("/api/discord/channel/{channel}", games_api.api_discord_channel),
        web.get("/api/session-token", api_session_token),
        web.get("/api/games", games_api.api_games),
        web.post("/api/games", games_api.api_games_create),
        web.get("/api/games/{gid}", games_api.api_game),
        web.post("/api/games/{gid}/seat", games_api.api_game_seat),
        web.post("/api/games/{gid}/options", games_api.api_game_options),
        web.post("/api/games/{gid}/watch", games_api.api_game_watch),
        web.post("/api/games/{gid}/start", games_api.api_game_start),
        web.get("/api/games/{gid}/socket", games_api.api_game_socket),
        web.put("/api/games/{gid}/start-image", games_api.api_game_start_image_put),
        web.get("/api/games/{gid}/start-image", games_api.api_game_start_image_get),
        web.put("/api/games/{gid}/snap/{step:[0-9]{1,9}}", games_api.api_game_snapshot_put),
        web.get("/api/games/{gid}/snap/latest", games_api.api_game_snapshot_latest),
        web.put("/api/games/{gid}/desync/{step:[0-9]{1,9}}", games_api.api_game_desync_put),
        web.get("/r/{code}", games_api.invite),
        web.get("/", game_file),
        web.get("/{name}", game_file),
    ])
    print(f"server.py: serving {app[KEY_ROOT]}", flush=True)
    print(f"server.py: providers {', '.join(app[KEY_PROVIDERS]) or 'none'}", flush=True)
    print(f"server.py: storage {app[KEY_STORAGE].description}", flush=True)
    print(f"server.py: email link {'on' if app[KEY_EMAIL].sender else 'off'}", flush=True)
    print(f"server.py: broadcast {'on' if app[KEY_BROADCAST].enabled else 'off'}", flush=True)
    print(f"server.py: {len(app[KEY_ADMINS])} admin emails", flush=True)
    print(f"server.py: game build {app[KEY_GAME_FILES].build or 'none'}", flush=True)
    print(f"server.py: referee {script or 'none'}", flush=True)
    return app


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_FOLDER
    port = int(os.environ.get("PORT", DEFAULT_PORT))
    web.run_app(make_app(root), host="0.0.0.0", port=port, print=None, access_log=None)
    return 0


if __name__ == "__main__":
    sys.exit(main())
