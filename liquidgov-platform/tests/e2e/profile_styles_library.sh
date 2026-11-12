#!/usr/bin/env bash
# The profile style library (slice 2 of profile CSS; Brandon 2026-11-12). Anyone signed in publishes their CURRENT
# Profile CSS under a name (it must pass sanitize_author_css then); /styles lists styles, newest first or most used
# first, each with its author and a preview that cannot touch the page or the other previews (a sandboxed iframe);
# "Use this style" COPIES the CSS into your own Profile CSS (the author's later edits never change copies) and /me says
# "based on <author>'s <name>"; Report goes to /admin/styles, where an operator can take any style down. A published
# style is a frozen copy; authors remove, never edit. Blocked people's styles are hidden both ways.
# Usage: bash profile_styles_library.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18122. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-profile-styles-library; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18122; URL=http://127.0.0.1:$PORT
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
# a POST: prints "<code> <Location>"
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{http_code} %{redirect_url}' ${s:+-b "lg_session=$s"} "$@" "$URL$p"; }
page() { curl -s -o "$W/$3" -w '%{http_code}' ${1:+-b "lg_session=$1"} "$URL$2"; }
publish() { post "$1" /styles/publish --data-urlencode "name=$2"; }
style_id() { awk -F'\t' -v n="$1" '$1=="sty" && $5==n{print $2}' "$W/store.lgp" | head -1; }
# a appears before b in the saved page (both present)
before() { python3 -c 'import sys; t=open(sys.argv[1],encoding="utf-8").read(); a=t.find(sys.argv[2]); b=t.find(sys.argv[3]); sys.exit(0 if 0<=a<b else 1)' "$W/$1" "$2" "$3"; }

# the text of the <p class="... lg-based-on ...">...</p> element on a saved page
based() { python3 -c 'import re,sys; m=re.search(r"<p class=\"[^\"]*lg-based-on[^\"]*\">.*?</p>", open(sys.argv[1],encoding="utf-8").read(), re.S); print(m.group(0) if m else "")' "$W/$1"; }

CSS_A='.lg-theme h1{color:#7e3fa1} .lg-theme{background:#fdf6e3}'
CSS_B='.lg-theme h1{color:#2b5d8a} .lg-theme{background:#eef4fa}'
CSS_C='.lg-theme h1{color:#3c7a3c}'

start
r=$(page "" /styles anon0.html)
check "signed out: /styles answers 200" '[ "$r" = 200 ]' "$r"

# --- publish
check "precondition: pal saves CSS A" '[[ "$(savecss $PAL "$CSS_A")" == *"ok=css"* ]]' "not saved"
r=$(post "" /styles/publish --data-urlencode "name=Night Paper")
check "signed out: publish goes to login" '[[ "$r" == *"/login"* ]]' "$r"
check "...and stores no style" '! grep -q "^sty	" $W/store.lgp' "$(grep '^sty' $W/store.lgp)"
r=$(publish $PAL "")
check "an empty name is refused" '[[ "$r" == *"err=style"* && "$r" == *"reason=name"* ]]' "$r"
r=$(publish $PAL "$(printf 'x%.0s' $(seq 61))")
check "a 61-character name is refused" '[[ "$r" == *"reason=name"* ]]' "$r"
check "...neither stored a style" '! grep -q "^sty	" $W/store.lgp' "$(grep -c '^sty' $W/store.lgp)"
r=$(publish $STRANGER "Nothing")
check "publishing with no Profile CSS is refused" '[[ "$r" == *"err=style"* && "$r" == *"reason=empty"* ]]' "$r"
r=$(publish $PAL "Night Paper")
check "pal publishes Night Paper (goes to /styles)" '[[ "$r" == *"/styles"* && "$r" != *"err="* ]]' "$r"
NP=$(style_id "Night Paper")
check "the store holds Night Paper by pal with CSS A" 'grep -qP "^sty\t$NP\tpal\t[0-9]+\tNight Paper\t" $W/store.lgp && grep "^sty" $W/store.lgp | grep -qF "$CSS_A"' "$(grep '^sty' $W/store.lgp)"
check "...and the next-id row" 'grep -q "^styn	" $W/store.lgp' "no styn"

# CSS that passed older rules but fails sanitize_author_css today is refused at publish time
stop
python3 - "$W/store.lgp" <<'PY'
import sys
p = sys.argv[1]; t = open(p, encoding="utf-8").read()
ids = {}
for line in t.split("\n"):
    f = line.split("\t")
    if f[0] == "a" and len(f) > 2: ids[f[2]] = f[1]
t = t.rstrip("\n") + "\nac\t%s\th1{color:\\72 ed}\n" % ids["stranger"]
open(p, "w", encoding="utf-8").write(t)
PY
start
r=$(publish $STRANGER "Escaped")
check "stored CSS that fails today's filter is refused (reason=escape)" '[[ "$r" == *"err=style"* && "$r" == *"reason=escape"* ]]' "$r"
check "...and not stored" '[ -z "$(style_id Escaped)" ]' "stored"
savecss $STRANGER "" >/dev/null

check "precondition: Bbottin saves CSS B" '[[ "$(savecss $ADMIN "$CSS_B")" == *"ok=css"* ]]' "not saved"
sleep 1.1
r=$(publish $ADMIN "Sun Room")
SR=$(style_id "Sun Room")
check "Bbottin publishes Sun Room" '[ -n "$SR" ] && [ "$SR" != "$NP" ]' "$r"

# --- the list
for who in ANON STRANGER; do
    s=""; [ $who = STRANGER ] && s=$STRANGER
    r=$(page "$s" /styles list-$who.html)
    check "$who: /styles answers 200" '[ "$r" = 200 ]' "$r"
    check "$who: lists Night Paper" 'grep -q "Night Paper" $W/list-$who.html' "missing"
    check "$who: lists Sun Room" 'grep -q "Sun Room" $W/list-$who.html' "missing"
    check "$who: names each author (links to /u/pal and /u/Bbottin)" 'grep -q "href=\"/u/pal\"" $W/list-$who.html && grep -q "href=\"/u/Bbottin\"" $W/list-$who.html' "no author links"
    check "$who: newest first (Sun Room before Night Paper)" 'before list-$who.html "Sun Room" "Night Paper"' "order"
    check "$who: each preview is an iframe on its own preview URL" 'grep -q "src=\"/styles/$NP/preview\"" $W/list-$who.html && grep -q "src=\"/styles/$SR/preview\"" $W/list-$who.html' "no preview frames"
    check "$who: every preview iframe is sandboxed" '[ "$(grep -o "<iframe[^>]*>" $W/list-$who.html | grep -c sandbox)" = "$(grep -o "<iframe[^>]*>" $W/list-$who.html | wc -l)" ] && [ "$(grep -o "<iframe[^>]*>" $W/list-$who.html | wc -l)" -ge 2 ]' "$(grep -o '<iframe[^>]*>' $W/list-$who.html)"
    check "$who: no preview may run scripts or share the origin" '! grep -o "<iframe[^>]*>" $W/list-$who.html | grep -qE "allow-scripts|allow-same-origin"' "allowed"
    check "$who: the styles CSS is NOT on the list page itself" '! grep -qF "7e3fa1" $W/list-$who.html && ! grep -qF "2b5d8a" $W/list-$who.html' "CSS inline on the page"
done
check "signed out: no Use button (needs sign-in)" '! grep -q "action=\"/styles/use\"" $W/list-ANON.html' "use form shown"
check "signed in: a Use button" 'grep -q "action=\"/styles/use\"" $W/list-STRANGER.html' "no use form"
check "signed in: a Report button" 'grep -q "action=\"/styles/report\"" $W/list-STRANGER.html' "no report form"

r=$(page "" /styles/$NP/preview prev-np.html)
check "the preview of Night Paper answers 200" '[ "$r" = 200 ]' "$r"
check "...and holds CSS A" 'grep -qF "7e3fa1" $W/prev-np.html' "no CSS"
check "...nested in its scope" 'grep -qF ".lg-profile-scope{$CSS_A}" $W/prev-np.html' "$(grep -o 'lg-profile-scope{[^<]\{0,60\}' $W/prev-np.html | head -1)"
check "...but not Sun Room's CSS" '! grep -qF "2b5d8a" $W/prev-np.html' "other CSS in preview"
check "...and no scripts" '! grep -qi "<script" $W/prev-np.html' "script in preview"
r=$(page "" /styles/999/preview prev-none.html)
check "a missing style's preview is 404" '[ "$r" = 404 ]' "$r"

# --- use (copy)
r=$(post $STRANGER /styles/use -d "id=$NP")
check "stranger uses Night Paper" '[[ "$r" == *"/me"* && "$r" != *"err="* ]]' "$r"
page $STRANGER /me me-stranger.html >/dev/null
check "stranger's Profile CSS is now CSS A (in the /me box)" 'grep -qF "7e3fa1" $W/me-stranger.html' "not copied"
page "" /u/stranger u-stranger.html >/dev/null
check "...and on /u/stranger" 'grep -qF ".lg-profile-scope{$CSS_A}" $W/u-stranger.html' "not on profile"
check "/me says based on pal's Night Paper" 'based me-stranger.html | grep -q "Night Paper" && based me-stranger.html | grep -q "href=\"/u/pal\""' "$(based me-stranger.html)"
page $ADMIN /me me-admin.html >/dev/null
check "control: Bbottin (copied nothing) has no based-on line" '! grep -q "lg-based-on" $W/me-admin.html' "based-on shown"
post $STRANGER /styles/use -d "id=$NP" >/dev/null
check "using it twice stores one use row" '[ "$(grep -cP "^stu\t$NP\tstranger\t" $W/store.lgp)" = 1 ]' "$(grep -c '^stu' $W/store.lgp)"
r=$(post "" /styles/use -d "id=$NP")
check "signed out: use goes to login" '[[ "$r" == *"/login"* ]]' "$r"

page "" "/styles?sort=used" used.html >/dev/null
check "most used first: Night Paper (1 use) before Sun Room (0)" 'before used.html "Night Paper" "Sun Room"' "order"
check "...Night Paper shows 1 use" 'grep -q "lg-style-uses\">1 use<" $W/used.html' "$(grep -o 'lg-style-uses">[^<]*' $W/used.html)"
page "" /styles new2.html >/dev/null
check "control: the newest order is still Sun Room first" 'before new2.html "Sun Room" "Night Paper"' "order"

# --- the author's later edits never change copies; a published style is a frozen copy
check "pal saves CSS C" '[[ "$(savecss $PAL "$CSS_C")" == *"ok=css"* ]]' "not saved"
page "" /u/pal u-pal.html >/dev/null
check "control: pal's own profile now has CSS C" 'grep -qF "3c7a3c" $W/u-pal.html' "no C"
page "" /u/stranger u-stranger2.html >/dev/null
check "stranger's copy is still CSS A" 'grep -qF "7e3fa1" $W/u-stranger2.html' "copy changed"
check "...and never CSS C" '! grep -qF "3c7a3c" $W/u-stranger2.html' "C leaked into the copy"
page "" /styles/$NP/preview prev-np2.html >/dev/null
check "the published Night Paper is still CSS A" 'grep -qF "7e3fa1" $W/prev-np2.html && ! grep -qF "3c7a3c" $W/prev-np2.html' "library entry changed"

# --- based-on survives editing the copy, goes when the CSS is cleared
savecss $STRANGER "$CSS_A .lg-theme p{margin:0}" >/dev/null
page $STRANGER /me me-s2.html >/dev/null
check "based-on survives editing the copy" 'grep -q "lg-based-on" $W/me-s2.html' "gone"
savecss $STRANGER "" >/dev/null
page $STRANGER /me me-s3.html >/dev/null
check "based-on goes when the Profile CSS is cleared" '! grep -q "lg-based-on" $W/me-s3.html' "still shown"
post $STRANGER /styles/use -d "id=$NP" >/dev/null

# --- report and take down
r=$(post "" /styles/report -d "id=$SR" --data-urlencode "reason=rude words")
check "signed out: report goes to login" '[[ "$r" == *"/login"* ]]' "$r"
r=$(post $STRANGER /styles/report -d "id=$SR" --data-urlencode "reason=rude words")
check "stranger reports Sun Room" '[[ "$r" != *"err="* ]] && grep -qP "^str\t$SR\tstranger\t" $W/store.lgp' "$r $(grep '^str' $W/store.lgp)"
r=$(page $STRANGER /admin/styles adm-stranger.html)
check "non-operator: /admin/styles is 404" '[ "$r" = 404 ]' "$r"
r=$(page $ADMIN /admin/styles adm.html)
check "operator: /admin/styles answers 200" '[ "$r" = 200 ]' "$r"
check "...lists the reported Sun Room with the reason" 'grep -q "Sun Room" $W/adm.html && grep -q "rude words" $W/adm.html' "missing"
check "...reported first (Sun Room before Night Paper)" 'before adm.html "Sun Room" "Night Paper"' "order"
r=$(post $STRANGER /admin/styles/takedown -d "id=$SR")
check "non-operator cannot take a style down" '[ -n "$(style_id "Sun Room")" ]' "$r"
r=$(post $PAL /styles/remove -d "id=$SR")
check "a non-author cannot remove someone else's style" '[ -n "$(style_id "Sun Room")" ]' "$r"
r=$(post $ADMIN /admin/styles/takedown -d "id=$SR")
check "operator takes Sun Room down" '[ -z "$(style_id "Sun Room")" ]' "$r"
check "...its report rows are gone" '! grep -qP "^str\t$SR\t" $W/store.lgp' "$(grep '^str' $W/store.lgp)"
check "...a copy is in the moderation archive" 'grep -q "Sun Room" $W/store.lgp.moderation.jsonl 2>/dev/null' "no archive line"
page "" /styles after-td.html >/dev/null
check "Sun Room is gone from /styles" '! grep -q "Sun Room" $W/after-td.html' "still listed"
check "control: Night Paper is still listed" 'grep -q "Night Paper" $W/after-td.html' "gone too"

# --- blocks hide styles both ways
post $STRANGER /settings/block -d "who=pal" >/dev/null
page $STRANGER /styles blk.html >/dev/null
check "stranger blocked pal: pal's Night Paper is hidden from stranger" '! grep -q "Night Paper" $W/blk.html' "shown"
page $PAL /styles blk-pal.html >/dev/null
r=$(page $ADMIN /styles blk-admin.html)
check "control: Bbottin still sees Night Paper" 'grep -q "Night Paper" $W/blk-admin.html' "hidden"

# --- survives a restart
stop; start
page "" "/styles?sort=used" restart.html >/dev/null
check "after a restart Night Paper and its use count are kept" 'grep -q "Night Paper" $W/restart.html && grep -q "lg-style-uses\">1 use<" $W/restart.html' "$(grep -o 'lg-style-uses">[^<]*' $W/restart.html)"

# --- the author removes it; copies stay
r=$(post $PAL /styles/remove -d "id=$NP")
check "pal removes Night Paper" '[ -z "$(style_id "Night Paper")" ]' "$r"
check "...its use rows go with it" '! grep -qP "^stu\t$NP\t" $W/store.lgp' "$(grep '^stu' $W/store.lgp)"
page "" /u/stranger u-stranger3.html >/dev/null
check "stranger's copied CSS stays" 'grep -qF "7e3fa1" $W/u-stranger3.html' "copy gone"
page $STRANGER /me me-s4.html >/dev/null
check "...but the based-on line is gone (the style is gone)" '! grep -q "lg-based-on" $W/me-s4.html' "still shown"
check "control: the CSS box still holds the copy" 'grep -qF "7e3fa1" $W/me-s4.html' "box empty"

echo "passes=$passes fails=$fails"
[ $fails = 0 ]
