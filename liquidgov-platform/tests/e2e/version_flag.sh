#!/usr/bin/env bash
# liquidgov_http --version (Brandon 2026-11-13, prebuilt deploys: deploy/PLAN-prebuilt.md): prints
# "liquidgov_http <commit> tree <tree>" and exits 0 without starting the server (a local build says "unknown"), so a
# deploy can check the program it is about to install is the one GitHub built for this tree.
# Usage: bash version_flag.sh <path to liquidgov_http> <path to liquidgov_e2e_seed>
# Port 18129 (to prove no server starts). Never touches the live site.
set -u
BIN=$(readlink -f "$1")
W=/tmp/lg-version-flag; rm -rf "$W"; mkdir -p "$W/blobs"
PORT=18129; URL=http://127.0.0.1:$PORT
fails=0; passes=0
ok()  { echo "PASS $1"; passes=$((passes+1)); }
bad() { echo "FAIL $1"; fails=$((fails+1)); }
check() { if eval "$2"; then ok "$1"; else bad "$1 ($3)"; fi; }
if curl -s -o /dev/null --max-time 2 $URL/; then echo "port $PORT is busy; refusing"; exit 2; fi
[ -x "$BIN" ] || { echo "missing binary"; exit 2; }
out=$(cd "$W" && LIQUIDGOV_STORE=$W/store.lgp LIQUIDGOV_BLOBS=$W/blobs LIQUIDGOV_PORT=$PORT timeout 5 "$BIN" --version 2>&1); rc=$?
check "--version exits 0 on its own (no timeout)" '[ $rc = 0 ]' "rc=$rc"
check "--version prints 'liquidgov_http <commit> tree <tree>'" '[[ "$out" =~ ^liquidgov_http\ [0-9a-f]{7,40}\ tree\ [0-9a-f]{7,40}$ || "$out" = "liquidgov_http unknown tree unknown" ]]' "$out"
check "--version did not create a store (no server started)" '[ ! -e $W/store.lgp ]' "store written"
check "control: nothing is listening on the port" '! curl -s -o /dev/null --max-time 1 $URL/' "a server answered"
echo "passes=$passes fails=$fails"
[ $fails = 0 ]
