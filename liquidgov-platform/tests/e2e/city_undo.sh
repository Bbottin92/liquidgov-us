#!/usr/bin/env bash
# Rollback check for the city features: deploy/city-undo.py must let the version
# BEFORE the city features open a store the new version wrote, keeping every
# non-city record (posts, mail, friends, groups, votes) and removing only city ones.
# Usage: bash city_undo.sh <new liquidgov_http> <liquidgov_e2e_seed> <old liquidgov_http> <city-undo.py>
# Throwaway store, port 18087. Never touches the live site.
set -u
NEW=$1; SEED=$2; OLD=$3; UNDO=$4
W=/tmp/lg-city-undo; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18087; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }

if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
for b in "$NEW" "$SEED" "$OLD"; do [ -x "$b" ] || { echo "missing binary $b"; exit 2; }; done
[ -f "$UNDO" ] || { echo "missing $UNDO"; exit 2; }
[ "$(readlink -f "$NEW")" != "$(readlink -f "$OLD")" ] || { echo "old and new are the same file"; exit 2; }

$SEED "$W/store.lgp" > "$W/sessions.txt" || { echo "seed failed"; exit 1; }
sid() { awk -v n=$1 '$1==n{print $2}' "$W/sessions.txt"; }
ADMIN=$(sid Bbottin); PAL=$(sid pal); STRANGER=$(sid stranger)
[ -n "$ADMIN" ] && [ -n "$PAL" ] && [ -n "$STRANGER" ] || { echo "seed gave no sessions"; exit 1; }

SERVER=""
start() { # binary -> 0 if it serves, 1 if it exits
    env -u LIQUIDGOV_OPERATORS LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT \
        "$1" >> "$W/server.log" 2>&1 &
    SERVER=$!
    for i in $(seq 50); do
        kill -0 $SERVER 2>/dev/null || { wait $SERVER 2>/dev/null; SERVER=""; return 1; }
        curl -s -o /dev/null --max-time 1 $URL/ && return 0
        sleep 0.2
    done
    kill $SERVER; SERVER=""; return 1
}
stop() { [ -n "$SERVER" ] && kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null; SERVER=""; }
trap stop EXIT
get()  { curl -s -o "$W/$3" -w '%{http_code} %{redirect_url}' -b "lg_session=$1" "$URL$2"; }
post() { local s=$1 p=$2; shift 2; curl -s -o /dev/null -w '%{http_code} %{redirect_url}' -b "lg_session=$s" "$@" "$URL$p"; }

# 1. New version: ordinary activity plus every kind of city record.
start "$NEW" || { echo "new server did not start"; cat "$W/server.log"; exit 1; }
post $PAL /feed -F "text=Ordinary feed post survives" >/dev/null
post $PAL /feed -F "text=I wish the buses ran later on weekends" -F "anybody=1" >/dev/null
post $PAL /mail -F "to=stranger" -F "text=Ordinary mail survives" >/dev/null
post $PAL /communities -d "slug=fres1" -d "name=Fresno Neighbors" -d "category=0" -d "subcategory=Neighbors" -d "about=x" --data-urlencode "city=Fresno, CA" >/dev/null
post $STRANGER /c/fres1/join -d "" >/dev/null
post $STRANGER /c/fres1 -F "text=Ordinary group post survives" >/dev/null
post $PAL /city/fresno-ca/list -d "on=1" >/dev/null
post $PAL /city/fresno-ca/tag --data-urlencode "tag=Parks" >/dev/null
post $PAL /proposals -d "title=Plain sitewide poll" -d "body=no city" -d "kind=0" >/dev/null
post $PAL /proposals -d "title=Plant trees" -d "body=More shade" -d "kind=0" --data-urlencode "city=Fresno, CA" --data-urlencode "who=Parks Department" -d "tag=Parks" >/dev/null
stop
plain_id=$(awk -F'\t' '$1=="prop" && $5=="Plain sitewide poll"{print $2}' "$W/store.lgp")
city_id=$(awk -F'\t' '$1=="prop" && $5=="Plant trees"{print $2}' "$W/store.lgp")
start "$NEW" || { echo "new server did not restart"; exit 1; }
post $STRANGER /proposals/vote -d "id=$plain_id" -d "choice=0" >/dev/null
post $STRANGER /proposals/reply -d "id=$city_id" --data-urlencode "text=A comment" >/dev/null
post $STRANGER /proposals/react -d "id=$city_id" -d "kind=support" >/dev/null
post $STRANGER /settings/map -d "show=1" -d "groups=1" -d "friends=1" >/dev/null
stop

# Preconditions: the store really holds both kinds of records.
for t in ccity clist pcity ctag ptag prply prpln prx mapp pain painn; do
    check "precondition: store has $t" 'grep -q "^$t	" $W/store.lgp' "none"
done
for t in post m comm cm pvote; do
    check "precondition: store has $t" 'grep -q "^$t	" $W/store.lgp' "none"
done
grep -vE '^(ccity|clist|pcity|pwho|ctag|ptag|prply|prpln|prx|mapp|pain|painn)	' "$W/store.lgp" > "$W/expected.lgp"
cp "$W/store.lgp" "$W/before.lgp"

# 2. The problem: the old version refuses this store.
if start "$OLD"; then bad "precondition: old version refuses the city store (it started)"; stop
else ok "precondition: old version refuses the city store"; fi
check "the old version says why" 'grep -q "unknown record type" $W/server.log' "$(tail -2 $W/server.log)"
check "the refused start left the store alone" 'cmp -s $W/store.lgp $W/before.lgp' "changed"

# 3. Dry run changes nothing.
python3 "$UNDO" "$W/store.lgp" > "$W/dry.txt" 2>&1; rc=$?
check "dry run exits 0" '[ $rc = 0 ]' "rc=$rc"
check "dry run counts the city records" 'grep -q "city records to remove: [1-9]" $W/dry.txt' "$(cat $W/dry.txt)"
check "dry run left the store alone" 'cmp -s $W/store.lgp $W/before.lgp' "changed"
check "dry run made no backup" '! ls $W/store.lgp.pre-city-undo-* >/dev/null 2>&1' "backup made"

# 4. Refusals.
printf 'NOTLGP1\nccity\ta\tb\n' > "$W/bad.lgp"
python3 "$UNDO" "$W/bad.lgp" --go > /dev/null 2>&1; rc=$?
check "a non-LGP1 file is refused" '[ $rc = 1 ] && grep -q "^NOTLGP1" $W/bad.lgp' "rc=$rc"
python3 "$UNDO" "$W/missing.lgp" --go > /dev/null 2>&1; rc=$?
check "a missing file is refused" '[ $rc = 1 ]' "rc=$rc"
python3 "$UNDO" > /dev/null 2>&1; rc=$?
check "no argument prints usage" '[ $rc = 2 ]' "rc=$rc"

# 5. The real run.
chmod 600 "$W/store.lgp"
python3 "$UNDO" "$W/store.lgp" --go > "$W/go.txt" 2>&1; rc=$?
check "undo exits 0" '[ $rc = 0 ]' "rc=$rc $(cat $W/go.txt)"
check "undo kept every other record byte for byte, in order" 'cmp -s $W/store.lgp $W/expected.lgp' "differs"
bk=$(ls "$W"/store.lgp.pre-city-undo-* 2>/dev/null | head -1)
check "the backup is the untouched store" '[ -n "$bk" ] && cmp -s "$bk" $W/before.lgp' "no backup"
check "the backup is private (0600)" '[ "$(stat -c %a "$bk")" = 600 ]' "$(stat -c %a "$bk")"
check "the store keeps its mode" '[ "$(stat -c %a $W/store.lgp)" = 600 ]' "$(stat -c %a $W/store.lgp)"
check "no temp file left" '[ ! -e $W/store.lgp.city-undo.tmp ]' "tmp left"
python3 "$UNDO" "$W/store.lgp" --go > "$W/again.txt" 2>&1; rc=$?
check "running it again removes nothing" '[ $rc = 0 ] && grep -q "nothing to remove" $W/again.txt && cmp -s $W/store.lgp $W/expected.lgp' "$(cat $W/again.txt)"

# 6. The old version opens it, with everything else intact.
if start "$OLD"; then ok "old version starts on the undone store"; else bad "old version starts on the undone store ($(tail -2 $W/server.log))"; fi
if [ -n "$SERVER" ]; then
    get $PAL /feed feed.html >/dev/null
    check "feed post kept" 'grep -q "Ordinary feed post survives" $W/feed.html' "missing"
    get $STRANGER "/mail?with=pal" mail.html >/dev/null
    check "mail kept" 'grep -q "Ordinary mail survives" $W/mail.html' "missing"
    get $STRANGER /c/fres1 group.html >/dev/null
    check "group and its post kept" 'grep -q "Fresno Neighbors" $W/group.html && grep -q "Ordinary group post survives" $W/group.html' "missing"
    check "group membership kept" 'grep -q "/u/stranger" $W/group.html' "missing"
    get $PAL /proposals props.html >/dev/null
    check "both proposals kept (the city one without its city)" 'grep -q "Plain sitewide poll" $W/props.html && grep -q "Plant trees" $W/props.html' "missing"
    check "the vote kept" 'grep -q "^pvote	$plain_id	stranger	0" $W/store.lgp' "missing"
    get $PAL /friends fr.html >/dev/null
    check "friends kept" 'grep -q "^f	" $W/store.lgp' "missing"
    stop
fi

# 7. Rolling forward again: restore people's map privacy choices from the undo's
#    backup BEFORE the reinstalled version starts, so nobody who hid reappears.
want_row=$(grep "^mapp	stranger	" "$W/before.lgp")
check "precondition: stranger had a privacy row (posts and comments hidden)" '[ "$want_row" = "mapp	stranger	1	1	0	0	1	0" ]' "$want_row"
check "precondition: the undone store has no privacy rows" '! grep -q "^mapp	" $W/store.lgp' "has some"
python3 "$UNDO" --restore-privacy "$bk" "$W/store.lgp" > "$W/rdry.txt" 2>&1; rc=$?
check "restore dry run changes nothing" '[ $rc = 0 ] && grep -q "1 to restore" $W/rdry.txt && ! grep -q "^mapp	" $W/store.lgp' "$(cat $W/rdry.txt)"
cp "$W/store.lgp" "$W/pre-restore.lgp"
python3 "$UNDO" --restore-privacy "$bk" "$W/store.lgp" --go > "$W/rgo.txt" 2>&1; rc=$?
check "restore puts stranger's row back exactly" '[ $rc = 0 ] && [ "$(grep "^mapp	stranger	" $W/store.lgp)" = "$want_row" ]' "$(cat $W/rgo.txt)"
check "restore changed nothing else" 'diff <(grep -v "^mapp	" $W/store.lgp) $W/pre-restore.lgp >/dev/null' "other lines changed"
check "restore made its own backup" 'ls $W/store.lgp.pre-privacy-restore-* >/dev/null 2>&1' "no backup"
python3 "$UNDO" --restore-privacy "$bk" "$W/store.lgp" --go > "$W/ragain.txt" 2>&1; rc=$?
check "restoring twice changes nothing" '[ $rc = 0 ] && grep -q "nothing to restore" $W/ragain.txt' "$(cat $W/ragain.txt)"
python3 "$UNDO" --restore-privacy "$W/bad.lgp" "$W/store.lgp" --go > /dev/null 2>&1; rc=$?
check "a non-LGP1 backup is refused" '[ $rc = 1 ]' "rc=$rc"
if start "$NEW"; then
    ok "new version starts on the undone, restored store"
    get $STRANGER /settings rs.html >/dev/null
    check "stranger's choices are back: posts and comments still hidden" '! grep -q "name=\"posts\" value=\"1\" checked" $W/rs.html && ! grep -q "name=\"comments\" value=\"1\" checked" $W/rs.html && grep -q "name=\"friends\" value=\"1\" checked" $W/rs.html' "reset to defaults"
    stop
else bad "new version starts on the undone, restored store"; fi
# The only crash allowed is step 2's deliberate refusal (old version, unknown city record).
check "the only crash is the expected refusal" '[ "$(grep -ciE "abort|segfault|terminate called" $W/server.log)" = 1 ] && grep -A1 "terminate called" $W/server.log | grep -q "unknown record type"' "$(grep -iE -A1 "abort|segfault|terminate called" $W/server.log)"
echo "passes=$passes fails=$fails"
echo "EXIT=$fails"
