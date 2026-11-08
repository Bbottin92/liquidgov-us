#!/usr/bin/env bash
# Operator moderation of city departments/councils (open item "moderation gap left: delete city departments/councils",
# Brandon OK'd 2026-11-08). /admin/departments lists every (city, department) with how many proposals name it; delete
# archives it to <store>.moderation.jsonl first (refused if the archive can't be written), removes it from the city and
# from every proposal that named it, and logs delete_city_tag. Same name in another city is untouched. Survives a
# restart. Also: /favicon.svg is cacheable (same deploy). Must FAIL on the previous binary (no /admin/departments).
# Usage: bash city_moderation.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18098. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
W=/tmp/lg-city-moderation; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18098; URL=http://127.0.0.1:$PORT
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
    env LIQUIDGOV_OPERATORS=Bbottin LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT \
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
ARCH="$W/store.lgp.moderation.jsonl"
ctags() { grep -c "^ctag" "$W/store.lgp"; }
ptags() { grep -c "^ptag" "$W/store.lgp"; }
has_ctag() { awk -F'\t' -v k="$1" -v t="$2" '$1=="ctag" && $2==k && $3==t {f=1} END{exit !f}' "$W/store.lgp"; }

start
# Two cities, each with a group (only members can name departments); Fresno has three, Clovis has its own "Parks".
post $PAL /communities -d "slug=fres1" -d "name=Fresno Neighbors" -d "category=0" -d "subcategory=Neighbors" -d "about=n" --data-urlencode "city=Fresno, CA" >/dev/null
post $PAL /communities -d "slug=clov1" -d "name=Clovis Neighbors" -d "category=0" -d "subcategory=Neighbors" -d "about=n" --data-urlencode "city=Clovis, CA" >/dev/null
for t in Parks Council Transit; do post $PAL /city/fresno-ca/tag --data-urlencode "tag=$t" >/dev/null; done
post $PAL /city/clovis-ca/tag --data-urlencode "tag=Parks" >/dev/null
post $PAL /proposals -d "title=Plant trees" -d "body=Shade" -d "kind=0" --data-urlencode "city=Fresno, CA" -d "tag=Parks" -d "tag=Council" >/dev/null
post $PAL /proposals -d "title=Fix benches" -d "body=Broken" -d "kind=0" --data-urlencode "city=Fresno, CA" -d "tag=Parks" >/dev/null
pa=$(awk -F'\t' '$1=="prop" && $5=="Plant trees"{print $2}' "$W/store.lgp")
pb=$(awk -F'\t' '$1=="prop" && $5=="Fix benches"{print $2}' "$W/store.lgp")
check "precondition: 4 departments (3 Fresno, 1 Clovis), 3 proposal links, two proposals" \
    '[ "$(ctags)" = 4 ] && [ "$(ptags)" = 3 ] && [ -n "$pa" ] && [ -n "$pb" ]' "ctag=$(ctags) ptag=$(ptags) pa=$pa pb=$pb"

# Non-operators see nothing and change nothing.
r=$(get $PAL /admin/departments na.html); check "a non-operator gets 404 on the page" '[[ "$r" == 404* ]]' "$r"
r=$(post $PAL /admin/departments/delete --data-urlencode "city=fresno-ca" -d "tag=Parks"); check "a non-operator's delete is 404" '[[ "$r" == 404* ]]' "$r"
check "...and changed nothing" '[ "$(ctags)" = 4 ]' "$(ctags)"

# The operator list.
r=$(get $ADMIN /admin/departments ad.html)
check "the operator page lists Fresno's Parks with its 2 proposals" 'grep -q "<td>Parks</td><td>2</td>" $W/ad.html && grep -q "fresno-ca" $W/ad.html' "$r"
check "...and the Departments tab is in the operator nav" 'grep -q "href=\"/admin/departments\"" $W/ad.html' "no tab"
r=$(get $ADMIN "/admin/departments?q=clovis" aq.html)
check "search by city shows only that city" 'grep -q "clovis-ca" $W/aq.html && ! grep -q "Transit" $W/aq.html' "$r"

# An unwritable archive refuses the delete.
mkdir -p "$ARCH"
r=$(post $ADMIN /admin/departments/delete --data-urlencode "city=fresno-ca" -d "tag=Parks")
check "an unwritable archive refuses the delete" '[[ "$r" == *archive-failed* ]] && [ "$(ctags)" = 4 ]' "$r ctag=$(ctags)"
rmdir "$ARCH"

# The real delete.
r=$(post $ADMIN /admin/departments/delete --data-urlencode "city=fresno-ca" -d "tag=Parks")
check "the delete redirects with done=deleted" '[[ "$r" == *done=deleted* ]]' "$r"
check "Fresno's Parks is gone" '! has_ctag fresno-ca Parks' "$(grep ^ctag $W/store.lgp)"
check "Fresno's Council and Transit stay" 'has_ctag fresno-ca Council && has_ctag fresno-ca Transit' "$(grep ^ctag $W/store.lgp)"
check "Clovis's own Parks stays" 'has_ctag clovis-ca Parks' "$(grep ^ctag $W/store.lgp)"
check "both proposals dropped Parks; Plant trees keeps Council" \
    '[ "$(ptags)" = 1 ] && awk -F"\t" -v p="$pa" "\$1==\"ptag\" && \$2==p && \$3==\"Council\" {f=1} END{exit !f}" $W/store.lgp' "$(grep ^ptag $W/store.lgp)"
check "the proposals themselves are untouched" '[ -n "$(awk -F"\t" "\$1==\"prop\" && \$2==\"$pb\"" $W/store.lgp)" ]' "proposal gone"
get $PAL /city/fresno-ca city.html >/dev/null
check "the city page no longer lists Parks but still lists Council" '! grep -q "<li>Parks</li>" $W/city.html && grep -q "<li>Council</li>" $W/city.html' "$(grep -o "<li>[^<]*</li>" $W/city.html | head)"
check "the archive has it, who deleted it, and the two proposals" \
    'grep -q "\"city_tag_city\":\"fresno-ca\",\"city_tag\":\"Parks\",\"proposals\":\[$pa,$pb\]" "$ARCH" && grep -q "\"deleted_by\":\"Bbottin\"" "$ARCH"' "$(cat "$ARCH" 2>/dev/null)"
get $ADMIN /admin/log log.html >/dev/null
check "the audit log has delete_city_tag" 'grep -q "delete_city_tag" $W/log.html' "missing"
r=$(post $ADMIN /admin/departments/delete --data-urlencode "city=fresno-ca" -d "tag=Parks")
check "deleting it again changes nothing" '[[ "$r" == *done=none* ]] && [ "$(wc -l < "$ARCH")" = 1 ]' "$r lines=$(wc -l < "$ARCH")"
r=$(post $ADMIN /admin/departments/delete --data-urlencode "city=nowhere" -d "tag=Parks")
check "an unknown city changes nothing" '[[ "$r" == *done=none* ]]' "$r"

# A restart keeps the result (a real lifecycle boundary, not just the same process).
stop; start
check "after a restart Parks is still gone from Fresno and Clovis keeps its own" '! has_ctag fresno-ca Parks && has_ctag clovis-ca Parks && [ "$(ctags)" = 3 ] && [ "$(ptags)" = 1 ]' "ctag=$(ctags) ptag=$(ptags)"
get $PAL /city/fresno-ca city2.html >/dev/null
check "...and the city page agrees" '! grep -q "<li>Parks</li>" $W/city2.html && grep -q "<li>Council</li>" $W/city2.html' "page"

# Same deploy: the favicon can be cached.
h=$(curl -s -D - -o /dev/null "$URL/favicon.svg" | tr -d '\r' | grep -i "^cache-control:")
check "/favicon.svg sends a cache header" '[[ "$h" == *max-age=86400* ]]' "$h"

stop
echo "passes=$passes fails=$fails"
[ $fails -eq 0 ] && echo "EXIT=0" || echo "EXIT=1"
[ $fails -eq 0 ]
