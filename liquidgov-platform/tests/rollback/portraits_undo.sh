#!/usr/bin/env bash
# Rollback check for earlier self-portraits: deploy/portraits-undo.py must let the version BEFORE portrait history open a
# store the new version wrote, removing only the avh/avhs records (each person's earlier portraits and their "show on
# my profile" choice) and keeping everything else byte for byte, the current portrait (av) included.
# Not under tests/e2e: like city_undo and hints_undo it needs the previous build, which lgcheck.sh and CI do not pass.
# Run it by hand for this LGP1 format change (and before a rollback):
# Usage: bash portraits_undo.sh <new liquidgov_http> <liquidgov_e2e_seed> <old liquidgov_http> <portraits-undo.py>
# Throwaway store, port 18143 (outside naming.toml's e2e range). Never touches the live site.
set -u
NEW=$1; SEED=$2; OLD=$3; UNDO=$4
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-portraits-undo; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18143; URL=http://127.0.0.1:$PORT
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

# A real PNG of one flat colour (the site decodes and re-encodes what it is given).
mkpng() { python3 - "$1" "$2" "$3" "$4" <<'PY'
import struct, sys, zlib
path, r, g, b = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
w = h = 32
raw = b"".join(b"\x00" + bytes([r, g, b]) * w for _ in range(h))
def ch(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + ch(b"IDAT", zlib.compress(raw)) + ch(b"IEND", b""))
PY
}
mkpng "$W/red.png" 200 30 30; mkpng "$W/green.png" 30 200 30; mkpng "$W/blue.png" 30 30 200

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

# 0. A store where nobody has saved a second portrait still opens on the old version (no avh rows).
start "$NEW" || { echo "new server did not start"; exit 1; }
post $PAL /me/avatar -F "photo=@$W/red.png;type=image/png"
stop
check "one portrait only: the new version wrote no avh rows" '! grep -q "^avh	" $W/store.lgp' "$(grep '^avh' $W/store.lgp)"
if start "$OLD"; then ok "one portrait only: the old version opens the store as it is"; stop
else bad "one portrait only: the old version opens the store as it is ($(tail -2 $W/server.log))"; fi

# 1. The new version writes the history and the choice, next to ordinary records.
start "$NEW" || { echo "new server did not start"; exit 1; }
post $PAL /me/avatar -F "photo=@$W/green.png;type=image/png"
post $PAL /me/avatar -F "photo=@$W/blue.png;type=image/png"
post $PAL /me/portrait/old/share -d on=1
post $PAL /feed -F "text=Ordinary feed post survives"
stop

for t in avh avhs; do
    check "precondition: store has $t" 'grep -q "^$t	" $W/store.lgp' "none"
done
check "precondition: two earlier portraits (red, green) are kept" '[ "$(grep -c "^avh	" $W/store.lgp)" = 2 ]' "$(grep -c '^avh' $W/store.lgp)"
check "precondition: the current portrait (av) is still there" 'grep -q "^av	" $W/store.lgp' "no av row"
grep -vE '^(avh|avhs)	' "$W/store.lgp" > "$W/expected.lgp"
cp "$W/store.lgp" "$W/before.lgp"

# 2. The problem: the old version refuses this store.
if start "$OLD"; then bad "precondition: old version refuses the store with portrait history (it started)"; stop
else ok "precondition: old version refuses the store with portrait history"; fi
check "the old version says why" 'grep -q "unknown record type" $W/server.log' "$(tail -2 $W/server.log)"
check "the refused start left the store alone" 'cmp -s $W/store.lgp $W/before.lgp' "changed"

# 3. Dry run changes nothing.
python3 "$UNDO" "$W/store.lgp" > "$W/dry.txt" 2>&1; rc=$?
check "dry run exits 0" '[ $rc = 0 ]' "rc=$rc"
check "dry run counts the portrait history records" 'grep -q "portrait history records to remove: 3" $W/dry.txt' "$(cat $W/dry.txt)"
check "dry run left the store alone" 'cmp -s $W/store.lgp $W/before.lgp' "changed"
check "dry run made no backup" '! ls $W/store.lgp.pre-portraits-undo-* >/dev/null 2>&1' "backup made"

# 4. Refusals.
printf 'NOTLGP1\navh\ta\t1\n' > "$W/bad.lgp"
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
bk=$(ls "$W"/store.lgp.pre-portraits-undo-* 2>/dev/null | head -1)
check "the backup is the untouched store" '[ -n "$bk" ] && cmp -s "$bk" $W/before.lgp' "no backup"
check "the backup is private (0600)" '[ "$(stat -c %a "$bk")" = 600 ]' "$(stat -c %a "$bk")"
check "the store keeps its mode" '[ "$(stat -c %a $W/store.lgp)" = 600 ]' "$(stat -c %a $W/store.lgp)"
check "no temp file left" '[ ! -e $W/store.lgp.portraits-undo.tmp ]' "tmp left"
python3 "$UNDO" "$W/store.lgp" --go > "$W/again.txt" 2>&1; rc=$?
check "running it again removes nothing" '[ $rc = 0 ] && grep -q "nothing to remove" $W/again.txt && cmp -s $W/store.lgp $W/expected.lgp' "$(cat $W/again.txt)"

# 6. The old version opens it, with everything else intact (pal still has the current portrait).
if start "$OLD"; then ok "old version starts on the undone store"; else bad "old version starts on the undone store ($(tail -2 $W/server.log))"; fi
if [ -n "$SERVER" ]; then
    get $PAL /feed feed.html >/dev/null
    check "feed post kept" 'grep -q "Ordinary feed post survives" $W/feed.html' "missing"
    r=$(get $PAL /portrait/pal portrait.png)
    check "the current portrait is still served" '[ "$r" = 200 ]' "$r"
    stop
fi

# 7. Rolling forward again: without the rows pal has no earlier portraits; with them put back, they are back.
if start "$NEW"; then ok "the new version starts on the undone store"; else bad "new version on the undone store ($(tail -2 $W/server.log))"; fi
if [ -n "$SERVER" ]; then
    r=$(get $PAL /me/portrait/old/0 old0.png)
    check "without the rows there is no earlier portrait 0" '[ "$r" = 404 ]' "$r"
    stop
fi
grep -E '^(avh|avhs)	' "$bk" >> "$W/store.lgp"
if start "$NEW"; then ok "the new version starts with the avh lines put back"; else bad "restart with lines back ($(tail -2 $W/server.log))"; fi
if [ -n "$SERVER" ]; then
    r=$(get $PAL /me/portrait/old/0 old0.png)
    check "pal's earlier portrait 0 is back" '[ "$r" = 200 ]' "$r"
    stop
fi

echo "passes=$passes fails=$fails"
[ $fails = 0 ]
