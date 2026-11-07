#!/usr/bin/env bash
# End-to-end check of slice 1 (city on groups + /city/<key>) against the real server binary,
# on a throwaway store and port 18081. Never touches the live site.
# Usage: bash city_slice1.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
set -u
BIN=$1; SEED=$2
W=/tmp/lg-city-slice1; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18081; URL=http://127.0.0.1:$PORT
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
[ -n "$ADMIN" ] && [ -n "$PAL" ] && [ -n "$STRANGER" ] || { echo "seed gave no sessions"; cat "$W/sessions.txt"; exit 1; }

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
mkgroup() { # session slug name city-or-NONE
    local args=(-d "slug=$2" -d "name=$3" -d "category=0" -d "subcategory=Neighbors" -d "about=about $3")
    [ "$4" != NONE ] && args+=(--data-urlencode "city=$4")
    post "$1" /communities "${args[@]}"
}

start

r=$(mkgroup $PAL fres1 "Fresno Neighbors" "  Fresno,  CA ")
check "create with a city goes to the group" '[[ "$r" == *"/c/fres1" ]]' "$r"
r=$(mkgroup $ADMIN fres2 "Fresno Gardens" "fresno ca")
check "second spelling accepted" '[[ "$r" == *"/c/fres2" ]]' "$r"
r=$(mkgroup $STRANGER plain "Plain Group" NONE)
check "old form shape (no city field) still creates" '[[ "$r" == *"/c/plain" ]]' "$r"
r=$(mkgroup $STRANGER evil "Evil" "<script>alert(1)</script> Town")
check "odd city text accepted" '[[ "$r" == *"/c/evil" ]]' "$r"

r=$(get $STRANGER /communities groups.html)
check "groups page 200" '[[ "$r" == 200* ]]' "$r"
check "cities card links fresno-ca" 'grep -q "href=\"/city/fresno-ca\"" $W/groups.html' "no link"
check "fresno shows 2 groups under the common spelling" 'grep -q "Fresno, CA &middot; 2" $W/groups.html' "$(grep -o "Fresno[^<]*" $W/groups.html | head -3)"
check "typeahead offers Fresno, CA" 'grep -q "<option value=\"Fresno, CA\">" $W/groups.html' "no option"
check "script city is escaped on groups page" '! grep -q "<script>alert(1)" $W/groups.html && grep -q "&lt;script&gt;alert(1)" $W/groups.html' "unescaped"
check "group row shows its city link" 'grep -q "<a class=\"muted\" href=\"/city/fresno-ca\">Fresno, CA</a>" $W/groups.html' "no row link"

r=$(get $STRANGER /city/fresno-ca city.html)
check "city page 200" '[[ "$r" == 200* ]]' "$r"
check "city page lists fres1" 'grep -q "href=\"/c/fres1\"" $W/city.html' "missing"
check "city page lists fres2" 'grep -q "href=\"/c/fres2\"" $W/city.html' "missing"
check "city page leaves out other groups" '! grep -q "href=\"/c/plain\"" $W/city.html && ! grep -q "href=\"/c/evil\"" $W/city.html' "extra group"
check "city page says it is a label" 'grep -q "a label, not a government" $W/city.html' "no copy"
check "city page counts 2 groups and 2 people" 'grep -q "2 groups &middot; 2 people in them" $W/city.html' "$(grep -o "[0-9]* group[^<]*" $W/city.html | head -2)"
check "city page offers start-a-group with the city" 'grep -q "/communities?city=Fresno%2C%20CA" $W/city.html' "no start link"

r=$(get $STRANGER "/city/Fresno,%20CA" x.html)
check "typed city url redirects to its key" '[[ "$r" == 30*"/city/fresno-ca" ]]' "$r"
r=$(get $STRANGER /city/nowhere x.html)
check "unknown city is 404" '[[ "$r" == 404* ]]' "$r"
# /city/ would first lose its slash (site-wide), so this asks for /city itself.
r=$(get $STRANGER /city x.html)
check "empty city goes to groups" '[[ "$r" == 30*"/communities" ]]' "$r"
r=$(get "" /city/fresno-ca x.html)
check "signed-out visitor is not shown the city" '! grep -q "href=\"/c/fres1\"" $W/x.html' "$r"
r=$(get $STRANGER "/city/%3Cscript%3E%20town" x.html)
check "script-y city url redirects to a safe key" '[[ "$r" == 30*"/city/script-town" ]]' "$r"
r=$(get $STRANGER /city/script-alert-1-script-town evil.html)
check "script city page escapes its name" '[[ "$r" == 200* ]] && ! grep -q "<script>alert(1)" $W/evil.html' "$r"

r=$(get $PAL /c/fres1 g.html)
check "group page shows its city link" 'grep -q "<a href=\"/city/fresno-ca\">Fresno, CA</a>" $W/g.html' "no link"
check "owner sees Change city" 'grep -q "Change city" $W/g.html' "no form"
get $STRANGER /c/fres1 g2.html >/dev/null
check "non-owner gets no city form" '! grep -q "/c/fres1/city" $W/g2.html' "form shown"

r=$(post $STRANGER /c/fres1/city --data-urlencode "city=Oakland, CA")
check "non-owner city change refused" '[[ "$r" == *"err=city" ]]' "$r"
get $STRANGER /city/fresno-ca city.html >/dev/null
check "refused change left fres1 in Fresno" 'grep -q "href=\"/c/fres1\"" $W/city.html' "moved"

get $STRANGER /c/plain g3.html >/dev/null
check "owner of a city-less group sees Add a city" 'grep -q "Add a city" $W/g3.html' "no form"
r=$(post $STRANGER /c/plain/city --data-urlencode "city=Fresno, CA")
check "owner adds a city to an old group" '[[ "$r" == *"/c/plain" && "$r" != *err* ]]' "$r"
get $STRANGER /city/fresno-ca city.html >/dev/null
check "it now shows on the city page (3 groups, 3 people)" 'grep -q "href=\"/c/plain\"" $W/city.html && grep -q "3 groups &middot; 3 people" $W/city.html' "$(grep -o "[0-9]* group[^<]*" $W/city.html | head -1)"

r=$(get $STRANGER "/communities?city=Fresno%2C%20CA" pre.html)
check "start form is prefilled and open" 'grep -q "value=\"Fresno, CA\"" $W/pre.html && grep -q "<details open><summary>Start a group" $W/pre.html' "$r"

check "store holds ccity rows" 'grep -c "^ccity" $W/store.lgp | grep -qx 4' "$(grep "^ccity" $W/store.lgp)"

# Restart: the cities must survive a real process boundary.
stop
start
get $STRANGER /city/fresno-ca city.html >/dev/null
check "after restart the city page still lists all 3" 'grep -q "3 groups &middot; 3 people" $W/city.html' "$(grep -o "[0-9]* group[^<]*" $W/city.html | head -1)"

r=$(post $STRANGER /c/plain/city --data-urlencode "city=")
check "owner clears the city" '[[ "$r" == *"/c/plain" && "$r" != *err* ]]' "$r"
get $STRANGER /city/fresno-ca city.html >/dev/null
check "cleared group left the city page" '! grep -q "href=\"/c/plain\"" $W/city.html' "still there"

# curl with no -d sends GET, and GET /c/fres1/delete is not the delete form.
r=$(post $PAL /c/fres1/delete -d "")
get $STRANGER /city/fresno-ca city.html >/dev/null
check "deleted group left the city page" '! grep -q "href=\"/c/fres1\"" $W/city.html && grep -q "href=\"/c/fres2\"" $W/city.html' "$r"

check "server log has no crash lines" '! grep -qiE "abort|segfault|terminate" $W/server.log' "$(tail -3 $W/server.log)"
stop
echo "passes=$passes fails=$fails"
echo "EXIT=$fails"
exit "$fails"
