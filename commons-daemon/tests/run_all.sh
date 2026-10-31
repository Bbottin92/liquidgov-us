#!/usr/bin/env bash
# Runs every test_*.sh in this directory, one at a time (fixed ports), and reports.
#   ./run_all.sh                  all tests against the civic stub (what CI does)
#   CIVIC_MODE=real ./run_all.sh  the same tests against the real liquidgov_http (fidelity check)
#   ./run_all.sh test_guest       just the named test(s)
# Needs: the built daemon (../build/commons_daemon), python3, curl.
cd "$(dirname "$0")" || exit 1
if [ ! -x "${COMMONS_DAEMON_BIN:-../build/commons_daemon}" ]; then
  echo "daemon binary not found (build it first, or set COMMONS_DAEMON_BIN)"; exit 2
fi
if [ "$#" -gt 0 ]; then tests=("$@"); else tests=($(ls test_*.sh | sed 's/\.sh$//' | sort)); fi
failed=()
start=$(date +%s)
for t in "${tests[@]}"; do
  echo "################ $t (civic: ${CIVIC_MODE:-stub})"
  t0=$(date +%s)
  # a hard limit per test file: a regression that hangs the daemon must FAIL, not stall the job
  if timeout "${TEST_TIMEOUT:-420}" bash "./${t%.sh}.sh"; then echo "---- $t OK ($(( $(date +%s) - t0 ))s)"; else echo "---- $t FAILED (exit $?)"; failed+=("$t"); fi
  # nothing may outlive its test file (fixed ports): sweep leftovers whatever the result
  pkill -x commons_daemon 2>/dev/null; pkill -f "civic_stub.py --port" 2>/dev/null; sleep 0.3
  echo
done
echo "================ ${#tests[@]} test files, ${#failed[@]} failed, $(( $(date +%s) - start ))s total"
if [ "${#failed[@]}" -gt 0 ]; then printf ' FAILED: %s\n' "${failed[@]}"; exit 1; fi
