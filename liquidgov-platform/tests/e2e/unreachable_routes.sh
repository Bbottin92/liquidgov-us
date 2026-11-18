#!/usr/bin/env bash
# Three things no page reached (site inventory 2026-11-17, gaps.md; Brandon 2026-11-17 "Yes, fix them", queue q104):
#   1. /terms ("How this association works") had no link anywhere: the footer now links it next to Privacy.
#   2. A member could not remove their own portrait: the Remove portrait form left /me in 6c4291e (2026-09-10) while
#      POST /me/avatar/clear stayed. /me shows the form again, only when there is a portrait to remove.
#   3. /me/post/canvas and POST /me/post lost their only link with the /me composer (e015d27); posting lives on /feed.
#      Both are deleted. Controls: /feed/canvas and posting to /feed still work.
# Usage: bash unreachable_routes.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18142. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-unreachable-routes; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18142; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
PAL=$(sid pal); STRANGER=$(sid stranger)
[ -n "$PAL" ] && [ -n "$STRANGER" ] || { echo "seed gave no sessions"; exit 1; }

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
page() { curl -s -o "$W/$3" -w '%{http_code}' ${1:+-b "lg_session=$1"} "$URL$2"; }
code() { curl -s -o /dev/null -w '%{http_code}' ${1:+-b "lg_session=$1"} "$URL$2"; }
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{http_code}' -b "lg_session=$s" "$@" "$URL$p"; }
# a real 4x4 PNG, made here so the test needs no fixture file
python3 -c 'import struct,zlib
def ch(t,d): return struct.pack(">I",len(d))+t+d+struct.pack(">I",zlib.crc32(t+d)&0xffffffff)
raw=b"".join(b"\x00"+b"\xff\x00\x00"*4 for _ in range(4))
open("'"$W"'/p.png","wb").write(b"\x89PNG\r\n\x1a\n"+ch(b"IHDR",struct.pack(">IIBBBBB",4,4,8,2,0,0,0))+ch(b"IDAT",zlib.compress(raw))+ch(b"IEND",b""))' \
    || { echo "could not make the test PNG"; exit 1; }

start

# 1. Terms is linked from the footer of every page
r=$(page "" / home.html)
check "precondition: / answers 200" '[ "$r" = 200 ]' "$r"
check "the footer links /terms" 'grep -q "<a href=\"/terms\">" $W/home.html' "no /terms link"
check "control: the footer still links /privacy" 'grep -q "<a href=\"/privacy\">" $W/home.html' "no /privacy link"
page $PAL /me me0.html >/dev/null
check "the footer links /terms on a signed-in page too" 'grep -q "<a href=\"/terms\">" $W/me0.html' "no /terms link"
r=$(code "" /terms)
check "control: /terms answers 200" '[ "$r" = 200 ]' "$r"

# 2. Remove portrait is back on /me, only when there is one
check "no portrait yet: /me has no Remove portrait form" '! grep -q "action=\"/me/avatar/clear\"" $W/me0.html' "form shown with no portrait"
r=$(post $PAL /me/avatar -F "photo=@$W/p.png;type=image/png")
r2=$(code "" /portrait/pal)
check "precondition: the portrait upload worked (/portrait/pal 200)" '[ "$r2" = 200 ]' "upload $r, portrait $r2"
page $PAL /me me1.html >/dev/null
check "with a portrait: /me has the Remove portrait form" 'grep -q "action=\"/me/avatar/clear\"" $W/me1.html' "no form"
check "...with a button that says so" 'grep -q "Remove portrait" $W/me1.html' "no button text"
page $STRANGER /me me_s.html >/dev/null
check "another member without a portrait sees no form" '! grep -q "action=\"/me/avatar/clear\"" $W/me_s.html' "form shown"
r=$(post $PAL /me/avatar/clear -d "")  # -d: curl sends a GET when a POST carries no data
r2=$(code "" /portrait/pal)
check "Remove portrait removes it (/portrait/pal 404 after)" '[ "$r2" = 404 ]' "clear $r, portrait $r2"
page $PAL /me me2.html >/dev/null
check "after removing: the form is gone again" '! grep -q "action=\"/me/avatar/clear\"" $W/me2.html' "form still shown"

# 3. the unused composer routes are gone; the real ones stay
r=$(code $PAL /me/post/canvas)
check "/me/post/canvas is gone (404)" '[ "$r" = 404 ]' "$r"
r=$(post $PAL /me/post --data-urlencode "text=hello")
check "POST /me/post is gone (404)" '[ "$r" = 404 ]' "$r"
r=$(code $PAL /feed/canvas)
check "control: /feed/canvas still answers 200" '[ "$r" = 200 ]' "$r"
r=$(post $PAL /feed --data-urlencode "text=still posts")
check "control: posting to /feed still works (redirect)" '[[ "$r" == 30* ]]' "$r"
page $PAL /feed feed.html >/dev/null
check "control: ...and the post shows on /feed" 'grep -q "still posts" $W/feed.html' "post missing"

echo "passes=$passes fails=$fails"
[ $fails = 0 ]
