#!/usr/bin/env bash
# Prebuilt deploys (deploy/PLAN-prebuilt.md), PC side, before a go: fetch the program GitHub built for exactly this
# liquidgov-platform tree and check it. No server contact. Leaves liquidgov-platform/.prebuilt/{liquidgov_http,
# liquidgov_http.sha256,BUILD-INFO}, which the normal sync then copies to the server.
# Refuses unless: a successful release-binary run exists whose head commit has HEAD's liquidgov-platform tree hash
# (the same rule as deploy/ci-gate.sh); the artifact's sha256 matches; BUILD-INFO's tree is HEAD's tree; and the
# program's own --version prints that tree.
# Usage: bash deploy/fetch-prebuilt.sh   -> last line PREBUILT_OK <tree> (exit 0) or PREBUILT_REFUSED <why> (exit 1)
# Test seams: LIQUIDGOV_GH (the gh command), LIQUIDGOV_GATE_REPO (repo root). Tests: tests/repo/prebuilt_check.py
set -uo pipefail
REPO="${LIQUIDGOV_GATE_REPO:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
GH=${LIQUIDGOV_GH:-gh}
GH_REPO="Bbottin92/liquidgov-us"
refuse() { echo "PREBUILT_REFUSED $1"; exit 1; }
TREE=$(git -C "$REPO" rev-parse HEAD:liquidgov-platform 2>/dev/null) || refuse "git cannot read HEAD's liquidgov-platform tree"
OUT="$REPO/liquidgov-platform/.prebuilt"
runs=$($GH run list --repo "$GH_REPO" --workflow release-binary.yml --status success --limit 30 \
       --json databaseId,headSha -q '.[] | "\(.databaseId) \(.headSha)"' 2>/dev/null) || refuse "gh could not list release-binary runs"
RID=""
while read -r id sha; do
    [ -n "$id" ] || continue
    t=$(git -C "$REPO" rev-parse "$sha:liquidgov-platform" 2>/dev/null) || continue
    if [ "$t" = "$TREE" ]; then RID=$id; break; fi
done <<< "$runs"
[ -n "$RID" ] || refuse "no successful release-binary run for tree $TREE (merge to master and let it build, or use the compile path)"
rm -rf "$OUT" && mkdir -p "$OUT" || refuse "cannot make $OUT"
$GH run download "$RID" --repo "$GH_REPO" -n "liquidgov_http-$TREE" -D "$OUT" >/dev/null 2>&1 || refuse "download of run $RID failed"
( cd "$OUT" && sha256sum -c --quiet liquidgov_http.sha256 ) >/dev/null 2>&1 || refuse "checksum mismatch"
grep -qx "tree=$TREE" "$OUT/BUILD-INFO" || refuse "BUILD-INFO is for another tree: $(grep '^tree=' "$OUT/BUILD-INFO")"
chmod 755 "$OUT/liquidgov_http"
v=$("$OUT/liquidgov_http" --version 2>/dev/null) || refuse "the program did not run here (--version)"
case "$v" in *" tree $TREE") ;; *) refuse "--version says '$v'";; esac
echo "run $RID: $(grep -E '^(commit|os|glibc_needed)=' "$OUT/BUILD-INFO" | tr '\n' ' ')"
echo "PREBUILT_OK $TREE"
