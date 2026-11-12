#!/usr/bin/env bash
# Reactions can't be mistaken for votes (Brief 7 item 3, PROPOSALS-AUDIT section 2 item 2): Support / Oppose sat
# between the answer buttons and the comments, looking like a second way to vote.
#   R1 on /proposals and /officials, every card's reaction buttons come after its answer forms and its comments
#   R2 they sit in their own box (class "reactions") that says "A reaction is not a vote." before the buttons
#   R3 the stylesheet sets that box apart (a rule line above it) and makes its buttons smaller than answer buttons
#   R4 control: a reaction still counts and shows your check mark
# Usage: bash reactions_apart.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18117. Never touches the live site. New file: committed suites are frozen.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-reactions-apart; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18117; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
PAL=$(sid pal); STRANGER=$(sid stranger)
[ -n "$PAL" ] && [ -n "$STRANGER" ] || { echo "seed gave no sessions"; exit 1; }

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
# per-card layout facts of a saved page: for each card holding a proposal, prints
# "<id> <last answer form pos> <comments details end> <first reaction form pos> <reactions box pos> <note in box 0|1>"
layout() { python3 - "$W/$1" <<'PY'
import re, sys
html = open(sys.argv[1]).read()
cards = html.split('<div class="card')[1:]
for c in cards:
    m = re.search(r'id="proposal-(\d+)"', c)
    if not m or '/proposals/react' not in c:
        continue
    votes = [x.start() for x in re.finditer(r'action="/proposals/(?:vote|option)"', c)]
    cm = re.search(r'<summary[^>]*>\d+ comments?</summary>', c)
    det = c.find('</details>', cm.end()) if cm else -1  # the end of the comments disclosure, not a later one
    react = c.find('action="/proposals/react"')
    box = c.find('<div class="reactions"')
    note_in = 0
    if box >= 0:
        end = c.find('action="/proposals/react"', box)
        note_in = 1 if 'A reaction is not a vote.' in c[box:end] else 0
    print(m.group(1), max(votes) if votes else -1, det, react, box, note_in)
PY
}

start
post $PAL /proposals -d "title=Plant trees" -d "body=Along Blackstone" -d "kind=0" >/dev/null
post $PAL /proposals -d "title=Park pick" -d "body=Which" -d "kind=3" -d "open=1" --data-urlencode $'options=A park\nB park' >/dev/null
post $PAL /proposals -d "title=Benches" -d "body=How many" -d "kind=1" >/dev/null
post $PAL /officials --data-urlencode "title=Mayor" --data-urlencode "place=Fresno, CA" -d "open=1" --data-urlencode $'options=Ann\nBo' >/dev/null
post $STRANGER /proposals/react -d "id=1" -d "kind=support" >/dev/null
get $STRANGER /proposals p.html >/dev/null; layout p.html > $W/p.lay
get $STRANGER /officials o.html >/dev/null; layout o.html > $W/o.lay
check "precondition: 3 proposal cards and 1 race card with reactions" '[ "$(wc -l < $W/p.lay)" = 3 ] && [ "$(wc -l < $W/o.lay)" = 1 ]' "$(cat $W/p.lay $W/o.lay | tr '\n' ';')"

all() { awk "$1" $W/p.lay $W/o.lay | grep -qv 1 && return 1; [ -n "$(cat $W/p.lay $W/o.lay)" ]; }
check "R1 every card: the reactions come after every answer form (vote, add option)" 'all "{print (\$4>\$2)?1:0}"' "$(cat $W/p.lay $W/o.lay | tr '\n' ';')"
check "R1 every card: the reactions come after the comments" 'all "{print (\$3>=0 && \$4>\$3)?1:0}"' "$(cat $W/p.lay $W/o.lay | tr '\n' ';')"
check "R2 every card: the reactions sit in their own box (class reactions) after the comments" 'all "{print (\$5>\$3 && \$5<\$4)?1:0}"' "$(cat $W/p.lay $W/o.lay | tr '\n' ';')"
check "R2 every card: the box says A reaction is not a vote. before its buttons" 'all "{print \$6}"' "$(cat $W/p.lay $W/o.lay | tr '\n' ';')"
check "R3 the stylesheet sets the box apart (a rule line above it) and makes its buttons smaller" 'curl -s "$URL/assets/site.css" > $W/site.css && grep -q "assets/site.css" $W/p.html && grep -q "\.reactions{[^}]*border-top" $W/site.css && grep -q "\.reactions form\.inline button[^{]*{[^}]*font-size" $W/site.css' "no .reactions rules"
check "R4 control: counts and a toggle still work (Support · 1 with the check mark)" 'grep -q "Support · 1 &#10003;" $W/p.html' "$(grep -o 'Support · [0-9][^<]*' $W/p.html | head -2)"

echo "passes=$passes fails=$fails"
echo "VERDICT reactions_apart $([ $fails -eq 0 ] && echo ALL-PASS || echo FAIL) $passes/$((passes+fails))"
[ $fails -eq 0 ]
