"""Writes the page of the web build: web/shell.html with its pictures inside.

The page shows the splash art and the progress bar while the browser
downloads the game (docs/WEB.md). The picture must show before any other
file arrives, so the page carries it as a data URI. The favicon is a data
URI too. The server then needs no new route for them.

build_web.bat runs this script before em++:
    python misc\\web_shell.py web\\shell.html build\\web_obj\\shell.html

The script replaces these tokens of the page:
- HEXDOM_SPLASH_URI with web/splash.jpg (misc/make_splash.py)
- HEXDOM_FAVICON_URI with web/favicon.png (misc/make_splash.py)
"""
import base64
import os
import sys

WEB = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "web")
TOKENS = (
    ("HEXDOM_SPLASH_URI", "splash.jpg", "image/jpeg"),
    ("HEXDOM_FAVICON_URI", "favicon.png", "image/png"),
)


def data_uri(name, media_type):
    with open(os.path.join(WEB, name), "rb") as picture:
        return "data:" + media_type + ";base64," + base64.b64encode(picture.read()).decode("ascii")


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: web_shell.py SHELL_IN SHELL_OUT")
    with open(sys.argv[1], encoding="utf-8") as source:
        page = source.read()
    for token, name, media_type in TOKENS:
        if token not in page:
            sys.exit("web_shell.py: the page has no " + token)
        page = page.replace(token, data_uri(name, media_type))
    with open(sys.argv[2], "w", encoding="utf-8", newline="\n") as target:
        target.write(page)


if __name__ == "__main__":
    main()
