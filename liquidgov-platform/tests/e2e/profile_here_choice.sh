#!/usr/bin/env bash
# Dots on profile pages, part 1: the choice (Brandon 2026-11-12: "normal pages with little dots for people that you
# would see moving around"; profile pages first; showing as a dot is a REQUIRED choice at signup, nothing pre-selected,
# per his 2026-11-12 privacy rule). Not chosen = hidden. Existing accounts are asked once, on a profile page. Settings
# can change it. The dots script is only on a profile page for someone who chose Yes.
# Usage: bash profile_here_choice.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18123. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-profile-here-choice; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18123; URL=http://127.0.0.1:$PORT
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
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{http_code} %{redirect_url}' ${s:+-b "lg_session=$s"} "$@" "$URL$p"; }
page() { curl -s -o "$W/$3" -w '%{http_code}' ${1:+-b "lg_session=$1"} "$URL$2"; }
# the input tags of the radio group named "here" on a saved page
radios() { grep -o '<input[^>]*name="here"[^>]*>' "$W/$1"; }

start
# --- not chosen yet: hidden, asked once on a profile page
check "precondition: no here rows in the seed store" '! grep -q "^here	" $W/store.lgp' "$(grep '^here' $W/store.lgp)"
page $PAL /u/stranger u1.html >/dev/null
check "not chosen: a profile page asks (the here-ask card)" 'grep -q "id=\"here-ask\"" $W/u1.html' "no card"
check "...with a Yes and a No form, neither pre-selected" '[ "$(grep -o "action=\"/here/choice\"" $W/u1.html | wc -l)" = 2 ]' "$(grep -o 'action="/here/choice"' $W/u1.html | wc -l)"
check "not chosen: no dots script" '! grep -q "LG_HERE" $W/u1.html' "script present"
page "" /u/stranger anon.html >/dev/null
check "signed out: no card" '! grep -q "here-ask" $W/anon.html' "card for signed out"
check "signed out: no dots script" '! grep -q "LG_HERE" $W/anon.html' "script for signed out"

# --- choose Yes from the card
r=$(post "" /here/choice -d "choice=1" -d "back=/u/stranger")
check "signed out: choosing goes to login" '[[ "$r" == *"/login"* ]]' "$r"
r=$(post $PAL /here/choice -d "choice=maybe" -d "back=/u/stranger")
check "a bad value is refused (no row written)" '! grep -qP "^here\tpal\t" $W/store.lgp' "$r $(grep '^here' $W/store.lgp)"
r=$(post $PAL /here/choice -d "choice=1" -d "back=/u/stranger")
check "pal chooses Yes and goes back to the page" '[[ "$r" == *"/u/stranger"* ]]' "$r"
check "the store holds pal's Yes" 'grep -qP "^here\tpal\t1$" $W/store.lgp' "$(grep '^here' $W/store.lgp)"
r=$(post $PAL /here/choice -d "choice=1" -d "back=https://evil.example/")
check "back never leaves the site" '[[ "$r" != *"evil.example"* ]]' "$r"
page $PAL /u/stranger u2.html >/dev/null
check "Yes: no card any more" '! grep -q "id=\"here-ask\"" $W/u2.html' "card still there"
check "Yes: the dots settings object names pal and the page" 'grep -q "LG_HERE" $W/u2.html && grep -o "LG_HERE=[^<]*" $W/u2.html | grep -q "\"me\":\"pal\"" && grep -o "LG_HERE=[^<]*" $W/u2.html | grep -q "\"page\":\"/u/stranger\""' "$(grep -o 'LG_HERE=[^<]*' $W/u2.html)"
check "Yes: the dots script is linked" 'grep -q "src=\"/assets/here.js" $W/u2.html' "no script"
page $PAL /feed feed.html >/dev/null
check "only profile pages: no dots on /feed" '! grep -q "LG_HERE" $W/feed.html' "script on feed"

# --- choose No
post $STRANGER /here/choice -d "choice=0" -d "back=/u/pal" >/dev/null
check "the store holds stranger's No" 'grep -qP "^here\tstranger\t0$" $W/store.lgp' "$(grep '^here' $W/store.lgp)"
page $STRANGER /u/pal u3.html >/dev/null
check "No: no card" '! grep -q "id=\"here-ask\"" $W/u3.html' "card"
check "No: no dots script" '! grep -q "LG_HERE" $W/u3.html' "script"

# --- settings
page $ADMIN /settings set-unset.html >/dev/null
check "settings has the dots choice" '[ "$(radios set-unset.html | wc -l)" = 2 ]' "$(radios set-unset.html)"
check "not chosen: neither radio is checked" '! radios set-unset.html | grep -q checked' "$(radios set-unset.html)"
page $PAL /settings set-yes.html >/dev/null
check "Yes: the Yes radio is checked" 'radios set-yes.html | grep "value=\"1\"" | grep -q checked && ! radios set-yes.html | grep "value=\"0\"" | grep -q checked' "$(radios set-yes.html)"
post $PAL /here/choice -d "choice=0" -d "back=/settings" >/dev/null
check "settings can change Yes to No" 'grep -qP "^here\tpal\t0$" $W/store.lgp' "$(grep '^here' $W/store.lgp)"
post $PAL /here/choice -d "choice=1" -d "back=/settings" >/dev/null

# --- signup: required, nothing pre-selected
page "" /signup su.html >/dev/null
check "signup asks: two required radios" '[ "$(radios su.html | wc -l)" = 2 ] && [ "$(radios su.html | grep -c required)" -ge 1 ]' "$(radios su.html)"
check "signup: neither pre-selected" '! radios su.html | grep -q checked' "$(radios su.html)"
signup() { curl -s -o "$W/$2" -w '%{http_code} %{redirect_url}' --data-urlencode "username=$1" -d "display_name=New" -d "password=correct-horse-9" \
    -d "email=$1@example.com" -d "phone=555123$RANDOM$RANDOM" ${3:+-d "here=$3"} "$URL/signup"; }
r=$(signup newbie1 su-none.html)
check "signup without the choice is refused" '! grep -qP "^a\t[0-9]+\tnewbie1\t" $W/store.lgp' "$r"
check "...and says why" 'grep -qi "dot" $W/su-none.html' "no message"
r=$(signup newbie2 su-yes.html 1)
check "signup with Yes creates the account" 'grep -qP "^a\t[0-9]+\tnewbie2\t" $W/store.lgp' "$r"
check "...and stores the Yes" 'grep -qP "^here\tnewbie2\t1$" $W/store.lgp' "$(grep '^here' $W/store.lgp)"
r=$(signup newbie3 su-no.html 0)
check "signup with No stores the No" 'grep -qP "^here\tnewbie3\t0$" $W/store.lgp' "$r $(grep '^here' $W/store.lgp)"

# --- survives a restart
stop; start
page $PAL /u/stranger u4.html >/dev/null
check "after a restart pal's Yes still holds" 'grep -q "LG_HERE" $W/u4.html' "lost"
page $ADMIN /u/pal u5.html >/dev/null
check "control: Bbottin (never chose) is still asked" 'grep -q "id=\"here-ask\"" $W/u5.html' "not asked"

echo "passes=$passes fails=$fails"
[ $fails = 0 ]
