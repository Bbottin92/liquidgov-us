#!/usr/bin/env bash
# The style library can be found (Brandon 2026-11-12, after styles1 went live: "i dont really see anything with the
# style library"; /styles had no link anywhere but deep in /me). A "Styles" link in the site menu (signed in) and
# the signed-out nav, a "Browse styles" link at the top of /me's Profile CSS card, and on a profile whose CSS came from
# a published style a small credit ("Style: <name> by <author>") linking to it, outside the styled area so no profile
# CSS can hide or fake it. The credit follows the same rules as /me's "based on" (the profile has CSS; the style is
# still in the library) and the library's own (hidden across a block, hidden when the style fails today's filter).
# Usage: bash styles_findable.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18126. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-styles-findable; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18126; URL=http://127.0.0.1:$PORT
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
savecss() { curl -s -o /dev/null -w '%{redirect_url}' -b "lg_session=$1" --data-urlencode "css=$2" "$URL/me/css"; }
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{http_code} %{redirect_url}' -b "lg_session=$s" "$@" "$URL$p"; }
page() { curl -s -o "$W/$3" -w '%{http_code}' ${1:+-b "lg_session=$1"} "$URL$2"; }
style_id() { awk -F'\t' -v n="$1" '$1=="sty" && $5==n{print $2}' "$W/store.lgp" | head -1; }
# the text of one element, found by a class name, on a saved page ("" when absent)
el() { python3 -c 'import re,sys; m=re.search(r"<(\w+)[^>]*class=\"[^\"]*\b"+re.escape(sys.argv[2])+r"\b[^\"]*\"[^>]*>.*?</\1>", open(sys.argv[1],encoding="utf-8").read(), re.S); print(m.group(0) if m else "")' "$W/$1" "$2"; }
# is <needle> inside the .lg-profile-scope div of a saved profile page?
in_scope() { python3 - "$W/$1" "$2" <<'PY'
import re, sys
t = open(sys.argv[1], encoding="utf-8").read()
s = t.find('class="lg-profile-scope"')
if s < 0: sys.exit(1)
o = t.rfind("<div", 0, s); depth = 0
for m in re.finditer(r"<div\b|</div>", t[o:]):
    depth += 1 if m.group(0) == "<div" else -1
    if depth == 0:
        sys.exit(0 if sys.argv[2] in t[o:o + m.end()] else 1)
sys.exit(1)
PY
}

start
# --- the menu and the nav
page $PAL /feed feed.html >/dev/null
check "signed in: the account menu links to /styles" 'el feed.html accountmenu | grep -q "href=\"/styles\""' "$(el feed.html accountmenu | grep -o 'href="[^"]*"' | tr '\n' ' ')"
check "control: the account menu still links to /map" 'el feed.html accountmenu | grep -q "href=\"/map\""' "menu changed"
page "" / home.html >/dev/null
check "signed out: the nav links to /styles" 'grep -o "<nav>.*</nav>" $W/home.html | grep -q "href=\"/styles\""' "$(grep -o '<nav>.\{0,300\}' $W/home.html | head -1)"
check "control: the signed-out nav still links to /laws" 'grep -o "<nav>.*</nav>" $W/home.html | grep -q "href=\"/laws\""' "nav changed"

# --- /me: Browse styles at the top of the Profile CSS card
page $STRANGER /me me0.html >/dev/null
check "/me: the Profile CSS card has a Browse styles link" 'python3 -c "import re,sys; t=open(sys.argv[1]).read(); a=t.find(\"id=\\\"profile-css\\\"\"); b=t.find(\"<textarea name=\\\"css\\\"\", a); seg=t[a:b]; sys.exit(0 if a>=0 and b>a and \"href=\\\"/styles\\\"\" in seg and \"Browse styles\" in seg else 1)" $W/me0.html' "not before the CSS box"

# --- the credit on a profile that uses a published style
savecss $PAL ".lg-theme h1{color:#7e3fa1}" >/dev/null
post $PAL /styles/publish --data-urlencode "name=Night Paper" >/dev/null
NP=$(style_id "Night Paper")
check "precondition: pal published Night Paper" '[ -n "$NP" ]' "$(grep '^sty' $W/store.lgp)"
post $STRANGER /styles/use -d "id=$NP" >/dev/null
page $ADMIN /u/stranger us.html >/dev/null
check "stranger's profile credits the style (lg-style-credit)" 'el us.html lg-style-credit | grep -q "Night Paper"' "$(el us.html lg-style-credit)"
check "...links to the style in the library" 'el us.html lg-style-credit | grep -q "href=\"/styles#style-$NP\""' "$(el us.html lg-style-credit)"
check "...and names its author with a link to /u/pal" 'el us.html lg-style-credit | grep -q "href=\"/u/pal\""' "$(el us.html lg-style-credit)"
check "...outside the styled area (no profile CSS can hide or fake it)" '! in_scope us.html "lg-style-credit"' "inside .lg-profile-scope"
page $ADMIN /u/pal up.html >/dev/null
check "control: pal's own profile (CSS of her own, never 'used') has no credit" '[ -z "$(el up.html lg-style-credit)" ]' "$(el up.html lg-style-credit)"
page $ADMIN "/u/stranger?plain=1" plain.html >/dev/null
check "plain view: no style shown, so no credit" '[ -z "$(el plain.html lg-style-credit)" ]' "$(el plain.html lg-style-credit)"
page "" /u/stranger anon.html >/dev/null
check "signed out: the credit shows too (the profile is public)" 'el anon.html lg-style-credit | grep -q "Night Paper"' "missing"

# blocks: someone pal blocked does not get a link to pal's style
post $PAL /settings/block -d "who=Bbottin" >/dev/null
page $ADMIN /u/stranger blk.html >/dev/null
check "pal blocked Bbottin: no credit to pal's style for Bbottin" '[ -z "$(el blk.html lg-style-credit)" ]' "$(el blk.html lg-style-credit)"
page "" /u/stranger blk-anon.html >/dev/null
check "control: still credited for others" 'el blk-anon.html lg-style-credit | grep -q "Night Paper"' "gone for everyone"
post $PAL /settings/unblock -d "who=Bbottin" >/dev/null

# the credit goes with the CSS, and with the style
savecss $STRANGER "" >/dev/null
page $ADMIN /u/stranger none.html >/dev/null
check "stranger cleared the CSS: no credit" '[ -z "$(el none.html lg-style-credit)" ]' "$(el none.html lg-style-credit)"
post $STRANGER /styles/use -d "id=$NP" >/dev/null
page $ADMIN /u/stranger back.html >/dev/null
check "precondition: used again, credited again" 'el back.html lg-style-credit | grep -q "Night Paper"' "missing"
post $PAL /styles/remove -d "id=$NP" >/dev/null
page $ADMIN /u/stranger gone.html >/dev/null
check "pal removed the style: no credit (the copied CSS stays)" '[ -z "$(el gone.html lg-style-credit)" ] && grep -q "7e3fa1" $W/gone.html' "$(el gone.html lg-style-credit)"

echo "passes=$passes fails=$fails"
[ $fails = 0 ]
