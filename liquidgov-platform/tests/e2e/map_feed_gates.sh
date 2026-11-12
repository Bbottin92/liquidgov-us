#!/usr/bin/env bash
# Map phase 1 test gaps from Grok Bot's check of wip/32 (scratch/2026-11-11-grokbot-map-phase1-check/CHECK.md,
# should-fix 3) that map_interests_feeds.sh and map_interest_words.sh leave open. Tests only; no feature.
#   Feeds, every "is absent" with a positive control first (L189), so a feed that is simply empty can't pass:
#     - stranger's Friends feed DOES show stranger's friend pal's friends-only post, and never Bbottin's;
#     - "Show my posts" off takes pal's posts out of Bbottin's Friends, Groups and Near-you feeds (pal stays a person);
#     - posts_visibility "only me" takes pal's friends-only post out of Bbottin's Friends feed;
#     - blocks, both ways: Bbottin blocks stranger -> stranger's posts leave Bbottin's Groups and Near-you feeds;
#       pal blocks Bbottin -> pal's posts leave all three of Bbottin's feeds.
#   People under an interest, the gates other than show-me-off (show-me, findability and blocks are covered already):
#     profile visible to Friends -> gone for a non-friend, still there for a friend; Only me -> gone for both;
#     Members -> back. (/settings/privacy resets findability when the field is missing, so every call here sends
#     findability=2: a person must drop out for the visibility, not for findability.)
# Usage: bash map_feed_gates.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18105. Never touches the live site. New file: the two map suites above are committed.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-map-feed-gates; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18105; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "missing binaries"; exit 2; }
$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
# seed: Bbottin<->pal and pal<->stranger are friends; Bbottin and stranger are not
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
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{http_code}' -b "lg_session=$s" "$@" "$URL$p"; }
node() { curl -s -o "$W/$3" -w '%{http_code}' -b "lg_session=$1" -G --data-urlencode "n=$2" "$URL/map/node"; }
has() { python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); sys.exit(0 if any(l["node"]["id"]==sys.argv[2] for l in d["links"]) else 1)' "$W/$1" "$2" 2>/dev/null; }
ids() { python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(" ".join(l["node"]["id"] for l in d["links"]))' "$W/$1" 2>/dev/null; }
profile() { post "$1" /me/profile --data-urlencode "display_name=$2" --data-urlencode "interests=$3" >/dev/null; }
pid_of() { awk -F'\t' -v t="$1" '$1=="post" && $5==t{print $2}' "$W/store.lgp" | head -1; }
# privacy <session> <profile_visibility 0-3> <posts_visibility 0-3>; findability always 2 (Typeahead), see above
privacy() { post "$1" /settings/privacy -d "findability=2" -d "profile_visibility=$2" -d "posts_visibility=$3" >/dev/null; }
mapprefs_all_but() { local s=$1 off=$2 a=(); for k in show groups posts comments friends pains; do [ "$k" = "$off" ] || a+=(-d "$k=1"); done; post "$s" /settings/map "${a[@]}" >/dev/null; }
feeds() { node $ADMIN feed:friends "$1-ff.json" >/dev/null; node $ADMIN feed:groups "$1-fg.json" >/dev/null; node $ADMIN feed:anybody-near "$1-fa.json" >/dev/null; }

start
# --- the world: one Fresno group with all three in it, all three listed in Fresno, all three like jam
post $PAL /communities -d "slug=fres1" -d "name=Fresno Neighbors" -d "category=0" -d "subcategory=Neighbors" -d "about=x" --data-urlencode "city=Fresno, CA" >/dev/null
for s in $ADMIN $STRANGER; do post $s /c/fres1/join -d "" >/dev/null; done
for s in $ADMIN $PAL $STRANGER; do post $s /city/fresno-ca/list -d "on=1" >/dev/null; done
profile $ADMIN "Brandon" "jam"; profile $PAL "Pal" "jam"; profile $STRANGER "Stranger" "jam"
post $PAL /feed -F "text=Friends-only from pal" >/dev/null
post $PAL /c/fres1 -F "text=Group post from pal" >/dev/null
post $PAL /feed -F "text=Public hello from pal" -F "anybody=1" >/dev/null
post $STRANGER /c/fres1 -F "text=Group post from stranger" >/dev/null
post $STRANGER /feed -F "text=Public hello from stranger" -F "anybody=1" >/dev/null
post $ADMIN /feed -F "text=Friends-only from Bbottin" >/dev/null
pf=$(pid_of "Friends-only from pal"); pg=$(pid_of "Group post from pal"); pa=$(pid_of "Public hello from pal")
sg=$(pid_of "Group post from stranger"); sa=$(pid_of "Public hello from stranger"); bf=$(pid_of "Friends-only from Bbottin")
check "precondition: six posts stored" '[ -n "$pf" ] && [ -n "$pg" ] && [ -n "$pa" ] && [ -n "$sg" ] && [ -n "$sa" ] && [ -n "$bf" ]' "pf=$pf pg=$pg pa=$pa sg=$sg sa=$sa bf=$bf"
check "precondition: all three listed in Fresno" '[ "$(grep -cP "^clist\tfresno-ca\t" $W/store.lgp)" = 3 ]' "clist=$(grep -c ^clist $W/store.lgp)"

# --- positive controls: every feed below has something in it before anything is taken away
feeds a
check "control: Bbottin's Friends feed -> pal's friends-only post" 'has a-ff.json post:$pf' "$(ids a-ff.json)"
check "control: Bbottin's Groups feed -> pal's and stranger's group posts" 'has a-fg.json post:$pg && has a-fg.json post:$sg' "$(ids a-fg.json)"
check "control: Bbottin's Near-you feed -> pal's and stranger's public posts" 'has a-fa.json post:$pa && has a-fa.json post:$sa' "$(ids a-fa.json)"

# --- stranger's Friends feed: not empty, and still never Bbottin's friends-only post
node $STRANGER feed:friends s-ff.json >/dev/null
check "stranger's Friends feed shows stranger's friend pal's friends-only post (it is not just empty)" 'has s-ff.json post:$pf' "$(ids s-ff.json)"
check "...and never Bbottin's friends-only post (stranger is not Bbottin's friend)" '! has s-ff.json post:$bf' "$(ids s-ff.json)"

# --- "Show my posts" off
mapprefs_all_but $PAL posts
feeds b
check "pal's 'Show my posts' off: pal's posts leave Bbottin's Friends, Groups and Near-you feeds" '! has b-ff.json post:$pf && ! has b-fg.json post:$pg && ! has b-fa.json post:$pa' "ff=$(ids b-ff.json) fg=$(ids b-fg.json) fa=$(ids b-fa.json)"
check "...stranger's posts stay (control)" 'has b-fg.json post:$sg && has b-fa.json post:$sa' "fg=$(ids b-fg.json) fa=$(ids b-fa.json)"
r=$(node $ADMIN user:pal b-pal.json)
check "...pal is still on the map as a person" '[ "$r" = 200 ]' "$r"
mapprefs_all_but $PAL none
feeds c
check "pal's posts back on: back in all three feeds" 'has c-ff.json post:$pf && has c-fg.json post:$pg && has c-fa.json post:$pa' "ff=$(ids c-ff.json) fg=$(ids c-fg.json) fa=$(ids c-fa.json)"

# --- posts_visibility only me
privacy $PAL 0 3
node $ADMIN feed:friends d-ff.json >/dev/null
check "pal's posts visible to 'only me': the friends-only post leaves Bbottin's Friends feed" '! has d-ff.json post:$pf' "$(ids d-ff.json)"
privacy $PAL 0 0
node $ADMIN feed:friends e-ff.json >/dev/null
check "pal's posts visible again: back in the Friends feed" 'has e-ff.json post:$pf' "$(ids e-ff.json)"

# --- people under an interest: profile visibility
node $ADMIN int:jam j1.json >/dev/null; node $PAL int:jam pj1.json >/dev/null
check "Bbottin's jam shows neither pal nor stranger (interest people hidden until the signup privacy step)" '! has j1.json user:pal && ! has j1.json user:stranger' "$(ids j1.json)"
check "pal's jam does not show stranger (interest people hidden)" '! has pj1.json user:stranger' "$(ids pj1.json)"
privacy $STRANGER 2 0
node $ADMIN int:jam j2.json >/dev/null; node $PAL int:jam pj2.json >/dev/null
check "stranger's profile visible to Friends: gone from Bbottin's jam (not friends)" '! has j2.json user:stranger && ! has j2.json user:pal' "$(ids j2.json)"
check "...not under stranger's friend pal's jam either (interest people hidden)" '! has pj2.json user:stranger' "$(ids pj2.json)"
privacy $STRANGER 3 0
node $PAL int:jam pj3.json >/dev/null
check "stranger's profile 'only me': gone from pal's jam too" '! has pj3.json user:stranger' "$(ids pj3.json)"
privacy $STRANGER 1 0
node $ADMIN int:jam j4.json >/dev/null
check "stranger's profile visible to Members: still not under Bbottin's jam (interest people hidden)" '! has j4.json user:stranger' "$(ids j4.json)"
privacy $STRANGER 0 0

# --- blocks, both ways, on feeds
post $ADMIN /friends/block -d "who=stranger" >/dev/null
feeds f
check "Bbottin blocks stranger: stranger's posts leave Bbottin's Groups and Near-you feeds" '! has f-fg.json post:$sg && ! has f-fa.json post:$sa' "fg=$(ids f-fg.json) fa=$(ids f-fa.json)"
check "...pal's posts stay (control)" 'has f-ff.json post:$pf && has f-fg.json post:$pg && has f-fa.json post:$pa' "ff=$(ids f-ff.json) fg=$(ids f-fg.json) fa=$(ids f-fa.json)"
post $PAL /friends/block -d "who=Bbottin" >/dev/null
feeds g
check "pal blocks Bbottin: pal's posts leave all three of Bbottin's feeds" '! has g-ff.json post:$pf && ! has g-fg.json post:$pg && ! has g-fa.json post:$pa' "ff=$(ids g-ff.json) fg=$(ids g-fg.json) fa=$(ids g-fa.json)"

echo "passes=$passes fails=$fails"
echo "EXIT=$([ $fails -eq 0 ] && echo 0 || echo 1)"
[ $fails -eq 0 ]
