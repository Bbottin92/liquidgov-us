#!/usr/bin/env bash
# Slice 5: comment on a proposal, and one reaction per person.
# Usage: bash city_slice5.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18085. Never touches the live site.
set -u
BIN=$1; SEED=$2
W=/tmp/lg-city-slice5; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18085; URL=http://127.0.0.1:$PORT
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
r=$(post $PAL /proposals -d "title=Plant trees" -d "body=More shade" -d "kind=0" --data-urlencode "city=Fresno, CA")
check "a city proposal opens" '[[ "$r" == *"/proposals?city=fresno-ca" && "$r" != *err* ]]' "$r"

get $PAL "/proposals?city=fresno-ca" props.html >/dev/null
pid=$(grep -o 'name="id" value="[0-9]*"' "$W/props.html" | head -1 | grep -o '[0-9]*')
check "the proposal has an id" '[ -n "$pid" ]' "no id"
check "a signed-in member gets a comment box" 'grep -q "Write a comment" $W/props.html && grep -q "A reaction is not a vote." $W/props.html' "no talk"

r=$(post $STRANGER /proposals/reply -d "id=999999" --data-urlencode "text=nowhere" --data-urlencode "next=/proposals?city=fresno-ca")
check "a missing proposal is not commented" '[[ "$r" == *"/proposals" && "$r" != *err* ]]' "$r"
check "no comment row yet" 'grep -c "^prply" $W/store.lgp | grep -qx 0' "$(grep "^prply" $W/store.lgp || true)"

r=$(post $STRANGER /proposals/reply -d "id=$pid" --data-urlencode "text=More shade on the street" --data-urlencode "next=/proposals?city=fresno-ca")
check "a comment is accepted" '[[ "$r" == *"/proposals?city=fresno-ca#proposal-"* && "$r" != *err* ]]' "$r"
r=$(post $STRANGER /proposals/reply -d "id=$pid" --data-urlencode "text=   " --data-urlencode "next=/proposals?city=fresno-ca")
check "a blank comment stays on the list" '[[ "$r" == *"/proposals?city=fresno-ca#proposal-"* ]]' "$r"
check "one comment row" 'grep -c "^prply" $W/store.lgp | grep -qx 1' "$(grep "^prply" $W/store.lgp)"

get $ADMIN "/proposals?city=fresno-ca" admin.html >/dev/null
check "another member sees the comment" 'grep -q "More shade on the street" $W/admin.html' "hidden"
get $PAL /city/fresno-ca city.html >/dev/null
check "the city page shows the comment" 'grep -q "More shade on the street" $W/city.html' "city"

r=$(post $PAL /proposals/react -d "id=$pid" -d "kind=support" --data-urlencode "next=/proposals?city=fresno-ca")
check "a reaction is accepted" '[[ "$r" == *"/proposals?city=fresno-ca#proposal-"* ]]' "$r"
check "one support row" 'grep -c "^prx" $W/store.lgp | grep -qx 1 && grep -q "^prx	$pid	pal	support$" $W/store.lgp' "$(grep "^prx" $W/store.lgp)"
r=$(post $PAL /proposals/react -d "id=$pid" -d "kind=support" --data-urlencode "next=/proposals?city=fresno-ca")
check "the same reaction again clears it" '[[ "$r" == *"/proposals?city=fresno-ca#proposal-"* ]]' "$r"
check "the reaction row is gone" 'grep -c "^prx" $W/store.lgp | grep -qx 0' "$(grep "^prx" $W/store.lgp || true)"

r=$(post $PAL /proposals/react -d "id=$pid" -d "kind=oppose" --data-urlencode "next=/proposals?city=fresno-ca")
r=$(post $PAL /proposals/react -d "id=$pid" -d "kind=love" --data-urlencode "next=/proposals?city=fresno-ca")
r=$(post $STRANGER /proposals/react -d "id=$pid" -d "kind=support" --data-urlencode "next=/proposals?city=fresno-ca")
check "a bad kind does not replace oppose" 'grep -q "^prx	$pid	pal	oppose$" $W/store.lgp' "$(grep "^prx" $W/store.lgp)"
check "two people, two rows" 'grep -c "^prx" $W/store.lgp | grep -qx 2' "$(grep "^prx" $W/store.lgp)"
get $PAL "/proposals?city=fresno-ca" props.html >/dev/null
check "counts show on the proposal" 'grep -q "Oppose · 1" $W/props.html && grep -q "Support · 1" $W/props.html' "counts"

r=$(post $PAL /officials -d "title=Mayor" --data-urlencode "place=Fresno, CA" --data-urlencode "options=Ada" -d "open=1")
check "a race opens" '[[ "$r" == *"/officials" && "$r" != *err* ]]' "$r"
get $PAL /officials off.html >/dev/null
check "the officials page shows the race" 'grep -q "<h2>Mayor</h2>" $W/off.html' "no race"
oid=$(awk 'BEGIN{RS="<h2>"} /^Mayor/{ if (match($0, /name="id" value="[0-9]+"/)) { s=substr($0,RSTART,RLENGTH); gsub(/[^0-9]/,"",s); print s; exit } }' "$W/off.html")
check "the race has an id" '[ -n "$oid" ]' "no id"
r=$(post $STRANGER /proposals/reply -d "id=$oid" --data-urlencode "text=Ask about the parks budget" --data-urlencode "next=/officials")
check "a race comment stays on officials" '[[ "$r" == *"/officials#proposal-"* ]]' "$r"
get $ADMIN /officials off.html >/dev/null
check "another member sees the race comment" 'grep -q "Ask about the parks budget" $W/off.html && grep -q "A reaction is not a vote." $W/off.html' "hidden"

stop
start
get $ADMIN "/proposals?city=fresno-ca" admin.html >/dev/null
check "comment and reactions survive a restart" 'grep -q "More shade on the street" $W/admin.html && grep -q "Oppose · 1" $W/admin.html && grep -q "Support · 1" $W/admin.html' "lost"
check "server log has no crash lines" '! grep -qiE "abort|segfault|terminate" $W/server.log' "$(tail -3 $W/server.log)"
stop
echo "passes=$passes fails=$fails"
echo "EXIT=$fails"
exit "$fails"
