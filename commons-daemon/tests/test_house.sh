#!/usr/bin/env bash
# A bound player's own house: durable wall slot and last position (across a real restart),
# owner-only placement, bound inventory, RAM-only demo house, and the checked-SQLite behaviour
# (a failing statement is a 500 for that request, rolls back, and never kills the daemon).
source "$(dirname "$0")/lib.sh"
start_civic; start_daemon
for u in alice bob; do su $u; done
H=$(house alice)

echo "-- first entry, a wall item, a position"
bjoin alice $H 7 14
assert_contains "join ok" "$BODY" '"ok":true'
assert_contains "the owner's join echoes the policy" "$BODY" '"house_policy":"friends"'
assert_not_contains "first ever entry has no resume position" "$BODY" 'resume_x'
bpickup alice chalk_block;        assert_code "pickup ok" 200
bpickup alice iron_gall_ink_page; assert_code "second pickup ok" 200
bpickup alice fence_post; assert_code "pickup of a non-lab catalog item is refused (whitelist, not a free mint)" 400
assert_contains "with the standard unknown_item reason" "$BODY" 'unknown_item'
assert_eq "no inventory row was created for it" "2" "$(dbq "select count(*) from inventory where player_id=(select player_id from players where lg_username='alice')")"
bplace alice $H chalk_block;      assert_contains "the owner places chalk" "$BODY" '"ok":true'
get "$D/placed/$H";               assert_contains "the wall shows chalk" "$BODY" 'chalk_block'
bmove alice $H 3 9 2
bleave alice $H
assert_eq "leaving wrote the position onto the players row" "3" "$(dbq "select last_x from players where lg_username='alice'")"

echo "-- a real daemon restart"
stop_daemon; start_daemon
occupants $H;  assert_eq "presence is RAM: empty right after the restart" "[]" "$BODY"
get "$D/placed/$H"; assert_contains "the wall survived (SQLite)" "$BODY" 'chalk_block'
bjoin alice $H 7 14
assert_contains "re-entry resumes where she left" "$BODY" '"resume_x":3'
assert_contains "including the row's y" "$BODY" '"resume_y":9'
assert_contains "and facing" "$BODY" '"resume_facing":2'
get "$D/presence/inventory" "$(jar alice)"
assert_contains "bound inventory survived: chalk" "$BODY" 'chalk_block'
assert_contains "bound inventory survived: ink" "$BODY" 'iron_gall_ink_page'

echo "-- walking out is a rekey, and it snapshots too"
bmove alice $H 5 5 0
post "$D/presence/rekey" "{\"id\":\"g_x\",\"secret\":\"w\",\"from_region\":\"$H\",\"to_region\":\"1,1\",\"x\":2,\"y\":2,\"facing\":0}" "$(jar alice)"
assert_eq "the rekey away from the house snapshotted (5)" "5" "$(dbq "select last_x from players where lg_username='alice'")"
bleave alice 1,1

echo "-- only the owner can place; nobody else gets in"
bplace bob $H chalk_block;     assert_code "another bound player cannot place in alice's house" 403
bjoin bob $H;                  assert_code "another bound player cannot enter (no friendship)" 403
post "$D/presence/join" "{\"id\":\"g_x\",\"secret\":\"w\",\"region\":\"$H\",\"x\":1,\"y\":1,\"facing\":0}"
assert_code "a guest cannot enter" 403
get "$D/placed/$H";            assert_contains "the wall is publicly readable" "$BODY" 'chalk_block'
get "$D/placed/instance:house:nobody-here"; assert_contains "GET of a house nobody entered does not create one" "$BODY" '"item":""'
assert_eq "...and made no players row" "0" "$(dbq "select count(*) from players where lg_username='nobody-here'")"

echo "-- guests: nothing persists, nothing is asked of civic"
mint; post "$D/presence/pickup" '{"item":"chalk_block"}'; assert_code "a guest pickup is refused (no guest persistence)" 403
get "$D/presence/inventory"; assert_eq "a guest's saved inventory is empty" '{"items":[]}' "$BODY"

echo "-- the shared demo house is RAM-only and needs a real guest credential"
post "$D/placed/instance:demo" '{"id":"g_bad","secret":"bad","item":"chalk_block"}'; assert_code "demo place without a valid guest credential" 403
mint; post "$D/placed/instance:demo" "{\"id\":\"$GID\",\"secret\":\"$GSEC\",\"item\":\"chalk_block\"}"
assert_contains "a real guest can place" "$BODY" '"ok":true'
get "$D/placed/instance:demo"; assert_contains "and it shows" "$BODY" 'chalk_block'
stop_daemon; start_daemon
get "$D/placed/instance:demo"; assert_eq "the demo wall does NOT survive a restart" '{"item":""}' "$BODY"

echo "-- a failing SQL statement is a 500 for that request, rolls back, and never kills the daemon"
python3 - <<'PY'
import sqlite3, os
c = sqlite3.connect(os.environ["DB_PATH"]); c.execute("CREATE TRIGGER t_fail BEFORE INSERT ON placed BEGIN SELECT RAISE(ABORT,'injected'); END;"); c.commit()
PY
bjoin alice $H >/dev/null
bplace alice $H iron_gall_ink_page
assert_code "the failed place is a 500" 500
assert_contains "with a generic, valid JSON body" "$BODY" '"reason":"server_error"'
assert_not_contains "that does not leak sqlite's message" "$BODY" 'injected'
get "$D/placed/$H"; assert_contains "the OLD wall item is still there (the DELETE was rolled back)" "$BODY" 'chalk_block'
get "$D/health";    assert_code "the daemon is alive" 200
python3 - <<'PY'
import sqlite3, os
c = sqlite3.connect(os.environ["DB_PATH"]); c.execute("DROP TRIGGER t_fail"); c.commit()
PY
bplace alice $H iron_gall_ink_page
assert_contains "trigger gone: the same place works (no dangling transaction)" "$BODY" '"ok":true'
python3 - <<'PY'
import sqlite3, os
c = sqlite3.connect(os.environ["DB_PATH"]); c.execute("CREATE TRIGGER t_snap BEFORE UPDATE OF last_x ON players BEGIN SELECT RAISE(ABORT,'injected'); END;"); c.commit()
PY
bleave alice $H
assert_contains "a failing snapshot does NOT stop a leave" "$BODY" '"ok":true'
occupants $H; assert_eq "she is out of the room" "[]" "$BODY"
python3 - <<'PY'
import sqlite3, os
c = sqlite3.connect(os.environ["DB_PATH"]); c.execute("DROP TRIGGER t_snap"); c.commit()
PY
assert_contains "the journal has the real sqlite error, not the response" "$(cat "$WORK/daemon.log")" 'injected'

echo "-- C2: floor items, cap 24 (BUILD property phase 1, was 8), owner only, survive restart"
bjoin alice $H 7 14 >/dev/null
bplacefloor alice $H seashell 5 10
assert_contains "owner puts a shell on the floor" "$BODY" '"ok":true'
get "$D/placed/$H"
assert_contains "GET lists the shell" "$BODY" 'seashell'
assert_contains "at the tile" "$BODY" '"x":5'
bplacefloor bob $H chalk_block 6 10
assert_code "a visitor cannot floor-place" 403
btakefloor bob $H 5 10
assert_code "a visitor cannot take" 403
bplacefloor alice $H cider_apple 7 15
assert_code "the door tile is refused" 400
bplacefloor alice $H cider_apple 5 10
assert_code "same tile is occupied" 409
# 22 more (23 total with the shell above), a grid on y=2/y=3 well clear of the shell (y=10) and the
# door (7,14 / 7,15). Checked against the actual GET afterward, not just "the loop didn't error" --
# a cap silently off by a few would otherwise pass this test for the wrong reason.
i=1
while [ $i -le 22 ]; do
  gx=$(( ( (i - 1) % 13 ) + 1 ))
  gy=$(( 2 + (i - 1) / 13 ))
  bplacefloor alice $H oak_gall $gx $gy >/dev/null
  i=$((i + 1))
done
get "$D/placed/$H"
echo "$BODY" > "$WORK/floor_after_loop.json"
assert_eq "all 22 loop placements actually landed, none silently refused" 22 "$(python3 -c "import json; d=json.load(open('$WORK/floor_after_loop.json')); print(sum(1 for f in d['floor'] if f['item']=='oak_gall'))")"
bplacefloor alice $H oak_gall 1 4
assert_code "the 24th floor item (1 shell + 22 gall + this one) succeeds" 200
bplacefloor alice $H oak_gall 2 4
assert_code "the 25th floor item is refused" 409
assert_contains "full" "$BODY" 'full'
btakefloor alice $H 5 10
assert_contains "take returns the slug" "$BODY" 'seashell'
get "$D/placed/$H"
assert_not_contains "the shell is gone" "$BODY" 'seashell'
assert_contains "the wall is untouched" "$BODY" 'iron_gall_ink_page'
bleave alice $H >/dev/null
stop_daemon; start_daemon
get "$D/placed/$H"
assert_contains "floor items survived the restart" "$BODY" 'oak_gall'
assert_contains "and the wall too" "$BODY" 'iron_gall_ink_page'

