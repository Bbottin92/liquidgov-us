#!/usr/bin/env bash
# The owner-set house policy: friends (default) / locked / open, owner-only setter, live effect,
# lots discovery for friends, and persistence across a real restart.
source "$(dirname "$0")/lib.sh"
start_civic; start_daemon
for u in alice bob carol; do su $u; done
befriend alice bob
H=$(house alice)

echo "-- default policy"
bjoin alice $H;  assert_contains "the owner's join echoes friends" "$BODY" '"house_policy":"friends"'
bjoin bob $H;    assert_contains "a friend is welcome" "$BODY" '"ok":true'
blots bob;       assert_eq "the friend's lots list alice" '{"lots":["alice"]}' "$BODY"

echo "-- the setter: owner-only, cookie identity only"
post "$D/presence/house-policy" '{"policy":"locked"}';  assert_code "a guest (no cookie) is refused" 403
assert_contains "as not_bound" "$BODY" 'not_bound'
bpolicy alice public;     assert_code "an unknown policy is refused" 400
assert_contains "as bad_policy" "$BODY" 'bad_policy'
post "$D/presence/house-policy" '{}' "$(jar alice)"; assert_code "a missing policy is refused" 400
post "$D/presence/house-policy" '{"policy":"locked","username":"alice"}' "$(jar bob)"
assert_code "bob naming alice in the body only ever affects bob (409: he has no house yet)" 409
assert_contains "as no_house" "$BODY" 'no_house'
bmove bob $H 3 3;         assert_contains "alice's policy is unchanged: bob still moves" "$BODY" '"ok":true'
bpolicy carol locked;     assert_code "a bound player with no house cannot set one" 409

echo "-- locking is read live (no cache wait)"
bpolicy alice locked;     assert_contains "alice locks" "$BODY" '"policy":"locked"'
bmove bob $H 4 4
assert_contains "the visitor is walked out on their very next move" "$BODY" 'not_welcome'
occupants $H; assert_not_contains "and is gone from the room" "$BODY" 'u:bob'
bjoin bob $H;  assert_code "re-entry refused while locked" 403
blots bob;     assert_eq "the door goes dark: lots empty" '{"lots":[]}' "$BODY"
bjoin carol $H; assert_code "a stranger is still refused" 403
bjoin alice $H; assert_contains "the OWNER still enters a locked house" "$BODY" '"house_policy":"locked"'
bmove alice $H 3 3;  assert_contains "and moves in it" "$BODY" '"ok":true'

echo "-- it survives a real restart"
stop_daemon; start_daemon
bjoin alice $H; assert_contains "still locked after the restart" "$BODY" '"house_policy":"locked"'
bjoin bob $H;   assert_code "and the friend is still refused" 403
blots bob;      assert_eq "lots still empty" '{"lots":[]}' "$BODY"

echo "-- unlocking"
bpolicy alice friends; assert_contains "alice unlocks" "$BODY" '"policy":"friends"'
bjoin bob $H;   assert_contains "the friend is welcome again" "$BODY" '"ok":true'
blots bob;      assert_eq "and the door lights again" '{"lots":["alice"]}' "$BODY"
bleave alice $H
post "$D/presence/rekey" "{\"id\":\"g_x\",\"secret\":\"w\",\"from_region\":\"1,1\",\"to_region\":\"$H\",\"x\":7,\"y\":14,\"facing\":0}" "$(jar alice)"
assert_contains "a rekey into her own house echoes the policy too" "$BODY" '"house_policy":"friends"'
