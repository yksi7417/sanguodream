"""The YouTube live broadcast of the game, an option (docs/WEB.md).

The page records the game canvas with MediaRecorder and sends the chunks
through the WebSocket /api/broadcast. The server pipes the chunks into
ffmpeg, and ffmpeg sends the stream to the RTMP ingest of YouTube. YouTube
keeps each broadcast as a video of the channel.

The broadcast is on only if the secret YOUTUBE_STREAM_KEY is set. Only one
broadcast runs at a time. The server never writes the stream key to the log.
"""
import asyncio
import collections
import json
import os

from aiohttp import WSMsgType, web

# YOUTUBE_INGEST is for a local check: ffmpeg then writes the stream to a file.
YOUTUBE_INGEST = os.environ.get("YOUTUBE_INGEST", "rtmp://a.rtmp.youtube.com/live2/")
STDERR_LINES_KEPT = 20
MAX_CHUNK_BYTES = 8 * 1024 * 1024
# A strategy game changes slowly: 24 frames each second at 720 lines is
# sufficient, and one shared CPU can encode it.
VIDEO_OPTIONS = [
    "-vf", "scale=-2:720,fps=24,format=yuv420p",
    "-c:v", "libx264", "-preset", "veryfast", "-tune", "zerolatency",
    "-b:v", "2500k", "-maxrate", "2500k", "-bufsize", "5000k", "-g", "48",
]
# YouTube wants an audio track. The game has no sound, so the track is silent.
# "-re" makes the silence come at the speed of a clock, as the video does.
# "-shortest" stops the output when the video stops.
AUDIO_INPUT = ["-re", "-f", "lavfi", "-i", "anullsrc=channel_layout=stereo:sample_rate=44100"]
AUDIO_OPTIONS = ["-c:a", "aac", "-b:a", "128k", "-shortest"]


class Broadcast:
    def __init__(self):
        self.stream_key = os.environ.get("YOUTUBE_STREAM_KEY", "")
        self.enabled = bool(self.stream_key)
        self.running = None   # the name of the user who broadcasts

    def command(self):
        return [
            "ffmpeg", "-y", "-hide_banner", "-loglevel", "warning",
            "-thread_queue_size", "512", "-i", "pipe:0",
            *AUDIO_INPUT,
            "-map", "0:v:0", "-map", "1:a:0",
            *VIDEO_OPTIONS, *AUDIO_OPTIONS,
            "-f", "flv", YOUTUBE_INGEST + self.stream_key,
        ]

    async def handle(self, request, user):
        if not self.enabled:
            raise web.HTTPNotFound()
        if self.running:
            raise web.HTTPConflict(text=f"{self.running} broadcasts now")
        socket = web.WebSocketResponse(max_msg_size=MAX_CHUNK_BYTES)
        await socket.prepare(request)
        self.running = user["name"]
        print(f"broadcast: {user['name']} starts", flush=True)
        process = await asyncio.create_subprocess_exec(
            *self.command(), stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.PIPE)
        errors = collections.deque(maxlen=STDERR_LINES_KEPT)
        reader = asyncio.create_task(self.read_errors(process, errors, socket))
        try:
            async for message in socket:
                if message.type == WSMsgType.BINARY:
                    process.stdin.write(message.data)
                    await process.stdin.drain()
                elif message.type == WSMsgType.ERROR:
                    break
        except (BrokenPipeError, ConnectionResetError):
            pass
        finally:
            self.running = None
            if process.stdin and not process.stdin.is_closing():
                process.stdin.close()
            try:
                await asyncio.wait_for(process.wait(), timeout=10)
            except asyncio.TimeoutError:
                process.kill()
            reader.cancel()
            print(f"broadcast: {user['name']} stops, ffmpeg exit {process.returncode}", flush=True)
            for line in errors:
                print(f"broadcast: ffmpeg: {line}", flush=True)
        return socket

    async def read_errors(self, process, errors, socket):
        while True:
            line = await process.stderr.readline()
            if not line:
                break
            text = line.decode("utf-8", "replace").rstrip().replace(self.stream_key, "<key>")
            errors.append(text)
            if not socket.closed:
                await socket.send_str(json.dumps({"ffmpeg": text}))
