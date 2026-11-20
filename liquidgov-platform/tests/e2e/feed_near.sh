#!/usr/bin/env bash
# Feed: the city-based "Near me" tab is gone (wip/179, 2026-11-20). It was built as "posts in groups in your city";
#   Brandon meant posts from people whose droplet is left within a chosen distance on the street map, which needs the
#   droplet on the street map first, so the wrong tab was taken out before it went live. This suite now checks that
#   no Near me tab is offered, that an old /feed?filter=near link shows All, and that the mix-in menu does not list it.
# Usage: bash feed_near.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18148. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-feed-near; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18148; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
# seed: Bbottin<->pal and pal<->stranger are friends
ME=$(sid Bbottin); PAL=$(sid pal); STR=$(sid stranger)
[ -n "$ME" ] && [ -n "$PAL" ] && [ -n "$STR" ] || { echo "seed gave no sessions"; exit 1; }

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
tabs()  { python3 -c 'import re,sys; m=re.search(r"<div class=\"segtabs\">(.*?)</div>", open(sys.argv[1]).read(), re.S); print(m.group(1) if m else "")' "$W/$1"; }
order() { python3 -c 'import re,sys; print(" ".join(re.findall(r"<p>(F-[a-z0-9-]+)</p>", open(sys.argv[1]).read())))' "$W/$1"; }

start
post $STR /communities -d "slug=fres&name=Fresno+Bikes&category=10&subcategory=Bikes&city=Fresno%2C+CA" >/dev/null
post $STR /c/fres -F "text=F-fres-one" >/dev/null
post $PAL /feed -F "text=F-pal-plain" >/dev/null
post $ME /me/profile -d "display_name=Brandon&living=fresno+ca" >/dev/null
r=$(curl -s -o "$W/a.html" -w '%{http_code}' -b "lg_session=$ME" "$URL/feed?filter=all")
check "All answers 200" '[ "$r" = 200 ]' "$r"
check "no Near me tab on the feed" '! tabs a.html | grep -q "Near me"' "$(tabs a.html)"
check "...and no filter=near link anywhere on the page" '! grep -q "filter=near" $W/a.html' "found"
r=$(curl -s -o "$W/n.html" -w '%{http_code}' -b "lg_session=$ME" "$URL/feed?filter=near")
check "an old /feed?filter=near link still answers 200" '[ "$r" = 200 ]' "$r"
check "...and shows All (All is the current tab, the same posts in the same order)" \
    'tabs n.html | grep -q "aria-current=\"page\" href=\"/feed?filter=all\">All<" && [ "$(order n.html)" = "$(order a.html)" ]' "$(order n.html) vs $(order a.html)"
check "the mix-in menu does not offer Near me" '! grep -qi "name=\"tab\" value=\"near\"\|value=\"near\"" $W/a.html' "offered"
stop
echo "passes=$passes fails=$fails"
echo "EXIT=$([ $fails -eq 0 ] && echo 0 || echo 1)"
[ $fails -eq 0 ]
