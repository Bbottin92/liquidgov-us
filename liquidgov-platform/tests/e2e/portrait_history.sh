#!/usr/bin/env bash
# Earlier self-portraits (Brandon 2026-11-16: "save and load prior self portraits"; retention 10, private unless the owner
# chooses to show them). Saving a new self-portrait moves the one it replaces into a list the owner can load back into
# the canvas on /me/portrait; the last 10 are kept; only the owner sees them unless they turn "show on my profile" on.
# Usage: bash portrait_history.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18141. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-portrait-history; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18141; URL=http://127.0.0.1:$PORT
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

mkpng() { python3 - "$1" "$2" <<'PY'
import struct, sys, zlib
path, n = sys.argv[1], int(sys.argv[2])
r, g, b = (n * 37) % 256, (n * 91 + 40) % 256, (n * 53 + 100) % 256
w = h = 32
raw = b"".join(b"\x00" + bytes([r, g, b]) * w for _ in range(h))
def ch(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + ch(b"IDAT", zlib.compress(raw)) + ch(b"IEND", b""))
PY
}
for n in $(seq 1 14); do mkpng "$W/p$n.png" $n; done

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
trap stop EXIT
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{http_code}' -b "lg_session=$s" "$@" "$URL$p"; }
get() { curl -s -o "$W/$3" -w '%{http_code}' ${1:+-b "lg_session=$1"} "$URL$2"; }
save() { post $PAL /me/avatar -F "photo=@$W/p$1.png;type=image/png" >/dev/null; }   # pal saves picture n as the self-portrait
# how many earlier portraits pal can load: /me/portrait/old/0, 1, ... until one is not served
hist_count() { local n=0; while [ "$(get $PAL /me/portrait/old/$n x.png)" = 200 ]; do n=$((n+1)); [ $n -gt 30 ] && break; done; echo $n; }

start
r=$(get $PAL /me/portrait portrait0.html)
check "/me/portrait answers 200" '[ "$r" = 200 ]' "$r"
check "no portrait yet: no Earlier portraits card" '! grep -q "Earlier portraits" $W/portrait0.html' "card shown"

save 1
check "first portrait saved: nothing earlier to load" '[ "$(hist_count)" = 0 ]' "$(hist_count)"
get $PAL /portrait/pal cur1.png >/dev/null
save 2
check "second portrait saved: the first one is now an earlier portrait" '[ "$(hist_count)" = 1 ]' "$(hist_count)"
get $PAL /me/portrait/old/0 old0.png >/dev/null
check "...and it is byte for byte the portrait that was current before" 'cmp -s $W/old0.png $W/cur1.png' "differs"
check "...served as an image" 'file -b $W/old0.png | grep -qi "PNG image"' "$(file -b $W/old0.png)"

get $PAL /portrait/pal cur2.png >/dev/null
save 3
get $PAL /me/portrait/old/1 old1.png >/dev/null
check "newest earlier portrait is last (index 1) and is the one replaced by the third save" 'cmp -s $W/old1.png $W/cur2.png' "differs"
check "...the oldest is still first (index 0)" 'get $PAL /me/portrait/old/0 again0.png >/dev/null; cmp -s $W/again0.png $W/cur1.png' "differs"

r=$(get $PAL /me/portrait portrait3.html)
check "the owner's page lists them: a Load button for Current and for each earlier one" '[ "$(grep -o "oldportrait" $W/portrait3.html | wc -l)" -ge 3 ]' "$(grep -o 'oldportrait' $W/portrait3.html | wc -l)"
check "...each earlier one points at its picture" 'grep -q "/me/portrait/old/0" $W/portrait3.html && grep -q "/me/portrait/old/1" $W/portrait3.html' "no thumbs"
check "...and the page can load a click into the canvas (canvas.js is the engine)" 'grep -q "/assets/canvas.js" $W/portrait3.html' "no canvas.js"

# Same picture again: nothing new is kept.
get $PAL /portrait/pal cur3.png >/dev/null
before=$(hist_count)
save 3
check "saving the same picture again keeps nothing new" '[ "$(hist_count)" = "$before" ]' "$(hist_count) vs $before"
check "...and the current portrait is unchanged" 'get $PAL /portrait/pal cur3b.png >/dev/null; cmp -s $W/cur3.png $W/cur3b.png' "changed"

# Private by default.
check "a stranger cannot open pal's earlier portrait" '[ "$(get $STRANGER /portrait-history/pal/0 s0.png)" = 404 ]' "$(get $STRANGER /portrait-history/pal/0 s0.png)"
check "someone not signed in cannot either" '[ "$(get "" /portrait-history/pal/0 n0.png)" = 404 ]' "$(get "" /portrait-history/pal/0 n0.png)"
check "the owner-only URL does not show pal's pictures to a stranger" '[ "$(get $STRANGER /me/portrait/old/0 s1.png)" = 404 ]' "$(get $STRANGER /me/portrait/old/0 s1.png)"
check "a signed-out visitor is not served the owner-only URL" '[ "$(get "" /me/portrait/old/0 n1.png)" != 200 ]' "$(get "" /me/portrait/old/0 n1.png)"
get $STRANGER /u/pal upal.html >/dev/null
check "pal's public page shows no earlier portraits by default" '! grep -q "Earlier portraits" $W/upal.html' "shown"
check "a stranger cannot delete pal's earlier portraits" 'post $STRANGER /me/portrait/old/delete -d n=0 >/dev/null; [ "$(hist_count)" = "$before" ]' "$(hist_count)"

# The owner's choice to show them.
check "pal turns 'show on my profile' on" '[ "$(post $PAL /me/portrait/old/share -d on=1)" = 303 ]' "not 303"
check "a stranger can now open the earlier portrait" '[ "$(get $STRANGER /portrait-history/pal/0 s2.png)" = 200 ]' "$(get $STRANGER /portrait-history/pal/0 s2.png)"
check "...it is the same picture" 'cmp -s $W/s2.png $W/cur1.png' "differs"
get $STRANGER /u/pal upal2.html >/dev/null
check "pal's public page now shows the strip" 'grep -q "Earlier portraits" $W/upal2.html && grep -q "/portrait-history/pal/0" $W/upal2.html' "not shown"
check "an index past the list is a 404" '[ "$(get $STRANGER /portrait-history/pal/99 s3.png)" = 404 ]' "$(get $STRANGER /portrait-history/pal/99 s3.png)"
check "a bad index is a 404, not an error" '[ "$(get $STRANGER /portrait-history/pal/abc s4.png)" = 404 ] && [ "$(get $STRANGER /portrait-history/pal/1x s5.png)" = 404 ] && [ "$(get $STRANGER /portrait-history/pal/ s7.png)" != 200 ]' "bad index"
check "pal turns it off again" '[ "$(post $PAL /me/portrait/old/share -d on=0)" = 303 ] && [ "$(get $STRANGER /portrait-history/pal/0 s6.png)" = 404 ]' "still visible"

# Retention: the last 10.
for n in 4 5 6 7 8 9 10 11 12 13 14; do save $n; done
check "after 14 saves 10 earlier portraits are kept" '[ "$(hist_count)" = 10 ]' "$(hist_count)"
get $PAL /me/portrait/old/9 newest.png >/dev/null; get $PAL /portrait/pal cur14.png >/dev/null
check "the newest earlier one is the 13th portrait, not the current 14th" '! cmp -s $W/newest.png $W/cur14.png' "same as current"

# Delete one.
check "delete earlier portrait 0 (as its owner)" '[ "$(post $PAL /me/portrait/old/delete -d n=0)" = 303 ] && [ "$(hist_count)" = 9 ]' "$(hist_count)"
check "a bad delete index changes nothing" 'post $PAL /me/portrait/old/delete -d n=99 >/dev/null; post $PAL /me/portrait/old/delete -d n=x >/dev/null; [ "$(hist_count)" = 9 ]' "$(hist_count)"

# Survives a restart (the picture files are adopted again on start).
get $PAL /me/portrait/old/0 beforerestart.png >/dev/null
stop; start
check "after a restart the earlier portraits are still there" '[ "$(hist_count)" = 9 ]' "$(hist_count)"
get $PAL /me/portrait/old/0 afterrestart.png >/dev/null
check "...with the same pictures" 'cmp -s $W/beforerestart.png $W/afterrestart.png' "differs"

# The store holds the records the format doc names.
stop
check "the store has avh rows" '[ "$(grep -c "^avh	" $W/store.lgp)" = 9 ]' "$(grep -c '^avh' $W/store.lgp)"

echo "passes=$passes fails=$fails"
[ $fails = 0 ]
