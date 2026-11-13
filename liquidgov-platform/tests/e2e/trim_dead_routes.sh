#!/usr/bin/env bash
# Dead code trimmed (Brandon 2026-11-13: "delete anything that doesnt contribute... trim the bloat"): the old sandboxed
# profile-theme document /u/<name>/theme (unused since profile CSS styles the real page) and the old World demo's
# /world/ws socket (no page has opened it since /world became a redirect to /commons/) are gone. Kept, as controls:
# the /world redirect for old bookmarks, the profile page, and the other WebSocket routes.
# Usage: bash trim_dead_routes.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18128. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-trim-dead-routes; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18128; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
ADMIN=$(sid Bbottin); PAL=$(sid pal); STRANGER=$(sid stranger)
[ -n "$ADMIN" ] && [ -n "$PAL" ] && [ -n "$STRANGER" ] || { echo "seed gave no sessions"; exit 1; }

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
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{http_code}' -b "lg_session=$s" "$@" "$URL$p"; }
page() { curl -s -o "$W/$3" -w '%{http_code}' ${1:+-b "lg_session=$1"} "$URL$2"; }
# the composer bar's HTML (the div.composerbar) on a saved page
bar() { python3 -c 'import re,sys; t=open(sys.argv[1],encoding="utf-8").read(); m=re.search(r"<div class=\"composerbar\">.*?<button type=\"submit\">Post</button></div>", t, re.S); print(m.group(0) if m else "")' "$W/$1"; }

start
code() { curl -s -o /dev/null -w '%{http_code}' ${1:+-b "lg_session=$1"} "$URL$2"; }
upgrade() { curl -s -o /dev/null -w '%{http_code}' --max-time 3 -b "lg_session=$1" -H "Connection: Upgrade" -H "Upgrade: websocket" -H "Sec-WebSocket-Version: 13" -H "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==" "$URL$2"; }
r=$(code $PAL /u/pal/theme)
check "/u/pal/theme is gone (404)" '[ "$r" = 404 ]' "$r"
r=$(code $PAL /u/pal)
check "control: /u/pal still answers 200" '[ "$r" = 200 ]' "$r"
r=$(curl -s -o /dev/null -w '%{http_code} %{redirect_url}' "$URL/world")
check "control: /world still redirects old bookmarks to /commons/" '[[ "$r" == 30* && "$r" == *"/commons/"* ]]' "$r"
r=$(upgrade $PAL /world/ws)
check "/world/ws no longer upgrades (no 101)" '[ "$r" != 101 ]' "$r"
r=$(upgrade $PAL /community/ws)
check "control: /community/ws still upgrades (101)" '[ "$r" = 101 ]' "$r"

echo "passes=$passes fails=$fails"
[ $fails = 0 ]
