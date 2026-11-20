#!/usr/bin/env bash
# Feed filters, slice 1 (Brandon 2026-11-16 "Feed filters" + 2026-11-20 "just do it all"; wip/160).
#   /feed?filter= picks the view; the choice lives only in the address, nothing new is stored.
#   all           friends' posts, my own and my groups' posts, newest first (unchanged)
#   friends       friends' and my own posts only (unchanged)
#   committees    "Groups": posts in groups I belong to only (unchanged; this is "from my groups only")
#   discussed     "Most discussed": the All posts, most replies first; a tie keeps newest first
#   friendsfirst  "Friends first": the All posts, my friends' and my own first, then the rest; newest first in each
#   An unknown filter is All. A post from someone who is neither a friend nor in my groups never shows.
# Usage: bash feed_filters.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18146. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-feed-filters; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18146; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
# seed: Bbottin<->pal and pal<->stranger are friends; Bbottin and stranger are not
ME=$(sid Bbottin); PAL=$(sid pal); STR=$(sid stranger)
[ -n "$ME" ] && [ -n "$PAL" ] && [ -n "$STR" ] || { echo "seed gave no sessions"; exit 1; }

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
# The feed's own tab row, and the order of the marker words (F-...) in the posts of a saved page.
tabs()  { python3 -c 'import re,sys; m=re.search(r"<div class=\"segtabs\">(.*?)</div>", open(sys.argv[1]).read(), re.S); print(m.group(1) if m else "")' "$W/$1"; }
order() { python3 -c 'import re,sys; print(" ".join(re.findall(r"<p>(F-[a-z-]+)</p>", open(sys.argv[1]).read())))' "$W/$1"; }
# The id of the post whose text is $2, read from the article that holds it on page $1.
pid()   { python3 -c 'import re,sys; m=re.search(r"id=\"post-(\d+)\"(?:(?!</article>).)*?<p>"+re.escape(sys.argv[2])+"</p>", open(sys.argv[1]).read(), re.S); print(m.group(1) if m else "")' "$W/$1" "$2"; }

start
# A group the stranger owns and I join (pal is not in it).
post $STR /communities -d "slug=club&name=Club&category=10&subcategory=Bikes" >/dev/null
post $ME /c/club/join -d "" >/dev/null
# Oldest first. The ages are one second apart so newest-first is exact.
post $PAL /feed -F "text=F-pal-quiet" >/dev/null;               sleep 1.1
post $STR /c/club -F "text=F-club-busy" >/dev/null;            sleep 1.1
post $PAL /feed -F "text=F-pal-chatty" >/dev/null;             sleep 1.1
post $STR /c/club -F "text=F-club-newest" >/dev/null;          sleep 1.1
post $STR /feed -F "anybody=1" -F "text=F-outsider" >/dev/null # a public post by a non-friend, outside my groups
get $ME "/feed?filter=all" seed.html >/dev/null
BUSY=$(pid seed.html F-club-busy); CHATTY=$(pid seed.html F-pal-chatty)
check "control: the posts are in my All feed (busy=$BUSY chatty=$CHATTY)" '[ -n "$BUSY" ] && [ -n "$CHATTY" ]' "$(order seed.html)"
for who in $PAL $ME $STR; do post $who /post/$BUSY/reply -d "text=a reply" >/dev/null; done   # 3 replies
post $ME /post/$CHATTY/reply -d "text=one reply" >/dev/null                                     # 1 reply

r=$(get $ME "/feed?filter=all" all.html)
check "All answers 200" '[ "$r" = 200 ]' "$r"
check "All: newest first, unchanged" '[ "$(order all.html)" = "F-club-newest F-pal-chatty F-club-busy F-pal-quiet" ]' "$(order all.html)"
r=$(get $ME "/feed?filter=friends" fr.html)
check "Friends: friends' posts only, newest first, unchanged" '[ "$(order fr.html)" = "F-pal-chatty F-pal-quiet" ]' "$r $(order fr.html)"
r=$(get $ME "/feed?filter=committees" gr.html)
check "Groups (from my groups only): group posts only, unchanged" '[ "$(order gr.html)" = "F-club-newest F-club-busy" ]' "$r $(order gr.html)"

r=$(get $ME "/feed?filter=discussed" dis.html)
check "Most discussed answers 200" '[ "$r" = 200 ]' "$r"
check "Most discussed: most replies first, a tie stays newest first" \
    '[ "$(order dis.html)" = "F-club-busy F-pal-chatty F-club-newest F-pal-quiet" ]' "$(order dis.html)"
check "Most discussed is the current tab" 'tabs dis.html | grep -q "aria-current=\"page\" href=\"/feed?filter=discussed\">Most discussed<"' "$(tabs dis.html)"

r=$(get $ME "/feed?filter=friendsfirst" ff.html)
check "Friends first answers 200" '[ "$r" = 200 ]' "$r"
check "Friends first: friends' posts first, then my groups', newest first in each" \
    '[ "$(order ff.html)" = "F-pal-chatty F-pal-quiet F-club-newest F-club-busy" ]' "$(order ff.html)"
check "Friends first is the current tab" 'tabs ff.html | grep -q "aria-current=\"page\" href=\"/feed?filter=friendsfirst\">Friends first<"' "$(tabs ff.html)"

check "the tab row offers all five filters" \
    '(for k in all friends committees discussed friendsfirst; do tabs all.html | grep -q "filter=$k\"" || exit 1; done)' "$(tabs all.html)"
r=$(get $ME "/feed?filter=nonsense" x.html)
check "an unknown filter is All (tab and order)" \
    'tabs x.html | grep -q "aria-current=\"page\" href=\"/feed?filter=all\"" && [ "$(order x.html)" = "$(order all.html)" ]' "$r $(order x.html)"
check "a non-friend's post outside my groups shows in no view" \
    '! grep -q "F-outsider" "$W/all.html" "$W/fr.html" "$W/gr.html" "$W/dis.html" "$W/ff.html" "$W/x.html"' "leaked"
r=$(curl -s -o /dev/null -w '%{http_code}' "$URL/feed?filter=discussed")
check "signed out, a filter link still asks you to log in (no posts)" '[ "$r" = 302 ] || [ "$r" = 303 ]' "$r"

echo "passes=$passes fails=$fails"
echo "EXIT=$([ $fails -eq 0 ] && echo 0 || echo 1)"
[ $fails -eq 0 ]
