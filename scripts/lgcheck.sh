#!/usr/bin/env bash
# The full local check for a liquidgov-platform branch (run inside WSL). Replaces the per-task copies
# (scratch lgcheck-juketube.sh, lgcheck-care.sh: three hand-copies on 2026-11-08, each changing only paths), and the
# inbox proposal "commit a run-all-e2e script" (2026-11-08 01:07: two hand-written runners each had a flaw).
#   compile -j1 (7.4 GB laptop; 2026-11-07 lock-up) -> every unit test -> every e2e suite, each judged by its OWN
#   passes=/fails=/EXIT= line, not only its exit code -> each NEW suite run again on the PREVIOUS build, where it must
#   FAIL (a check that cannot fail proves nothing).
# Suites run from CR-stripped copies in <out>/tree/tests/e2e, with <out>/tree/assets linked to the real assets,
# because several suites start the server in $(dirname $0)/../.. . city_undo is skipped (it needs an old binary and
# the undo script; run it by hand for an LGP1 format change).
# Usage: bash scripts/lgcheck.sh <liquidgov-platform dir> <build dir> <previous build dir> [new_suite ...]
#   e.g. bash scripts/lgcheck.sh /mnt/c/Users/Brandon/liquidgov-us-x/liquidgov-platform ~/lg-x-check ~/lg-care-check/new city_moderation
# Last line: EXIT=0 only when everything passed and every new suite failed on the previous build.
set -uo pipefail
if [ $# -lt 3 ]; then sed -n '2,13p' "$0"; exit 2; fi
SRC=$(readlink -f "$1"); OUT=$2; OLD=$3; shift 3; NEW=("$@")
TREE="$OUT/tree"
[ -f "$SRC/CMakeLists.txt" ] || { echo "EXIT=no-source ($SRC has no CMakeLists.txt)"; exit 1; }
[ -x "$OLD/liquidgov_http" ] && [ -x "$OLD/liquidgov_e2e_seed" ] || { echo "EXIT=no-previous-build ($OLD)"; exit 1; }
for n in "${NEW[@]}"; do [ -f "$SRC/tests/e2e/$n.sh" ] || { echo "EXIT=no-such-suite $n"; exit 1; }; done
mkdir -p "$OUT" "$TREE/tests/e2e"
echo "== compile $(date -Is) $SRC"
cmake -S "$SRC" -B "$OUT" -DCMAKE_BUILD_TYPE=Debug >/dev/null || { echo "EXIT=configure-fail"; exit 1; }
cmake --build "$OUT" -j1 2>&1 | grep -E "error|warning: unused|Error|\[100%\]" | head -60
[ "${PIPESTATUS[0]}" = 0 ] || { echo "EXIT=compile-fail"; exit 1; }
BIN="$OUT/liquidgov_http"; SEED="$OUT/liquidgov_e2e_seed"
[ -x "$BIN" ] && [ -x "$SEED" ] || { echo "EXIT=no-binaries"; exit 1; }
# the instrument is fresh: the binary is newer than every source file it was built from
newest=$(find "$SRC/src" "$SRC/libs" -name '*.cpp' -o -name '*.hpp' | xargs ls -t | head -1)
[ "$BIN" -nt "$newest" ] || { echo "EXIT=binary-older-than-source ($newest)"; exit 1; }
ln -sfn "$SRC/assets" "$TREE/assets"
[ -d "$TREE/assets" ] || { echo "EXIT=assets-link-broken"; exit 1; }
fail=0
echo "== unit tests"
n=0
for t in "$OUT"/liquidgov_platform_*_test; do
    [ -x "$t" ] || continue; n=$((n+1))
    if "$t" > "$OUT/$(basename "$t").out" 2>&1; then echo "PASS $(basename "$t")"; else echo "FAIL $(basename "$t")"; tail -5 "$OUT/$(basename "$t").out"; fail=1; fi
done
[ "$n" -gt 0 ] || { echo "FAIL no unit tests found"; fail=1; }
judge() { # file rc: the suite's own last summary line decides, plus its exit code
    local f=$1 rc=$2 last
    last=$(grep -E '^(passes=[0-9]+ fails=|EXIT=|---- [0-9]+ failed)' "$f" | tail -1)
    [ "$rc" = 0 ] || return 1
    case "$last" in
        *"fails=0"|"EXIT=0"|"---- 0 failed"*) return 0 ;;
        "") grep -q '^FAIL' "$f" && return 1; return 0 ;;
        *) return 1 ;;
    esac
}
echo "== e2e suites"
n=0
for py in "$SRC"/tests/e2e/*.py; do [ -f "$py" ] && tr -d '\r' < "$py" > "$TREE/tests/e2e/$(basename "$py")"; done
for s in "$SRC"/tests/e2e/*.sh; do
    name=$(basename "$s" .sh)
    [ "$name" = city_undo ] && { echo "SKIP city_undo (needs old binary + undo script; run by hand for an LGP1 change)"; continue; }
    n=$((n+1)); tr -d '\r' < "$s" > "$TREE/tests/e2e/$name.sh"
    timeout 600 bash "$TREE/tests/e2e/$name.sh" "$BIN" "$SEED" > "$OUT/e2e-$name.out" 2>&1; rc=$?
    if judge "$OUT/e2e-$name.out" $rc; then echo "PASS $name ($(grep -E '^(passes=|EXIT=|----|[a-z_]+: passes)' "$OUT/e2e-$name.out" | tail -2 | tr '\n' ' '))"
    else echo "FAIL $name rc=$rc"; grep -E '^FAIL' "$OUT/e2e-$name.out" | head -12; fail=1; fi
done
echo "ran $n suites"
[ "$n" -gt 0 ] || { echo "FAIL no e2e suites ran"; fail=1; }
for name in "${NEW[@]}"; do
    echo "== $name on the previous build (must fail)"
    timeout 600 bash "$TREE/tests/e2e/$name.sh" "$OLD/liquidgov_http" "$OLD/liquidgov_e2e_seed" > "$OUT/old-$name.out" 2>&1; rc=$?
    if judge "$OUT/old-$name.out" $rc; then echo "BAD $name passes on the previous build: it cannot tell old from new"; fail=1
    else echo "GOOD $name fails on the previous build: $(grep -c '^FAIL' "$OUT/old-$name.out") FAIL lines"; grep '^FAIL' "$OUT/old-$name.out" | head -8; fi
done
echo "EXIT=$fail"
