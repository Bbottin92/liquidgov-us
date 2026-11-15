#!/usr/bin/env bash
# Shared harness for the repo's own wrapper tests (test_*.sh). Source it from a test_*.sh.
# Moved here from tests/repo/lib.sh on 2026-11-15 (the Commons was archived); the daemon parts are gone:
# this keeps only the counters, the assertions and a scratch folder that is removed when the test ends.
set -u

TESTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$TESTS_DIR/../.." && pwd)"
TEST_NAME="$(basename "${0:-test}" .sh)"
WORK="$(mktemp -d /tmp/repo-test.XXXXXX)"
PASS=0; FAIL=0

_on_exit() {
  local rc=$?
  chmod -R u+w "$WORK" 2>/dev/null
  rm -rf "$WORK"
  echo "== $TEST_NAME: $PASS passed, $FAIL failed"
  if [ "$rc" -ne 0 ]; then exit "$rc"; fi
  [ "$FAIL" -eq 0 ] || exit 1
  exit 0
}
trap _on_exit EXIT
trap 'exit 143' TERM
trap 'exit 130' INT

_ok() { PASS=$((PASS+1)); echo "  ok   $1"; }
_bad() { FAIL=$((FAIL+1)); echo "  FAIL $1"; [ -n "${2:-}" ] && echo "       $2"; }
assert_eq() { if [ "$2" = "$3" ]; then _ok "$1"; else _bad "$1" "expected: $2 | got: $3"; fi; }
assert_contains() { case "$2" in *"$3"*) _ok "$1";; *) _bad "$1" "expected to contain: $3 | got: $2";; esac; }
assert_not_contains() { case "$2" in *"$3"*) _bad "$1" "must NOT contain: $3 | got: $2";; *) _ok "$1";; esac; }
