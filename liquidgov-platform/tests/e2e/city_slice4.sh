#!/usr/bin/env bash
# Slice 4: departments and councils on a city, picked by a proposal.
# Usage: bash city_slice4.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18084. Never touches the live site.
set -u
BIN=$1; SEED=$2
W=/tmp/lg-city-slice4; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18084; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }

$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
ADMIN=$(sid Bbottin); PAL=$(sid pal); STRANGER=$(sid stranger)
echo "sessions: admin=${ADMIN:+yes} pal=${PAL:+yes} stranger=${STRANGER:+yes}"
[ -n "$ADMIN" ] && [ -n "$PAL" ] && [ -n "$STRANGER" ] || { echo "seed gave no sessions"; exit 1; }

SERVER=""
start() {
    env -u LIQUIDGOV_OPERATORS LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT \
        "$BIN" >> "$W/server.log" 2>&1 &
    SERVER=$!
    for i in $(seq 50); do
        kill -0 $SERVER 2>/dev/null || { echo "server died"; cat "$W/server.log"; exit 1; }
        curl -s -o /dev/null --max-time 1 $URL/ && return
        sleep 0.2
    done
    echo "server did not start"; cat "$W/server.log"; kill $SERVER; exit 1
}
stop() { [ -n "$SERVER" ] && kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null; SERVER=""; }
trap stop EXIT
get()  { curl -s -o "$W/$3" -w '%{http_code} %{redirect_url}' -b "lg_session=$1" "$URL$2"; }
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{http_code} %{redirect_url}' -b "lg_session=$s" "$@" "$URL$p"; }
# One-line pages: the text after a heading and before the next h3.
after_h3() { awk -v h="$2" 'BEGIN{RS="<h3>" h "</h3>"} NR==2{s=$0; if (match(s, /<h3>[[:alpha:]]/)) s=substr(s, 1, RSTART-1); printf "%s", s}' "$1"; }

start
r=$(post $PAL /communities -d "slug=fres1" -d "name=Fresno Neighbors" -d "category=0" -d "subcategory=Neighbors" -d "about=neighbors" --data-urlencode "city=Fresno, CA")
check "fresno group created" '[[ "$r" == *"/c/fres1" ]]' "$r"

r=$(post $STRANGER /city/fresno-ca/tag --data-urlencode "tag=Parks")
check "a non-member cannot add a department" '[[ "$r" == *err=ctag* ]]' "$r"

r=$(post $PAL /city/fresno-ca/tag -d "tag=   ")
check "a blank department is refused" '[[ "$r" == *err=ctagname* ]]' "$r"

r=$(post $PAL /city/fresno-ca/tag --data-urlencode "tag=Parks")
check "a member adds a department" '[[ "$r" == *"/city/fresno-ca" && "$r" != *err* ]]' "$r"
r=$(post $PAL /city/fresno-ca/tag --data-urlencode "tag=parks")
check "a case duplicate stays on the city page" '[[ "$r" == *"/city/fresno-ca" && "$r" != *err* ]]' "$r"
get $PAL /city/fresno-ca city.html >/dev/null
check "the first spelling is listed once" '[ "$(grep -o "<li>Parks</li>" $W/city.html | wc -l | tr -d " ")" = 1 ]' "count"
check "store has one department row" 'grep -c "^ctag" $W/store.lgp | grep -qx 1' "$(grep "^ctag" $W/store.lgp)"

r=$(post $PAL /city/fresno-ca/tag --data-urlencode "tag=Council")
check "a second department is added" '[[ "$r" == *"/city/fresno-ca" && "$r" != *err* ]]' "$r"
check "store has two department rows" 'grep -c "^ctag" $W/store.lgp | grep -qx 2' "$(grep "^ctag" $W/store.lgp)"

get $PAL "/proposals?city=fresno-ca" props.html >/dev/null
check "the proposal form offers this city's departments" 'grep -q "name=\"tag\" value=\"Parks\"" $W/props.html && grep -q "name=\"tag\" value=\"Council\"" $W/props.html' "no boxes"

r=$(post $PAL /proposals -d "title=Plant trees" -d "body=More shade" -d "kind=0" --data-urlencode "city=Fresno, CA" -d "tag=Parks" -d "tag=Nope")
check "a tagged proposal opens on the city list" '[[ "$r" == *"/proposals?city=fresno-ca" && "$r" != *err* ]]' "$r"
r=$(post $PAL /proposals -d "title=Benches" -d "body=More seats" -d "kind=0" --data-urlencode "city=Fresno, CA")
check "an untagged city proposal opens" '[[ "$r" == *"/proposals?city=fresno-ca" && "$r" != *err* ]]' "$r"

get $PAL /city/fresno-ca city.html >/dev/null
check "Parks groups only the proposal that picked it" 'c=$(after_h3 $W/city.html "Parks"); [[ "$c" == *"Plant trees"* && "$c" != *"Benches"* ]]' "grouping"
check "an untagged proposal is in its own group" 'c=$(after_h3 $W/city.html "Not tagged"); [[ "$c" == *"Benches"* && "$c" != *"Plant trees"* ]]' "untagged"
check "a tag the city does not have is not a heading" '! grep -q "<h3>Nope</h3>" $W/city.html' "Nope"
check "store kept only the real tag on the proposal" 'grep -c "^ptag" $W/store.lgp | grep -qx 1' "$(grep "^ptag" $W/store.lgp)"

stop
start
get $PAL /city/fresno-ca city.html >/dev/null
check "departments and grouping survive a restart" 'grep -q "<li>Parks</li>" $W/city.html && grep -q "<li>Council</li>" $W/city.html && grep -q "<h3>Parks</h3>" $W/city.html && grep -q "Plant trees" $W/city.html' "lost"
check "server log has no crash lines" '! grep -qiE "abort|segfault|terminate" $W/server.log' "$(tail -3 $W/server.log)"
stop
echo "passes=$passes fails=$fails"
echo "EXIT=$fails"
exit "$fails"
