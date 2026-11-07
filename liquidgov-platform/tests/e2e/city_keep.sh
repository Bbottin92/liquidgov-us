#!/usr/bin/env bash
# "Keep it" (Brandon 2026-11-06): a city's proposals and department/council tags
# survive the city's last group leaving, and show again once a group names the city.
# Usage: bash city_keep.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18086. Never touches the live site.
set -u
BIN=$1; SEED=$2
W=/tmp/lg-city-keep; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18086; URL=http://127.0.0.1:$PORT
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
r=$(post $PAL /communities -d "slug=fres1" -d "name=Fresno Neighbors" -d "category=0" -d "subcategory=Neighbors" -d "about=x" --data-urlencode "city=Fresno, CA")
check "fresno group created" '[[ "$r" == *"/c/fres1" ]]' "$r"
r=$(post $PAL /city/fresno-ca/tag --data-urlencode "tag=Parks")
check "a department is added" '[[ "$r" != *err* ]]' "$r"
r=$(post $PAL /proposals -d "title=Plant trees" -d "body=More shade" -d "kind=0" --data-urlencode "city=Fresno, CA" -d "tag=Parks")
check "a tagged city proposal opens" '[[ "$r" == *"city=fresno-ca"* && "$r" != *err* ]]' "$r"

# The owner re-spells the city: the old key has no group left.
r=$(post $PAL /c/fres1/city --data-urlencode "city=Fresno, California")
check "owner re-spells the city" '[[ "$r" == *"/c/fres1" && "$r" != *err* ]]' "$r"
check "store keeps the proposal city" 'grep -q "^pcity	[0-9]*	fresno-ca$" $W/store.lgp' "$(grep ^pcity $W/store.lgp)"
check "store keeps the department" 'grep -q "^ctag	fresno-ca	Parks$" $W/store.lgp' "$(grep ^ctag $W/store.lgp)"
check "store keeps the proposal tag" 'grep -q "^ptag	[0-9]*	Parks$" $W/store.lgp' "$(grep ^ptag $W/store.lgp)"

r=$(get $STRANGER /city/fresno-ca empty.html)
check "the groupless city page is 200, not 404" '[[ "$r" == 200* ]]' "$r"
check "it points to the kept proposal" 'grep -q "1 proposal is still about this city" $W/empty.html && grep -q "/proposals?city=fresno-ca" $W/empty.html' "no pointer"
get $STRANGER "/proposals?city=fresno-ca" props.html >/dev/null
check "the city filter still lists it" 'grep -q "Plant trees" $W/props.html' "missing"
check "the proposal still names its city and tag" 'grep -q "/city/fresno-ca" $W/props.html && grep -q "Parks" $W/props.html' "missing"

# Restart, then a group names the old spelling again.
stop; start
check "after restart the store still has them" 'grep -q "^pcity" $W/store.lgp && grep -q "^ctag	fresno-ca	Parks$" $W/store.lgp' "gone"
r=$(post $STRANGER /communities -d "slug=fres2" -d "name=Fresno Again" -d "category=0" -d "subcategory=Neighbors" -d "about=y" --data-urlencode "city=fresno ca")
check "a new group names the city again" '[[ "$r" == *"/c/fres2" ]]' "$r"
r=$(get $STRANGER /city/fresno-ca city.html)
check "the city page is back" '[[ "$r" == 200* ]] && grep -q "href=\"/c/fres2\"" $W/city.html' "$r"
check "with the kept proposal and its department" 'grep -q "Plant trees" $W/city.html && grep -q "<h3>Parks</h3>" $W/city.html' "missing"

r=$(get $STRANGER /city/nowhere x.html)
check "a city with nothing at all is still 404" '[[ "$r" == 404* ]]' "$r"
check "server log has no crash lines" '! grep -qiE "abort|segfault|terminate" $W/server.log' "$(tail -3 $W/server.log)"
stop
echo "passes=$passes fails=$fails"
echo "EXIT=$fails"
