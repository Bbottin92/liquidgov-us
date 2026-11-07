#!/usr/bin/env bash
# run_all.sh discovers this wrapper automatically in CI.
source "$(dirname "$0")/lib.sh"
python3 "$TESTS_DIR/restart_bypass_check.py" > "$WORK/restart_bypass.out" 2>&1
rc=$?
out="$(cat "$WORK/restart_bypass.out")"
echo "$out"
assert_eq "restart bypass regression exits successfully" "0" "$rc"
assert_contains "regression summary reports no failures" "$out" " checks, 0 failed"
