#!/usr/bin/env bash
# Rollback check for the profile style library: deploy/styles-undo.py must let the version BEFORE the library open a
# store the new version wrote, removing only sty/styn/stu/str and keeping everything else byte for byte, including the
# Profile CSS someone copied from the library (it lives in their own ac record).
# Not under tests/e2e: like city_undo it needs the previous build, which lgcheck.sh and CI do not pass. Run it by hand
# for this LGP1 format change (and before a rollback):
# Usage: bash styles_undo.sh <new liquidgov_http> <liquidgov_e2e_seed> <old liquidgov_http> <styles-undo.py>
# Throwaway store, port 18141 (outside naming.toml's e2e range). Never touches the live site.
set -u
NEW=$1; SEED=$2; OLD=$3; UNDO=$4
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-styles-undo; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18141; URL=http://127.0.0.1:$PORT
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

# 1. The new version writes every kind of style record, next to ordinary ones.
start "$NEW" || { echo "new server did not start"; exit 1; }
post $PAL /me/css --data-urlencode "css=.lg-theme h1{color:#7e3fa1}"
post $PAL /styles/publish --data-urlencode "name=Night Paper"
post $STRANGER /styles/use -d "id=$(awk -F'\t' '$1=="sty"{print $2; exit}' "$W/store.lgp")"
post $ADMIN /styles/report -d "id=$(awk -F'\t' '$1=="sty"{print $2; exit}' "$W/store.lgp")" --data-urlencode "reason=test"
post $PAL /feed -F "text=Ordinary feed post survives"
stop

for t in sty styn stu str; do
    check "precondition: store has $t" 'grep -q "^$t	" $W/store.lgp' "none"
done
check "precondition: stranger's copied CSS is in ac" '[ "$(grep -c "^ac	.*7e3fa1" $W/store.lgp)" = 2 ]' "$(grep -c '^ac' $W/store.lgp)"
grep -vE '^(sty|styn|stu|str)	' "$W/store.lgp" > "$W/expected.lgp"
cp "$W/store.lgp" "$W/before.lgp"

# 2. The problem: the old version refuses this store.
if start "$OLD"; then bad "precondition: old version refuses the style store (it started)"; stop
else ok "precondition: old version refuses the style store"; fi
check "the old version says why" 'grep -q "unknown record type" $W/server.log' "$(tail -2 $W/server.log)"
check "the refused start left the store alone" 'cmp -s $W/store.lgp $W/before.lgp' "changed"

# 3. Dry run changes nothing.
python3 "$UNDO" "$W/store.lgp" > "$W/dry.txt" 2>&1; rc=$?
check "dry run exits 0" '[ $rc = 0 ]' "rc=$rc"
check "dry run counts the style records" 'grep -q "style records to remove: [1-9]" $W/dry.txt' "$(cat $W/dry.txt)"
check "dry run left the store alone" 'cmp -s $W/store.lgp $W/before.lgp' "changed"
check "dry run made no backup" '! ls $W/store.lgp.pre-styles-undo-* >/dev/null 2>&1' "backup made"

# 4. Refusals.
printf 'NOTLGP1\nsty\ta\tb\n' > "$W/bad.lgp"
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
bk=$(ls "$W"/store.lgp.pre-styles-undo-* 2>/dev/null | head -1)
check "the backup is the untouched store" '[ -n "$bk" ] && cmp -s "$bk" $W/before.lgp' "no backup"
check "the backup is private (0600)" '[ "$(stat -c %a "$bk")" = 600 ]' "$(stat -c %a "$bk")"
check "the store keeps its mode" '[ "$(stat -c %a $W/store.lgp)" = 600 ]' "$(stat -c %a $W/store.lgp)"
check "no temp file left" '[ ! -e $W/store.lgp.styles-undo.tmp ]' "tmp left"
python3 "$UNDO" "$W/store.lgp" --go > "$W/again.txt" 2>&1; rc=$?
check "running it again removes nothing" '[ $rc = 0 ] && grep -q "nothing to remove" $W/again.txt && cmp -s $W/store.lgp $W/expected.lgp' "$(cat $W/again.txt)"

# 6. The old version opens it, with everything else intact, copied CSS included.
if start "$OLD"; then ok "old version starts on the undone store"; else bad "old version starts on the undone store ($(tail -2 $W/server.log))"; fi
if [ -n "$SERVER" ]; then
    get $PAL /feed feed.html >/dev/null
    check "feed post kept" 'grep -q "Ordinary feed post survives" $W/feed.html' "missing"
    get $STRANGER /u/stranger u.html >/dev/null
    check "stranger's copied Profile CSS kept" 'grep -q "7e3fa1" $W/u.html' "missing"
    get $PAL /u/pal p.html >/dev/null
    check "pal's own Profile CSS kept" 'grep -q "7e3fa1" $W/p.html' "missing"
    stop
fi

# 7. Rolling forward again with the backup's style lines brings the library back.
grep -E '^(sty|styn|stu|str)	' "$bk" >> "$W/store.lgp"
if start "$NEW"; then ok "the new version starts with the style lines put back"; else bad "new version restart ($(tail -2 $W/server.log))"; fi
if [ -n "$SERVER" ]; then
    get "" /styles s.html >/dev/null
    check "the library is back" 'grep -q "Night Paper" $W/s.html' "missing"
    stop
fi

echo "passes=$passes fails=$fails"
[ $fails = 0 ]
