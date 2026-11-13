"""linux_only_skip_check.py: Linux-only checks are skipped (and said so) on Windows, and still run on Linux.
Why (Brandon 2026-11-13 "yes go with option A"): prebuilt_check.py drives deploy/fetch-prebuilt.sh with a fake gh and a
fake --version program that exit 127 under Git Bash, so the PC's pre-push refused every push that touched it (lesson
lessons.md:431, seen twice). GitHub CI (Linux) runs it on every push. One list: run_hook_checks.LINUX_ONLY, read by both
the runner and affected_checks.py (the hooks' selector). The platform is forced with LG_CHECKS_PLATFORM.

  L0  preconditions: LINUX_ONLY exists, is non-empty, and every entry is a check file on disk
  L1  runner, win32: the Linux-only check is not run, a "SKIPPED (Linux-only)" line names it, exit 0
  L2  runner, linux: the same check runs and its exit-127 crash fails the run (should-fire)
  L3  selector, win32: its name is not printed (the hook will not run it) and the SKIPPED line names it
  L4  selector, linux: its name IS printed (should-not-skip), and no Linux-only SKIPPED line
  L5  a normal check still runs and passes on win32

Usage: python tests/repo/linux_only_skip_check.py [repo root] -> last line passes=N fails=M; exit 1 on FAIL."""
import importlib.util
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
RUNNER = os.path.join(ROOT, "scripts", "run_hook_checks.py")
SELECTOR = os.path.join(ROOT, "scripts", "affected_checks.py")
TESTS = os.path.join(ROOT, "tests", "repo")
passes = fails = 0


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[-500:]))
    passes += bool(ok)
    fails += not ok


def env_for(plat):
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
    env["LG_CHECKS_PLATFORM"] = plat
    return env


linux_only = []
if os.path.exists(RUNNER):
    spec = importlib.util.spec_from_file_location("run_hook_checks", RUNNER)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    linux_only = sorted(getattr(mod, "LINUX_ONLY", ()))
check("L0 precondition: LINUX_ONLY is non-empty", len(linux_only) > 0, linux_only)
on_disk = set(os.listdir(TESTS))
check("L0 every LINUX_ONLY entry is a check on disk", linux_only and all(n in on_disk for n in linux_only),
      [n for n in linux_only if n not in on_disk])

if linux_only:
    target = linux_only[0]
    tmp = tempfile.mkdtemp(prefix="linux-only-")
    try:
        fake = os.path.join(tmp, "repo")
        os.makedirs(os.path.join(fake, "scripts"))
        os.makedirs(os.path.join(fake, "tests", "repo"))
        shutil.copy(SELECTOR, os.path.join(fake, "scripts"))
        shutil.copy(RUNNER, os.path.join(fake, "scripts"))
        bodies = {target: "import sys; print('FAIL fake gh :: (127, \"\")'); sys.exit(1)",
                  "a_ok_check.py": "print('PASS a'); print('passes=1 fails=0')"}
        for f, body in bodies.items():
            open(os.path.join(fake, "tests", "repo", f), "w", encoding="utf-8").write(body + "\n")
        runner = os.path.join(fake, "scripts", "run_hook_checks.py")
        selector = os.path.join(fake, "scripts", "affected_checks.py")

        r = subprocess.run([sys.executable, runner, fake], capture_output=True, text=True, env=env_for("win32"), timeout=120)
        out = r.stdout + r.stderr
        check("L1 win32 runner: exit 0 with only the Linux-only check broken", r.returncode == 0, (r.returncode, out[-400:]))
        check("L1 win32 runner: the Linux-only check is not run", ("== %s:" % target) not in out, out[-400:])
        check("L1 win32 runner: SKIPPED (Linux-only) line names it",
              any(l.startswith("SKIPPED (Linux-only)") and target in l.split() for l in out.splitlines()), out[-400:])
        check("L5 win32 runner: the normal check still ran and passed", "== a_ok_check.py: exit 0" in out, out[-400:])

        r = subprocess.run([sys.executable, runner, fake], capture_output=True, text=True, env=env_for("linux"), timeout=120)
        out = r.stdout + r.stderr
        check("L2 linux runner: the Linux-only check runs", ("== %s:" % target) in out, out[-400:])
        check("L2 linux runner: its crash fails the run (exit 1)", r.returncode == 1, (r.returncode, out[-400:]))

        # the selector is not in a git repo here, so it cannot list changed files and picks every check (its fallback)
        r = subprocess.run([sys.executable, selector, fake, "--staged"], capture_output=True, text=True,
                           env=env_for("win32"), timeout=120)
        names = [l.strip() for l in r.stdout.splitlines()]
        check("L3 win32 selector: exit 0", r.returncode == 0, (r.returncode, r.stderr[-300:]))
        check("L3 win32 selector: the Linux-only check is not handed to the hook", target not in names, names)
        check("L3 win32 selector: the normal check is still handed to the hook", "a_ok_check.py" in names, names)
        check("L3 win32 selector: SKIPPED (Linux-only) line names it",
              any(l.startswith("SKIPPED (Linux-only)") and target in l.split() for l in r.stderr.splitlines()),
              r.stderr[-300:])

        r = subprocess.run([sys.executable, selector, fake, "--staged"], capture_output=True, text=True,
                           env=env_for("linux"), timeout=120)
        names = [l.strip() for l in r.stdout.splitlines()]
        check("L4 linux selector: the Linux-only check IS handed to the hook", target in names, names)
        check("L4 linux selector: no Linux-only SKIPPED line", "SKIPPED (Linux-only)" not in r.stderr, r.stderr[-300:])
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
