#!/usr/bin/env bash
# Feed filters, slice 5: the mix-in setting (Brandon 2026-11-20: one usual setting with Off, a per-tab choice in the
# feed's corner menu, Friends off unless chosen there, saved on the account; a new account starts at Rarely).
#   - no fmix record (an account from before): nothing is mixed in; the menu says "Mix-in: Off"
#   - usual 1 in 3: after every 2 posts of the tab, one Unfamiliar post it does not already show, with a reason line
#   - Friends stays off with the usual setting, and mixes once chosen on that tab
#   - a tab set to Off, and back to "Same as usual"; Unread and Unfamiliar are never mixed into
#   - odd values are ignored; the choice survives a restart (fmix record); a new account starts at Rarely
# Usage: bash feed_mix.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18155. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-feed-mix; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18155; URL=http://127.0.0.1:$PORT
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
order() { python3 -c 'import re,sys; print(" ".join(re.findall(r"<p>(F-[a-z0-9-]+)</p>", open(sys.argv[1]).read())))' "$W/$1"; }
why()   { python3 -c 'import re,sys; m=re.search(r"<p class=\"muted feedwhy\">([^<]*)</p>(?:(?!class=\"card\").)*?<p>"+re.escape(sys.argv[2])+"</p>", open(sys.argv[1]).read(), re.S); print(m.group(1) if m else "")' "$W/$1" "$2"; }
menu()  { python3 -c 'import re,sys; m=re.search(r"<summary>(Mix-in: [A-Za-z]+)</summary>", open(sys.argv[1]).read()); print(m.group(1) if m else "")' "$W/$1"; }
mix()   { post $ME /feed/mix -d "$1" >/dev/null; }

start
post $STR /communities -d "slug=club&name=Club&category=10&subcategory=Games" >/dev/null
for n in 1 2 3; do post $STR /c/club -F "text=F-u$n" >/dev/null; done        # unfamiliar to me: a group I'm not in
for n in 1 2 3 4 5 6; do post $PAL /feed -F "text=F-p$n" >/dev/null; done     # my friend's posts

r=$(get $ME "/feed?filter=all" a0.html)
check "no setting saved (an account from before): All is only my circle, nothing mixed in" \
    '[ "$(order a0.html)" = "F-p6 F-p5 F-p4 F-p3 F-p2 F-p1" ]' "$r $(order a0.html)"
check "... and the corner menu says Mix-in: Off" '[ "$(menu a0.html)" = "Mix-in: Off" ]' "$(menu a0.html)"
check "... and the store has no fmix row yet" '! grep -q "^fmix	" "$W/store.lgp"' "$(grep '^fmix' "$W/store.lgp")"

mix "view=all&every=3"
r=$(get $ME "/feed?filter=all" a1.html)
check "usual 1 in 3: after every 2 posts, one Unfamiliar post, newest first" \
    '[ "$(order a1.html)" = "F-p6 F-p5 F-u3 F-p4 F-p3 F-u2 F-p2 F-p1 F-u1" ]' "$(order a1.html)"
check "a mixed-in post says why" '[ "$(why a1.html F-u3)" = "Mixed in from outside your circle. From a group you&#39;re not in: Club" ] || why a1.html F-u3 | grep -q "^Mixed in from outside your circle. From a group you.*re not in: Club$"' "$(why a1.html F-u3)"
check "my friend's posts carry no reason line" '[ -z "$(why a1.html F-p6)" ]' "$(why a1.html F-p6)"
check "the menu says Mix-in: Often" '[ "$(menu a1.html)" = "Mix-in: Often" ]' "$(menu a1.html)"

r=$(get $ME "/feed?filter=friends" f1.html)
check "Friends stays off with the usual setting" '[ "$(order f1.html)" = "F-p6 F-p5 F-p4 F-p3 F-p2 F-p1" ] && [ "$(menu f1.html)" = "Mix-in: Off" ]' "$(order f1.html) / $(menu f1.html)"
mix "view=friends&every=3&tab_every=5"
r=$(get $ME "/feed?filter=friends" f2.html)
check "Friends set to 1 in 5 on its own tab: one after every 4 posts" \
    '[ "$(order f2.html)" = "F-p6 F-p5 F-p4 F-p3 F-u3 F-p2 F-p1" ]' "$(order f2.html)"

mix "view=all&every=3&tab_every=0"
r=$(get $ME "/feed?filter=all" a2.html)
check "All set to Off on its own tab (usual still 1 in 3): nothing mixed in" \
    '[ "$(order a2.html)" = "F-p6 F-p5 F-p4 F-p3 F-p2 F-p1" ]' "$(order a2.html)"
r=$(get $ME "/feed?filter=discussed" d.html)
check "another tab still uses the usual 1 in 3" 'order d.html | grep -q "F-u3"' "$(order d.html)"
mix "view=all&every=3&tab_every=usual"
r=$(get $ME "/feed?filter=all" a3.html)
check "back to Same as usual: mixed in again" '[ "$(order a3.html)" = "$(order a1.html)" ]' "$(order a3.html)"

for t in unread unfamiliar; do
    r=$(get $ME "/feed?filter=$t" $t.html)
    check "the $t tab is never mixed into" '! grep -q "Mixed in from outside your circle" "$W/'$t'.html"' "found"
done

mix "view=all&every=7&tab_every=9"
r=$(get $ME "/feed?filter=all" a4.html)
check "odd values are ignored (still 1 in 3)" '[ "$(order a4.html)" = "$(order a1.html)" ]' "$(order a4.html)"
mix "view=nosuchtab&every=3&tab_every=5"
check "a tab that does not exist is not saved" '! grep -q "nosuchtab" "$W/store.lgp"' "$(grep '^fmix' "$W/store.lgp")"

stop; start
r=$(get $ME "/feed?filter=friends" f3.html)
check "after a restart the choices are still there (Friends 1 in 5)" '[ "$(order f3.html)" = "$(order f2.html)" ]' "$(order f3.html)"
check "the store holds one fmix row for me" '[ "$(grep -c "^fmix	Bbottin	3	" "$W/store.lgp")" = 1 ]' "$(grep '^fmix' "$W/store.lgp")"

r=$(curl -s -o /dev/null -w '%{http_code}' --data-urlencode "username=newbie1" -d "display_name=New Person" -d "password=correct-horse-9" \
    -d "email=newbie1@example.com" -d "phone=5551230$RANDOM" -d here=1 "$URL/signup")
check "a new account starts at Rarely (1 in 10)" 'grep -q "^fmix	newbie1	10	" "$W/store.lgp"' "$r $(grep '^fmix' "$W/store.lgp")"
r=$(curl -s -o /dev/null -w '%{http_code}' -d "view=all&every=3" "$URL/feed/mix")
check "signed out, saving the setting asks you to log in" '[ "$r" = 302 ] || [ "$r" = 303 ]' "$r"
check "the newcomer tip for the mix-in is there" 'grep -q "id: .mix., path:" "$HERE/assets/hints.js"' "missing"

echo "passes=$passes fails=$fails"
echo "EXIT=$([ $fails -eq 0 ] && echo 0 || echo 1)"
[ $fails -eq 0 ]
