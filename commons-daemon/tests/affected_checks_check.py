"""affected_checks_check.py: the pre-commit/pre-push gate runs only the checks a change could affect
(affected_checks.py, Brandon 2026-11-10), never skips one it can't rule out, and runs everything when the hooks or
the selector change. Both the selection rules and the real script against a throwaway git repo (--staged and --push).
Fails on the old code (no affected_checks.py). Usage: python affected_checks_check.py <root>"""
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", ".."))
TESTS = os.path.join(ROOT, "commons-daemon", "tests")
passes = fails = 0


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:300]))
    passes += bool(ok)
    fails += not ok


# import from <root> only: Python also puts this file's own folder on the path, which would test another copy
SCRIPTS = os.path.join(ROOT, "scripts")  # the selector lives in scripts/ (naming.toml: this folder is *_check.py only)
sys.path[:] = [SCRIPTS] + [p for p in sys.path if os.path.abspath(p or ".") != HERE]
try:
    import affected_checks as ac
except ImportError as e:
    check("affected_checks.py exists", False, e)
    print("passes=%d fails=%d" % (passes, fails))
    sys.exit(1)

on_disk = {n for n in os.listdir(TESTS) if ac.is_check(n)}
check("precondition: 18+ checks found on disk", len(on_disk) >= 18, sorted(on_disk))
check("every check on disk is in the map (keep CHECKS current)", on_disk == set(ac.CHECKS),
      sorted(on_disk ^ set(ac.CHECKS)))


def sel(changed, added=()):
    return ac.select(list(changed), list(added), on_disk)[0]


got = sel(["Persistence/notes.md"])
check("a notes.md edit runs nothing", got == [], got)
got = sel(["liquidgov-platform/src/http_server.cpp"])
check("http_server.cpp runs the store, app, pwa, naming and narration checks",
      {"lgp1_consistency.py", "one_app_check.py", "pwa_files_check.py", "naming_check.py",
       "narration_files_check.py"} <= set(got), got)
check("... and not the deploy-script checks", "make_go_check.py" not in got and "ci_gate_check.py" not in got, got)
got = sel(["deploy/care-go.sh"])
check("a deploy script runs the make-go, gate and restart checks",
      {"make_go_check.py", "ci_gate_check.py", "preflight_retry_check.py", "restart_bypass_check.py"} <= set(got), got)
check("... and not the store or narration checks", "lgp1_consistency.py" not in got and "narration_files_check.py" not in got, got)
got = sel(["deploy/city-undo.py"])
check("deploy/city-undo.py also runs the store consistency check", "lgp1_consistency.py" in got, got)
got = sel(["scripts/tidy_worktrees.py"])
check("scripts/ runs the tidy checks", {"tidy_worktrees_check.py", "tidy_cwd_check.py", "tidy_in_use_check.py"} <= set(got), got)
got = sel(["docs/archive/new.md"], ["docs/archive/new.md"])
check("a new file anywhere runs naming_check (and only that)", got == ["naming_check.py"], got)
got = sel(["commons-daemon/tests/ci_gate_check.py"])
check("a check's own file runs that check", "ci_gate_check.py" in got, got)
for f in (".githooks/pre-push", "scripts/affected_checks.py"):
    got = sel([f])
    check("%s runs every check" % f, set(got) == on_disk, len(got))
got = ac.select(["Persistence/notes.md"], [], on_disk | {"zz_new_check.py"})[0]
check("an unmapped new check always runs", got == ["zz_new_check.py"], got)
got = sel(["deploy\\care-go.sh"])
check("Windows separators still match", "make_go_check.py" in got, got)

# the real script in a throwaway repo: --staged and --push
tmp = tempfile.mkdtemp(prefix="affected-checks-")
env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}


def git(*args):
    return subprocess.run(["git", "-C", tmp] + list(args), capture_output=True, text=True, env=env, check=True).stdout


RAW_CR = []


def run(mode, stdin=""):
    # bytes, not text: the hook reads this output in sh, where a \r stays on each name (2026-11-10, every check failed)
    p = subprocess.run([sys.executable, os.path.join(SCRIPTS, "affected_checks.py"), tmp, mode], input=stdin.encode(),
                       capture_output=True, env=env, timeout=60)
    if b"\r" in p.stdout:
        RAW_CR.append(mode)
    return p.returncode, p.stdout.decode().split(), p.stderr.decode(errors="replace")


try:
    git("init", "-q")
    git("config", "user.email", "t@t")
    git("config", "user.name", "t")
    for rel in ("Persistence/notes.md", "liquidgov-platform/src/http_server.cpp"):
        os.makedirs(os.path.join(tmp, os.path.dirname(rel)), exist_ok=True)
        open(os.path.join(tmp, rel), "w").write("x\n")
    git("add", "-A")
    git("commit", "-q", "-m", "base")
    base = git("rev-parse", "HEAD").strip()
    open(os.path.join(tmp, "Persistence/notes.md"), "a").write("y\n")
    git("add", "-A")
    rc, out, err = run("--staged")
    check("real --staged: a staged notes edit selects nothing", rc == 0 and out == [], (rc, out, err))
    open(os.path.join(tmp, "liquidgov-platform/src/http_server.cpp"), "a").write("y\n")
    git("add", "-A")
    rc, out, err = run("--staged")
    check("real --staged: a staged http_server.cpp edit selects the store check", rc == 0 and "lgp1_consistency.py" in out,
          (rc, out, err))
    git("commit", "-q", "-m", "two")
    head = git("rev-parse", "HEAD").strip()
    rc, out, err = run("--push", "refs/heads/master %s refs/heads/master %s\n" % (head, base))
    check("real --push: the pushed commits' files decide", rc == 0 and "lgp1_consistency.py" in out
          and "make_go_check.py" not in out, (rc, out, err))
    rc, out, err = run("--push", "refs/heads/x %s refs/heads/x %s\n" % (head, "0" * 40))
    check("real --push: a new branch with no origin/master falls back to every check",
          rc == 0 and set(out) == on_disk and "every check" in err, (rc, len(out), err))
    check("the output has plain \\n line ends (no \\r for sh to keep)", not RAW_CR, RAW_CR)
finally:
    shutil.rmtree(tmp, ignore_errors=True)
print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
