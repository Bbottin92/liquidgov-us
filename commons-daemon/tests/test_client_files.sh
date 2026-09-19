#!/usr/bin/env bash
# The client's split files and their LOAD ORDER (commons/index.html + style.css + js/*.js): the eight scripts are loaded IN TAG
# ORDER under node with a stubbed DOM, and any ReferenceError fails the build (a missing name would blank the game).
# See tests/client_files_unit.js for what is checked and why concatenating the sources is not enough.
# In CI node is installed: a missing node there is a FAILURE, not a skip.
source "$(dirname "$0")/lib.sh"

COMMONS="$REPO/commons"; UNIT="$TESTS_DIR/client_files_unit.js"
NODE="$(command -v node || true)"; WIN=0
if [ -z "$NODE" ] && command -v node.exe >/dev/null 2>&1; then NODE="$(command -v node.exe)"; WIN=1; fi
if [ -z "$NODE" ]; then
  if [ "${CI:-}" = "true" ]; then _bad "node is installed in CI" "no node binary"; else echo "  skip: no node here (the client file test runs in CI)"; fi
  exit 0
fi

echo "-- the files exist as documented"
for f in index.html style.css js/world.js js/interiors.js js/sprites.js js/actors.js js/net.js js/input.js js/interact.js js/main.js; do
  [ -s "$COMMONS/$f" ] && r=present || r=MISSING; assert_eq "commons/$f" "present" "$r"
done
for f in "$COMMONS"/js/*.js; do
  if "$NODE" --check "$([ "$WIN" = 1 ] && wslpath -w "$f" || echo "$f")" > "$WORK/check.log" 2>&1; then _ok "syntax ok: js/$(basename "$f")"; else _bad "syntax: js/$(basename "$f")" "$(head -c 300 "$WORK/check.log")"; fi
done
assert_eq "no inline <script> body is left in index.html (one line per tag, no code)" "0" "$(awk '/<script>/{c++} END{print c+0}' "$COMMONS/index.html")"

echo "-- structure and LOAD ORDER (client_files_unit.js)"
# (Windows node under WSL: write to a file so $? is node's own exit code; ${PIPESTATUS[0]} after $(...| tr) would always be tr's 0.)
if [ "$WIN" = 1 ]; then "$NODE" "$(wslpath -w "$UNIT")" "$(wslpath -w "$COMMONS")" > "$WORK/unit.out" 2>&1; rc=$?; out="$(tr -d '\r' < "$WORK/unit.out")"
else out="$("$NODE" "$UNIT" "$COMMONS" 2>&1)"; rc=$?; fi
echo "$out" | sed 's/^/     /'
assert_eq "no client-file check failed (node's exit code)" "0" "$rc"
assert_contains "and the unit's own summary says 0 failed" "$out" " checks, 0 failed"
n="$(sed -n 's/^client_files_unit: \([0-9]*\) checks.*/\1/p' <<<"$out")"
[ "${n:-0}" -ge 40 ] && _ok "at least 40 checks executed ($n)" || _bad "at least 40 checks executed" "$n"
