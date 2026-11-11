"""affected_checks_master_check.py: a push that updates master runs EVERY check, whatever changed (Brandon 2026-11-10,
"only if necessary to avoid drift"): the selector's path map can miss a file a check reads, and nothing else runs
these checks, so master pushes (about as often as deploys) are where a skipped failure is caught. Other branches keep
the affected-only selection. Real script, throwaway git repo. Fails on the old code. Usage: python <this> <root>"""
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
SEL = os.path.join(ROOT, "scripts", "affected_checks.py")
TESTS = os.path.join(ROOT, "commons-daemon", "tests")
passes = fails = 0


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:300]))
    passes += bool(ok)
    fails += not ok


on_disk = {n for n in os.listdir(TESTS) if n.endswith("_check.py") or n == "lgp1_consistency.py"}
check("precondition: the selector exists", os.path.isfile(SEL), SEL)
tmp = tempfile.mkdtemp(prefix="affected-master-")
env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}


def git(*args):
    return subprocess.run(["git", "-C", tmp] + list(args), capture_output=True, text=True, env=env, check=True).stdout


def run(stdin):
    p = subprocess.run([sys.executable, SEL, tmp, "--push"], input=stdin, capture_output=True, text=True, env=env,
                       timeout=60)
    return p.returncode, p.stdout.split(), p.stderr


try:
    git("init", "-q")
    git("config", "user.email", "t@t")
    git("config", "user.name", "t")
    os.makedirs(os.path.join(tmp, "Persistence"))
    open(os.path.join(tmp, "Persistence", "notes.md"), "w").write("x\n")
    git("add", "-A")
    git("commit", "-q", "-m", "base")
    base = git("rev-parse", "HEAD").strip()
    open(os.path.join(tmp, "Persistence", "notes.md"), "a").write("y\n")
    git("commit", "-q", "-am", "notes")
    head = git("rev-parse", "HEAD").strip()
    rc, out, err = run("refs/heads/wip/x %s refs/heads/wip/x %s\n" % (head, base))
    check("precondition: a notes-only push to a wip branch runs nothing", rc == 0 and out == [], (rc, out, err))
    rc, out, err = run("refs/heads/master %s refs/heads/master %s\n" % (head, base))
    check("a notes-only push to master runs every check", rc == 0 and set(out) == on_disk, (rc, len(out), err))
    check("... and says why", "master" in err, err)
    rc, out, err = run("refs/heads/wip/x %s refs/heads/wip/x %s\nrefs/heads/master %s refs/heads/master %s\n"
                       % (head, base, head, base))
    check("master among several pushed refs still runs every check", rc == 0 and set(out) == on_disk, (rc, len(out), err))
    rc, out, err = run("refs/heads/master2 %s refs/heads/master2 %s\n" % (head, base))
    check("whole name only: 'master2' is not master", rc == 0 and out == [], (rc, out, err))
finally:
    shutil.rmtree(tmp, ignore_errors=True)
print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
