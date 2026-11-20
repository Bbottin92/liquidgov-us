#!/usr/bin/env bash
# Pools show what they are (Brandon 2026-11-16, future-list item 1): the map's node data carries the whole text of a post,
# a proposal and a comment, who wrote it and when; a comment is a pool of its own (pcom:<id> on a post, rcom:<id> on a
# proposal) linked from the thing it is on. Same privacy as the map already had: a post you may not see, or a comment by
# someone blocked either way with you, is not there.
# Usage: bash map_pool_content.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18138. Never touches the live site.
# Topic lookups go to a dead local port (LIQUIDGOV_WIKI_BASE): the real Wikipedia, reachable from CI runners,
# added topic results to the exact-match checks now and then (PR #10, run 44720432094).
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-map-pool-content; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18138; URL=http://127.0.0.1:$PORT
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
(cd "$HERE" && exec env -u LIQUIDGOV_OPERATORS LIQUIDGOV_WIKI_BASE=http://127.0.0.1:9/ LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT "$BIN") > "$W/server.log" 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null' EXIT
for i in $(seq 50); do
    kill -0 $SERVER 2>/dev/null || { echo "server died"; cat "$W/server.log"; exit 1; }
    curl -s -o /dev/null --max-time 1 $URL/ && break
    sleep 0.2
done
node_of() { curl -s -b "lg_session=$1" "$URL/map/node?n=$2"; }
# python reads the JSON from stdin: j <expr over d>
j() { python3 -c 'import json,sys; d=json.loads(sys.stdin.read()); print(eval(sys.argv[1]))' "$1" 2>/dev/null; }

LONG="The river cleanup crew meets at the old bridge on Saturday. Bring gloves, bring water, bring a friend who has never done this before, and stay for the food afterwards."
curl -s -o /dev/null -b "lg_session=$PAL" -F "text=$LONG" -F "anybody=1" $URL/feed
PID=$(curl -s -b "lg_session=$ADMIN" "$URL/map/search?tz=0&q=river%20cleanup" | j 'd["results"][0]["id"].split(":")[1]')
check "found the post" '[ -n "$PID" ]' "no post id"
curl -s -o /dev/null -b "lg_session=$ADMIN" -d "text=I will bring the grabbers and a wagon for the bags" "$URL/post/$PID/reply"

n=$(node_of "$ADMIN" "post:$PID")
check "a post's node carries its whole text" '[ "$(echo "$n" | j "d[\"node\"][\"body\"]")" = "$LONG" ]' "$n"
check "...who wrote it" '[ "$(echo "$n" | j "d[\"node\"][\"by\"]")" = "pal" ]' "$n"
check "...and when (a unix time)" '[ "$(echo "$n" | j "d[\"node\"][\"ts\"] > 1700000000")" = "True" ]' "$n"
CID=$(echo "$n" | j '[l["node"]["id"] for l in d["links"] if l["node"]["kind"]=="comment"][0]')
check "the post links to its comment, a pool of its own" '[ "${CID%%:*}" = "pcom" ]' "$CID"
c=$(node_of "$ADMIN" "$CID")
check "the comment's node carries its whole text and who wrote it" '[ "$(echo "$c" | j "d[\"node\"][\"body\"]")" = "I will bring the grabbers and a wagon for the bags" ] && [ "$(echo "$c" | j "d[\"node\"][\"by\"]")" = "Bbottin" ]' "$c"
check "the comment links back to its post and to its author" 'echo "$c" | j "sorted(l[\"node\"][\"id\"] for l in d[\"links\"])" | grep -q "post:$PID" && echo "$c" | j "sorted(l[\"node\"][\"id\"] for l in d[\"links\"])" | grep -q "user:Bbottin"' "$c"

# a proposal and its comment
curl -s -o /dev/null -b "lg_session=$PAL" -d "title=Shade at bus stops" -d "body=Plant trees at the ten busiest bus stops." -d "kind=0" $URL/proposals
PR=$(curl -s -b "lg_session=$ADMIN" "$URL/map/search?tz=0&q=shade%20bus" | j '[r["id"] for r in d["results"] if r["id"].startswith("prop:")][0]')
curl -s -o /dev/null -b "lg_session=$ADMIN" -d "id=${PR#prop:}" -d "text=Start with the stops by the school" $URL/proposals/reply
p=$(node_of "$ADMIN" "$PR")
check "a proposal's node carries its body and author" '[ "$(echo "$p" | j "d[\"node\"][\"body\"]")" = "Plant trees at the ten busiest bus stops." ] && [ "$(echo "$p" | j "d[\"node\"][\"by\"]")" = "pal" ]' "$p"
RC=$(echo "$p" | j '[l["node"]["id"] for l in d["links"] if l["node"]["kind"]=="comment"][0]')
check "the proposal links to its comment (rcom:)" '[ "${RC%%:*}" = "rcom" ]' "$RC"

# privacy: a comment by someone blocked either way is not there; a post you may not see has no comments for you
curl -s -o /dev/null -b "lg_session=$PAL" -d who=Bbottin "$URL/friends/block"
c2=$(node_of "$PAL" "$CID")
check "a comment by someone you blocked is not there for you" '[ "$(echo "$c2" | j "d.get(\"error\",\"\")")" = "not found" ]' "$c2"
n2=$(node_of "$PAL" "post:$PID")
check "...nor linked from its post" '! echo "$n2" | grep -q "pcom:"' "$n2"
c3=$(node_of "$ADMIN" "$CID")
check "...and the other way round: the blocked one no longer sees it either" '[ "$(echo "$c3" | j "d.get(\"error\",\"\")")" = "not found" ]' "$c3"
curl -s -o /dev/null -b "lg_session=$STRANGER" "$URL/map/node?n=pcom:99999" -w ""
x=$(node_of "$STRANGER" "pcom:99999")
check "an unknown comment is not found" '[ "$(echo "$x" | j "d.get(\"error\",\"\")")" = "not found" ]' "$x"
code=$(curl -s -o /dev/null -w '%{http_code}' "$URL/map/node?n=pcom:1")
check "signed out: refused" '[ "$code" = 401 ]' "$code"

echo "passes=$passes fails=$fails"
[ $fails = 0 ]
