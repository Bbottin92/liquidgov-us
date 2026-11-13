#!/usr/bin/env bash
# The Mail/Social/Civic manifests and icons still served for phones that already installed them
# (pwa_files_check.py). The three-app picker stays out.
source "$(dirname "$0")/lib.sh"

PY="$(command -v python3 || true)"
if [ -z "$PY" ]; then
  if [ "${CI:-}" = "true" ]; then _bad "python3 is installed in CI" "no python3"; else echo "  skip: no python3 here"; fi
  exit 0
fi
echo "-- the old manifests and icons that still answer (pwa_files_check.py)"
"$PY" "$TESTS_DIR/pwa_files_check.py" "$REPO" > "$WORK/pwa_files.out" 2>&1; rc=$?
out="$(cat "$WORK/pwa_files.out")"
echo "$out" | sed 's/^/     /'
assert_eq "no pwa-files check failed (python's exit code)" "0" "$rc"
assert_contains "and the checker's own summary says 0 failed" "$out" " checks, 0 failed"
n="$(sed -n 's/^pwa_files_check: \([0-9]*\) checks.*/\1/p' <<<"$out")"
[ "${n:-0}" -ge 30 ] && _ok "at least 30 checks executed ($n)" || _bad "at least 30 checks executed" "$n"
