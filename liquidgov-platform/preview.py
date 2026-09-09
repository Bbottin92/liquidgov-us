# Self-contained LiquidGov preview. Double-click start.bat.
from http.server import BaseHTTPRequestHandler, HTTPServer
from html import escape
from urllib.parse import parse_qs, unquote
import hashlib
import json
import os
import secrets
import time

PORT = 8080
DIR = os.path.dirname(os.path.abspath(__file__))
STORE = os.path.join(DIR, "accounts.json")

CSS = """
body{font:16px/1.45 system-ui,sans-serif;max-width:46rem;margin:2rem auto;padding:0 1rem;
background:#0f1419;color:#e7ecf1}a{color:#7dbeff}h1{font-size:1.5rem}
.card{background:#1a2330;padding:1.2rem 1.4rem;border-radius:12px;margin:1rem 0}
input,textarea,select{width:100%;box-sizing:border-box;padding:.5rem;margin:.3rem 0 .8rem;
border-radius:8px;border:1px solid #334;background:#0f1419;color:#e7ecf1}
button{background:#2f6fed;color:#fff;border:0;padding:.5rem 1rem;border-radius:8px;cursor:pointer;margin:.15rem}
.warn{color:#ffcc80;font-size:.92rem}nav a{margin-right:.7rem;font-size:.95rem}
.muted{color:#9aa7b5;font-size:.9rem}.msg{border-top:1px solid #334;padding:.45rem 0}
"""


def load():
    d = json.load(open(STORE, encoding="utf-8")) if os.path.exists(STORE) else {}
    for k, v in (("users", {}), ("next_id", 1), ("friends", []), ("requests", []),
                 ("messages", []), ("notes", []), ("blocks", []), ("groups", []),
                 ("world", []), ("themes", []), ("jukebox", []), ("files", []),
                 ("board", []), ("polls", []), ("events", []), ("shop", []), ("reports", []),
                 ("wiki", {}), ("bookmarks", []), ("motions", []), ("rooms", {}),
                 ("invites", []), ("places", [])):
        d.setdefault(k, v)
    return d


def save(data):
    json.dump(data, open(STORE, "w", encoding="utf-8"), indent=2)


DATA = load()
SESS = {}


def page(title, body):
    return ("<!doctype html><html lang='en'><head><meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            f"<title>{title}</title><style>{CSS}</style></head><body>{body}</body></html>").encode()


def cookie(headers):
    raw = headers.get("Cookie") or ""
    for part in raw.split(";"):
        part = part.strip()
        if part.startswith("lg_session="):
            return part.split("=", 1)[1]
    return ""


def nav(u):
    if not u:
        return "<nav><a href='/'>Home</a> <a href='/signup'>Sign up</a> <a href='/login'>Log in</a></nav>"
    n = sum(1 for x in DATA["notes"] if x.get("to") == u and not x.get("read"))
    b = f" ({n})" if n else ""
    return ("<nav><a href='/'>Home</a> <a href='/me'>Profile</a> <a href='/friends'>Friends</a> "
            "<a href='/mail'>Messages</a> "
            f"<a href='/notes'>Notes{b}</a> <a href='/settings'>Settings</a> "
            "<details style='display:inline'><summary style='cursor:pointer;color:#7dbeff'>More</summary> "
            "<a href='/search'>Search</a> <a href='/groups'>Groups</a> <a href='/world'>World</a> "
            "<a href='/gallery'>Gallery</a> <a href='/jukebox'>Jukebox</a> <a href='/files'>Files</a> "
            "<a href='/board'>Board</a> <a href='/polls'>Polls</a> <a href='/events'>Events</a> "
            "<a href='/shop'>Shop</a> <a href='/wiki'>Wiki</a> <a href='/civic'>Civic</a> "
            "<a href='/rooms'>Rooms</a> <a href='/saved'>Saved</a> <a href='/places'>Places</a> "
            "<a href='/online'>Online</a> <a href='/stats'>Stats</a> <a href='/mod'>Reports</a> "
            "<a href='/export'>Export</a> <a href='/help'>Help</a></details> "
            "<a href='/logout'>Log out</a></nav>")


def are_friends(a, b):
    return [a, b] in DATA["friends"] or [b, a] in DATA["friends"]


def blocked(a, b):
    return [a, b] in DATA["blocks"] or [b, a] in DATA["blocks"]


def notify(to, text):
    DATA["notes"].append({"to": to, "text": text, "ts": int(time.time()), "read": False})


class H(BaseHTTPRequestHandler):
    def log_message(self, *a):
        return

    def user(self):
        return SESS.get(cookie(self.headers))

    def rec(self, name):
        return DATA["users"].get(name)

    def send_html(self, body):
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def redir(self, loc, extra=None):
        self.send_response(303)
        self.send_header("Location", loc)
        if extra:
            for k, v in extra:
                self.send_header(k, v)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def form(self):
        n = int(self.headers.get("Content-Length") or 0)
        return {k: (v[0] if v else "") for k, v in parse_qs(self.rfile.read(n).decode("utf-8", "replace")).items()}

    def do_GET(self):
        raw = self.path.split("?", 1)
        path, qs = raw[0], raw[1] if len(raw) > 1 else ""
        q = {k: (v[0] if v else "") for k, v in parse_qs(qs).items()}
        u = self.user()

        if path == "/":
            if u:
                self.send_html(page("LiquidGov.US", nav(u) +
                    f"<h1>You're in.</h1><div class='card'><p>Signed in as <strong>{escape(u)}</strong>.</p>"
                    "<p class='muted'>Preview: friends, mail, groups, search, settings, blocks.</p></div>"))
            else:
                self.send_html(page("LiquidGov.US", nav(None) +
                    "<h1>LiquidGov.US</h1><div class='card'>"
                    "<p><strong>This is genesis, not a government.</strong></p>"
                    "<p>Voluntary association and working software. Nobody here is your city, "
                    "state, or federal government. Website control is temporary; a working "
                    "election among enrolled citizens is meant to end it. Real authority stays "
                    "a later landslide — not this page.</p>"
                    "<p class='warn'>Genesis operators run the machines and say so in public.</p>"
                    "<p><a href='/signup'>Create an account</a> · <a href='/login'>Log in</a></p></div>"))
            return
        if path == "/signup":
            self.send_html(page("Sign up", nav(u) +
                "<div class='card'><h1>Create account</h1><form method='POST' action='/signup'>"
                "<label>Username</label><input name='username' required>"
                "<label>Password (8+)</label><input type='password' name='password' required>"
                "<label>Phone</label><input name='phone' required><button>Continue</button></form></div>"))
            return
        if path == "/login":
            self.send_html(page("Log in", nav(u) +
                "<div class='card'><h1>Log in</h1><form method='POST' action='/login'>"
                "<label>Username</label><input name='username' required>"
                "<label>Password</label><input type='password' name='password' required>"
                "<button>Continue</button></form></div>"))
            return
        if path == "/logout":
            tok = cookie(self.headers)
            SESS.pop(tok, None)
            self.redir("/", [("Set-Cookie", "lg_session=; Path=/; Max-Age=0")])
            return
        if path.startswith("/u/"):
            name = unquote(path[3:])
            r = self.rec(name)
            if not r:
                self.send_html(page("?", nav(u) + "<p>No such account.</p>"))
                return
            color = r.get("color") or "#1a2330"
            self.send_html(page(name, nav(u) +
                f"<div class='card' style='border-left:8px solid {escape(color)}'><h1>{escape(name)}</h1>"
                f"<p>{escape(r.get('blurb') or 'No blurb yet.')}</p>"
                + (f"<p class='warn'>{escape(r.get('pin',''))}</p>" if r.get("pin") else "")
                + (f"<p><a href='/mail?with={escape(name)}'>Message</a></p>" if u and u != name else "")
                + "</div>"))
            return
        if not u:
            self.redir("/login")
            return
        if path == "/me":
            r = self.rec(u) or {}
            inner = r.get("inner") or []
            self.send_html(page("Profile", nav(u) +
                f"<div class='card'><h1>{escape(r.get('face','🙂'))} {escape(u)}</h1>"
                + (f"<p class='muted'>{escape(r.get('status',''))}</p>" if r.get("status") else "")
                + f"<p class='muted'>#{r.get('id')} · {escape(str(r.get('phone','')))}</p>"
                f"<p>{escape(r.get('blurb') or 'No blurb yet.')}</p>"
                + (f"<p class='warn'>Pinned: {escape(r.get('pin',''))}</p>" if r.get("pin") else "")
                + "<form method='POST' action='/me'><label>Blurb</label>"
                f"<textarea name='blurb' rows='3'>{escape(r.get('blurb',''))}</textarea>"
                "<label>Pinned line</label>"
                f"<input name='pin' value='{escape(r.get('pin',''))}'>"
                "<label>Status</label>"
                f"<input name='status' value='{escape(r.get('status',''))}'>"
                "<label>Face</label>"
                f"<input name='face' value='{escape(r.get('face','🙂'))}' maxlength='4'>"
                "<label>Profile color</label>"
                f"<input name='color' value='{escape(r.get('color','#2f6fed'))}'>"
                "<button>Save profile</button></form>"
                "<h2>Inner circle</h2><p class='muted'>Max 12. Friends only.</p><ul>"
                + "".join(f"<li>{escape(n)}</li>" for n in inner) + "</ul>"
                "<form method='POST' action='/inner'><input name='who' placeholder='friend username'>"
                "<button>Add to inner circle</button></form></div>"))
            return
        if path == "/friends":
            incoming = [x for x in DATA["requests"] if x["to"] == u]
            outgoing = [x for x in DATA["requests"] if x["from"] == u]
            names = [b if a == u else a for a, b in DATA["friends"] if u in (a, b)]
            body = nav(u) + "<div class='card'><h1>Friends</h1>"
            body += "<form method='POST' action='/friends'><input name='who' placeholder='exact username' required>"
            body += "<button>Send request</button></form><h2>Friends</h2><ul>"
            body += "".join(
                f"<li><a href='/u/{escape(n)}'>{escape(n)}</a> · <a href='/mail?with={escape(n)}'>Message</a> "
                f"<form style='display:inline' method='POST' action='/block'><input type='hidden' name='who' value='{escape(n)}'>"
                f"<button>Block</button></form></li>" for n in names) or "<li class='muted'>None</li>"
            body += "</ul><h2>Incoming</h2><ul>"
            body += "".join(
                f"<li>{escape(req['from'])} <form style='display:inline' method='POST' action='/friends/accept'>"
                f"<input type='hidden' name='who' value='{escape(req['from'])}'><button>Accept</button></form></li>"
                for req in incoming) or "<li class='muted'>None</li>"
            body += "</ul><h2>Outgoing</h2><ul>"
            if not outgoing:
                body += "<li class='muted'>None</li>"
            for x in outgoing:
                body += (f"<li>{escape(x['to'])} <form style='display:inline' method='POST' action='/friends/accept'>"
                         f"<input type='hidden' name='who' value='{escape(x['from'])}'>"
                         f"<input type='hidden' name='as_user' value='{escape(x['to'])}'>"
                         f"<button>Accept as {escape(x['to'])}</button></form></li>")
            body += "</ul></div>"
            self.send_html(page("Friends", body))
            return
        if path == "/search":
            term = q.get("q", "").strip()
            hits = []
            if term:
                for name, rec in DATA["users"].items():
                    if blocked(u, name):
                        continue
                    if name.lower() != term.lower():
                        continue
                    if name == u or are_friends(u, name) or rec.get("discover", "exact") != "hidden":
                        hits.append(name)
            body = nav(u) + "<div class='card'><h1>Search</h1><form method='GET' action='/search'>"
            body += f"<input name='q' value='{escape(term)}'><button>Search</button></form><ul>"
            body += "".join(f"<li><a href='/u/{escape(n)}'>{escape(n)}</a></li>" for n in hits) or "<li class='muted'>No match (or hidden).</li>"
            body += "</ul></div>"
            self.send_html(page("Search", body))
            return
        if path == "/mail":
            other = q.get("with", "")
            friends = [b if a == u else a for a, b in DATA["friends"] if u in (a, b) and not blocked(u, a if a != u else b)]
            body = nav(u) + "<div class='card'><h1>Messages</h1><ul>"
            body += "".join(f"<li><a href='/mail?with={escape(n)}'>{escape(n)}</a></li>" for n in friends) or "<li class='muted'>Friend someone first.</li>"
            body += "</ul>"
            if other and not blocked(u, other):
                thread = [m for m in DATA["messages"] if {m["from"], m["to"]} == {u, other}]
                body += f"<h2>{escape(other)}</h2>"
                for m in thread[-50:]:
                    body += f"<div class='msg'><strong>{escape(m['from'])}</strong> {escape(m['text'])}</div>"
                body += f"<form method='POST' action='/mail'><input type='hidden' name='to' value='{escape(other)}'>"
                body += "<input name='text' required><button>Send</button></form>"
            body += "</div>"
            self.send_html(page("Messages", body))
            return
        if path == "/groups":
            mine = [g for g in DATA["groups"] if u in g.get("members", [])]
            body = nav(u) + "<div class='card'><h1>Groups</h1>"
            body += "<form method='POST' action='/groups'><input name='name' placeholder='group name' required>"
            body += "<button>Create group</button></form><ul>"
            for g in mine:
                body += f"<li><a href='/g/{g['id']}'>{escape(g['name'])}</a> — {', '.join(escape(m) for m in g['members'])}</li>"
            body += "</ul></div>"
            self.send_html(page("Groups", body))
            return
        if path.startswith("/g/"):
            gid = path[3:]
            g = next((x for x in DATA["groups"] if str(x["id"]) == gid), None)
            if not g or u not in g["members"]:
                self.send_html(page("Group", nav(u) + "<p>Not in this group.</p>"))
                return
            body = nav(u) + f"<div class='card'><h1>{escape(g['name'])}</h1><p>Members: {', '.join(escape(m) for m in g['members'])}</p>"
            body += "<form method='POST' action='/groups/add'><input type='hidden' name='id' value='%s'>" % escape(gid)
            body += "<input name='who' placeholder='friend to add'><button>Add friend</button></form></div>"
            self.send_html(page(g["name"], body))
            return
        if path == "/world":
            marks = DATA["world"][-80:]
            dots = "".join(
                f"<div style='position:absolute;left:{m['x']}%;top:{m['y']}%;transform:translate(-50%,-50%)'>"
                f"{escape(m['who'][:8])}</div>" for m in marks)
            body = nav(u) + "<div class='card'><h1>World</h1><p class='muted'>Click the room to drop a mark. Preview only — not the C++ canvas.</p>"
            body += "<form method='POST' action='/world' id='wf'>"
            body += "<input type='hidden' name='x' id='x'><input type='hidden' name='y' id='y'></form>"
            body += "<div id='room' style='position:relative;height:280px;background:#0b1018;border-radius:12px;border:1px solid #334;cursor:crosshair'>"
            body += dots + "</div><script>"
            body += "document.getElementById('room').onclick=function(e){var r=this.getBoundingClientRect();"
            body += "document.getElementById('x').value=((e.clientX-r.left)/r.width*100).toFixed(1);"
            body += "document.getElementById('y').value=((e.clientY-r.top)/r.height*100).toFixed(1);"
            body += "document.getElementById('wf').submit();}</script></div>"
            self.send_html(page("World", body))
            return
        if path == "/gallery":
            body = nav(u) + "<div class='card'><h1>Theme gallery</h1>"
            body += "<form method='POST' action='/gallery'><input name='name' placeholder='theme name' required>"
            body += "<input name='color' placeholder='#2f6fed' required><button>Publish</button></form>"
            for t in DATA["themes"]:
                body += (f"<div class='msg'><span style='display:inline-block;width:1rem;height:1rem;background:{escape(t['color'])}'></span> "
                         f"{escape(t['name'])} by {escape(t['author'])} "
                         f"<form style='display:inline' method='POST' action='/gallery/install'>"
                         f"<input type='hidden' name='color' value='{escape(t['color'])}'><button>Install color</button></form></div>")
            body += "</div>"
            self.send_html(page("Gallery", body))
            return
        if path == "/jukebox":
            body = nav(u) + "<div class='card'><h1>Jukebox</h1><p class='muted'>Queue a YouTube URL. Preview does not embed autoplay.</p>"
            body += "<form method='POST' action='/jukebox'><input name='url' placeholder='https://www.youtube.com/watch?v=...' required>"
            body += "<button>Add to queue</button></form><ol>"
            for t in DATA["jukebox"][-20:]:
                body += f"<li>{escape(t['who'])}: <a href='{escape(t['url'])}'>{escape(t['url'])}</a></li>"
            body += "</ol></div>"
            self.send_html(page("Jukebox", body))
            return
        if path == "/files":
            mine = [x for x in DATA["files"] if x["from"] == u or x["to"] == u]
            friends = [b if a == u else a for a, b in DATA["friends"] if u in (a, b)]
            body = nav(u) + "<div class='card'><h1>Files</h1><p class='muted'>Text-only stand-in. Not the C++ allow-list pipeline.</p>"
            body += "<form method='POST' action='/files'><select name='to'>"
            body += "".join(f"<option>{escape(n)}</option>" for n in friends)
            body += "</select><input name='name' placeholder='filename.txt' required>"
            body += "<textarea name='body' rows='3' placeholder='file contents'></textarea><button>Send</button></form>"
            for x in reversed(mine[-20:]):
                body += f"<div class='msg'><strong>{escape(x['from'])} → {escape(x['to'])}</strong> {escape(x['name'])}<pre>{escape(x['body'][:500])}</pre></div>"
            body += "</div>"
            self.send_html(page("Files", body))
            return
        if path == "/board":
            body = nav(u) + "<div class='card'><h1>Board</h1><form method='POST' action='/board'>"
            body += "<textarea name='text' rows='3' required placeholder='Public post'></textarea><button>Post</button></form>"
            for p in reversed(DATA["board"][-40:]):
                body += (f"<div class='msg'><strong><a href='/u/{escape(p['who'])}'>{escape(p['who'])}</a></strong> "
                         f"{escape(p['text'])} · {p.get('up',0)} "
                         f"<form style='display:inline' method='POST' action='/board/up'>"
                         f"<input type='hidden' name='id' value='{p.get('id',0)}'><button>+1</button></form>"
                         f"<form style='display:inline' method='POST' action='/saved'>"
                         f"<input type='hidden' name='ref' value='board:{p.get('id',0)}'><button>Save</button></form>"
                         f"<form style='display:inline' method='POST' action='/report'>"
                         f"<input type='hidden' name='what' value='board:{p.get('id',0)}'><button>Report</button></form></div>")
            body += "</div>"
            self.send_html(page("Board", body))
            return
        if path == "/polls":
            body = nav(u) + "<div class='card'><h1>Polls</h1><form method='POST' action='/polls'>"
            body += "<input name='q' placeholder='Question' required>"
            body += "<input name='opts' placeholder='yes,no,maybe' required><button>Create</button></form>"
            for p in reversed(DATA["polls"][-20:]):
                body += f"<div class='msg'><strong>{escape(p['q'])}</strong> — {escape(p['who'])}<ul>"
                for i, opt in enumerate(p["opts"]):
                    c = p.get("votes", {}).get(opt, 0)
                    body += (f"<li>{escape(opt)} ({c}) <form style='display:inline' method='POST' action='/polls/vote'>"
                             f"<input type='hidden' name='id' value='{p['id']}'>"
                             f"<input type='hidden' name='opt' value='{escape(opt)}'><button>Vote</button></form></li>")
                body += "</ul></div>"
            body += "</div>"
            self.send_html(page("Polls", body))
            return
        if path == "/events":
            body = nav(u) + "<div class='card'><h1>Events</h1><form method='POST' action='/events'>"
            body += "<input name='title' placeholder='Title' required>"
            body += "<input name='when' placeholder='When' required><button>Add</button></form><ul>"
            for e in DATA["events"][-30:]:
                body += f"<li><strong>{escape(e['title'])}</strong> — {escape(e['when'])} ({escape(e['who'])})</li>"
            body += "</ul></div>"
            self.send_html(page("Events", body))
            return
        if path == "/shop":
            body = nav(u) + "<div class='card'><h1>Shop</h1><p class='muted'>Listings only. No payments.</p>"
            body += "<form method='POST' action='/shop'><input name='title' placeholder='Item' required>"
            body += "<input name='price' placeholder='Price note'><button>List</button></form>"
            for s in reversed(DATA["shop"][-30:]):
                body += f"<div class='msg'><strong>{escape(s['title'])}</strong> {escape(s.get('price',''))} — {escape(s['who'])}</div>"
            body += "</div>"
            self.send_html(page("Shop", body))
            return
        if path == "/wiki":
            pages = sorted(DATA["wiki"].keys())
            title = q.get("p", pages[0] if pages else "Main")
            text = DATA["wiki"].get(title, "Empty page. Edit below.")
            body = nav(u) + "<div class='card'><h1>Wiki</h1><form method='GET' action='/wiki'>"
            body += f"<input name='p' value='{escape(title)}'><button>Open</button></form>"
            body += "<p>" + " · ".join(f"<a href='/wiki?p={escape(p)}'>{escape(p)}</a>" for p in pages) + "</p>"
            body += f"<h2>{escape(title)}</h2><p>{escape(text)}</p>"
            body += f"<form method='POST' action='/wiki'><input type='hidden' name='p' value='{escape(title)}'>"
            body += f"<textarea name='text' rows='6'>{escape(text)}</textarea><button>Save page</button></form></div>"
            self.send_html(page("Wiki", body))
            return
        if path == "/civic":
            body = nav(u) + "<div class='card'><h1>Civic</h1><p class='warn'>Preview straw-poll only. Not liquidgov-core. Not seating.</p>"
            body += "<form method='POST' action='/civic'><input name='title' placeholder='Motion title' required>"
            body += "<button>File motion</button></form>"
            for m in reversed(DATA["motions"][-30:]):
                body += (f"<div class='msg'><strong>{escape(m['title'])}</strong> — {escape(m['who'])} · support {m.get('n',0)} "
                         f"<form style='display:inline' method='POST' action='/civic/support'>"
                         f"<input type='hidden' name='id' value='{m['id']}'><button>Support</button></form></div>")
            body += "</div>"
            self.send_html(page("Civic", body))
            return
        if path == "/rooms":
            DATA["rooms"].setdefault("general", [])
            room = q.get("r", "general") or "general"
            DATA["rooms"].setdefault(room, [])
            body = nav(u) + "<div class='card'><h1>Rooms</h1><form method='GET' action='/rooms'>"
            body += f"<input name='r' value='{escape(room)}'><button>Join</button></form>"
            body += "<p>" + " · ".join(f"<a href='/rooms?r={escape(n)}'>{escape(n)}</a>" for n in sorted(DATA["rooms"])) + "</p>"
            for msg in DATA["rooms"][room][-40:]:
                body += f"<div class='msg'><strong>{escape(msg['who'])}</strong> {escape(msg['text'])}</div>"
            body += f"<form method='POST' action='/rooms'><input type='hidden' name='r' value='{escape(room)}'>"
            body += "<input name='text' required><button>Send</button></form></div>"
            self.send_html(page("Rooms", body))
            return
        if path == "/saved":
            mine = [x for x in DATA["bookmarks"] if x.get("who") == u]
            body = nav(u) + "<div class='card'><h1>Saved</h1><ul>"
            body += "".join(f"<li>{escape(x.get('ref',''))}</li>" for x in mine) or "<li class='muted'>Nothing saved.</li>"
            body += "</ul></div>"
            self.send_html(page("Saved", body))
            return
        if path == "/places":
            body = nav(u) + "<div class='card'><h1>Places</h1><form method='POST' action='/places'>"
            body += "<input name='name' placeholder='Place name' required>"
            body += "<input name='note' placeholder='Note'><button>Add place</button></form><ul>"
            for p in DATA["places"][-40:]:
                body += f"<li><strong>{escape(p['name'])}</strong> — {escape(p.get('note',''))} ({escape(p['who'])})</li>"
            body += "</ul></div>"
            self.send_html(page("Places", body))
            return
        if path == "/stats":
            body = nav(u) + "<div class='card'><h1>Stats</h1><ul>"
            body += f"<li>Accounts {len(DATA['users'])}</li>"
            body += f"<li>Friend edges {len(DATA['friends'])}</li>"
            body += f"<li>Messages {len(DATA['messages'])}</li>"
            body += f"<li>Board posts {len(DATA['board'])}</li>"
            body += f"<li>Motions {len(DATA['motions'])}</li>"
            body += f"<li>Live sessions {len(SESS)}</li></ul></div>"
            self.send_html(page("Stats", body))
            return
        if path == "/mod":
            body = nav(u) + "<div class='card'><h1>Reports</h1><p class='muted'>Preview queue. No severity tiers.</p><ul>"
            body += "".join(f"<li>{escape(r.get('who',''))} flagged {escape(str(r.get('what','')))}</li>"
                            for r in DATA["reports"][-40:]) or "<li class='muted'>None</li>"
            body += "</ul></div>"
            self.send_html(page("Reports", body))
            return
        if path == "/export":
            rec = dict(self.rec(u) or {})
            rec.pop("pass", None)
            blob = json.dumps({"user": rec, "friends": [p for p in DATA["friends"] if u in p],
                               "sent": [m for m in DATA["messages"] if m.get("from") == u]}, indent=2)
            body = nav(u) + "<div class='card'><h1>Export</h1><p class='muted'>Your rows only. Password hash omitted.</p>"
            body += f"<pre>{escape(blob)}</pre></div>"
            self.send_html(page("Export", body))
            return
        if path == "/help":
            body = nav(u) + "<div class='card'><h1>Help</h1><p>Local preview routes:</p><ul>"
            for href, lab in (("/", "Home"), ("/me", "Profile"), ("/friends", "Friends"), ("/search", "Search"),
                              ("/mail", "Messages"), ("/groups", "Groups"), ("/world", "World"),
                              ("/gallery", "Gallery"), ("/jukebox", "Jukebox"), ("/files", "Files"),
                              ("/board", "Board"), ("/polls", "Polls"), ("/events", "Events"),
                              ("/shop", "Shop"), ("/wiki", "Wiki"), ("/civic", "Civic"),
                              ("/rooms", "Rooms"), ("/saved", "Saved"), ("/places", "Places"),
                              ("/online", "Online"), ("/notes", "Notes"), ("/settings", "Settings")):
                body += f"<li><a href='{href}'>{lab}</a></li>"
            body += "</ul><p class='muted'>Not the C++ kernel. Not liquidgov.us live.</p></div>"
            self.send_html(page("Help", body))
            return
        if path == "/online":
            names = sorted(set(SESS.values()))
            body = nav(u) + "<div class='card'><h1>Online</h1><p class='muted'>Anyone with a live session on this process. Real presence is friends-only.</p><ul>"
            body += "".join(f"<li>{escape(n)}</li>" for n in names) or "<li class='muted'>Just you.</li>"
            body += "</ul></div>"
            self.send_html(page("Online", body))
            return
        if path == "/notes":
            mine = [x for x in DATA["notes"] if x.get("to") == u]
            for x in mine:
                x["read"] = True
            save(DATA)
            body = nav(u) + "<div class='card'><h1>Notes</h1>"
            body += "".join(f"<div class='msg'>{escape(x['text'])}</div>" for x in reversed(mine[-40:])) or "<p class='muted'>None.</p>"
            body += "</div>"
            self.send_html(page("Notes", body))
            return
        if path == "/settings":
            r = self.rec(u) or {}
            disc = r.get("discover", "exact")
            mine = [sid for sid, name in SESS.items() if name == u]
            blocked_list = [b if a == u else a for a, b in DATA["blocks"] if u in (a, b)]
            body = nav(u) + "<div class='card'><h1>Settings</h1><form method='POST' action='/settings'>"
            body += "<p>Who can find me</p>"
            for val, lab in (("hidden", "Friends only"), ("exact", "Exact match"), ("typeahead", "Exact match")):
                chk = "checked" if disc == val else ""
                body += f"<label><input type='radio' name='discover' value='{val}' {chk}> {lab}</label><br>"
            body += "<p></p><button>Save</button></form><h2>Sessions</h2><ul>"
            body += "".join(f"<li>{escape(sid[:8])}…</li>" for sid in mine)
            body += "</ul><form method='POST' action='/logout_all'><button>Log out other sessions</button></form>"
            body += "<h2>Change password</h2><form method='POST' action='/password'>"
            body += "<input type='password' name='old' placeholder='current' required>"
            body += "<input type='password' name='new' placeholder='new (8+)' required><button>Change</button></form>"
            body += "<h2>Invite codes</h2><form method='POST' action='/invite'><button>Make invite</button></form><ul>"
            body += "".join(f"<li><code>{escape(c)}</code></li>" for c in DATA["invites"][-8:]) or "<li class='muted'>None</li>"
            body += "</ul>"
            body += "<h2>Blocked</h2><ul>" + ("".join(f"<li>{escape(n)}</li>" for n in blocked_list) or "<li class='muted'>None</li>")
            body += "</ul><h2 class='warn'>Delete account</h2>"
            body += "<form method='POST' action='/delete'><button>Delete my account</button></form></div>"
            self.send_html(page("Settings", body))
            return
        self.send_html(b"Not found")

    def do_POST(self):
        path = self.path.split("?", 1)[0]
        f = self.form()
        u = self.user()
        if path == "/signup":
            user, pw, phone = f.get("username", "").strip(), f.get("password", ""), f.get("phone", "").strip()
            if not user or len(pw) < 8 or not phone:
                self.send_html(page("Sign up", "<p>Need username, 8+ password, phone.</p>"))
                return
            if user in DATA["users"]:
                self.send_html(page("Sign up", "<p>Username taken.</p>"))
                return
            DATA["users"][user] = {"id": DATA["next_id"], "pass": hashlib.sha256(pw.encode()).hexdigest(),
                                   "phone": phone, "blurb": "", "discover": "exact", "inner": [], "color": "#2f6fed"}
            DATA["next_id"] += 1
            save(DATA)
            tok = secrets.token_hex(16)
            SESS[tok] = user
            self.redir("/me", [("Set-Cookie", f"lg_session={tok}; HttpOnly; Path=/; SameSite=Lax")])
            return
        if path == "/login":
            user, pw = f.get("username", "").strip(), f.get("password", "")
            rec = DATA["users"].get(user)
            if not rec or rec["pass"] != hashlib.sha256(pw.encode()).hexdigest():
                self.send_html(page("Log in", "<p>Invalid username or password.</p>"))
                return
            DATA["users"][user]["seen"] = int(time.time())
            save(DATA)
            tok = secrets.token_hex(16)
            SESS[tok] = user
            self.redir("/me", [("Set-Cookie", f"lg_session={tok}; HttpOnly; Path=/; SameSite=Lax")])
            return
        if not u:
            self.redir("/login")
            return
        if path == "/me":
            DATA["users"][u]["blurb"] = f.get("blurb", "")[:2000]
            DATA["users"][u]["pin"] = f.get("pin", "")[:160]
            DATA["users"][u]["status"] = f.get("status", "")[:80]
            DATA["users"][u]["face"] = f.get("face", "🙂")[:8]
            col = f.get("color", "#2f6fed")[:16]
            DATA["users"][u]["color"] = col
            save(DATA)
            self.redir("/me")
            return
        if path == "/inner":
            who = f.get("who", "").strip()
            r = DATA["users"][u]
            r.setdefault("inner", [])
            if who and are_friends(u, who) and who not in r["inner"] and len(r["inner"]) < 12:
                r["inner"].append(who)
                save(DATA)
            self.redir("/me")
            return
        if path == "/friends":
            who = f.get("who", "").strip()
            if who and who != u and who in DATA["users"] and not blocked(u, who):
                pair = {"from": u, "to": who}
                if pair not in DATA["requests"] and not are_friends(u, who):
                    DATA["requests"].append(pair)
                    notify(who, f"{u} sent a friend request")
                    save(DATA)
            self.redir("/friends")
            return
        if path == "/friends/accept":
            who = f.get("who", "").strip()
            as_user = f.get("as_user", "").strip() or u
            DATA["requests"] = [x for x in DATA["requests"] if not (x.get("from") == who and x.get("to") == as_user)]
            if who and not are_friends(who, as_user):
                DATA["friends"].append([as_user, who])
                notify(who, f"{as_user} accepted")
            save(DATA)
            self.redir("/friends")
            return
        if path == "/block":
            who = f.get("who", "").strip()
            if who and [u, who] not in DATA["blocks"]:
                DATA["blocks"].append([u, who])
                DATA["friends"] = [p for p in DATA["friends"] if not (u in p and who in p)]
                save(DATA)
            self.redir("/friends")
            return
        if path == "/mail":
            other, text = f.get("to", "").strip(), f.get("text", "").strip()
            if other and text and are_friends(u, other) and not blocked(u, other):
                DATA["messages"].append({"from": u, "to": other, "text": text[:2000], "ts": int(time.time())})
                notify(other, f"New message from {u}")
                save(DATA)
            self.redir("/mail?with=" + other)
            return
        if path == "/groups":
            name = f.get("name", "").strip()[:80]
            if name:
                DATA["groups"].append({"id": len(DATA["groups"]) + 1, "name": name, "members": [u]})
                save(DATA)
            self.redir("/groups")
            return
        if path == "/groups/add":
            gid, who = f.get("id", ""), f.get("who", "").strip()
            g = next((x for x in DATA["groups"] if str(x["id"]) == gid), None)
            if g and u in g["members"] and who and are_friends(u, who) and who not in g["members"]:
                g["members"].append(who)
                notify(who, f"Added to group {g['name']}")
                save(DATA)
            self.redir("/g/" + gid)
            return
        if path == "/places":
            name, note = f.get("name", "").strip()[:80], f.get("note", "").strip()[:160]
            if name:
                DATA["places"].append({"who": u, "name": name, "note": note})
                save(DATA)
            self.redir("/places")
            return
        if path == "/password":
            old, new = f.get("old", ""), f.get("new", "")
            rec = DATA["users"].get(u)
            if rec and rec["pass"] == hashlib.sha256(old.encode()).hexdigest() and len(new) >= 8:
                rec["pass"] = hashlib.sha256(new.encode()).hexdigest()
                save(DATA)
            self.redir("/settings")
            return
        if path == "/invite":
            DATA["invites"].append(secrets.token_hex(4))
            save(DATA)
            self.redir("/settings")
            return
        if path == "/civic":
            title = f.get("title", "").strip()[:120]
            if title:
                DATA["motions"].append({"id": len(DATA["motions"]) + 1, "who": u, "title": title, "n": 0, "voters": []})
                save(DATA)
            self.redir("/civic")
            return
        if path == "/civic/support":
            pid = f.get("id", "")
            for m in DATA["motions"]:
                if str(m["id"]) == pid and u not in m.get("voters", []):
                    m["n"] = m.get("n", 0) + 1
                    m.setdefault("voters", []).append(u)
                    save(DATA)
                    break
            self.redir("/civic")
            return
        if path == "/rooms":
            room = f.get("r", "general").strip()[:40] or "general"
            text = f.get("text", "").strip()[:500]
            DATA["rooms"].setdefault(room, [])
            if text:
                DATA["rooms"][room].append({"who": u, "text": text})
                DATA["rooms"][room] = DATA["rooms"][room][-100:]
                save(DATA)
            self.redir("/rooms?r=" + room)
            return
        if path == "/wiki":
            title = f.get("p", "Main").strip()[:40] or "Main"
            DATA["wiki"][title] = f.get("text", "")[:8000]
            save(DATA)
            self.redir("/wiki?p=" + title)
            return
        if path == "/board/up":
            pid = f.get("id", "")
            for p in DATA["board"]:
                if str(p.get("id")) == pid:
                    p["up"] = p.get("up", 0) + 1
                    save(DATA)
                    break
            self.redir("/board")
            return
        if path == "/saved":
            ref = f.get("ref", "").strip()[:40]
            if ref and not any(x.get("who") == u and x.get("ref") == ref for x in DATA["bookmarks"]):
                DATA["bookmarks"].append({"who": u, "ref": ref})
                save(DATA)
            self.redir("/saved")
            return
        if path == "/board":
            text = f.get("text", "").strip()[:500]
            if text:
                DATA["board"].append({"id": len(DATA["board"]) + 1, "who": u, "text": text})
                save(DATA)
            self.redir("/board")
            return
        if path == "/polls":
            qn, opts = f.get("q", "").strip()[:120], [o.strip() for o in f.get("opts", "").split(",") if o.strip()][:6]
            if qn and opts:
                DATA["polls"].append({"id": len(DATA["polls"]) + 1, "who": u, "q": qn, "opts": opts, "votes": {o: 0 for o in opts}, "voters": []})
                save(DATA)
            self.redir("/polls")
            return
        if path == "/polls/vote":
            pid, opt = f.get("id", ""), f.get("opt", "")
            for p in DATA["polls"]:
                if str(p["id"]) == pid and opt in p["opts"] and u not in p.get("voters", []):
                    p["votes"][opt] = p["votes"].get(opt, 0) + 1
                    p.setdefault("voters", []).append(u)
                    save(DATA)
                    break
            self.redir("/polls")
            return
        if path == "/events":
            title, when = f.get("title", "").strip()[:80], f.get("when", "").strip()[:80]
            if title:
                DATA["events"].append({"who": u, "title": title, "when": when})
                save(DATA)
            self.redir("/events")
            return
        if path == "/shop":
            title, price = f.get("title", "").strip()[:80], f.get("price", "").strip()[:40]
            if title:
                DATA["shop"].append({"who": u, "title": title, "price": price})
                save(DATA)
            self.redir("/shop")
            return
        if path == "/report":
            DATA["reports"].append({"who": u, "what": f.get("what", "")[:80], "ts": int(time.time())})
            save(DATA)
            self.redir("/board")
            return
        if path == "/world":
            try:
                x, y = float(f.get("x", "50")), float(f.get("y", "50"))
            except ValueError:
                x, y = 50.0, 50.0
            DATA["world"].append({"who": u, "x": max(0, min(100, x)), "y": max(0, min(100, y))})
            DATA["world"] = DATA["world"][-200:]
            save(DATA)
            self.redir("/world")
            return
        if path == "/gallery":
            name, color = f.get("name", "").strip()[:40], f.get("color", "").strip()[:16] or "#2f6fed"
            if name:
                DATA["themes"].append({"name": name, "color": color, "author": u})
                save(DATA)
            self.redir("/gallery")
            return
        if path == "/gallery/install":
            DATA["users"][u]["color"] = f.get("color", "#2f6fed")[:16]
            save(DATA)
            self.redir("/me")
            return
        if path == "/jukebox":
            url = f.get("url", "").strip()[:300]
            if url.startswith("https://www.youtube.com/") or url.startswith("https://youtu.be/"):
                DATA["jukebox"].append({"who": u, "url": url})
                save(DATA)
            self.redir("/jukebox")
            return
        if path == "/files":
            other, name, bodytxt = f.get("to", "").strip(), f.get("name", "").strip()[:80], f.get("body", "")[:4000]
            if other and name and are_friends(u, other) and not blocked(u, other):
                DATA["files"].append({"from": u, "to": other, "name": name, "body": bodytxt})
                notify(other, f"{u} sent a file: {name}")
                save(DATA)
            self.redir("/files")
            return
        if path == "/settings":
            val = f.get("discover", "exact")
            if val in ("hidden", "exact", "typeahead"):
                DATA["users"][u]["discover"] = val
                save(DATA)
            self.redir("/settings")
            return
        if path == "/logout_all":
            keep = cookie(self.headers)
            dead = [sid for sid, name in list(SESS.items()) if name == u and sid != keep]
            for sid in dead:
                SESS.pop(sid, None)
            self.redir("/settings")
            return
        if path == "/delete":
            DATA["users"].pop(u, None)
            DATA["friends"] = [p for p in DATA["friends"] if u not in p]
            DATA["requests"] = [x for x in DATA["requests"] if u not in (x.get("from"), x.get("to"))]
            save(DATA)
            for sid, name in list(SESS.items()):
                if name == u:
                    SESS.pop(sid, None)
            self.redir("/", [("Set-Cookie", "lg_session=; Path=/; Max-Age=0")])
            return
        self.send_html(b"Not found")


if __name__ == "__main__":
    print(f"LiquidGov preview  http://127.0.0.1:{PORT}")
    HTTPServer(("127.0.0.1", PORT), H).serve_forever()
