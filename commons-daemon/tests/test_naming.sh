#!/usr/bin/env bash
# New files match naming.toml. The self-test must fail a badly named file and a shared port first.
source "$(dirname "$0")/lib.sh"

PY="$(command -v python3 || true)"
if [ -z "$PY" ]; then
  if [ "${CI:-}" = "true" ]; then _bad "python3 is installed in CI" "no python3"; else echo "  skip: no python3 here"; fi
  exit 0
fi
echo "-- a badly named file and a shared port must fail (naming_check.py --self-test)"
"$PY" "$TESTS_DIR/naming_check.py" --self-test > "$WORK/naming_self.out" 2>&1; rc=$?
out="$(cat "$WORK/naming_self.out")"
echo "$out" | sed 's/^/     /'
assert_eq "the self-test exits 0 only after both bad cases failed" "0" "$rc"
assert_contains "and it says the bad name failed" "$out" "bad-name fail=True"
assert_contains "and it says the shared port failed" "$out" "port-clash fail=True"

echo "-- this repo matches naming.toml (naming_check.py)"
"$PY" "$TESTS_DIR/naming_check.py" "$REPO" > "$WORK/naming.out" 2>&1; rc=$?
out="$(cat "$WORK/naming.out")"
echo "$out" | sed 's/^/     /'
assert_eq "no naming check failed (python's exit code)" "0" "$rc"
assert_contains "and the checker's own summary says 0 failed" "$out" " checks, 0 failed"
n="$(sed -n 's/^naming_check: \([0-9]*\) checks.*/\1/p' <<<"$out")"
[ "${n:-0}" -ge 50 ] && _ok "at least 50 checks executed ($n)" || _bad "at least 50 checks executed" "$n"
