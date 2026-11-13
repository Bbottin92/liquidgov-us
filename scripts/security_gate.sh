#!/usr/bin/env bash
# Security gate for liquidgov-platform (Brandon 2026-11-13: "start with the security gate"; decisions.md). One script for
# the laptop (WSL) and GitHub CI (.github/workflows/security.yml), like lgcheck.sh, so the two cannot drift apart.
#
# Steps (run all, or name some):
#   secrets   gitleaks over the WHOLE git history (every branch and tag the clone has), findings redacted
#   static    cppcheck over our own C++ (src/ + libs/, the vendored third_party_* libraries excluded)
#   sanitize  AddressSanitizer + UndefinedBehaviorSanitizer build, then every unit test and e2e suite via lgcheck.sh;
#             any sanitizer report file fails the step even when the suite itself passed
#   probe     start the server on a throwaway seeded store and run the OWASP ZAP baseline scan (spider + passive
#             rules) against it, signed out. Signed-in pages are NOT scanned yet (said in the output).
#
# Every step prints what it covered (commits, files, suites, URLs) and refuses a zero count: a scan that read
# nothing must not look like a clean scan. A tool that is missing is SKIP on the laptop but FAIL on CI.
# Output: one "GATE <step> PASS|FAIL|SKIP <detail>" line per step, findings in <out>/<step>/, last line EXIT=<n>.
#
# Usage: bash scripts/security_gate.sh <repo root> <out dir> [secrets|static|sanitize|probe ...]
# Tool versions are pinned in .github/workflows/security.yml (the CI installs them); locally whatever is on PATH.
set -uo pipefail
if [ $# -lt 2 ]; then sed -n '2,20p' "$0"; exit 2; fi
ROOT=$(readlink -f "$1"); OUT=$(mkdir -p "$2" && readlink -f "$2"); shift 2
STEPS=("$@"); [ ${#STEPS[@]} -gt 0 ] || STEPS=(secrets static sanitize probe)
SRC="$ROOT/liquidgov-platform"
[ -f "$SRC/CMakeLists.txt" ] || { echo "EXIT=no-source ($SRC has no CMakeLists.txt)"; exit 1; }
HERE=$(cd "$(dirname "$0")" && pwd)
ON_CI=${GITHUB_ACTIONS:-}
fail=0

result() { # step verdict detail
    echo "GATE $1 $2 $3"
    [ "$2" = PASS ] || [ "$2" = SKIP ] || fail=1
}
need() { # step tool -> 0 when present; otherwise SKIP (laptop) or FAIL (CI)
    command -v "$2" >/dev/null 2>&1 && return 0
    if [ -n "$ON_CI" ]; then result "$1" FAIL "($2 not installed on CI)"; else result "$1" SKIP "($2 not on PATH; runs on CI)"; fi
    return 1
}

step_secrets() {
    need secrets gitleaks || return
    local d="$OUT/secrets" commits rc
    mkdir -p "$d"
    commits=$(git -C "$ROOT" rev-list --all --count 2>/dev/null) || { result secrets FAIL "(git rev-list failed)"; return; }
    [ "${commits:-0}" -gt 1 ] || { result secrets FAIL "(only ${commits:-0} commit(s) in the clone: shallow checkout? need fetch-depth 0)"; return; }
    # --log-opts=--all: every ref, not just HEAD. --redact: the report never holds the secret itself.
    gitleaks git --redact --no-banner --log-opts="--all" --report-format json --report-path "$d/gitleaks.json" "$ROOT" \
        > "$d/gitleaks.log" 2>&1; rc=$?
    tail -3 "$d/gitleaks.log"
    local n
    n=$(python3 -c 'import json,sys; print(len(json.load(open(sys.argv[1]))))' "$d/gitleaks.json" 2>/dev/null) \
        || { result secrets FAIL "(gitleaks exit $rc and no readable report)"; return; }
    # gitleaks reads <repo>/.gitleaksignore by itself: the triaged false positives, one fingerprint per line
    local ignored=0
    [ -f "$ROOT/.gitleaksignore" ] && ignored=$(grep -cvE '^\s*(#|$)' "$ROOT/.gitleaksignore")
    if [ "$rc" = 0 ] && [ "$n" = 0 ]; then result secrets PASS "(commits scanned: $commits, new findings: 0, triaged and ignored: $ignored)"
    else
        python3 - "$d/gitleaks.json" <<'EOF'
import json, sys
for f in json.load(open(sys.argv[1])):
    print("  LEAK %s %s:%s commit %s" % (f.get("RuleID"), f.get("File"), f.get("StartLine"), str(f.get("Commit"))[:8]))
EOF
        result secrets FAIL "(commits scanned: $commits, findings: $n, gitleaks exit $rc)"
    fi
}

step_static() {
    need static cppcheck || return
    local d="$OUT/static" files incs rc
    mkdir -p "$d"
    find "$SRC/src" "$SRC/libs" \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' \) -not -path '*/third_party_*' \
        | sort > "$d/files.txt"
    files=$(wc -l < "$d/files.txt")
    [ "$files" -gt 0 ] || { result static FAIL "(no C++ files found under $SRC)"; return; }
    incs=(); for i in "$SRC"/libs/*/include "$SRC/src"; do [ -d "$i" ] && incs+=("-I$i"); done
    # warning/portability/performance, not style: style findings are not security findings and would bury them.
    # missingIncludeSystem: cppcheck does not need the system headers to find these bugs.
    cppcheck --std=c++17 --language=c++ --enable=warning,portability,performance --inline-suppr \
        --suppress=missingIncludeSystem --error-exitcode=1 -j2 "${incs[@]}" \
        --template='{file}:{line}: {severity}: {id}: {message}' --file-list="$d/files.txt" \
        > "$d/cppcheck.log" 2> "$d/findings.txt"; rc=$?
    local checked n
    checked=$(grep -c '^Checking ' "$d/cppcheck.log")
    n=$(grep -cE ': (error|warning|portability|performance): ' "$d/findings.txt")
    [ "$checked" -gt 0 ] || { result static FAIL "(cppcheck checked 0 files; exit $rc)"; return; }
    if [ "$rc" = 0 ] && [ "$n" = 0 ]; then result static PASS "(files listed: $files, checked: $checked, findings: 0)"
    else
        grep -E ': (error|warning|portability|performance): ' "$d/findings.txt" | head -40 | sed 's/^/  /'
        result static FAIL "(files listed: $files, checked: $checked, findings: $n, cppcheck exit $rc)"
    fi
}

step_sanitize() {
    need sanitize cmake || return
    local d="$OUT/sanitize" b="$OUT/sanitize/build" rc
    mkdir -p "$d/reports"
    rm -f "$d"/reports/*
    local flags="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined"
    cmake -S "$SRC" -B "$b" -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="$flags" -DCMAKE_C_FLAGS="$flags" \
        -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" > "$d/configure.log" 2>&1 \
        || { result sanitize FAIL "(configure failed: $d/configure.log)"; tail -5 "$d/configure.log"; return; }
    # Build first: lgcheck.sh refuses a "previous build" dir that has no binaries yet (first CI run, 43503116478:
    # EXIT=no-previous-build). -j2 on CI (two cores); -j1 anywhere else (the laptop's -j1 rule).
    local jobs=1; [ -n "$ON_CI" ] && jobs=2
    cmake --build "$b" -j"$jobs" > "$d/build.log" 2>&1 \
        || { result sanitize FAIL "(sanitizer build failed: $d/build.log)"; grep -m5 -E 'error' "$d/build.log"; return; }
    # The servers the suites start inherit these: every report lands in a file named <prefix>.<pid>, so a report
    # from a server whose output the suite threw away is still seen here.
    export ASAN_OPTIONS="log_path=$d/reports/asan:detect_leaks=1:halt_on_error=1:strict_string_checks=1:detect_stack_use_after_return=1"
    export UBSAN_OPTIONS="log_path=$d/reports/ubsan:print_stacktrace=1:halt_on_error=1"
    # lgcheck.sh keeps the sanitizer flags (they are in the build dir's CMake cache). GITHUB_ACTIONS is cleared for
    # it only to skip its extra no-PCH compile, which is a build check, not a sanitizer one.
    env -u GITHUB_ACTIONS bash "$HERE/lgcheck.sh" "$SRC" "$b" "$b" > "$d/lgcheck.txt" 2>&1; rc=$?
    grep -E '^(PASS|FAIL|SKIP|ran |EXIT=)' "$d/lgcheck.txt" | tail -60 | sed 's/^/  /'
    local units suites reports last
    units=$(grep -cE '^(PASS|FAIL) liquidgov_platform_.*_test$' "$d/lgcheck.txt")
    suites=$(sed -n 's/^ran \([0-9]*\) suites$/\1/p' "$d/lgcheck.txt" | tail -1)
    last=$(tail -1 "$d/lgcheck.txt")
    reports=$(find "$d/reports" -type f | wc -l)
    for r in "$d"/reports/*; do [ -f "$r" ] && { echo "  REPORT $(basename "$r"):"; grep -m3 -E 'ERROR|runtime error|SUMMARY' "$r" | sed 's/^/    /'; }; done
    [ "$units" -gt 0 ] && [ "${suites:-0}" -gt 0 ] \
        || { result sanitize FAIL "(ran unit tests: $units, e2e suites: ${suites:-0}; lgcheck said: $last)"; return; }
    if [ "$last" = "EXIT=0" ] && [ "$reports" = 0 ]; then
        result sanitize PASS "(unit tests: $units, e2e suites: $suites, sanitizer reports: 0)"
    else
        result sanitize FAIL "(unit tests: $units, e2e suites: $suites, sanitizer reports: $reports, lgcheck: $last)"
    fi
}

step_probe() {
    need probe docker || return
    local d="$OUT/probe" bin seed port=18190 w rc server
    # Outside naming.toml's e2e range (18080-18139) so no suite ever claims it.
    mkdir -p "$d"; chmod 777 "$d"   # the ZAP container writes its reports here as its own user
    bin=$(ls "$OUT"/sanitize/build/liquidgov_http "$OUT"/build/liquidgov_http 2>/dev/null | head -1)
    if [ -z "$bin" ]; then
        cmake -S "$SRC" -B "$OUT/build" -DCMAKE_BUILD_TYPE=Debug > "$d/configure.log" 2>&1 \
            && cmake --build "$OUT/build" --target liquidgov_http liquidgov_e2e_seed -j2 > "$d/build.log" 2>&1 \
            || { result probe FAIL "(build failed: $d/build.log)"; return; }
        bin="$OUT/build/liquidgov_http"
    fi
    seed="$(dirname "$bin")/liquidgov_e2e_seed"
    [ -x "$bin" ] && [ -x "$seed" ] || { result probe FAIL "(no liquidgov_http / liquidgov_e2e_seed next to $bin)"; return; }
    if curl -s -o /dev/null --max-time 2 "http://127.0.0.1:$port/"; then result probe FAIL "(port $port is busy; refusing)"; return; fi
    w=$(mktemp -d); mkdir -p "$w/blobs"
    "$seed" "$w/store.lgp" > "$w/sessions.txt" || { result probe FAIL "(seed failed)"; return; }
    (cd "$SRC" && exec env -u LIQUIDGOV_OPERATORS LIQUIDGOV_STORE="$w/store.lgp" LIQUIDGOV_BLOBS="$w/blobs" \
        LIQUIDGOV_PORT=$port "$bin") > "$d/server.log" 2>&1 &
    server=$!
    local up=""
    for _ in $(seq 100); do
        kill -0 $server 2>/dev/null || break
        curl -s -o /dev/null --max-time 1 "http://127.0.0.1:$port/" && { up=1; break; }
        sleep 0.2
    done
    if [ -z "$up" ]; then kill $server 2>/dev/null; result probe FAIL "(server did not start: $d/server.log)"; tail -5 "$d/server.log"; return; fi
    # zap-baseline.py: spider for 1 minute, then passive rules only (no attack traffic). Exit 0 clean, 1 FAIL-level,
    # 2 WARN-level, 3 error. Both 1 and 2 fail this step until the triage decides which rules matter (report-only now).
    docker run --rm --network host -v "$d:/zap/wrk:rw" "${ZAP_IMAGE:-zaproxy/zap-stable:2.16.1}" \
        zap-baseline.py -t "http://127.0.0.1:$port/" -m 1 -J zap.json -r zap.html > "$d/zap.log" 2>&1; rc=$?
    kill $server 2>/dev/null; wait $server 2>/dev/null
    rm -rf "$w"
    grep -E '^(WARN-NEW|FAIL-NEW|FAIL-INPROG|WARN-INPROG)' "$d/zap.log" | head -40 | sed 's/^/  /'
    [ -f "$d/zap.json" ] || { result probe FAIL "(ZAP exit $rc and no report: $d/zap.log)"; tail -5 "$d/zap.log"; return; }
    local urls summary
    urls=$(python3 -c '
import json,sys
d=json.load(open(sys.argv[1]))
u=set()
for s in d.get("site",[]):
    for a in s.get("alerts",[]):
        for i in a.get("instances",[]): u.add(i.get("uri"))
print(len(u))' "$d/zap.json" 2>/dev/null)
    summary=$(grep -E '^FAIL-NEW: [0-9]+' "$d/zap.log" | tail -1)
    [ -n "$summary" ] || { result probe FAIL "(ZAP exit $rc but no summary line: the scan may not have run; $d/zap.log)"; return; }
    if [ "$rc" = 0 ]; then result probe PASS "(signed-out scan clean; $summary; signed-in pages not scanned)"
    else result probe FAIL "(ZAP exit $rc; $summary; URLs with alerts: ${urls:-?}; signed-in pages not scanned)"; fi
}

for s in "${STEPS[@]}"; do
    case "$s" in
        secrets|static|sanitize|probe) echo "== $s $(date -Is)"; "step_$s" ;;
        *) echo "unknown step $s"; fail=1 ;;
    esac
done
echo "EXIT=$fail"
exit $fail
