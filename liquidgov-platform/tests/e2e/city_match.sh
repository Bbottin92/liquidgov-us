#!/usr/bin/env bash
# People who Flowed the same subject (local only). The subject is the phrase,
# case-folded. A shared word is not enough. You see those people only after
# you Flow your own box, and only when map_person_ok already allows them.
# No count. Throwaway store, port 18087. Never touches the live site or port 8080.
# Usage: bash city_match.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-city-match; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18087; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
PAL=$(sid pal); STRANGER=$(sid stranger); ADMIN=$(sid Bbottin)
[ -n "$PAL" ] && [ -n "$STRANGER" ] && [ -n "$ADMIN" ] || { echo "seed gave no sessions"; exit 1; }

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
get()  { curl -s -o "$W/$3" -w '%{http_code} %{redirect_url}' -b "lg_session=$1" "$URL$2"; }
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{http_code} %{redirect_url}' -b "lg_session=$s" "$@" "$URL$p"; }
node() { curl -s -o "$W/$3" -w '%{http_code}' -b "lg_session=$1" -G --data-urlencode "n=$2" "$URL/map/node"; }
has() { python3 - "$W/$1" "$2" "${3:-}" <<'EOF'
import json, sys
d = json.load(open(sys.argv[1]))
hit = [l for l in d["links"] if l["node"]["id"] == sys.argv[2] and (not sys.argv[3] or l["edge"] == sys.argv[3])]
sys.exit(0 if hit else 1)
EOF
}
card() { python3 - "$W/$1" "$2" "$3" "$4" <<'EOF'
import pathlib, re, sys
t = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
m = re.search(r'id="pain' + re.escape(sys.argv[2]) + r'"[\s\S]*?</div>', t)
hit = bool(m and sys.argv[3] in m.group(0))
sys.exit(0 if hit == (sys.argv[4] == "yes") else 1)
EOF
}
# The Also-flowed sentence on one card has the name and no digit.
also_line() { python3 - "$W/$1" "$2" "$3" "$4" <<'EOF'
import pathlib, re, sys
t = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
m = re.search(r'id="pain' + re.escape(sys.argv[2]) + r'"[\s\S]*?</div>', t)
card = m.group(0) if m else ""
line = re.search(r'Also flowed by .*?</p>', card)
want = sys.argv[3] == "yes"
ok = bool(line) == want
if line and want:
    ok = sys.argv[4] in line.group(0) and re.search(r'\d', line.group(0)) is None
sys.exit(0 if ok else 1)
EOF
}
no_same() { python3 - "$W/$1" <<'EOF'
import json, sys
d = json.load(open(sys.argv[1]))
sys.exit(0 if all(l["edge"] != "same subject" for l in d["links"]) else 1)
EOF
}
# Name on the subject page's Also-flowed sentence only, not the site header.
page_also() { python3 - "$W/$1" "$2" "$3" <<'EOF'
import pathlib, re, sys
t = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
m = re.search(r'Also flowed by .*?</p>', t)
line = m.group(0) if m else ""
want = sys.argv[3] == "yes"
ok = (sys.argv[2] in line) == want
if want:
    ok = ok and re.search(r"\d", line) is None
sys.exit(0 if ok else 1)
EOF
}

start
post $PAL /feed -F "text=I wish the buses ran later on weekends." -F "anybody=1" >/dev/null
post $PAL /feed -F "text=I wish more buses downtown." -F "anybody=1" >/dev/null
post $STRANGER /feed -F "text=I wish The Buses Ran Later On Weekends." -F "anybody=1" >/dev/null

bus=$(awk -F'	' '$1=="pain" && $3=="pal" && index(tolower($8),"the buses ran later on weekends"){print $2}' "$W/store.lgp")
down=$(awk -F'	' '$1=="pain" && $3=="pal" && index(tolower($8),"more buses downtown"){print $2}' "$W/store.lgp")
sbus=$(awk -F'	' '$1=="pain" && $3=="stranger" && index(tolower($8),"the buses ran later on weekends"){print $2}' "$W/store.lgp")
check "precondition: three suggestions" '[ -n "$bus" ] && [ -n "$down" ] && [ -n "$sbus" ]' "bus=$bus down=$down sbus=$sbus"
check "the stored phrases differ in case" 'grep -q "	the buses ran later on weekends$" $W/store.lgp && grep -q "	The Buses Ran Later On Weekends$" $W/store.lgp' "$(grep ^pain $W/store.lgp)"

post $PAL /problems/keep -d "id=$bus" >/dev/null
post $PAL /problems/keep -d "id=$down" >/dev/null
post $STRANGER /problems/keep -d "id=$sbus" >/dev/null
post $PAL /problems/flow -d "id=$bus" >/dev/null
post $PAL /problems/flow -d "id=$down" >/dev/null

get $PAL /problems alone.html >/dev/null
check "flowing alone shows no one else" 'also_line alone.html $bus no "" && also_line alone.html $down no ""' "early list"
r=$(get $STRANGER /pain/$bus sub.html)
check "a kept-but-still person can open the subject" '[[ "$r" == 200* ]]' "$r"
check "and sees no match list yet" '! grep -q "Also flowed" $W/sub.html' "early list"
r=$(get $ADMIN /pain/$bus admin.html)
check "a third person can open the subject" '[[ "$r" == 200* ]] && grep -q "Flowed by" $W/admin.html' "$r"
check "a third person sees no match list" '! grep -q "Also flowed" $W/admin.html && ! grep -q "stranger" $W/admin.html' "directory"
r=$(node $ADMIN pain:$bus adm.json)
check "a third person's map has no same-subject edge" '[ "$r" = 200 ] && no_same adm.json' "$r"
node $PAL pain:$bus own.json >/dev/null
check "the owner's map has no same-subject edge yet" 'no_same own.json && has own.json user:pal noticed' "edge"

post $STRANGER /problems/flow -d "id=$sbus" >/dev/null
get $PAL /problems both.html >/dev/null
check "the owner sees who else flowed that subject" 'also_line both.html $bus yes stranger' "missing"
check "a different subject stays unmatched" 'also_line both.html $down no ""' "downtown matched"
check "the weekend card does not name the third person" 'card both.html $bus Bbottin no' "directory"
get $STRANGER /problems sprob.html >/dev/null
check "they see the owner on their own card" 'also_line sprob.html $sbus yes pal' "missing"
r=$(get $STRANGER /pain/$bus sub2.html)
check "their subject page names no extra people" '[[ "$r" == 200* ]] && grep -q "Flowed by" $W/sub2.html && ! grep -q "Also flowed" $W/sub2.html' "list"
r=$(get $ADMIN /pain/$bus admin2.html)
check "the third person still sees no match list" '[[ "$r" == 200* ]] && ! grep -q "Also flowed" $W/admin2.html && ! grep -q ">stranger<" $W/admin2.html' "directory"
node $PAL pain:$bus own2.json >/dev/null
check "the owner's map links that person's box" 'has own2.json pain:$sbus "same subject" && ! has own2.json pain:$down' "missing"
node $STRANGER pain:$bus pub.json >/dev/null
check "their map links their own box, not a department" 'has pub.json pain:$sbus "same subject" && has pub.json user:pal noticed && ! grep -q "could-act" $W/pub.json' "wrong links"
node $ADMIN pain:$bus adm2.json >/dev/null
check "the third person's map still has no match edge" 'no_same adm2.json' "edge"

post $ADMIN /feed -F "text=I wish the buses ran later on weekends." -F "anybody=1" >/dev/null
abus=$(awk -F'	' '$1=="pain" && $3=="Bbottin"{print $2}' "$W/store.lgp")
check "precondition: the third suggestion" '[ -n "$abus" ]' "abus=$abus"
post $ADMIN /problems/keep -d "id=$abus" >/dev/null
post $ADMIN /problems/flow -d "id=$abus" >/dev/null
get $PAL /problems three.html >/dev/null
check "both other people are named, with no count" 'also_line three.html $bus yes stranger && also_line three.html $bus yes Bbottin' "missing"
r=$(get $ADMIN /pain/$bus admin3.html)
check "once they flow it, the page names the other person" '[[ "$r" == 200* ]] && page_also admin3.html stranger yes && page_also admin3.html Bbottin no' "list"

post $PAL /friends/block -d "who=stranger" >/dev/null
get $PAL /problems blk.html >/dev/null
check "a block removes that person from the list" 'card blk.html $bus stranger no && also_line blk.html $bus yes Bbottin' "still listed"
post $PAL /settings/unblock -d "who=stranger" >/dev/null
get $PAL /problems unblk.html >/dev/null
check "unblocking brings them back" 'also_line unblk.html $bus yes stranger' "missing"

post $STRANGER /settings/map -d groups=1 -d posts=1 -d comments=1 -d pains=1 >/dev/null
get $PAL /problems hide.html >/dev/null
check "show-me off removes them from the list" 'card hide.html $bus stranger no && also_line hide.html $bus yes Bbottin' "still listed"
post $STRANGER /settings/map -d show=1 -d groups=1 -d posts=1 -d comments=1 -d pains=1 >/dev/null
get $PAL /problems show.html >/dev/null
check "show-me on brings them back" 'also_line show.html $bus yes stranger' "missing"

post $STRANGER /settings/privacy -d findability=1 -d profile_visibility=3 -d posts_visibility=0 >/dev/null
get $PAL /problems only.html >/dev/null
check "only-me removes them from the list" 'card only.html $bus stranger no' "still listed"
post $STRANGER /settings/privacy -d findability=1 -d profile_visibility=0 -d posts_visibility=0 >/dev/null
get $PAL /problems pubprof.html >/dev/null
check "a visible profile brings them back" 'also_line pubprof.html $bus yes stranger' "missing"

post $STRANGER /problems/still -d "id=$sbus" >/dev/null
get $PAL /problems still.html >/dev/null
check "Still removes them from the list" 'card still.html $bus stranger no && also_line still.html $bus yes Bbottin' "still listed"
get $STRANGER /problems sstill.html >/dev/null
check "their own Still card has no match list" 'also_line sstill.html $sbus no ""' "list"
r=$(get $STRANGER /pain/$bus after.html)
check "they can still open the subject, with no list" '[[ "$r" == 200* ]] && ! grep -q "Also flowed" $W/after.html' "list"

post $STRANGER /problems/flow -d "id=$sbus" >/dev/null
post $STRANGER /problems/forget -d "id=$sbus" >/dev/null
get $PAL /problems gone.html >/dev/null
check "Forget removes them from the list" 'card gone.html $bus stranger no && also_line gone.html $bus yes Bbottin' "still listed"

check "server log has no crash lines" '! grep -qiE "abort|segfault|terminate called" $W/server.log' "$(tail -3 $W/server.log)"
stop
echo "passes=$passes fails=$fails"
echo "EXIT=$fails"
