#!/usr/bin/env bash
# Problems and solutions (first slice, per Grok's review): noticing wants and
# problems in a person's OWN new public words, Keep/Forget, possible solutions,
# private map boxes, the crisis 988 note, and the off switch.
# Usage: bash problems.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18091. Never touches the live site.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-problems; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18091; URL=http://127.0.0.1:$PORT
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
rows() { grep -c "^pain	" "$W/store.lgp"; }

start
# A city with a bus department, a related proposal and group, so solutions exist.
post $PAL /communities -d "slug=fres1" -d "name=Fresno Bus Riders" -d "category=0" -d "subcategory=Transit" -d "about=riding the buses" --data-urlencode "city=Fresno, CA" >/dev/null
post $PAL /city/fresno-ca/tag --data-urlencode "tag=Fresno Area Express (buses)" >/dev/null
post $PAL /city/fresno-ca/tag --data-urlencode "tag=Parks Department" >/dev/null
post $ADMIN /proposals -d "title=Late-night buses on weekends" -d "body=Route 1" -d "kind=0" >/dev/null

get $PAL /settings set.html >/dev/null
check "Settings has the switch, on by default, with the operator note" 'grep -q "name=\"pains\" value=\"1\" checked" $W/set.html && grep -q "operator can read" $W/set.html' "missing"
check "precondition: no problem rows yet" '[ "$(rows)" = 0 ]' "$(rows)"

# Debate phrases are not triggers. Limited rooms are read for pal only.
post $PAL /feed -F "text=Someone should fix the buses. Why can't they run on time?" -F "anybody=1" >/dev/null
check "debate phrases are not triggers" '[ "$(rows)" = 0 ]' "$(grep ^pain $W/store.lgp)"
post $PAL /feed -F "text=I wish the buses ran later (friends only)" >/dev/null
check "a friends-only post is noticed for pal" 'grep "^pain	" $W/store.lgp | grep -q "fpost:.*the buses ran later"' "$(grep ^pain $W/store.lgp)"
post $PAL /mail -F "to=stranger" -F "text=I hate when the buses are late" >/dev/null
check "a message pal sent is noticed for pal" 'grep "^pain	" $W/store.lgp | grep -q "mail:.*the buses are late"' "$(grep ^pain $W/store.lgp)"
post $PAL /c/fres1 -F "text=I hate when the buses skip my stop" >/dev/null
check "a group post is noticed for pal" 'grep "^pain	" $W/store.lgp | grep -q "gpost:.*the buses skip my stop"' "$(grep ^pain $W/store.lgp)"

# Sources that ARE read.
post $PAL /feed -F "text=I really wish the buses ran later on weekends. Anyway!" -F "anybody=1" >/dev/null
check "a public post is noticed (as a suggestion, not kept)" 'grep -q "^pain	[0-9]*	pal	wish	0	[0-9]*	post:[0-9]*	the buses ran later on weekends$" $W/store.lgp' "$(grep ^pain $W/store.lgp)"
post $PAL /proposals -d "title=Shade" -d "body=I'm so tired of the heat at bus stops" -d "kind=0" >/dev/null
check "a proposal is noticed" 'grep -q "	pal	pain	0	[0-9]*	prop:[0-9]*	the heat at bus stops$" $W/store.lgp' "$(grep ^pain $W/store.lgp)"
pid=$(awk -F'\t' '$1=="prop" && $5=="Late-night buses on weekends"{print $2}' "$W/store.lgp")
post $PAL /proposals/reply -d "id=$pid" --data-urlencode "text=I hate when the parks close early" >/dev/null
check "a proposal comment is noticed" 'grep -q "	pal	pain	0	[0-9]*	prply:[0-9]*	the parks close early$" $W/store.lgp' "$(grep ^pain $W/store.lgp)"
check "only short phrases are stored, not the whole text" '! grep "^pain	" $W/store.lgp | grep -q "Anyway"' "full text stored"

# Crisis words: nothing stored, 988 shown once, nobody told.
before=$(rows)
post $STRANGER /feed -F "text=honestly I wish I wasn't here anymore" -F "anybody=1" >/dev/null
check "crisis words store nothing" '[ "$(rows)" = "$before" ]' "$(grep ^pain $W/store.lgp)"
get $STRANGER /feed care.html >/dev/null
check "the writer's next page shows 988" 'grep -q "tel:988" $W/care.html' "no 988"
get $STRANGER /feed care2.html >/dev/null
check "...only once" '! grep -q "tel:988" $W/care2.html' "shown again"
get $PAL /feed other.html >/dev/null
check "...and to nobody else" '! grep -q "tel:988" $W/other.html' "shown to pal"
check "nothing about it on disk" '! grep -q "^pain	[0-9]*	stranger	" $W/store.lgp && ! grep "^pain	" $W/store.lgp | grep -q "wasn"' "stored"

# The page: suggestions, Keep, Forget, owner only.
r=$(get $PAL /problems prob.html)
check "/problems lists the three suggestions" '[[ "$r" == 200* ]] && grep -q "the buses ran later on weekends" $W/prob.html && grep -q "the heat at bus stops" $W/prob.html && grep -q "the parks close early" $W/prob.html' "$r"
check "labelled Want and Problem" 'grep -q ">Want<" $W/prob.html && grep -q ">Problem<" $W/prob.html' "labels"
get $STRANGER /problems sprob.html >/dev/null
check "someone else sees none of pal's" '! grep -q "the buses ran later on weekends\|the parks close early" $W/sprob.html' "leaked"
bus=$(awk -F'\t' '$1=="pain" && $8=="the buses ran later on weekends"{print $2}' "$W/store.lgp")
heat=$(awk -F'\t' '$1=="pain" && $8=="the heat at bus stops"{print $2}' "$W/store.lgp")
parks=$(awk -F'\t' '$1=="pain" && $8=="the parks close early"{print $2}' "$W/store.lgp")
post $STRANGER /problems/keep -d "id=$bus" >/dev/null
check "someone else cannot Keep it" 'grep -q "^pain	$bus	pal	wish	0	" $W/store.lgp' "kept by stranger"
post $STRANGER /problems/forget -d "id=$bus" >/dev/null
check "someone else cannot Forget it" 'grep -q "^pain	$bus	" $W/store.lgp' "forgotten by stranger"
post $PAL /problems/keep -d "id=$bus" >/dev/null
post $PAL /problems/forget -d "id=$heat" >/dev/null
check "Keep marks it kept; Forget deletes it" 'grep -q "^pain	$bus	pal	wish	1	" $W/store.lgp && ! grep -q "^pain	$heat	" $W/store.lgp' "$(grep ^pain $W/store.lgp)"
get $PAL /problems prob2.html >/dev/null
check "a kept one shows possible solutions: the bus department, proposal and group" 'grep -q "Possible solutions" $W/prob2.html && grep -q "Fresno Area Express (buses)" $W/prob2.html && grep -q "Late-night buses on weekends" $W/prob2.html && grep -q "Fresno Bus Riders" $W/prob2.html' "missing"
check "unrelated things are not suggested" '! grep -q "Parks Department</a>" $W/prob2.html' "parks suggested for buses"
check "Start a proposal is prefilled, not posted" 'grep -q "/proposals?title=the%20buses%20ran%20later%20on%20weekends&amp;city=fresno-ca#new" $W/prob2.html' "no link"
get $PAL "/proposals?title=the%20buses%20ran%20later%20on%20weekends&city=fresno-ca" form.html >/dev/null
check "the proposal form opens with that title" 'grep -q "value=\"the buses ran later on weekends\"" $W/form.html' "not prefilled"

# The map: a private box on pal's own box, with its solutions.
node $PAL user:pal me.json >/dev/null
check "pal's own map box grows the kept problem" 'has me.json pain:$bus noticed' "missing"
check "...but not unkept suggestions" '! has me.json pain:$parks' "suggestion on map"
node $PAL pain:$bus pb.json >/dev/null
check "the problem box grows to its solutions" 'has pb.json "dept:fresno-ca|Fresno Area Express (buses)" could-act && has pb.json group:fres1 working-on' "missing"
r=$(node $STRANGER pain:$bus x.json)
check "nobody else can open pal's problem box" '[ "$r" = 404 ]' "$r"
node $STRANGER user:pal spal.json >/dev/null
check "nobody else sees it on pal's box" '! grep -q "pain:" $W/spal.json' "leaked"
r=$(get $PAL "/map?focus=pain:$bus" mp.html)
check "/map can open on the problem box" '[[ "$r" == 200* ]] && grep -q "data-focus=\"pain:$bus\"" $W/mp.html' "$r"

# Survives a restart; deleting the source removes it; the off switch deletes all.
stop; start
check "kept problem survives a restart" 'grep -q "^pain	$bus	pal	wish	1	" $W/store.lgp' "gone"
node $PAL user:pal me2.json >/dev/null
check "...and is still on pal's map" 'has me2.json pain:$bus' "missing"
src=$(awk -F'\t' '$1=="pain" && $2=="'"$bus"'"{print $7}' "$W/store.lgp"); src_id=${src#post:}
post $PAL "/post/$src_id/delete" -d "" >/dev/null
check "precondition: the source post was deleted" '! grep -q "^post	$src_id	" $W/store.lgp' "still there"
check "deleting the source post removes its problem" '! grep -q "^pain	$bus	" $W/store.lgp' "still there"
post $PAL /feed -F "text=I wish the library opened on Sundays" -F "anybody=1" >/dev/null
check "precondition: pal has problem rows before switching off" '[ "$(grep -c "	pal	" <(grep ^pain $W/store.lgp))" -ge 1 ]' "none"
post $PAL /settings/map -d show=1 -d groups=1 -d posts=1 -d comments=1 >/dev/null
check "switching it off deletes everything it found" '! grep "^pain	" $W/store.lgp | grep -q "	pal	"' "$(grep ^pain $W/store.lgp)"
post $PAL /feed -F "text=I wish the pool opened earlier" -F "anybody=1" >/dev/null
check "while off, nothing new is noticed" '! grep "^pain	" $W/store.lgp | grep -q "	pal	"' "noticed while off"
get $PAL /problems off.html >/dev/null
check "/problems says it is off" 'grep -q "turned off" $W/off.html' "no note"

check "server log has no crash lines" '! grep -qiE "abort|segfault|terminate called" $W/server.log' "$(tail -3 $W/server.log)"
stop
echo "passes=$passes fails=$fails"
echo "EXIT=$fails"
