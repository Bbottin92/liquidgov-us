#!/usr/bin/env bash
# A profile's Juketube "Recently played" collapses repeats (Brandon 2026-11-11 4:37 PM, Brief 4 item 6; site-look2
# FINDINGS 3: nine of ten rows were the same track, each "watch again" with no title).
#   - one row per track (li.jbhist-row data-yt="<id>"), most recently played first
#   - each row: a title link (a.jbtitle to the YouTube watch page; the title itself is filled in by the page from
#     /jukebox/oembed), its play count ("10 plays" / "1 play") and the last played date ("last YYYY-MM-DD")
#   - at most 5 rows show; the rest sit under a "Show more" (details/summary, works without script)
#   - the owner and a visitor see the same list
# Plays are made the real way: the owner queues tracks (/me/jukebox/add) and the widget's completion call
# (/u/<owner>/jukebox/complete) logs each play. Order: A x9, B, C, D, E, F, G, then A again.
# Usage: bash juketube_history_collapse.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18111. Never touches the live site. New file: committed suites are frozen.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-juketube-history; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18111; URL=http://127.0.0.1:$PORT
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
get()  { curl -s -o "$W/$3" -w '%{http_code}' -b "lg_session=$1" "$URL$2"; }
A=AAAAAAAAAA1; B=BBBBBBBBBB2; C=CCCCCCCCCC3; D=DDDDDDDDDD4; E=EEEEEEEEEE5; F=FFFFFFFFFF6; G=GGGGGGGGGG7
TODAY=$(date -u +%F)
# rows of the Recently played block: "<id>|<where: top or more>|<row text>" per line, in page order
rows() { python3 - "$W/$1" <<'PY'
import re, sys, html
s = open(sys.argv[1]).read()
i = s.find("Recently played")
if i < 0: sys.exit(0)
blk = s[i:]
more = blk.find("<details")
for m in re.finditer(r'<li[^>]*class="[^"]*jbhist-row[^"]*"[^>]*data-yt="([^"]+)"[^>]*>(.*?)</li>', blk, re.S):
    where = "more" if 0 <= more < m.start() else "top"
    text = re.sub(r"\s+", " ", html.unescape(re.sub(r"<[^>]+>", " ", m.group(2)))).strip()
    print("%s|%s|%s" % (m.group(1), where, text))
PY
}
# the title link of a row: href and class
titlelink() { python3 -c 'import re,sys; s=open(sys.argv[1]).read(); m=re.search(r"<li[^>]*data-yt=\""+sys.argv[2]+r"\"[^>]*>(.*?)</li>", s, re.S); a=m and re.search(r"<a[^>]*class=\"[^\"]*jbtitle[^\"]*\"[^>]*>", m.group(1)); print(a.group(0) if a else "")' "$W/$1" "$2"; }

start
for v in $A $B $C $D $E $F $G; do post $ADMIN /me/jukebox/add --data-urlencode "url=https://www.youtube.com/watch?v=$v" >/dev/null; done
play() { post $ADMIN /u/Bbottin/jukebox/complete -d "video_id=$1" >/dev/null; sleep 0.05; }
for i in 1 2 3 4 5 6 7 8 9; do play $A; done
for v in $B $C $D $E $F $G $A; do play $v; done

r=$(get $PAL /u/Bbottin p.html)
check "visitor: /u/Bbottin answers 200 with Recently played" '[ "$r" = 200 ] && grep -q "Recently played" $W/p.html' "$r"
rows p.html > "$W/rows.txt"
check "one row per track: 7 rows for 7 tracks (16 plays)" '[ "$(wc -l < $W/rows.txt)" = 7 ]' "$(cat $W/rows.txt)"
check "track A has exactly one row" '[ "$(grep -c "^$A|" $W/rows.txt)" = 1 ]' "$(cat $W/rows.txt)"
check "... most recently played first: A, then G, F, E, D, C, B" \
    '[ "$(cut -d"|" -f1 $W/rows.txt | tr "\n" " ")" = "$A $G $F $E $D $C $B " ]' "$(cut -d"|" -f1 $W/rows.txt | tr "\n" " ")"
check "... A's row says 10 plays" 'grep "^$A|" $W/rows.txt | grep -q "10 plays"' "$(grep "^$A|" $W/rows.txt)"
check "... and its last played date (last $TODAY)" 'grep "^$A|" $W/rows.txt | grep -q "last $TODAY"' "$(grep "^$A|" $W/rows.txt)"
check "B's row says 1 play (singular)" 'grep "^$B|" $W/rows.txt | grep -q "1 play\b" && ! grep "^$B|" $W/rows.txt | grep -q "1 plays"' "$(grep "^$B|" $W/rows.txt)"
check "at most 5 rows show; the other 2 are under Show more" \
    '[ "$(grep -c "|top|" $W/rows.txt)" = 5 ] && [ "$(grep -c "|more|" $W/rows.txt)" = 2 ] && grep -q "<summary[^>]*>Show more</summary>" $W/p.html' "$(cut -d"|" -f1,2 $W/rows.txt | tr "\n" " ")"
check "each row's title links to the track (a.jbtitle to youtube watch)" \
    'titlelink p.html $A | grep -q "youtube.com/watch?v=$A"' "$(titlelink p.html $A)"
check "the page fills in titles from /jukebox/oembed" 'grep -q "/jukebox/oembed?id=" $W/p.html && grep -q "jbtitle" $W/p.html' "missing"
check "no row says watch again any more" '! grep -q "watch again" $W/rows.txt' "$(grep "watch again" $W/rows.txt | head -2)"

r=$(get $ADMIN /u/Bbottin o.html)
rows o.html > "$W/rows_o.txt"
check "the owner sees the same list" '[ "$r" = 200 ] && diff -q $W/rows.txt $W/rows_o.txt >/dev/null' "$(diff $W/rows.txt $W/rows_o.txt | head -4)"

echo "passes=$passes fails=$fails"
[ $fails -eq 0 ]
