#!/usr/bin/env bash
# Juketube History survives a restart (Brandon 2026-11-21: "theres no history appearing"). A shared station's history
# lived only in server memory, so every restart (every deploy) emptied it. Now each finished or replaced track is also
# saved (jh records under owner "jbs:<station>", jukebox.hpp kStationHistoryPrefix) and /jukebox/history?station=
# reads the saved list first.
#   H1 after two plays on the site-wide station, History lists the first one (it was replaced)
#   H2 after a restart of the server on the same store, History still lists it   <- fails on the memory-only code
#   H3 the store file holds the saved entry as a jh record under jbs:
#   H4 the served juketube.js asks for titles for History rows (fillTitle) and copies with a fallback (legacyCopy)
# Usage: bash juketube_history.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18157. Never touches the live site.
set -u
BIN=$1; SEED=$2
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-juketube-history; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18157; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }

$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
A=$(awk '$1=="Bbottin"{print $2}' "$W/sessions.txt")
[ -n "$A" ] || { echo "seed gave no session"; exit 1; }

SERVER=""
start() {
    (cd "$HERE" && exec env -u LIQUIDGOV_OPERATORS LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs \
        LIQUIDGOV_PORT=$PORT "$BIN") >> "$W/server.log" 2>&1 &
    SERVER=$!
    for i in $(seq 50); do
        kill -0 $SERVER 2>/dev/null || { echo "server died"; cat "$W/server.log"; exit 1; }
        curl -s -o /dev/null --max-time 1 $URL/ && return
        sleep 0.2
    done
    echo "server did not start"; cat "$W/server.log"; kill $SERVER; exit 1
}
stop() { [ -n "$SERVER" ] && kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null; SERVER=""; }
trap stop EXIT
hist() { curl -s -b "lg_session=$A" "$URL/jukebox/history?station="; }

start
out=$(python3 "$HERE/tests/e2e/juketube_history_client.py" $PORT "$A" 2>&1)
check "precondition: the two plays were sent" '[[ "$out" == PLAYED* ]]' "$out"
V1=$(awk '{print $2}' <<<"$out")
h=$(hist)
check "H1 History lists the replaced first track" '[[ "$h" == *"$V1"* ]]' "$h"

stop
start
h=$(hist)
check "H2 after a restart, History still lists it" '[[ "$h" == *"$V1"* ]]' "$h"
check "H3 the store holds it as a jh record under jbs:" 'grep "^jh" "$W/store.lgp" | grep "jbs" | grep -q "$V1"' "$(grep '^jh' "$W/store.lgp" | head -3)"

js=$(curl -s "$URL/assets/juketube.js")
check "H4 juketube.js fills History titles and copies with a fallback" \
    'grep -q "fillTitle(id)" <<<"$js" && grep -q "legacyCopy(u)" <<<"$js"' "missing"

stop
echo "passes=$passes fails=$fails"
[ $fails -eq 0 ] && echo "EXIT=0" || echo "EXIT=1"
[ $fails -eq 0 ]
