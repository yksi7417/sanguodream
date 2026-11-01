"""The HTML pages of the site: the sign-in page, the pages of the email link and the members page."""
import html
import time

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
main.wide { width: min(1000px, calc(100vw - 32px)); padding: 24px 0; }
.table { overflow-x: auto; }
table { width: 100%; border-collapse: collapse; font-size: 14px; }
th, td { padding: 8px 10px; border-bottom: 1px solid #2c313d; text-align: left; white-space: nowrap; }
th { color: #9aa0aa; font-weight: 500; }
td.number { text-align: right; }
/* The splash art fills the window (misc/make_splash.py). The panel continues the card of the art:
   it starts under the line of the card and hides the empty bar. --art is the width of the picture. */
body.art { display: block; --art: max(100vw, calc(100vh * 16 / 9));
           background: #08090c url("/splash.jpg") center / cover no-repeat fixed; }
body.art main { position: absolute; left: 50%; transform: translateX(-50%); box-sizing: border-box;
                top: calc(50vh + var(--art) * 0.0609); width: min(calc(var(--art) * 0.4703), calc(100vw - 16px));
                min-height: calc(var(--art) * 0.0805); padding: 16px 24px 8px; background: #0f0f0f;
                border: 1px solid #3a3a42; border-top: 0; }
body.art p { margin-bottom: 14px; }
.signin { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 0 16px; }
"""

# The service worker shows the splash while the machine of the site starts (web/sw.js).
START_WORKER = """<script>
if ("serviceWorker" in navigator) navigator.serviceWorker.register("/sw.js").catch(function () {});
</script>"""


def page(title, body, main_class=""):
    """A page without a class of its own shows the splash art. The members page has a wide table and no art."""
    attribute = f' class="{main_class}"' if main_class else ""
    body_attribute = "" if main_class else ' class="art"'
    worker = "" if main_class else START_WORKER
    return f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{html.escape(title)}</title><style>{STYLE}</style></head>
<body{body_attribute}><main{attribute}>{body}</main>{worker}</body></html>"""


def moment(stamp):
    return time.strftime("%Y-%m-%d %H:%M", time.gmtime(stamp)) if stamp else "-"


def members_page(members):
    """The table of the users for an admin. Each value is escaped, because the user gives the name."""
    rows = []
    for record in members:
        role = "admin" if record.get("admin") else "member"
        cells = [html.escape(str(record.get("name", "?"))), html.escape(str(record.get("provider", "?"))),
                 html.escape(str(record.get("email", "")) or "-"), role]
        row = "".join(f"<td>{cell}</td>" for cell in cells)
        row += f'<td class="number">{int(record.get("saves", 0))}</td>'
        row += f"<td>{moment(record.get('joined'))}</td><td>{moment(record.get('last_login'))}</td>"
        rows.append(f"<tr>{row}</tr>")
    body = f"""<h1>Members</h1>
<p>{len(members)} users signed in. The times are UTC.</p>
<div class="table"><table id="members">
<thead><tr><th>Name</th><th>Sign-in</th><th>Email</th><th>Role</th><th>Saves</th><th>Joined</th><th>Last sign-in</th></tr></thead>
<tbody>{"".join(rows)}</tbody></table></div>
<p><a href="/">Back to the game</a></p>"""
    return page("hexdom: members", body, "wide")


def forbidden_page():
    body = """<h1>Members</h1>
<p class="error">Only an admin can see this page.</p>
<p><a href="/">Back to the game</a></p>"""
    return page("hexdom: members", body)


def login_page(providers, email_on, error=""):
    buttons = "".join(
        f'<a class="button" id="login-{name}" href="/auth/{name}/start">Sign in with {html.escape(provider.label)}</a>'
        for name, provider in providers.items()
    )
    form = """<form method="post" action="/auth/email/request">
<input type="email" name="email" id="email-address" autocomplete="email" placeholder="you@example.com" required>
<button type="submit" id="email-submit">Email me a sign-in link</button>
</form>""" if email_on else ""
    if not buttons and not form:
        buttons = '<p class="error">No sign-in provider is set up on this server.</p>'
    message = f'<p class="error">{html.escape(error)}</p>' if error else ""
    choices = f'<div class="signin"><div>{buttons}</div>{form}</div>'
    return page("hexdom: sign in", f"<p>Sign in to play.</p>{message}{choices}")


def email_sent_page():
    """The same page for each address, so that the page does not show if the address is known."""
    body = """<h1>Look in your email</h1>
<p id="email-sent">We sent a sign-in link to the address. The link works one time, for 15 minutes.</p>
<p><a href="/login">Back to the sign-in</a></p>"""
    return page("hexdom: look in your email", body)


def email_limit_page():
    body = """<h1>Too many requests</h1>
<p class="error">Too many requests came from this network. Try again in one hour.</p>
<p><a href="/login">Back to the sign-in</a></p>"""
    return page("hexdom: too many requests", body)


def email_confirm_page(token, address):
    """A GET of the link shows this page. Only the button uses the token."""
    body = f"""<h1>Sign in</h1>
<p>Sign in to hexdom as {html.escape(address)}.</p>
<form method="post" action="/auth/email/confirm">
<input type="hidden" name="token" value="{html.escape(token)}">
<button type="submit" id="email-confirm">Sign in</button>
</form>"""
    return page("hexdom: sign in", body)


def email_bad_link_page():
    body = """<h1>This link does not work</h1>
<p class="error">A link works one time, for 15 minutes.</p>
<p><a href="/login">Ask for a new link</a></p>"""
    return page("hexdom: sign in", body)
