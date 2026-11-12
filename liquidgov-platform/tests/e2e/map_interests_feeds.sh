#!/usr/bin/env bash
# Map SPEC phase 1 (Grok Bot's "everything expands from You", scratch/2026-11-11-grokbot-map-spec/SPEC.md sections 3.0,
# 3.3, 3.4, 3.7; Brandon 2026-11-11 "yes, build phase 1"): the You ring gains Interests, Feeds and Events.
#   Interests: one int:<key> box per comma item of your profile's interests, derived at read time (no store change).
#     An interest grows matching groups and proposals (whole words, never part of a word) and people who list the same
#     interest ONLY when they are on the map (show-me, not blocked), findable by name, and listed in one of your cities.
#   Feeds: feed:friends, feed:groups, feed:anybody-near, feed:mine, each its newest posts you may see (map_post_ok).
#     Drawings is never offered (Brandon). A feed with nothing in it is left off.
#   Events: nothing exists yet (M1.4), so the box is left off, like any empty kind.
# All of it is viewer-only: another person asking for your int:/feed: boxes gets nothing.
# Usage: bash map_interests_feeds.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Throwaway store, port 18103. Never touches the live site. New file: map_you.sh / map_you_look.sh are frozen.
set -u
BIN=$(readlink -f "$1"); SEED=$(readlink -f "$2")
HERE=$(cd "$(dirname "$0")/../.." && pwd)
W=/tmp/lg-map-interests-feeds; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18103; URL=http://127.0.0.1:$PORT
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
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{http_code}' -b "lg_session=$s" "$@" "$URL$p"; }
node() { curl -s -o "$W/$3" -w '%{http_code}' -b "lg_session=$1" -G --data-urlencode "n=$2" "$URL/map/node"; }
# Does the JSON in $1 list a link to node id $2?
has() { python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); sys.exit(0 if any(l["node"]["id"]==sys.argv[2] for l in d["links"]) else 1)' "$W/$1" "$2" 2>/dev/null; }
# every link id in $1, space separated
ids() { python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(" ".join(l["node"]["id"] for l in d["links"]))' "$W/$1" 2>/dev/null; }
field() { python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["node"].get(sys.argv[2],""))' "$W/$1" "$2" 2>/dev/null; }
sub_of() { python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(next((l["node"].get("sub","") for l in d["links"] if l["node"]["id"]==sys.argv[2]), ""))' "$W/$1" "$2" 2>/dev/null; }
label_of() { python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print(next((l["node"]["label"] for l in d["links"] if l["node"]["id"]==sys.argv[2]), ""))' "$W/$1" "$2" 2>/dev/null; }
profile() { post "$1" /me/profile --data-urlencode "display_name=$2" --data-urlencode "interests=$3" >/dev/null; }
pid_of() { awk -F'\t' -v t="$1" '$1=="post" && $5==t{print $2}' "$W/store.lgp" | head -1; }

start
# --- before anyone lists an interest or posts: no Interests, no Feeds, never Events
node $STRANGER you s0.json >/dev/null
check "precondition: stranger's you box answers" '[ "$(field s0.json kind)" = you ]' "$(head -c 200 $W/s0.json)"
check "no interests listed: Interests is left off the ring" '! has s0.json mine:interests' "$(ids s0.json)"
check "no posts anywhere: Feeds is left off the ring" '! has s0.json mine:feeds' "$(ids s0.json)"
check "Events is always shown, even with nothing yet (decision 1)" 'has s0.json mine:events' "$(ids s0.json)"

# --- the world: Bbottin and pal in Fresno; groups and proposals that do and don't match "parks"
post $PAL /communities -d "slug=fres1" -d "name=Fresno Neighbors" -d "category=0" -d "subcategory=Neighbors" -d "about=x" --data-urlencode "city=Fresno, CA" >/dev/null
post $ADMIN /c/fres1/join -d "" >/dev/null
post $ADMIN /city/fresno-ca/list -d "on=1" >/dev/null
post $PAL /city/fresno-ca/list -d "on=1" >/dev/null
post $STRANGER /communities -d "slug=parkcrew" -d "name=Parks Crew" -d "category=3" -d "subcategory=Outdoors" --data-urlencode "about=we clean up parks" >/dev/null
post $STRANGER /communities -d "slug=sparky" -d "name=Sparkshop" -d "category=3" -d "subcategory=Makers" --data-urlencode "about=sparks and solder" >/dev/null
post $PAL /proposals -d "title=More parks downtown" -d "body=shade" -d "kind=0" --data-urlencode "city=Fresno, CA" >/dev/null
post $PAL /proposals -d "title=Parking meters" -d "body=coins" -d "kind=0" --data-urlencode "city=Fresno, CA" >/dev/null
post $PAL /proposals -d "title=New park benches" -d "body=wood" -d "kind=0" --data-urlencode "city=Fresno, CA" >/dev/null
benchprop=$(awk -F'\t' '$1=="prop" && $5=="New park benches"{print $2}' "$W/store.lgp")
parksprop=$(awk -F'\t' '$1=="prop" && $5=="More parks downtown"{print $2}' "$W/store.lgp")
parkingprop=$(awk -F'\t' '$1=="prop" && $5=="Parking meters"{print $2}' "$W/store.lgp")
profile $ADMIN "Brandon" "Parks, chess,  Public Transit "
profile $PAL "Pal" "parks"
profile $STRANGER "Stranger" "parks"
check "precondition: proposals stored, Bbottin and pal listed in Fresno" '[ -n "$parksprop" ] && [ -n "$parkingprop" ] && grep -qP "^clist\tfresno-ca\tBbottin" $W/store.lgp && grep -qP "^clist\tfresno-ca\tpal" $W/store.lgp' "parks=$parksprop parking=$parkingprop clist=$(grep -c ^clist $W/store.lgp)"
check "precondition: Bbottin's interests saved" 'grep "^prof" $W/store.lgp | grep -q "Transit"' "$(grep -c ^prof $W/store.lgp) prof rows"

# --- Interests
node $ADMIN you you.json >/dev/null
check "Interests is on your ring once you list some" 'has you.json mine:interests' "$(ids you.json)"
check "Interests' second line is your own list" '[ "$(sub_of you.json mine:interests)" = "Parks, chess, Public Transit" ]' "sub=[$(sub_of you.json mine:interests)]"
r=$(node $ADMIN mine:interests int.json)
check "Interests -> one box per interest (parks, chess, public-transit)" '[ "$r" = 200 ] && has int.json int:parks && has int.json int:chess && has int.json int:public-transit' "$r $(ids int.json)"
check "an interest box is labelled as you typed it" '[ "$(label_of int.json int:public-transit)" = "Public Transit" ]' "label=[$(label_of int.json int:public-transit)]"
check "Interests leads back to you" 'has int.json you' "no way back"
r=$(node $ADMIN int:parks parks.json)
check "int:parks answers" '[ "$r" = 200 ]' "$r"
check "parks -> the group about parks (Parks Crew)" 'has parks.json group:parkcrew' "$(ids parks.json)"
check "parks does NOT match inside a word (Sparkshop, 'sparks')" '! has parks.json group:sparky' "$(ids parks.json)"
check "parks -> the proposal 'More parks downtown'" 'has parks.json prop:$parksprop' "$(ids parks.json)"
check "parks matches the singular 'park' (New park benches): a plural is the same word" '[ -n "$benchprop" ] && has parks.json prop:$benchprop' "bench=$benchprop $(ids parks.json)"
check "parks does NOT match 'Parking meters'" '! has parks.json prop:$parkingprop' "$(ids parks.json)"
check "parks -> pal (lists parks, on the map, in your city Fresno)" 'has parks.json user:pal' "$(ids parks.json)"
check "parks leaves out stranger (lists parks but is in none of your cities)" '! has parks.json user:stranger' "$(ids parks.json)"
check "parks leads back to Interests" 'has parks.json mine:interests' "$(ids parks.json)"
check "no box under an interest carries a count line (the back-link to Interests carries YOUR list)" '[ -z "$(python3 -c "import json;d=json.load(open(\"$W/parks.json\"));print(\" \".join(l[\"node\"][\"id\"] for l in d[\"links\"] if l[\"node\"].get(\"sub\") and not l[\"node\"][\"id\"].startswith(\"mine:\")))")" ]' "$(head -c 300 $W/parks.json)"
# both ways: stranger lists Fresno (today the store lists only people in a group there, so join first), then hides
post $STRANGER /c/fres1/join -d "" >/dev/null; post $STRANGER /city/fresno-ca/list -d "on=1" >/dev/null
node $ADMIN int:parks parks2.json >/dev/null
check "after stranger lists Fresno, parks -> stranger" 'has parks2.json user:stranger' "$(ids parks2.json)"
post $STRANGER /settings/map -d "x=1" >/dev/null  # every map box left unchecked: show-me off
node $ADMIN int:parks parks3.json >/dev/null
check "after stranger turns 'show me on the map' off, parks leaves them out" '! has parks3.json user:stranger' "$(ids parks3.json) mapp=$(grep -P '^mapp\tstranger' $W/store.lgp)"
# viewer-only
r=$(node $STRANGER int:chess sc.json)
check "someone else asking for your int:chess gets nothing (it is not their interest)" '[ "$r" = 404 ]' "$r"
r=$(node $PAL mine:interests pint.json)
check "pal's Interests is pal's own (parks), never Bbottin's chess" '[ "$r" = 200 ] && has pint.json int:parks && ! has pint.json int:chess' "$r $(ids pint.json)"
r=$(curl -s -o /dev/null -w '%{http_code}' -G --data-urlencode "n=int:parks" "$URL/map/node")
check "signed out: no interest box" '[ "$r" = 401 ] || [ "$r" = 404 ]' "$r"

# --- Feeds
post $PAL /feed -F "text=Friends-only from pal" >/dev/null
post $PAL /c/fres1 -F "text=Group post from pal" >/dev/null
post $PAL /feed -F "text=Public hello from pal" -F "anybody=1" >/dev/null
post $ADMIN /feed -F "text=My own post" >/dev/null
fpost=$(pid_of "Friends-only from pal"); gpost=$(pid_of "Group post from pal"); apost=$(pid_of "Public hello from pal"); mpost=$(pid_of "My own post")
check "precondition: four posts stored" '[ -n "$fpost" ] && [ -n "$gpost" ] && [ -n "$apost" ] && [ -n "$mpost" ]' "f=$fpost g=$gpost a=$apost m=$mpost"
node $ADMIN you you2.json >/dev/null
check "Feeds is on your ring once there are posts for you" 'has you2.json mine:feeds' "$(ids you2.json)"
check "Feeds' second line never offers Drawings" '[ -n "$(sub_of you2.json mine:feeds)" ] && ! sub_of you2.json mine:feeds | grep -qi drawing' "sub=[$(sub_of you2.json mine:feeds)]"
check "Events is still shown (decision 1)" 'has you2.json mine:events' "$(ids you2.json)"
r=$(node $ADMIN mine:feeds feeds.json)
check "Feeds -> Friends, Groups, Near you and Yours" '[ "$r" = 200 ] && has feeds.json feed:friends && has feeds.json feed:groups && has feeds.json feed:anybody-near && has feeds.json feed:mine' "$r $(ids feeds.json)"
check "Feeds offers exactly those four (no Drawings)" '[ "$(ids feeds.json | tr " " "\n" | grep "^feed:" | sort | tr "\n" " ")" = "feed:anybody-near feed:friends feed:groups feed:mine " ]' "$(ids feeds.json)"
node $ADMIN feed:friends ff.json >/dev/null
check "Friends feed -> pal's friends-only post" 'has ff.json post:$fpost' "$(ids ff.json)"
check "Friends feed leaves out your own post" '! has ff.json post:$mpost' "$(ids ff.json)"
node $ADMIN feed:groups fg.json >/dev/null
check "Groups feed -> the post in your group" 'has fg.json post:$gpost' "$(ids fg.json)"
node $ADMIN feed:anybody-near fa.json >/dev/null
check "Near you -> a public post by someone listed in your city" 'has fa.json post:$apost' "$(ids fa.json)"
check "Near you leaves out friends-only posts" '! has fa.json post:$fpost' "$(ids fa.json)"
node $ADMIN feed:mine fm.json >/dev/null
check "Yours -> your own post, nobody else's" 'has fm.json post:$mpost && ! has fm.json post:$fpost' "$(ids fm.json)"
check "a feed leads back to Feeds" 'has fm.json mine:feeds' "$(ids fm.json)"
r=$(node $ADMIN feed:drawings fd.json)
check "there is no Drawings feed" '[ "$r" = 404 ]' "$r"
# viewer-only: stranger is not Bbottin's friend, so stranger's Friends feed never shows Bbottin's friends-only post
node $STRANGER feed:friends sff.json >/dev/null
check "stranger's Friends feed never shows a post stranger may not see (Bbottin's friends-only)" '! has sff.json post:$mpost' "$(ids sff.json)"

echo "passes=$passes fails=$fails"
echo "EXIT=$([ $fails -eq 0 ] && echo 0 || echo 1)"
[ $fails -eq 0 ]
