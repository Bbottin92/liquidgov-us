#!/usr/bin/env bash
# Shared harness for the test_*.sh wrappers in tests/repo (each runs one repo check with python3 and asserts on its
# output). Source it from a test_*.sh. Moved here from the Commons daemon's harness (2026-11-13, Brandon: "edit and
# move the shared checks"); only the parts the wrappers use: REPO, TESTS_DIR, WORK, and the assert helpers.
set -u

TESTS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$TESTS_DIR/../.." && pwd)"
TEST_NAME="$(basename "${0:-test}" .sh)"
WORK="$(mktemp -d /tmp/repo-test.XXXXXX)"
PASS=0; FAIL=0

_on_exit() {
  local rc=$?
  rm -rf "$WORK"
  echo "== $TEST_NAME: $PASS passed, $FAIL failed"
  if [ "$rc" -ne 0 ]; then exit "$rc"; fi   # a FATAL exit keeps its own status
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
