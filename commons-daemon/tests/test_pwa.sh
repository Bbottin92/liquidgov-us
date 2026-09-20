#!/usr/bin/env bash
# The home-screen apps (BUILD pwa): the Commons manifest + apple-touch-icon + icons, and civic's Social and Civic manifests
# (same pattern as Mail). See tests/pwa_check.py for what is checked; everything comes from the shipped files, no server.
source "$(dirname "$0")/lib.sh"

PY="$(command -v python3 || true)"
if [ -z "$PY" ]; then
  if [ "${CI:-}" = "true" ]; then _bad "python3 is installed in CI" "no python3"; else echo "  skip: no python3 here"; fi
  exit 0
fi
echo "-- the manifests, icons and wiring (pwa_check.py)"
"$PY" "$TESTS_DIR/pwa_check.py" "$REPO" > "$WORK/pwa.out" 2>&1; rc=$?
out="$(cat "$WORK/pwa.out")"
echo "$out" | sed 's/^/     /'
assert_eq "no pwa check failed (python's exit code)" "0" "$rc"
assert_contains "and the checker's own summary says 0 failed" "$out" " checks, 0 failed"
n="$(sed -n 's/^pwa_check: \([0-9]*\) checks.*/\1/p' <<<"$out")"
[ "${n:-0}" -ge 60 ] && _ok "at least 60 checks executed ($n)" || _bad "at least 60 checks executed" "$n"
