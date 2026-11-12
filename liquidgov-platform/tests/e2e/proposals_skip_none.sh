#!/usr/bin/env bash
# Every kind can be skipped, Choice has None of these, the scale has no hidden 50 (Brief 7 item 4, PROPOSALS-AUDIT
# matrix: "Abstain / no preference: Missing", "None of these: Missing", "Accidental scale vote of 50: Missing").
# Rules written down first (lessons 2026-11-08): never answered = no row, counted nowhere, as before; Skip = a stored
# answer "skip", shown as "N skipped", never in a median or range; None of these = "none", its own count; an old tab
# whose scale was never moved sends no value, so nothing is saved (not a 50).
#   K1 Yes/No offers Skip (the existing Abstain)
#   K2 Number, Date, Scale and both Choices offer "Skip / no preference" (value=skip); a skip is stored and is not a
#      number (median from the others, "1 skipped"), and the skipper sees it checked
#   K3 closed and open Choice offer "None of these" (value=none), counted on its own
#   K4 the scale: no value= before you answer, no name= until moved, Save disabled; an empty Save stores nothing
#   K5 a date skip is never the latest date
#   K6 control: other words are still refused, whole token and exact case
# Usage: bash proposals_skip_none.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18118. Never touches the live site. New file: committed suites are frozen.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-proposals-skip-none; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18118; URL=http://127.0.0.1:$PORT
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
# value= of the vote form on proposal $2 whose button reads exactly $3 (whole label; count and check mark allowed)
offer_of() { python3 - "$W/$1" "$2" "$3" <<'PY'
import re, sys
html, rid, label = open(sys.argv[1]).read(), sys.argv[2], sys.argv[3]
for f in re.findall(r'<form[^>]*action="/proposals/vote".*?</form>', html, re.S):
    if not re.search(r'name="id" value="%s"' % re.escape(rid), f):
        continue
    b = re.search(r'<button[^>]*>(.*?)</button>', f, re.S)
    if not b or not re.fullmatch(re.escape(label) + r'(?: · \d+)?(?: &#10003;)?', b.group(1).strip()):
        continue
    v = re.search(r'name="(?:value|choice)" value="([^"]*)"', f)
    print(v.group(1) if v else "")
    break
PY
}
# the card of proposal $2 on a saved page, as text with tags dropped
card_text() { python3 -c 'import re,sys,html; h=open(sys.argv[1]).read(); i=h.find("id=\"proposal-"+sys.argv[2]+"\""); j=h.find("id=\"proposal-",i+5); c=h[i:j if j>0 else len(h)]; print(re.sub(r"\s+"," ",html.unescape(re.sub(r"<[^>]+>"," ",c))))' "$W/$1" "$2"; }
# the scale input of proposal $2: prints "<value attr or -> <name attr or -> <Save button disabled 0|1>"
scale_of() { python3 - "$W/$1" "$2" <<'PY'
import re, sys
html, rid = open(sys.argv[1]).read(), sys.argv[2]
for f in re.findall(r'<form[^>]*action="/proposals/vote".*?</form>', html, re.S):
    if 'name="id" value="%s"' % rid not in f or 'type="range"' not in f:
        continue
    inp = re.search(r'<input type="range"[^>]*>', f).group(0)
    v = re.search(r'\svalue="([^"]*)"', inp); n = re.search(r'\sname="([^"]*)"', inp)
    btn = re.search(r'<button([^>]*)>Save my answer</button>', f)
    print(v.group(1) if v else "-", n.group(1) if n else "-", 1 if btn and re.search(r'\sdisabled\b', btn.group(1)) else 0)
PY
}
row() { grep -c "^pbv	$1	$2	$3$" $W/store.lgp; }

start
post $PAL /proposals -d "title=Plant trees" -d "body=Along Blackstone" -d "kind=0" >/dev/null                          # 1
post $PAL /proposals -d "title=Benches" -d "body=How many" -d "kind=1" >/dev/null                                      # 2
post $PAL /proposals -d "title=Cleanup day" -d "body=When" -d "kind=2" >/dev/null                                      # 3
post $PAL /proposals -d "title=Park pick" -d "body=Which" -d "kind=3" --data-urlencode $'options=A park\nB park' >/dev/null  # 4
post $PAL /proposals -d "title=Dog park name" -d "body=Names" -d "kind=3" -d "open=1" --data-urlencode "options=Bark" >/dev/null  # 5
post $PAL /proposals -d "title=Lights" -d "body=Brightness" -d "kind=4" >/dev/null                                     # 6
get $STRANGER /proposals p.html >/dev/null
check "precondition: six proposals" '[ "$(grep -o "id=\"proposal-[0-9]*\"" $W/p.html | sort -u | wc -l)" = 6 ]' "$(grep -o 'id="proposal-[0-9]*"' $W/p.html | tr '\n' ' ')"

v=$(offer_of p.html 1 "Skip"); check "K1 Yes/No offers Skip (the existing Abstain, choice=2)" '[ "$v" = 2 ]' "value=$v"
for id in 2 3 4 5 6; do
    v=$(offer_of p.html $id "Skip / no preference"); check "K2 proposal $id offers Skip / no preference (value=skip)" '[ "$v" = skip ]' "value=$v"
done
for id in 4 5; do
    v=$(offer_of p.html $id "None of these"); check "K3 choice $id offers None of these (value=none)" '[ "$v" = none ]' "value=$v"
done

read sv sn sd <<<"$(scale_of p.html 6)"
check "K4 the scale does not pre-fill 50 (no value= before you answer)" '[ "$sv" = "-" ]' "value=$sv"
check "K4 the untouched scale sends nothing (no name= until moved)" '[ "$sn" = "-" ]' "name=$sn"
check "K4 Save my answer is disabled until the voter picks" '[ "$sd" = 1 ]' "disabled=$sd"
post $STRANGER /proposals/vote -d "id=6" -d "next=/proposals" >/dev/null
check "K4 a Save with no mark stores nothing" '! grep -q "^pbv	6	" $W/store.lgp' "$(grep "^pbv	6" $W/store.lgp)"
post $STRANGER /proposals/vote -d "id=6" -d "value=30" >/dev/null
get $STRANGER /proposals p2.html >/dev/null
read sv sn sd <<<"$(scale_of p2.html 6)"
check "K4 after answering 30: shows 30, sends it, Save enabled" '[ "$sv" = 30 ] && [ "$sn" = value ] && [ "$sd" = 0 ]' "$sv $sn $sd"

post $STRANGER /proposals/vote -d "id=2" -d "value=skip" >/dev/null
post $PAL /proposals/vote -d "id=2" -d "value=10" >/dev/null
check "K2 Skip is stored as its own answer" '[ "$(row 2 stranger skip)" = 1 ]' "$(grep "^pbv	2" $W/store.lgp)"
get $STRANGER /proposals p3.html >/dev/null; t=$(card_text p3.html 2)
check "K2 a skip is not a number: the median is 10 from 1 response, 1 skipped" '[[ "$t" == *"Median: 10"* && "$t" == *"1 response "* && "$t" == *"1 skipped"* ]]' "$t"
check "K2 the skipper sees Skip / no preference checked" '[ "$(offer_of p3.html 2 "Skip / no preference · 1 &#10003;")" = skip ] || [ "$(offer_of p3.html 2 "Skip / no preference &#10003;")" = skip ]' "$(grep -o 'Skip / no preference[^<]*' $W/p3.html | head -3 | tr '\n' ';')"

post $STRANGER /proposals/vote -d "id=3" -d "value=skip" >/dev/null
post $PAL /proposals/vote -d "id=3" -d "value=2027-01-05" >/dev/null
get $STRANGER /proposals p4.html >/dev/null; t=$(card_text p4.html 3)
check "K5 a date skip is not the latest date" '[[ "$t" == *"latest 2027-01-05"* && "$t" != *"latest skip"* && "$t" == *"1 skipped"* ]]' "$t"

post $STRANGER /proposals/vote -d "id=4" -d "value=none" >/dev/null
post $PAL /proposals/vote -d "id=4" -d "value=0" >/dev/null
get $STRANGER /proposals p5.html >/dev/null
check "K3 None of these is stored and counted on its own" '[ "$(row 4 stranger none)" = 1 ] && [ "$(offer_of p5.html 4 "None of these · 1 &#10003;")" = none ] && [ "$(offer_of p5.html 4 "A park · 1")" = 0 ]' "$(grep "^pbv	4" $W/store.lgp | tr '\n' ';')"

post $STRANGER /proposals/vote -d "id=2" -d "value=maybe" >/dev/null
check "K6 control: a made-up word is still refused (skip stays)" '[ "$(row 2 stranger skip)" = 1 ] && ! grep -q "maybe" $W/store.lgp' "$(grep "^pbv	2" $W/store.lgp)"
post $STRANGER /proposals/vote -d "id=4" -d "value=None" >/dev/null
check "K6 control: whole token, exact case: None is not none" '[ "$(row 4 stranger none)" = 1 ]' "$(grep "^pbv	4" $W/store.lgp)"

echo "passes=$passes fails=$fails"
echo "VERDICT proposals_skip_none $([ $fails -eq 0 ] && echo ALL-PASS || echo FAIL) $passes/$((passes+fails))"
[ $fails -eq 0 ]
