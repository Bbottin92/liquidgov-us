#!/usr/bin/env bash
# Builds the daemon on the droplet: `cmake --build build -j1` (a 512MB box: -j1 avoids an OOM/GCC-ICE; there is no
# override), then chowns the tree to the service user. NO reconfigure unless asked: a configure re-runs
# FetchContent (sqlite.org) and is only needed when CMakeLists.txt changed. deploy-daemon.sh passes --reconfigure
# for exactly that case. Does not restart the service.
#
# Usage: build-on-droplet.sh [--reconfigure]
set -euo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

reconf=0
for a in "$@"; do
  case "$a" in
    --reconfigure) reconf=1;;
    -h|--help) sed -n '2,9p' "$0"; exit 0;;
    *) die "unknown argument: $a";;
  esac
done

d="$(q "$DAEMON_DIR")"
if [ "$reconf" = 1 ]; then
  say "configuring (CMake input changed)"
  remote "cmake -S $d -B $d/build"
else
  remote "test -f $d/build/CMakeCache.txt" || die "no configured build dir at $DAEMON_DIR/build: pass --reconfigure (first install?)"
fi
say "building with -j1"
remote "cmake --build $d/build -j1"
remote "chown -R $(q "$SERVICE_USER:$SERVICE_USER") $d"
say "built."
