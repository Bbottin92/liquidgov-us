#!/usr/bin/env bash
# M1.1 step 1 (goals.md; Brandon 2026-11-10 "you would see "You" or your profile handle at the base, and from there
# youd see it branch off like groups, friends, events, politics, interests"; Grok Bot map review 2026-11-11 top pick):
# the signed-in map starts at a "you" box whose first ring is kinds of connection (Friends, Groups, Places, Politics,
# Problems), each branching into your own items. Derived at read time: no new record type. Interests and Events come
# later (no interest boxes or events exist yet). Privacy: the kind boxes hold only the viewer's own connections, and
# every item still goes through map_gnode (blocks, show-me, audiences). Nobody can open another person's "you".
# Usage: bash map_you.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18106. Never touches the live site. New file: map_graph.sh is frozen.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-map-you; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18106; URL=http://127.0.0.1:$PORT
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
field() { python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["node"][sys.argv[2]])' "$W/$1" "$2"; }

start
post $PAL /communities -d "slug=fres1" -d "name=Fresno Neighbors" -d "category=0" -d "subcategory=Neighbors" -d "about=x" --data-urlencode "city=Fresno, CA" >/dev/null
post $ADMIN /c/fres1/join -d "" >/dev/null
post $STRANGER /communities -d "slug=secret" -d "name=Stranger Things" -d "category=3" -d "subcategory=Music" -d "about=x" >/dev/null
post $PAL /proposals -d "title=Late buses" -d "body=route 1" -d "kind=0" --data-urlencode "city=Fresno, CA" >/dev/null
post $PAL /proposals -d "title=Not mine" -d "body=x" -d "kind=0" >/dev/null
buses=$(awk -F'\t' '$1=="prop" && $5=="Late buses"{print $2}' "$W/store.lgp")
notmine=$(awk -F'\t' '$1=="prop" && $5=="Not mine"{print $2}' "$W/store.lgp")
post $ADMIN /proposals/reply -d "id=$buses" --data-urlencode "text=Yes please" >/dev/null
check "precondition: ids found and Bbottin is in fres1" '[ -n "$buses" ] && [ -n "$notmine" ] && grep -qP "^cm\tfres1\tBbottin$" $W/store.lgp' "buses=$buses notmine=$notmine"

# --- the "you" box
r=$(node $ADMIN you you.json)
check "/map/node?n=you answers for the signed-in person" '[ "$r" = 200 ]' "$r"
check "... the box is you: your handle, kind you, opens your profile" '[ "$(field you.json label)" = Bbottin ] && [ "$(field you.json kind)" = you ] && [ "$(field you.json page)" = /u/Bbottin ]' "$(cat $W/you.json | head -c 200)"
check "its first ring has Friends, Groups, Places and Politics" 'has you.json mine:friends && has you.json mine:groups && has you.json mine:places && has you.json mine:politics' "$(cat $W/you.json | head -c 400)"
check "an empty kind is left off (Bbottin has no kept problems)" '! has you.json mine:problems' "Problems shown with nothing in it"
r=$(curl -s -o /dev/null -w '%{http_code}' -G --data-urlencode "n=you" "$URL/map/node")
check "signed out: no you box" '[ "$r" = 401 ]' "$r"

# --- each kind holds only your own connections
node $ADMIN mine:friends fr.json >/dev/null
check "Friends -> pal" 'has fr.json user:pal friends' "missing"
check "Friends does not list a non-friend (stranger)" '! has fr.json user:stranger' "stranger listed"
check "Friends leads back to you" 'has fr.json you' "no way back"
node $ADMIN mine:groups gr.json >/dev/null
check "Groups -> Fresno Neighbors" 'has gr.json group:fres1 member' "missing"
check "Groups does not list a group you are not in" '! has gr.json group:secret' "listed"
node $ADMIN mine:places pl.json >/dev/null
check "Places -> your city (from your group there)" 'has pl.json city:fresno-ca' "missing"
check "Places -> the whole site (the MAINstream) as its own branch" 'has pl.json root' "missing"
node $ADMIN mine:politics po.json >/dev/null
check "Politics -> the proposal you commented on" 'has po.json prop:$buses' "missing"
check "Politics does not list a proposal you never touched" '! has po.json prop:$notmine' "listed"
node $ADMIN root root.json >/dev/null
check "the MAINstream links back to you" 'has root.json you' "missing"

# --- someone else's view
node $STRANGER you syou.json >/dev/null
check "stranger's you box is stranger, not Bbottin" '[ "$(field syou.json label)" = stranger ]' "$(field syou.json label)"
node $STRANGER mine:friends sfr.json >/dev/null
check "stranger's Friends is their own (pal), never Bbottin's" 'has sfr.json user:pal && ! has sfr.json user:Bbottin' "wrong list"
r=$(node $STRANGER "mine:nonsense" bad.json)
check "an unknown kind box is not found" '[ "$r" = 404 ]' "$r"
# kept problems show under Problems, only for their owner
post $ADMIN /feed -F "text=I wish the buses ran later at night" -F "anybody=1" >/dev/null
pain=$(awk -F'\t' '$1=="pain" && $3=="Bbottin" && $8=="the buses ran later at night"{print $2}' "$W/store.lgp" | head -1)
[ -n "$pain" ] && post $ADMIN /problems/keep -d "id=$pain" >/dev/null
if [ -n "$pain" ]; then
    node $ADMIN you you2.json >/dev/null
    check "after keeping a problem, Problems is on your ring" 'has you2.json mine:problems' "missing"
    node $ADMIN mine:problems pr.json >/dev/null
    check "Problems -> your kept problem" 'has pr.json pain:$pain' "missing"
else
    bad "precondition: an 'I wish' post became a suggested problem to keep ($(grep -c ^pain $W/store.lgp) pain rows)"
fi

# --- the page starts at you
curl -s -o "$W/js.txt" "$URL/assets/lgmap.js"
check "lgmap.js starts the signed-in map at you" 'grep -q "load(.you.)" $W/js.txt' "still starts at root"

echo "passes=$passes fails=$fails"
[ $fails -eq 0 ]
