#!/usr/bin/env bash
# Runs every test_*.sh in this directory, one at a time, and reports (moved from tests/repo/run_all.sh).
#   bash tests/repo/run_all.sh                 all of them
#   bash tests/repo/run_all.sh test_naming     just the named test(s)
# Needs: python3; nginx and openssl for test_nginx_www (a missing nginx is a skip here, a failure when CI=true).
cd "$(dirname "$0")" || exit 1
if [ "$#" -gt 0 ]; then tests=("$@"); else tests=($(ls test_*.sh | sed 's/\.sh$//' | sort)); fi
failed=()
start=$(date +%s)
for t in "${tests[@]}"; do
  echo "################ $t"
  t0=$(date +%s)
  if timeout "${TEST_TIMEOUT:-420}" bash "./${t%.sh}.sh"; then echo "---- $t OK ($(( $(date +%s) - t0 ))s)"; else echo "---- $t FAILED (exit $?)"; failed+=("$t"); fi
  echo
done
echo "================ ${#tests[@]} test files, ${#failed[@]} failed, $(( $(date +%s) - start ))s total"
if [ "${#failed[@]}" -gt 0 ]; then printf ' FAILED: %s\n' "${failed[@]}"; exit 1; fi
