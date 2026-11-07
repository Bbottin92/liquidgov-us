#!/usr/bin/env bash
# Flow a kept problem box (local only). A kept box stays on the owner's map
# until they Flow it. Other people then see that subject, and only when they
# may see the owner. Still puts it back. Someone who has not Flowed
# the same subject gets no match list and no count.
# Usage: bash city_flow.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18093. Never touches the live site or port 8080.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-city-flow; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18093; URL=http://127.0.0.1:$PORT
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
page_of() { python3 - "$W/$1" "$2" <<'EOF'
import json, sys
d = json.load(open(sys.argv[1]))
want = sys.argv[2]
found = d["node"]["page"] if d["node"]["id"] == want else ""
if not found:
    for l in d["links"]:
        if l["node"]["id"] == want:
            found = l["node"]["page"]
print(found)
EOF
}
# card <html> <id> <needle> <yes|no> — needle inside that one kept card.
card() { python3 - "$W/$1" "$2" "$3" "$4" <<'EOF'
import pathlib, re, sys
t = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
m = re.search(r'id="pain' + re.escape(sys.argv[2]) + r'"[\s\S]*?</div>', t)
hit = bool(m and sys.argv[3] in m.group(0))
sys.exit(0 if hit == (sys.argv[4] == "yes") else 1)
EOF
}
noticed_clean() { python3 - "$W/$1" <<'EOF'
import pathlib, sys
t = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
n, sep, _ = t.partition("<h2>Kept</h2>")
sys.exit(0 if sep and "/problems/flow" not in n and "the parks close early" in n else 1)
EOF
}
pub_phrase() { python3 - "$W/$1" <<'EOF'
import json, sys
d = json.load(open(sys.argv[1], encoding="utf-8"))
ok = d["node"]["label"] == "the buses ran later on weekends"
ok = ok and all("/problems" not in l["node"]["page"] for l in d["links"])
sys.exit(0 if ok else 1)
EOF
}

start
post $PAL /communities -d "slug=fres1" -d "name=Fresno Bus Riders" -d "category=0" -d "subcategory=Transit" -d "about=riding the buses" --data-urlencode "city=Fresno, CA" >/dev/null
post $PAL /city/fresno-ca/tag --data-urlencode "tag=Fresno Area Express (buses)" >/dev/null
post $PAL /feed -F "text=I really wish the buses ran later on weekends. Anyway!" -F "anybody=1" >/dev/null
post $PAL /feed -F "text=I wish the library opened on Sundays" -F "anybody=1" >/dev/null
post $PAL /proposals -d "title=Shade" -d "body=I hate when the parks close early" -d "kind=0" >/dev/null

bus=$(awk -F'	' '$1=="pain" && $8=="the buses ran later on weekends"{print $2}' "$W/store.lgp")
lib=$(awk -F'	' '$1=="pain" && $8=="the library opened on Sundays"{print $2}' "$W/store.lgp")
parks=$(awk -F'	' '$1=="pain" && $8=="the parks close early"{print $2}' "$W/store.lgp")
check "precondition: three suggestions" '[ -n "$bus" ] && [ -n "$lib" ] && [ -n "$parks" ]' "bus=$bus lib=$lib parks=$parks"

get $PAL /problems before.html >/dev/null
check "a suggestion has no Flow button" '! grep -q "action=\"/problems/flow\"" $W/before.html' "flow on a suggestion"
post $STRANGER /problems/flow -d "id=$bus" >/dev/null
post $PAL /problems/flow -d "id=$parks" >/dev/null
check "Flow does nothing until it is kept" 'grep -q "^pain	$bus	pal	wish	0	[0-9]*	post:[0-9]*	the buses ran later on weekends$" $W/store.lgp && grep -q "^pain	$parks	pal	pain	0	" $W/store.lgp' "$(grep ^pain "$W/store.lgp")"

post $PAL /problems/keep -d "id=$bus" >/dev/null
post $PAL /problems/keep -d "id=$lib" >/dev/null
r=$(node $STRANGER pain:$bus x.json)
check "a kept box is still hidden" '[ "$r" = 404 ]' "$r"
node $STRANGER user:pal spal.json >/dev/null
check "and it is not on their box for anyone else" '! grep -q "pain:" $W/spal.json' "leaked"
node $PAL user:pal me.json >/dev/null
check "the owner sees both kept boxes" 'has me.json pain:$bus noticed && has me.json pain:$lib noticed' "missing"
node $PAL pain:$bus own.json >/dev/null
check "the owner still sees who could act" 'has own.json "dept:fresno-ca|Fresno Area Express (buses)" could-act' "missing"
check "the owner's box still opens their own list" '[ "$(page_of own.json pain:$bus)" = "/problems#pain$bus" ]' "$(page_of own.json pain:$bus)"

get $PAL /problems kept.html >/dev/null
check "kept cards offer Flow" 'grep -q "action=\"/problems/flow\"" $W/kept.html && grep -q "id=\"pain$bus\"" $W/kept.html' "no button"
check "the unkept card still has no Flow button" 'noticed_clean kept.html' "flow on a suggestion"
post $STRANGER /problems/flow -d "id=$bus" >/dev/null
check "someone else cannot Flow it" 'grep -q "^pain	$bus	pal	wish	1	[0-9]*	post:[0-9]*	the buses ran later on weekends$" $W/store.lgp' "$(grep ^pain "$W/store.lgp")"

post $PAL /problems/flow -d "id=$bus" >/dev/null
check "Flow writes the 9th field on that row only" 'grep -q "^pain	$bus	pal	wish	1	[0-9]*	post:[0-9]*	the buses ran later on weekends	1$" $W/store.lgp && grep -q "^pain	$lib	pal	wish	1	[0-9]*	post:[0-9]*	the library opened on Sundays$" $W/store.lgp' "$(grep ^pain "$W/store.lgp")"
get $PAL /problems flowed.html >/dev/null
check "the flowed card offers Still" 'card flowed.html $bus /problems/still yes && card flowed.html $bus "action=\"/problems/flow\"" no' "no still"
check "the other kept card is still Flow" 'card flowed.html $lib "action=\"/problems/flow\"" yes' "library flowed too"

r=$(node $STRANGER pain:$bus pub.json)
check "someone else can open the flowed subject" '[ "$r" = 200 ]' "$r"
check "they see the phrase, and the page is not the private list" '[ "$(page_of pub.json pain:$bus)" = "/pain/$bus" ] && pub_phrase pub.json' "$(page_of pub.json pain:$bus)"
check "they see who flowed it, not a department or another box" 'has pub.json user:pal noticed && ! has pub.json "dept:fresno-ca|Fresno Area Express (buses)" && ! has pub.json pain:$lib' "wrong links"
node $STRANGER user:pal spal2.json >/dev/null
check "their box shows the flowed subject only" 'has spal2.json pain:$bus noticed && ! has spal2.json pain:$lib' "library leaked"
r=$(node $STRANGER pain:$lib x.json)
check "the box they left Still is refused" '[ "$r" = 404 ]' "$r"
r=$(get $STRANGER /pain/$bus sub.html)
check "the subject page shows the phrase and who flowed it" '[[ "$r" == 200* ]] && grep -q "the buses ran later on weekends" $W/sub.html && grep -q "Flowed by" $W/sub.html && grep -q "href=\"/u/pal\"" $W/sub.html' "$r"
check "someone who has not flowed it sees no one else" '! grep -q "Also flowed" $W/sub.html' "match list"
check "that page is not the private list" '! grep -q "the library opened on Sundays\|the parks close early\|Only you can see this page\|Possible solutions" $W/sub.html' "list leaked"
r=$(get $PAL /pain/$bus owner.html)
check "the owner is sent back to their list" '[[ "$r" == 303*"/problems#pain$bus"* ]]' "$r"
r=$(curl -s -o /dev/null -w '%{http_code} %{redirect_url}' "$URL/pain/$bus")
check "signed out is sent to login" '[[ "$r" == 303*"/login?next=%2Fpain%2F$bus"* ]]' "$r"
r=$(get $STRANGER /pain/999999 miss.html)
check "a missing subject is not found" '[[ "$r" == 404* ]] && ! grep -q "the buses ran later" $W/miss.html' "$r"
r=$(get $STRANGER /pain/$lib hidden.html)
check "a Still subject looks the same as a missing one" '[[ "$r" == 404* ]] && grep -q "No such subject" $W/hidden.html && ! grep -q "the library opened" $W/hidden.html' "$r"
get $STRANGER /problems sprob.html >/dev/null
check "their problems page is still only theirs" '! grep -q "the buses ran later on weekends" $W/sprob.html' "leaked"

# Hidden and blocked people do not leak a flowed box. pains stays on, or the rows would be deleted.
post $PAL /settings/map -d groups=1 -d posts=1 -d comments=1 -d pains=1 >/dev/null
r=$(node $STRANGER pain:$bus x.json)
check "show off: the subject is refused" '[ "$r" = 404 ] && grep -q "^pain	$bus	pal	wish	1	.*	1$" $W/store.lgp' "$r"
r=$(get $STRANGER /pain/$bus hide.html)
check "show off: the subject page is not found" '[[ "$r" == 404* ]]' "$r"
node $PAL user:pal me2.json >/dev/null
check "show off: the owner still sees it" 'has me2.json pain:$bus noticed' "missing"
post $PAL /settings/map -d show=1 -d groups=1 -d posts=1 -d comments=1 -d pains=1 >/dev/null
r=$(node $STRANGER pain:$bus x.json)
check "show on again: the subject is back" '[ "$r" = 200 ]' "$r"

post $PAL /settings/privacy -d findability=1 -d profile_visibility=3 -d posts_visibility=0 >/dev/null
r=$(get $STRANGER /pain/$bus onlyme.html)
check "profile only-me: the subject is not found" '[[ "$r" == 404* ]]' "$r"
r=$(node $STRANGER pain:$bus x.json)
check "profile only-me: the map box is refused" '[ "$r" = 404 ]' "$r"
post $PAL /settings/privacy -d findability=1 -d profile_visibility=0 -d posts_visibility=0 >/dev/null
r=$(node $STRANGER pain:$bus x.json)
check "profile public again: the subject is back" '[ "$r" = 200 ]' "$r"

post $PAL /friends/block -d "who=stranger" >/dev/null
r=$(node $STRANGER pain:$bus x.json)
check "blocked by the owner: the map box is refused" '[ "$r" = 404 ]' "$r"
r=$(get $STRANGER /pain/$bus blk.html)
check "blocked by the owner: the subject page is not found" '[[ "$r" == 404* ]]' "$r"
post $PAL /settings/unblock -d "who=stranger" >/dev/null
r=$(node $STRANGER pain:$bus x.json)
check "after unblock the subject is back" '[ "$r" = 200 ]' "$r"
post $STRANGER /friends/block -d "who=pal" >/dev/null
r=$(get $STRANGER /pain/$bus blk2.html)
check "blocking them hides the subject too" '[[ "$r" == 404* ]]' "$r"
post $STRANGER /settings/unblock -d "who=pal" >/dev/null
r=$(node $STRANGER pain:$bus x.json)
check "after they unblock, the subject is back" '[ "$r" = 200 ]' "$r"

post $PAL /problems/still -d "id=$bus" >/dev/null
check "Still drops the 9th field and keeps the box" 'grep -q "^pain	$bus	pal	wish	1	[0-9]*	post:[0-9]*	the buses ran later on weekends$" $W/store.lgp' "$(grep ^pain "$W/store.lgp")"
r=$(node $STRANGER pain:$bus x.json)
check "after Still, other people are refused again" '[ "$r" = 404 ]' "$r"
node $PAL user:pal me3.json >/dev/null
check "after Still, the owner still has it" 'has me3.json pain:$bus noticed' "missing"
node $STRANGER user:pal spal3.json >/dev/null
check "after Still, it is gone from their box" '! grep -q "pain:" $W/spal3.json' "still there"

post $PAL /problems/flow -d "id=$bus" >/dev/null
stop; start
check "a flowed row survives a restart" 'grep -q "^pain	$bus	pal	wish	1	.*	the buses ran later on weekends	1$" $W/store.lgp' "$(grep ^pain "$W/store.lgp")"
r=$(node $STRANGER pain:$bus x.json)
check "and other people still see it" '[ "$r" = 200 ]' "$r"
post $PAL /problems/forget -d "id=$bus" >/dev/null
check "Forget removes a flowed row" '! grep -q "^pain	$bus	" $W/store.lgp' "still there"
r=$(node $STRANGER pain:$bus x.json)
check "and the subject is gone" '[ "$r" = 404 ]' "$r"

check "server log has no crash lines" '! grep -qiE "abort|segfault|terminate called" $W/server.log' "$(tail -3 $W/server.log)"
stop
echo "passes=$passes fails=$fails"
echo "EXIT=$fails"
