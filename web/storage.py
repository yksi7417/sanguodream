"""The storage of the users and the saves of the site (docs/WEB.md).

Two backends have the same functions:
- S3Storage keeps the files in an S3 bucket, for example Tigris on fly.io.
  The command `fly storage create` sets BUCKET_NAME, AWS_ACCESS_KEY_ID,
  AWS_SECRET_ACCESS_KEY, AWS_ENDPOINT_URL_S3 and AWS_REGION. boto3 reads
  all of them except BUCKET_NAME.
- FolderStorage keeps the files in a local folder, for the tests and for
  a local server.

The keys:
- users/<uid>.json: the record of a user, as JSON.
- saves/<uid>/<name>: a save file of a user.
- signin/<hash>.json, emails/<hash>.json: the tokens and the index of the sign-in
  with an email link (email_link.py).
- builds/<build id>/<name>: index.wasm and index.data of a build, only in the
  bucket (game_files.py).
"""
import asyncio
import json
import os
import re

SAVE_NAME = re.compile(r"^[A-Za-z0-9_][A-Za-z0-9_.-]{0,63}$")
UID = re.compile(r"^[0-9a-f]{24}$")
MAX_SAVE_BYTES = 16 * 1024 * 1024


def valid_save_name(name):
    return bool(SAVE_NAME.match(name))


def user_key(uid):
    if not UID.match(uid):
        raise ValueError("bad uid")
    return f"users/{uid}.json"


def save_key(uid, name):
    if not UID.match(uid) or not valid_save_name(name):
        raise ValueError("bad save key")
    return f"saves/{uid}/{name}"


def save_prefix(uid):
    if not UID.match(uid):
        raise ValueError("bad uid")
    return f"saves/{uid}/"


class FolderStorage:
    def __init__(self, root):
        self.root = os.path.realpath(root)
        os.makedirs(self.root, exist_ok=True)
        self.description = f"folder {self.root}"

    def _path(self, key):
        full = os.path.realpath(os.path.join(self.root, key))
        if not full.startswith(self.root + os.sep):
            raise ValueError("key outside the storage folder")
        return full

    async def get(self, key):
        path = self._path(key)
        if not os.path.isfile(path):
            return None
        with open(path, "rb") as source:
            return source.read()

    async def put(self, key, data):
        path = self._path(key)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        temporary = path + ".part"
        with open(temporary, "wb") as target:
            target.write(data)
        os.replace(temporary, path)

    async def delete(self, key):
        path = self._path(key)
        if os.path.isfile(path):
            os.remove(path)

    async def list(self, prefix):
        folder = self._path(prefix.rstrip("/"))
        if not os.path.isdir(folder):
            return []
        items = []
        for name in sorted(os.listdir(folder)):
            path = os.path.join(folder, name)
            if os.path.isfile(path) and valid_save_name(name):
                stat = os.stat(path)
                items.append({"name": name, "size": stat.st_size, "modified": int(stat.st_mtime)})
        return items


class S3Storage:
    def __init__(self, bucket):
        import boto3  # Only the S3 backend needs boto3.
        from botocore.config import Config

        self.bucket = bucket
        # A presigned address needs SigV4. Tigris and S3 accept it for each request.
        self.client = boto3.client("s3", config=Config(signature_version="s3v4"))
        self.description = f"bucket {bucket}"

    async def get(self, key):
        def read():
            try:
                return self.client.get_object(Bucket=self.bucket, Key=key)["Body"].read()
            except self.client.exceptions.NoSuchKey:
                return None

        return await asyncio.to_thread(read)

    async def put(self, key, data):
        await asyncio.to_thread(self.client.put_object, Bucket=self.bucket, Key=key, Body=data)

    async def list(self, prefix):
        def read():
            items = []
            pages = self.client.get_paginator("list_objects_v2").paginate(Bucket=self.bucket, Prefix=prefix)
            for page in pages:
                for entry in page.get("Contents", []):
                    name = entry["Key"][len(prefix):]
                    if valid_save_name(name):
                        modified = int(entry["LastModified"].timestamp())
                        items.append({"name": name, "size": entry["Size"], "modified": modified})
            return sorted(items, key=lambda item: item["name"])

        return await asyncio.to_thread(read)

    # ---- the game files (game_files.py) ----

    async def exists(self, key):
        def head():
            try:
                self.client.head_object(Bucket=self.bucket, Key=key)
                return True
            except self.client.exceptions.ClientError as error:
                if error.response.get("Error", {}).get("Code") in ("404", "NoSuchKey", "NotFound"):
                    return False
                raise

        return await asyncio.to_thread(head)

    async def put_file(self, key, path, content_type, content_encoding, cache_control):
        extra = {"ContentType": content_type, "CacheControl": cache_control}
        if content_encoding:
            extra["ContentEncoding"] = content_encoding
        await asyncio.to_thread(self.client.upload_file, path, self.bucket, key, ExtraArgs=extra)

    async def set_cors(self, origins, methods, headers):
        rules = {"CORSRules": [{"AllowedOrigins": origins, "AllowedMethods": methods,
                                "AllowedHeaders": headers, "MaxAgeSeconds": 3600}]}
        await asyncio.to_thread(self.client.put_bucket_cors, Bucket=self.bucket, CORSConfiguration=rules)

    async def list_keys(self, prefix):
        """Each key under the prefix, with the time of its last change."""
        def read():
            keys = []
            for page in self.client.get_paginator("list_objects_v2").paginate(Bucket=self.bucket, Prefix=prefix):
                keys += [(entry["Key"], int(entry["LastModified"].timestamp())) for entry in page.get("Contents", [])]
            return keys

        return await asyncio.to_thread(read)

    async def delete(self, key):
        await asyncio.to_thread(self.client.delete_object, Bucket=self.bucket, Key=key)

    def presigned_get(self, key, seconds):
        """An address that reads one object until the end time. The signing needs no request."""
        return self.client.generate_presigned_url("get_object", Params={"Bucket": self.bucket, "Key": key},
                                                  ExpiresIn=seconds)

    def presigned_put(self, key, seconds):
        """An address that writes one object until the end time (a save of a shared game)."""
        return self.client.generate_presigned_url("put_object", Params={"Bucket": self.bucket, "Key": key},
                                                  ExpiresIn=seconds)

    async def size(self, key):
        """The bytes of one object, or None if it is not there."""
        def head():
            try:
                return self.client.head_object(Bucket=self.bucket, Key=key)["ContentLength"]
            except self.client.exceptions.ClientError as error:
                if error.response.get("Error", {}).get("Code") in ("404", "NoSuchKey", "NotFound"):
                    return None
                raise

        return await asyncio.to_thread(head)


def open_storage():
    """Give the S3 backend if BUCKET_NAME is set, else the folder backend."""
    bucket = os.environ.get("BUCKET_NAME", "")
    if bucket:
        return S3Storage(bucket)
    # On fly.io a folder of the machine does not stay after a deploy, so the server needs the bucket.
    if os.environ.get("FLY_APP_NAME"):
        raise SystemExit("storage.py: BUCKET_NAME is empty; run flyctl storage create -a <app>")
    return FolderStorage(os.environ.get("STORAGE_DIR", "storage"))


async def read_user(storage, uid):
    data = await storage.get(user_key(uid))
    return json.loads(data) if data else None


async def all_users(storage):
    """Give each good record of a user."""
    users = []
    for row in await storage.list("users/"):
        try:
            record = json.loads(await storage.get("users/" + row["name"]) or b"null")
        except ValueError:
            continue
        if isinstance(record, dict) and "uid" in record:
            users.append(record)
    return users


async def write_user(storage, user):
    await storage.put(user_key(user["uid"]), json.dumps(user, ensure_ascii=False).encode("utf-8"))
