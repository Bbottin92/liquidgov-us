#!/usr/bin/env bash
# Share to LiquidGov (2026-11-15): the installed app is a share target, so a phone's Share menu offers it from other
# apps; what was shared opens in the feed's post box. Checks the manifest's share_target, that /share puts title, text
# and link together as one text (a link already in the text is not repeated) and sends it to the feed's post box
# (/feed#share=...), that the post box carries the script that fills it, and that a signed-out visitor is sent to log in.
# Usage: bash share_target.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18134. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-share-target; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18134; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
PAL=$(awk '$1=="pal"{print $2}' "$W/sessions.txt")
[ -n "$PAL" ] || { echo "seed gave no sessions"; exit 1; }

(cd "$HERE" && exec env -u LIQUIDGOV_OPERATORS LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT \
    "$BIN") >> "$W/server.log" 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null' EXIT
for i in $(seq 50); do
    kill -0 $SERVER 2>/dev/null || { echo "server died"; cat "$W/server.log"; exit 1; }
    curl -s -o /dev/null --max-time 1 $URL/ && break
    sleep 0.2
done
loc() { curl -s -o /dev/null -w '%{redirect_url}' ${1:+-b "lg_session=$1"} "$URL$2"; }

curl -s $URL/manifest.webmanifest > "$W/manifest.json"
check "the manifest is valid JSON with a share target: GET /share with title, text and url" \
  'python3 -c "import json,sys; t=json.load(open(sys.argv[1]))[\"share_target\"]; sys.exit(0 if t==dict(action=\"/share\",method=\"GET\",params=dict(title=\"title\",text=\"text\",url=\"url\")) else 1)" "$W/manifest.json"' \
  "$(head -c 300 "$W/manifest.json")"

L=$(loc "$PAL" "/share?title=Cool%20clip&text=Look%20at%20this%20https%3A%2F%2Fx.com%2Fa%2Fstatus%2F1&url=https%3A%2F%2Fx.com%2Fa%2Fstatus%2F1")
check "/share sends title and text to the feed's post box, the link not repeated" \
  '[ "$L" = "$URL/feed#share=Cool%20clip%0ALook%20at%20this%20https%3A%2F%2Fx.com%2Fa%2Fstatus%2F1" ]' "$L"
L2=$(loc "$PAL" "/share?url=https%3A%2F%2Fyoutu.be%2FdQw4w9WgXcQ")
check "/share with only a link sends the link" '[ "$L2" = "$URL/feed#share=https%3A%2F%2Fyoutu.be%2FdQw4w9WgXcQ" ]' "$L2"
L3=$(loc "" "/share?url=https%3A%2F%2Fyoutu.be%2FdQw4w9WgXcQ")
check "signed out: /share asks you to log in" '[ "${L3%%\?*}" = "$URL/login" ]' "$L3"
curl -s -b "lg_session=$PAL" $URL/feed > "$W/feed.html"
check "the feed's post box carries the script that fills it from #share=" 'grep -q "#share=" "$W/feed.html" && grep -q "location.hash" "$W/feed.html"' "no script"

echo "passes=$passes fails=$fails"
[ $fails = 0 ]
