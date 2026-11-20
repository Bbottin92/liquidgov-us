#!/usr/bin/env bash
# Feed filters, slice 3: Near me (Brandon 2026-11-20: "Groups in my city"; wip/162).
#   /feed?filter=near shows posts in groups whose city is the city I live in (my own profile's "Where you live now",
#   matched the way /city/<key> matches: "fresno ca" = "Fresno, CA"), whether or not I am a member, newest first.
#   Groups in other cities or with no city, and posts outside groups, are not shown. Posts by someone I blocked or
#   who blocked me are left out. With no city on my profile the tab says how to add one.
# Usage: bash feed_near.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18148. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-feed-near; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18148; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
# seed: Bbottin<->pal and pal<->stranger are friends
ME=$(sid Bbottin); PAL=$(sid pal); STR=$(sid stranger)
[ -n "$ME" ] && [ -n "$PAL" ] && [ -n "$STR" ] || { echo "seed gave no sessions"; exit 1; }

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
get()  { curl -s -o "$W/$3" -w '%{http_code}' -b "lg_session=$1" "$URL$2"; }
tabs()  { python3 -c 'import re,sys; m=re.search(r"<div class=\"segtabs\">(.*?)</div>", open(sys.argv[1]).read(), re.S); print(m.group(1) if m else "")' "$W/$1"; }
order() { python3 -c 'import re,sys; print(" ".join(re.findall(r"<p>(F-[a-z0-9-]+)</p>", open(sys.argv[1]).read())))' "$W/$1"; }

start
post $STR /communities -d "slug=fres&name=Fresno+Bikes&category=10&subcategory=Bikes&city=Fresno%2C+CA" >/dev/null
post $STR /communities -d "slug=sf&name=SF+Bikes&category=10&subcategory=Bikes&city=San+Francisco" >/dev/null
post $STR /communities -d "slug=nocity&name=Anywhere&category=10&subcategory=Bikes" >/dev/null
post $PAL /c/fres/join -d "" >/dev/null
post $STR /c/fres -F "text=F-fres-one" >/dev/null
post $PAL /c/fres -F "text=F-fres-pal" >/dev/null
post $STR /c/sf -F "text=F-sf" >/dev/null
post $STR /c/nocity -F "text=F-none" >/dev/null
post $PAL /feed -F "text=F-pal-plain" >/dev/null

r=$(get $ME "/feed?filter=near" n0.html)
check "Near me answers 200" '[ "$r" = 200 ]' "$r"
check "Near me is a tab, and the current one" 'tabs n0.html | grep -q "aria-current=\"page\" href=\"/feed?filter=near\">Near me<"' "$(tabs n0.html)"
check "no city on my profile: no posts, and it says where to add one" \
    '[ -z "$(order n0.html)" ] && grep -q "Near me needs the city you live in" "$W/n0.html" && grep -q "href=\"/me\"" "$W/n0.html"' "$(order n0.html)"

post $ME /me/profile -d "display_name=Brandon&living=fresno+ca" >/dev/null
r=$(get $ME "/feed?filter=near" n1.html)
check "my city written differently still matches: the Fresno group's posts, newest first, though I am not a member" \
    '[ "$(order n1.html)" = "F-fres-pal F-fres-one" ]' "$(order n1.html)"
check "... and it says which city" 'grep -q "Posts in groups in fresno ca" "$W/n1.html"' "missing"
check "other cities, groups with no city and posts outside groups are left out" \
    '! grep -qE "F-sf|F-none|F-pal-plain" "$W/n1.html"' "$(order n1.html)"

post $ME /me/profile -d "display_name=Brandon&living=Boise" >/dev/null
r=$(get $ME "/feed?filter=near" n2.html)
check "a city with no groups: no posts, and it says so" \
    '[ -z "$(order n2.html)" ] && grep -q "No group posts in Boise yet" "$W/n2.html"' "$(order n2.html)"

post $ME /me/profile -d "display_name=Brandon&living=Fresno%2C+CA" >/dev/null
post $PAL /settings/block -d "who=Bbottin" >/dev/null
r=$(get $ME "/feed?filter=near" n3.html)
check "someone who blocked me: their post is left out" '[ "$(order n3.html)" = "F-fres-one" ]' "$(order n3.html)"
post $ME /settings/block -d "who=stranger" >/dev/null
r=$(get $ME "/feed?filter=near" n4.html)
check "someone I blocked: their post is left out" '[ -z "$(order n4.html)" ]' "$(order n4.html)"

r=$(curl -s -o /dev/null -w '%{http_code}' "$URL/feed?filter=near")
check "signed out, the Near me link asks you to log in" '[ "$r" = 302 ] || [ "$r" = 303 ]' "$r"

echo "passes=$passes fails=$fails"
echo "EXIT=$([ $fails -eq 0 ] && echo 0 || echo 1)"
[ $fails -eq 0 ]
