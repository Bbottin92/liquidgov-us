#!/usr/bin/env bash
# CI gate for liquidgov.us deploys (Brandon 2026-11-08: "yes build the gate"). Every generated deploy/*-go.sh calls
# this before it changes anything: a deploy goes on only if the platform CI job (.github/workflows/platform.yml)
# passed for exactly the liquidgov-platform/ tree being deployed. Before this, CI ran only after a deploy (the order
# was deploy -> merge -> push), so it could report a broken change only once visitors had it.
#
# Matched by TREE, not by commit: deploy/sync-to-droplet.sh ships liquidgov-platform/, and the commit that adds the
# generated go/remote pair touches only deploy/ and naming.toml, which the platform job does not run for (its paths
# filter). So the newest platform run whose head commit has the same liquidgov-platform tree hash as HEAD decides:
# completed + success -> CI_GATE_OK; failure, still running, or no run at all -> refused. A gh or git error refuses too
# (fails closed).
# Usage: bash deploy/ci-gate.sh <go|dry>
#   go:  by default (block, Brandon 2026-11-20) it exits 0 only on CI_GATE_OK (or an override), else CI_GATE_REFUSED and
#        exit 1. With LIQUIDGOV_CI_GATE_MODE=warn (the default from 2026-11-16 to 2026-11-20) a missing or failed CI
#        result prints CI_GATE_WARNING and exits 0.
#   dry: prints what go would decide (CI_GATE_OK or CI_GATE_WOULD_REFUSE) and always exits 0.
# Override, emergencies only and only with Brandon's explicit OK for that deploy (e.g. a rollback while GitHub is down):
#   LIQUIDGOV_CI_OVERRIDE="<reason>" -> prints CI_GATE_OVERRIDE: <reason> into the deploy log and lets it through.
# Test seams: LIQUIDGOV_GH (the gh command), LIQUIDGOV_GATE_REPO (the repo root); tests: tests/repo/ci_gate_check.py
set -uo pipefail
MODE="${1:-}"
if [ "$MODE" != "go" ] && [ "$MODE" != "dry" ]; then echo "usage: bash deploy/ci-gate.sh <go|dry>"; exit 2; fi
REPO="${LIQUIDGOV_GATE_REPO:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
GH_REPO="Bbottin92/liquidgov-us"

decide() { # $1 = OK|REFUSED, $2 = why
    if [ "$1" = OK ]; then echo "CI_GATE_OK $2"; exit 0; fi
    if [ "$MODE" = dry ]; then echo "CI_GATE_WOULD_REFUSE $2"; exit 0; fi
    if [ "${LIQUIDGOV_CI_GATE_MODE:-block}" = warn ]; then
        # Opt-in only. Warn was the default from 2026-11-16 (CI was off, nobody on the site yet) until 2026-11-20, when CI
        # ran on every push again and Brandon chose block as the default ("you lead the way").
        echo "CI_GATE_WARNING $2 -- not blocking (LIQUIDGOV_CI_GATE_MODE=warn; unset it to make this a stop)"
        exit 0
    fi
    echo "CI_GATE_REFUSED $2"
    echo "Run python3 scripts/local_ci.py, commit and push its deploy/ci-pass record (or let a GitHub platform run pass), then run go again."
    exit 1
}

# git_in_repo: WSL git, then git.exe through interop with retries (deploy/git-in-repo.sh, shared with sync-to-droplet.sh)
# shellcheck source=deploy/git-in-repo.sh
. "$(dirname "${BASH_SOURCE[0]}")/git-in-repo.sh"

if [ -n "${LIQUIDGOV_CI_OVERRIDE:-}" ]; then
    echo "CI_GATE_OVERRIDE: $LIQUIDGOV_CI_OVERRIDE"
    exit 0
fi

TREE="$(git_in_repo rev-parse "HEAD:liquidgov-platform")" || decide REFUSED "(cannot read the liquidgov-platform tree of HEAD in $REPO)"
[ -n "$TREE" ] || decide REFUSED "(empty liquidgov-platform tree hash for HEAD)"
DIRTY="$(git_in_repo status --porcelain -- liquidgov-platform)" || decide REFUSED "(cannot read git status in $REPO)"
[ -z "$DIRTY" ] || decide REFUSED "(uncommitted changes under liquidgov-platform/: CI never saw them)"

# A local CI record (Brandon 2026-11-15: "can we skip github if it costs money now?"): scripts/local_ci.py runs the
# same workflows here and, when everything that gates passes, writes deploy/ci-pass/<this tree>.txt. A committed record
# for exactly this liquidgov-platform tree counts as the platform run passing; otherwise GitHub is asked, as before.
REC="$(git_in_repo show "HEAD:deploy/ci-pass/$TREE.txt" 2>/dev/null | head -1 | tr -d '\r')"
if [ "$REC" = "LIQUIDGOV_LOCAL_CI PASS" ]; then decide OK "(local CI record deploy/ci-pass/$TREE.txt)"; fi

if [ -n "${LIQUIDGOV_GH:-}" ]; then GH="$LIQUIDGOV_GH"
elif command -v gh >/dev/null 2>&1; then GH=gh
elif command -v gh.exe >/dev/null 2>&1; then GH=gh.exe
else decide REFUSED "(no gh command: cannot ask GitHub for the CI result)"; fi

# Newest first. --repo: gh inside WSL cannot find the repo through this worktree's Windows .git path.
RUNS="$($GH run list --repo "$GH_REPO" --workflow platform.yml --limit 100 \
        --json databaseId,headSha,status,conclusion \
        -q '.[] | "\(.databaseId) \(.headSha) \(.status) \(.conclusion)"' 2>&1)" \
    || decide REFUSED "(gh run list failed: $(printf '%s' "$RUNS" | head -c 200))"
RUNS="${RUNS//$'\r'/}"

while read -r id sha status concl; do
    [ -n "${id:-}" ] || continue
    # One quick try first: most runs belong to other commits, and a commit this clone lacks always fails.
    if ! t="$(LIQUIDGOV_GIT_TRIES=1 git_in_repo rev-parse "$sha:liquidgov-platform" 2>/dev/null)"; then
        # A failure may be a dropped interop call, not a missing commit: if git no longer answers at all, refuse
        # (2026-11-12: a dropped read made a green run look like "no platform CI run").
        git_in_repo rev-parse --verify -q HEAD >/dev/null \
            || decide REFUSED "(git stopped answering while reading the tree of run $id on ${sha:0:7})"
        t="$(git_in_repo rev-parse "$sha:liquidgov-platform" 2>/dev/null)" || continue # err-ok: git answers HEAD, so this commit is missing from the clone and cannot match
    fi
    [ "$t" = "$TREE" ] || continue
    if [ "$status" = completed ] && [ "${concl:-}" = success ]; then
        decide OK "platform run $id passed for commit ${sha:0:7} (liquidgov-platform tree ${TREE:0:12})"
    fi
    decide REFUSED "(the newest platform run for this tree, $id on ${sha:0:7}, is status=$status conclusion=${concl:-none})"
done <<< "$RUNS"
decide REFUSED "(no platform CI run for this liquidgov-platform tree ${TREE:0:12}: push the branch first)"
