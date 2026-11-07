#!/usr/bin/env bash
# One installable app and the closed Commons (one_app_check.py). A new file: committed tests stay frozen (#6847b).
source "$(dirname "$0")/lib.sh"

PY="$(command -v python3 || true)"
if [ -z "$PY" ]; then
  if [ "${CI:-}" = "true" ]; then _bad "python3 is installed in CI" "no python3"; else echo "  skip: no python3 here"; fi
  exit 0
fi
echo "-- the manifests, icons and wiring (one_app_check.py)"
"$PY" "$TESTS_DIR/one_app_check.py" "$REPO" > "$WORK/one_app.out" 2>&1; rc=$?
out="$(cat "$WORK/one_app.out")"
echo "$out" | sed 's/^/     /'
assert_eq "no one-app check failed (python's exit code)" "0" "$rc"
assert_contains "and the checker's own summary says 0 failed" "$out" " checks, 0 failed"
n="$(sed -n 's/^one_app_check: \([0-9]*\) checks.*/\1/p' <<<"$out")"
[ "${n:-0}" -ge 12 ] && _ok "at least 12 checks executed ($n)" || _bad "at least 12 checks executed" "$n"
