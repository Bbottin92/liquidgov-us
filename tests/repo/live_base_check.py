"""live_base_check.py: deploy/live_base.py says which branch new work must be cut from (q91, 2026-11-17).
Why: live liquidgov.us ran code that master did not have (the wip/136-139 chain, "live ahead of GitHub"); starting map work
from master meant it lacked the live KINDS/MARKS code, and finding the right base took about eight calls.
The tool takes the live commit (--commit <sha>, from the server's DEPLOYED_COMMIT; with no --commit it reads it over ssh
the way sync-to-droplet.sh does) and says: whether master already contains it, and if not which branches do, and the
command to cut a worktree from the newest of them. Exit 0: master contains the live commit (cut from master). Exit 1: it
does not (cut from the named branch). Exit 2: it could not tell (never a quiet answer).
Cases, in a throwaway repo (master m1; wip/a = m1 + a1; wip/b = wip/a + b1):
  1 live = m1 (on master)          -> exit 0, "master contains"
  2 live = a1 (only on the branches) -> exit 1, names wip/a and wip/b, the cut command uses the tip that contains the rest (wip/b)
  3 live = an unknown commit        -> exit 2 with a message
  4 live = not a commit id          -> exit 2 with a message
  5 live = a1 but the repo has no master branch -> exit 2 (cannot tell)
Usage: python tests/repo/live_base_check.py <repo root>   -> last line passes=N fails=M; exit 1 on any FAIL."""
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
TOOL = os.path.join(ROOT, "deploy", "live_base.py")
passes = fails = 0


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:400]))
    passes += bool(ok)
    fails += not ok


ENV = {k: v for k, v in os.environ.items() if not k.startswith("GIT_") and not k.startswith("LIQUIDGOV_")}


def git(cwd, *args):
    r = subprocess.run(["git", "-c", "user.name=t", "-c", "user.email=t@t", "-c", "commit.gpgsign=false"] + list(args),
                       cwd=cwd, capture_output=True, text=True, env=ENV, stdin=subprocess.DEVNULL)
    if r.returncode != 0:
        raise RuntimeError("git %s: %s" % (" ".join(args), r.stderr.strip()))
    return r.stdout.strip()


def commit(repo, name):
    with open(os.path.join(repo, name + ".txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write(name + "\n")
    git(repo, "add", "-A")
    git(repo, "commit", "-qm", name)
    return git(repo, "rev-parse", "HEAD")


def run(repo, *args):
    r = subprocess.run([sys.executable, TOOL] + list(args) + [repo], capture_output=True, text=True, env=ENV, timeout=60,
                       stdin=subprocess.DEVNULL)
    return r.returncode, (r.stdout + r.stderr).strip()


tmp = tempfile.mkdtemp(prefix="live-base-check-")
try:
    repo = os.path.join(tmp, "repo")
    os.makedirs(repo)
    git(repo, "init", "-q")
    git(repo, "checkout", "-q", "-b", "master")
    m1 = commit(repo, "m1")
    git(repo, "checkout", "-q", "-b", "wip/a")
    a1 = commit(repo, "a1")
    git(repo, "checkout", "-q", "-b", "wip/b")
    commit(repo, "b1")
    git(repo, "checkout", "-q", "master")

    check("precondition: the tool file exists and the throwaway repo has master, wip/a, wip/b",
          os.path.exists(TOOL) and git(repo, "branch", "--list", "wip/*").count("wip/") == 2, TOOL)

    rc, out = run(repo, "--commit", m1)
    check("1 live is on master -> exit 0, says master contains it", rc == 0 and "master contains" in out, (rc, out))
    rc, out = run(repo, "--commit", a1)
    check("2 live only on the branches -> exit 1, names wip/a and wip/b, cuts from wip/b (the tip that holds the rest)",
          rc == 1 and "wip/a" in out and "wip/b" in out and "git worktree add" in out and out.rstrip().endswith("wip/b"), (rc, out))
    rc, out = run(repo, "--commit", "1234567890abcdef1234567890abcdef12345678")
    check("3 an unknown commit -> exit 2 with a message", rc == 2 and "not in this repo" in out, (rc, out))
    rc, out = run(repo, "--commit", "not-a-commit")
    check("4 not a commit id -> exit 2 with a message", rc == 2 and "commit id" in out, (rc, out))
    git(repo, "branch", "-m", "master", "trunk")
    rc, out = run(repo, "--commit", a1)
    check("5 no master branch -> exit 2 (cannot tell, never a quiet answer)", rc == 2 and "master" in out, (rc, out))
finally:
    shutil.rmtree(tmp, ignore_errors=True)

print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
