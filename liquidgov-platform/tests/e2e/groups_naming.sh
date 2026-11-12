#!/usr/bin/env bash
# The user-facing name is "Groups" everywhere (Brandon 2026-11-11 4:30 PM decision 3): no "Communities", "Your rooms"
# or "Committees" in visible copy (nav, headings, buttons, the feed tab, stats). Routes and code names are unchanged:
# /communities and ?filter=committees still work.
#   - /feed: the third tab reads Groups and still links filter=committees; no Communities tab
#   - /home: the dashboard's list of your groups is headed "Your groups" (not "Your rooms"); the stats say Groups
#   - signed out /: the stats tile says Groups (not Communities)
#   - /u/<name> of a member: the profile's group list is headed Groups (not Committees)
#   - /settings: the notification is "Group invites"; /listen: the Juketube search says "people and groups"
#   - /communities still answers 200
# Usage: bash groups_naming.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18113. Never touches the live site. New file: committed suites are frozen (feed_no_drawings.sh
# still expects the old "Communities" tab; see REPORT4).
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-groups-naming; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18113; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
ADMIN=$(sid Bbottin); PAL=$(sid pal)
[ -n "$ADMIN" ] && [ -n "$PAL" ] || { echo "seed gave no sessions"; exit 1; }

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
tabs() { python3 -c 'import re,sys; m=re.search(r"<div class=\"segtabs\">(.*?)</div>", open(sys.argv[1]).read(), re.S); print(m.group(1) if m else "")' "$W/$1"; }

start
post $ADMIN /communities -d "slug=nm1" -d "name=Naming Neighbors" -d "category=0" -d "subcategory=Neighbors" -d "about=x" --data-urlencode "city=Fresno, CA" >/dev/null

get $ADMIN /feed f.html >/dev/null
check "/feed: the third tab reads Groups and still links filter=committees" 'tabs f.html | grep -q "filter=committees\">Groups<"' "$(tabs f.html)"
check "/feed: no Communities tab" '! tabs f.html | grep -q "Communities"' "$(tabs f.html)"

r=$(get $ADMIN /home h.html)
check "/home: your groups are headed Your groups" '[ "$r" = 200 ] && grep -q "<h2>Your groups</h2>" $W/h.html' "$r"
check "/home: no Your rooms, no Communities" '! grep -q "Your rooms" $W/h.html && ! grep -q ">Communities<\|>Community<" $W/h.html' "$(grep -o "Your rooms\|>Communit[a-z]*<" $W/h.html | head -3)"

curl -s -o "$W/g.html" "$URL/"
check "signed out /: the stats tile says Groups, not Communities" '! grep -q ">Communities<\|>Community<" $W/g.html && grep -q "<span>Groups</span>\|<span>Group</span>" $W/g.html' "$(grep -o '<span>[A-Z][a-z]*</span>' $W/g.html | head -6 | tr '\n' ' ')"

get $PAL /u/Bbottin u.html >/dev/null
check "/u/Bbottin: the group list is headed Groups, not Committees" '! grep -q ">Committees<" $W/u.html && grep -q "<p class=\"eyebrow\"[^>]*>Groups</p>" $W/u.html' "$(grep -o '<p class="eyebrow"[^>]*>[^<]*</p>' $W/u.html | head -4 | tr '\n' ' ')"

get $ADMIN /settings s.html >/dev/null
check "/settings: the notification says Group invites" 'grep -q "Group invites" $W/s.html && ! grep -q "Community invites" $W/s.html' "missing"
get $ADMIN /listen l.html >/dev/null; check "the Juketube search says people and groups" 'grep -q "Search people and groups" $W/l.html && ! grep -q "Search people and communities" $W/l.html' "$(grep -o 'placeholder="Search people[^"]*"' $W/l.html | head -1)"

r=$(get $ADMIN /communities c.html)
check "/communities still answers 200" '[ "$r" = 200 ]' "$r"

echo "passes=$passes fails=$fails"
[ $fails -eq 0 ]
