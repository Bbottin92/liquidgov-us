#!/usr/bin/env bash
# The extra world props (commons/js/world.js SCATTER): they fill plain ground only, keep the paths, doors, seams and region names clear,
# and never seal off any walkable tile. See tests/world_scatter_unit.js. In CI node is installed: a missing node there is a FAILURE, not a skip.
source "$(dirname "$0")/lib.sh"

COMMONS="$REPO/commons"; UNIT="$TESTS_DIR/world_scatter_unit.js"
NODE="$(command -v node || true)"; WIN=0
if [ -z "$NODE" ] && command -v node.exe >/dev/null 2>&1; then NODE="$(command -v node.exe)"; WIN=1; fi
if [ -z "$NODE" ]; then
  if [ "${CI:-}" = "true" ]; then _bad "node is installed in CI" "no node binary"; else echo "  skip: no node here (the scatter test runs in CI)"; fi
  exit 0
fi

echo "-- the wiring"
W="$COMMONS/js/world.js"
assert_eq "the scatter table is declared before the world is built (it is read at load time)" "yes" \
  "$([ "$(grep -n '^var SCATTER = \[' "$W" | cut -d: -f1)" -lt "$(grep -n '^(function bakeTileIds' "$W" | cut -d: -f1)" ] && echo yes || echo no)"
assert_eq "scatterRegions() is called once, from dressRegions()" "1" "$(grep -c '^  scatterRegions();' "$W")"
assert_eq "it is the last thing dressRegions() does (so it can only fill what is left)" "yes" \
  "$(awk '/^function dressRegions/{f=1} f&&/scatterRegions\(\);/{s=NR} f&&/^}/{if(s&&NR==s+1){print "yes"}else{print "no"}; exit}' "$W")"
assert_eq "it refuses to overwrite anything but plain ground (refusals are recorded, not silent)" "1" "$(grep -c 'SCATTER_REFUSED.push' "$W")"

echo "-- the world (world_scatter_unit.js)"
# (Windows node under WSL: write to a file so $? is node's own exit code; see test_client_files.sh.)
if [ "$WIN" = 1 ]; then "$NODE" "$(wslpath -w "$UNIT")" "$(wslpath -w "$COMMONS")" > "$WORK/unit.out" 2>&1; rc=$?; out="$(tr -d '\r' < "$WORK/unit.out")"
else out="$("$NODE" "$UNIT" "$COMMONS" 2>&1)"; rc=$?; fi
echo "$out" | sed 's/^/     /'
assert_eq "no scatter check failed (node's exit code)" "0" "$rc"
assert_contains "and the unit's own summary says 0 failed" "$out" " checks, 0 failed"
n="$(sed -n 's/^world_scatter_unit: \([0-9]*\) checks.*/\1/p' <<<"$out")"
[ "${n:-0}" -ge 14 ] && _ok "at least 14 checks executed ($n)" || _bad "at least 14 checks executed" "$n"
