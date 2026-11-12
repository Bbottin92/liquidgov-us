#!/usr/bin/env bash
# People under an interest are HIDDEN until the signup privacy step exists (Brandon 2026-11-12: the privacy step
# "ships with the interest boxes or before them, never after"; interest people went live first in mapkinds1, so they
# come off). An interest box still grows its groups and proposals; people stay reachable everywhere else (Friends).
# Usage: bash map_interest_people_hidden.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18120. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-map-interest-people-hidden; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18120; URL=http://127.0.0.1:$PORT
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
has() { python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); sys.exit(0 if any(l["node"]["id"]==sys.argv[2] for l in d["links"]) else 1)' "$W/$1" "$2" 2>/dev/null; }
anyuser() { python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); sys.exit(0 if any(l["node"]["id"].startswith("user:") for l in d["links"]) else 1)' "$W/$1" 2>/dev/null; }
ids() { python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(" ".join(l["node"]["id"] for l in d["links"]))' "$W/$1" 2>/dev/null; }
grp() { post "$1" /communities -d "slug=$2" --data-urlencode "name=$3" -d "category=3" -d "subcategory=x" --data-urlencode "about=$4" ${5:+--data-urlencode "city=$5"} >/dev/null; }
profile() { post "$1" /me/profile --data-urlencode "display_name=$2" --data-urlencode "interests=$3" >/dev/null; }

start
grp $PAL fres1 "Fresno Neighbors" "x" "Fresno, CA"
post $ADMIN /c/fres1/join -d "" >/dev/null; post $STRANGER /c/fres1/join -d "" >/dev/null
for who in $ADMIN $PAL $STRANGER; do post $who /city/fresno-ca/list -d "on=1" >/dev/null; done
grp $STRANGER jamgroup "Friday Jam" "bring an instrument"
profile $ADMIN "Brandon" "jam"
profile $PAL "Pal" "jam"
profile $STRANGER "Stranger" "jam"
# Same setup as the case that showed people before (map_interest_words.sh:71), so an empty result means hidden.
check "precondition: all three listed in Fresno" '[ "$(grep -cP "^clist\tfresno-ca\t" $W/store.lgp)" = 3 ]' "clist=$(grep -c ^clist $W/store.lgp)"
check "precondition: pal and stranger saved the interest jam" 'grep -P "\tpal\t" $W/store.lgp | grep -q -w jam && grep -P "\tstranger\t" $W/store.lgp | grep -q -w jam' "$(grep -w jam $W/store.lgp | cut -c1-80)"

r=$(node $ADMIN int:jam jam.json)
check "int:jam answers 200" '[ "$r" = 200 ]' "$r"
check "jam still grows its group (positive control)" 'has jam.json group:jamgroup' "$(ids jam.json)"
check "jam shows NO people at all" '! anyuser jam.json' "$(ids jam.json)"
check "...not pal (a friend)" '! has jam.json user:pal' "$(ids jam.json)"
check "...not stranger (not a friend)" '! has jam.json user:stranger' "$(ids jam.json)"
check "jam's raw JSON never names user:pal or user:stranger" '! grep -q -E "user:(pal|stranger)\"" $W/jam.json' "$(ids jam.json)"

# people themselves still work elsewhere: pal is Bbottin's friend in the seed
node $ADMIN mine:friends fr.json >/dev/null
check "pal is still under your Friends (people not broken)" 'has fr.json user:pal' "$(ids fr.json)"

echo "passes=$passes fails=$fails"
echo "EXIT=$([ $fails -eq 0 ] && echo 0 || echo 1)"
[ $fails -eq 0 ]
