#!/usr/bin/env bash
# The client's guest-credential mint (commons/index.html, netMint): a credential only from a 200 with {id, secret};
# a failure retries with capped backoff instead of leaving the player local-only until they reload.
# Runs the SHIPPED code (extracted from the page between the netMint:begin/netMint:end markers) under node with a fake
# clock and a scripted fetch (tests/netmint_unit.js). In CI node is installed: a missing node there is a FAILURE.
source "$(dirname "$0")/lib.sh"

HTML="$REPO/commons/index.html"; UNIT="$TESTS_DIR/netmint_unit.js"
NODE="$(command -v node || true)"; WIN=0
if [ -z "$NODE" ] && command -v node.exe >/dev/null 2>&1; then NODE="$(command -v node.exe)"; WIN=1; fi   # WSL: the Windows node
if [ -z "$NODE" ]; then
  if [ "${CI:-}" = "true" ]; then _bad "node is installed in CI" "no node binary"; else echo "  skip: no node here (the client unit test runs in CI)"; fi
  exit 0
fi

echo "-- the page carries the markers, and the mint is checked, retried and shared"
assert_eq "one netMint:begin marker" "1" "$(grep -c '// ---- netMint:begin' "$HTML")"
assert_eq "one netMint:end marker" "1" "$(grep -c '// ---- netMint:end' "$HTML")"
assert_eq "the mint is called from exactly two places (startup, and the re-auth handler); comments do not count" "2" "$(grep -cE '^[^/]*netMint\(\)(;|\.then)' "$HTML")"

echo "-- the unit cases ($("$NODE" --version | tr -d '\r'))"
if [ "$WIN" = 1 ]; then out="$("$NODE" "$(wslpath -w "$UNIT")" "$(wslpath -w "$HTML")" 2>&1 | tr -d '\r')"; rc=${PIPESTATUS[0]}
else out="$("$NODE" "$UNIT" "$HTML" 2>&1)"; rc=$?; fi
echo "$out" | sed 's/^/     /'
assert_eq "no unit case failed" "0" "$rc"
n="$(sed -n 's/^netmint_unit: \([0-9]*\) checks.*/\1/p' <<<"$out")"
[ "${n:-0}" -ge 80 ] && _ok "at least 80 checks executed ($n)" || _bad "at least 80 checks executed" "$n"
