#!/usr/bin/env bash
# Syncs liquidgov-platform/ source to the production droplet's /opt/liquidgov
# (which is NOT a git clone -- see the repo's other deploy docs). Does not
# build, chown, or restart the service; run those separately afterward.
#
# Why this script exists, not a hand-typed rsync command: on 2026-09-12 a
# hand-typed `rsync -az --exclude build --exclude .git ./ root@...:/opt/liquidgov/`
# silently overwrote the live liquidgov_accounts.lgp (and would have done the
# same to liquidgov_blobs/) with a stale local copy that happened to be
# sitting inside liquidgov-platform/ from an earlier local test run -- real
# account data was lost with no backup anywhere. The source tree and the
# live data directory share a parent folder, so "sync everything except
# build and .git" is never actually safe here. This script hardcodes every
# exclusion that matters so a future deploy can't reintroduce that mistake
# by hand-typing an rsync command from memory.
#
# Usage: deploy/sync-to-droplet.sh [user@]host[:path]
#   defaults to root@143.198.159.58:/opt/liquidgov/
#   LIQUIDGOV_DEPLOY_DRY_RUN=1  list what would change, change nothing (run this first)
#   LIQUIDGOV_DEPLOY_SSH="..."  the ssh command rsync uses (default: ssh -i $LIQUIDGOV_DEPLOY_KEY). Each assistant
#                               uses its own key; from WSL on the PC, Windows' ssh.exe can read it, e.g.
#                               "/mnt/c/Windows/System32/OpenSSH/ssh.exe -i C:/Users/Brandon/.ssh/id_ed25519_claude -o IdentitiesOnly=yes"
# Files are compared by checksum, so unchanged sources keep their times and the build stays incremental.

set -euo pipefail

DEST="${1:-root@143.198.159.58:/opt/liquidgov/}"
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")/../liquidgov-platform" && pwd)/"
KEY="${LIQUIDGOV_DEPLOY_KEY:-$HOME/.ssh/liquidgov_deploy_key}"
SSH_CMD="${LIQUIDGOV_DEPLOY_SSH:-ssh -i $KEY}"
DRY=()
[ "${LIQUIDGOV_DEPLOY_DRY_RUN:-}" = "1" ] && DRY=(--dry-run --itemize-changes)

# Only committed code goes live (2026-11-07). On 2026-11-07 the Flow and
# subject-match slices were synced from an uncommitted working tree, so
# liquidgov.us ran code that existed in no commit: the old "live ahead of
# GitHub" problem again. A real sync now refuses unless the tree is clean,
# and prints the commit it deploys (put that in `ctx log ... --ref`).
# Dry runs still work on a dirty tree. LIQUIDGOV_DEPLOY_ALLOW_DIRTY="<why>"
# overrides, loudly. This worktree's .git points at a Windows path WSL git
# cannot read, so git.exe (Windows git through WSL interop) is tried next;
# if neither can say whether the tree is clean, the sync refuses.
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=deploy/git-in-repo.sh
. "$(dirname "${BASH_SOURCE[0]}")/git-in-repo.sh"
if ! COMMIT="$(git_in_repo rev-parse --short HEAD)"; then
    COMMIT=""
fi
if ! DIRTY="$(git_in_repo status --porcelain)"; then
    DIRTY="(could not read git status for $REPO)"
fi
if [ -n "$DIRTY" ] || [ -z "$COMMIT" ]; then
    if [ ${#DRY[@]} -gt 0 ]; then
        echo "WARNING: uncommitted changes; fine for a dry run, but a real sync will refuse:"
        printf '%s\n' "$DIRTY" | head -20
    elif [ -n "${LIQUIDGOV_DEPLOY_ALLOW_DIRTY:-}" ]; then
        echo "WARNING: deploying UNCOMMITTED changes on top of ${COMMIT:-unknown} because: $LIQUIDGOV_DEPLOY_ALLOW_DIRTY"
        printf '%s\n' "$DIRTY" | head -20
    else
        echo "Refusing: only committed code goes live. Commit first (git is the record of what runs on liquidgov.us)."
        printf '%s\n' "$DIRTY" | head -20
        echo "To override on purpose: LIQUIDGOV_DEPLOY_ALLOW_DIRTY=\"<reason>\" $0 $*"
        exit 3
    fi
fi
echo "Deploying commit ${COMMIT:-unknown}${DIRTY:+ (plus uncommitted changes)}"

# CI gate on every REAL sync (Brandon 2026-11-09): deploy/ci-gate.sh go refuses unless the platform CI run passed for
# this exact liquidgov-platform tree. Every deploy/*-go.sh syncs through this script, so this covers the 12 go scripts
# made before make-go.py added its own (earlier) gate call; new ones simply pass the gate twice. Dry runs are not
# gated: they change nothing. LIQUIDGOV_DEPLOY_ALLOW_DIRTY does not get past it (CI never saw uncommitted code);
# LIQUIDGOV_CI_OVERRIDE="<reason>" does, logged, and only with Brandon's explicit OK for that deploy.
# Tests: commons-daemon/tests/sync_gate_check.py.
if [ ${#DRY[@]} -eq 0 ]; then
    if ! bash "$(dirname "${BASH_SOURCE[0]}")/ci-gate.sh" go; then
        echo "Refusing to sync: the CI gate (deploy/ci-gate.sh) did not pass. Nothing was sent."
        exit 4
    fi
fi

# No-rollback guard on every REAL sync (Brandon 2026-11-09): the program a rollback restores (built from the LIVE
# commit) must still open the store this commit writes; deploy/rollback_guard.py compares the two file_store.cpp
# versions. The live commit comes from <dest>/DEPLOYED_COMMIT on the server (written below by every real sync), or
# LIQUIDGOV_LIVE_COMMIT=<commit> (the first deploy after this change, before the file exists). Unknown -> refused
# (fails closed). LIQUIDGOV_NO_ROLLBACK_OK="<reason + undo plan>" gets past a refusal, logged, only with Brandon's OK.
# The file holds the LAST TWO synced commits (newest first) and the guard checks this commit against BOTH: the file is
# written right after the sync, before the build, so after a failed build the running program is the older line
# (side-agent note 2026-11-09). Cost: the deploy right after an overridden one is checked against the commit before
# it too, so it may need the override once more.
# Tests: commons-daemon/tests/rollback_guard_check.py.
DEST_HOST="${DEST%%:*}"
DEST_DIR="${DEST#*:}"
pick_python() {
    local p
    for p in python3 python; do
        command -v "$p" >/dev/null 2>&1 || continue
        "$p" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)' >/dev/null 2>&1 </dev/null && { echo "$p"; return 0; }
    done
    return 1
}
PREV_LIVE=""
if [ ${#DRY[@]} -eq 0 ]; then
    if [ -n "${LIQUIDGOV_LIVE_COMMIT:-}" ]; then
        LIVES="$LIQUIDGOV_LIVE_COMMIT"
    else
        # shellcheck disable=SC2086  # SSH_CMD is a command line, split on purpose (as rsync -e does)
        LIVES="$($SSH_CMD "$DEST_HOST" "cat ${DEST_DIR%/}/DEPLOYED_COMMIT" 2>/dev/null </dev/null | tr -d '\r' | head -2 | tr '\n' ' ')" || LIVES=""
    fi
    read -r -a LIVE_LIST <<< "$LIVES"
    if [ ${#LIVE_LIST[@]} -eq 0 ]; then LIVE_LIST=(""); fi
    for LIVE in "${LIVE_LIST[@]}"; do
        if ! [[ "$LIVE" =~ ^[0-9a-f]{7,40}$ ]]; then
            echo "Refusing to sync: the live commit is unknown (no usable ${DEST_DIR%/}/DEPLOYED_COMMIT on $DEST_HOST, got '${LIVE:0:40}')."
            echo "Give it from the last deploy record: LIQUIDGOV_LIVE_COMMIT=<commit> $0 $*   Nothing was sent."
            exit 5
        fi
    done
    PREV_LIVE="${LIVE_LIST[0]}"
    GUARD="$(dirname "${BASH_SOURCE[0]}")/rollback_guard.py"
    PY="$(pick_python)" || PY=""
    if [ -n "$PY" ] && [ -f "$GUARD" ]; then
        for LIVE in "${LIVE_LIST[@]}"; do
            if ! "$PY" "$GUARD" "$LIVE" "$REPO"; then
                echo "Refusing to sync: the no-rollback guard (deploy/rollback_guard.py) did not pass against $LIVE. Nothing was sent."
                exit 5
            fi
        done
    elif [ -n "${LIQUIDGOV_NO_ROLLBACK_OK:-}" ]; then
        # the override is the emergency path, so it must not depend on Python or the guard file being there
        echo "ROLLBACK_GUARD_OVERRIDE: $LIQUIDGOV_NO_ROLLBACK_OK (guard not run: ${PY:-no Python 3.8+}, $GUARD)"
    else
        echo "Refusing to sync: cannot run the no-rollback guard (${PY:-no Python 3.8+}; $GUARD). Nothing was sent."
        exit 5
    fi
fi

echo "Syncing $SRC -> $DEST (excluding build/.git/data files)${DRY:+ [dry run]}"
# --no-times (2026-11-08): -a includes -t, which set every UNCHANGED file's modification time to the local checkout's,
# so a fresh worktree made the next server build recompile everything (the 14:22 PT deploy rebuilt the whole tree for
# one changed .cpp). Without -t, unchanged files keep their server times and a changed file gets "now", so make
# still rebuilds exactly what changed. --checksum decides what changed, so times are not needed for that.
rsync -az --no-times --checksum "${DRY[@]}" -e "$SSH_CMD" \
    --exclude build \
    --exclude .git \
    --exclude '*.lgp' \
    --exclude '*.lgp.*' \
    --exclude 'liquidgov_blobs/' \
    --exclude '.env.local' \
    "$SRC" "$DEST"

# Record what was just synced for the next deploy's rollback guard: this commit first, the previous newest second.
# Written right after the sync, before the build, so after a failed build line 1 names a commit the running program
# was not built from; line 2 is the one it was, and the next guard checks both.
# If it cannot be written, stop here, before any build or restart: the next deploy's guard would refuse anyway.
if [ ${#DRY[@]} -eq 0 ]; then
    FULL="$(git_in_repo rev-parse HEAD)" || FULL=""
    REC="$(mktemp)"
    # newest first; the previous newest stays as line 2 (the running program if this deploy's build fails)
    if [ -n "$PREV_LIVE" ] && [ "${PREV_LIVE}" != "${FULL:0:${#PREV_LIVE}}" ]; then
        printf '%s\n%s\n' "$FULL" "$PREV_LIVE" > "$REC"
    else
        printf '%s\n' "$FULL" > "$REC"
    fi
    if [ -z "$FULL" ] || ! rsync -z --chmod=F644 -e "$SSH_CMD" "$REC" "${DEST%/}/DEPLOYED_COMMIT"; then
        rm -f "$REC"
        echo "Synced, but could NOT record ${DEST%/}/DEPLOYED_COMMIT (commit '${FULL:0:12}'). Stopping before any build or restart."
        exit 6
    fi
    rm -f "$REC"
    echo "Recorded DEPLOYED_COMMIT ${FULL:0:12} on $DEST_HOST"
fi

echo "Sync done. Still need on the remote: cmake --build <path>/build -j1, chown, systemctl restart liquidgov."
