"""The HTML pages of the site: the sign-in page, the house password page and the members page."""
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
"""


def page(title, body, main_class=""):
    attribute = f' class="{main_class}"' if main_class else ""
    return f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{html.escape(title)}</title><style>{STYLE}</style></head>
<body><main{attribute}>{body}</main></body></html>"""


def moment(stamp):
    return time.strftime("%Y-%m-%d %H:%M", time.gmtime(stamp)) if stamp else "-"


def members_page(members):
    """The table of the users for an admin. Each value is escaped, because the user gives the name."""
    rows = []
    for record in members:
        role = "admin" if record.get("admin") else ("member" if record.get("member") else "no house password")
        cells = [html.escape(str(record.get("name", "?"))), html.escape(str(record.get("provider", "?"))),
                 html.escape(str(record.get("email", "")) or "-"), role]
        row = "".join(f"<td>{cell}</td>" for cell in cells)
        row += f'<td class="number">{int(record.get("saves", 0))}</td>'
        row += f"<td>{moment(record.get('joined'))}</td><td>{moment(record.get('last_login'))}</td>"
        rows.append(f"<tr>{row}</tr>")
    member_count = sum(1 for record in members if record.get("member"))
    body = f"""<h1>Members</h1>
<p>{len(members)} users signed in. {member_count} of them typed the house password. The times are UTC.</p>
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
