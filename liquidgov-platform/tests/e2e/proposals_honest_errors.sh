#!/usr/bin/env bash
# Honest errors (Brief 7 item 5, PROPOSALS-AUDIT matrix): err=range had no message; a refused answer came back with
# no word; duplicate options and impossible dates were accepted; a full option list said "Duplicate, empty, or you
# already added one".
#   E1 a bad number range at create shows a message
#   E2 a refused answer (out of range, a Yes/No choice that does not exist, a race vote by name) comes back at the
#      proposal with err=vote and "Your answer didn't save" on /proposals, ?id= and /officials; nothing is stored
#   E3 duplicate options (any letter case) are refused at create, on /proposals and /officials
#   E4 impossible dates (Feb 31, Feb 29 in a common year, Apr 31) are refused; Feb 29 in a leap year is kept
#   E5 the 49th option says the list is full; a duplicate add keeps its own message
#   E6 control: a good answer comes back with no err
# Usage: bash proposals_honest_errors.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18119. Never touches the live site. New file: committed suites are frozen.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-proposals-honest-errors; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18119; URL=http://127.0.0.1:$PORT
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
loc() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{redirect_url}' -b "lg_session=$s" "$@" "$URL$p"; }
# the visible text of a page fetched at the full URL $2 (after a redirect)
page_text() { curl -s -b "lg_session=$1" "$2" | python3 -c 'import re,sys,html; t=re.sub(r"<[^>]+>"," ",sys.stdin.read()); print(re.sub(r"\s+"," ",html.unescape(t)))'; }
props() { grep -c "^prop	" $W/store.lgp; }

start
post $PAL /proposals -d "title=Benches" -d "body=How many" -d "kind=1" -d "min=0" -d "max=10" >/dev/null           # 1
post $PAL /proposals -d "title=Cleanup day" -d "body=When" -d "kind=2" >/dev/null                                   # 2
post $PAL /proposals -d "title=Plant trees" -d "body=Along Blackstone" -d "kind=0" >/dev/null                       # 3
post $PAL /proposals -d "title=Dog park name" -d "body=Names" -d "kind=3" -d "open=1" --data-urlencode "options=Bark" >/dev/null  # 4
post $PAL /officials --data-urlencode "title=Mayor" --data-urlencode "place=Fresno, CA" --data-urlencode $'options=Ann\nBo' >/dev/null  # 5
check "precondition: four proposals and a race" '[ "$(props)" = 5 ]' "$(props)"

l=$(loc $PAL /proposals -d "title=Bad range" -d "body=x" -d "kind=1" -d "min=9" -d "max=2")
w=$(curl -s -b "lg_session=$PAL" "$l" | grep -o '<p class="warn">[^<]*</p>')
check "E1 a bad number range says so (err=range has a warning naming the minimum)" '[[ "$l" == *"err=range"* && "$w" == *"inimum"* ]]' "$l :: $w"

l=$(loc $STRANGER /proposals/vote -d "id=1" -d "value=99" -d "next=/proposals")
check "E2 an out-of-range number is refused back at the proposal with err=vote" '[ "$l" = "$URL/proposals?err=vote#proposal-1" ]' "$l"
t=$(page_text $STRANGER "$l")
check "E2 the page says the answer didn't save" '[[ "$t" == *"Your answer didn"*"t save"* ]]' "$(echo "$t" | head -c 600)"
check "E2 nothing was stored" '! grep -q "^pbv	1	stranger	" $W/store.lgp' "$(grep "^pbv	1" $W/store.lgp)"
l=$(loc $STRANGER /proposals/vote -d "id=3" -d "choice=7" -d "next=/proposals")
check "E2 a Yes/No choice that does not exist is refused, not turned into Skip" '[[ "$l" == *"err=vote"* ]] && ! grep -q "^pvote	3	stranger" $W/store.lgp' "$l $(grep "^pvote" $W/store.lgp)"
l=$(loc $STRANGER /proposals/vote -d "id=5" -d "value=Ann" -d "next=/officials")
t=$(page_text $STRANGER "$l")
check "E2 /officials: a refused vote says so there" '[[ "$l" == "$URL/officials?err=vote#proposal-5" && "$t" == *"Your answer didn"*"t save"* ]]' "$l"
l=$(loc $STRANGER /proposals/vote -d "id=1" -d "value=99" --data-urlencode "next=/proposals?id=1")
t=$(page_text $STRANGER "$l")
check "E2 a single proposal page shows the message too" '[[ "$l" == "$URL/proposals?id=1&err=vote#proposal-1" && "$t" == *"Your answer didn"*"t save"* ]]' "$l"

l=$(loc $PAL /proposals -d "title=Dupes" -d "body=x" -d "kind=3" --data-urlencode $'options=Park\npark')
t=$(page_text $PAL "$l")
check "E3 duplicate options at create are refused and named" '[[ "$l" == *"err=dupe"* && "$t" == *"same"* ]] && [ "$(props)" = 5 ]' "$l props=$(props)"
l=$(loc $PAL /officials --data-urlencode "title=Clerk" --data-urlencode "place=Fresno, CA" --data-urlencode $'options=Ann\nANN')
t=$(page_text $PAL "$l")
check "E3 /officials: duplicate candidates refused with the same message" '[[ "$l" == *"/officials?err=dupe"* && "$t" == *"same"* ]] && [ "$(props)" = 5 ]' "$l props=$(props)"

l=$(loc $STRANGER /proposals/vote -d "id=2" -d "value=2027-02-31" -d "next=/proposals")
check "E4 an impossible date (Feb 31) is refused" '[[ "$l" == *"err=vote"* ]] && ! grep -q "^pbv	2	stranger" $W/store.lgp' "$l"
l=$(loc $STRANGER /proposals/vote -d "id=2" -d "value=2027-02-29" -d "next=/proposals")
check "E4 Feb 29 in a common year is refused" '[[ "$l" == *"err=vote"* ]]' "$l"
l=$(loc $STRANGER /proposals/vote -d "id=2" -d "value=2028-02-29" -d "next=/proposals")
check "E4 control: Feb 29 in a leap year is kept" '[[ "$l" != *"err="* ]] && grep -q "^pbv	2	stranger	2028-02-29$" $W/store.lgp' "$l"
l=$(loc $STRANGER /proposals/vote -d "id=2" -d "value=2027-04-31" -d "next=/proposals")
check "E4 April 31 is refused (the answer stays 2028-02-29)" '[[ "$l" == *"err=vote"* ]] && grep -q "^pbv	2	stranger	2028-02-29$" $W/store.lgp' "$l"

for i in $(seq 2 48); do post $PAL /proposals/option -d "id=4" -d "label=Name $i" >/dev/null; done
check "precondition: the list holds 48 options" '[ "$(grep -c "^pbo	4	" $W/store.lgp)" = 48 ]' "$(grep -c "^pbo	4	" $W/store.lgp)"
l=$(loc $STRANGER /proposals/option -d "id=4" -d "label=One more" -d "next=/proposals")
t=$(page_text $STRANGER "$l")
check "E5 the 49th option says the list is full, back at the proposal" '[[ "$l" == "$URL/proposals?err=optfull#proposal-4" && "$t" == *"full"* ]]' "$l :: $(echo "$t" | grep -o 'Proposals.\{0,120\}' | head -1)"
post $PAL /proposals -d "title=Two names" -d "body=x" -d "kind=3" -d "open=1" --data-urlencode "options=Rex" >/dev/null
l=$(loc $STRANGER /proposals/option -d "id=6" -d "label=rex" -d "next=/proposals")
check "E5 control: a duplicate add keeps its own message (err=option), not full" '[ "$l" = "$URL/proposals?err=option#proposal-6" ]' "$l"

l=$(loc $STRANGER /proposals/vote -d "id=1" -d "value=4" -d "next=/proposals")
check "E6 control: a good answer comes back with no err" '[ "$l" = "$URL/proposals#proposal-1" ] && grep -q "^pbv	1	stranger	4$" $W/store.lgp' "$l"

echo "passes=$passes fails=$fails"
echo "VERDICT proposals_honest_errors $([ $fails -eq 0 ] && echo ALL-PASS || echo FAIL) $passes/$((passes+fails))"
[ $fails -eq 0 ]
