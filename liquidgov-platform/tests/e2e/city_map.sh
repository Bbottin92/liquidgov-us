#!/usr/bin/env bash
# Slice 6: the map view. /map (MAINstream -> each city's Current -> groups and
# proposals, plus Sitewide) and /city/<key>/map (people here | groups | city |
# departments | proposals, with lines). Only opted-in people; blocks respected.
# Usage: bash city_map.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18088. Never touches the live site.
set -u
BIN=$1; SEED=$2
W=/tmp/lg-city-map; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18088; URL=http://127.0.0.1:$PORT
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
    env -u LIQUIDGOV_OPERATORS LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT \
        "$BIN" >> "$W/server.log" 2>&1 &
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
get()  { curl -s -o "$W/$3" -w '%{http_code} %{redirect_url}' -b "lg_session=$1" "$URL$2"; }
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{http_code} %{redirect_url}' -b "lg_session=$s" "$@" "$URL$p"; }
# Just the map's own <svg> (the page header has its own links and icons).
svgonly() { perl -0ne 'print $1 if /(<svg class="cmap".*?<\/svg>)/s' "$W/$1" > "$W/$2"; }
# Count lines (edges) of one kind between two node ids, either direction.
edge() { grep -oE "class=\"ce ce-$2\" data-a=\"[^\"]*\" data-b=\"[^\"]*\"" "$W/$1" | grep -cE "data-a=\"$3\" data-b=\"$4\"|data-a=\"$4\" data-b=\"$3\""; }
# Node id of the box whose title is exactly $2 (matched literally).
nid() { T="$2" perl -ne 'while (/data-n="([^"]*)"><title>(.*?)<\/title>/g) { if ($2 eq $ENV{T}) { print "$1\n"; exit } }' "$W/$1"; }

start
post $PAL /communities -d "slug=fres1" -d "name=Fresno Neighbors" -d "category=0" -d "subcategory=Neighbors" -d "about=x" --data-urlencode "city=Fresno, CA" >/dev/null
post $ADMIN /communities -d "slug=fres2" --data-urlencode "name=<b>Bold</b> Gardens" -d "category=0" -d "subcategory=Gardens" -d "about=y" --data-urlencode "city=fresno ca" >/dev/null
post $STRANGER /communities -d "slug=oak1" -d "name=Oakland Bikes" -d "category=0" -d "subcategory=Transit" -d "about=z" --data-urlencode "city=Oakland, CA" >/dev/null
post $STRANGER /c/fres1/join -d "" >/dev/null
post $PAL /city/fresno-ca/list -d "on=1" >/dev/null
post $ADMIN /city/fresno-ca/list -d "on=1" >/dev/null
post $PAL /city/fresno-ca/tag --data-urlencode "tag=Parks" >/dev/null
post $PAL /city/fresno-ca/tag --data-urlencode "tag=Council" >/dev/null
post $PAL /proposals -d "title=Plant trees" -d "body=shade" -d "kind=0" --data-urlencode "city=Fresno, CA" -d "tag=Parks" >/dev/null
post $ADMIN /proposals -d "title=Late buses" -d "body=route 1" -d "kind=0" --data-urlencode "city=Fresno, CA" >/dev/null
post $PAL /proposals -d "title=Sitewide poll" -d "body=no city" -d "kind=0" >/dev/null
trees=$(awk -F'\t' '$1=="prop" && $5=="Plant trees"{print $2}' "$W/store.lgp")
buses=$(awk -F'\t' '$1=="prop" && $5=="Late buses"{print $2}' "$W/store.lgp")
post $ADMIN /proposals/reply -d "id=$trees" --data-urlencode "text=Yes" >/dev/null
post $PAL /proposals/react -d "id=$buses" -d "kind=support" >/dev/null
post $STRANGER /proposals/reply -d "id=$buses" --data-urlencode "text=unlisted commenter" >/dev/null
check "precondition: two people listed, stranger not" 'grep -c "^clist	fresno-ca	" $W/store.lgp | grep -qx 2' "$(grep ^clist $W/store.lgp)"

# MAINstream
r=$(get $STRANGER /map ms.html)
check "/map is 200" '[[ "$r" == 200* ]]' "$r"
check "MAINstream root box" 'grep -q "<title>MAINstream</title>" $W/ms.html' "no root"
check "Fresno branch opens the Fresno map" 'grep -q "href=\"/city/fresno-ca/map\"" $W/ms.html' "no link"
check "Oakland branch too" 'grep -q "href=\"/city/oakland-ca/map\"" $W/ms.html' "no link"
check "Sitewide branch holds the city-less poll" 'grep -q "<title>Proposals not about any one city</title>" $W/ms.html && grep -q "<title>Proposal: Sitewide poll</title>" $W/ms.html' "missing"
check "a Fresno proposal leaf" 'grep -q "<title>Proposal: Plant trees</title>" $W/ms.html' "missing"
check "group names are escaped" '! grep -q "<b>Bold</b>" $W/ms.html && grep -q "&lt;b&gt;Bold&lt;/b&gt; Gardens" $W/ms.html' "unescaped"
svgonly ms.html msv.html
check "precondition: the MAINstream svg was found" '[ -s $W/msv.html ]' "empty"
check "no people on the MAINstream" '! grep -q "/u/" $W/msv.html' "people shown"
froot=$(nid msv.html "Fresno, CA&#39;s Current"); [ -z "$froot" ] && froot=$(nid msv.html "Fresno, CA's Current")
check "root connects to Fresno" '[ -n "$froot" ] && [ "$(edge msv.html in root $froot)" = 1 ]' "id=$froot"

# City map
r=$(get $STRANGER /city/fresno-ca/map cm.html)
svgonly cm.html cmv.html
check "precondition: the city map svg was found" '[ -s $W/cmv.html ]' "empty"
check "/city/fresno-ca/map is 200" '[[ "$r" == 200* ]]' "$r"
check "breadcrumb back to MAINstream" 'grep -q "<a href=\"/map\">MAINstream</a> &rsaquo; Fresno, CA" $W/cm.html' "no crumb"
check "both Fresno groups, not Oakland" 'grep -q "href=\"/c/fres1\"" $W/cm.html && grep -q "href=\"/c/fres2\"" $W/cm.html && ! grep -q "/c/oak1" $W/cm.html' "groups"
check "listed people shown" 'grep -q "href=\"/u/pal\"" $W/cmv.html && grep -q "href=\"/u/Bbottin\"" $W/cmv.html' "people"
check "unlisted member not shown" '! grep -q "href=\"/u/stranger\"" $W/cmv.html' "stranger shown"
check "departments shown" 'grep -q "<title>Parks</title>" $W/cmv.html && grep -q "<title>Council</title>" $W/cmv.html' "depts"
u_pal=$(nid cmv.html pal); u_admin=$(nid cmv.html Bbottin)
g1=$(nid cmv.html "Fresno Neighbors (/fres1)"); dp=$(nid cmv.html Parks)
p_trees=$(nid cmv.html "Proposal $trees: Plant trees"); p_buses=$(nid cmv.html "Proposal $buses: Late buses")
echo "ids: pal=$u_pal admin=$u_admin g1=$g1 parks=$dp trees=$p_trees buses=$p_buses"
check "precondition: every node id was found" '[ -n "$u_pal" ] && [ -n "$u_admin" ] && [ -n "$g1" ] && [ -n "$dp" ] && [ -n "$p_trees" ] && [ -n "$p_buses" ]' "missing id"
check "pal member of Fresno Neighbors" '[ "$(edge cmv.html member $u_pal $g1)" = 1 ]' "no edge"
check "no member line where there is no membership (Bbottin never joined fres1)" '[ "$(edge cmv.html member $u_admin $g1)" = 0 ]' "unexpected"
check "Plant trees touches Parks" '[ "$(edge cmv.html touches $dp $p_trees)" = 1 ]' "no edge"
check "untagged Late buses hangs off the city" '[ "$(edge cmv.html in c $p_buses)" = 1 ]' "no edge"
check "pal wrote Plant trees" '[ "$(edge cmv.html wrote $u_pal $p_trees)" = 1 ]' "no edge"
check "Bbottin commented on Plant trees" '[ "$(edge cmv.html commented $u_admin $p_trees)" = 1 ]' "no edge"
check "Bbottin wrote Late buses" '[ "$(edge cmv.html wrote $u_admin $p_buses)" = 1 ]' "no edge"
check "precondition: pal's reaction is stored" 'grep -q "^prx	$buses	pal	support" $W/store.lgp' "no prx"
check "who reacted is not drawn (reactions are counts only)" '! grep -q "class=\"ce ce-reacted\"" $W/cmv.html && [ "$(edge cmv.html "[a-z]*" $u_pal $p_buses)" = 0 ]' "reaction line drawn"
check "map has a legend and the opt-in note" 'grep -q "cmaplegend" $W/cm.html && grep -q "Only people who chose to be listed" $W/cm.html' "no legend"
check "hover script present" 'grep -q "classList.add(.focus.)" $W/cm.html' "no script"

# Blocks: Bbottin blocks stranger, stranger no longer sees Bbottin on the map.
post $ADMIN /friends/block -d "who=stranger" >/dev/null
get $STRANGER /city/fresno-ca/map cm2.html >/dev/null
svgonly cm2.html cm2v.html
check "a person who blocked you is left off" '[ -s $W/cm2v.html ] && ! grep -q "href=\"/u/Bbottin\"" $W/cm2v.html && grep -q "href=\"/u/pal\"" $W/cm2v.html' "still shown"
get $PAL /city/fresno-ca/map cm3.html >/dev/null
svgonly cm3.html cm3v.html
check "people not involved in the block still see Bbottin" 'grep -q "href=\"/u/Bbottin\"" $W/cm3v.html' "hidden from pal"

# Links in and out
get $STRANGER /city/fresno-ca city.html >/dev/null
check "city page links its map and the MAINstream" 'grep -q "href=\"/city/fresno-ca/map\">Map view" $W/city.html && grep -q "href=\"/map\">MAINstream" $W/city.html' "no links"
get $STRANGER /communities groups.html >/dev/null
check "Groups page links the MAINstream map" 'grep -q "href=\"/map\">See them on the MAINstream map" $W/groups.html' "no link"

# Edge cases
r=$(get $STRANGER "/city/Fresno,%20CA/map" x.html)
check "typed city map url redirects to its key" '[[ "$r" == 30*"/city/fresno-ca/map" ]]' "$r"
r=$(get $STRANGER /city/nowhere/map x.html)
check "unknown city map is 404" '[[ "$r" == 404* ]]' "$r"
r=$(get "" /map x.html)
check "signed-out visitor gets no map" '! grep -q "MAINstream</title>" $W/x.html' "$r"
r=$(get "" /city/fresno-ca/map x.html)
check "signed-out visitor gets no city map" '! grep -q "/u/pal" $W/x.html' "$r"
check "server log has no crash lines" '! grep -qiE "abort|segfault|terminate called" $W/server.log' "$(tail -3 $W/server.log)"
stop
echo "passes=$passes fails=$fails"
echo "EXIT=$fails"
