#!/usr/bin/env bash
# Search on the map (Brandon 2026-11-15: "a search bar at the top where you can search for stuff"): the map page has
# the bar; GET /map/search?q= lists what the viewer's map may show (map_gnode), people by the people search's rule.
# Checks: the bar is on the map page; a group is found by part of its name, any case; a person by exact handle; a blocked
# person is not; one letter finds nothing; signed out is refused.
# Usage: bash map_search.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18137. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-map-search; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18137; URL=http://127.0.0.1:$PORT
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
(cd "$HERE" && exec env -u LIQUIDGOV_OPERATORS LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT "$BIN") > "$W/server.log" 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null' EXIT
for i in $(seq 50); do
    kill -0 $SERVER 2>/dev/null || { echo "server died"; cat "$W/server.log"; exit 1; }
    curl -s -o /dev/null --max-time 1 $URL/ && break
    sleep 0.2
done
curl -s -o /dev/null -b "lg_session=$PAL" -d slug=synth-heads -d name="Synth Heads" -d category=10 -d subcategory=music $URL/communities
ids() { curl -s -b "lg_session=$1" "$URL/map/search?q=$2" | python3 -c 'import json,sys; print(" ".join(r["id"] for r in json.load(sys.stdin)["results"]))'; }

check "the map page has the search bar" 'curl -s -b "lg_session=$ADMIN" $URL/map | grep -q "id=\"lgm-q\""' "no bar"
r=$(ids "$ADMIN" "SYNTH")
check "a group by part of its name, any case" '[ "$r" = "group:synth-heads" ]' "$r"
r=$(ids "$ADMIN" "pal")
check "a person by handle" 'echo " $r " | grep -q " user:pal "' "$r"
curl -s -o /dev/null -b "lg_session=$PAL" -d who=Bbottin "$URL/friends/block"
r=$(ids "$ADMIN" "pal")
check "a person who blocked you is not found" '! echo " $r " | grep -q " user:pal "' "$r"
r=$(ids "$ADMIN" "s")
check "one letter finds nothing" '[ -z "$r" ]' "$r"
code=$(curl -s -o /dev/null -w '%{http_code}' "$URL/map/search?q=synth")
check "signed out is refused" '[ "$code" = 401 ]' "$code"

echo "passes=$passes fails=$fails"
[ $fails = 0 ]
