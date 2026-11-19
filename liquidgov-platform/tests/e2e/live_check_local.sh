#!/usr/bin/env bash
# The deploy's own live check (deploy/city-live-check.py) run against THIS build before any deploy (2026-11-19).
# Why: on the sechead1 go the live check still asserted the old framing behavior (no anti-framing headers on a normal
# page) and printed FAILS 2 only after the program was live (lesson deploying-site.md:81). Run here, a checker that
# disagrees with the build fails in the local test run, before the dry run.
#   - every program-served check of city-live-check.py passes against a fresh server (FAILS 0)
#   - the nginx-only checks are skipped and named (SKIPPED line), never counted as passed
# Throwaway store, port 18145. Never touches the live site.
# Usage: bash live_check_local.sh <path to liquidgov_http> <path to liquidgov_e2e_seed> [path to city-live-check.py]
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
CHECKER=${3:-$HERE/../deploy/city-live-check.py}
W=/tmp/lg-live-check-local; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18145; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }

[ -f "$CHECKER" ] || { echo "FAIL the live checker is not at $CHECKER"; echo "passes=0 fails=1"; echo "EXIT=1"; exit 1; }
if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }

(cd "$HERE" && exec env -u LIQUIDGOV_OPERATORS LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT \
    "$BIN") >> "$W/server.log" 2>&1 &
SERVER=$!
up=0
for i in $(seq 50); do
    kill -0 $SERVER 2>/dev/null || { echo "server died"; cat "$W/server.log"; exit 1; }
    curl -s -o /dev/null --max-time 1 $URL/ && { up=1; break; }
    sleep 0.2
done
[ $up = 1 ] || { echo "server did not start"; kill $SERVER; exit 1; }

out=$(python3 "$CHECKER" "$URL" 2>&1); rc=$?
echo "$out" | sed 's/^/  checker: /'
if [ $rc = 0 ] && echo "$out" | grep -qx "FAILS 0"; then ok "city-live-check.py: FAILS 0 against this build"
else bad "city-live-check.py against this build (rc=$rc; its FAIL lines above)"; fi
if echo "$out" | grep -q "^SKIPPED commons, commons-post"; then ok "the nginx-only checks are skipped and named"
else bad "no SKIPPED line for the nginx-only checks"; fi
if echo "$out" | grep -qE "^(OK|FAIL) +commons"; then bad "a commons check ran against a local build"; else ok "no commons check ran locally"; fi

kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null
if grep -qiE "abort|segfault|terminate called" "$W/server.log"; then bad "server log has crash lines"; else ok "server log has no crash lines"; fi
echo "passes=$passes fails=$fails"
echo "EXIT=$fails"
[ $fails = 0 ]
