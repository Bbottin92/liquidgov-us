#!/usr/bin/env bash
# Proposal votes, reactions, comments and added choices keep your place (the rest of CODE-SCAN-UX A1, Brief 6 item 3;
# posts were done in f3c1e95 with #post-N). On iPhone Safari a 303 without a fragment lands at the top of the page.
#   P1 each proposal on /proposals, /proposals?id=N and a race on /officials carries id="proposal-N"
#   P2 Yes/No vote, reaction, comment and an added choice go back to their next= page at #proposal-N (an old
#      #fragment in next= is replaced); with no next= to /proposals#proposal-N
#   P3 on /officials (no next= on the vote and add-a-candidate forms): back to /officials#proposal-N
#   P4 control: a bad id still goes to /proposals with no fragment (nothing to land on)
# Usage: bash proposal_keep_place.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18114. Never touches the live site. New file: committed suites are frozen.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-proposal-keep-place; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18114; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
ADMIN=$(sid Bbottin); PAL=$(sid pal)
[ -n "$ADMIN" ] && [ -n "$PAL" ] || { echo "seed gave no sessions"; exit 1; }

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
# where a POST redirects to
loc() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{redirect_url}' -b "lg_session=$s" "$@" "$URL$p"; }
get()  { curl -s -o "$W/$3" -w '%{http_code}' -b "lg_session=$1" "$URL$2"; }
# the id of the proposal titled $2 on a saved /proposals page (its eyebrow "Proposal N" comes right before it)
pid_of() { python3 -c 'import re,sys; m=re.search(r"Proposal (\d+) [^<]*</p>\s*<h3[^>]*>"+re.escape(sys.argv[2])+"</h3>", open(sys.argv[1]).read()); print(m.group(1) if m else "")' "$W/$1" "$2"; }
# the id of the race titled $2 on a saved /officials page (the first id= field after its heading)
race_of() { python3 -c 'import re,sys; m=re.search(r"<h2>"+re.escape(sys.argv[2])+r"</h2>.*?name=\"id\" value=\"(\d+)\"", open(sys.argv[1]).read(), re.S); print(m.group(1) if m else "")' "$W/$1" "$2"; }

start
post $PAL /proposals -d "title=Plant shade trees" -d "body=Along Blackstone" -d "kind=0" >/dev/null
post $PAL /proposals -d "title=Pick a park day" -d "body=Which day" -d "kind=3" -d "open=1" --data-urlencode $'options=Saturday\nSunday' >/dev/null
post $PAL /officials --data-urlencode "title=Mayor race" --data-urlencode "place=Fresno, CA" -d "open=1" --data-urlencode $'options=Ann\nBo' >/dev/null
get $ADMIN /proposals p.html >/dev/null
YN=$(pid_of p.html "Plant shade trees"); CH=$(pid_of p.html "Pick a park day")
get $ADMIN /officials o.html >/dev/null
RACE=$(race_of o.html "Mayor race")
check "precondition: both proposals and the race exist" '[ -n "$YN" ] && [ -n "$CH" ] && [ -n "$RACE" ]' "yn=$YN ch=$CH race=$RACE"

check "P1 /proposals: the proposal carries id=proposal-$YN" 'grep -q "id=\"proposal-$YN\"" $W/p.html' "$(grep -o 'id="proposal-[0-9]*"' $W/p.html | head -3)"
get $ADMIN "/proposals?id=$YN" one.html >/dev/null
check "P1 /proposals?id=$YN carries id=proposal-$YN" 'grep -q "id=\"proposal-$YN\"" $W/one.html' "$(grep -o 'id="proposal-[0-9]*"' $W/one.html | head -3)"
check "P1 /officials: the race carries id=proposal-$RACE" 'grep -q "id=\"proposal-$RACE\"" $W/o.html' "$(grep -o 'id="proposal-[0-9]*"' $W/o.html | head -3)"

l=$(loc $ADMIN /proposals/vote -d "id=$YN" -d "choice=0" -d "next=/proposals")
check "P2 Yes goes back to /proposals at #proposal-$YN" '[ "$l" = "$URL/proposals#proposal-$YN" ]' "$l"
l=$(loc $ADMIN /proposals/vote -d "id=$YN" -d "choice=1" --data-urlencode "next=/proposals?city=fresno#proposal-999")
check "P2 No keeps next='s query and replaces its old fragment" '[ "$l" = "$URL/proposals?city=fresno#proposal-$YN" ]' "$l"
l=$(loc $ADMIN /proposals/vote -d "id=$YN" -d "choice=2")
check "P2 a vote with no next= goes to /proposals#proposal-$YN" '[ "$l" = "$URL/proposals#proposal-$YN" ]' "$l"
l=$(loc $ADMIN /proposals/react -d "id=$YN" -d "kind=support" --data-urlencode "next=/proposals?id=$YN")
check "P2 a reaction goes back at #proposal-$YN" '[ "$l" = "$URL/proposals?id=$YN#proposal-$YN" ]' "$l"
l=$(loc $ADMIN /proposals/react -d "id=$YN" -d "kind=support")
check "P2 a reaction with no next= goes to /proposals#proposal-$YN" '[ "$l" = "$URL/proposals#proposal-$YN" ]' "$l"
l=$(loc $ADMIN /proposals/reply -d "id=$YN" -d "text=good idea" -d "next=/proposals")
check "P2 a comment goes back at #proposal-$YN" '[ "$l" = "$URL/proposals#proposal-$YN" ]' "$l"
l=$(loc $ADMIN /proposals/vote -d "id=$CH" -d "value=1" -d "next=/proposals")
check "P2 a choice vote goes back at #proposal-$CH" '[ "$l" = "$URL/proposals#proposal-$CH" ]' "$l"
l=$(loc $ADMIN /proposals/option -d "id=$CH" -d "label=Friday" -d "next=/proposals")
check "P2 an added choice goes back at #proposal-$CH" '[ "$l" = "$URL/proposals#proposal-$CH" ]' "$l"

l=$(loc $ADMIN /proposals/vote -d "id=$RACE" -d "value=Ann")
check "P3 a race vote (no next=) goes back to /officials#proposal-$RACE" '[ "$l" = "$URL/officials#proposal-$RACE" ]' "$l"
l=$(loc $ADMIN /proposals/option -d "id=$RACE" -d "label=Cy")
check "P3 an added candidate (no next=) goes back to /officials#proposal-$RACE" '[ "$l" = "$URL/officials#proposal-$RACE" ]' "$l"
l=$(loc $ADMIN /proposals/react -d "id=$RACE" -d "kind=support" -d "next=/officials")
check "P3 a race reaction goes back to /officials#proposal-$RACE" '[ "$l" = "$URL/officials#proposal-$RACE" ]' "$l"

l=$(loc $ADMIN /proposals/vote -d "id=abc" -d "choice=0" -d "next=/proposals")
check "P4 control: a bad id goes to /proposals with no fragment" '[ "$l" = "$URL/proposals" ]' "$l"

echo "passes=$passes fails=$fails"
echo "VERDICT proposal_keep_place $([ $fails -eq 0 ] && echo ALL-PASS || echo FAIL) $passes/$((passes+fails))"
[ $fails -eq 0 ]
