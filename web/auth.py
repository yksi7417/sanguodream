"""The sign-in of the site: Google, Apple and Facebook (docs/WEB.md).

The flow:
1. /auth/<provider>/start puts a signed state cookie and sends the browser
   to the provider.
2. The provider sends the browser back to /auth/<provider>/callback with a
   code. The server gets the identity of the user from the code.
3. The server gives a signed session cookie. A new user then types the
   house password one time (server.py).

A provider is on only if its secrets are set. The "dev" provider signs in
with a name and no password. It is for the tests on a local computer: it is
on only if AUTH_DEV_LOGIN is 1, and never on fly.io.

The cookies are JSON, base64url, and an HMAC-SHA256 with SESSION_SECRET.
"""
import asyncio
import base64
import hashlib
import hmac
import json
import os
import re
import secrets
import time
import urllib.parse

import jwt

SESSION_COOKIE = "hx_session"
STATE_COOKIE = "hx_oauth"
SESSION_SECONDS = 30 * 24 * 3600
STATE_SECONDS = 600
FACEBOOK_GRAPH = "https://graph.facebook.com/" + os.environ.get("FACEBOOK_GRAPH_VERSION", "v23.0")
FACEBOOK_DIALOG = "https://www.facebook.com/" + os.environ.get("FACEBOOK_GRAPH_VERSION", "v23.0")
GOOGLE_ISSUERS = ("https://accounts.google.com", "accounts.google.com")
APPLE_ISSUER = "https://appleid.apple.com"
NAME_MAX_LENGTH = 60


class AuthError(Exception):
    pass


def b64url(data):
    return base64.urlsafe_b64encode(data).rstrip(b"=").decode("ascii")


def b64url_decode(text):
    return base64.urlsafe_b64decode(text + "=" * (-len(text) % 4))


def sign(secret, payload):
    body = b64url(json.dumps(payload, separators=(",", ":")).encode("utf-8"))
    mac = hmac.new(secret, body.encode("ascii"), hashlib.sha256).digest()
    return body + "." + b64url(mac)


def verify(secret, token):
    """Give the payload of a signed token, or None if it is bad or old."""
    if not token or token.count(".") != 1:
        return None
    body, mac = token.split(".")
    expected = hmac.new(secret, body.encode("ascii"), hashlib.sha256).digest()
    try:
        if not hmac.compare_digest(b64url_decode(mac), expected):
            return None
        payload = json.loads(b64url_decode(body))
    except ValueError:
        return None
    if not isinstance(payload, dict) or payload.get("exp", 0) < time.time():
        return None
    return payload


def user_id(provider, subject):
    return hashlib.sha256(f"{provider}:{subject}".encode("utf-8")).hexdigest()[:24]


def clean_name(name, fallback):
    name = " ".join(str(name or "").split())[:NAME_MAX_LENGTH]
    return name or fallback


def pkce_pair():
    verifier = secrets.token_urlsafe(48)
    challenge = b64url(hashlib.sha256(verifier.encode("ascii")).digest())
    return verifier, challenge


async def read_json(response):
    data = await response.json(content_type=None)
    if response.status != 200 or not isinstance(data, dict):
        raise AuthError(f"the provider answered {response.status}")
    return data


class Google:
    name = "google"
    label = "Google"
    jwks = jwt.PyJWKClient("https://www.googleapis.com/oauth2/v3/certs")

    def __init__(self):
        self.client_id = os.environ.get("GOOGLE_CLIENT_ID", "")
        self.client_secret = os.environ.get("GOOGLE_CLIENT_SECRET", "")
        self.enabled = bool(self.client_id and self.client_secret)

    def authorize_url(self, redirect_uri, state):
        query = {
            "client_id": self.client_id,
            "redirect_uri": redirect_uri,
            "response_type": "code",
            "scope": "openid email profile",
            "state": state["state"],
            "nonce": state["nonce"],
            "code_challenge": state["challenge"],
            "code_challenge_method": "S256",
            "prompt": "select_account",
        }
        return "https://accounts.google.com/o/oauth2/v2/auth?" + urllib.parse.urlencode(query)

    async def identity(self, http, params, state, redirect_uri):
        form = {
            "code": params.get("code", ""),
            "client_id": self.client_id,
            "client_secret": self.client_secret,
            "redirect_uri": redirect_uri,
            "grant_type": "authorization_code",
            "code_verifier": state["verifier"],
        }
        async with http.post("https://oauth2.googleapis.com/token", data=form) as response:
            tokens = await read_json(response)
        claims = await decode_id_token(self.jwks, tokens.get("id_token", ""), self.client_id, GOOGLE_ISSUERS, state["nonce"])
        name = clean_name(claims.get("name"), claims.get("email", "Google user"))
        return claims["sub"], name, claims.get("email", ""), email_verified(claims)


PEM_LINE_LENGTH = 64


def apple_pem(text):
    """Give the .p8 key of Apple as PEM text.

    A secret often loses the line breaks of the .p8 file, or the BEGIN and
    END lines. This function accepts the full file, the file with "\\n" in
    place of the line breaks, or only the base64 body."""
    text = text.replace("\\n", "\n").strip()
    if not text or "-----BEGIN" in text and "\n" in text:
        return text
    body = re.sub(r"-----[^-]+-----|\s", "", text)
    lines = [body[start:start + PEM_LINE_LENGTH] for start in range(0, len(body), PEM_LINE_LENGTH)]
    return "-----BEGIN PRIVATE KEY-----\n" + "\n".join(lines) + "\n-----END PRIVATE KEY-----\n"


class Apple:
    name = "apple"
    label = "Apple"
    jwks = jwt.PyJWKClient("https://appleid.apple.com/auth/keys")

    def __init__(self):
        self.client_id = os.environ.get("APPLE_CLIENT_ID", "")      # the Services ID
        self.team_id = os.environ.get("APPLE_TEAM_ID", "")
        self.key_id = os.environ.get("APPLE_KEY_ID", "")
        self.private_key = apple_pem(os.environ.get("APPLE_PRIVATE_KEY", ""))
        self.enabled = bool(self.client_id and self.team_id and self.key_id and self.private_key)

    def authorize_url(self, redirect_uri, state):
        query = {
            "client_id": self.client_id,
            "redirect_uri": redirect_uri,
            "response_type": "code",
            "response_mode": "form_post",   # Apple sends the name only with form_post.
            "scope": "name email",
            "state": state["state"],
            "nonce": state["nonce"],
        }
        return "https://appleid.apple.com/auth/authorize?" + urllib.parse.urlencode(query)

    def client_secret(self):
        now = int(time.time())
        claims = {"iss": self.team_id, "iat": now, "exp": now + 300, "aud": APPLE_ISSUER, "sub": self.client_id}
        return jwt.encode(claims, self.private_key, algorithm="ES256", headers={"kid": self.key_id})

    async def identity(self, http, params, state, redirect_uri):
        form = {
            "client_id": self.client_id,
            "client_secret": self.client_secret(),
            "code": params.get("code", ""),
            "grant_type": "authorization_code",
            "redirect_uri": redirect_uri,
        }
        async with http.post("https://appleid.apple.com/auth/token", data=form) as response:
            tokens = await read_json(response)
        claims = await decode_id_token(self.jwks, tokens.get("id_token", ""), self.client_id, (APPLE_ISSUER,), state["nonce"])
        # Apple sends the name of the user only at the first sign-in.
        name = ""
        try:
            user = json.loads(params.get("user", "") or "{}")
            parts = user.get("name", {})
            name = f"{parts.get('firstName', '')} {parts.get('lastName', '')}"
        except (ValueError, AttributeError):
            pass
        return claims["sub"], clean_name(name, ""), claims.get("email", ""), email_verified(claims)


class Facebook:
    name = "facebook"
    label = "Facebook"

    def __init__(self):
        self.app_id = os.environ.get("FACEBOOK_APP_ID", "")
        self.app_secret = os.environ.get("FACEBOOK_APP_SECRET", "")
        self.enabled = bool(self.app_id and self.app_secret)

    def authorize_url(self, redirect_uri, state):
        query = {
            "client_id": self.app_id,
            "redirect_uri": redirect_uri,
            "response_type": "code",
            "scope": "public_profile,email",
            "state": state["state"],
        }
        return FACEBOOK_DIALOG + "/dialog/oauth?" + urllib.parse.urlencode(query)

    async def identity(self, http, params, state, redirect_uri):
        query = {
            "client_id": self.app_id,
            "client_secret": self.app_secret,
            "redirect_uri": redirect_uri,
            "code": params.get("code", ""),
        }
        async with http.get(FACEBOOK_GRAPH + "/oauth/access_token", params=query) as response:
            tokens = await read_json(response)
        token = tokens.get("access_token", "")
        proof = hmac.new(self.app_secret.encode("utf-8"), token.encode("utf-8"), hashlib.sha256).hexdigest()
        query = {"fields": "id,name,email", "access_token": token, "appsecret_proof": proof}
        async with http.get(FACEBOOK_GRAPH + "/me", params=query) as response:
            me = await read_json(response)
        if not me.get("id"):
            raise AuthError("Facebook gave no user id")
        # The Graph API does not say if the email is verified, so it never makes an admin.
        return me["id"], clean_name(me.get("name"), "Facebook user"), me.get("email", ""), False


class Dev:
    """Sign-in with a name only, for the tests on a local computer."""
    name = "dev"
    label = "Test user"

    def __init__(self):
        on_fly = bool(os.environ.get("FLY_APP_NAME"))
        self.enabled = os.environ.get("AUTH_DEV_LOGIN") == "1" and not on_fly

    def authorize_url(self, redirect_uri, state):
        return redirect_uri + "?" + urllib.parse.urlencode({"state": state["state"], "code": "dev"})

    async def identity(self, http, params, state, redirect_uri):
        name = clean_name(state.get("dev_name"), "Tester")
        # The tests give a test user the admin role through ADMIN_EMAILS=<name>@dev.test.
        email = re.sub(r"[^a-z0-9]", "", name.lower()) + "@dev.test"
        return name, name, email, True


async def decode_id_token(jwks, token, audience, issuers, nonce):
    if not token:
        raise AuthError("the provider gave no ID token")
    try:
        key = await asyncio.to_thread(jwks.get_signing_key_from_jwt, token)
        claims = jwt.decode(token, key.key, algorithms=["RS256"], audience=audience, options={"verify_iss": False})
    except jwt.PyJWTError as error:
        raise AuthError(f"the ID token is not valid: {error}") from error
    if claims.get("iss") not in issuers:
        raise AuthError("the ID token has an unknown issuer")
    if not hmac.compare_digest(str(claims.get("nonce", "")), nonce):
        raise AuthError("the ID token has a bad nonce")
    if not claims.get("sub"):
        raise AuthError("the ID token has no subject")
    return claims


def email_verified(claims):
    # Google sends a boolean. Apple can send the string "true".
    return claims.get("email_verified") in (True, "true")


def admin_emails():
    """Give the set of the ADMIN_EMAILS setting: email addresses, with commas between them."""
    return frozenset(email.strip().lower() for email in os.environ.get("ADMIN_EMAILS", "").split(",") if email.strip())


def is_admin(user, admins):
    """A user is an admin if the provider verified the email and the email is in ADMIN_EMAILS."""
    email = str(user.get("email", "")).lower()
    return bool(email) and user.get("email_verified") is True and email in admins


def load_providers():
    """Give the providers that are on, in the order of the sign-in page."""
    providers = [Google(), Apple(), Facebook(), Dev()]
    return {provider.name: provider for provider in providers if provider.enabled}


def new_state(provider, next_path, dev_name=""):
    verifier, challenge = pkce_pair()
    return {
        "provider": provider,
        "state": secrets.token_urlsafe(24),
        "nonce": secrets.token_urlsafe(24),
        "verifier": verifier,
        "challenge": challenge,
        "next": next_path,
        "dev_name": dev_name,
        "exp": int(time.time()) + STATE_SECONDS,
    }
