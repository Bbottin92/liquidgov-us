#!/usr/bin/env bash
# Cookie bind (a bound player's id comes from the civic session, never from the client) and
# rebind (a guest who logs in mid-session is relabelled u:<name> IN PLACE, no ghost).
source "$(dirname "$0")/lib.sh"
start_civic; start_daemon
for u in alice bob carol; do su $u; done

echo "-- bound join/move/leave: garbage id and secret are ignored"
bjoin alice 1,1 5 5
assert_contains "bound join says bound" "$BODY" '"bound":true'
assert_eq "and returns the raw username" alice "$(jget username)"
occupants 1,1;  assert_contains "occupant id is u:<name>" "$BODY" '"id":"u:alice"'
assert_not_contains "no guest-style id anywhere" "$BODY" '"id":"g_'
bmove alice 1,1 6 6;  assert_contains "bound move ok with garbage id/secret" "$BODY" '"ok":true'
mint; gjoin $GID $GSEC 2,2 4 4 Guesty; assert_contains "a guest still joins normally" "$BODY" '"bound":false'
gmove $GID "wrong" 2,2;  assert_code "a guest with a wrong secret still 403s" 403
bleave alice 1,1; occupants 1,1;  assert_eq "bound leave empties the region" "[]" "$BODY"

echo "-- a client cannot claim to be bound"
post "$D/presence/join" '{"id":"u:alice","secret":"w","region":"1,1","x":1,"y":1,"facing":0}'
assert_code "id u:alice without the cookie is refused" 403
assert_contains "as a bad credential" "$BODY" 'bad_credential'

echo "-- rekey promotes and leaves no ghost"
mint; G1=$GID; S1=$GSEC; gjoin $G1 $S1 3,3 2 2 Ghosty
post "$D/presence/rekey" "{\"id\":\"$G1\",\"secret\":\"$S1\",\"from_region\":\"3,3\",\"to_region\":\"4,4\",\"x\":1,\"y\":1,\"facing\":0}" "$(jar bob)"
assert_contains "rekey with a cookie is bound" "$BODY" '"bound":true'
occupants 3,3; assert_eq "the OLD region has no ghost" "[]" "$BODY"
occupants 4,4; assert_contains "the new region has u:bob" "$BODY" '"id":"u:bob"'
assert_eq "exactly one row there" 1 "$(echo "$BODY" | grep -o '"id"' | wc -l)"

echo "-- rebind: a guest logs in mid-session; its next MOVE relabels the row in place"
mint; W_ID=$GID; W_SEC=$GSEC; gjoin $W_ID $W_SEC 1,1 5 5 wanderer
post "$D/presence/move" "{\"id\":\"$W_ID\",\"secret\":\"$W_SEC\",\"region\":\"1,1\",\"x\":9,\"y\":7,\"facing\":2}" "$(jar carol)"
assert_contains "the move answer says promoted" "$BODY" '"promoted":true'
assert_eq "as the raw username" carol "$(jget username)"
occupants 1,1
assert_contains "one row, now u:carol" "$BODY" '"id":"u:carol"'
assert_not_contains "no g_ ghost" "$BODY" '"id":"g_'
assert_contains "at the NEW position" "$BODY" '"x":9'
bmove carol 1,1 10 8;  assert_contains "the next move needs no guest secret any more" "$BODY" '"ok":true'
bleave carol 1,1

echo "   guards"
mint; V_ID=$GID; V_SEC=$GSEC; gjoin $V_ID $V_SEC 2,2 4 4 second
post "$D/presence/move" "{\"id\":\"$V_ID\",\"secret\":\"WRONG\",\"region\":\"2,2\",\"x\":6,\"y\":6,\"facing\":0}" "$(jar alice)"
assert_code "the cookie with a WRONG guest secret is refused (no proof of the row)" 403
occupants 2,2; assert_contains "and the guest row is untouched" "$BODY" '"id":"g_'
gmove $V_ID $V_SEC 2,2 6 6;  assert_contains "no cookie + right secret still moves as a guest" "$BODY" '"ok":true'

echo "   a second tab of the same account: newer wins, one id one row"
bjoin alice 1,1 5 5                                     # alice is bound in 1,1
mint; T_ID=$GID; T_SEC=$GSEC; gjoin $T_ID $T_SEC 3,3 2 2 tabtwo
post "$D/presence/move" "{\"id\":\"$T_ID\",\"secret\":\"$T_SEC\",\"region\":\"3,3\",\"x\":3,\"y\":3,\"facing\":0}" "$(jar alice)"
assert_contains "the guest tab is promoted" "$BODY" '"promoted":true'
occupants 3,3; assert_contains "u:alice is now in 3,3" "$BODY" '"id":"u:alice"'
occupants 1,1; assert_eq "her older row in 1,1 was superseded" "[]" "$BODY"

echo "   a name collision is disambiguated"
mint; N1=$GID; N1S=$GSEC; gjoin $N1 $N1S 4,4 1 1 bob         # a guest already displays as "bob" in 4,4? bob is bound there
mint; N2=$GID; N2S=$GSEC; gjoin $N2 $N2S 5,5 1 1 alice
mint; N3=$GID; N3S=$GSEC; gjoin $N3 $N3S 5,5 2 2 tabthree
post "$D/presence/move" "{\"id\":\"$N3\",\"secret\":\"$N3S\",\"region\":\"5,5\",\"x\":3,\"y\":3,\"facing\":0}" "$(jar alice)"
assert_contains "promoted next to a guest already called alice" "$BODY" '"promoted":true'
assert_eq "labelled 'alice (2)', not a duplicate" "alice (2)" "$(jget display_name)"

echo "   never into a house region"
mint; H_ID=$GID; H_SEC=$GSEC
post "$D/presence/move" "{\"id\":\"$H_ID\",\"secret\":\"$H_SEC\",\"region\":\"instance:house:alice\",\"x\":3,\"y\":3,\"facing\":0}" "$(jar alice)"
assert_not_contains "promotion is never applied inside a house region" "$BODY" '"promoted"'
