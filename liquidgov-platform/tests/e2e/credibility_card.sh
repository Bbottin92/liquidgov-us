#!/usr/bin/env bash
# Credibility, first slice (2026-11-15; Persistence/TheCommons/CLAUDE_CREDIBILITY_QUESTION.md): Settings shows you your
# own Contribution, computed from what the site keeps: how many different people replied to, voted on or reacted to what
# you made. The anti-sabotage rules are what this checks: a person counts once however often they reply; your own
# replies do not count; someone who is not your friend is counted as beyond your friends; someone you blocked does not
# count; nobody else sees your card (it is not on your profile).
# Usage: bash credibility_card.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18135. Never touches the live site. Seed: Bbottin<->pal friends; Bbottin and stranger are not.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-credibility-card; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18135; URL=http://127.0.0.1:$PORT
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
(cd "$HERE" && exec env -u LIQUIDGOV_OPERATORS LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT \
    "$BIN") >> "$W/server.log" 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null' EXIT
for i in $(seq 50); do
    kill -0 $SERVER 2>/dev/null || { echo "server died"; cat "$W/server.log"; exit 1; }
    curl -s -o /dev/null --max-time 1 $URL/ && break
    sleep 0.2
done
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -b "lg_session=$s" "$@" "$URL$p"; }
card() { curl -s -b "lg_session=$1" "$URL/settings" | tr '\n' ' ' | grep -o '<div class="card" id="credibility">.*' | sed 's#</div>.*##'; }
row() { awk -F'\t' -v t="$1" -v pat="$2" '$1==t && $0 ~ pat' "$W/store.lgp"; }

c0=$(card $ADMIN)
check "Settings has your credibility card, marked as only for you" 'echo "$c0" | grep -q "Your credibility" && echo "$c0" | grep -q "only you see this"' "$c0"
post $ADMIN /feed -F "text=Credneedle post" -F "anybody=1"
pid=$(row post Credneedle | cut -f2 | head -1)
check "precondition: the post exists" '[ -n "$pid" ]' "no post"
for i in 1 2 3 4 5; do post $PAL "/post/$pid/reply" --data-urlencode "text=friend reply $i"; done
for i in 1 2; do post $STRANGER "/post/$pid/reply" --data-urlencode "text=stranger reply $i"; done
for i in 1 2 3; do post $ADMIN "/post/$pid/reply" --data-urlencode "text=my own reply $i"; done
c1=$(card $ADMIN)
check "a person counts once however often they reply, and your own replies do not count: 2 people" \
  'echo "$c1" | grep -q "2 people engaged with them"' "$c1"
check "the one who is not your friend is counted beyond your friends" 'echo "$c1" | grep -q "(1 beyond your friends)"' "$c1"
check "it counts what you made: 1 post" 'echo "$c1" | grep -q "0 proposals, 1 post<"' "$c1"
check "a first week with fewer than 3 people is New" 'echo "$c1" | grep -q "<strong>New</strong>"' "$c1"
post $ADMIN /friends/block --data-urlencode "who=stranger"
c2=$(card $ADMIN)
check "someone you blocked does not count (1 person, 0 beyond your friends)" \
  'echo "$c2" | grep -q "1 person engaged with them (0 beyond your friends)"' "$c2"
curl -s -b "lg_session=$PAL" "$URL/u/Bbottin" > "$W/profile.html"
check "nobody else sees your card (not on your profile)" '! grep -q "Your credibility" "$W/profile.html"' "on the profile"

echo "passes=$passes fails=$fails"
[ $fails = 0 ]
