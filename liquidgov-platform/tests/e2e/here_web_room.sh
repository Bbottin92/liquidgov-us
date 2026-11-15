#!/usr/bin/env bash
# Drops on other websites (the browser extension, 2026-11-15): /here/ws accepts a room named "/x/" + 64 lowercase hex,
# the SHA-256 the extension makes from a page's address; the server never sees the address. Checks: two people in the
# same hashed room see each other; a different hash is a different room; a malformed name, a signed-out visitor and
# someone who chose No are refused; a block hides the two from each other in the room.
# Usage: bash here_web_room.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18136. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
E2E="$(cd "$(dirname "$0")" && pwd)"
W=/tmp/lg-here-web-room; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18136; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] && [ -f "$E2E/here_ws_client.py" ] || { echo "missing binaries or client"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
ADMIN=$(sid Bbottin); PAL=$(sid pal); STRANGER=$(sid stranger)
[ -n "$ADMIN" ] && [ -n "$PAL" ] && [ -n "$STRANGER" ] || { echo "seed gave no sessions"; exit 1; }
(cd "$HERE" && exec env -u LIQUIDGOV_OPERATORS LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT "$BIN") > "$W/server.log" 2>&1 &
SERVER=$!
trap 'kill $SERVER $(jobs -p) 2>/dev/null' EXIT
for i in $(seq 50); do
    kill -0 $SERVER 2>/dev/null || { echo "server died"; cat "$W/server.log"; exit 1; }
    curl -s -o /dev/null --max-time 1 $URL/ && break
    sleep 0.2
done
for s in $ADMIN $PAL; do curl -s -o /dev/null -b "lg_session=$s" -d choice=1 -d back=/settings "$URL/here/choice"; done

A=/x/$(printf 'lg-web-v1|https://example.com/a' | sha256sum | cut -c1-64)
B=/x/$(printf 'lg-web-v1|https://example.com/b' | sha256sum | cut -c1-64)
cl() { python3 "$E2E/here_ws_client.py" $PORT "$@"; }
who() { python3 -c 'import json,sys; t=sys.stdin.read().strip(); print(" ".join(sorted(p["u"] for p in json.loads(t)["people"])) if t.startswith("{") else t)'; }

cl "$PAL" "$A" 2 0.3 0.4 > "$W/p1" & P=$!
r=$(cl "$ADMIN" "$A" 2 0.6 0.4 | who); wait $P; p=$(who < "$W/p1")
check "the same hashed page: both see each other" '[ "$r" = "Bbottin pal" ] && [ "$p" = "Bbottin pal" ]' "admin saw '$r', pal saw '$p'"

cl "$PAL" "$A" 2 0.3 0.4 > "$W/p2" & P=$!
r=$(cl "$ADMIN" "$B" 2 0.6 0.4 | who); wait $P; p=$(who < "$W/p2")
check "different pages are different rooms" '[ "$r" = "Bbottin" ] && [ "$p" = "pal" ]' "admin saw '$r', pal saw '$p'"

up=$(echo "$A" | tr a-f A-F)
r1=$(cl "$PAL" "/x/abc" 1); r2=$(cl "$PAL" "$up" 1); r3=$(cl "$PAL" "${A}0" 1)
check "malformed room names are refused" '[ "${r1%% *}${r2%% *}${r3%% *}" = "REFUSEDREFUSEDREFUSED" ]' "$r1 / $r2 / $r3"
r=$(cl - "$A" 1)
check "signed out is refused" '[ "${r%% *}" = "REFUSED" ]' "$r"
r=$(cl "$STRANGER" "$A" 1)
check "someone who has not chosen Yes is refused" '[ "${r%% *}" = "REFUSED" ]' "$r"

y=$(cl "$PAL" "$A" 1 | python3 -c 'import json,sys; print(json.load(sys.stdin).get("you",""))')
check "the snapshot says which dot is yours" '[ "$y" = "pal" ]' "$y"

tk() { curl -s -b "lg_session=$PAL" "$URL/here/ticket" | python3 -c 'import json,sys; print(json.load(sys.stdin)["ticket"])'; }
T=$(tk)
r=$(cl - "$A&ticket=$T" 1 | python3 -c 'import json,sys; t=sys.stdin.read(); print(json.loads(t).get("you","") if t.startswith("{") else t)')
check "a ticket lets the extension in without a cookie" '[ "$r" = "pal" ]' "$r"
r=$(cl - "$A&ticket=$T" 1)
check "a ticket works once" '[ "${r%% *}" = "REFUSED" ]' "$r"
T=$(tk); r=$(cl - "/u/pal&ticket=$T" 1)
check "a ticket only opens web-page rooms, not a profile" '[ "${r%% *}" = "REFUSED" ]' "$r"
code=$(curl -s -o "$W/t.json" -w '%{http_code}' "$URL/here/ticket")
check "signed out: no ticket" '[ "$code" = 401 ]' "$code"
check "the ticket answer allows no other website to read it" '! curl -s -D - -o /dev/null -b "lg_session=$PAL" -H "Origin: https://evil.example" "$URL/here/ticket" | grep -qi "access-control"' "CORS header"

curl -s -o /dev/null -b "lg_session=$PAL" -d who=Bbottin "$URL/friends/block"
cl "$PAL" "$A" 2 0.3 0.4 > "$W/p3" & P=$!
r=$(cl "$ADMIN" "$A" 2 0.6 0.4 | who); wait $P; p=$(who < "$W/p3")
check "a block hides the two from each other on a web page" '[ "$r" = "Bbottin" ] && [ "$p" = "pal" ]' "admin saw '$r', pal saw '$p'"

echo "passes=$passes fails=$fails"
[ $fails = 0 ]
