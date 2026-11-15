#!/usr/bin/env bash
# When one part of the LGP1 store changes, everything that depends on it must change too: header docs, the undo
# tool and its test, writer/reader symmetry, and the asset files the server opens. See tests/lgp1_consistency.py.
# Brandon 2026-11-07: "when one thing changes, everything thats effected by that change is also changed accordingly".
source "$(dirname "$0")/lib.sh"

PY="$(command -v python3 || true)"
if [ -z "$PY" ]; then
  if [ "${CI:-}" = "true" ]; then _bad "python3 is installed in CI" "no python3"; else echo "  skip: no python3 here"; fi
  exit 0
fi
echo "-- LGP1 record types, undo tool, docs and assets agree (lgp1_consistency.py)"
"$PY" "$TESTS_DIR/lgp1_consistency.py" "$REPO" > "$WORK/cons.out" 2>&1; rc=$?
out="$(cat "$WORK/cons.out")"
echo "$out" | sed 's/^/     /'
assert_eq "no consistency check failed (python's exit code)" "0" "$rc"
assert_contains "and the checker's own summary says 0 failed" "$out" " checks, 0 failed"
n="$(sed -n 's/^lgp1_consistency: \([0-9]*\) checks.*/\1/p' <<<"$out")"
[ "${n:-0}" -ge 150 ] && _ok "at least 150 checks executed ($n)" || _bad "at least 150 checks executed" "$n"
