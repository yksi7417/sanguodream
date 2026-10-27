"""Serve the web game behind a password (docs/WEB.md, "The fly.io site").

Usage: python -u web/server.py [FOLDER]

The server gives the files of FOLDER (default /srv/web) to a browser that
sends the correct user name and password through HTTP basic auth. It reads
these environment variables:
- SITE_PASSWORD: the password. The server does not start without it.
- SITE_USER: the user name. The default is "hexdom".
- PORT: the port. The default is 8080.

If the browser accepts gzip and the file FILE.gz exists, the server sends
FILE.gz. The Dockerfile makes these files. The server uses only the
standard library of Python.
"""
import base64
import email.utils
import hmac
import os
import sys
import time
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

DEFAULT_FOLDER = "/srv/web"
DEFAULT_USER = "hexdom"
DEFAULT_PORT = 8080
REALM = "hexdom"
# The server waits this time before it refuses a password, to make a guess slow.
FAILED_AUTH_DELAY_SECONDS = 1.0
COPY_CHUNK_SIZE = 1 << 20

CONTENT_TYPES = {
    ".html": "text/html; charset=utf-8",
    ".js": "text/javascript; charset=utf-8",
    ".wasm": "application/wasm",
    ".data": "application/octet-stream",
}


class GameHandler(BaseHTTPRequestHandler):
    server_version = "hexdom"
    sys_version = ""

    def do_GET(self):
        self.send_file(with_body=True)

    def do_HEAD(self):
        self.send_file(with_body=False)

    def authorized(self):
        header = self.headers.get("Authorization", "")
        scheme, _, encoded = header.partition(" ")
        if scheme.lower() != "basic":
            return False
        try:
            decoded = base64.b64decode(encoded.strip(), validate=True)
        except ValueError:
            return False
        user, _, password = decoded.partition(b":")
        user_ok = hmac.compare_digest(user, self.server.site_user)
        password_ok = hmac.compare_digest(password, self.server.site_password)
        return user_ok and password_ok

    def resolve(self):
        """Give the path of the file of the request, or None."""
        path = urllib.parse.unquote(urllib.parse.urlsplit(self.path).path)
        if "\0" in path:
            return None
        if path.endswith("/"):
            path += "index.html"
        root = self.server.root
        full = os.path.realpath(os.path.join(root, path.lstrip("/")))
        if not full.startswith(root + os.sep) or not os.path.isfile(full):
            return None
        return full

    def send_file(self, with_body):
        if not self.authorized():
            time.sleep(FAILED_AUTH_DELAY_SECONDS)
            self.send_response(401)
            self.send_header("WWW-Authenticate", f'Basic realm="{REALM}", charset="UTF-8"')
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        full = self.resolve()
        if full is None:
            self.send_error(404)
            return

        modified = int(os.path.getmtime(full))
        since = self.headers.get("If-Modified-Since")
        if since:
            try:
                if int(email.utils.parsedate_to_datetime(since).timestamp()) >= modified:
                    self.send_response(304)
                    self.send_common_headers(full, modified)
                    self.end_headers()
                    return
            except (TypeError, ValueError):
                pass

        send_path = full
        compressed = full + ".gz"
        accepts_gzip = "gzip" in self.headers.get("Accept-Encoding", "")
        if accepts_gzip and os.path.isfile(compressed):
            send_path = compressed

        self.send_response(200)
        self.send_common_headers(full, modified)
        if send_path != full:
            self.send_header("Content-Encoding", "gzip")
        self.send_header("Content-Length", str(os.path.getsize(send_path)))
        self.end_headers()
        if with_body:
            with open(send_path, "rb") as source:
                while True:
                    chunk = source.read(COPY_CHUNK_SIZE)
                    if not chunk:
                        break
                    self.wfile.write(chunk)

    def send_common_headers(self, full, modified):
        extension = os.path.splitext(full)[1]
        self.send_header("Content-Type", CONTENT_TYPES.get(extension, "application/octet-stream"))
        self.send_header("Last-Modified", email.utils.formatdate(modified, usegmt=True))
        # The file names do not change between builds, so the browser asks again each time.
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Vary", "Accept-Encoding")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("X-Frame-Options", "DENY")
        self.send_header("Referrer-Policy", "no-referrer")


def main():
    password = os.environ.get("SITE_PASSWORD", "")
    if not password:
        print("server.py: SITE_PASSWORD is empty; set it with fly secrets set", file=sys.stderr)
        return 1
    root = os.path.realpath(sys.argv[1] if len(sys.argv) > 1 else DEFAULT_FOLDER)
    port = int(os.environ.get("PORT", DEFAULT_PORT))

    server = ThreadingHTTPServer(("0.0.0.0", port), GameHandler)
    server.root = root
    server.site_user = os.environ.get("SITE_USER", DEFAULT_USER).encode("utf-8")
    server.site_password = password.encode("utf-8")
    print(f"server.py: serving {root} on port {port}", flush=True)
    server.serve_forever()
    return 0


if __name__ == "__main__":
    sys.exit(main())
