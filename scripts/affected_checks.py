#!/usr/bin/env python3
"""affected_checks.py: which pre-commit/pre-push checks a change could affect (Brandon 2026-11-10: "the only thing that
needs to be tested are things that may be affected by changes that were made"; every commit ran all 18, even a
one-line notes edit). Prints one check file name per line on stdout and its reason on stderr.

Each check lists the paths it reads (from its source, 2026-11-10). A check runs when a changed file matches one of
them, or when its own file changes. Everything runs when the hooks or this selector change, or when the changed files
can't be found. A check file in this folder that is not in CHECKS always runs (a new check is never silently skipped).
naming_check also runs whenever a file is added or renamed anywhere (it judges every new name).

Usage: python affected_checks.py <repo root> --staged          (pre-commit: the index against HEAD)
       python affected_checks.py <repo root> --push < <stdin>  (pre-push: git's "<lref> <lsha> <rref> <rsha>" lines)"""
import fnmatch
import os
import subprocess
import sys

# lives in scripts/ (naming.toml: commons-daemon/tests holds only *_check.py); the checks are in commons-daemon/tests
HERE = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "commons-daemon", "tests")
ZERO = "0" * 40
SRC, ASSETS = "liquidgov-platform/src/*", "liquidgov-platform/assets/*"
DEPLOY = ["deploy/*", "naming.toml"]
# check -> path patterns it reads (fnmatch; '*' crosses '/'). Order = run order.
CHECKS = {
    "lgp1_consistency.py": ["liquidgov-platform/libs/*", SRC, ASSETS, "deploy/city-undo.py",
                            "liquidgov-platform/tests/e2e/city_undo.sh"],
    "one_app_check.py": [SRC, ASSETS],
    "pwa_files_check.py": ["commons/*", "commons-daemon/deploy/*", "Persistence/TheCommons/art/*", SRC],
    "naming_check.py": ["naming.toml", "liquidgov-platform/tests/*", "commons-daemon/*", SRC, "deploy/*"],
    "restart_bypass_check.py": DEPLOY + ["liquidgov-platform/tests/e2e/*"],
    "make_go_check.py": DEPLOY,
    "tidy_worktrees_check.py": ["scripts/*"],
    "next_port_check.py": ["scripts/*", "naming.toml", "liquidgov-platform/tests/e2e/*"],
    "make_go_records_check.py": DEPLOY,
    "tidy_cwd_check.py": ["scripts/*"],
    "tidy_in_use_check.py": ["scripts/*"],
    "make_go_dry_label_check.py": DEPLOY,
    "ci_gate_check.py": DEPLOY,
    "sync_gate_check.py": DEPLOY,
    "rollback_guard_check.py": DEPLOY + ["liquidgov-platform/libs/liquidgov_file_store/*"],
    "narration_files_check.py": [SRC, ASSETS],
    "make_go_record_length_check.py": DEPLOY,
    "preflight_retry_check.py": DEPLOY,
    "affected_checks_check.py": [],
}
EVERYTHING = [".githooks/*", "scripts/affected_checks.py"]


def is_check(name):
    return name.endswith("_check.py") or name == "lgp1_consistency.py"


def run_order(on_disk):
    return [n for n in list(CHECKS) + sorted(set(on_disk) - set(CHECKS)) if n in on_disk]


def select(changed, added, on_disk):
    """changed: repo paths ('/'); added: the subset added or renamed; on_disk: check file names in this folder.
    Returns (check names in run order, reason)."""
    changed = [c.replace("\\", "/") for c in changed]
    added = [a.replace("\\", "/") for a in added]
    hit = [c for c in changed if any(fnmatch.fnmatch(c, p) for p in EVERYTHING)]
    if hit:
        return run_order(on_disk), "hook or selector changed (%s): every check" % ", ".join(hit)
    pick = set(n for n in on_disk if n not in CHECKS)  # unmapped: always
    for c in changed:
        if c.startswith("commons-daemon/tests/") and os.path.basename(c) in on_disk:
            pick.add(os.path.basename(c))
        for name, pats in CHECKS.items():
            if any(fnmatch.fnmatch(c, p) for p in pats):
                pick.add(name)
    if added:
        pick.add("naming_check.py")
    picked = [n for n in run_order(on_disk) if n in pick]
    return picked, "%d changed file(s): %d of %d check(s)" % (len(changed), len(picked), len(on_disk))


def git_lines(root, *args):
    r = subprocess.run(["git", "-C", root] + list(args), capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError("git %s: %s" % (" ".join(args), r.stderr.strip()[:200]))
    return [l for l in r.stdout.splitlines() if l.strip()]


def name_status(lines):
    changed, added = [], []
    for l in lines:
        parts = l.split("\t")
        status, paths = parts[0], parts[1:]
        changed.extend(paths)
        if status[:1] in ("A", "R", "C"):
            added.append(paths[-1])
    return changed, added


def changed_files(root, mode, stdin_text=""):
    if mode == "--staged":
        return name_status(git_lines(root, "diff", "--cached", "--name-status", "-M"))
    changed, added = [], []
    for line in stdin_text.splitlines():
        parts = line.split()
        if len(parts) != 4 or parts[1] == ZERO:
            continue  # a branch delete pushes no files
        lsha, rsha = parts[1], parts[3]
        base = rsha if rsha != ZERO else git_lines(root, "merge-base", lsha, "origin/master")[0]
        c, a = name_status(git_lines(root, "diff", "--name-status", "-M", base, lsha))
        changed += c
        added += a
    return changed, added


def main():
    if len(sys.argv) != 3 or sys.argv[2] not in ("--staged", "--push"):
        print(__doc__, file=sys.stderr)
        return 2
    root = sys.argv[1]
    # plain \n: Windows Python writes \r\n, and the hook's `for check in $checks` kept the \r on every name (2026-11-10)
    sys.stdout.reconfigure(newline="\n")
    on_disk = {n for n in os.listdir(HERE) if is_check(n)}
    try:
        stdin_text = sys.stdin.read() if sys.argv[2] == "--push" else ""
        changed, added = changed_files(root, sys.argv[2], stdin_text)
        picked, why = select(changed, added, on_disk)
    except Exception as e:  # can't tell what changed: run everything rather than skip
        picked, why = run_order(on_disk), "could not list the changed files (%s): every check" % e
    print("affected_checks: " + why, file=sys.stderr)
    for n in picked:
        print(n)
    return 0


if __name__ == "__main__":
    sys.exit(main())
