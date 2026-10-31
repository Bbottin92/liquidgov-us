#!/usr/bin/env bash
# Visiting under the default `friends` policy: who gets in, what a visitor can and cannot do,
# and that a block walks a visitor out (after the 15s friends-cache window).
source "$(dirname "$0")/lib.sh"
start_civic; start_daemon
for u in alice bob carol; do su $u; done
befriend alice bob        # alice <-> bob mutual friends; carol is a stranger
H=$(house alice)

echo "-- entry rules"
bjoin bob $H;    assert_code "a friend is refused while the OWNER is not in the game" 403
assert_contains "with the visit-refusal reason" "$BODY" 'not_your_house'
bjoin alice $H;  assert_contains "the owner enters (unchanged path)" "$BODY" '"ok":true'
bjoin bob $H;    assert_contains "now the friend is welcome" "$BODY" '"ok":true'
assert_not_contains "a visitor gets no resume fields" "$BODY" 'resume_x'
bjoin carol $H;  assert_code "a non-friend is refused" 403
post "$D/presence/join" "{\"id\":\"g_x\",\"secret\":\"w\",\"region\":\"$H\",\"x\":1,\"y\":1,\"facing\":0}"
assert_code "a guest is refused (guests have no friends)" 403
occupants $H
assert_contains "the owner is in the room" "$BODY" 'u:alice'
assert_contains "and so is the visitor" "$BODY" 'u:bob'

echo "-- what a visitor may do"
bplace bob $H chalk_block;   assert_code "a visitor cannot place in the owner's house" 403
bplace alice $H chalk_block; assert_contains "the owner can" "$BODY" '"ok":true'
get "$D/placed/$H";          assert_contains "the wall is visible to anyone" "$BODY" 'chalk_block'
assert_eq "the visit created NO players row for the visitor" "0" "$(dbq "select count(*) from players where lg_username='bob'")"
assert_eq "and stored no position for the owner from it" "" "$(dbq "select last_x from players where lg_username='alice'")"
bmove bob $H 4 4;  assert_contains "a friend moves fine while still a friend" "$BODY" '"ok":true'

echo "-- a block ends the visit (15s friends cache, then the next move)"
block alice bob
bmove bob $H 5 5
assert_contains "still cached: inside the window the visit continues" "$BODY" '"ok":true'
sleep 16
bmove bob $H 6 6
assert_contains "after the window: not_welcome" "$BODY" 'not_welcome'
assert_code "answered 200 (a 403 would read as a dead credential)" 200
occupants $H
assert_not_contains "the visitor is out of the room" "$BODY" 'u:bob'
assert_contains "the owner remains" "$BODY" 'u:alice'
bjoin bob $H;  assert_code "and cannot come back in" 403

echo "-- the owner's own path is untouched"
bmove alice $H 3 3 2
bleave alice $H
bjoin alice $H
assert_contains "the owner resumes where she left" "$BODY" '"resume_x":3'
