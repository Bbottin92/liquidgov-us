#!/usr/bin/env bash
# A group's post box opens the canvas with a "Canvas" button with a paintbrush icon, next to the photo button
# (Brandon 2026-11-12: "when i click draw instead in a group page's post section, it just brings up camera option,
# pictures, or files. not the canvas. and is should just say canvas and have a drawing or painting icon instead of
# saying draw instead"). The photo button's box clips its invisible file input, so the input can never cover the
# Canvas button (on iPhone Safari it spilled out over "Draw instead"; tests/browser/group_canvas_tap_check.py taps it).
# Usage: bash group_canvas_button.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18125. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-group-canvas-button; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18125; URL=http://127.0.0.1:$PORT
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
post $PAL /communities -d slug=tower --data-urlencode "name=Tower Garden" -d category=3 -d subcategory=x --data-urlencode "about=x" >/dev/null
r=$(page $PAL /c/tower member.html)
check "precondition: pal's group page answers 200" '[ "$r" = 200 ]' "$r"
check "precondition: the post box is there (composer bar)" '[ -n "$(bar member.html)" ]' "no composer bar"
check "the bar has a Canvas link to /c/tower/canvas" 'bar member.html | grep -qE "<a [^>]*href=\"/c/tower/canvas\"[^>]*>.*Canvas</a>"' "$(bar member.html | grep -o '<a [^>]*>.\{0,300\}' | head -2)"
check "...with an icon (an svg inside the link)" 'bar member.html | grep -oE "<a [^>]*href=\"/c/tower/canvas\".*</a>" | grep -q "<svg"' "no icon"
check "the link is labelled for screen readers" 'bar member.html | grep -oE "<a [^>]*href=\"/c/tower/canvas\"[^>]*>" | grep -qE "aria-label=\"[^\"]*[Cc]anvas"' "$(bar member.html | grep -oE '<a [^>]*href="/c/tower/canvas"[^>]*>')"
check "no \"Draw instead\" anywhere on the page" '! grep -q "Draw instead" $W/member.html' "still there"
check "control: the photo button is still in the bar" 'bar member.html | grep -q "name=\"media\""' "photo input gone"
check "the Canvas link is NOT inside the photo label (a tap on it can't reach the file input)" '! bar member.html | python3 -c "import sys,re; t=sys.stdin.read(); m=re.search(r\"<label class=\\\"filepick\\\".*?</label>\", t, re.S); sys.exit(0 if m and \"/canvas\" in m.group(0) else 1)"' "inside the label"
r=$(page $PAL /c/tower/canvas canvas.html)
check "the Canvas link's page opens (200)" '[ "$r" = 200 ]' "$r"
check "...and it is the canvas (a canvas element)" 'grep -q "<canvas" $W/canvas.html' "no canvas"
r=$(page $STRANGER /c/tower nonmember.html)
check "control: a non-member sees no post box and no Canvas link" '! grep -q "/c/tower/canvas" $W/nonmember.html' "link shown to non-member"
grep -o '\.filepick{[^}]*}' "$HERE/assets/site.css" > "$W/filepick.css"
check "site.css: the photo button clips its invisible input (overflow:hidden on .filepick)" 'grep -q "overflow:hidden" $W/filepick.css' "$(cat $W/filepick.css)"

echo "passes=$passes fails=$fails"
[ $fails = 0 ]
