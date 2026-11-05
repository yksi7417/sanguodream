"""The routes of the shared games (rooms.py; docs/MULTIPLAYER.md).

The lobby, the start image, the snapshots, the invite link and the socket of a
room. server.py adds these routes to the application.
"""
import asyncio
import json
import re

import aiohttp
from aiohttp import web

import auth
import rooms
from http_common import (KEY_GAME_FILES, KEY_ROOMS, current_user, read_object, redirect, refused,
                         require_member, same_origin, set_cookie, user_of_token)


async def find_room(request):
    room = await request.app[KEY_ROOMS].get(request.match_info["gid"])
    if not room:
        raise web.HTTPNotFound()
    return room


async def api_discord_channel(request):
    """MP8: the room of a voice channel of Discord, or 404."""
    await require_member(request)
    try:
        room = await request.app[KEY_ROOMS].room_of_channel(request.match_info["channel"])
    except rooms.RoomError:
        room = None
    if not room:
        raise web.HTTPNotFound()
    return web.json_response(room.public(), headers={"Cache-Control": "no-store"})


async def api_games(request):
    """The games of the member: the games of the host and of the seats."""
    user = await require_member(request)
    games = await request.app[KEY_ROOMS].games_of(user["uid"])
    return web.json_response({"games": [room.summary(user["uid"]) for room in games]}, headers={"Cache-Control": "no-store"})


async def api_games_create(request):
    user = await require_member(request)
    body = await read_object(request)
    channel = body.get("channel")   # MP8: the voice channel of a Discord Activity: one room for each channel
    try:
        if channel is not None and await request.app[KEY_ROOMS].room_of_channel(channel):
            raise rooms.RoomError("the voice channel has a room")
        room = await request.app[KEY_ROOMS].create(
            user, body.get("name"), body.get("scenario"), body.get("pack"), request.app[KEY_GAME_FILES].build or "",
            body.get("mode"), body.get("turn_seconds"), body.get("factions"))
        if channel is not None:
            await request.app[KEY_ROOMS].link_channel(room, channel)
    except rooms.RoomError as error:
        return refused(error)
    print(f"rooms: {user['name']} made the game {room.gid}", flush=True)
    return web.json_response(room.public(), status=201)


async def api_game(request):
    user = await require_member(request)
    room = await find_room(request)
    data = room.public()
    data["you_host"] = user["uid"] == room.record["host"]   # the lobby shows the options of the host
    data["you_viewer"] = room.is_viewer(user["uid"])        # MP7: the page opens the socket, or it offers "Watch"
    seat = room.seat_of(user["uid"])
    data["you_seat"] = seat is not None                     # MP9: a person without a seat can take a free seat
    data["you_force"] = seat["force"] if seat else None     # the lobby gives "Leave" only on the seat of this person
    return web.json_response(data, headers={"Cache-Control": "no-store"})


async def api_game_seat(request):
    user = await require_member(request)
    room = await find_room(request)
    body = await read_object(request)
    force = body.get("force")
    if force is not None and not isinstance(force, int):
        raise web.HTTPBadRequest(text="bad force")
    async with room.lock:
        try:
            await request.app[KEY_ROOMS].take_seat(room, user, force)
        except rooms.RoomError as error:
            return refused(error)
    return web.json_response(room.public())


async def api_game_options(request):
    """The host sets the mode, the time of a turn and the pause option (MP4)."""
    user = await require_member(request)
    room = await find_room(request)
    body = await read_object(request)
    async with room.lock:
        try:
            await request.app[KEY_ROOMS].set_options(room, user, body)
        except rooms.RoomError as error:
            return refused(error)
    return web.json_response(room.public())


async def api_game_watch(request):
    """MP7: a member watches a game that runs."""
    user = await require_member(request)
    room = await find_room(request)
    async with room.lock:
        try:
            await request.app[KEY_ROOMS].watch(room, user)
        except rooms.RoomError as error:
            return refused(error)
    return web.json_response(room.public())


async def api_game_start(request):
    user = await require_member(request)
    room = await find_room(request)
    async with room.lock:
        try:
            await request.app[KEY_ROOMS].start(room, user)
        except rooms.RoomError as error:
            return refused(error)
    return web.json_response(room.public())


async def api_game_start_image_put(request):
    user = await require_member(request)
    room = await find_room(request)
    if request.content_length is not None and request.content_length > rooms.MAX_START_IMAGE_BYTES:
        raise web.HTTPRequestEntityTooLarge(max_size=rooms.MAX_START_IMAGE_BYTES, actual_size=request.content_length)
    data = await request.read()
    async with room.lock:
        try:
            await request.app[KEY_ROOMS].put_start_image(room, user, data)
        except rooms.RoomError as error:
            return refused(error)
    return web.json_response(room.public())


async def api_game_start_image_get(request):
    user = await require_member(request)
    room = await find_room(request)
    try:
        data = await request.app[KEY_ROOMS].start_image(room, user)
    except rooms.RoomError as error:
        return refused(error)
    if data is None:
        raise web.HTTPNotFound()
    return web.Response(body=data, content_type="application/octet-stream", headers={"Cache-Control": "no-store"})


async def api_game_snapshot_put(request):
    """MP5: the author of the step that ends a game turn puts the image of the state.
    The query gives the checksum of the state."""
    user = await require_member(request)
    room = await find_room(request)
    if request.content_length is not None and request.content_length > rooms.MAX_START_IMAGE_BYTES:
        raise web.HTTPRequestEntityTooLarge(max_size=rooms.MAX_START_IMAGE_BYTES, actual_size=request.content_length)
    try:
        step = int(request.match_info["step"])
    except ValueError:
        raise web.HTTPBadRequest(text="bad step")
    data = await request.read()
    async with room.lock:
        try:
            await request.app[KEY_ROOMS].put_snapshot(room, user, step, request.query.get("checksum"), data)
        except rooms.RoomError as error:
            return refused(error)
    return web.json_response({"snapshot": step})


def snapshots_in_bucket(request):
    """The CORS rule of the bucket lets the pages reach it only after the publish of the game files."""
    return request.app[KEY_GAME_FILES].in_bucket and hasattr(request.app[KEY_ROOMS].store, "presigned_put")


async def api_game_snapshot_upload(request):
    """The host asks for a signed address, and then uploads the save directly to the bucket.
    Without a bucket, the address is null, and the page uses the route of api_game_snapshot_put."""
    user = await require_member(request)
    room = await find_room(request)
    async with room.lock:
        try:
            url = await request.app[KEY_ROOMS].snapshot_upload_url(room, user, int(request.match_info["step"]), request.query.get("checksum"))
        except rooms.RoomError as error:
            return refused(error)
    return web.json_response({"url": url if snapshots_in_bucket(request) else None})


async def api_game_snapshot_finish(request):
    """The host uploaded the save to the bucket. The server records it as the latest save."""
    user = await require_member(request)
    room = await find_room(request)
    step = int(request.match_info["step"])
    async with room.lock:
        try:
            await request.app[KEY_ROOMS].finish_snapshot(room, user, step, request.query.get("checksum"))
        except rooms.RoomError as error:
            return refused(error)
    return web.json_response({"snapshot": step})


async def api_game_snapshot_latest(request):
    """MP5: the latest snapshot. All players use this one address. With a bucket, the server
    redirects to a signed address, and the name of the file gives its step. With the query
    direct=0, or without a bucket, the server sends the bytes with the header X-Snapshot-Step."""
    user = await require_member(request)
    room = await find_room(request)
    store = request.app[KEY_ROOMS]
    try:
        if snapshots_in_bucket(request) and request.query.get("direct") != "0":
            step = await store.latest_snapshot_step(room, user)
            if step is None:
                raise web.HTTPNotFound(text="no snapshot")
            url = store.store.presigned_get(rooms.snapshot_key(room.gid, step), rooms.SNAPSHOT_URL_SECONDS)
            raise web.HTTPFound(url, headers={"Cache-Control": "no-store"})
        latest = await store.latest_snapshot(room, user)
    except rooms.RoomError as error:
        return refused(error)
    if latest is None:
        raise web.HTTPNotFound(text="no snapshot")
    step, data = latest
    return web.Response(body=data, content_type="application/octet-stream",
                        headers={"Cache-Control": "no-store", "X-Snapshot-Step": str(step)})


async def api_game_desync_put(request):
    """MP6: the image of the state of a player after the step of a desync, for the developer."""
    user = await require_member(request)
    room = await find_room(request)
    if request.content_length is not None and request.content_length > rooms.MAX_START_IMAGE_BYTES:
        raise web.HTTPRequestEntityTooLarge(max_size=rooms.MAX_START_IMAGE_BYTES, actual_size=request.content_length)
    data = await request.read()
    async with room.lock:
        try:
            await request.app[KEY_ROOMS].put_desync_image(room, user, int(request.match_info["step"]), data)
        except rooms.RoomError as error:
            return refused(error)
    return web.json_response({"desync": int(request.match_info["step"])})


NEXT_COOKIE = "hexdom_next"
NEXT_SECONDS = 3600
INVITE_PATH = re.compile(r"/r/[A-Z]{6}")


async def invite(request):
    """The invite link of a room: a member goes to the game page of the room.
    A visitor signs in first, and the page "/" then sends the visitor back here."""
    user = await current_user(request)
    if not user:
        response = redirect("/login")
        if INVITE_PATH.fullmatch(request.path):
            set_cookie(response, request, NEXT_COOKIE, request.path, NEXT_SECONDS)
        return response
    room = await request.app[KEY_ROOMS].find_code(request.match_info["code"].upper())
    response = redirect(f"/?game={room.gid}") if room else web.Response(status=404, text="no game has this invite code")
    response.del_cookie(NEXT_COOKIE, path="/")
    return response


TOKEN_HELLO_SECONDS = 10   # MP8: a socket without the cookie must send the hello with the token in this time
TOKEN_CLOSE_CODE = 4401    # MP8: the close code of a bad token: 4000 + the HTTP status 401


async def token_hello(request, socket, room):
    """MP8: the page of the Discord Activity has no cookie. Its first message is the hello
    with the session token. Give the user and the hello, or (None, None) after a 401."""
    try:
        message = await socket.receive(timeout=TOKEN_HELLO_SECONDS)
        hello = json.loads(message.data) if message.type == aiohttp.WSMsgType.TEXT else {}
    except (asyncio.TimeoutError, ValueError, TypeError):
        hello = {}
    token = hello.get("token") if isinstance(hello, dict) else None
    user = await user_of_token(request.app, token if isinstance(token, str) else "")
    if user and room.is_viewer(user["uid"]):
        return user, message.data
    await socket.send_json({"type": "error", "reason": "401"})
    await socket.close(code=TOKEN_CLOSE_CODE, message=b"no member session")
    return None, None


async def api_game_socket(request):
    """The WebSocket of a room: the steps, the checksums and the chat (rooms.py). A page
    without the cookie asks for "?token=1" and sends its session token in the hello (MP8)."""
    by_token = request.query.get("token") == "1" and not request.cookies.get(auth.SESSION_COOKIE)
    user = None if by_token else await require_member(request)
    if not same_origin(request):
        raise web.HTTPForbidden(text="cross-origin request")
    room = await find_room(request)
    if user and not room.is_viewer(user["uid"]):
        raise web.HTTPForbidden(text="no seat in this game")
    socket = web.WebSocketResponse(max_msg_size=rooms.MAX_STEP_BYTES, heartbeat=rooms.PING_SECONDS)
    await socket.prepare(request)
    hello = None
    if by_token:
        user, hello = await token_hello(request, socket, room)
        if not user:
            return socket
    peer = rooms.Peer(socket, user)
    async with room.lock:
        old = rooms.join(room, peer)
    if old:
        await old.socket.close(code=aiohttp.WSCloseCode.POLICY_VIOLATION, message=b"a new socket of the same member")
    try:
        if hello:
            async with room.lock:   # the hello of the token: the first message of the socket
                try:
                    await rooms.handle_text(request.app[KEY_ROOMS], room, peer, hello)
                except rooms.RoomError as error:
                    await peer.send_json({"type": "error", "reason": str(error)})
        async for message in socket:
            if message.type not in (aiohttp.WSMsgType.TEXT, aiohttp.WSMsgType.BINARY) or peer.dead:
                break
            async with room.lock:
                try:
                    if message.type == aiohttp.WSMsgType.TEXT:
                        ok = await rooms.handle_text(request.app[KEY_ROOMS], room, peer, message.data)
                    else:
                        ok = await rooms.handle_binary(request.app[KEY_ROOMS], room, peer, message.data)
                except rooms.RoomError as error:
                    ok = True
                    await peer.send_json({"type": "error", "reason": str(error)})
            if not ok:
                await socket.close(code=aiohttp.WSCloseCode.POLICY_VIOLATION, message=b"too many messages")
                break
    finally:
        async with room.lock:
            await rooms.leave(request.app[KEY_ROOMS], room, peer)
    return socket
