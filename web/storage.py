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

        self.bucket = bucket
        self.client = boto3.client("s3")
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


def open_storage():
    """Give the S3 backend if BUCKET_NAME is set, else the folder backend."""
    bucket = os.environ.get("BUCKET_NAME", "")
    if bucket:
        return S3Storage(bucket)
    return FolderStorage(os.environ.get("STORAGE_DIR", "storage"))


async def read_user(storage, uid):
    data = await storage.get(user_key(uid))
    return json.loads(data) if data else None


async def write_user(storage, user):
    await storage.put(user_key(user["uid"]), json.dumps(user, ensure_ascii=False).encode("utf-8"))
