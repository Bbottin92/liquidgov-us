"""sync_gate_check.py: deploy/sync-to-droplet.sh runs the CI gate on every REAL sync (Brandon 2026-11-09), so the go
scripts made before make-go.py added its own gate call are gated too.
Runs the real sync-to-droplet.sh + ci-gate.sh from <repo root> in a throwaway git repo, with a fake gh (no GitHub) and
a fake rsync that only records that it was called (nothing leaves this machine). Both directions:
  refused, rsync never called: CI red; no CI run for the tree.
  goes on, rsync called: CI green; a dry run while CI is red (dry runs change nothing and are not gated); the override,
  which must log its reason.
Wiring: every deploy/*-go.sh in <repo root> syncs through sync-to-droplet.sh (else this gate would not cover it).
Usage: python tests/repo/sync_gate_check.py <repo root>   -> last line passes=N fails=M; exit 1 on any FAIL."""
import glob
import os
import shutil
import subprocess
import sys
import tempfile

BASH = shutil.which("bash") or "bash"
ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
passes = fails = 0


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:400]))
    passes += bool(ok)
    fails += not ok


def fwd(p):
    return p.replace("\\", "/")


def git(cwd, *args):
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
    r = subprocess.run(["git"] + list(args), cwd=cwd, capture_output=True, text=True, env=env)
    if r.returncode != 0:
        raise RuntimeError("git %s failed: %s" % (" ".join(args), r.stderr.strip()))
    return r.stdout.strip()


tmp = tempfile.mkdtemp(prefix="sync-gate-check-")
try:
    repo = os.path.join(tmp, "repo")
    os.makedirs(os.path.join(repo, "deploy"))
    os.makedirs(os.path.join(repo, "liquidgov-platform"))
    for f in ("sync-to-droplet.sh", "ci-gate.sh", "git-in-repo.sh"):
        src = os.path.join(ROOT, "deploy", f)
        if os.path.exists(src):
            shutil.copy2(src, os.path.join(repo, "deploy", f))
    with open(os.path.join(repo, "liquidgov-platform", "a.cpp"), "w", encoding="utf-8", newline="\n") as f:
        f.write("int a = 1;\n")
    with open(os.path.join(repo, ".gitignore"), "w", encoding="utf-8", newline="\n") as f:
        f.write("")
    git(repo, "init", "-q")
    git(repo, "config", "user.email", "t@t")
    git(repo, "config", "user.name", "t")
    git(repo, "config", "core.autocrlf", "false")
    git(repo, "add", "-A")
    git(repo, "commit", "-q", "-m", "code")
    head = git(repo, "rev-parse", "HEAD")

    bin_dir = os.path.join(tmp, "bin")
    os.makedirs(bin_dir)
    marker = os.path.join(tmp, "rsync-called.txt")
    with open(os.path.join(bin_dir, "rsync"), "w", encoding="utf-8", newline="\n") as f:
        f.write('#!/usr/bin/env bash\necho "$*" >> "%s"\nexit 0\n' % fwd(marker))
    os.chmod(os.path.join(bin_dir, "rsync"), 0o755)
    runs_file = os.path.join(tmp, "runs.txt")
    fake_gh = os.path.join(tmp, "fake-gh.sh")
    with open(fake_gh, "w", encoding="utf-8", newline="\n") as f:
        f.write('#!/usr/bin/env bash\ncat "%s"\n' % fwd(runs_file))

    def sync(runs, dry=False, extra=None):
        with open(runs_file, "w", encoding="utf-8", newline="\n") as f:
            f.write("".join(l + "\n" for l in runs))
        if os.path.exists(marker):
            os.remove(marker)
        env = {k: v for k, v in os.environ.items()
               if not k.startswith("GIT_") and not k.startswith("LIQUIDGOV_")}
        env.update({"PATH": bin_dir + os.pathsep + os.environ.get("PATH", ""), "LIQUIDGOV_GH": "bash " + fwd(fake_gh),
                    "LIQUIDGOV_DEPLOY_SSH": "true", "LIQUIDGOV_DEPLOY_KEY": "/nonexistent",
                    "LIQUIDGOV_CI_GATE_MODE": "block"})
        env.update({"LIQUIDGOV_LIVE_COMMIT": head, "LIQUIDGOV_NO_ROLLBACK_OK": "sync_gate_check tests the CI gate only"})  # Brandon OK'd this line 2026-11-09 (rollback guard)
        if dry:
            env["LIQUIDGOV_DEPLOY_DRY_RUN"] = "1"
        env.update(extra or {})
        try:
            r = subprocess.run([BASH, fwd(os.path.join(repo, "deploy", "sync-to-droplet.sh")), "nobody@127.0.0.1:/nowhere/"],
                               capture_output=True, text=True, env=env, stdin=subprocess.DEVNULL, timeout=60)
        except subprocess.TimeoutExpired:
            return 124, "TIMEOUT", False
        return r.returncode, (r.stdout + r.stderr).strip(), os.path.exists(marker)

    # precondition: the fake rsync is the one the script finds, and a plain call reaches it
    rc, out, called = sync(["1 %s completed success" % head], dry=True)
    check("precondition: a dry run reaches the fake rsync (instrument works)", rc == 0 and called, (rc, out[-300:]))
    # Python bytecode never ships (2026-11-12 and 2026-11-18: a local test or CI run left tests/browser/__pycache__/*.pyc in the
    # working tree, git ignores them, rsync copied them, and the go script's allow-list refused 25 unexpected files). The fix is
    # here, in the one script every go script syncs through, not in each go script's allow-list.
    args = open(marker, encoding="utf-8").read() if os.path.exists(marker) else ""
    check("rsync is told to exclude __pycache__ folders", "--exclude __pycache__/" in args, args[-400:])
    check("rsync is told to exclude *.pyc files", "--exclude *.pyc" in args, args[-400:])
    check("the older exclusions are still there (build, .git, *.lgp, blobs, .env.local)",
          all(e in args for e in ("--exclude build", "--exclude .git", "--exclude *.lgp", "--exclude liquidgov_blobs/", "--exclude .env.local")),
          args[-400:])

    rc, out, called = sync(["2 %s completed failure" % head])
    check("real sync, CI red -> refused before rsync", rc != 0 and not called and "CI_GATE_REFUSED" in out, (rc, called, out[-300:]))

    rc, out, called = sync([])
    check("real sync, no CI run for the tree -> refused before rsync", rc != 0 and not called and "CI_GATE_REFUSED" in out,
          (rc, called, out[-300:]))

    rc, out, called = sync(["3 %s completed success" % head])
    check("real sync, CI green -> rsync runs", rc == 0 and called and "CI_GATE_OK" in out, (rc, called, out[-300:]))

    rc, out, called = sync(["4 %s completed failure" % head], dry=True)
    check("dry run while CI is red -> not gated (changes nothing)", rc == 0 and called and "CI_GATE" not in out,
          (rc, called, out[-300:]))

    rc, out, called = sync(["5 %s completed failure" % head], extra={"LIQUIDGOV_CI_OVERRIDE": "rollback, GitHub down"})
    check("override -> rsync runs, reason logged", rc == 0 and called and "CI_GATE_OVERRIDE: rollback, GitHub down" in out,
          (rc, called, out[-300:]))

    gos = sorted(glob.glob(os.path.join(ROOT, "deploy", "*-go.sh")))
    missing = [os.path.basename(g) for g in gos if "sync-to-droplet.sh" not in open(g, encoding="utf-8").read()]
    check("wiring: every deploy/*-go.sh (%d) syncs through sync-to-droplet.sh" % len(gos), gos and not missing, missing)
finally:
    shutil.rmtree(tmp, ignore_errors=True)

print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
