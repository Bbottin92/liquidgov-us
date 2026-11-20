#!/usr/bin/env bash
# What the site answers for its static files (2026-11-17, slice 1 of the website cleanup, queue q57).
# The asset routes were one hand-copied block per file; they are being folded into one table-driven handler. This suite
# is the spec both versions must meet: for every static route the status, the content type, the Cache-Control line and
# the body (compared with the file on disk, hashed here, not copied from the server's code) must be exactly these.
#   versioned-aware routes: "...?v=<anything>" -> a year, immutable; the bare URL -> no-store (an old page, a bookmark)
#   icons -> one day; the share images and the intro video -> a year, immutable; the video also answers Range with 206
# Usage: bash asset_routes.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18140. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-asset-routes; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18140; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
[ -d "$HERE/assets" ] || { echo "no assets folder at $HERE"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }

SERVER=""
start() {
    (cd "$HERE" && exec env -u LIQUIDGOV_OPERATORS LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT \
        "$BIN") >> "$W/server.log" 2>&1 &
    SERVER=$!
    for i in $(seq 50); do
        kill -0 $SERVER 2>/dev/null || { echo "server died"; cat "$W/server.log"; exit 1; }
        curl -s -o /dev/null --max-time 1 $URL/ && return
        sleep 0.2
    done
    echo "server did not start"; kill $SERVER; exit 1
}
stop() { [ -n "$SERVER" ] && kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null; SERVER=""; }
trap stop EXIT
start

# The python half reads the table below, asks the server, and prints PASS/FAIL lines; its last line is "passes=N fails=M".
python3 - "$URL" "$HERE" <<'PY'
import hashlib, http.client, os, sys
from urllib.parse import urlparse

url, here = sys.argv[1], sys.argv[2]
u = urlparse(url)
passes = fails = 0
YEAR = "Cache-Control: public, max-age=31536000, immutable"
DAY = "Cache-Control: public, max-age=86400"
NOSTORE = "Cache-Control: no-store"
JS, CSS = "text/javascript; charset=utf-8", "text/css; charset=utf-8"

def get(path, headers=None):
    c = http.client.HTTPConnection(u.hostname, u.port, timeout=15)
    c.request("GET", path, headers=headers or {})
    r = c.getresponse()
    body = r.read()
    hdrs = {k.lower(): v for k, v in r.getheaders()}
    c.close()
    return r.status, hdrs, body

def sha(b): return hashlib.sha256(b).hexdigest()[:16]
def disk(rel):
    with open(os.path.join(here, rel), "rb") as f: return f.read()

def check(name, cond, why=""):
    global passes, fails
    if cond: print("PASS " + name); passes += 1
    else: print("FAIL " + name + (" (" + why + ")" if why else "")); fails += 1

def cache_of(hdrs):
    return "Cache-Control: " + hdrs["cache-control"] if "cache-control" in hdrs else "(none)"

# 1. routes that honour a ?v= version: (url path, file, content type)
versioned = [("/assets/" + n, "assets/" + n, JS) for n in (
    "setup.js", "mail_widget.js", "draft_publish.js", "people_search.js", "jukebox.js", "here.js", "canvas.js",
    "walker.js", "here_media.js", "lgmap_walk.js", "tip.js", "home_drop.js", "intro.js", "video_links.js", "hints.js",
    "juketube.js", "lgmap.js", "push.js", "finerange.js")]
versioned += [("/assets/" + n, "assets/" + n, CSS) for n in ("site.css", "setup.css", "juketube.css", "lgmap.css")]
for path, rel, ctype in versioned:
    want = sha(disk(rel))
    st, h, b = get(path + "?v=0123456789")
    check(path + "?v= : 200, " + ctype + ", a year immutable, body = file on disk",
          st == 200 and h.get("content-type") == ctype and cache_of(h) == YEAR and sha(b) == want,
          "status %s type %s cache %s body %s want %s" % (st, h.get("content-type"), cache_of(h), sha(b), want))
    st, h, b = get(path)
    check(path + " bare : 200, no-store, body = file on disk",
          st == 200 and h.get("content-type") == ctype and cache_of(h) == NOSTORE and sha(b) == want,
          "status %s cache %s" % (st, cache_of(h)))

# 2. the service worker: never cached, may control the whole site
st, h, b = get("/sw.js")
check("/sw.js : 200, no-store, Service-Worker-Allowed: /, body = assets/sw.js",
      st == 200 and h.get("content-type") == JS and cache_of(h) == NOSTORE and h.get("service-worker-allowed") == "/"
      and sha(b) == sha(disk("assets/sw.js")), "status %s cache %s sw-allowed %s" % (st, cache_of(h), h.get("service-worker-allowed")))

# 3. icons: one day
icons = [("/apple-touch-icon.png", "assets/apple-touch-icon.png"), ("/icon-192.png", "assets/icon-192.png"),
         ("/icon-512.png", "assets/icon-512.png"),
         ("/apple-touch-icon-precomposed.png", "assets/apple-touch-icon.png"),
         ("/assets/apple-touch-icon.png", "assets/apple-touch-icon.png"),
         ("/assets/icon-192.png", "assets/icon-192.png"), ("/assets/icon-512.png", "assets/icon-512.png")]
for k in ("mail", "social", "civic"):
    icons += [("/icon-%s.png" % k, "assets/icon-%s.png" % k), ("/icon-%s-192.png" % k, "assets/icon-%s-192.png" % k),
              ("/icon-%s-512.png" % k, "assets/icon-%s-512.png" % k)]
for path, rel in icons:
    st, h, b = get(path)
    check(path + " : 200, image/png, one day, body = file",
          st == 200 and h.get("content-type") == "image/png" and cache_of(h) == DAY and sha(b) == sha(disk(rel)),
          "status %s type %s cache %s" % (st, h.get("content-type"), cache_of(h)))

# 4. share images: a year immutable
for path, rel, ctype in (("/assets/og-image-v4.jpg", "assets/og-image-v4.jpg", "image/jpeg"),
                         ("/assets/og-image-v3.jpg", "assets/og-image-v3.jpg", "image/jpeg"),
                         ("/assets/og-image-v2.jpg", "assets/og-image-v2.jpg", "image/jpeg"),
                         ("/assets/og-image.png", "assets/og-image.png", "image/png")):
    st, h, b = get(path)
    check(path + " : 200, " + ctype + ", a year immutable, body = file",
          st == 200 and h.get("content-type") == ctype and cache_of(h) == YEAR and sha(b) == sha(disk(rel)),
          "status %s type %s cache %s" % (st, h.get("content-type"), cache_of(h)))

# 5. the intro video: whole file, and a Range request answered with 206 and the right slice
vid = disk("assets/intro.mp4")
st, h, b = get("/assets/intro.mp4")
check("/assets/intro.mp4 : 200, video/mp4, a year immutable, Accept-Ranges bytes, body = file",
      st == 200 and h.get("content-type") == "video/mp4" and cache_of(h) == YEAR and h.get("accept-ranges") == "bytes"
      and sha(b) == sha(vid), "status %s cache %s ranges %s" % (st, cache_of(h), h.get("accept-ranges")))
st, h, b = get("/assets/intro.mp4", {"Range": "bytes=10-109"})
check("/assets/intro.mp4 Range bytes=10-109 : 206, Content-Range, 100 bytes = that slice of the file",
      st == 206 and h.get("content-range") == "bytes 10-109/%d" % len(vid) and b == vid[10:110],
      "status %s content-range %s len %d" % (st, h.get("content-range"), len(b)))
st, h, b = get("/assets/intro.mp4", {"Range": "bytes=999999999-"})
check("/assets/intro.mp4 Range past the end : whole file (200), never an error",
      st == 200 and sha(b) == sha(vid), "status %s" % st)

# 6. a route that does not exist is a plain 404, and the server keeps answering afterwards
st, h, b = get("/assets/not_a_real_file.js")
check("/assets/not_a_real_file.js : 404", st == 404, "status %s" % st)
st, h, b = get("/assets/site.css?v=1")
check("the server still answers after a 404", st == 200)

print("passes=%d fails=%d" % (passes, fails))
sys.exit(0 if fails == 0 else 1)
PY
rc=$?
stop
[ "$rc" = 0 ]
