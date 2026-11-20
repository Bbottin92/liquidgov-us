#!/usr/bin/env bash
# Feed filters, slice 2: Unread since you last looked (Brandon 2026-11-16 "Feed filters"; wip/161).
#   /feed?filter=unread shows the All posts newer than the last one this browser was shown in the Unread tab, without
#   my own posts, and marks them seen: the lg_feedseen=<user>.<post id> cookie (per browser; nothing stored on the
#   server). wip/171: an older post with a new reply from someone else is unread again (cookie
#   <user>.<post id>.<reply id>; the older <user>.<post id> cookie counts replies from the next visit). The tab reads "Unread (N)" while N > 0. Opening another tab marks nothing seen. A cookie for another
#   account, or a garbled one, counts as nothing seen.
# Usage: bash feed_unread.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18147. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-feed-unread; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18147; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
# seed: Bbottin<->pal are friends
ME=$(sid Bbottin); PAL=$(sid pal)
[ -n "$ME" ] && [ -n "$PAL" ] || { echo "seed gave no sessions"; exit 1; }

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
# get <extra cookies> <path> <file>: saves the body to $W/<file> and the headers to $W/<file>.h
SEEN=""   # this "browser"'s lg_feedseen value, kept the way a browser would
get() {
    local c="lg_session=$ME"; [ -n "$1" ] && c="$c; $1"
    curl -s -D "$W/$3.h" -o "$W/$3" -w '%{http_code}' -b "$c" "$URL$2"
}
R=""
browse() { # like get, with this browser's cookie; keeps any new lg_feedseen it is given; status in $R (no subshell)
    R=$(get "${SEEN:+lg_feedseen=$SEEN}" "$1" "$2")
    local v; v=$(grep -i '^Set-Cookie: lg_feedseen=' "$W/$2.h" | sed 's/^[^=]*=//; s/;.*//' | tr -d '\r')
    if [ -n "$v" ]; then SEEN=$v; fi
}
tabs()  { python3 -c 'import re,sys; m=re.search(r"<div class=\"segtabs\">(.*?)</div>", open(sys.argv[1]).read(), re.S); print(m.group(1) if m else "")' "$W/$1"; }
order() { python3 -c 'import re,sys; print(" ".join(re.findall(r"<p>(F-[a-z0-9-]+)</p>", open(sys.argv[1]).read())))' "$W/$1"; }

start
post $PAL /feed -F "text=F-pal-one" >/dev/null
post $PAL /feed -F "text=F-pal-two" >/dev/null
post $ME  /feed -F "text=F-mine-one" >/dev/null

browse "/feed?filter=all" a1.html; r=$R
check "All answers 200" '[ "$r" = 200 ]' "$r"
check "first visit: the tab counts pal's two posts, not mine" 'tabs a1.html | grep -q "filter=unread\">Unread (2)<"' "$(tabs a1.html)"
check "opening All marks nothing seen (no lg_feedseen cookie)" '! grep -qi "^Set-Cookie: lg_feedseen" "$W/a1.html.h"' "$(grep -i set-cookie "$W/a1.html.h")"

browse "/feed?filter=unread" u1.html; r=$R
check "Unread answers 200" '[ "$r" = 200 ]' "$r"
check "Unread shows pal's two posts, newest first, not mine" '[ "$(order u1.html)" = "F-pal-two F-pal-one" ]' "$(order u1.html)"
check "Unread is the current tab" 'tabs u1.html | grep -q "aria-current=\"page\" href=\"/feed?filter=unread\">Unread"' "$(tabs u1.html)"
check "opening Unread sets the seen cookie for this account (HttpOnly)" \
    'grep -qi "^Set-Cookie: lg_feedseen=Bbottin\.[0-9]*\.[0-9]*; HttpOnly" "$W/u1.html.h"' "$(grep -i set-cookie "$W/u1.html.h")"

browse "/feed?filter=unread" u2.html; r=$R
check "looking again: nothing new" '[ -z "$(order u2.html)" ] && grep -q "Nothing new since you last looked here" "$W/u2.html"' "$(order u2.html)"
check "... and the tab has no count" 'tabs u2.html | grep -q "filter=unread\">Unread<"' "$(tabs u2.html)"
check "... and no new cookie is sent" '! grep -qi "^Set-Cookie: lg_feedseen" "$W/u2.html.h"' "$(grep -i set-cookie "$W/u2.html.h")"

post $PAL /feed -F "text=F-pal-three" >/dev/null
post $ME  /feed -F "text=F-mine-two" >/dev/null
browse "/feed?filter=all" a2.html; r=$R
check "a new post from pal: the tab counts 1 (my own new post does not count)" 'tabs a2.html | grep -q "filter=unread\">Unread (1)<"' "$(tabs a2.html)"
check "All still shows every post, unchanged" \
    '[ "$(order a2.html)" = "F-mine-two F-pal-three F-mine-one F-pal-two F-pal-one" ]' "$(order a2.html)"
browse "/feed?filter=unread" u3.html; r=$R
check "Unread shows only pal's new post" '[ "$(order u3.html)" = "F-pal-three" ]' "$(order u3.html)"

pid()   { python3 -c 'import re,sys; m=re.search(r"id=\"post-(\d+)\"(?:(?!</article>).)*?<p>"+re.escape(sys.argv[2])+"</p>", open(sys.argv[1]).read(), re.S); print(m.group(1) if m else "")' "$W/$1" "$2"; }
ONE=$(pid u3.html F-pal-one 2>/dev/null); [ -n "$ONE" ] || ONE=$(pid a2.html F-pal-one)
post $PAL /post/$ONE/reply -d "text=a reply from pal" >/dev/null
browse "/feed?filter=all" a3.html; r=$R
check "pal replies to an old post: the tab counts 1" 'tabs a3.html | grep -q "filter=unread\">Unread (1)<"' "$(tabs a3.html)"
browse "/feed?filter=unread" u4.html; r=$R
check "Unread shows the old post again" '[ "$(order u4.html)" = "F-pal-one" ]' "$(order u4.html)"
check "... with the reason: a new reply" 'grep -q "New reply since you last looked." "$W/u4.html"' ""
browse "/feed?filter=unread" u5.html; r=$R
check "looking again after the reply: nothing new" '[ -z "$(order u5.html)" ]' "$(order u5.html)"
post $ME /post/$ONE/reply -d "text=my own reply" >/dev/null
browse "/feed?filter=all" a4.html; r=$R
check "my own reply does not make a post unread" 'tabs a4.html | grep -q "filter=unread\">Unread<"' "$(tabs a4.html)"
OLDSEEN=${SEEN%.*}   # the older cookie, without the reply id
post $PAL /post/$ONE/reply -d "text=another reply" >/dev/null
r=$(get "lg_feedseen=$OLDSEEN" "/feed?filter=unread" old.html)
check "the older cookie (no reply id): replies do not count yet" '[ -z "$(order old.html)" ]' "$(order old.html)"
check "... and opening Unread moves it to the new cookie" 'grep -qi "^Set-Cookie: lg_feedseen=Bbottin\.[0-9]*\.[0-9]*;" "$W/old.html.h"' "$(grep -i set-cookie "$W/old.html.h")"
r=$(get "lg_feedseen=pal.999999" "/feed?filter=all" o.html)
check "a seen cookie for another account counts as nothing seen" 'tabs o.html | grep -q "Unread (3)<"' "$(tabs o.html)"
r=$(get "lg_feedseen=Bbottin.12x" "/feed?filter=all" g.html)
check "a garbled seen cookie counts as nothing seen" 'tabs g.html | grep -q "Unread (3)<"' "$(tabs g.html)"
r=$(curl -s -o /dev/null -w '%{http_code}' "$URL/feed?filter=unread")
check "signed out, the Unread link asks you to log in" '[ "$r" = 302 ] || [ "$r" = 303 ]' "$r"

echo "passes=$passes fails=$fails"
echo "EXIT=$([ $fails -eq 0 ] && echo 0 || echo 1)"
[ $fails -eq 0 ]
