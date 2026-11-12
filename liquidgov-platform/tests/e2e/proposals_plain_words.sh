#!/usr/bin/env bash
# Proposals in plain words (Brief 7 item 2, PROPOSALS-AUDIT section 4 "Plain language"). The intro sent new members to
# /laws/13 (charter text) for "how do I answer this?", and labels drifted (Cast, Open for votes, Open choice).
#   L1 /proposals and /officials open with the audit's "How this works" line; no Phase 2 / /laws/13 link on /proposals
#   L2 the create form: "Yes, no, or skip", "Let people add another option", "Who could act on this? (optional",
#      and the button Publish (not "Open for votes")
#   L3 card eyebrows: Yes/No "Yes, no, or skip"; open choice "Pick one (people can add options)"; closed "Pick one";
#      Number/Date/Scale submit with "Save my answer" (no "Cast" button anywhere)
#   L4 control: /officials keeps "Not a certified election."
# Usage: bash proposals_plain_words.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18116. Never touches the live site. New file: committed suites are frozen.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-proposals-plain-words; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18116; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
PAL=$(sid pal); STRANGER=$(sid stranger)
[ -n "$PAL" ] && [ -n "$STRANGER" ] || { echo "seed gave no sessions"; exit 1; }

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
HOW='How this works: Anyone with an account can start a question or add their answer. You can change your answer at any time. This is a straw poll, not an official election, and nothing is sent to the city.'
# visible text of a saved page, tags dropped and whitespace folded (a link inside the line still counts)
text_of() { python3 -c 'import re,sys,html; t=re.sub(r"<[^>]+>","",open(sys.argv[1]).read()); print(re.sub(r"\s+"," ",html.unescape(t)))' "$W/$1"; }
# eyebrow of the proposal titled $2: the <p ... id="proposal-N"> right before its <h3>
eyebrow_of() { python3 -c 'import re,sys,html; m=re.search(r"id=\"proposal-\d+\">([^<]*)</p>\s*<h3[^>]*>"+re.escape(sys.argv[2])+"</h3>", open(sys.argv[1]).read()); print(html.unescape(m.group(1)) if m else "")' "$W/$1" "$2"; }

start
post $PAL /proposals -d "title=Plant trees" -d "body=Along Blackstone" -d "kind=0" >/dev/null
post $PAL /proposals -d "title=Benches" -d "body=How many" -d "kind=1" >/dev/null
post $PAL /proposals -d "title=Cleanup day" -d "body=When" -d "kind=2" >/dev/null
post $PAL /proposals -d "title=Park pick" -d "body=Which" -d "kind=3" --data-urlencode $'options=A park\nB park' >/dev/null
post $PAL /proposals -d "title=Dog park name" -d "body=Names" -d "kind=3" -d "open=1" --data-urlencode "options=Bark" >/dev/null
post $PAL /proposals -d "title=Lights" -d "body=Brightness" -d "kind=4" >/dev/null
post $PAL /officials --data-urlencode "title=Mayor" --data-urlencode "place=Fresno, CA" --data-urlencode $'options=Ann\nBo' >/dev/null
get $STRANGER /proposals p.html >/dev/null; text_of p.html > $W/p.txt
get $STRANGER /officials o.html >/dev/null; text_of o.html > $W/o.txt
check "precondition: six proposals listed" '[ "$(grep -o "id=\"proposal-[0-9]*\"" $W/p.html | sort -u | wc -l)" = 6 ]' "$(grep -o 'id="proposal-[0-9]*"' $W/p.html | tr '\n' ' ')"

check "L1 /proposals has the How this works line" 'grep -qF "$HOW" $W/p.txt' "$(head -c 700 $W/p.txt | tail -c 400)"
check "L1 /officials has the How this works line" 'grep -qF "$HOW" $W/o.txt' "missing"
check "L1 /proposals has no Phase 2 / /laws/13 link" '! grep -q "/laws/13" $W/p.html && ! grep -qw "Phase 2" $W/p.txt' "$(grep -o '<a href="/laws/13">[^<]*</a>' $W/p.html)"

check "L2 kind option reads Yes, no, or skip" 'grep -q "<option value=\"0\">Yes, no, or skip</option>" $W/p.html' "$(grep -o '<option value="0">[^<]*</option>' $W/p.html)"
check "L2 write-in box reads Let people add another option" 'grep -qF "Let people add another option" $W/p.txt && ! grep -qF "Allow people to add a choice" $W/p.txt' "old label"
check "L2 who-acts label reads Who could act on this? (optional" 'grep -qF "Who could act on this? (optional" $W/p.txt && ! grep -qF "Who could act on it (optional)" $W/p.txt' "old label"
check "L2 the form button is Publish" 'grep -q "<button>Publish</button>" $W/p.html && ! grep -qF "Open for votes" $W/p.txt' "$(grep -o '<button>[^<]*</button>' $W/p.html | head -3 | tr '\n' ' ')"

e=$(eyebrow_of p.html "Plant trees"); check "L3 Yes/No eyebrow says Yes, no, or skip" '[[ "$e" == *"· Yes, no, or skip" ]]' "$e"
e=$(eyebrow_of p.html "Dog park name"); check "L3 open choice eyebrow says Pick one (people can add options)" '[[ "$e" == *"· Pick one (people can add options)" ]]' "$e"
e=$(eyebrow_of p.html "Park pick"); check "L3 closed choice eyebrow says Pick one (whole label)" '[[ "$e" == *"· Pick one" ]]' "$e"
n=$(grep -o "<button[^>]*>Save my answer</button>" $W/p.html | wc -l | tr -d " ")
check "L3 Number, Date and Scale each submit with Save my answer" '[ "$n" = 3 ]' "count=$n"
check "L3 no Cast button anywhere" '! grep -q "<button[^>]*>Cast</button>" $W/p.html' "Cast found"

check "L4 control: /officials still says Not a certified election." 'grep -qF "Not a certified election." $W/o.txt' "lost"

echo "passes=$passes fails=$fails"
echo "VERDICT proposals_plain_words $([ $fails -eq 0 ] && echo ALL-PASS || echo FAIL) $passes/$((passes+fails))"
[ $fails -eq 0 ]
