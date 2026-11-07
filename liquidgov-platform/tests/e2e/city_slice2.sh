#!/usr/bin/env bash
# Slice 2: People here on /city/<key>. Opt-in, per city, with Add friend.
# Usage: bash city_slice2.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18082. Never touches the live site.
set -u
BIN=$1; SEED=$2
W=/tmp/lg-city-slice2; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18082; URL=http://127.0.0.1:$PORT
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
# The page is one line and the header links to the viewer's own /u/<name>.
# The People card is the text after its h2 and before "Start a group".
card() { awk 'BEGIN{RS="<h2>People here</h2>"} NR==2{split($0,a,"Start a group"); printf "%s", a[1]}' "$1"; }

start
r=$(post $PAL /communities -d "slug=fres1" -d "name=Fresno Neighbors" -d "category=0" -d "subcategory=Neighbors" -d "about=neighbors" --data-urlencode "city=Fresno, CA")
check "fresno group created" '[[ "$r" == *"/c/fres1" ]]' "$r"
r=$(post $ADMIN /communities -d "slug=oak1" -d "name=Oakland Bikes" -d "category=0" -d "subcategory=Neighbors" -d "about=bikes" --data-urlencode "city=Oakland, CA")
check "oakland group created" '[[ "$r" == *"/c/oak1" ]]' "$r"

get $PAL /city/fresno-ca city.html >/dev/null
check "people section is on the city page" 'grep -q "People here" $W/city.html' "no heading"
check "starter is not listed by default" 'c=$(card $W/city.html); [[ "$c" == *"Nobody has chosen to be listed yet"* && "$c" != *"/u/pal"* ]]' "pal listed"

r=$(post $PAL /city/fresno-ca/list -d "on=1")
check "opt in stays on the city page" '[[ "$r" == *"/city/fresno-ca" && "$r" != *err* ]]' "$r"
get $PAL /city/fresno-ca city.html >/dev/null
check "opted-in person is listed" 'grep -q "href=\"/u/pal\"" $W/city.html && grep -q ">you<" $W/city.html' "missing"
check "store has one clist row" 'grep -c "^clist" $W/store.lgp | grep -qx 1' "$(grep "^clist" $W/store.lgp)"

get $STRANGER /city/fresno-ca city.html >/dev/null
check "a friend sees Friends, not Add friend" 'grep -q "Friends" $W/city.html && ! grep -q "Add friend" $W/city.html' "wrong action"
check "stranger is still unlisted" 'c=$(card $W/city.html); [[ "$c" != *"/u/stranger"* ]]' "stranger listed"

r=$(post $ADMIN /c/fres1/join -d "")
check "admin joins fresno" '[[ "$r" == *"/c/fres1" ]]' "$r"
r=$(post $ADMIN /city/fresno-ca/list -d "on=1")
check "admin opts in" '[[ "$r" == *"/city/fresno-ca" && "$r" != *err* ]]' "$r"
get $STRANGER /city/fresno-ca city.html >/dev/null
check "stranger gets Add friend for someone they do not know" 'grep -q "Add friend" $W/city.html && grep -q "name=\"who\" value=\"Bbottin\"" $W/city.html' "no button"

r=$(post $PAL /city/oakland-ca/list -d "on=1")
check "cannot list in a city you have not joined" '[[ "$r" == *err=list* ]]' "$r"
get $PAL /city/oakland-ca oak.html >/dev/null
check "oakland does not show the fresno listing" 'c=$(card $W/oak.html); [[ "$c" == *"Nobody has chosen to be listed yet"* && "$c" != *"/u/"* ]]' "pal on oakland"

r=$(post $STRANGER /c/fres1/join -d "")
check "stranger joins fresno" '[[ "$r" == *"/c/fres1" ]]' "$r"
get $PAL /city/fresno-ca city.html >/dev/null
check "joining does not list you" '! grep -q "href=\"/u/stranger\"" $W/city.html' "listed on join"
r=$(post $STRANGER /city/fresno-ca/list -d "on=1")
check "stranger opts in" '[[ "$r" == *"/city/fresno-ca" && "$r" != *err* ]]' "$r"
get $PAL /city/fresno-ca city.html >/dev/null
check "stranger is listed only after opting in" 'grep -q "href=\"/u/stranger\"" $W/city.html' "missing"

r=$(post $STRANGER /c/fres1/leave -d "")
check "stranger leaves" '[[ "$r" == *"/c/fres1" ]]' "$r"
get $PAL /city/fresno-ca city.html >/dev/null
check "leaving drops you from the list" '! grep -q "href=\"/u/stranger\"" $W/city.html && grep -q "href=\"/u/pal\"" $W/city.html' "still listed"

stop
start
get $PAL /city/fresno-ca city.html >/dev/null
check "listing survives a restart" 'grep -q "href=\"/u/pal\"" $W/city.html && grep -q "href=\"/u/Bbottin\"" $W/city.html' "lost"
check "server log has no crash lines" '! grep -qiE "abort|segfault|terminate" $W/server.log' "$(tail -3 $W/server.log)"
stop
echo "passes=$passes fails=$fails"
echo "EXIT=$fails"
exit "$fails"
