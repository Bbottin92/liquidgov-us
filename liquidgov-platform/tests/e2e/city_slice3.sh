#!/usr/bin/env bash
# Slice 3: a proposal can name a city. Opt-in people list is unchanged.
# Usage: bash city_slice3.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18083. Never touches the live site.
set -u
BIN=$1; SEED=$2
W=/tmp/lg-city-slice3; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18083; URL=http://127.0.0.1:$PORT
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

start
r=$(post $PAL /communities -d "slug=fres1" -d "name=Fresno Neighbors" -d "category=0" -d "subcategory=Neighbors" -d "about=neighbors" --data-urlencode "city=Fresno, CA")
check "fresno group created" '[[ "$r" == *"/c/fres1" ]]' "$r"

get $PAL /proposals props.html >/dev/null
check "proposal form asks for a city and who could act" 'grep -q "Who could act on this" $W/props.html && grep -q "name=\"city\"" $W/props.html && grep -q "nothing is sent to the city" $W/props.html' "form missing"

r=$(post $PAL /proposals -d "title=Sitewide" -d "body=A banner for the whole site" -d "kind=0")
check "a proposal with no city stays on the full list" '[[ "$r" == *"/proposals" && "$r" != *err* && "$r" != *city=* ]]' "$r"

r=$(post $PAL /proposals -d "title=Ghost" -d "body=Should not open" -d "kind=0" --data-urlencode "city=Nowhere")
check "unknown city is refused" '[[ "$r" == *err=pcity* ]]' "$r"
get $PAL /proposals props.html >/dev/null
check "refused city did not open a proposal" '! grep -q "Ghost" $W/props.html' "ghost exists"

r=$(post $PAL /proposals -d "title=Plant trees" -d "body=More shade in the park" -d "kind=0" --data-urlencode "city=Fresno, CA" --data-urlencode "who=The city council")
check "city proposal opens on that city's list" '[[ "$r" == *"/proposals?city=fresno-ca" && "$r" != *err* ]]' "$r"
get $PAL "/proposals?city=fresno-ca" props.html >/dev/null
check "filter shows the city proposal only" 'grep -q "Plant trees" $W/props.html && grep -q "The city council" $W/props.html && ! grep -q "Sitewide" $W/props.html' "filter wrong"

r=$(get $PAL "/proposals?city=Fresno,%20CA" redir.html)
check "typed city filters by its key" '[[ "$r" == *"city=fresno-ca"* ]]' "$r"

get $PAL /city/fresno-ca city.html >/dev/null
check "city page lists the proposal" 'grep -q "Plant trees" $W/city.html && grep -q "Open a proposal for this city" $W/city.html && ! grep -q "None for this city yet" $W/city.html' "missing on city page"
get $STRANGER /city/fresno-ca city.html >/dev/null
check "another member sees the city proposal" 'grep -q "Plant trees" $W/city.html' "hidden"

get $PAL /proposals props.html >/dev/null
check "the full list still has both proposals" 'grep -q "Plant trees" $W/props.html && grep -q "Sitewide" $W/props.html' "list dropped one"

r=$(post $PAL /officials -d "title=Mayor" --data-urlencode "place=Fresno, CA" -d "open=1" --data-urlencode "options=Ada")
check "officials race still opens" '[[ "$r" == *"/officials" ]]' "$r"
get $PAL /officials off.html >/dev/null
check "officials page shows the race" 'grep -q "Mayor" $W/off.html' "no race"
get $PAL "/proposals?city=fresno-ca" props.html >/dev/null
check "an officials race is not a city proposal" '! grep -q "Mayor" $W/props.html' "race leaked"
check "store has one city row" 'grep -c "^pcity" $W/store.lgp | grep -qx 1' "$(grep "^pcity" $W/store.lgp)"

id=$(grep -o 'name="id" value="[0-9]*"' $W/city.html | head -1 | grep -o '[0-9]*' || true)
get $PAL /city/fresno-ca city.html >/dev/null
id=$(grep -o 'name="id" value="[0-9]*"' $W/city.html | head -1 | grep -o '[0-9]*' || true)
r=$(post $STRANGER /proposals/vote -d "id=$id" -d "choice=0" -d "next=/city/fresno-ca")
check "a vote from the city page stays there" '[[ -n "$id" && "$r" == *"/city/fresno-ca#proposal-"* ]]' "id=$id $r"

stop
start
get $PAL /city/fresno-ca city.html >/dev/null
check "city proposal survives a restart" 'grep -q "Plant trees" $W/city.html && grep -q "The city council" $W/city.html' "lost"
check "server log has no crash lines" '! grep -qiE "abort|segfault|terminate" $W/server.log' "$(tail -3 $W/server.log)"
stop
echo "passes=$passes fails=$fails"
echo "EXIT=$fails"
exit "$fails"
