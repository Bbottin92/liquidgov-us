"""ci_skip_list_check.py: run_hook_checks.py reports PC-only checks as SKIPPED off Windows, never as passed, and a
check that ran no cases is not a pass (Brandon 2026-11-12 picked "skip list in runner"). Why: the first GitHub run
(43302640573) crashed in tidy_worktrees_check.py (cmd /c mklink), and tidy_in_use_check.py exited 0 with passes=0,
which the runner counted as a pass. The platform is forced with LG_CHECKS_PLATFORM so both sides test anywhere.

  S0  preconditions: the runner exists and its WINDOWS_ONLY list is non-empty
  S1  every WINDOWS_ONLY name is a check file on disk (no stale entry hides nothing while looking like coverage)
  S2  linux: a crashing Windows-only check is not run, the "SKIPPED (Windows-only)" line names it, exit 0
  S3  win32: the same check runs and its crash fails the run (exit 1)
  S4  a check that exits 0 but prints passes=0 fails the run and is called out as running no cases
  S5  a normal passing check still passes (exit 0) on linux

Usage: python commons-daemon/tests/ci_skip_list_check.py [repo root] -> last line passes=N fails=M; exit 1 on FAIL."""
import importlib.util
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
RUNNER = os.path.join(ROOT, "scripts", "run_hook_checks.py")
SELECTOR = os.path.join(ROOT, "scripts", "affected_checks.py")
TESTS = os.path.join(ROOT, "commons-daemon", "tests")
passes = fails = 0


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[-500:]))
    passes += bool(ok)
    fails += not ok


def fake_repo(tmp, name, bodies):
    fake = os.path.join(tmp, name)
    os.makedirs(os.path.join(fake, "scripts"))
    os.makedirs(os.path.join(fake, "commons-daemon", "tests"))
    shutil.copy(SELECTOR, os.path.join(fake, "scripts"))
    shutil.copy(RUNNER, os.path.join(fake, "scripts"))
    for f, body in bodies.items():
        open(os.path.join(fake, "commons-daemon", "tests", f), "w", encoding="utf-8").write(body + "\n")
    return fake


def run(fake, plat):
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
    env["LG_CHECKS_PLATFORM"] = plat
    r = subprocess.run([sys.executable, os.path.join(fake, "scripts", "run_hook_checks.py"), fake],
                       capture_output=True, text=True, env=env, timeout=120)
    return r.returncode, r.stdout + r.stderr


windows_only = []
if os.path.exists(RUNNER):
    spec = importlib.util.spec_from_file_location("run_hook_checks", RUNNER)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    windows_only = sorted(getattr(mod, "WINDOWS_ONLY", ()))
check("S0 precondition: runner exists", os.path.exists(RUNNER), RUNNER)
check("S0 precondition: WINDOWS_ONLY is non-empty", len(windows_only) > 0, windows_only)
on_disk = set(os.listdir(TESTS))
check("S1 every WINDOWS_ONLY entry is a check on disk", windows_only and all(n in on_disk for n in windows_only),
      [n for n in windows_only if n not in on_disk])

if os.path.exists(RUNNER) and windows_only:
    pc_only = windows_only[0]
    crash = "raise FileNotFoundError(\"No such file or directory: 'cmd'\")"
    ok_body = "print('PASS a'); print('passes=1 fails=0')"
    tmp = tempfile.mkdtemp(prefix="ci-skip-")
    try:
        fake = fake_repo(tmp, "pc", {pc_only: crash, "a_ok_check.py": ok_body})
        rc, out = run(fake, "linux")
        check("S2 linux: exit 0 with only the PC-only check broken", rc == 0, (rc, out[-400:]))
        check("S2 linux: the PC-only check is not run", "== %s" % pc_only not in out, out[-400:])
        check("S2 linux: SKIPPED (Windows-only) line names it",
              any(l.startswith("SKIPPED (Windows-only)") and pc_only in l for l in out.splitlines()), out[-400:])
        check("S5 linux: the normal check still ran and passed", "== a_ok_check.py: exit 0" in out, out[-400:])
        rc, out = run(fake, "win32")
        check("S3 win32: the PC-only check runs", "== %s" % pc_only in out, out[-400:])
        check("S3 win32: its crash fails the run", rc == 1, (rc, out[-400:]))
        fake = fake_repo(tmp, "empty", {"z_empty_check.py": "print('z: passes=0 fails=0 (skipped)')"})
        rc, out = run(fake, "linux")
        check("S4 passes=0 fails the run", rc == 1, (rc, out[-400:]))
        check("S4 ...and says it ran no cases", "ran no cases" in out and "z_empty_check.py" in out, out[-400:])
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
