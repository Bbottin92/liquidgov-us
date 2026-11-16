#!/usr/bin/env bash
# Hints for newcomers (Brandon 2026-11-16: "a hint phase for newcomers that you can easily turn off and you can turn
# back on from the dropdown menu"). Checks: an existing account has hints off (no script on its pages); a NEW account is
# created with hints on; POST /settings/hints turns them on and off, dismisses one hint ("got it") and resets the
# dismissed list; the menu item says what a tap will do; a dismissed hint stays dismissed across off and on; a bad id is
# dropped; signed out nothing happens; the script is not sent on a framed page.
# Usage: bash hints_newcomers.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18150. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-hints-newcomers; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18150; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
PAL=$(sid pal)
[ -n "$PAL" ] || { echo "seed gave no sessions"; exit 1; }
(cd "$HERE" && exec env -u LIQUIDGOV_OPERATORS LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT "$BIN") > "$W/server.log" 2>&1 &
SERVER=$!
trap 'kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null' EXIT
for i in $(seq 50); do
    kill -0 $SERVER 2>/dev/null || { echo "server died"; cat "$W/server.log"; exit 1; }
    curl -s -o /dev/null --max-time 1 $URL/ && break
    sleep 0.2
done
feed() { curl -s -b "lg_session=$PAL" $URL/feed; }
done_list() { feed | grep -o 'LG_HINTS={"done":\[[^]]*\]}' | head -1; }
hints() { curl -s -o /dev/null -w '%{http_code}' -b "lg_session=$PAL" "$@" $URL/settings/hints; }

check "an existing account has hints off: no hints script" '! feed | grep -q "LG_HINTS"' "$(done_list)"
check "...and the menu offers to turn them on" 'feed | grep -q "Hints: off"' "$(feed | grep -o "Hints: [a-z]*" | head -1)"
check "a signed-out visitor cannot change hints" '[ "$(curl -s -o /dev/null -w "%{http_code}" -d on=1 $URL/settings/hints)" != 200 ]' "changed"

hints -d on=1 >/dev/null
check "turned on: the page loads hints.js" 'feed | grep -q "hints.js"' "no script"
check "...with nothing dismissed yet" '[ "$(done_list)" = "LG_HINTS={\"done\":[]}" ]' "$(done_list)"
check "...and the menu says Hints: on" 'feed | grep -q "Hints: on"' "$(feed | grep -o "Hints: [a-z]*" | head -1)"
check "hints.js is served" '[ "$(curl -s -o /dev/null -w "%{http_code}" $URL/assets/hints.js)" = 200 ]' "not served"

r=$(hints -d dismiss=menu -d ajax=1)
check "got it on one hint answers ok" '[ "$r" = 200 ]' "$r"
check "...and it is now in the dismissed list" '[ "$(done_list)" = "LG_HINTS={\"done\":[\"menu\"]}" ]' "$(done_list)"
hints -d "dismiss=bad id;<script>" >/dev/null
check "a hint id with odd characters is dropped" '[ "$(done_list)" = "LG_HINTS={\"done\":[\"menu\"]}" ]' "$(done_list)"
hints -d dismiss=menu >/dev/null
check "dismissing twice keeps one" '[ "$(done_list)" = "LG_HINTS={\"done\":[\"menu\"]}" ]' "$(done_list)"

hints -d on=0 >/dev/null
check "turned off: no hints script" '! feed | grep -q "LG_HINTS"' "$(done_list)"
check "...and the menu offers to turn them on again" 'feed | grep -q "Hints: off"' "$(feed | grep -o "Hints: [a-z]*" | head -1)"
hints -d on=1 >/dev/null
check "turned back on: the dismissed hint stays dismissed" '[ "$(done_list)" = "LG_HINTS={\"done\":[\"menu\"]}" ]' "$(done_list)"
hints -d reset=1 >/dev/null
check "Show hints again clears the dismissed list" '[ "$(done_list)" = "LG_HINTS={\"done\":[]}" ]' "$(done_list)"

check "the menu form goes back to the page it was on" '[ "$(curl -s -o /dev/null -w "%{redirect_url}" -b "lg_session=$PAL" -e $URL/feed -d on=0 $URL/settings/hints)" = "$URL/feed" ]' "no redirect"
hints -d on=1 >/dev/null
check "a framed page does not get the hints script" '! curl -s -b "lg_session=$PAL" "$URL/feed?embed=1" | grep -q "LG_HINTS"' "got it"

signup() { curl -s -o "$W/$2" -w '%{http_code}' --data-urlencode "username=$1" -d "display_name=New Person" -d "password=correct-horse-9" \
    -d "email=$1@example.com" -d "phone=555123$RANDOM$RANDOM" -d here=1 "$URL/signup"; }
signup newbie1 su.html >/dev/null
check "a brand-new account is created with hints on" 'grep -qP "^hints\tnewbie1\t1\t" $W/store.lgp' "$(grep '^hints' $W/store.lgp)"
sleep 0.3
echo "passes=$passes fails=$fails"
[ $fails = 0 ]
