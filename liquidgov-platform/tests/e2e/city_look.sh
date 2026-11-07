#!/usr/bin/env bash
# "Look at what I've written" (local only). The button reads this person's
# own posts, proposals, proposal comments, and messages they sent, and
# offers each find as Keep or Forget. Friends posts, group posts, and
# messages stay on their map only: they cannot be Flowed, and other people
# do not see them. It does not read the other person's messages, reposts,
# comments on ordinary posts, or office races. Throwaway store, port 18088.
# Never touches the live site or port 8080.
# Usage: bash city_look.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-city-look; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18088; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
PAL=$(sid pal); STRANGER=$(sid stranger); ADMIN=$(sid Bbottin)
[ -n "$PAL" ] && [ -n "$STRANGER" ] && [ -n "$ADMIN" ] || { echo "seed gave no sessions"; exit 1; }

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
get()  { curl -s -o "$W/$3" -w '%{http_code} %{redirect_url}' -b "lg_session=$1" "$URL$2"; }
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{http_code} %{redirect_url}' -b "lg_session=$s" "$@" "$URL$p"; }

# pain_row <owner> <phrase> [source-prefix]
# exits 0 when that owner has an unkept suggestion with that phrase.
pain_row() {
    python3 - "$W/store.lgp" "$1" "$2" "${3:-}" <<'EOF'
import sys
path, owner, phrase, prefix = sys.argv[1:]
for line in open(path, encoding="utf-8", errors="replace"):
    if not line.startswith("pain\t"):
        continue
    f = line.rstrip("\n").split("\t")
    if len(f) < 8 or f[2] != owner or f[7] != phrase or f[4] != "0":
        continue
    if prefix and not f[6].startswith(prefix):
        continue
    sys.exit(0)
sys.exit(1)
EOF
}
map_has() {
    python3 - "$W/$1" "$2" <<'EOF'
import json, sys
d = json.load(open(sys.argv[1]))
sys.exit(0 if any(l["node"]["id"] == sys.argv[2] and l["edge"] == "noticed" for l in d.get("links", [])) else 1)
EOF
}
map_lacks() {
    python3 - "$W/$1" "$2" <<'EOF'
import json, sys
d = json.load(open(sys.argv[1]))
if "links" not in d:
    sys.exit(1)
sys.exit(0 if not any(l["node"]["id"] == sys.argv[2] for l in d["links"]) else 1)
EOF
}
kept_private() {
    python3 - "$W/store.lgp" "$1" <<'EOF'
import sys
path, pid = sys.argv[1:]
for line in open(path, encoding="utf-8", errors="replace"):
    f = line.rstrip("\n").split("\t")
    if f and f[0] == "pain" and len(f) > 1 and f[1] == pid:
        sys.exit(0 if len(f) == 8 and f[4] == "1" else 1)
sys.exit(1)
EOF
}
pal_count() {
    python3 - "$W/store.lgp" <<'EOF'
import sys
n = 0
for line in open(sys.argv[1], encoding="utf-8", errors="replace"):
    if line.startswith("pain\t") and line.split("\t")[2:3] == ["pal"]:
        n += 1
print(n)
EOF
}
field() { # tag match-field match-value print-field
    python3 - "$W/store.lgp" "$1" "$2" "$3" "$4" <<'EOF'
import sys
path, tag, mf, mv, out = sys.argv[1:]
mf, out = int(mf), int(out)
for line in open(path, encoding="utf-8", errors="replace"):
    f = line.rstrip("\n").split("\t")
    if f and f[0] == tag and len(f) > max(mf, out) and f[mf] == mv:
        print(f[out]); sys.exit(0)
sys.exit(1)
EOF
}

start
post $PAL /communities -d "slug=fres1" -d "name=Fresno Bus Riders" -d "category=0" -d "subcategory=Transit" -d "about=riding the buses" --data-urlencode "city=Fresno, CA" >/dev/null
post $PAL /settings/map -d show=1 -d groups=1 -d posts=1 -d comments=1 >/dev/null
# curl with no -d/-F sends GET. An empty body is still a POST.
r=$(post $PAL /problems/look -d "")
check "a look while the switch is off does not claim the scan was empty" '[[ "$r" == 303* ]] && [[ "$r" != *looked=none* ]]' "$r"
get $PAL /problems off.html >/dev/null
check "the off page has no look button and no empty-scan line" '! grep -q "Look at what I.ve written" $W/off.html && ! grep -q "Nothing new in what you.ve written" $W/off.html && grep -q "turned off" $W/off.html' "off page"
check "switching off left pal with no problem rows" '[ "$(pal_count)" = 0 ]' "$(pal_count)"

# Written while the switch is off, so nothing is noticed until they ask.
post $PAL /feed -F "text=I wish the library stayed open later" >/dev/null
post $PAL /c/fres1 -F "text=I hate when the buses skip my stop" >/dev/null
post $PAL /mail -F "to=stranger" -F "text=I hate when the sidewalks flood" >/dev/null
post $PAL /feed -F "text=I really wish the buses ran later on weekends. Anyway!" -F "anybody=1" >/dev/null
post $PAL /feed -F "text=I want to die" -F "anybody=1" >/dev/null
post $PAL /proposals -d "title=Shade" -d "body=I'm so tired of the heat at bus stops" -d "kind=0" >/dev/null
post $PAL /proposals -d "title=Weekend routes" -d "body=Route 1 only" -d "kind=0" >/dev/null
pid=$(field prop 4 "Weekend routes" 1) || { echo "no bland proposal"; exit 1; }
post $PAL /proposals/reply -d "id=$pid" --data-urlencode "text=I hate when the parks close early" >/dev/null
buspost=$(field post 4 "I really wish the buses ran later on weekends. Anyway!" 1) || { echo "no public post"; exit 1; }
post $PAL "/post/$buspost/reply" --data-urlencode "text=I wish the crosswalks had more time" >/dev/null
post $PAL /officials --data-urlencode "title=I wish the mayor listened" --data-urlencode "place=Fresno, CA" -d "open=1" --data-urlencode "options=Ada" >/dev/null
post $STRANGER /feed -F "text=I wish the night buses ran more often" -F "anybody=1" >/dev/null
night=$(field post 4 "I wish the night buses ran more often" 1) || { echo "no stranger post"; exit 1; }
post $PAL /me/repost -d "post_id=$night" >/dev/null

check "precondition: the office race was stored" 'field prop 4 "I wish the mayor listened" 1 >/dev/null' "missing"
check "while off, pal's old public words are not noticed" '[ "$(pal_count)" = 0 ]' "$(pal_count)"

post $PAL /settings/map -d show=1 -d groups=1 -d posts=1 -d comments=1 -d pains=1 >/dev/null
check "turning the switch back on does not scan by itself" '[ "$(pal_count)" = 0 ]' "$(pal_count)"
get $PAL /problems before.html >/dev/null
check "the button is on the problems page" 'grep -q "Look at what I.ve written" $W/before.html && ! grep -q "Nothing new in what you.ve written" $W/before.html' "missing"
check "settings names the button" 'get $PAL /settings set.html >/dev/null; grep -q "Look at what I.ve written" $W/set.html && grep -q "operator can read" $W/set.html' "missing"

r=$(post $PAL /problems/look -d "")
check "a look that finds something does not use the empty-scan redirect" '[[ "$r" == 303* ]] && [[ "$r" != *looked=none* ]]' "$r"
get $PAL /problems found.html >/dev/null
check "the old public post is a suggestion, not a kept box" 'pain_row pal "the buses ran later on weekends" post:' "store"
check "the old proposal is a suggestion" 'pain_row pal "the heat at bus stops" prop:' "store"
check "the old proposal comment is a suggestion" 'pain_row pal "the parks close early" prply:' "store"
check "the page offers Keep and Forget for the public post" 'grep -q "the buses ran later on weekends" $W/found.html && grep -q ">Keep<" $W/found.html && grep -q ">Forget<" $W/found.html' "page"
check "a crisis post shows 988 and stores nothing from that post" 'grep -q "tel:988" $W/found.html && ! pain_row pal "want to die"' "crisis"
check "a comment on a post is not read" '! pain_row pal "the crosswalks had more time"' "leaked"
check "an office race is not read" '! pain_row pal "the mayor listened"' "leaked"
check "a repost of someone else's post is not read" '! pain_row pal "the night buses ran more often"' "leaked"
check "the empty-scan line is absent when something was found" '! grep -q "Nothing new in what you.ve written" $W/found.html' "empty line"
check "a friends-only post is a private suggestion" 'pain_row pal "the library stayed open later" fpost:' "store"
check "a group post is a private suggestion" 'pain_row pal "the buses skip my stop" gpost:' "store"
check "a message pal sent is a private suggestion" 'pain_row pal "the sidewalks flood" mail:' "store"
check "pal has the three public finds plus three private ones" '[ "$(pal_count)" = 6 ]' "$(pal_count)"
get $STRANGER /problems stranger.html >/dev/null
check "pal's finds stay off stranger's page" '! grep -q "the heat at bus stops" $W/stranger.html && ! grep -q "the library stayed open later" $W/stranger.html && ! grep -q "the sidewalks flood" $W/stranger.html' "leaked"

lib=$(python3 - "$W/store.lgp" <<'EOF'
import sys
for line in open(sys.argv[1], encoding="utf-8", errors="replace"):
    f = line.rstrip("\n").split("\t")
    if f and f[0] == "pain" and len(f) >= 8 and f[2] == "pal" and f[7] == "the library stayed open later":
        print(f[1]); raise SystemExit
raise SystemExit(1)
EOF
)
post $PAL /problems/keep -d "id=$lib" >/dev/null
curl -s -o "$W/palmap.json" -b "lg_session=$PAL" -G --data-urlencode "n=user:pal" "$URL/map/node" >/dev/null
curl -s -o "$W/strangermap.json" -b "lg_session=$STRANGER" -G --data-urlencode "n=user:pal" "$URL/map/node" >/dev/null
check "pal's own map shows the kept friends-post subject" 'map_has palmap.json "pain:$lib"' "missing"
check "stranger's map of pal hides that subject" 'map_lacks strangermap.json "pain:$lib"' "leaked"
r=$(get $STRANGER "/pain/$lib" pain.html)
check "stranger cannot open the private subject" '[[ "$r" == 404* ]] && ! grep -q "the library stayed open later" $W/pain.html' "$r"
post $PAL /problems/flow -d "id=$lib" >/dev/null
check "Flow does not publish a friends-post subject" 'kept_private "$lib"' "flowed"
get $PAL /problems kept.html >/dev/null
check "the kept private subject says it stays on your map" 'grep -q "This stays on your map. Other people do not see it." $W/kept.html' "missing"

post $STRANGER /mail -F "to=pal" -F "text=I wish the trains ran all night" >/dev/null
check "the other person's message is not read" '! pain_row pal "the trains ran all night"' "leaked"

before=$(pal_count)
r=$(post $PAL /problems/look -d "")
check "a second look redirects to the empty-scan note" '[[ "$r" == *looked=none* ]]' "$r"
get $PAL "/problems?looked=none" none.html >/dev/null
check "the page says nothing new" 'grep -q "Nothing new in what you.ve written" $W/none.html' "missing"
check "looking again still shows 988, because that post is read again and still stored as nothing" 'grep -q "tel:988" $W/none.html' "missing"
check "the second look adds no rows" '[ "$(pal_count)" = "$before" ]' "$(pal_count)"

post $PAL /feed -F "text=I wish the pool opened earlier" -F "anybody=1" >/dev/null
check "a new public post is still noticed without the button" 'pain_row pal "the pool opened earlier" post:' "not noticed"
after=$(pal_count)
r=$(post $PAL /problems/look -d "")
check "looking again does not duplicate that new post" '[[ "$r" == *looked=none* ]] && [ "$(pal_count)" = "$after" ]' "$(pal_count) $r"

r=$(curl -s -o /dev/null -w '%{http_code} %{redirect_url}' -X POST "$URL/problems/look")
check "signed-out look goes to login" '[[ "$r" == 303* ]] && [[ "$r" == *login* ]]' "$r"
check "server log has no crash lines" '! grep -qiE "abort|segfault|terminate called" $W/server.log' "$(tail -3 $W/server.log)"
stop
echo "passes=$passes fails=$fails"
echo "EXIT=$fails"
