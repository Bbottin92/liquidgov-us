#!/usr/bin/env bash
# Rebuild timings for a finished liquidgov-platform build (Brandon 2026-11-13: "every deploy that adds a record type or
# touches a feature should recompile only that feature's files"; measure before/after from CI's own logs).
# Touches (mtime only, contents unchanged) one site feature file, one store record module and one record header, and
# times the -j1 rebuild of liquidgov_http after each (-j1: the small server builds one file at a time). Prints one
# "TIMING <what> <seconds>s <n> recompiled" line per case. Called by scripts/lgcheck.sh after its compile step.
# Usage: bash scripts/build_timing.sh <liquidgov-platform dir> <build dir>
set -uo pipefail
SRC=$1; OUT=$2
one() { # what file
    local what=$1 f=$2
    [ -f "$SRC/$f" ] || { echo "TIMING $what skipped (no $f)"; return; }
    touch "$SRC/$f"
    local t0=$(date +%s.%N)
    local n=$(cmake --build "$OUT" --target liquidgov_http -j1 2>&1 | grep -cE "Building CXX object")
    local t1=$(date +%s.%N)
    printf 'TIMING %s %.1fs %d recompiled\n' "$what" "$(python3 -c "print($t1 - $t0)")" "$n"
}
one feature-cpp src/site/features/styles/styles.cpp
one store-record-cpp libs/liquidgov_file_store/src/records/here.cpp
one store-record-header libs/liquidgov_file_store/include/liquidgov/platform/records/styles.hpp
one store-class-header libs/liquidgov_file_store/include/liquidgov/platform/file_store.hpp
