#!/usr/bin/env bash
# The daemon's 15s caches of civic answers and what happens when civic goes silent (a frozen civic,
# SIGSTOP, makes every real call cost its 1s timeout, so timing PROVES cache hit vs civic asked).
# Rule under test: a real answer is cached, a failure is NEVER cached.
source "$(dirname "$0")/lib.sh"
start_civic; start_daemon
for u in alice bob carol; do su $u; done
befriend alice bob
H=$(house alice)

echo "-- session cache (cookie -> username)"
bjoin alice 1,1 5 5 >/dev/null                       # a miss: civic is asked, the answer cached
freeze_civic
bmove alice 1,1 6 6
assert_contains "cache HIT with civic silent: still bound and moving" "$BODY" '"ok":true'
assert_between "and instant (civic was NOT asked)" 0 0.6 "$TIME"
_run -H 'X-Noise: 1' -H 'User-Agent: other' -b "$(jar alice)" -X POST --data-binary '{"id":"g_x","secret":"w","region":"1,1","x":7,"y":7,"facing":0}' "$D/presence/move"
assert_contains "keyed on the Cookie value only: unrelated headers still hit" "$BODY" '"ok":true'
assert_between "instant too" 0 0.6 "$TIME"
bjoin bob 1,1 4 4
assert_code "a cookie never seen while civic is silent: guest fallback (403)" 403
assert_between "after ~1s (civic WAS asked and gave nothing)" 0.8 3 "$TIME"
get "$D/health"; assert_between "a guest with no cookie is never sent to civic (instant)" 0 0.6 "$TIME"
thaw_civic
bjoin bob 1,1 4 4
assert_contains "civic back: bob resolves at once (the failure was NOT cached)" "$BODY" '"bound":true'
sleep 15
freeze_civic
bmove alice 1,1 8 8
assert_code "after the 15s TTL the entry has expired: civic is asked, is silent, guest fallback" 403
assert_between "(that took the ~1s timeout)" 0.8 3 "$TIME"
thaw_civic
bmove alice 1,1 8 8;  assert_contains "civic back: alice resolves again" "$BODY" '"ok":true'
bleave alice 1,1; bleave bob 1,1

echo "-- friends cache (a civic failure is not cached as 'you have no friends')"
bjoin alice $H >/dev/null
bjoin bob $H;    assert_contains "bob (a friend) is inside alice's house at t=0" "$BODY" '"ok":true'
sleep 16                                             # bob's session AND friends cache entries have both expired
bpickup bob chalk_block                              # a session-only request: refreshes the session cache, leaves friends stale
freeze_civic
bmove bob $H 3 3
assert_contains "civic silent + friends stale: the visitor STAYS (old behaviour: not_welcome)" "$BODY" '"ok":true'
assert_between "(the ~1s proves civic was really asked)" 0.8 3 "$TIME"
blots bob
assert_eq "his lots are dark for that poll" '{"lots":[]}' "$BODY"
assert_between "(one bounded wait)" 0.8 3 "$TIME"
bjoin carol $H
assert_code "a NEW entrant while civic is silent is refused, not waved in" 403
thaw_civic
blots bob;  assert_eq "civic back: lots are right at once (the failure was not pinned)" '{"lots":["alice"]}' "$BODY"
bmove bob $H 4 4;  assert_contains "the visitor is still there and moving" "$BODY" '"ok":true'
echo "   a REAL 'no friends' answer is still cached"
blots carol;  assert_eq "carol has no friends (civic answered)" '{"lots":[]}' "$BODY"
freeze_civic
blots carol
assert_between "cached: instant with civic silent" 0 0.6 "$TIME"
get "$D/presence/lots"; assert_between "a guest's lots: instant" 0 0.6 "$TIME"
thaw_civic
echo "   a real unfriend/block still ends a friends visit after the window (regression)"
block alice bob
sleep 16
bmove bob $H 5 5;  assert_contains "not_welcome" "$BODY" 'not_welcome'
