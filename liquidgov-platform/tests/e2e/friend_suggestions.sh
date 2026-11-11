#!/usr/bin/env bash
# "People you may know" (Brandon 2026-11-10: "you shouldnt have to know someones specific handle. you should be able
# to see suggestions before even typing anything in. like people from groups youre in, or from the same town").
# His answers: only people whose findability is Typeahead; friends in common BY NAME from map-visible friendships; on
# /friends, on home (where signup lands) and an Add button on a group's member list. Requests get a face, a link and
# Accept/Decline. Never suggests yourself, friends, pending requests or blocked people.
# Usage: bash friend_suggestions.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18102. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-friend-suggest; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18102; URL=http://127.0.0.1:$PORT
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
# Seed friendships: Bbottin<->pal, pal<->stranger. So for Bbottin, stranger is a friend of a friend.

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
get()  { curl -s -o "$W/$3" -w '%{http_code}' -b "lg_session=$1" "$URL$2"; }
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{http_code} %{redirect_url}' -b "lg_session=$s" "$@" "$URL$p"; }
# the "People you may know" block of a saved page, and one person's row in it
block() { python3 - "$W/$1" <<'EOF'
import re, sys
t = open(sys.argv[1], encoding="utf-8").read()
m = re.search(r'id="suggest".*?<!--/suggest-->', t, re.S)
print(m.group(0) if m else "")
EOF
}
row() { python3 - "$W/$1" "$2" <<'EOF'
import re, sys
t = open(sys.argv[1], encoding="utf-8").read()
m = re.search(r'<div class="person suggest" data-user="%s">.*?<!--/row-->' % re.escape(sys.argv[2]), t, re.S)
print(m.group(0) if m else "")
EOF
}

start
r=$(get $ADMIN /friends f0.html)
check "precondition: /friends is 200 for Bbottin" '[ "$r" = 200 ]' "$r"
check "precondition: Bbottin and pal are friends, stranger is not yet" 'grep -q "href=\"/u/pal\"" $W/f0.html' "pal missing"

# --- the one-time switch (Brandon 2026-11-10): map "friends" is ON by default; everyone who never saved Map
# settings gets that default written down and a note, once. (The seed's three people never saved them.)
mapp_friends() { awk -F'\t' -v u="$1" '$1=="mapp" && $2==u {print $7}' "$W/store.lgp"; }
check "the switch wrote friends-on for people who never saved Map settings" '[ "$(mapp_friends Bbottin)" = 1 ] && [ "$(mapp_friends pal)" = 1 ] && [ "$(mapp_friends stranger)" = 1 ]' "$(grep '^mapp' $W/store.lgp)"
get $ADMIN /notices n0.html >/dev/null
check "... and told them, once" '[ "$(grep -o "Your friends now show on the map" $W/n0.html | wc -l)" = 1 ]' "$(grep -c 'now show' $W/n0.html)"
stop; start
get $ADMIN /notices n1.html >/dev/null
check "a restart does not tell them again" '[ "$(grep -o "Your friends now show on the map" $W/n1.html | wc -l)" = 1 ]' "$(grep -o 'now show' $W/n1.html | wc -l)"

# --- friend of a friend, named like other social sites ("Friends with pal"), only through a friendship both
# friends leave visible on the map
get $ADMIN /friends f0.html >/dev/null
B=$(block f0.html); S=$(row f0.html stranger)
check "/friends shows People you may know" '[ -n "$B" ] && grep -q "People you may know" <<<"$B"' "no block"
check "stranger (a friend of a friend, visible on the map) is suggested" '[ -n "$S" ]' "no row"
check "... naming the friend in common" 'grep -q "Friends with pal" <<<"$S"' "$S"
check "... with a face and a link to the profile" 'grep -q "class=\"portrait" <<<"$S" && grep -q "href=\"/u/stranger\"" <<<"$S"' "$S"
check "... and a one-tap Add friend" 'grep -q "action=\"/friends\"" <<<"$S" && grep -q "name=\"who\" value=\"stranger\"" <<<"$S" && grep -q ">Add friend<" <<<"$S"' "$S"
check "a friend is not suggested" '[ -z "$(row f0.html pal)" ]' "pal suggested"
check "you are not suggested to yourself" '[ -z "$(row f0.html Bbottin)" ]' "self suggested"
# pal hides friends on the map (everything else left on): the friendship stops counting
post $PAL /settings/map -d "show=1" -d "groups=1" -d "posts=1" -d "comments=1" -d "pains=1" >/dev/null
get $ADMIN /friends fh.html >/dev/null
check "a friendship hidden on the map suggests nobody" '[ -z "$(row fh.html stranger)" ]' "$(row fh.html stranger)"
post $PAL /settings/map -d "show=1" -d "groups=1" -d "posts=1" -d "comments=1" -d "pains=1" -d "friends=1" >/dev/null

# --- a shared group and the same town add reasons
post $PAL /communities -d "slug=tgarden" -d "name=Tower Garden" -d "category=0" -d "subcategory=Neighbors" -d "about=x" --data-urlencode "city=Fresno, CA" >/dev/null
post $ADMIN /c/tgarden/join -d "" >/dev/null
post $STRANGER /c/tgarden/join -d "" >/dev/null
post $ADMIN /city/fresno-ca/list -d "on=1" >/dev/null
post $STRANGER /city/fresno-ca/list -d "on=1" >/dev/null
get $ADMIN /friends f1.html >/dev/null; S=$(row f1.html stranger)
check "a shared group is a reason" 'grep -q "In Tower Garden with you" <<<"$S"' "$S"
check "the same town is a reason" 'grep -q "Also in Fresno" <<<"$S"' "$S"
check "the shared group comes first (strongest)" 'python3 -c "import sys; s=sys.argv[1]; sys.exit(0 if 0 <= s.find(\"Tower Garden\") < s.find(\"Friends with\") else 1)" "$S"' "$S"

# --- home (where signup lands) and a group's member list
get $ADMIN / home.html >/dev/null
check "home shows People you may know with stranger" '[ -n "$(row home.html stranger)" ]' "not on home"
get $ADMIN /c/tgarden grp.html >/dev/null
check "a group's member list offers Add friend for a non-friend" 'grep -q "data-add=\"stranger\"" $W/grp.html' "no add"
check "... not for a friend or yourself" '! grep -q "data-add=\"pal\"" $W/grp.html && ! grep -q "data-add=\"Bbottin\"" $W/grp.html' "extra add"

# --- sending a request: the suggestion goes, the outgoing list shows a face and a link
r=$(post $ADMIN /friends -d "who=stranger" -d "next=/friends")
get $ADMIN /friends f2.html >/dev/null
check "after Add, stranger is no longer suggested" '[ -z "$(row f2.html stranger)" ]' "still suggested"
check "Outgoing shows stranger with a face and a link" 'python3 -c "import re,sys; t=open(sys.argv[1]).read(); m=re.search(r\"<h2>Outgoing</h2>.*?</div>\s*</div>\", t, re.S); s=m.group(0) if m else \"\"; sys.exit(0 if \"/u/stranger\" in s and \"portrait\" in s else 1)" $W/f2.html' "plain name"
get $STRANGER /friends s1.html >/dev/null
check "Incoming shows Bbottin with a face, a link, Accept and Decline" 'grep -q "action=\"/friends/accept\"" $W/s1.html && grep -q "action=\"/friends/decline\"" $W/s1.html && grep -q "href=\"/u/Bbottin\"" $W/s1.html' "missing parts"
r=$(post $STRANGER /friends/decline -d "who=Bbottin")
check "Decline redirects back" '[[ "$r" == 30[23]* ]]' "$r"
get $STRANGER /friends s2.html >/dev/null
check "after Decline the request is gone for both" '! grep -q "action=\"/friends/accept\"" $W/s2.html && ! grep -q "name=\"who\" value=\"Bbottin\"" <<<"$(python3 -c "import re,sys; t=open(sys.argv[1]).read(); m=re.search(r\"<h2>Incoming</h2>.*?<h2>\", t, re.S); print(m.group(0) if m else \"\")" $W/s2.html)"' "still there"
check "the store no longer holds the request" '! awk -F"\t" "\$1==\"r\" && \$2==\"Bbottin\" && \$3==\"stranger\"" $W/store.lgp | grep -q .' "$(grep -n stranger $W/store.lgp | head -3)"

# --- findability and blocks
post $STRANGER /settings/privacy -d "findability=1" >/dev/null
get $ADMIN /friends f3.html >/dev/null
check "someone findable only by exact name is not suggested" '[ -z "$(row f3.html stranger)" ]' "suggested"
post $STRANGER /settings/privacy -d "findability=2" >/dev/null
get $ADMIN /friends f4.html >/dev/null
check "precondition: findable again, suggested again" '[ -n "$(row f4.html stranger)" ]' "not back"
post $STRANGER /friends/block -d "who=Bbottin" >/dev/null
get $ADMIN /friends f5.html >/dev/null
check "a person who blocked you is not suggested" '[ -z "$(row f5.html stranger)" ]' "suggested"

echo "friend_suggestions: $passes passed, $fails failed"
[ $fails -eq 0 ]
