#!/usr/bin/env bash
# The canvas pages load the drawing engine from assets/canvas.js (moved out of canvas.cpp's inline script 2026-11-13,
# code moved, not rewritten; Brandon: "MODULARIZE EVERYTHING"). Each page passes its own values in window.LG_CANVAS
# (px, frames, user, mode, postTarget); the engine is no longer inline; the file is served and parses.
# Usage: bash canvas_asset.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18127. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-canvas-asset; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18127; URL=http://127.0.0.1:$PORT
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
post $PAL /communities -d slug=tower --data-urlencode "name=Tower" -d category=3 -d subcategory=x --data-urlencode "about=x" >/dev/null
cfg() { grep -o 'window.LG_CANVAS={[^<]*}' "$W/$1"; }
for spec in "/me/canvas frames ''" "/me/portrait portrait ''" "/feed/canvas post /feed" "/me/post/canvas post /me/post" "/c/tower/canvas post /c/tower"; do
    set -- $spec; path=$1; mode=$2; target=$3; [ "$target" = "''" ] && target=""
    f=$(echo "$path" | tr '/' '_').html
    r=$(page $PAL "$path" "$f")
    check "$path answers 200" '[ "$r" = 200 ]' "$r"
    check "$path links /assets/canvas.js" 'grep -q "<script src=\"/assets/canvas.js" $W/$f' "no script tag"
    check "$path passes mode $mode" 'cfg $f | grep -q "\"mode\":\"$mode\""' "$(cfg $f)"
    check "$path passes user pal and postTarget '$target'" 'cfg $f | grep -q "\"user\":\"pal\"" && cfg $f | grep -q "\"postTarget\":\"$target\""' "$(cfg $f)"
    check "$path no longer carries the engine inline" '! grep -q "var LAYN=3" $W/$f' "inline engine"
    check "control: $path still has its Save button" 'grep -q "id=\"csave\"" $W/$f' "no save button"
done
r=$(curl -s -o "$W/canvas.js" -w '%{http_code}' "$URL/assets/canvas.js")
check "/assets/canvas.js is served (200)" '[ "$r" = 200 ]' "$r"
check "...and is the engine" 'grep -q "var LAYN=3" $W/canvas.js && grep -q "window.LG_CANVAS" $W/canvas.js' "not the engine"
if command -v node >/dev/null; then check "...and parses (node --check)" 'node --check $W/canvas.js' "syntax error"; fi

echo "passes=$passes fails=$fails"
[ $fails = 0 ]
