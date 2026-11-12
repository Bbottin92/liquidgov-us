#!/usr/bin/env python3
"""run_hook_checks.py: run EVERY check the git hooks can run (commons-daemon/tests/*_check.py + lgp1_consistency.py),
for CI (.github/workflows/hook-checks.yml). Waste-audit PLAN 0.3 (Grok Bot 2026-11-11; Brandon 2026-11-12 "you take
0.3"): until then the checks ran only in .githooks, so a clone without core.hooksPath, or a push that skipped the
hooks, got past all of them.

The list is affected_checks.py's (is_check + run_order), the same one the hooks pick from, so a new check file is
run here without editing this script or the workflow. Each check gets the repo root as its argument, as in pre-push.
Prints each check's last line, the FAIL lines of a failing one, then "SUMMARY hook-checks passes=N fails=M".
Exit 1 when any check fails, times out or can't start, or when no check is found (an empty run is not a pass).
A check that exits 0 but prints passes=0 ran no cases: that fails the run too.
WINDOWS_ONLY checks test PC-only tools (tidy_worktrees.py: Windows junctions via cmd /c mklink, in-use folders). Off
Windows they are not run and are listed on a "SKIPPED (Windows-only)" line, never counted as passed; the git hooks on
the PC still run them (Brandon 2026-11-12, after GitHub run 43302640573). LG_CHECKS_PLATFORM overrides sys.platform
(for ci_skip_list_check.py).

Usage: python3 scripts/run_hook_checks.py <repo root> [--list]   (--list prints the check names and runs nothing)"""
import importlib.util
import os
import re
import subprocess
import sys
import time

TIMEOUT = 600  # seconds per check; the slowest (temp git repos, fake gh) take a few seconds
WINDOWS_ONLY = ("tidy_worktrees_check.py", "tidy_in_use_check.py")
NO_CASES = re.compile(r"\bpasses=0\b")


def load_selector(root):
    path = os.path.join(root, "scripts", "affected_checks.py")
    spec = importlib.util.spec_from_file_location("affected_checks", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def check_names(root):
    ac = load_selector(root)
    tests = os.path.join(root, "commons-daemon", "tests")
    return tests, ac.run_order({n for n in os.listdir(tests) if ac.is_check(n)})


def main(argv):
    if not argv or len(argv) > 2 or (len(argv) == 2 and argv[1] != "--list"):
        print(__doc__, file=sys.stderr)
        return 2
    root = os.path.abspath(argv[0])
    try:
        tests, names = check_names(root)
    except Exception as e:  # can't list the checks: that is a failure, never a quiet pass
        print("FAIL could not list the checks: %r" % e)
        return 1
    if len(argv) == 2:
        print("\n".join(names))
        return 0
    if not names:
        print("FAIL no checks found in %s" % tests)
        print("SUMMARY hook-checks passes=0 fails=1")
        return 1
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
    on_windows = os.environ.get("LG_CHECKS_PLATFORM", sys.platform).startswith("win")
    skipped = [] if on_windows else [n for n in names if n in WINDOWS_ONLY]
    names = [n for n in names if n not in skipped]
    failed = []
    for name in names:
        start = time.time()
        try:
            r = subprocess.run([sys.executable, os.path.join(tests, name), root], capture_output=True, text=True,
                               env=env, timeout=TIMEOUT)
            rc, out = r.returncode, (r.stdout + r.stderr).rstrip()
        except subprocess.TimeoutExpired:
            rc, out = "timeout", "FAIL timed out after %d s" % TIMEOUT
        except OSError as e:
            rc, out = "error", "FAIL could not start: %r" % e
        lines = out.splitlines() or ["(no output)"]
        print("== %s: exit %s (%.1f s) | %s" % (name, rc, time.time() - start, lines[-1]), flush=True)
        if rc == 0 and NO_CASES.search(lines[-1]):
            failed.append(name)
            print("   FAIL %s ran no cases (passes=0): not a pass" % name, flush=True)
        elif rc != 0:
            failed.append(name)
            shown = [l for l in lines if "FAIL" in l][:30] or lines[-20:]
            print("\n".join("   " + l for l in shown), flush=True)
    if skipped:
        print("SKIPPED (Windows-only): %s  (not passed; the PC's git hooks run them)" % " ".join(skipped))
    print("SUMMARY hook-checks passes=%d fails=%d" % (len(names) - len(failed), len(failed)))
    if failed:
        print("failed: " + " ".join(failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
