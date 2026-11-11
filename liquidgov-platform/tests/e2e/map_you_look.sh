#!/usr/bin/env bash
# M1.1 look (Grok Bot's mockup 1; Brandon 2026-11-11 pasted it: "thats literally the only thing i actually cared
# about"): your kind boxes carry a short second line built only from your own connections, the you box says YOU, the
# /map page has the trail (You > Groups > ...), and lgmap.js places your kinds on a ring around you. The picture
# itself is checked by tests/browser/phone_check.py case 2 (ring fits, you in the middle, no overlaps).
# Usage: bash map_you_look.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18110. Never touches the live site. New file: map_you.sh is frozen.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-map-you-look; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18110; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
ADMIN=$(sid Bbottin); STRANGER=$(sid stranger)
[ -n "$ADMIN" ] && [ -n "$STRANGER" ] || { echo "seed gave no sessions"; exit 1; }

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
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -b "lg_session=$s" "$@" "$URL$p"; }
node() { curl -s -o "$W/$3" -w '%{http_code}' -b "lg_session=$1" -G --data-urlencode "n=$2" "$URL/map/node"; }
# the "sub" of link $2 in JSON file $1 (empty when none)
sub_of() { python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(next((l["node"].get("sub","") for l in d["links"] if l["node"]["id"]==sys.argv[2]), ""))' "$W/$1" "$2"; }

start
post $ADMIN /communities -d "slug=fres1" -d "name=Fresno Neighbors" -d "category=0" -d "subcategory=Neighbors" -d "about=x" --data-urlencode "city=Fresno, CA"
post $STRANGER /communities -d "slug=other" -d "name=Stranger Things" -d "category=3" -d "subcategory=Music" -d "about=x"
r=$(node $ADMIN you you.json)
check "precondition: the you box answers" '[ "$r" = 200 ]' "$r"
check "the you box's second line is YOU" '[ "$(python3 -c "import json;print(json.load(open(\"$W/you.json\"))[\"node\"].get(\"sub\",\"\"))")" = YOU ]' "$(head -c 200 $W/you.json)"
check "Friends says how many friends you have (Bbottin has 1: pal)" '[ "$(sub_of you.json mine:friends)" = "1 person" ]' "sub=[$(sub_of you.json mine:friends)]"
check "Groups names your group" '[ "$(sub_of you.json mine:groups)" = "Fresno Neighbors" ]' "sub=[$(sub_of you.json mine:groups)]"
check "... and never a group you are not in" '! grep -q "Stranger Things" $W/you.json' "leaked"
check "Places names your city, then the rest of the site" '[[ "$(sub_of you.json mine:places)" == *", then the rest of the site" ]]' "sub=[$(sub_of you.json mine:places)]"
node $STRANGER you syou.json >/dev/null
check "someone else's Groups line is their own (stranger: Stranger Things)" '[ "$(sub_of syou.json mine:groups)" = "Stranger Things" ]' "sub=[$(sub_of syou.json mine:groups)]"
node $ADMIN root root.json >/dev/null
check "boxes that are not your kinds carry no second line" '! grep -q "\"sub\"" <(python3 -c "import json,sys; d=json.load(open(sys.argv[1])); [print(json.dumps(l[\"node\"])) for l in d[\"links\"] if l[\"node\"][\"kind\"]!=\"you\"]" $W/root.json)' "a sub on a non-kind box"
curl -s -o "$W/map.html" -b "lg_session=$ADMIN" "$URL/map"
check "the map page has the trail" 'grep -q "id=\"lgm-trail\"" $W/map.html' "no trail"
curl -s -o "$W/js.txt" "$URL/assets/lgmap.js"
check "lgmap.js places your kinds on a ring and draws the trail" 'grep -q "function ringPlace" $W/js.txt && grep -q "function trail" $W/js.txt' "missing"

echo "passes=$passes fails=$fails"
[ $fails -eq 0 ]
