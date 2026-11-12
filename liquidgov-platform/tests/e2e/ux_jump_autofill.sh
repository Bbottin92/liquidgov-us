#!/usr/bin/env bash
# Taps that jump to the top (A) and saved-password autofill in non-login boxes (B): the top findings of
# CODE-SCAN-UX.md (Brandon 2026-11-11 4:37 PM, Brief 4 item 9).
#   A1 posts carry id="post-N"; Second, Reply and Repost go back to the page they came from at #post-N (an old
#      #fragment in the Referer is replaced), so the page comes back where the post is, not at the top
#   A2 the account-menu checkbox #navc is not parked off-screen at left:-999px (iOS scrolls to a focused control)
#   A3 the map's "Open page" link never starts as href="#"
#   B1 /settings has no password field at all; change password lives on /settings/password (with a username field
#      for the password manager), and its errors come back there
#   B1b the block-list and delete-account boxes do not say "username" (or show it) and turn autocomplete off
#   B2 /friends "Add by exact name" and a group's invite box: no "username" placeholder or label, autocomplete off
#   B3 /me: display name is autocomplete="nickname"; the Juketube relay box does not say username, autocomplete off
# Usage: bash ux_jump_autofill.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18112. Never touches the live site. New file: committed suites are frozen.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-ux-jump-autofill; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18112; URL=http://127.0.0.1:$PORT
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
# where a POST redirects to
loc() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{redirect_url}' -b "lg_session=$s" "$@" "$URL$p"; }
get()  { curl -s -o "$W/$3" -w '%{http_code}' -b "lg_session=$1" "$URL$2"; }
# the opening tag of the first element matching a regex on its attributes
tag() { python3 -c 'import re,sys; m=re.search(r"<"+sys.argv[2]+r"\b[^>]*>", open(sys.argv[1]).read()); print(m.group(0) if m else "")' "$W/$1" "$2"; }

start
post $PAL /feed -F "text=Public hello from pal" -F "anybody=1" >/dev/null
get $ADMIN /feed f.html >/dev/null
PID=$(grep -o 'action="/post/[0-9]*/second"' $W/f.html | head -1 | grep -o '[0-9]\+')
check "precondition: the feed shows pal's post with a Second button" '[ -n "$PID" ]' "no post"
check "A1 the post's article has id=post-$PID" 'grep -q "<article class=\"post\" id=\"post-$PID\"" $W/f.html' "$(grep -o '<article[^>]*>' $W/f.html | head -2)"
l=$(loc $ADMIN /post/$PID/second -H "Referer: $URL/feed?filter=friends#post-999")
check "A1 Second goes back to the same page at #post-$PID (old fragment replaced)" '[ "$l" = "$URL/feed?filter=friends#post-$PID" ]' "$l"
l=$(loc $ADMIN /post/$PID/reply -H "Referer: $URL/u/pal" -d "text=hi")
check "A1 Reply goes back to the same page at #post-$PID" '[ "$l" = "$URL/u/pal#post-$PID" ]' "$l"
l=$(loc $ADMIN /me/repost -H "Referer: $URL/feed" -d "post_id=$PID")
check "A1 Repost goes back to the page it was tapped on at #post-$PID, not to /me" '[ "$l" = "$URL/feed#post-$PID" ]' "$l"
l=$(loc $ADMIN /post/$PID/second)
check "A1 no Referer: /feed at #post-$PID" '[ "$l" = "$URL/feed#post-$PID" ]' "$l"

get $ADMIN /map m.html >/dev/null
check "A2 #navc is not parked at left:-999px" '! grep -q "#navc{position:absolute;left:-999px}" $W/m.html && grep -q "id=\"navc\"" $W/m.html' "$(grep -o '#navc{[^}]*}' $W/m.html | head -2)"
check "A3 the map's Open page link has no href=\"#\"" '[ -n "$(tag m.html "a[^>]*id=\"lgm-open\"")" ] && ! tag m.html "a[^>]*id=\"lgm-open\"" | grep -q "href=\"#\""' "$(tag m.html "a[^>]*id=\"lgm-open\"")"

get $ADMIN /settings s.html >/dev/null
check "B1 /settings has no password field" '! grep -qi "type=\"password\"" $W/s.html' "$(grep -o '<input[^>]*password[^>]*>' $W/s.html | head -2)"
check "B1 ... and links to /settings/password" 'grep -q "href=\"/settings/password\"" $W/s.html' "missing"
r=$(get $ADMIN /settings/password sp.html)
check "B1 /settings/password answers 200 with current and new password fields" '[ "$r" = 200 ] && grep -q "autocomplete=\"current-password\"" $W/sp.html && grep -q "autocomplete=\"new-password\"" $W/sp.html' "$r"
check "B1 ... and a username field for the password manager (your name, read-only)" 'tag sp.html "input[^>]*autocomplete=\"username\"" | grep -q "value=\"Bbottin\""' "$(tag sp.html "input[^>]*autocomplete=\"username\"")"
l=$(loc $ADMIN /settings/password -d "current=wrong-password" -d "next=whatever-new-1")
check "B1 a wrong current password comes back to /settings/password with the error" '[[ "$l" == "$URL/settings/password?err="* ]]' "$l"
check "B1b the block box does not say username and has autocomplete off" \
    'b=$(tag s.html "input[^>]*name=\"[a-z_]*\"[^>]*required[^>]*style=\"flex:1\""); [ -n "$b" ] && ! grep -qi username <<<"$b" && grep -q "autocomplete=\"off\"" <<<"$b"' "$(tag s.html "input[^>]*name=\"[a-z_]*\"[^>]*required[^>]*style=\"flex:1\"")"
check "B1b the delete-account box does not show your username as its placeholder" \
    '! grep -q "placeholder=\"Bbottin\"" $W/s.html' "$(grep -o '<input[^>]*placeholder="Bbottin"[^>]*>' $W/s.html)"

get $ADMIN /friends fr.html >/dev/null
check "B2 /friends: no username placeholder anywhere" '! grep -q "placeholder=\"username\"" $W/fr.html' "$(grep -o '<input[^>]*placeholder="username"[^>]*>' $W/fr.html)"
check "B2 /friends: Add by exact name has autocomplete off" \
    'python3 -c "import re,sys; s=open(sys.argv[1]).read(); i=s.find(\"Add by exact name\"); m=re.search(r\"<input[^>]*>\", s[i:]); sys.exit(0 if i>=0 and m and \"autocomplete=\\\"off\\\"\" in m.group(0) else 1)" $W/fr.html' "missing"
post $ADMIN /communities -d "slug=ux1" -d "name=UX Neighbors" -d "category=0" -d "subcategory=Neighbors" -d "about=x" --data-urlencode "city=Fresno, CA" >/dev/null
get $ADMIN /c/ux1 c.html >/dev/null
check "B2 group page: invite has no username label or placeholder, autocomplete off" \
    'grep -q "/c/ux1/invite" $W/c.html && ! grep -q "Invite by username" $W/c.html && ! grep -q "placeholder=\"username\"" $W/c.html && tag c.html "input[^>]*name=\"who\"[^>]*required" | grep -q "autocomplete=\"off\""' "$(tag c.html "input[^>]*name=\"who\"[^>]*required")"

get $ADMIN /me me.html >/dev/null
check "B3 /me display name is autocomplete=nickname" 'tag me.html "input[^>]*name=\"display_name\"" | grep -q "autocomplete=\"nickname\""' "$(tag me.html "input[^>]*name=\"display_name\"")"
check "B3 /me relay box: no username, autocomplete off" \
    'r=$(tag me.html "input[^>]*name=\"relay\""); [ -n "$r" ] && ! grep -qi username <<<"$r" && grep -q "autocomplete=\"off\"" <<<"$r"' "$(tag me.html "input[^>]*name=\"relay\"")"

echo "passes=$passes fails=$fails"
[ $fails -eq 0 ]
