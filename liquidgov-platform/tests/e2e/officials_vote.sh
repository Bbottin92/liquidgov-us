#!/usr/bin/env bash
# Officials votes count (Brief 7 item 1, PROPOSALS-AUDIT "Officials vote by candidate name"). /officials posted the
# candidate's NAME as value=, the store only takes an option index for a Choice, so every vote was dropped with no word.
#   O1 /officials offers each candidate with the same index payload as a Choice proposal (value="1" for the 2nd name)
#   O2 what the page offers is stored (pbv <race> <voter> 1) and shown as "Bo · 1" with the voter's check mark
#   O3 a candidate named "0" is voted for by its own index, not mistaken for the first candidate
#   O4 one encoding: a vote cast from /proposals?id=<race> is the same vote /officials counts
#   O5 a race is not listed twice: /proposals has no card for it, only a labeled link to /officials
#   O6 logged out: counts show, no vote forms (a POST would only bounce to the login page)
#   O7 control: a POST of the candidate's name stores nothing
# Usage: bash officials_vote.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18115. Never touches the live site. New file: committed suites are frozen.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-officials-vote; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18115; URL=http://127.0.0.1:$PORT
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
# the id of the race titled $2 on a saved /officials page (the first id= field after its heading)
race_of() { python3 -c 'import re,sys; m=re.search(r"<h2>"+re.escape(sys.argv[2])+r"</h2>.*?name=\"id\" value=\"(\d+)\"", open(sys.argv[1]).read(), re.S); print(m.group(1) if m else "")' "$W/$1" "$2"; }
# the value= a page's vote form sends for the button labeled exactly $3 on race $2 (whole label: "Bo" is not "Bob")
offer_of() { python3 - "$W/$1" "$2" "$3" <<'PY'
import re, sys
html, rid, label = open(sys.argv[1]).read(), sys.argv[2], sys.argv[3]
for f in re.findall(r'<form[^>]*action="/proposals/vote".*?</form>', html, re.S):
    if not re.search(r'name="id" value="%s"' % re.escape(rid), f):
        continue
    b = re.search(r'<button[^>]*>(.*?)</button>', f, re.S)
    if not b or not re.fullmatch(re.escape(label) + r'(?: · \d+)?(?: &#10003;)?', b.group(1).strip()):
        continue
    v = re.search(r'name="value" value="([^"]*)"', f)
    print(v.group(1) if v else "")
    break
PY
}
pbv_rows() { grep -c "^pbv	$1	" $W/store.lgp; }

start
post $PAL /officials --data-urlencode "title=Mayor race" --data-urlencode "place=Fresno, CA" -d "open=1" --data-urlencode $'options=Ann\nBo\nBob' >/dev/null
post $PAL /officials --data-urlencode "title=Seat zero" --data-urlencode "place=Fresno, CA" --data-urlencode $'options=Zed\n0' >/dev/null
get $STRANGER /officials o.html >/dev/null
RACE=$(race_of o.html "Mayor race"); ZERO=$(race_of o.html "Seat zero")
check "precondition: both races exist" '[ -n "$RACE" ] && [ -n "$ZERO" ]' "race=$RACE zero=$ZERO"

v=$(offer_of o.html "$RACE" "Bo")
check "O1 /officials offers Bo as option index 1 (the Choice payload)" '[ "$v" = "1" ]' "value=$v"
r=$(post $STRANGER /proposals/vote -d "id=$RACE" --data-urlencode "value=$v" -d "next=/officials")
check "O2 the offered vote is stored" 'grep -q "^pbv	$RACE	stranger	1$" $W/store.lgp' "$(grep "^pbv" $W/store.lgp)"
get $STRANGER /officials o2.html >/dev/null
v2=$(offer_of o2.html "$RACE" "Bo · 1 &#10003;")
check "O2 /officials shows Bo · 1 with the voter's check mark" '[ "$v2" = "1" ]' "$(grep -o 'Bo[^<]*</button>' $W/o2.html | head -3)"
v3=$(offer_of o2.html "$RACE" "Bob · 0")
check "O2 Bob (a name that starts with Bo) is still 0" '[ "$v3" = "2" ]' "$(grep -o 'Bob[^<]*</button>' $W/o2.html | head -3)"

vz=$(offer_of o.html "$ZERO" "0")
check "O3 the candidate named 0 is offered as its own index 1" '[ "$vz" = "1" ]' "value=$vz"
post $STRANGER /proposals/vote -d "id=$ZERO" --data-urlencode "value=$vz" -d "next=/officials" >/dev/null
get $STRANGER /officials o3.html >/dev/null
check "O3 the vote counts for 0, not for Zed" '[ "$(offer_of o3.html "$ZERO" "0 · 1 &#10003;")" = "1" ] && [ "$(offer_of o3.html "$ZERO" "Zed · 0")" = "0" ]' "$(grep -o '>[^<>]*· [0-9][^<]*</button>' $W/o3.html | head -8 | tr '\n' ' ')"

post $PAL /proposals/vote -d "id=$RACE" -d "value=0" --data-urlencode "next=/proposals?id=$RACE" >/dev/null
get $STRANGER /officials o4.html >/dev/null
check "O4 a vote cast by index elsewhere counts on /officials (Ann · 1)" '[ "$(offer_of o4.html "$RACE" "Ann · 1")" = "0" ]' "$(grep -o 'Ann[^<]*</button>' $W/o4.html | head -3)"

get $STRANGER /proposals p.html >/dev/null
check "O5 /proposals has no card for the race" '! grep -q "id=\"proposal-$RACE\"" $W/p.html && ! grep -q "Mayor race" $W/p.html' "$(grep -o 'id="proposal-[0-9]*"' $W/p.html | tr '\n' ' ')"
check "O5 /proposals links to /officials with a label about races (not just the menu)" 'grep -qi "<a href=\"/officials\">[^<]*race" $W/p.html' "$(grep -o '<a href="/officials">[^<]*</a>' $W/p.html | tr '\n' ' ')"

curl -s -o $W/out.html $URL/officials
check "O6 logged out: the race shows its counts" 'grep -q "Bo · 1" $W/out.html && grep -q "Ann · 1" $W/out.html' "$(grep -o 'Bo[^<]*' $W/out.html | head -3)"
check "O6 logged out: no vote forms" '! grep -q "action=\"/proposals/vote\"" $W/out.html' "vote form shown"

before=$(pbv_rows $RACE)
post $STRANGER /proposals/vote -d "id=$RACE" --data-urlencode "value=Ann" -d "next=/officials" >/dev/null
check "O7 control: a POST of the name Ann stores nothing new" '[ "$(pbv_rows $RACE)" = "$before" ] && grep -q "^pbv	$RACE	stranger	1$" $W/store.lgp' "$(grep "^pbv	$RACE" $W/store.lgp)"

echo "passes=$passes fails=$fails"
echo "VERDICT officials_vote $([ $fails -eq 0 ] && echo ALL-PASS || echo FAIL) $passes/$((passes+fails))"
[ $fails -eq 0 ]
