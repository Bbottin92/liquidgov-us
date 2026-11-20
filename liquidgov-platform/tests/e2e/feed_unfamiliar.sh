#!/usr/bin/env bash
# Feed filters, slice 4: Unfamiliar (Brandon 2026-11-20: "New people & groups", no quality bar for now; wip/163).
#   /feed?filter=unfamiliar shows posts from people I am not friends with and have never replied to or seconded:
#   their public (Anybody) posts, and their posts in groups I am not in. Never my own, a friend's, a post in one of my
#   groups, a post I may not see, or anyone blocked either way. Each carries a one-line reason. Worked out per request
#   from my own activity; nothing is stored.
# Usage: bash feed_unfamiliar.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18149. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-feed-unfamiliar; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18149; URL=http://127.0.0.1:$PORT
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
tabs()  { python3 -c 'import re,sys; m=re.search(r"<div class=\"segtabs\">(.*?)</div>", open(sys.argv[1]).read(), re.S); print(m.group(1) if m else "")' "$W/$1"; }
order() { python3 -c 'import re,sys; print(" ".join(re.findall(r"<p>(F-[a-z0-9-]+)</p>", open(sys.argv[1]).read())))' "$W/$1"; }
# The reason line in the same card as the post whose text is $2.
why()   { python3 -c 'import re,sys; m=re.search(r"<p class=\"muted feedwhy\">([^<]*)</p>(?:(?!class=\"card\").)*?<p>"+re.escape(sys.argv[2])+"</p>", open(sys.argv[1]).read(), re.S); print(m.group(1) if m else "")' "$W/$1" "$2"; }
pid()   { python3 -c 'import re,sys; m=re.search(r"id=\"post-(\d+)\"(?:(?!</article>).)*?<p>"+re.escape(sys.argv[2])+"</p>", open(sys.argv[1]).read(), re.S); print(m.group(1) if m else "")' "$W/$1" "$2"; }

start
post $STR /communities -d "slug=club&name=Chess+Club&category=10&subcategory=Games" >/dev/null
post $STR /communities -d "slug=ours&name=Ours&category=10&subcategory=Games" >/dev/null
post $ME  /c/ours/join -d "" >/dev/null
post $PAL /feed -F "anybody=1" -F "text=F-pal-public" >/dev/null      # a friend: never unfamiliar
post $ME  /feed -F "anybody=1" -F "text=F-mine-public" >/dev/null     # my own: never
post $STR /c/ours -F "text=F-in-my-group" >/dev/null                  # a group I am in: not unfamiliar
post $STR /feed -F "text=F-str-friends-only" >/dev/null               # stranger's friends only: I may not see it
post $STR /c/club -F "text=F-club" >/dev/null                         # a group I am not in
post $STR /feed -F "anybody=1" -F "text=F-str-public" >/dev/null      # public, from someone I have not met

r=$(get $ME "/feed?filter=unfamiliar" u1.html)
check "Unfamiliar answers 200" '[ "$r" = 200 ]' "$r"
check "Unfamiliar is a tab, and the current one" 'tabs u1.html | grep -q "aria-current=\"page\" href=\"/feed?filter=unfamiliar\">Unfamiliar<"' "$(tabs u1.html)"
check "shows the stranger's public post and their post in a group I'm not in, newest first" \
    '[ "$(order u1.html)" = "F-str-public F-club" ]' "$(order u1.html)"
check "never a friend's, my own, one in my group, or one I may not see" \
    '! grep -qE "F-pal-public|F-mine-public|F-in-my-group|F-str-friends-only" "$W/u1.html"' "$(order u1.html)"
check "the group post says why: a group I'm not in, by name" \
    '[ "$(why u1.html F-club)" = "From a group you&#39;re not in: Chess Club" ] || [ "$(why u1.html F-club)" = "From a group you&#x27;re not in: Chess Club" ] || [ "$(why u1.html F-club)" = "From a group you'"'"'re not in: Chess Club" ]' "$(why u1.html F-club)"
check "the public post says why" 'why u1.html F-str-public | grep -q "^Public, from someone you"' "$(why u1.html F-str-public)"
check "the page says it is worked out from my own activity only" 'grep -q "Worked out from your own activity only" "$W/u1.html"' "missing"
r=$(get $ME "/feed?filter=all" a.html)
check "other tabs show no reason lines" '! grep -q "feedwhy" "$W/a.html"' "found"

post $ME /settings/block -d "who=stranger" >/dev/null
r=$(get $ME "/feed?filter=unfamiliar" b1.html)
check "someone I blocked: nothing from them" '[ -z "$(order b1.html)" ]' "$(order b1.html)"
post $ME /settings/unblock -d "who=stranger" >/dev/null
post $STR /settings/block -d "who=Bbottin" >/dev/null
r=$(get $ME "/feed?filter=unfamiliar" b2.html)
check "someone who blocked me: nothing from them" '[ -z "$(order b2.html)" ]' "$(order b2.html)"
post $STR /settings/unblock -d "who=Bbottin" >/dev/null
r=$(get $ME "/feed?filter=unfamiliar" b3.html)
check "control: unblocked, both posts are back" '[ "$(order b3.html)" = "F-str-public F-club" ]' "$(order b3.html)"

CLUB=$(pid b3.html F-club)
post $ME /post/$CLUB/second -d "" >/dev/null
r=$(get $ME "/feed?filter=unfamiliar" s1.html)
check "once I second one of their posts, they are familiar: none of their posts show" \
    '[ -z "$(order s1.html)" ] && grep -q "Nothing unfamiliar right now" "$W/s1.html"' "$(order s1.html)"
post $ME /post/$CLUB/second -d "" >/dev/null   # take the second back
r=$(get $ME "/feed?filter=unfamiliar" s2.html)
check "second taken back: unfamiliar again" '[ "$(order s2.html)" = "F-str-public F-club" ]' "$(order s2.html)"
post $ME /post/$CLUB/reply -d "text=hello" >/dev/null
r=$(get $ME "/feed?filter=unfamiliar" s3.html)
check "once I reply to one of their posts, they are familiar" '[ -z "$(order s3.html)" ]' "$(order s3.html)"

r=$(curl -s -o /dev/null -w '%{http_code}' "$URL/feed?filter=unfamiliar")
check "signed out, the Unfamiliar link asks you to log in" '[ "$r" = 302 ] || [ "$r" = 303 ]' "$r"

echo "passes=$passes fails=$fails"
echo "EXIT=$([ $fails -eq 0 ] && echo 0 || echo 1)"
[ $fails -eq 0 ]
