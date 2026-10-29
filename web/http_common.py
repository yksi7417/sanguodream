"""The parts of the server that each group of routes uses (server.py, games_api.py).

They are the keys of the application, the session of the user, the check of the
origin and the JSON body of a request.
"""
import json
import os
import time
import urllib.parse

import aiohttp
from aiohttp import web

import auth
import broadcast
import game_files
import rooms
import storage

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
KEY_ROOMS = web.AppKey("rooms", rooms.Rooms)


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


async def user_of_token(app, token):
    """Give the record of the user of a signed session token, or None."""
    session = auth.verify(app[KEY_SECRET], token)
    if not session:
        return None
    try:
        return await storage.read_user(app[KEY_STORAGE], session.get("uid", ""))
    except ValueError:
        return None


async def current_user(request):
    """Give the record of the signed-in user, or None. The page of the Discord Activity
    has no cookie: it sends the session token in the header Authorization (MP8)."""
    token = request.cookies.get(auth.SESSION_COOKIE, "")
    header = request.headers.get("Authorization", "")
    if not token and header.startswith("Bearer "):
        token = header[len("Bearer "):].strip()
    return await user_of_token(request.app, token)


def session_token(request, uid):
    return auth.sign(request.app[KEY_SECRET], {"uid": uid, "exp": int(time.time()) + auth.SESSION_SECONDS})


async def require_member(request):
    user = await current_user(request)
    if user and user.get("member"):
        return user
    if request.path.startswith("/api/"):
        raise web.HTTPUnauthorized()
    raise web.HTTPFound("/house" if user else "/login")


def same_origin(request):
    """A form post or an upload must come from a page of this site. The page of the
    Discord Activity has the origin <client id>.discordsays.com (MP8)."""
    origin = request.headers.get("Origin")
    if origin is None:
        return True
    host = urllib.parse.urlsplit(origin).netloc
    discord = request.app[KEY_PROVIDERS].get("discord")
    return host == request.host or bool(discord and host == f"{discord.client_id}.discordsays.com")


MAX_JSON_BYTES = 8 * 1024   # the body of a request of the rooms; the saves have their own limit


async def read_object(request):
    if request.content_length is not None and request.content_length > MAX_JSON_BYTES:
        raise web.HTTPRequestEntityTooLarge(max_size=MAX_JSON_BYTES, actual_size=request.content_length)
    data = await request.content.read(MAX_JSON_BYTES + 1)
    if len(data) > MAX_JSON_BYTES:
        raise web.HTTPRequestEntityTooLarge(max_size=MAX_JSON_BYTES, actual_size=len(data))
    try:
        body = json.loads(data)
    except ValueError:
        raise web.HTTPBadRequest(text="bad json")
    if not isinstance(body, dict):
        raise web.HTTPBadRequest(text="bad json")
    return body


def refused(error):
    return web.json_response({"error": str(error)}, status=409)
