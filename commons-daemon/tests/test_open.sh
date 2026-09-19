#!/usr/bin/env bash
# `open` houses: any bound, non-blocked player may enter while the owner is in the game. Blocks are
# honoured in EITHER direction through civic's /internal/blocked, which fails closed. Discovery is
# /presence/lots (friends first, then open houses of non-friends, six in all, never for guests).
source "$(dirname "$0")/lib.sh"
start_civic; start_daemon
for u in alice bob carol dave eve zed fay; do su $u; done
befriend alice bob
block alice dave          # alice blocked dave
block eve alice           # eve blocked alice (the OTHER direction)
H=$(house alice)
bjoin alice $H >/dev/null
bpolicy alice open;  assert_contains "alice sets her house open" "$BODY" '"policy":"open"'

echo "-- who can walk in"
bjoin carol $H;   assert_contains "a stranger who is not blocked" "$BODY" '"ok":true'
bjoin bob $H;     assert_contains "a friend (as before)" "$BODY" '"ok":true'
bjoin dave $H;    assert_code "someone the owner blocked is refused" 403
bjoin eve $H;     assert_code "someone who blocked the owner is refused (either direction)" 403
post "$D/presence/join" "{\"id\":\"g_x\",\"secret\":\"w\",\"region\":\"$H\",\"x\":1,\"y\":1,\"facing\":0}"
assert_code "a guest never enters" 403
bjoin carol "instance:house:al ice";  assert_code "a name with a space is refused (never forwarded to civic)" 403
post "$D/presence/join" "{\"id\":\"g_x\",\"secret\":\"w\",\"region\":\"instance:house:alice\\r\\nX-Evil: 1\",\"x\":1,\"y\":1,\"facing\":0}" "$(jar carol)"
assert_code "CR/LF in the owner name is refused" 403

echo "-- discovery: /presence/lots"
bjoin zed "$(house zed)" >/dev/null; bpolicy zed open >/dev/null
blots carol;  assert_eq "a stranger sees the open houses, sorted" '{"lots":["alice","zed"]}' "$BODY"
blots bob;    assert_eq "a friend sees the friend door first, then the open one" '{"lots":["alice","zed"]}' "$BODY"
blots alice;  assert_eq "the owner sees her friend, then zed, never her own house" '{"lots":["bob","zed"]}' "$BODY"
blots dave;   assert_eq "someone blocked by alice does not see her house" '{"lots":["zed"]}' "$BODY"
blots eve;    assert_eq "nor does someone who blocked her" '{"lots":["zed"]}' "$BODY"
get "$D/presence/lots"; assert_eq "a guest sees no lots at all" '{"lots":[]}' "$BODY"

echo "-- policy changes are read live"
bpolicy alice friends >/dev/null
bmove carol $H 3 3;  assert_contains "friends: the stranger is out on their next move" "$BODY" 'not_welcome'
blots carol;         assert_eq "alice leaves the stranger's lots" '{"lots":["zed"]}' "$BODY"
bmove bob $H 3 3;    assert_contains "the friend stays" "$BODY" '"ok":true'
bpolicy alice open >/dev/null
bjoin carol $H;      assert_contains "open again: the stranger re-enters" "$BODY" '"ok":true'
bpolicy alice locked >/dev/null
bmove bob $H 4 4;    assert_contains "locked: even the friend is out" "$BODY" 'not_welcome'
blots carol;         assert_eq "locked: not in anyone's lots" '{"lots":["zed"]}' "$BODY"
bpolicy alice open >/dev/null

echo "-- a block ends a visit (after the 15s cache), and hides the house"
bjoin carol $H >/dev/null
bmove carol $H 3 3;  assert_contains "carol is inside and moving" "$BODY" '"ok":true'
block alice carol
bmove carol $H 4 4;  assert_contains "still cached inside the window" "$BODY" '"ok":true'
sleep 16
bmove carol $H 5 5;  assert_contains "after the window: not_welcome" "$BODY" 'not_welcome'
bjoin carol $H;      assert_code "and cannot come back in" 403
blots carol;         assert_eq "alice is hidden from carol's lots" '{"lots":["zed"]}' "$BODY"

echo "-- fail closed: civic frozen"
freeze_civic
bjoin fay $H;   assert_code "a stranger never probed is refused when civic says nothing" 403
assert_between "...after one bounded wait (the 1s civic timeout)" 0.8 3 "$TIME"
blots bob;      assert_between "lots with civic silent: one bounded wait, not one per candidate" 0.8 3 "$TIME"
thaw_civic
bjoin fay $H;   assert_contains "civic back: the failure was NOT cached, fay walks in at once" "$BODY" '"ok":true'
