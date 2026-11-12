#!/usr/bin/env bash
# The Feed has no Drawings category (Brandon 2026-11-10: drop it; Grok Bot's site pass 2026-11-11 still saw the tab).
#   /feed offers All, Friends and Communities, and no Drawings tab or filter=drawings link.
#   An old /feed?filter=drawings link (bookmarks, history) is answered as All: the All tab is current and a plain text
#   post shows (the old Drawings view left out every post without a picture, so it showed "Nothing in this view yet").
# Usage: bash feed_no_drawings.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18107. Never touches the live site. New file: committed suites are frozen.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-feed-no-drawings; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18107; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
# seed: Bbottin<->pal are friends
ADMIN=$(sid Bbottin); PAL=$(sid pal)
[ -n "$ADMIN" ] && [ -n "$PAL" ] || { echo "seed gave no sessions"; exit 1; }

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
get()  { curl -s -o "$W/$3" -w '%{http_code}' -b "lg_session=$1" "$URL$2"; }
# the feed's segmented control only (the composer and posts may say "drawing" for other reasons)
tabs() { python3 -c 'import re,sys; m=re.search(r"<div class=\"segtabs\">(.*?)</div>", open(sys.argv[1]).read(), re.S); print(m.group(1) if m else "")' "$W/$1"; }

start
post $PAL /feed -F "text=Plain words from pal" >/dev/null

r=$(get $ADMIN /feed f.html)
check "/feed answers 200" '[ "$r" = 200 ]' "$r"
check "control: the feed's tabs are there (All, Friends, Communities)" \
    'tabs f.html | grep -q "filter=all\">All<" && tabs f.html | grep -q "filter=friends\">Friends<" && tabs f.html | grep -q "filter=committees\">Communities<"' "$(tabs f.html)"
check "control: pal's plain post is in the feed" 'grep -q "Plain words from pal" "$W/f.html"' "no post"
check "no Drawings tab" '! tabs f.html | grep -qi "drawing"' "$(tabs f.html)"
check "no filter=drawings link anywhere on /feed" '! grep -qi "filter=drawings" "$W/f.html"' "found"

r=$(get $ADMIN "/feed?filter=drawings" d.html)
check "an old /feed?filter=drawings link still answers 200" '[ "$r" = 200 ]' "$r"
check "... as All: the All tab is current" 'tabs d.html | grep -q "aria-current=\"page\" href=\"/feed?filter=all\""' "$(tabs d.html)"
check "... as All: pal's plain post (no picture) shows" 'grep -q "Plain words from pal" "$W/d.html"' "missing"
check "... and it offers no Drawings tab" '! tabs d.html | grep -qi "drawing"' "$(tabs d.html)"

echo "passes=$passes fails=$fails"
echo "EXIT=$([ $fails -eq 0 ] && echo 0 || echo 1)"
[ $fails -eq 0 ]
