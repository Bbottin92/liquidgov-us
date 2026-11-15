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
# words in what people wrote (any order), comments, your own messages, dates and times (tz=0: UTC)
got() { curl -s -b "lg_session=$1" "$URL/map/search?tz=0&q=$2" | python3 -c 'import json,sys; print(" ".join(r["kind"] + "=" + (r["id"] or r["page"]) for r in json.load(sys.stdin)["results"]))'; }
curl -s -o /dev/null -b "lg_session=$PAL" -F "text=The river cleanup crew meets at the old bridge" -F "anybody=1" $URL/feed
r=$(got "$ADMIN" "bridge%20cleanup")
check "a post by its words, in any order" 'echo "$r" | grep -q "^post=post:[0-9]*"' "$r"
PID=$(echo "$r" | grep -o "post:[0-9]*" | head -1 | cut -d: -f2)
curl -s -o /dev/null -b "lg_session=$ADMIN" -d "text=Bring gloves and trash grabbers" "$URL/post/$PID/reply"
r=$(got "$PAL" "trash%20grabbers")
check "a comment by its words, leading to its post" '[ "$r" = "comment=post:$PID" ]' "$r"
curl -s -o /dev/null -b "lg_session=$PAL" -F "to=stranger" -F "text=Secret picnic plans for Saturday" $URL/mail
r=$(got "$STRANGER" "picnic")
check "a message you got, opening its conversation" '[ "$r" = "message=/mail?with=pal" ]' "$r"
r=$(got "$PAL" "picnic")
check "a message you sent" '[ "$r" = "message=/mail?with=stranger" ]' "$r"
r=$(got "$ADMIN" "picnic")
check "someone else's message is never found" '[ -z "$r" ]' "$r"
r=$(got "$ADMIN" "today%20bridge")
check "today and a word" 'echo "$r" | grep -q "post=post:$PID"' "$r"
r=$(got "$ADMIN" "yesterday%20bridge")
check "yesterday and a word: not today's post" '[ -z "$r" ]' "$r"
r=$(got "$ADMIN" "$(date -u +%Y-%m-%d)")
check "a date alone lists what was written that day" 'echo "$r" | grep -q "post=post:$PID" && echo "$r" | grep -q "comment=post:$PID"' "$r"
H=$(date -u +%H); H12=$(( (10#$H + 11) % 12 + 1 )); AP=$([ $((10#$H)) -lt 12 ] && echo am || echo pm)
r=$(got "$ADMIN" "bridge%20${H12}${AP}")
check "a time of day and a word" 'echo "$r" | grep -q "post=post:$PID"' "$r"
OTHER=$(( (10#$H + 6) % 24 )); r=$(got "$ADMIN" "bridge%20${OTHER}:00")
check "another time of day: not found" '[ -z "$r" ]' "$r"
r=$(got "$ADMIN" "$(date -u +%b%%20%-d)%20bridge")
check "a month and day (nov 12)" 'echo "$r" | grep -q "post=post:$PID"' "$r"

curl -s -o /dev/null -b "lg_session=$PAL" -d who=Bbottin "$URL/friends/block"
r=$(ids "$ADMIN" "pal")
check "a person who blocked you is not found" '! echo " $r " | grep -q " user:pal "' "$r"
r=$(ids "$ADMIN" "s")
check "one letter finds nothing" '[ -z "$r" ]' "$r"
code=$(curl -s -o /dev/null -w '%{http_code}' "$URL/map/search?q=synth")
check "signed out is refused" '[ "$code" = 401 ]' "$code"

echo "passes=$passes fails=$fails"
[ $fails = 0 ]
