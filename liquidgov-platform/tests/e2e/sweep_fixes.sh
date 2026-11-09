#!/usr/bin/env bash
# Site sweep fixes, batch 1 (Brandon 2026-11-09: "make it more user friendly ... settings ... organized ... error
# handling"; report scratch/2026-11-09-claude-site-sweep/report.md). Server-level checks; the visual ones (the
# Juketube button's space, the phone map's first view, press-and-hold) are a browser check in the same scratch folder.
#   /logs works for the operator (it read LIQUIDGOV_OPERATOR, which the live service never sets) and only for them;
#   anchor targets stop below the sticky header; Proposals list first with the form behind "New proposal" (open when
#   prefilled, after an error, or with nothing to read); initials instead of empty avatar circles; a Settings index;
#   plain words (Log in, Proposals, the Departments search hint, city names not keys); every Laws chapter's narration
#   (route for 1..14 with byte ranges, a Listen button, bad numbers 404); the footer Disclaimer size; the map's hint
#   and the phone press-and-hold code are served.
# Usage: bash sweep_fixes.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18100. Runs from liquidgov-platform/ (the server reads assets/ from its working directory).
set -u
BIN=$1; SEED=$2
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-sweep-fixes; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18100; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
[ -f "$HERE/assets/chapter2.mp3" ] || { echo "no assets/chapter2.mp3 under $HERE"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
ADMIN=$(sid Bbottin); PAL=$(sid pal)
[ -n "$ADMIN" ] && [ -n "$PAL" ] || { echo "seed gave no sessions"; exit 1; }

SERVER=""
start() {
    # exactly the live service's operator setting: LIQUIDGOV_OPERATORS only (LIQUIDGOV_OPERATOR unset on purpose)
    (cd "$HERE" && exec env -u LIQUIDGOV_OPERATOR LIQUIDGOV_OPERATORS=Bbottin LIQUIDGOV_STORE=$W/store.lgp \
        LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT "$BIN") >> "$W/server.log" 2>&1 &
    SERVER=$!
    for i in $(seq 50); do
        kill -0 $SERVER 2>/dev/null || { echo "server died"; cat "$W/server.log"; exit 1; }
        curl -s -o /dev/null --max-time 1 $URL/ && return
        sleep 0.2
    done
    echo "server did not start"; cat "$W/server.log"; kill $SERVER; exit 1
}
stop() { [ -n "$SERVER" ] && kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null; SERVER=""; }
trap stop EXIT
get()  { curl -s -o "$W/$3" -w '%{http_code}' -b "lg_session=$1" "$URL$2"; }
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{http_code} %{redirect_url}' -b "lg_session=$s" "$@" "$URL$p"; }

start

# /logs
r=$(get $ADMIN /logs logs-op.txt); check "/logs: the operator gets it (200) with only LIQUIDGOV_OPERATORS set" '[ "$r" = 200 ]' "$r"
r=$(get $PAL /logs logs-pal.txt); check "/logs: a member gets 404" '[ "$r" = 404 ]' "$r"
r=$(curl -s -o /dev/null -w '%{http_code}' $URL/logs); check "/logs: logged out gets 404" '[ "$r" = 404 ]' "$r"

# Proposals: open form with nothing to read; closed once there is a proposal; open again with ?title= and ?err=
r=$(get $PAL /proposals p0.html)
check "proposals, none yet: the form is open" 'grep -q "<details class=\"card newprop\" id=\"new\" open><summary>New proposal</summary>" $W/p0.html' "$r"
check "proposals intro: plain words, Phase 2 links to chapter 13" 'grep -q "not an identity-verified vote" $W/p0.html && grep -q "href=\"/laws/13\">Phase 2</a>" $W/p0.html && ! grep -q "CitizenId vote" $W/p0.html' "$r"
post $PAL /proposals --data-urlencode "title=Shade trees on Blackstone" --data-urlencode "body=Plant them." -d "kind=0" >/dev/null
r=$(get $PAL /proposals p1.html)
check "proposals, one exists: the form is closed behind New proposal" 'grep -q "<details class=\"card newprop\" id=\"new\"><summary>New proposal</summary>" $W/p1.html' "$r"
check "proposals, one exists: it is listed" 'grep -q "Shade trees on Blackstone" $W/p1.html' "$r"
r=$(get $PAL "/proposals?title=Bus+shelters" p2.html); check "proposals ?title=: open and prefilled" 'grep -q "id=\"new\" open>" $W/p2.html && grep -q "value=\"Bus shelters\"" $W/p2.html' "$r"
r=$(get $PAL "/proposals?err=title" p3.html); check "proposals after an error: open" 'grep -q "id=\"new\" open>" $W/p3.html' "$r"
check "proposals: #new link opens it (script present)" 'grep -q "location.hash===.#new.)d.open=true" $W/p1.html' "no script"

# initials, settings index, login copy, anchors under the header, footer
r=$(get $PAL /u/pal u.html); check "empty avatar shows the initial, not an empty circle" 'grep -q "ph\" aria-hidden=\"true\">P</span>" $W/u.html' "$r"
check "anchor targets stop below the sticky header" 'grep -q "\[id\]{scroll-margin-top:5.5rem}" $W/u.html' "no css"
check "footer Disclaimer matches the footer links" 'grep -q "footer.site summary{font-size:inherit" $W/u.html' "no css"
r=$(get $PAL /settings s.html)
check "settings: an index of all nine sections" '[ "$(grep -o "<a href=\"#[a-z]*\">" $W/s.html | wc -l)" -ge 9 ] && grep -q "class=\"setidx\"" $W/s.html' "$r"
for a in security sessions map privacy blocks jukebox identity data notifications; do
    check "settings: section #$a exists" 'grep -q "<h2 id=\"$a\">" $W/s.html' "missing"
done
r=$(curl -s -o "$W/login.html" -w '%{http_code}' $URL/login); check "log in: plain words" 'grep -q "Log in with your username and password." $W/login.html && ! grep -q "daily credential" $W/login.html' "$r"

# Operator > Departments: city names, its own search hint, search by the city's name
post $PAL /communities -d "slug=fres1" -d "name=Fresno Neighbors" -d "category=0" -d "subcategory=Neighbors" -d "about=n" --data-urlencode "city=Fresno, CA" >/dev/null
post $PAL /city/fresno-ca/tag --data-urlencode "tag=Parks Department" >/dev/null
r=$(get $ADMIN /admin/departments d.html)
check "departments: precondition, the department is listed" 'grep -q "Parks Department" $W/d.html' "$r"
check "departments: the city's name, not its key" 'grep -q ">Fresno, CA</a></td>" $W/d.html' "$r"
check "departments: its own search hint" 'grep -q "placeholder=\"City or name\"" $W/d.html' "$r"
r=$(get $ADMIN "/admin/departments?q=Fresno%2C+CA" d2.html); check "departments: search by the city's name finds it" 'grep -q "Parks Department" $W/d2.html' "$r"

# narration
sz2=$(stat -c %s "$HERE/assets/chapter2.mp3")
code=$(curl -s -o "$W/c2.mp3" -w '%{http_code} %{content_type}' $URL/assets/chapter2.mp3)
check "narration: chapter 2 is served whole (200 audio/mpeg, file size)" '[ "$code" = "200 audio/mpeg" ] && [ "$(stat -c %s $W/c2.mp3)" = "$sz2" ]' "$code"
hdr=$(curl -s -D - -o "$W/c2r.bin" -H "Range: bytes=0-1" $URL/assets/chapter2.mp3 | tr -d '\r')
check "narration: a byte range gets 206 + Content-Range + 2 bytes" 'echo "$hdr" | grep -q "^HTTP/1.1 206" && echo "$hdr" | grep -q "^Content-Range: bytes 0-1/$sz2" && [ "$(stat -c %s $W/c2r.bin)" = 2 ]' "$(echo "$hdr" | head -3 | tr '\n' ' ')"
hdr=$(curl -s -D - -o /dev/null -H "Range: bytes=$((sz2+5))-" $URL/assets/chapter2.mp3 | tr -d '\r')
check "narration: a range past the end gets 416" 'echo "$hdr" | grep -q "^HTTP/1.1 416"' "$(echo "$hdr" | head -1)"
for bad in chapter15 chapter0 chapter02 chapterx chapter-1; do
    c=$(curl -s -o /dev/null -w '%{http_code}' $URL/assets/$bad.mp3); check "narration: /assets/$bad.mp3 is 404" '[ "$c" = 404 ]' "$c"
done
c=$(curl -s -o /dev/null -w '%{http_code}' $URL/assets/chapter1.mp3); check "narration: chapter 1 still served (Genesis)" '[ "$c" = 200 ]' "$c"
for n in 1 2 14; do
    r=$(curl -s -o "$W/law$n.html" -w '%{http_code}' $URL/laws/$n)
    check "narration: /laws/$n has a Listen button for chapter$n.mp3" 'grep -q "id=\"narrbtn\"" $W/law$n.html && grep -q "src=\"/assets/chapter$n.mp3\"" $W/law$n.html' "$r"
done
check "narration: a failed play says so (error + play().catch)" 'grep -q "could not play" $W/law2.html && grep -q "p.catch(fail)" $W/law2.html' "no handler"

# map: hint + press-and-hold code served; Juketube button space
r=$(get $PAL /map m.html); check "map: the phone hint says hold to open" 'grep -q "<strong>hold it</strong> to open its page" $W/m.html' "$r"
js=$(curl -s $URL/assets/lgmap.js)
check "map: press-and-hold code served (HOLD_MS, open flag)" 'echo "$js" | grep -q "HOLD_MS = 450" && echo "$js" | grep -q "select(n, !narrow())"' "old lgmap.js"
css=$(curl -s $URL/assets/juketube.css)
check "juketube: room at the bottom of the page for the button" 'echo "$css" | grep -q "^body{padding-bottom:calc("' "old juketube.css"

echo "passes=$passes fails=$fails"
[ "$fails" = 0 ]
