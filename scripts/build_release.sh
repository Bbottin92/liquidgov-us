#!/usr/bin/env bash
# Build the production program the way a prebuilt deploy ships it (deploy/PLAN-prebuilt.md): Release, stripped,
# libstdc++/libgcc (and libsodium when libsodium.a exists) linked in, the commit and liquidgov-platform tree hash baked
# in (liquidgov_http --version). Writes <out>/liquidgov_http, <out>/liquidgov_http.sha256 and <out>/BUILD-INFO.
# Usage: bash scripts/build_release.sh [<out dir, default dist>] [<build dir, default build-release>]
# Exit 0 and last line RELEASE_BUILT <tree>, or RELEASE_FAILED <why>.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-$ROOT/dist}; BUILD=${2:-$ROOT/build-release}
fail() { echo "RELEASE_FAILED $1"; exit 1; }
COMMIT=$(git -C "$ROOT" rev-parse HEAD) || fail "git rev-parse HEAD"
TREE=$(git -C "$ROOT" rev-parse HEAD:liquidgov-platform) || fail "git rev-parse HEAD:liquidgov-platform"
[ -z "$(git -C "$ROOT" status --porcelain -- liquidgov-platform)" ] || fail "liquidgov-platform/ has uncommitted changes (the tree hash would not match the program)"
cmake -S "$ROOT/liquidgov-platform" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DLG_STATIC_RUNTIME=ON \
      -DLG_BUILD_COMMIT="$COMMIT" -DLG_BUILD_TREE="$TREE" >/dev/null || fail "cmake configure"
cmake --build "$BUILD" --target liquidgov_http -j"$(nproc)" || fail "build"
mkdir -p "$OUT"
cp "$BUILD/liquidgov_http" "$OUT/liquidgov_http" && strip "$OUT/liquidgov_http" || fail "copy/strip"
got=$("$OUT/liquidgov_http" --version) || fail "--version did not run"
[ "$got" = "liquidgov_http $COMMIT tree $TREE" ] || fail "--version says '$got'"
GLIBC=$(objdump -T "$OUT/liquidgov_http" | grep -oE 'GLIBC_[0-9]+(\.[0-9]+)*' | sed 's/GLIBC_//' | sort -V | tail -1)
OS=$(. /etc/os-release 2>/dev/null; echo "${PRETTY_NAME:-unknown}")
SHARED=$(ldd "$OUT/liquidgov_http" | awk '{print $1}' | tr '\n' ' ')
( cd "$OUT" && sha256sum liquidgov_http > liquidgov_http.sha256 ) || fail "sha256sum"
printf 'commit=%s\ntree=%s\nos=%s\nglibc_needed=%s\nshared_libs=%s\n' "$COMMIT" "$TREE" "$OS" "$GLIBC" "$SHARED" > "$OUT/BUILD-INFO"
cat "$OUT/BUILD-INFO"
echo "RELEASE_BUILT $TREE"
