#!/usr/bin/env python3
"""The git hooks and run_hook_checks.py leave Linux-only checks to GitHub when they run on Windows (Brandon 2026-11-14:
"fix the windows hook so i dont need no-verify"). prebuilt_check.py starts deploy/fetch-prebuilt.sh with a plain
`bash`; from Windows Python that is System32's WSL bash.exe and exits 127, so the PC's hooks refused every commit of a
deploy pair and every push to master (spatial1, 2026-11-14: both went through with --no-verify). One list,
run_hook_checks.LINUX_ONLY, the mirror of WINDOWS_ONLY: on Windows those checks are not run and are named on a
"SKIPPED (Linux-only)" line, never counted as passed; everywhere else they run as before. GitHub's hook-checks run
(Linux) runs them on every push, and ship.py waits for that run.

Here: a throwaway repo with the real .githooks, selector and runner, a check called prebuilt_check.py that always
fails, and one that passes. A real `git commit` and a real `git push` to master run the hooks. The platform is forced
with LG_CHECKS_PLATFORM, as in ci_skip_list_check.py, so both sides test anywhere. This file's git calls drop GIT_*.
    python3 hook_linux_only_skip_check.py [repo root]
Prints 'hook_linux_only_skip_check: N checks, F failed'; exit 1 if any failed."""
import importlib.util
import os
import shutil
import stat
import subprocess
import sys
import tempfile

ROOT = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
checks = failed = 0
# LG_CHECK_MODE too (2026-11-19): pre-commit runs pre-push with LG_CHECK_MODE=--staged, and the fake repo's pushes below inherited it,
# so they picked checks by staged files (0) instead of "every check" and L4 failed in every commit that selected this file
CLEAN = {k: v for k, v in os.environ.items() if not k.startswith("GIT_") and k != "LG_CHECK_MODE"}
CLEAN["PYTHONDONTWRITEBYTECODE"] = "1"
NAME = "prebuilt_check.py"
BROKEN = ("import os\nopen(os.environ['LG_RAN'], 'a').write('ran\\n')\n"
          "print(\"FAIL no release run -> refused  :: (127, '')\")\nprint('prebuilt_check: 1 checks, 1 failed')\nraise SystemExit(1)\n")
FINE = "print('PASS a')\nprint('a_ok_check: 1 checks, 0 failed')\n"


def check(ok, what, detail=""):
    global checks, failed
    checks += 1
    print(("PASS " if ok else "FAIL ") + what + ("" if ok else "  :: %s" % (str(detail)[-600:],)))
    if not ok:
        failed += 1


def git(cwd, *args, env=None):
    cmd = ["git", "-c", "user.name=lg", "-c", "user.email=lg@example.invalid", "-c", "commit.gpgsign=false"]
    return subprocess.run(cmd + list(args), cwd=cwd, capture_output=True, text=True, env=env or CLEAN,
                          stdin=subprocess.DEVNULL)


def rmtree(path):
    def writable(func, p, _):
        os.chmod(p, stat.S_IWRITE)
        func(p)
    shutil.rmtree(path, onerror=writable)


def build(tmp):
    """a checkout on master with a bare origin and the hooks on. Returns its path."""
    main, origin = os.path.join(tmp, "main"), os.path.join(tmp, "origin.git")
    git(tmp, "init", "-q", "--bare", origin)
    git(tmp, "init", "-q", main)
    git(main, "checkout", "-q", "-b", "master")
    for rel in (".githooks/pre-commit", ".githooks/pre-push", "scripts/affected_checks.py", "scripts/run_hook_checks.py"):
        dst = os.path.join(main, *rel.split("/"))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copyfile(os.path.join(ROOT, *rel.split("/")), dst)
        if rel.startswith(".githooks/"):
            os.chmod(dst, 0o755)
    tests = os.path.join(main, "tests", "repo")
    os.makedirs(tests)
    for name, body in ((NAME, BROKEN), ("a_ok_check.py", FINE)):
        with open(os.path.join(tests, name), "w", encoding="utf-8", newline="\n") as f:
            f.write(body)
    with open(os.path.join(main, "naming.toml"), "w", encoding="utf-8", newline="\n") as f:
        f.write("# base\n")
    git(main, "add", "-A")
    git(main, "update-index", "--chmod=+x", ".githooks/pre-commit", ".githooks/pre-push")
    git(main, "commit", "-qm", "base")  # hooks are not on yet
    git(main, "remote", "add", "origin", origin)
    git(main, "push", "-q", "origin", "master")
    git(main, "config", "core.hooksPath", ".githooks")
    return main


def ran(path):
    there = os.path.exists(path)
    if there:
        os.remove(path)
    return there


def cases(tmp):
    spec = importlib.util.spec_from_file_location("rhc", os.path.join(ROOT, "scripts", "run_hook_checks.py"))
    rhc = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(rhc)
    linux_only, windows_only = tuple(getattr(rhc, "LINUX_ONLY", ())), tuple(rhc.WINDOWS_ONLY)
    on_disk = set(os.listdir(os.path.join(ROOT, "tests", "repo")))
    check(NAME in linux_only, "L0 run_hook_checks.LINUX_ONLY names prebuilt_check.py", linux_only)
    check(bool(linux_only) and all(n in on_disk for n in linux_only), "L0 every LINUX_ONLY entry is a check on disk",
          [n for n in linux_only if n not in on_disk])
    check(not set(linux_only) & set(windows_only), "L0 no check is both Linux-only and Windows-only",
          sorted(set(linux_only) & set(windows_only)))

    main = build(tmp)
    marker = os.path.join(tmp, "ran.txt")
    sel = [sys.executable, os.path.join(main, "scripts", "affected_checks.py"), main]

    def env(plat):
        return dict(CLEAN, LG_CHECKS_PLATFORM=plat, LG_RAN=marker)

    def listed(plat):
        r = subprocess.run(sel + ["--linux-only"], capture_output=True, text=True, env=env(plat))
        return r.returncode, r.stdout.split()
    check(listed("win32") == (0, list(linux_only)), "L1 the selector's --linux-only: on Windows, exactly the list", listed("win32"))
    check(listed("linux") == (0, []), "L1 ...and nothing on Linux", listed("linux"))
    head = git(main, "rev-parse", "HEAD").stdout.strip()
    r = subprocess.run(sel + ["--push"], input="refs/heads/master %s refs/heads/master %s\n" % (head, "0" * 40),
                       capture_output=True, text=True, env=env("win32"))
    check(r.returncode == 0 and NAME in r.stdout.split(),
          "L1 the selection itself is the same on Windows (the hook does the skipping, and says so)", r.stdout.split())

    def edit(text):
        with open(os.path.join(main, "naming.toml"), "a", encoding="utf-8", newline="\n") as f:
            f.write(text)   # naming.toml is one of the files prebuilt_check.py reads: the selector picks it
        git(main, "add", "naming.toml")

    # control: on Linux the hooks still run it, and its failure still refuses the commit
    edit("# linux\n")
    r = git(main, "commit", "-qm", "on linux", env=env("linux"))
    out = r.stdout + r.stderr
    check(r.returncode != 0 and ran(marker) and "SKIPPED (Linux-only)" not in out,
          "L2 control, Linux: the hook runs prebuilt_check.py and its failure refuses the commit", (r.returncode, out))

    # Windows: not run, said so, the commit goes through, the other check still ran
    r = git(main, "commit", "-qm", "on windows", env=env("win32"))
    out = r.stdout + r.stderr
    was_run = ran(marker)
    check(r.returncode == 0 and git(main, "log", "-1", "--format=%s").stdout.strip() == "on windows",
          "L3 Windows: the commit goes through", (r.returncode, out))
    check(not was_run and "prebuilt_check: 1 checks, 1 failed" not in out, "L3 Windows: prebuilt_check.py is not run", out)
    check(any(l.startswith("SKIPPED (Linux-only)") and NAME in l for l in out.splitlines()),
          "L3 Windows: a SKIPPED (Linux-only) line names it (never counted as passed)", out)
    check("a_ok_check: 1 checks, 0 failed" in out, "L3 Windows: the other check still ran", out)

    # the push to master (every check): the same on Windows; refused on Linux
    r = git(main, "push", "-q", "origin", "master", env=env("linux"))
    check(r.returncode != 0 and ran(marker), "L4 control, Linux: the push to master is refused by the failing check",
          (r.returncode, r.stdout + r.stderr))
    r = git(main, "push", "-q", "origin", "master", env=env("win32"))
    out = r.stdout + r.stderr
    tip = git(main, "rev-parse", "HEAD").stdout.strip()
    check(r.returncode == 0 and not ran(marker) and git(main, "rev-parse", "origin/master").stdout.strip() == tip
          and any(l.startswith("SKIPPED (Linux-only)") and NAME in l for l in out.splitlines())
          and "a_ok_check: 1 checks, 0 failed" in out,
          "L4 Windows: the push to master goes through, prebuilt_check.py skipped and named, the other check run",
          (r.returncode, out))

    # a real failure is still a refusal on Windows: only the listed checks are left out
    with open(os.path.join(main, "tests", "repo", "a_ok_check.py"), "w", encoding="utf-8", newline="\n") as f:
        f.write("print('FAIL a')\nprint('a_ok_check: 1 checks, 1 failed')\nraise SystemExit(1)\n")
    git(main, "add", "-A")
    r = git(main, "commit", "-qm", "broken", env=env("win32"))
    check(r.returncode != 0, "L5 Windows: any other failing check still refuses the commit", (r.returncode, r.stdout + r.stderr))
    git(main, "reset", "-q", "--hard", "HEAD")

    # run_hook_checks.py (by hand on the PC)
    runner = [sys.executable, os.path.join(main, "scripts", "run_hook_checks.py"), main]
    r = subprocess.run(runner, capture_output=True, text=True, env=env("win32"))
    check(r.returncode == 0 and not ran(marker) and "== %s" % NAME not in r.stdout
          and any(l.startswith("SKIPPED (Linux-only)") and NAME in l for l in r.stdout.splitlines()),
          "L6 run_hook_checks.py on Windows: not run, named on a SKIPPED (Linux-only) line, exit 0", (r.returncode, r.stdout))
    r = subprocess.run(runner, capture_output=True, text=True, env=env("linux"))
    check(r.returncode == 1 and ran(marker) and "== %s" % NAME in r.stdout and "SKIPPED (Linux-only)" not in r.stdout,
          "L6 control, Linux (GitHub's run): it runs and its failure fails the run", (r.returncode, r.stdout))


tmp = tempfile.mkdtemp(prefix="lg-hook-linux-only-")
try:
    cases(tmp)
except Exception as e:  # a broken case is a failure, never a quiet pass
    check(False, "the cases ran to the end", repr(e))
finally:
    rmtree(tmp)

print("hook_linux_only_skip_check: %d checks, %d failed" % (checks, failed))
sys.exit(1 if failed else 0)
