#!/usr/bin/env bash
# The client's guest-credential mint (commons/index.html, netMint): a credential only from a 200 with {id, secret};
# a failure retries with capped backoff instead of leaving the player local-only until they reload.
# Runs the SHIPPED code (extracted from the page between the netMint:begin/netMint:end markers) under node with a fake
# clock and a scripted fetch (tests/netmint_unit.js). In CI node is installed: a missing node there is a FAILURE.
source "$(dirname "$0")/lib.sh"

HTML="$REPO/commons/js/net.js"; UNIT="$TESTS_DIR/netmint_unit.js"      # the client was split into js/*.js: netMint lives in js/net.js
CLIENT_JS="$REPO/commons/js"
NODE="$(command -v node || true)"; WIN=0
if [ -z "$NODE" ] && command -v node.exe >/dev/null 2>&1; then NODE="$(command -v node.exe)"; WIN=1; fi   # WSL: the Windows node
if [ -z "$NODE" ]; then
  if [ "${CI:-}" = "true" ]; then _bad "node is installed in CI" "no node binary"; else echo "  skip: no node here (the client unit test runs in CI)"; fi
  exit 0
fi

echo "-- the page carries the markers, and the mint is checked, retried and shared"
assert_eq "one netMint:begin marker" "1" "$(grep -c '// ---- netMint:begin' "$HTML")"
assert_eq "one netMint:end marker" "1" "$(grep -c '// ---- netMint:end' "$HTML")"
assert_eq "the mint is called from exactly two places across the client (startup in main.js, and the re-auth handler in net.js); comments do not count" "2" "$(cat "$CLIENT_JS"/*.js | grep -cE '^[^/]*netMint\(\)(;|\.then)')"

echo "-- the unit cases ($("$NODE" --version | tr -d '\r'))"
# (Windows node under WSL: write to a file so $? is node's own exit code; ${PIPESTATUS[0]} after $(...| tr) would always be tr's 0.)
if [ "$WIN" = 1 ]; then "$NODE" "$(wslpath -w "$UNIT")" "$(wslpath -w "$HTML")" > "$WORK/unit.out" 2>&1; rc=$?; out="$(tr -d '\r' < "$WORK/unit.out")"
else out="$("$NODE" "$UNIT" "$HTML" 2>&1)"; rc=$?; fi
echo "$out" | sed 's/^/     /'
assert_eq "no unit case failed (node's exit code)" "0" "$rc"
assert_contains "and the unit's own summary says 0 failed" "$out" " checks, 0 failed"
n="$(sed -n 's/^netmint_unit: \([0-9]*\) checks.*/\1/p' <<<"$out")"
[ "${n:-0}" -ge 80 ] && _ok "at least 80 checks executed ($n)" || _bad "at least 80 checks executed" "$n"
