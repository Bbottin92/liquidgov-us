#!/usr/bin/env python3
"""The git hooks skip Windows-only checks off Windows, exactly like CI (Brandon 2026-11-13: "yes skip windows checks
on linux"). run_hook_checks.py (CI) already leaves out its WINDOWS_ONLY checks off Windows; the hooks get their list
from affected_checks.py, which did not, so once the hooks were executable a commit on Linux that runs every check
failed on tidy_worktrees_check.py (it calls `cmd`). One list: affected_checks.py reads run_hook_checks.WINDOWS_ONLY.
Uses the selector's every-check path (a push to master), so no repo changes are needed. LG_CHECKS_PLATFORM stands in
for the platform, as in ci_skip_list_check.py.
    python3 hook_windows_skip_check.py [repo root]
Prints 'hook_windows_skip_check: N checks, F failed'; exit 1 if any failed."""
import importlib.util
import os
import subprocess
import sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
checks = failed = 0


def check(ok, what, detail=""):
    global checks, failed
    checks += 1
    print(("PASS " if ok else "FAIL ") + what + ("" if ok else "  :: %s" % (detail,)))
    if not ok:
        failed += 1


spec = importlib.util.spec_from_file_location("rhc", os.path.join(ROOT, "scripts", "run_hook_checks.py"))
rhc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rhc)
windows_only = set(rhc.WINDOWS_ONLY)
check(bool(windows_only), "precondition: run_hook_checks.py names Windows-only checks", rhc.WINDOWS_ONLY)

head = subprocess.run(["git", "-C", ROOT, "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
push = "refs/heads/master %s refs/heads/master %s\n" % (head, "0" * 40)


def picked(platform):
    env = dict(os.environ, LG_CHECKS_PLATFORM=platform)
    r = subprocess.run([sys.executable, os.path.join(ROOT, "scripts", "affected_checks.py"), ROOT, "--push"],
                       input=push, capture_output=True, text=True, env=env)
    return r.returncode, set(r.stdout.split()), r.stderr


rc, win, _ = picked("win32")
check(rc == 0 and windows_only <= win, "control: on Windows the hooks still run every Windows-only check", sorted(windows_only - win))
rc, lin, err = picked("linux")
check(rc == 0 and len(lin) > 0, "on Linux the hooks still run checks", (rc, len(lin)))
check(not (windows_only & lin), "on Linux the hooks leave out the Windows-only checks", sorted(windows_only & lin))
check(lin == win - windows_only, "...and nothing else: Linux runs exactly Windows' list minus those", sorted(win ^ lin))
check("SKIPPED (Windows-only)" in err and all(n in err for n in windows_only),
      "...and say which were skipped (never counted as passed)", err.strip()[-200:])

print("hook_windows_skip_check: %d checks, %d failed" % (checks, failed))
sys.exit(1 if failed else 0)
