#!/usr/bin/env bash
# /privacy says messages are readable by the operator (Brandon 2026-11-11 4:30 PM decision 5): exactly
# "Messages on this copy are stored so the operator can read them." right after "Posts, mail, and drawings stay until
# you delete them, or until an operator removes spam." in privacy_body(), the same words Settings > Privacy and the
# message panel already use.
#   - /privacy answers 200 signed out and signed in
#   - the sentence is there once, immediately after the "Posts, mail, and drawings stay..." sentence
#   - the rest of that card is unchanged (Friends are mutual accept... still follows)
# Usage: bash privacy_messages_note.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18109. Never touches the live site. New file: committed suites are frozen.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-privacy-messages; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18109; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
ADMIN=$(sid Bbottin)
[ -n "$ADMIN" ] || { echo "seed gave no sessions"; exit 1; }

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

S1="Posts, mail, and drawings stay until you delete them, or until an operator removes spam."
S2="Messages on this copy are stored so the operator can read them."
# the text of the page with tags dropped and whitespace squeezed, so the two sentences are compared as read
text() { python3 -c 'import re,sys,html; t=re.sub(r"<[^>]+>"," ",open(sys.argv[1]).read()); print(re.sub(r"\s+"," ",html.unescape(t)))' "$W/$1"; }

start
r=$(curl -s -o "$W/out.html" -w '%{http_code}' "$URL/privacy")
check "/privacy answers 200 signed out" '[ "$r" = 200 ]' "$r"
text out.html > "$W/out.txt"
check "control: the Posts, mail, and drawings sentence is there" 'grep -qF "$S1" $W/out.txt' "missing"
check "the Messages sentence is there exactly once" '[ "$(grep -oF "$S2" $W/out.txt | wc -l)" = 1 ]' "count=$(grep -oF "$S2" $W/out.txt | wc -l)"
check "... immediately after the Posts, mail, and drawings sentence" 'grep -qF "$S1 $S2" $W/out.txt' "not adjacent"
check "... and the card goes on as before (Friends are mutual accept.)" 'grep -qF "$S2 Friends are mutual accept." $W/out.txt' "changed"
r=$(curl -s -o "$W/in.html" -w '%{http_code}' -b "lg_session=$ADMIN" "$URL/privacy")
check "/privacy answers 200 signed in, with the sentence" '[ "$r" = 200 ] && text in.html | grep -qF "$S1 $S2"' "$r"

echo "passes=$passes fails=$fails"
[ $fails -eq 0 ]
