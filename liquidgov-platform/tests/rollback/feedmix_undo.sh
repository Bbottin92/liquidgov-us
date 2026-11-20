#!/usr/bin/env bash
# Rollback check for the feed's mix-in setting: deploy/feedmix-undo.py must let the version BEFORE the mix-in open a
# store the new version wrote, removing only the fmix records (each person's mix-in choice) and keeping everything else byte for byte.
# Not under tests/e2e: like city_undo it needs the previous build, which lgcheck.sh and CI do not pass. Run it by hand
# for this LGP1 format change (and before a rollback):
# Usage: bash feedmix_undo.sh <new liquidgov_http> <liquidgov_e2e_seed> <old liquidgov_http> <feedmix-undo.py>
# Throwaway store, port 18156 (outside naming.toml's e2e range). Never touches the live site.
set -u
NEW=$1; SEED=$2; OLD=$3; UNDO=$4
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-feedmix-undo; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18156; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
for b in "$NEW" "$SEED" "$OLD"; do [ -x "$b" ] || { echo "missing binary $b"; exit 2; }; done
[ -f "$UNDO" ] || { echo "missing $UNDO"; exit 2; }
[ "$(readlink -f "$NEW")" != "$(readlink -f "$OLD")" ] || { echo "old and new are the same file"; exit 2; }

$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
ADMIN=$(sid Bbottin); PAL=$(sid pal); STRANGER=$(sid stranger)
[ -n "$ADMIN" ] && [ -n "$PAL" ] && [ -n "$STRANGER" ] || { echo "seed gave no sessions"; exit 1; }

SERVER=""
start() { # binary -> 0 if it serves, 1 if it exits
    (cd "$HERE" && exec env -u LIQUIDGOV_OPERATORS LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT \
        "$1") >> "$W/server.log" 2>&1 &
    SERVER=$!
    for i in $(seq 50); do
        kill -0 $SERVER 2>/dev/null || { wait $SERVER 2>/dev/null; SERVER=""; return 1; }
        curl -s -o /dev/null --max-time 1 $URL/ && return 0
        sleep 0.2
    done
    kill $SERVER; SERVER=""; return 1
}
stop() { [ -n "$SERVER" ] && kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null; SERVER=""; }
trap stop EXIT
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -b "lg_session=$s" "$@" "$URL$p"; }
get()  { curl -s -o "$W/$3" -w '%{http_code}' -b "lg_session=$1" "$URL$2"; }

# 0. A store where nobody has touched the mix-in still opens on the old version (the seeded accounts have no fmix rows).
start "$NEW" || { echo "new server did not start"; exit 1; }
stop
check "nobody chose: the new version wrote no fmix rows" '! grep -q "^fmix	" $W/store.lgp' "$(grep '^fmix' $W/store.lgp)"
if start "$OLD"; then ok "nobody chose: the old version opens the store as it is"; stop
else bad "nobody chose: the old version opens the store as it is ($(tail -2 $W/server.log))"; fi

# 1. The new version writes the choice, next to ordinary records.
start "$NEW" || { echo "new server did not start"; exit 1; }
post $PAL /feed/mix -d "view=friends&every=3&tab_every=5"
post $STRANGER /feed/mix -d "view=all&every=0"
post $PAL /feed -F "text=Ordinary feed post survives"
stop

for t in fmix; do
    check "precondition: store has $t" 'grep -q "^$t	" $W/store.lgp' "none"
done
grep -vE '^(fmix)	' "$W/store.lgp" > "$W/expected.lgp"
cp "$W/store.lgp" "$W/before.lgp"

# 2. The problem: the old version refuses this store.
if start "$OLD"; then bad "precondition: old version refuses the store with choices (it started)"; stop
else ok "precondition: old version refuses the store with choices"; fi
check "the old version says why" 'grep -q "unknown record type" $W/server.log' "$(tail -2 $W/server.log)"
check "the refused start left the store alone" 'cmp -s $W/store.lgp $W/before.lgp' "changed"

# 3. Dry run changes nothing.
python3 "$UNDO" "$W/store.lgp" > "$W/dry.txt" 2>&1; rc=$?
check "dry run exits 0" '[ $rc = 0 ]' "rc=$rc"
check "dry run counts the fmix records" 'grep -q "fmix records to remove: 2" $W/dry.txt' "$(cat $W/dry.txt)"
check "dry run left the store alone" 'cmp -s $W/store.lgp $W/before.lgp' "changed"
check "dry run made no backup" '! ls $W/store.lgp.pre-feedmix-undo-* >/dev/null 2>&1' "backup made"

# 4. Refusals.
printf 'NOTLGP1\nfmix\ta\t3\n' > "$W/bad.lgp"
python3 "$UNDO" "$W/bad.lgp" --go > /dev/null 2>&1; rc=$?
check "a non-LGP1 file is refused" '[ $rc = 1 ] && grep -q "^NOTLGP1" $W/bad.lgp' "rc=$rc"
python3 "$UNDO" "$W/missing.lgp" --go > /dev/null 2>&1; rc=$?
check "a missing file is refused" '[ $rc = 1 ]' "rc=$rc"
python3 "$UNDO" > /dev/null 2>&1; rc=$?
check "no argument prints usage" '[ $rc = 2 ]' "rc=$rc"

# 5. The real run.
chmod 600 "$W/store.lgp"
python3 "$UNDO" "$W/store.lgp" --go > "$W/go.txt" 2>&1; rc=$?
check "undo exits 0" '[ $rc = 0 ]' "rc=$rc $(cat $W/go.txt)"
check "undo kept every other record byte for byte, in order" 'cmp -s $W/store.lgp $W/expected.lgp' "differs"
bk=$(ls "$W"/store.lgp.pre-feedmix-undo-* 2>/dev/null | head -1)
check "the backup is the untouched store" '[ -n "$bk" ] && cmp -s "$bk" $W/before.lgp' "no backup"
check "the backup is private (0600)" '[ "$(stat -c %a "$bk")" = 600 ]' "$(stat -c %a "$bk")"
check "the store keeps its mode" '[ "$(stat -c %a $W/store.lgp)" = 600 ]' "$(stat -c %a $W/store.lgp)"
check "no temp file left" '[ ! -e $W/store.lgp.feedmix-undo.tmp ]' "tmp left"
python3 "$UNDO" "$W/store.lgp" --go > "$W/again.txt" 2>&1; rc=$?
check "running it again removes nothing" '[ $rc = 0 ] && grep -q "nothing to remove" $W/again.txt && cmp -s $W/store.lgp $W/expected.lgp' "$(cat $W/again.txt)"

# 6. The old version opens it, with everything else intact.
if start "$OLD"; then ok "old version starts on the undone store"; else bad "old version starts on the undone store ($(tail -2 $W/server.log))"; fi
if [ -n "$SERVER" ]; then
    get $PAL /feed feed.html >/dev/null
    check "feed post kept" 'grep -q "Ordinary feed post survives" $W/feed.html' "missing"
    stop
fi

# 7. Rolling forward again: without the rows everyone has the mix-in off; with them put back, pal's choices are back.
if start "$NEW"; then ok "the new version starts on the undone store"; else bad "new version on the undone store ($(tail -2 $W/server.log))"; fi
if [ -n "$SERVER" ]; then
    get $PAL "/feed?filter=friends" f1.html >/dev/null
    check "without the rows pal's mix-in is off" 'grep -q "<summary>Mix-in: Off</summary>" $W/f1.html' "$(grep -o '<summary>Mix-in: [A-Za-z]*' $W/f1.html)"
    stop
fi
grep -E '^(fmix)	' "$bk" >> "$W/store.lgp"
if start "$NEW"; then ok "the new version starts with the fmix lines put back"; else bad "restart with lines back ($(tail -2 $W/server.log))"; fi
if [ -n "$SERVER" ]; then
    get $PAL "/feed?filter=friends" f2.html >/dev/null
    check "pal's choices are back (Friends 1 in 5: Sometimes)" 'grep -q "<summary>Mix-in: Sometimes</summary>" $W/f2.html' "$(grep -o '<summary>Mix-in: [A-Za-z]*' $W/f2.html)"
    stop
fi

echo "passes=$passes fails=$fails"
[ $fails = 0 ]
