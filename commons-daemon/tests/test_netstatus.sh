#!/usr/bin/env bash
# The client's "Reconnecting…" connection badge: the state machine (commons/js/net.js, run verbatim under node with a fake clock:
# tests/netstatus_unit.js) and how it is wired: observe-only, one heartbeat per tick, the element and its CSS, no d-pad overlap.
# In CI node is installed: a missing node there is a FAILURE, not a skip.
source "$(dirname "$0")/lib.sh"

C="$REPO/commons"; NET="$C/js/net.js"; MAIN="$C/js/main.js"; HTML="$C/index.html"; CSS="$C/style.css"; UNIT="$TESTS_DIR/netstatus_unit.js"
NODE="$(command -v node || true)"; WIN=0
if [ -z "$NODE" ] && command -v node.exe >/dev/null 2>&1; then NODE="$(command -v node.exe)"; WIN=1; fi
if [ -z "$NODE" ]; then
  if [ "${CI:-}" = "true" ]; then _bad "node is installed in CI" "no node binary"; else echo "  skip: no node here (the badge unit test runs in CI)"; fi
  exit 0
fi

# fn_body NAME FILE -> the text of `function NAME(...) { ... }` (up to the first line that is exactly "}")
fn_body() { awk -v n="$1" '$0 ~ "^function "n"\\(" {f=1} f {print} f && /^}$/ {exit}' "$2"; }

echo "-- how it is wired into the client"
assert_eq "one netStatus:begin marker in js/net.js" "1" "$(grep -c '// ---- netStatus:begin' "$NET")"
assert_eq "one netStatus:end marker" "1" "$(grep -c '// ---- netStatus:end' "$NET")"
assert_eq "frame() ticks the badge exactly once per frame" "1" "$(grep -c 'netStatusTick(t);' "$MAIN")"
assert_contains "and does it near the top of frame(), before any early exit can skip it" "$(fn_body frame "$MAIN" | head -6)" "netStatusTick(t)"
mv_body="$(fn_body netMove "$NET")"
assert_contains "netMove reports 'reached' for a 200 or a 403 (the daemon answered; the remint handles a 403)" "$mv_body" "netStatusReport(r.ok || r.status === 403)"
assert_contains "netMove reports a network error as a failure" "$mv_body" "function (e) { netStatusReport(false); throw e; }"
assert_contains "and re-throws it, so the old silent catch still runs (behaviour unchanged)" "$mv_body" ".catch(function () {})"
assert_eq "the move heartbeat is the ONLY reporter (a tick sends 1 move but 1-4 polls: counting polls would show the badge after ~1s)" "3" "$(grep -c 'netStatusReport(' "$NET" | awk '{print $1 + 0}')"   # the definition + the two calls in netMove
for f in netPollRegions netPollCurrentRegion netPollPlaced netPollLots netJoin netRekey netLeave; do
  assert_not_contains "$f does not report to the badge" "$(fn_body "$f" "$NET")" "netStatusReport"
done

echo "-- the element and its CSS"
assert_contains "index.html has #connStatus as a polite live region (role=status)" "$(grep 'id="connStatus"' "$HTML")" 'role="status" aria-live="polite"'
assert_contains "with the longer explanation in title" "$(grep 'id="connStatus"' "$HTML")" "others can't see you until it reconnects"
assert_eq "and the element is EMPTY in the markup (hidden until the machine writes text)" "1" "$(grep -c 'id="connStatus"[^>]*></div>' "$HTML")"
before_scripts="$(awk '/id="connStatus"/{a=NR} /<script src=/{if(!s)s=NR} END{print (a && s && a<s) ? "yes" : "no"}' "$HTML")"
assert_eq "it is in the markup BEFORE the scripts (they look it up at run time)" "yes" "$before_scripts"
rule="$(awk '/#connStatus\{/{f=1} f{print} f && /}/{exit}' "$CSS")"
assert_contains "CSS: fixed at the top" "$rule" "top:calc(.6rem"
assert_contains "CSS: above the d-pad (10) and Enter button (11), below the name overlay (20): z-index 15" "$rule" "z-index:15"
assert_contains "CSS: it never takes a tap" "$rule" "pointer-events:none"
assert_contains "CSS: the palette is the page's own variables" "$rule" "background:var(--paper); color:var(--ink)"
assert_contains "CSS: empty means hidden" "$(cat "$CSS")" "#connStatus:empty{ display:none; }"
assert_eq "the d-pad is anchored to the bottom, the badge to the top (they cannot overlap)" "bottom" "$(awk '/#dpad\{/{f=1} f && /bottom:/{print "bottom"; exit}' "$CSS")"
assert_not_contains "the badge does not use bottom:" "$rule" "bottom:"

echo "-- the state machine (netstatus_unit.js)"
# (Windows node under WSL: write to a file so $? is node's own exit code; ${PIPESTATUS[0]} after $(...| tr) would always be tr's 0.)
if [ "$WIN" = 1 ]; then "$NODE" "$(wslpath -w "$UNIT")" "$(wslpath -w "$NET")" > "$WORK/unit.out" 2>&1; rc=$?; out="$(tr -d '\r' < "$WORK/unit.out")"
else out="$("$NODE" "$UNIT" "$NET" 2>&1)"; rc=$?; fi
echo "$out" | sed 's/^/     /'
assert_eq "no unit case failed (node's exit code)" "0" "$rc"
assert_contains "and the unit's own summary says 0 failed" "$out" " checks, 0 failed"
n="$(sed -n 's/^netstatus_unit: \([0-9]*\) checks.*/\1/p' <<<"$out")"
[ "${n:-0}" -ge 30 ] && _ok "at least 30 checks executed ($n)" || _bad "at least 30 checks executed" "$n"
