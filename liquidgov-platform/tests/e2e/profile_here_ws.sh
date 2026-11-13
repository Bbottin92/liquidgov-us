#!/usr/bin/env bash
# Dots on profile pages, part 2: the live room (Brandon 2026-11-12). /here/ws?page=/u/<name> is one room per profile
# page. Only signed-in people who chose Yes join; you see everyone in the same room (yourself included) and nobody on
# another page; blocks hide people both ways and a profile's owner who blocked you keeps you out of their room; a
# private profile's room is closed to those who can't see the profile. Positions are fractions of the page (0..1),
# clamped, kept in memory only: leaving removes your dot, and nothing is written to the store.
# Usage: bash profile_here_ws.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18124. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
CLIENT="$(cd "$(dirname "$0")" && pwd)/here_ws_client.py"
W=/tmp/lg-profile-here-ws; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18124; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] && [ -f "$CLIENT" ] || { echo "missing binaries or client"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
ADMIN=$(sid Bbottin); PAL=$(sid pal); STRANGER=$(sid stranger)
[ -n "$ADMIN" ] && [ -n "$PAL" ] && [ -n "$STRANGER" ] || { echo "seed gave no sessions"; exit 1; }

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
trap 'stop; kill $(jobs -p) 2>/dev/null' EXIT
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{http_code}' ${s:+-b "lg_session=$s"} "$@" "$URL$p"; }
ws() { python3 "$CLIENT" $PORT "$@"; }   # <session|-> <page> <seconds> [x y]
# who is in a snapshot (sorted, space-separated); "" when none or not JSON
who() { python3 -c 'import json,sys
try: d=json.loads(sys.argv[1])
except Exception: print(""); sys.exit()
print(" ".join(sorted(p["u"] for p in d.get("people",[]))))' "$1"; }
pos() { python3 -c 'import json,sys
d=json.loads(sys.argv[1]); p=[p for p in d["people"] if p["u"]==sys.argv[2]][0]; print("%.2f %.2f"%(p["x"],p["y"]))' "$1" "$2" 2>/dev/null; }

start
post $PAL /here/choice -d choice=1 -d back=/settings >/dev/null
post $STRANGER /here/choice -d choice=0 -d back=/settings >/dev/null
check "precondition: pal Yes, stranger No, Bbottin not chosen" 'grep -qP "^here\tpal\t1$" $W/store.lgp && grep -qP "^here\tstranger\t0$" $W/store.lgp && ! grep -qP "^here\tBbottin\t" $W/store.lgp' "$(grep '^here' $W/store.lgp)"

# --- who may join
r=$(ws $PAL /u/stranger 1 0.2 0.3)
check "pal (Yes) joins /u/stranger and sees herself" '[ "$(who "$r")" = "pal" ]' "$r"
r=$(ws $ADMIN /u/stranger 1)
check "Bbottin (not chosen) is refused" '[[ "$r" == REFUSED* ]]' "$r"
r=$(ws $STRANGER /u/pal 1)
check "stranger (No) is refused" '[[ "$r" == REFUSED* ]]' "$r"
r=$(ws - /u/stranger 1)
check "signed out is refused" '[[ "$r" == REFUSED* ]]' "$r"
for p in /feed /u/nosuchperson "/u/../admin" "/u/pal/theme" ""; do
    r=$(ws $PAL "$p" 1)
    check "not a profile page ('$p') is refused" '[[ "$r" == REFUSED* ]]' "$r"
done

# --- two people in one room
post $ADMIN /here/choice -d choice=1 -d back=/settings >/dev/null
ws $PAL /u/stranger 4 0.2 0.3 > "$W/pal1.txt" & CP=$!
sleep 0.8
r=$(ws $ADMIN /u/stranger 2 0.7 0.4)
check "Bbottin sees pal and himself on /u/stranger" '[ "$(who "$r")" = "Bbottin pal" ]' "$r"
check "...pal at her position (0.20 0.30)" '[ "$(pos "$r" pal)" = "0.20 0.30" ]' "$(pos "$r" pal)"
wait $CP
r=$(cat "$W/pal1.txt")
check "...and pal saw Bbottin at his (0.70 0.40)" '[ "$(pos "$r" Bbottin)" = "0.70 0.40" ] || [ "$(who "$r")" = "pal" ]' "$r"
ws $PAL /u/stranger 3 5 -3 > "$W/pal2.txt" & CP=$!
sleep 0.8
r=$(ws $ADMIN /u/stranger 1.5)
check "positions are clamped to the page (5,-3 -> 1,0)" '[ "$(pos "$r" pal)" = "1.00 0.00" ]' "$r"
wait $CP

# --- another page is another room
ws $PAL /u/stranger 3 0.5 0.5 > /dev/null & CP=$!
sleep 0.8
r=$(ws $ADMIN /u/pal 1.5 0.1 0.1)
check "Bbottin on /u/pal does not see pal on /u/stranger" '[ "$(who "$r")" = "Bbottin" ]' "$r"
wait $CP

# --- leaving removes the dot
r=$(ws $ADMIN /u/stranger 1.2)
check "after pal left, Bbottin sees only himself" '[ "$(who "$r")" = "Bbottin" ]' "$r"
check "nothing about the room was written to the store" '! grep -qiE "^(hpos|here)	.*0\.[0-9]" $W/store.lgp' "$(grep -c . $W/store.lgp)"

# --- blocks: both ways, and the owner's room
post $PAL /settings/block -d "who=Bbottin" >/dev/null
ws $PAL /u/stranger 3 0.2 0.3 > "$W/pal3.txt" & CP=$!
sleep 0.8
r=$(ws $ADMIN /u/stranger 2 0.7 0.4)
check "pal blocked Bbottin: Bbottin does not see pal" '[ "$(who "$r")" = "Bbottin" ]' "$r"
wait $CP
r=$(cat "$W/pal3.txt")
check "...and pal does not see Bbottin" '[ "$(who "$r")" = "pal" ]' "$r"
r=$(ws $ADMIN /u/pal 1)
check "pal blocked Bbottin: Bbottin can't join pal's own page" '[[ "$r" == REFUSED* ]]' "$r"
post $PAL /settings/unblock -d "who=Bbottin" >/dev/null

# --- a private profile's room
post $STRANGER /settings/privacy -d "findability=1" -d "profile_visibility=3" -d "posts_visibility=0" -d "jukebox_policy=0" >/dev/null
r=$(ws $PAL /u/stranger 1)
check "stranger made the profile private: pal can't join its room" '[[ "$r" == REFUSED* ]]' "$r"
r=$(ws $PAL /u/Bbottin 1)
check "control: pal still joins a public profile's room" '[ "$(who "$r")" = "pal" ]' "$r"

echo "passes=$passes fails=$fails"
[ $fails = 0 ]
