#!/usr/bin/env bash
# Events is always on the You ring (Brandon 2026-11-11 4:30 PM PT, "do what you think is best"; Grok Bot chose; live map
# finding #1): even with nothing in it, the box and its chip show, with a friendly empty state, so people can see the
# map has a place for events before M1.4 builds them. Before this, an empty Events was left off like any empty kind.
#   - a brand-new account's ring has mine:events, labeled Events, second line "None yet"
#   - an account with posts, groups and friends has it too
#   - mine:events answers 200 and leads only back to you (no items yet)
#   - signed out there is no Events box (it is always the viewer's own)
#   - lgmap.js has the friendly empty text for it ("No events yet")
# Usage: bash map_events_always.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18108. Never touches the live site. New file: map_interests_feeds.sh is frozen (its two
# "Events is left off" checks are the old rule; see REPORT4).
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-map-events-always; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18108; URL=http://127.0.0.1:$PORT
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
node() { curl -s -o "$W/$3" -w '%{http_code}' -b "lg_session=$1" -G --data-urlencode "n=$2" "$URL/map/node"; }
# Does the JSON in $1 list a link to node id $2?
has() { python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); sys.exit(0 if any(l["node"]["id"]==sys.argv[2] for l in d["links"]) else 1)' "$W/$1" "$2" 2>/dev/null; }
# every link id in $1, space separated
ids() { python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(" ".join(l["node"]["id"] for l in d["links"]))' "$W/$1" 2>/dev/null; }
field() { python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["node"].get(sys.argv[2],""))' "$W/$1" "$2" 2>/dev/null; }
sub_of() { python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(next((l["node"].get("sub","") for l in d["links"] if l["node"]["id"]==sys.argv[2]), ""))' "$W/$1" "$2" 2>/dev/null; }
label_of() { python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(next((l["node"]["label"] for l in d["links"] if l["node"]["id"]==sys.argv[2]), ""))' "$W/$1" "$2" 2>/dev/null; }

start
node $STRANGER you s0.json >/dev/null
check "precondition: stranger's you box answers" '[ "$(field s0.json kind)" = you ]' "$(head -c 200 $W/s0.json)"
check "a new account with nothing in it: Events is on the You ring" 'has s0.json mine:events' "$(ids s0.json)"
check "... labeled Events" '[ "$(label_of s0.json mine:events)" = Events ]' "label=[$(label_of s0.json mine:events)]"
check "... with the friendly second line None yet" '[ "$(sub_of s0.json mine:events)" = "None yet" ]' "sub=[$(sub_of s0.json mine:events)]"

post $PAL /feed -F "text=Public hello from pal" -F "anybody=1" >/dev/null
post $ADMIN /feed -F "text=My own post" >/dev/null
post $ADMIN /communities -d "slug=fres1" -d "name=Fresno Neighbors" -d "category=0" -d "subcategory=Neighbors" -d "about=x" --data-urlencode "city=Fresno, CA" >/dev/null
node $ADMIN you a0.json >/dev/null
check "control: Bbottin's ring has Friends, Groups and Feeds" 'has a0.json mine:friends && has a0.json mine:groups && has a0.json mine:feeds' "$(ids a0.json)"
check "Bbottin's ring has Events too" 'has a0.json mine:events' "$(ids a0.json)"

r=$(node $ADMIN mine:events ev.json)
check "mine:events answers 200" '[ "$r" = 200 ]' "$r"
check "... it is Events, one of your kinds" '[ "$(field ev.json label)" = Events ] && [ "$(field ev.json kind)" = mine ]' "$(head -c 200 $W/ev.json)"
check "... and leads only back to you (no events exist yet)" '[ "$(ids ev.json)" = "you" ]' "$(ids ev.json)"

r=$(curl -s -o /dev/null -w '%{http_code}' -G --data-urlencode "n=mine:events" "$URL/map/node")
check "signed out: no Events box" '[ "$r" != 200 ]' "$r"

curl -s -o "$W/js.txt" "$URL/assets/lgmap.js"
check "lgmap.js shows a friendly empty state for Events" 'grep -q "No events yet" $W/js.txt' "missing"

echo "passes=$passes fails=$fails"
[ $fails -eq 0 ]
