"""The two big files of the game in the bucket (docs/WEB.md, "The game files in the bucket").

The server sends index.wasm and index.data from the bucket in place of the
machine, because the bucket has no fee for the traffic out. Only members get
the files:
- The bucket stays private. The server gives a member a presigned address for
  each file, with an end time of GAME_URL_SECONDS.
- The page asks for the addresses through /api/game-files (server.py), and
  the server checks the member first.

At the start, the server puts the files of its build in the bucket, at
builds/<build id>/<name>, if they are not there. It sets the CORS rule of the
bucket and removes the old builds. Thus a deploy needs no key on the computer
of the developer. Without the bucket, or after an error, the addresses are
the files of the server, as before.
"""
import asyncio
import hashlib
import os
import sys
import time

GAME_FILE_NAMES = ("index.wasm", "index.data")
CONTENT_TYPES = {"index.wasm": "application/wasm", "index.data": "application/octet-stream"}
GAME_URL_SECONDS = 600
BUILD_ID_DIGITS = 16
BUILDS_PREFIX = "builds/"
BUILDS_KEPT = 5
CACHE_CONTROL = "private, max-age=31536000, immutable"
HASH_CHUNK_BYTES = 1024 * 1024
BUILD_ID_FILE = "build_id"
# Tigris can answer 408 to one request. The server tries again after these waits; 0 ends the tries.
PUBLISH_RETRY_SECONDS = (10, 30, 90, 0)


def build_id_of(root):
    """The first 16 hex digits of the SHA-256 of the two files, or None without them."""
    digest = hashlib.sha256()
    for name in GAME_FILE_NAMES:
        path = os.path.join(root, name)
        if not os.path.isfile(path):
            return None
        with open(path, "rb") as source:
            for chunk in iter(lambda: source.read(HASH_CHUNK_BYTES), b""):
                digest.update(chunk)
    return digest.hexdigest()[:BUILD_ID_DIGITS]


def save_build_id(root):
    """The Dockerfile calls this function, so a start of the server does not read the big files."""
    with open(os.path.join(root, BUILD_ID_FILE), "w", encoding="ascii") as target:
        target.write(build_id_of(root))


def saved_build_id(root):
    """The id in the file of save_build_id, or None. A file of the machine stays out of the
    cache of the system after a stop, so a hash at the start takes seconds on the machine."""
    if not all(os.path.isfile(os.path.join(root, name)) for name in GAME_FILE_NAMES):
        return None
    try:
        with open(os.path.join(root, BUILD_ID_FILE), encoding="ascii") as source:
            return source.read().strip() or None
    except OSError:
        return None


def site_origin():
    """The origin of the pages for the CORS rule, without a request."""
    configured = os.environ.get("PUBLIC_URL", "").rstrip("/")
    if configured:
        return configured
    fly_app = os.environ.get("FLY_APP_NAME", "")
    return f"https://{fly_app}.fly.dev" if fly_app else ""


class GameFiles:
    def __init__(self, root):
        self.root = root
        self.build = saved_build_id(root) or build_id_of(root)
        self.sizes = {name: os.path.getsize(os.path.join(root, name)) for name in GAME_FILE_NAMES} if self.build else {}
        self.in_bucket = False   # True after the files and the CORS rule are in the bucket

    def key(self, name, build=None):
        return f"{BUILDS_PREFIX}{build or self.build}/{name}"

    def describe(self, storage):
        """The answer of /api/game-files: the build id and an address for each file."""
        use_bucket = self.in_bucket and hasattr(storage, "presigned_get")
        files = {}
        for name in GAME_FILE_NAMES:
            url = storage.presigned_get(self.key(name), GAME_URL_SECONDS) if use_bucket else name
            files[name] = {"url": url, "fallback": name, "size": self.sizes[name]}
        return {"build": self.build, "source": "bucket" if use_bucket else "server",
                "expires": int(time.time()) + GAME_URL_SECONDS if use_bucket else None, "files": files}

    async def publish(self, storage):
        """Put the files of this build in the bucket. After the last failed try, the server keeps its own files."""
        if not self.build or not hasattr(storage, "put_file"):
            return
        origin = site_origin()
        if not origin:
            print("game_files: no PUBLIC_URL or FLY_APP_NAME for the CORS rule; the server sends the files", flush=True)
            return
        for attempt, delay in enumerate(PUBLISH_RETRY_SECONDS, start=1):
            if await self.publish_once(storage, origin, attempt):
                return
            if delay:
                await asyncio.sleep(delay)

    async def publish_once(self, storage, origin, attempt):
        try:
            for name in GAME_FILE_NAMES:
                if await storage.exists(self.key(name)):
                    continue
                path = os.path.join(self.root, name)
                gzip_path = path + ".gz"   # the Dockerfile makes it
                source, encoding = (gzip_path, "gzip") if os.path.isfile(gzip_path) else (path, None)
                started = time.monotonic()
                await storage.put_file(self.key(name), source, CONTENT_TYPES[name], encoding, CACHE_CONTROL)
                print(f"game_files: {name} of build {self.build} is in the bucket "
                      f"({os.path.getsize(source)} bytes, {time.monotonic() - started:.1f} s)", flush=True)
            # PUT: the host uploads a save of a shared game directly to the bucket.
            await storage.set_cors([origin], ["GET", "HEAD", "PUT"], ["Range", "Content-Type"])
            await self.remove_old_builds(storage)
        except Exception as error:   # boto3 has many kinds of errors; each one keeps the files on the server
            print(f"game_files: try {attempt}: the bucket refused the game files, the server sends them: {error}",
                  flush=True)
            return False
        self.in_bucket = True
        print(f"game_files: the members get build {self.build} from the bucket", flush=True)
        return True

    async def remove_old_builds(self, storage):
        newest = {}
        keys = {}
        for key, modified in await storage.list_keys(BUILDS_PREFIX):
            build = key[len(BUILDS_PREFIX):].split("/", 1)[0]
            newest[build] = max(newest.get(build, 0), modified)
            keys.setdefault(build, []).append(key)
        kept = sorted(newest, key=newest.get, reverse=True)[:BUILDS_KEPT]
        for build in newest:
            if build in kept or build == self.build:
                continue
            for key in keys[build]:
                await storage.delete(key)
            print(f"game_files: removed the old build {build} from the bucket", flush=True)


def publish_context(files, storage):
    """An aiohttp cleanup context: publish in the background, so the health check answers at once."""
    async def context(app):
        task = asyncio.create_task(files.publish(storage))
        yield
        task.cancel()
    return context


if __name__ == "__main__":
    save_build_id(sys.argv[1])
