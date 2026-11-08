#!/usr/bin/env bash
# Operator moderation of runs for office (Brandon 2026-11-08: "what about officials?"). A run on /officials is a
# proposal marked as an office, so /admin/proposals lists it (labelled "Officials: <place>"), and Edit / Delete work
# on it like any proposal; the delete is archived first.
# Usage: bash officials_moderation.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18090. Never touches the live site.
set -u
BIN=$1; SEED=$2
W=/tmp/lg-officials-moderation; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18090; URL=http://127.0.0.1:$PORT
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
    env LIQUIDGOV_OPERATORS=Bbottin LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT \
        "$BIN" >> "$W/server.log" 2>&1 &
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
get()  { curl -s -o "$W/$3" -w '%{http_code} %{redirect_url}' -b "lg_session=$1" "$URL$2"; }
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{http_code} %{redirect_url}' -b "lg_session=$s" "$@" "$URL$p"; }

start
post $PAL /officials --data-urlencode "title=TROLL for mayor" --data-urlencode "place=Fresno, CA" --data-urlencode $'options=Yes\nNo' > /dev/null
oid=$(awk -F'\t' '$1=="prop" && $0 ~ /TROLL/ {print $2}' "$W/store.lgp" | head -1)
check "precondition: the office run is stored" '[ -n "$oid" ]' "none"
r=$(get $ADMIN /officials o1.html); check "precondition: it shows on /officials" 'grep -q "TROLL for mayor" $W/o1.html' "$r"
r=$(get $ADMIN /admin/proposals ap.html); check "the operator list shows it, labelled as an office run" 'grep -q "TROLL for mayor" $W/ap.html && grep -q "Officials: Fresno, CA" $W/ap.html' "$r"
r=$(post $ADMIN /admin/proposals/edit -d "id=$oid" --data-urlencode "title=Mayor of Fresno" --data-urlencode "body=Fresno, CA")
r=$(get $ADMIN /officials o2.html); check "an edit shows on /officials" 'grep -q "Mayor of Fresno" $W/o2.html && ! grep -q "TROLL" $W/o2.html' "$r"
r=$(post $ADMIN /admin/proposals/delete -d "id=$oid"); check "delete works on an office run" '[[ "$r" == *"done=deleted"* ]]' "$r"
r=$(get $ADMIN /officials o3.html); check "it is gone from /officials" '! grep -q "Mayor of Fresno" $W/o3.html' "$r"
check "the archive kept a copy" 'grep -q "Mayor of Fresno" "$W/store.lgp.moderation.jsonl"' "no archive line"
echo "passes=$passes fails=$fails"
[ "$fails" = 0 ]
