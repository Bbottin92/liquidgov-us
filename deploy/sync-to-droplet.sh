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
git_in_repo() {
    local out
    if out="$(git -C "$REPO" "$@" 2>/dev/null)"; then printf '%s' "$out"; return 0; fi
    if command -v git.exe >/dev/null 2>&1 && command -v wslpath >/dev/null 2>&1; then
        local wp; wp="$(wslpath -w "$REPO" 2>/dev/null)" || return 1
        out="$(git.exe -C "$wp" "$@" 2>/dev/null)" || return 1
        printf '%s' "${out//$'\r'/}"; return 0
    fi
    return 1
}
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

echo "Sync done. Still need on the remote: cmake --build <path>/build -j1, chown, systemctl restart liquidgov."
