#!/usr/bin/env bash
# Slice 6b: the interactive map's data (/map/node), its privacy settings
# (/settings/map), the framed right-hand field (Sec-Fetch-Dest: iframe), and the
# single proposal / single post pages it opens.
# Usage: bash map_graph.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Runs the server from liquidgov-platform/ so /assets/lgmap.* resolve.
# Throwaway store, port 18089. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-map-graph; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18089; URL=http://127.0.0.1:$PORT
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
# Seed friendships: Bbottin<->pal, pal<->stranger. Bbottin and stranger are not friends.

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
get()  { curl -s -o "$W/$3" -w '%{http_code} %{redirect_url}' -b "lg_session=$1" "${@:4}" "$URL$2"; }
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{http_code} %{redirect_url}' -b "lg_session=$s" "$@" "$URL$p"; }
node() { curl -s -o "$W/$3" -w '%{http_code}' -b "lg_session=$1" -G --data-urlencode "n=$2" "$URL/map/node"; }
# Does the JSON in $1 list a link to node id $2 (with edge $3 if given)?
has() { python3 - "$W/$1" "$2" "${3:-}" <<'EOF'
import json, sys
d = json.load(open(sys.argv[1]))
hit = [l for l in d["links"] if l["node"]["id"] == sys.argv[2] and (not sys.argv[3] or l["edge"] == sys.argv[3])]
sys.exit(0 if hit else 1)
EOF
}
mapset() { post "$1" /settings/map "${@:2}" >/dev/null; } # pass -d name=1 for each box left on

start
post $PAL /communities -d "slug=fres1" -d "name=Fresno Neighbors" -d "category=0" -d "subcategory=Neighbors" -d "about=x" --data-urlencode "city=Fresno, CA" >/dev/null
post $STRANGER /c/fres1/join -d "" >/dev/null
post $ADMIN /c/fres1/join -d "" >/dev/null
post $ADMIN /communities -d "slug=synth" -d "name=Synth Heads" -d "category=3" -d "subcategory=Music" -d "about=no city" >/dev/null
post $PAL /city/fresno-ca/list -d "on=1" >/dev/null
post $PAL /city/fresno-ca/tag --data-urlencode "tag=Parks" >/dev/null
post $PAL /proposals -d "title=Plant trees" -d "body=shade" -d "kind=0" --data-urlencode "city=Fresno, CA" -d "tag=Parks" >/dev/null
post $PAL /proposals -d "title=Late buses" -d "body=route 1" -d "kind=0" --data-urlencode "city=Fresno, CA" >/dev/null
post $ADMIN /proposals -d "title=Sitewide poll" -d "body=no city" -d "kind=0" >/dev/null
trees=$(awk -F'\t' '$1=="prop" && $5=="Plant trees"{print $2}' "$W/store.lgp")
buses=$(awk -F'\t' '$1=="prop" && $5=="Late buses"{print $2}' "$W/store.lgp")
sitewide=$(awk -F'\t' '$1=="prop" && $5=="Sitewide poll"{print $2}' "$W/store.lgp")
post $ADMIN /proposals/reply -d "id=$trees" --data-urlencode "text=Yes please" >/dev/null
post $PAL /feed -F "text=Public hello from pal" -F "anybody=1" >/dev/null
post $PAL /feed -F "text=Friends-only from pal" >/dev/null
post $ADMIN /feed -F "text=Friends-only from Bbottin" >/dev/null
afonly=$(awk -F'\t' '$1=="post" && $5=="Friends-only from Bbottin"{print $2}' "$W/store.lgp")
pub=$(awk -F'\t' '$1=="post" && $5=="Public hello from pal"{print $2}' "$W/store.lgp")
fonly=$(awk -F'\t' '$1=="post" && $5=="Friends-only from pal"{print $2}' "$W/store.lgp")
check "precondition: ids found" '[ -n "$trees" ] && [ -n "$buses" ] && [ -n "$sitewide" ] && [ -n "$pub" ] && [ -n "$fonly" ]' "t=$trees b=$buses s=$sitewide pub=$pub f=$fonly"

# --- The page and its files
r=$(get $STRANGER /map page.html)
check "/map is 200 with the MAINstream button" '[[ "$r" == 200* ]] && grep -q "id=\"lgm-main\"" $W/page.html && grep -q ">MAINstream<" $W/page.html' "$r"
check "/map loads its script and style" 'grep -q "/assets/lgmap.js" $W/page.html && grep -q "/assets/lgmap.css" $W/page.html' "missing"
check "/map keeps a no-script drawing" 'grep -q "<noscript><div class=\"cmapwrap\"><svg" $W/page.html' "no fallback"
r=$(get $STRANGER /assets/lgmap.js js.txt)
check "lgmap.js is served as javascript" '[[ "$r" == 200* ]] && grep -q "/map/node?n=" $W/js.txt' "$r"
r=$(get $STRANGER /assets/lgmap.css css.txt)
check "lgmap.css is served" '[[ "$r" == 200* ]] && grep -q "lgmap-panel.full" $W/css.txt' "$r"
get $STRANGER /settings settings.html >/dev/null
check "the top menu has Map" 'grep -q "<a href=\"/map\">Map</a>" $W/settings.html' "no menu link"
check "Settings has Map and connections" 'grep -q "Map and connections" $W/settings.html && grep -q "name=\"friends\" value=\"1\">" $W/settings.html' "no section"
check "friends starts unchecked" '! grep -q "name=\"friends\" value=\"1\" checked" $W/settings.html' "checked"

# --- One app (Brandon 2026-11-06): every page offers the same manifest and icon
get $STRANGER "/mail" mailp.html >/dev/null
check "Messages offers the one LiquidGov manifest and icon" 'grep -q "rel=\"manifest\" href=\"/manifest.webmanifest\"" $W/mailp.html && grep -q "apple-touch-icon\" href=\"/apple-touch-icon.png\"" $W/mailp.html && ! grep -q "manifest-mail\|icon-mail" $W/mailp.html' "still separate"
check "no 'You're on Messages' install nudge" '! grep -q "lgmailhint" $W/mailp.html' "nudge shown"
check "the home-screen name is LiquidGov" 'grep -q "apple-mobile-web-app-title\" content=\"LiquidGov\"" $W/mailp.html' "page title"
r=$(get $STRANGER /app/social x.html); check "old Social install page goes to the feed" '[[ "$r" == 30*"/feed" ]]' "$r"
r=$(get $STRANGER /app/civic x.html); check "old Civic install page goes to proposals" '[[ "$r" == 30*"/proposals" ]]' "$r"
r=$(get $STRANGER /app/mail x.html); check "old Messages install page goes to mail" '[[ "$r" == 30*"/mail" ]]' "$r"
r=$(get $STRANGER /install inst.html)
check "Get the app offers one app" '[[ "$r" == 200* ]] && grep -q "One LiquidGov icon" $W/inst.html && ! grep -q "Get Social\|Get Civic\|Get Messages" $W/inst.html' "$r"
r=$(curl -s -o /dev/null -w "%{http_code}" "$URL/manifest-social.webmanifest")
check "old manifests still answer for icons already on phones" '[ "$r" = 200 ]' "$r"

# --- Data: what connects to what
r=$(node $STRANGER root root.json)
check "root: 200" '[ "$r" = 200 ]' "$r"
check "root -> Fresno" 'has root.json city:fresno-ca in' "missing"
check "root -> Sitewide" 'has root.json sitewide in' "missing"
node $STRANGER sitewide sw.json >/dev/null
check "Sitewide -> the city-less poll and city-less group" 'has sw.json prop:$sitewide && has sw.json group:synth' "missing"
check "Sitewide does not hold city proposals" '! has sw.json prop:$trees' "has it"
node $STRANGER city:fresno-ca city.json >/dev/null
check "city -> its group" 'has city.json group:fres1 in' "missing"
check "city -> its department" 'has city.json "dept:fresno-ca|Parks" in' "missing"
check "city -> the untagged proposal" 'has city.json prop:$buses in' "missing"
check "city -> listed person" 'has city.json user:pal lives' "missing"
node $STRANGER "dept:fresno-ca|Parks" dept.json >/dev/null
check "department -> the proposal that touches it" 'has dept.json prop:$trees touches' "missing"
node $STRANGER prop:$trees prop.json >/dev/null
check "proposal -> its author, commenter, department, city" 'has prop.json user:pal wrote && has prop.json user:Bbottin commented && has prop.json "dept:fresno-ca|Parks" touches && has prop.json city:fresno-ca in' "missing"
node $STRANGER group:fres1 group.json >/dev/null
check "group -> its members" 'has group.json user:pal member && has group.json user:stranger member && has group.json user:Bbottin member' "missing"
node $STRANGER user:pal pal.json >/dev/null
check "person -> their group, city, proposals" 'has pal.json group:fres1 member && has pal.json city:fresno-ca lives && has pal.json prop:$trees wrote' "missing"
check "person -> their public post" 'has pal.json post:$pub posted' "missing"
check "a friends-only post shows to a friend (stranger is pal's friend)" 'has pal.json post:$fonly posted' "missing"
node $ADMIN user:pal palA.json >/dev/null
check "Bbottin (also pal's friend) sees it too" 'has palA.json post:$fonly posted' "missing"

r=$(node $STRANGER post:$afonly x.json)
check "a friends-only post is refused to a non-friend (stranger, Bbottin)" '[ -n "$afonly" ] && [ "$r" = 404 ]' "$r"
r=$(get $STRANGER "/post/$afonly" x.html)
check "...and so is its /post page" '[[ "$r" == 404* ]]' "$r"
node $STRANGER user:Bbottin bb.json >/dev/null
check "...and it is not among Bbottin's boxes" '! has bb.json post:$afonly' "listed"

# --- Relevance shading: scored from the viewer's own involvement
rel() { python3 - "$W/$1" "$2" <<'EOF'
import json, sys
d = json.load(open(sys.argv[1]))
for n in [d["node"]] + [l["node"] for l in d["links"]]:
    if n["id"] == sys.argv[2]:
        print(n.get("rel", "none")); break
else:
    print("missing")
EOF
}
node $PAL root prel.json >/dev/null
check "your own city is green (1.00)" '[ "$(rel prel.json city:fresno-ca)" = "1.0" ]' "$(rel prel.json city:fresno-ca)"
check "the Main Stream itself is not scored" '[ "$(python3 -c "import json;print(\"rel\" in json.load(open(\"$W/prel.json\"))[\"node\"])")" = False ]' "scored"
node $PAL sitewide psw.json >/dev/null
check "a group you are not in, with no city, is in the middle-low range" 'python3 -c "import sys;v=float(\"$(rel psw.json group:synth)\");sys.exit(0 if 0.2<=v<=0.5 else 1)"' "$(rel psw.json group:synth)"
node $PAL city:fresno-ca pcity.json >/dev/null
check "your own group is green" '[ "$(rel pcity.json group:fres1)" = "1.0" ]' "$(rel pcity.json group:fres1)"
check "a proposal you wrote is green" '[ "$(rel pcity.json prop:$buses)" = "1.0" ]' "$(rel pcity.json prop:$buses)"
node $STRANGER user:pal srel.json >/dev/null
check "for stranger, pal (a friend) is close (0.9) but not their own" '[ "$(rel srel.json user:pal)" = "0.9" ]' "$(rel srel.json user:pal)"
node $STRANGER city:fresno-ca scity.json >/dev/null
check "relevance differs by viewer: Plant trees is pal's (1.0) but only near stranger" 'v=$(rel scity.json "dept:fresno-ca|Parks"); [ "$v" != "missing" ] && python3 -c "import sys;sys.exit(0 if float(\"$v\")<0.96 else 1)"' "$(rel scity.json "dept:fresno-ca|Parks")"

# --- Friends: your own always; others' only when both opt in
check "stranger sees their own friendship with pal" 'has pal.json user:stranger friends' "missing"
check "stranger does not see pal and Bbottin's friendship (default off)" '! has pal.json user:Bbottin friends' "shown"
mapset $PAL -d show=1 -d groups=1 -d posts=1 -d comments=1 -d friends=1
node $STRANGER user:pal pal2.json >/dev/null
check "only pal opting in is not enough" '! has pal2.json user:Bbottin friends' "shown"
mapset $ADMIN -d show=1 -d groups=1 -d posts=1 -d comments=1 -d friends=1
node $STRANGER user:pal pal3.json >/dev/null
check "both opting in shows it" 'has pal3.json user:Bbottin friends' "missing"
check "the setting was saved as a mapp row" 'grep -q "^mapp	pal	1	1	1	1	1	0$" $W/store.lgp' "$(grep ^mapp $W/store.lgp)"

# --- Each privacy box does what it says
mapset $ADMIN -d show=1 -d posts=1 -d comments=1 -d friends=1   # groups off
node $STRANGER group:fres1 g2.json >/dev/null
check "groups off: Bbottin's membership line is gone" '! has g2.json user:Bbottin && has g2.json user:pal member' "still there"
node $ADMIN group:fres1 g2own.json >/dev/null
check "...but Bbottin still sees their own" 'has g2own.json user:Bbottin member' "hidden from self"
mapset $ADMIN -d show=1 -d groups=1 -d posts=1 -d friends=1     # comments off
node $STRANGER prop:$trees p2.json >/dev/null
check "comments off: commented line is gone" '! has p2.json user:Bbottin commented && has p2.json user:pal wrote' "still there"
mapset $PAL -d show=1 -d groups=1 -d comments=1 -d friends=1    # posts off
node $STRANGER user:pal pal4.json >/dev/null
check "posts off: pal's posts are gone" '! has pal4.json post:$pub' "still there"
r=$(node $STRANGER post:$pub x.json)
check "posts off: the post box itself is refused" '[ "$r" = 404 ]' "$r"
mapset $PAL -d groups=1 -d posts=1 -d comments=1 -d friends=1   # show off
r=$(node $STRANGER user:pal x.json)
check "show off: pal is refused" '[ "$r" = 404 ]' "$r"
node $STRANGER group:fres1 g3.json >/dev/null
node $STRANGER city:fresno-ca c3.json >/dev/null
node $STRANGER prop:$trees p3.json >/dev/null
check "show off: pal vanishes from group, city and proposal" '! has g3.json user:pal && ! has c3.json user:pal && ! has p3.json user:pal' "still there"
r=$(node $PAL user:pal x.json)
check "show off: pal still sees themself" '[ "$r" = 200 ]' "$r"
get $STRANGER /city/fresno-ca/map cmap.html >/dev/null
check "show off also hides pal on the static city map" '! perl -0ne "exit(/<svg class=\"cmap\".*?\/u\/pal\".*?<\/svg>/s ? 0 : 1)" $W/cmap.html' "shown"
mapset $PAL -d show=1 -d groups=1 -d posts=1 -d comments=1 -d friends=1

# --- Blocks
post $ADMIN /friends/block -d "who=stranger" >/dev/null
r=$(node $STRANGER user:Bbottin x.json)
check "a person who blocked you is refused" '[ "$r" = 404 ]' "$r"
node $STRANGER group:fres1 g4.json >/dev/null
check "...and left out of groups" '! has g4.json user:Bbottin' "still there"

# --- Bad input
r=$(node $STRANGER "city:Fresno, CA" x.json); check "non-key city id refused" '[ "$r" = 404 ]' "$r"
r=$(node $STRANGER "prop:abc" x.json); check "non-number proposal id refused" '[ "$r" = 404 ]' "$r"
r=$(node $STRANGER "prop:99999999999999999999999" x.json); check "huge id refused" '[ "$r" = 404 ]' "$r"
r=$(node $STRANGER "dept:fresno-ca|Nope" x.json); check "unknown department refused" '[ "$r" = 404 ]' "$r"
r=$(node $STRANGER "wat" x.json); check "unknown kind refused" '[ "$r" = 404 ]' "$r"
r=$(curl -s -o /dev/null -w '%{http_code}' "$URL/map/node?n=root"); check "signed out gets 401" '[ "$r" = 401 ]' "$r"
check "json is valid even with quotes in names" 'post $PAL /proposals -d "title=Say \"hi\" \\ there" -d "body=x" -d "kind=0" >/dev/null; node $STRANGER sitewide q.json >/dev/null; python3 -c "import json;json.load(open(\"$W/q.json\"))"' "bad json"

# --- The right-hand field: single pages, framed without the header, framing guard
r=$(get $STRANGER "/proposals?id=$trees" one.html)
check "/proposals?id= shows just that proposal" '[[ "$r" == 200* ]] && grep -q "Plant trees" $W/one.html && ! grep -q "Late buses" $W/one.html' "$r"
r=$(get $STRANGER "/proposals?id=999999" x.html); check "unknown proposal id 404" '[[ "$r" == 404* ]]' "$r"
r=$(get $STRANGER "/post/$pub" post.html)
check "/post/<id> shows a public post" '[[ "$r" == 200* ]] && grep -q "Public hello from pal" $W/post.html' "$r"
r=$(get $ADMIN "/post/$fonly" x.html)
check "/post/<id> opens a friends-only post for a friend (Bbottin, pal)" '[[ "$r" == 200* ]] && grep -q "Friends-only from pal" $W/x.html' "$r"
r=$(get $STRANGER "/post/abc" x.html); check "/post/abc 404" '[[ "$r" == 404* ]]' "$r"
curl -s -D "$W/fh.txt" -o "$W/framed.html" -b "lg_session=$STRANGER" -H "Sec-Fetch-Dest: iframe" "$URL/c/fres1"
check "framed page hides the site header" 'grep -q "header.top,footer.site{display:none" $W/framed.html' "not hidden"
check "framed page has no jukebox" '! grep -q "juketube-root" $W/framed.html' "jukebox"
check "framed page may only be framed by liquidgov itself" 'grep -qi "^Content-Security-Policy: frame-ancestors .self." $W/fh.txt && grep -qi "^X-Frame-Options: SAMEORIGIN" $W/fh.txt' "$(cat $W/fh.txt)"
curl -s -D "$W/fr.txt" -o /dev/null -b "lg_session=$STRANGER" -H "Sec-Fetch-Dest: iframe" -d "" "$URL/c/fres1/leave"
check "a framed redirect is guarded too" 'grep -qi "^X-Frame-Options: SAMEORIGIN" $W/fr.txt' "$(cat $W/fr.txt)"
curl -s -D "$W/nh.txt" -o "$W/normal.html" -b "lg_session=$STRANGER" -H "Sec-Fetch-Dest: document" "$URL/c/fres1"
check "a normal page keeps its header and jukebox" '! grep -q "header.top,footer.site{display:none" $W/normal.html && grep -q "juketube-root" $W/normal.html' "changed"
check "a normal page has no frame headers (unchanged)" '! grep -qi "^X-Frame-Options" $W/nh.txt' "added"

check "server log has no crash lines" '! grep -qiE "abort|segfault|terminate called" $W/server.log' "$(tail -3 $W/server.log)"
stop
echo "passes=$passes fails=$fails"
echo "EXIT=$fails"
