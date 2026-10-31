#!/usr/bin/env python3
"""A small in-memory stand-in for civic (liquidgov_http), for the daemon's tests.

Why it exists: the commons-daemon only ever talks to civic over loopback :8080, and
civic cannot be compiled on a GitHub runner (droplet-only dependencies). This stub
implements exactly what the daemon and its tests use, with civic's semantics:

  public-side (what a test drives, as a browser would):
    POST /signup            username, password, email, phone, display_name
                            -> 303 + Set-Cookie: lg_session=<token>   (bad/duplicate -> 200, no cookie)
    POST /friends           who          send a friend request
    POST /friends/accept    who          accept a request from `who`
    POST /friends/unfriend  who
    POST /friends/block     who
    POST /settings/delete   confirm=<own username>   -> deletes the account, 303
  same-box only (plain 404 if X-Forwarded-For is present or the peer isn't loopback):
    GET /internal/session-user            {"username":..}  or  {}
    GET /internal/friends                 {"username":..,"friends":[..]}  or  {}
                                          (mutual friends, minus anyone blocked either way)
    GET /internal/blocked/<owner>         {"blocked":true|false}; FAILS CLOSED: no session,
                                          unknown owner or a malformed name all say true
    GET /internal/user-exists/<name>      {"exists":true|false} for a well-formed name,
                                          {"exists":null} for a malformed one, never false
  GET /_stub/health                       readiness probe for the test harness

Fidelity: the same tests can be pointed at the REAL civic (CIVIC_MODE=real ./run_all.sh)
and must pass on both; that is how this stub is kept honest. State lives in RAM only.
"""
import argparse
import re
import secrets
import sys
import urllib.parse
from http.server import BaseHTTPRequestHandler, HTTPServer

USERNAME_RE = re.compile(r"^[A-Za-z0-9._-]{3,32}$")


class State:
    def __init__(self):
        self.users = {}         # lower(username) -> canonical username
        self.sessions = {}      # token -> canonical username
        self.requests = set()   # (from, to) pending friend requests
        self.friends = set()    # frozenset({a, b}) accepted friendships
        self.blocks = set()     # (blocker, blocked)

    def find(self, name):
        return self.users.get(name.lower())

    def is_blocked(self, blocker, blocked):
        return (blocker, blocked) in self.blocks

    def friends_of(self, user):
        out = []
        for pair in self.friends:
            if user in pair:
                (other,) = pair - {user}
                out.append(other)
        return sorted(out)

    def delete(self, user):
        self.users.pop(user.lower(), None)
        for tok in [t for t, u in self.sessions.items() if u == user]:
            del self.sessions[tok]
        self.requests = {r for r in self.requests if user not in r}
        self.friends = {p for p in self.friends if user not in p}
        self.blocks = {b for b in self.blocks if user not in b}


STATE = State()


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):  # quiet
        pass

    # ---- helpers ------------------------------------------------------------
    def send(self, status, body=b"", ctype="application/json", headers=None):
        if isinstance(body, str):
            body = body.encode()
        self.send_response(status)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        for k, v in (headers or {}).items():
            self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)
        self.close_connection = True

    def same_box(self):
        return "X-Forwarded-For" not in self.headers and self.client_address[0] in ("127.0.0.1", "::1")

    def session_user(self):
        cookie = self.headers.get("Cookie", "")
        m = re.search(r"lg_session=([^;\s]+)", cookie)
        return STATE.sessions.get(m.group(1)) if m else None

    def form(self):
        n = int(self.headers.get("Content-Length") or 0)
        raw = self.rfile.read(n).decode() if n else ""
        return {k: v[0] for k, v in urllib.parse.parse_qs(raw).items()}

    def redirect(self, extra=None):
        self.send(303, b"", "text/plain", dict({"Location": "/"}, **(extra or {})))

    # ---- routes -------------------------------------------------------------
    def do_GET(self):
        path = urllib.parse.unquote(self.path.split("?", 1)[0])
        if path == "/_stub/health":
            return self.send(200, '{"ok":true}')
        if path.startswith("/internal/"):
            if not self.same_box():
                return self.send(404, "not found\n", "text/plain")
            user = self.session_user()
            no_store = {"Cache-Control": "no-store"}
            if path == "/internal/session-user":
                return self.send(200, '{"username":"%s"}' % user if user else "{}", headers=no_store)
            if path == "/internal/friends":
                if not user:
                    return self.send(200, "{}", headers=no_store)
                fr = [f for f in STATE.friends_of(user)
                      if not STATE.is_blocked(user, f) and not STATE.is_blocked(f, user)]
                return self.send(200, '{"username":"%s","friends":[%s]}' % (user, ",".join('"%s"' % f for f in fr)),
                                 headers=no_store)
            if path.startswith("/internal/blocked/"):
                owner_name = path[len("/internal/blocked/"):]
                blocked = True  # fail closed
                if user and USERNAME_RE.match(owner_name):
                    owner = STATE.find(owner_name)
                    if owner:
                        blocked = STATE.is_blocked(user, owner) or STATE.is_blocked(owner, user)
                return self.send(200, '{"blocked":%s}' % ("true" if blocked else "false"), headers=no_store)
            if path.startswith("/internal/user-exists/"):
                who = path[len("/internal/user-exists/"):]
                if USERNAME_RE.match(who):
                    body = '{"exists":true}' if STATE.find(who) else '{"exists":false}'
                else:
                    body = '{"exists":null}'
                return self.send(200, body, headers=no_store)
        return self.send(404, "not found\n", "text/plain")

    def do_POST(self):
        path = self.path.split("?", 1)[0]
        f = self.form()
        me = self.session_user()
        if path == "/signup":
            name = f.get("username", "")
            if not USERNAME_RE.match(name) or STATE.find(name):
                return self.send(200, "<html>signup failed</html>", "text/html")
            STATE.users[name.lower()] = name
            tok = secrets.token_hex(16)
            STATE.sessions[tok] = name
            return self.redirect({"Set-Cookie": "lg_session=%s; Path=/; HttpOnly" % tok})
        if not me:
            return self.send(401, "no\n", "text/plain")
        who = STATE.find(f.get("who", ""))
        if path == "/friends":
            if who and who != me:
                STATE.requests.add((me, who))
            return self.redirect()
        if path == "/friends/accept":
            if who and (who, me) in STATE.requests:
                STATE.requests.discard((who, me))
                STATE.friends.add(frozenset({me, who}))
            return self.redirect()
        if path == "/friends/unfriend":
            if who:
                STATE.friends.discard(frozenset({me, who}))
            return self.redirect()
        if path == "/friends/block":
            if who and who != me:
                STATE.blocks.add((me, who))
            return self.redirect()
        if path == "/settings/delete":
            if f.get("confirm", "").strip() != me:
                return self.redirect()
            STATE.delete(me)
            return self.redirect({"Set-Cookie": "lg_session=; Max-Age=0; Path=/"})
        return self.send(404, "not found\n", "text/plain")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8080)
    args = ap.parse_args()
    srv = HTTPServer(("127.0.0.1", args.port), Handler)
    print("civic-stub listening on 127.0.0.1:%d" % args.port, file=sys.stderr, flush=True)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
