#!/usr/bin/env bash
# The one-time 988 note also shows inside a frame (2026-11-08). Framed pages (Sec-Fetch-Dest: iframe, or ?embed=1)
# are the map's field and every page while Juketube's persistent shell keeps music playing; they used to skip the
# note, so someone browsing with music on could go a long time without it. Must FAIL on the previous binary
# (check 2). problems_own_writing.sh keeps covering the full-page case; this file does not change it.
# Usage: bash care_notice_embed.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18097. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-care-embed; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18097; URL=http://127.0.0.1:$PORT
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
# full page; framed page the way a browser asks for it (Sec-Fetch-Dest: iframe); framed page forced by ?embed=1
get()    { curl -s -o "$W/$3" -w '%{http_code}' -b "lg_session=$1" "$URL$2"; }
framed() { curl -s -o "$W/$3" -w '%{http_code}' -b "lg_session=$1" -H "Sec-Fetch-Dest: iframe" "$URL$2"; }
post()   { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{http_code}' -b "lg_session=$s" "$@" "$URL$p"; }
crisis() { post $1 /feed -F "text=honestly I wish I wasn't here anymore" -F "anybody=1" >/dev/null; }

start

# 1. preconditions: a framed page really is the framed layout, and nobody has a pending note yet
framed $STRANGER /laws pre.html >/dev/null
check "precondition: a framed page uses the frame layout" 'grep -q "header.top,footer.site{display:none" $W/pre.html' "not embed"
check "precondition: no note before any crisis words" '! grep -q "tel:988" $W/pre.html' "note without a trigger"

# 2. crisis words, then the next page is FRAMED (the Juketube shell): the note shows there
crisis $STRANGER
framed $STRANGER /friends f1.html >/dev/null
check "the writer's next framed page shows 988" 'grep -q "tel:988" $W/f1.html' "no 988 in the frame"
check "...still the frame layout" 'grep -q "header.top,footer.site{display:none" $W/f1.html' "not embed"
framed $STRANGER /friends f2.html >/dev/null
check "...only once (next framed page)" '! grep -q "tel:988" $W/f2.html' "shown again"
get $STRANGER /feed f3.html >/dev/null
check "...only once (next full page)" '! grep -q "tel:988" $W/f3.html' "shown again on a full page"

# 3. ?embed=1 (the map field's forced form) shows it too
crisis $STRANGER
get $STRANGER "/laws?embed=1" e1.html >/dev/null
check "?embed=1 page shows 988" 'grep -q "tel:988" $W/e1.html' "no 988"

# 4. the full-page path still works, both ways
crisis $STRANGER
get $STRANGER /feed p1.html >/dev/null
check "a full page still shows 988" 'grep -q "tel:988" $W/p1.html' "no 988"
framed $STRANGER /friends p2.html >/dev/null
check "...and then not in a frame" '! grep -q "tel:988" $W/p2.html' "shown again in a frame"

# 5. nobody else sees it, framed or not
crisis $STRANGER
framed $PAL /friends o1.html >/dev/null
get $PAL /feed o2.html >/dev/null
check "another person's framed page has no 988" '! grep -q "tel:988" $W/o1.html' "shown to pal"
check "another person's full page has no 988" '! grep -q "tel:988" $W/o2.html' "shown to pal"
get $STRANGER /feed o3.html >/dev/null
check "...while the writer still gets theirs" 'grep -q "tel:988" $W/o3.html' "writer lost the note"

stop
echo "care_notice_embed: passes=$passes fails=$fails"
[ $fails -eq 0 ] && echo "EXIT=0" || echo "EXIT=1"
[ $fails -eq 0 ]
