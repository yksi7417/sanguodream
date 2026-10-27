"""The HTML pages of the sign-in: the sign-in page and the house password page."""
import html

STYLE = """
:root { color-scheme: dark; }
body { margin: 0; min-height: 100vh; display: flex; align-items: center; justify-content: center;
       background: #17191f; color: #d8d8d8; font: 15px system-ui, sans-serif; }
main { width: min(360px, calc(100vw - 32px)); }
h1 { font-size: 22px; font-weight: 600; margin: 0 0 6px; }
p { color: #9aa0aa; margin: 0 0 20px; line-height: 1.4; }
a.button, button { display: block; box-sizing: border-box; width: 100%; padding: 11px 14px; margin: 0 0 10px;
       border: 1px solid #3a3f4b; border-radius: 6px; background: #232731; color: #e8e8e8; font: inherit;
       text-align: center; text-decoration: none; cursor: pointer; }
a.button:hover, button:hover { background: #2c313d; }
input { box-sizing: border-box; width: 100%; padding: 10px 12px; margin: 0 0 12px; border: 1px solid #3a3f4b;
        border-radius: 6px; background: #101217; color: #e8e8e8; font: inherit; }
.error { color: #f08a7e; }
"""


def page(title, body):
    return f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{html.escape(title)}</title><style>{STYLE}</style></head>
<body><main>{body}</main></body></html>"""


def login_page(providers, error=""):
    if providers:
        buttons = "".join(
            f'<a class="button" id="login-{name}" href="/auth/{name}/start">Sign in with {html.escape(provider.label)}</a>'
            for name, provider in providers.items()
        )
    else:
        buttons = '<p class="error">No sign-in provider is set up on this server.</p>'
    message = f'<p class="error">{html.escape(error)}</p>' if error else ""
    return page("hexdom: sign in", f"<h1>hexdom</h1><p>Sign in to play.</p>{message}{buttons}")


def house_page(name, error=""):
    message = f'<p class="error">{html.escape(error)}</p>' if error else ""
    body = f"""<h1>House password</h1>
<p>Welcome, {html.escape(name)}. Type the house password one time to join.</p>{message}
<form method="post" action="/house">
<input type="password" name="password" id="house-password" autocomplete="current-password" autofocus required>
<button type="submit" id="house-submit">Join</button>
</form>
<form method="post" action="/logout"><button type="submit">Sign out</button></form>"""
    return page("hexdom: house password", body)
