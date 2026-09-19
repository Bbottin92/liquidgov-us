#!/usr/bin/env bash
# The civic /internal/* contract the daemon depends on. Runs against the stub AND the real
# civic (CIVIC_MODE=real): both must pass, which keeps civic_stub.py honest.
source "$(dirname "$0")/lib.sh"
start_civic
for u in alice bob carol; do su $u; done
befriend alice bob
block alice carol

echo "-- session-user"
get "$V/internal/session-user" "$(jar alice)"; assert_eq "session-user resolves the cookie's account" '{"username":"alice"}' "$BODY"
get "$V/internal/session-user";               assert_eq "no cookie -> {}" '{}' "$BODY"
_run -H "X-Forwarded-For: 1.2.3.4" -b "$(jar alice)" "$V/internal/session-user"
assert_code "X-Forwarded-For (as if via nginx) -> plain 404" 404

echo "-- friends (mutual only, blocks removed either way)"
get "$V/internal/friends" "$(jar alice)"; assert_contains "alice's friends include bob" "$BODY" '"bob"'
get "$V/internal/friends";                assert_eq "no session -> {}" '{}' "$BODY"
_run -H "X-Forwarded-For: 1.2.3.4" -b "$(jar alice)" "$V/internal/friends"; assert_code "friends with X-Forwarded-For -> 404" 404
block bob alice
get "$V/internal/friends" "$(jar alice)"; assert_not_contains "a block (bob blocked alice) removes the friend from alice's list" "$BODY" '"bob"'
get "$V/internal/friends" "$(jar bob)";   assert_not_contains "...and alice from bob's list (either direction)" "$BODY" '"alice"'

echo "-- blocked (fails closed)"
get "$V/internal/blocked/bob" "$(jar carol)";       assert_eq "carol -> bob (no block between them)" '{"blocked":false}' "$BODY"
get "$V/internal/blocked/carol" "$(jar alice)";     assert_eq "alice -> carol (alice blocked carol)" '{"blocked":true}' "$BODY"
get "$V/internal/blocked/alice" "$(jar carol)";     assert_eq "carol -> alice (the OTHER direction)" '{"blocked":true}' "$BODY"
get "$V/internal/blocked/alice";                    assert_eq "no session fails closed" '{"blocked":true}' "$BODY"
get "$V/internal/blocked/nobodyhere" "$(jar carol)"; assert_eq "unknown owner fails closed" '{"blocked":true}' "$BODY"
get "$V/internal/blocked/a%20b" "$(jar carol)";     assert_eq "malformed name fails closed" '{"blocked":true}' "$BODY"
_run -H "X-Forwarded-For: 1.2.3.4" -b "$(jar carol)" "$V/internal/blocked/bob"; assert_code "blocked with X-Forwarded-For -> 404" 404

echo "-- user-exists (never false for a malformed name)"
get "$V/internal/user-exists/alice";        assert_eq "existing account" '{"exists":true}' "$BODY"
get "$V/internal/user-exists/nobodyhere";   assert_eq "well-formed name that does not exist" '{"exists":false}' "$BODY"
get "$V/internal/user-exists/a%20b";        assert_eq "malformed name is null, never false" '{"exists":null}' "$BODY"
_run -H "X-Forwarded-For: 1.2.3.4" "$V/internal/user-exists/alice"; assert_code "user-exists with X-Forwarded-For -> 404" 404
delete_account carol
get "$V/internal/user-exists/carol";        assert_eq "a deleted account no longer exists" '{"exists":false}' "$BODY"
get "$V/internal/session-user" "$(jar carol)"; assert_eq "and its old cookie resolves to nobody" '{}' "$BODY"
