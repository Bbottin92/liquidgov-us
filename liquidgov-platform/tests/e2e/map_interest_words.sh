#!/usr/bin/env bash
# Interest words on the map, the cases Grok Bot's check of wip/32 found (scratch/2026-11-11-grokbot-map-phase1-check/
# CHECK.md, should-fix 2 and 3). Matching is by whole words; a simple plural is the same word, but:
#   "news" must not match "New" (news is not the plural of new), "jam" must not match "James";
#   "Kids’" typed with the iPhone's curly apostrophe still means "kids"; "Café" matches "CAFÉ" (case of accented letters).
# People under an interest also drop out when they are findable only by friends, or blocked (both ways round).
# Usage: bash map_interest_words.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18104. Never touches the live site. New file: map_interests_feeds.sh is committed.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-map-interest-words; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18104; URL=http://127.0.0.1:$PORT
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
ids() { python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(" ".join(l["node"]["id"] for l in d["links"]))' "$W/$1" 2>/dev/null; }
grp() { post "$1" /communities -d "slug=$2" --data-urlencode "name=$3" -d "category=3" -d "subcategory=x" --data-urlencode "about=$4" ${5:+--data-urlencode "city=$5"} >/dev/null; }
profile() { post "$1" /me/profile --data-urlencode "display_name=$2" --data-urlencode "interests=$3" >/dev/null; }

start
grp $PAL fres1 "Fresno Neighbors" "x" "Fresno, CA"
post $ADMIN /c/fres1/join -d "" >/dev/null; post $STRANGER /c/fres1/join -d "" >/dev/null
for who in $ADMIN $PAL $STRANGER; do post $who /city/fresno-ca/list -d "on=1" >/dev/null; done
grp $STRANGER jamgroup "Friday Jam" "bring an instrument"
grp $STRANGER jamesgang "James Gang" "fans of the band"
grp $STRANGER fresnonews "Fresno News" "local reporting"
grp $STRANGER kidsports "Little League" "Sports for kids every Saturday"
grp $STRANGER cafecrew "CAFÉ CREW" "coffee people"
post $PAL /proposals -d "title=New park benches" -d "body=wood" -d "kind=0" --data-urlencode "city=Fresno, CA" >/dev/null
benches=$(awk -F'\t' '$1=="prop" && $5=="New park benches"{print $2}' "$W/store.lgp")
profile $ADMIN "Brandon" "jam, news, Kids’ sports, Café"
profile $PAL "Pal" "jam"
profile $STRANGER "Stranger" "Jam"
check "precondition: everyone is listed in Fresno and the proposal is stored" '[ "$(grep -cP "^clist\tfresno-ca\t" $W/store.lgp)" = 3 ] && [ -n "$benches" ]' "clist=$(grep -c ^clist $W/store.lgp) benches=$benches"

r=$(node $ADMIN mine:interests int.json)
check "precondition: Interests -> jam, news, kids-sports, café" '[ "$r" = 200 ] && has int.json int:jam && has int.json int:news && has int.json int:kids-sports' "$r $(ids int.json)"
cafe=$(python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(next((l["node"]["id"] for l in d["links"] if l["node"]["label"]=="Café"), ""))' "$W/int.json")
check "precondition: Café has a box" '[ -n "$cafe" ]' "$(ids int.json)"

node $ADMIN int:jam jam.json >/dev/null
check "jam -> Friday Jam" 'has jam.json group:jamgroup' "$(ids jam.json)"
check "jam does NOT match James Gang" '! has jam.json group:jamesgang' "$(ids jam.json)"
check "jam -> pal and stranger (both list jam, in Fresno, findable)" 'has jam.json user:pal && has jam.json user:stranger' "$(ids jam.json)"
node $ADMIN int:news news.json >/dev/null
check "news -> Fresno News" 'has news.json group:fresnonews' "$(ids news.json)"
check "news does NOT match 'New park benches'" '! has news.json prop:$benches' "$(ids news.json)"
node $ADMIN int:kids-sports kids.json >/dev/null
check "Kids’ sports (curly apostrophe) -> 'Sports for kids'" 'has kids.json group:kidsports' "$(ids kids.json)"
[ -n "$cafe" ] && node $ADMIN "$cafe" cafe.json >/dev/null
check "Café -> CAFÉ CREW (accented capitals)" '[ -n "$cafe" ] && has cafe.json group:cafecrew' "$(ids cafe.json 2>/dev/null)"

# findable only by friends: pal is Bbottin's friend, stranger is not
post $STRANGER /settings/privacy -d "findability=0" >/dev/null
node $ADMIN int:jam jam2.json >/dev/null
check "stranger findable only by friends -> not under jam" '! has jam2.json user:stranger' "$(ids jam2.json)"
check "...pal still is (positive control)" 'has jam2.json user:pal' "$(ids jam2.json)"
post $STRANGER /settings/privacy -d "findability=2" >/dev/null
node $ADMIN int:jam jam3.json >/dev/null
check "stranger findable again -> back under jam" 'has jam3.json user:stranger' "$(ids jam3.json)"
# blocks, both ways
post $ADMIN /friends/block -d "who=stranger" >/dev/null
node $ADMIN int:jam jam4.json >/dev/null
check "after Bbottin blocks stranger, stranger is not under jam" '! has jam4.json user:stranger && has jam4.json user:pal' "$(ids jam4.json)"
post $PAL /friends/block -d "who=Bbottin" >/dev/null
node $ADMIN int:jam jam5.json >/dev/null
check "after pal blocks Bbottin, pal is not under Bbottin's jam" '! has jam5.json user:pal' "$(ids jam5.json)"

echo "passes=$passes fails=$fails"
echo "EXIT=$([ $fails -eq 0 ] && echo 0 || echo 1)"
[ $fails -eq 0 ]
