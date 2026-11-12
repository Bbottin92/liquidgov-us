# shellcheck shell=bash
# git_in_repo <git args...>: run git on $REPO (set by the caller) and print its output.
# Sourced by deploy/ci-gate.sh and deploy/sync-to-droplet.sh (one copy, not two).
# A worktree's .git can point at a Windows path that WSL git cannot read, so git.exe (Windows git through WSL interop)
# is tried next. Interop drops calls now and then ("accept4 failed 110": hideint1 and hideint2, 2026-11-12), so git.exe
# gets LIQUIDGOV_GIT_TRIES tries (default 3), 1 s apart. </dev/null on every call: git.exe through interop reads stdin,
# which swallowed the rest of ci-gate.sh's run list (live probe 2026-11-08).
# Returns 1 when no git could answer; callers decide what that means and must say so (never a silent skip).
# Tests: commons-daemon/tests/ci_gate_interop_check.py (fake git.exe that drops calls), ci_gate_check.py, sync_gate_check.py.
git_in_repo() {
    local out
    if out="$(git -C "$REPO" "$@" 2>/dev/null </dev/null)"; then printf '%s' "$out"; return 0; fi
    command -v git.exe >/dev/null 2>&1 && command -v wslpath >/dev/null 2>&1 || return 1
    local wp; wp="$(wslpath -w "$REPO" 2>/dev/null)" || return 1
    local i tries="${LIQUIDGOV_GIT_TRIES:-3}"
    for ((i = 1; i <= tries; i++)); do
        if out="$(git.exe -C "$wp" "$@" 2>/dev/null </dev/null)"; then printf '%s' "${out//$'\r'/}"; return 0; fi
        if [ "$i" -lt "$tries" ]; then sleep 1; fi
    done
    return 1
}
