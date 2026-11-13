#!/usr/bin/env python3
"""Prebuilt deploys (deploy/PLAN-prebuilt.md, Brandon 2026-11-13: "industry standard"): the PC-side fetch refuses
anything but the program GitHub built for exactly this tree, and the prebuilt template pair installs instead of
compiling. No server, no network: a fake gh and a fake program in a throwaway git repo.
    python3 prebuilt_check.py [repo root]
  1. fetch-prebuilt.sh: no release run for this tree -> PREBUILT_REFUSED; a run for ANOTHER tree -> refused;
     the right run with a good artifact -> PREBUILT_OK and the three files in liquidgov-platform/.prebuilt/;
     checksum mismatch, BUILD-INFO for another tree, or --version for another tree -> refused.
  2. The template pair: prebuilt-go.sh runs fetch-prebuilt.sh before the dry run; prebuilt-remote.sh's build phase has
     no cmake and checks the checksum, the server's glibc and --version before installing; make-go.py switches to it
     only with LIQUIDGOV_PREBUILT=1; naming.toml allows the remote half to restart; .gitignore keeps .prebuilt/ out.
Prints 'prebuilt_check: N checks, F failed'; exit 1 if any failed."""
import hashlib
import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile

ROOT = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
checks = failed = 0


def check(ok, what, detail=""):
    global checks, failed
    checks += 1
    print(("PASS " if ok else "FAIL ") + what + ("" if ok else "  :: %s" % (detail,)))
    if not ok:
        failed += 1


def rd(rel):
    p = os.path.join(ROOT, rel)
    if not os.path.exists(p):
        return ""
    with open(p, encoding="utf-8", errors="replace") as f:
        return f.read()


# Inside a git hook git exports GIT_DIR / GIT_INDEX_FILE / ...: without dropping them, the throwaway repo's git calls
# below land in the REAL repo (2026-11-13: commits "one" and "two" on the branch being committed, core.bare=true).
for _k in [k for k in os.environ if k.startswith("GIT_")]:
    del os.environ[_k]


def sh(cmd, cwd, env=None):
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, env=env, stdin=subprocess.DEVNULL)


FETCH = os.path.join(ROOT, "deploy", "fetch-prebuilt.sh")
check(os.path.exists(FETCH), "deploy/fetch-prebuilt.sh exists")

tmp = tempfile.mkdtemp(prefix="prebuilt-check-")
try:
    repo = os.path.join(tmp, "repo")
    os.makedirs(os.path.join(repo, "liquidgov-platform", "src"))
    os.makedirs(os.path.join(repo, "deploy"))
    with open(os.path.join(repo, "liquidgov-platform", "src", "a.cpp"), "w") as f:
        f.write("int a;\n")
    if os.path.exists(FETCH):
        shutil.copy(FETCH, os.path.join(repo, "deploy", "fetch-prebuilt.sh"))
    g = lambda *a: sh(["git", "-c", "user.name=t", "-c", "user.email=t@t"] + list(a), repo)
    g("init", "-q"); g("add", "-A"); g("commit", "-qm", "one")
    sha1 = g("rev-parse", "HEAD").stdout.strip(); tree1 = g("rev-parse", "HEAD:liquidgov-platform").stdout.strip()
    with open(os.path.join(repo, "liquidgov-platform", "src", "a.cpp"), "w") as f:
        f.write("int a = 2;\n")
    g("commit", "-qam", "two")
    sha2 = g("rev-parse", "HEAD").stdout.strip(); tree2 = g("rev-parse", "HEAD:liquidgov-platform").stdout.strip()

    art = os.path.join(tmp, "artifact")

    def make_artifact(version_tree, info_tree, corrupt=False):
        shutil.rmtree(art, ignore_errors=True); os.makedirs(art)
        prog = os.path.join(art, "liquidgov_http")
        with open(prog, "w") as f:
            f.write('#!/bin/sh\n[ "$1" = --version ] && echo "liquidgov_http %s tree %s"\n' % (sha2, version_tree))
        os.chmod(prog, 0o755)
        digest = hashlib.sha256(open(prog, "rb").read()).hexdigest()
        if corrupt:
            digest = "0" * 64
        with open(os.path.join(art, "liquidgov_http.sha256"), "w") as f:
            f.write("%s  liquidgov_http\n" % digest)
        with open(os.path.join(art, "BUILD-INFO"), "w") as f:
            f.write("commit=%s\ntree=%s\nos=Ubuntu 24.04\nglibc_needed=2.38\n" % (sha2, info_tree))

    fake_gh = os.path.join(tmp, "gh")
    runs = os.path.join(tmp, "runs.txt")
    with open(fake_gh, "w") as f:
        f.write('#!/bin/bash\nif [ "$1 $2" = "run list" ]; then cat %s; exit 0; fi\n'
                'if [ "$1 $2" = "run download" ]; then d=""; while [ $# -gt 0 ]; do [ "$1" = -D ] && d=$2; shift; done; '
                'cp %s/* "$d"/; exit 0; fi\nexit 1\n' % (runs, art))
    os.chmod(fake_gh, 0o755)
    env = dict(os.environ, LIQUIDGOV_GH=fake_gh, LIQUIDGOV_GATE_REPO=repo)

    def fetch(run_lines):
        with open(runs, "w") as f:
            f.write(run_lines)
        r = sh(["bash", os.path.join(repo, "deploy", "fetch-prebuilt.sh")], repo, env)
        last = (r.stdout.strip().splitlines() or [""])[-1]
        return r.returncode, last

    make_artifact(tree2, tree2)
    rc, last = fetch("")
    check(rc == 1 and last.startswith("PREBUILT_REFUSED") and "no successful" in last, "no release run -> refused", (rc, last))
    rc, last = fetch("101 %s\n" % sha1)
    check(rc == 1 and last.startswith("PREBUILT_REFUSED"), "only a run for ANOTHER tree -> refused", (rc, last))
    rc, last = fetch("101 %s\n102 %s\n" % (sha1, sha2))
    check(rc == 0 and last == "PREBUILT_OK " + tree2, "the run for this tree, good artifact -> PREBUILT_OK", (rc, last))
    pre = os.path.join(repo, "liquidgov-platform", ".prebuilt")
    check(all(os.path.exists(os.path.join(pre, n)) for n in ("liquidgov_http", "liquidgov_http.sha256", "BUILD-INFO")),
          "...and the three files are in liquidgov-platform/.prebuilt/", os.listdir(pre) if os.path.isdir(pre) else None)
    make_artifact(tree2, tree2, corrupt=True)
    rc, last = fetch("102 %s\n" % sha2)
    check(rc == 1 and "checksum" in last, "checksum mismatch -> refused", (rc, last))
    make_artifact(tree2, tree1)
    rc, last = fetch("102 %s\n" % sha2)
    check(rc == 1 and "BUILD-INFO" in last, "BUILD-INFO for another tree -> refused", (rc, last))
    make_artifact(tree1, tree2)
    rc, last = fetch("102 %s\n" % sha2)
    check(rc == 1 and "--version" in last, "--version for another tree -> refused", (rc, last))
finally:
    shutil.rmtree(tmp, ignore_errors=True)

go, rem, mk = rd("deploy/prebuilt-go.sh"), rd("deploy/prebuilt-remote.sh"), rd("deploy/make-go.py")
check(go and go.find("fetch-prebuilt.sh") != -1 and go.find("fetch-prebuilt.sh") < go.find("DRYLOG="),
      "prebuilt-go.sh runs fetch-prebuilt.sh before the dry run")
m = re.search(r"\n  build\)\n(.*?)\n    ;;", rem, re.S)
build = m.group(1) if m else ""
check(bool(build) and "cmake" not in build, "prebuilt-remote.sh's build phase does not compile", build[:200])
check(all(k in build for k in ("sha256sum -c", "ldd --version", "--version", "install -m 0755")) and
      build.find("--version)") < build.find("install -m 0755"),
      "...and checks checksum, glibc and --version before installing", build[:300])
check(re.search(r'LIQUIDGOV_PREBUILT"?\)? == "1"', mk) is not None and 'AUTO_RESTART = "prebuilt"' in mk and
      'AUTO_NO_RESTART, AUTO_RESTART = "jbshell", "care"' in mk, "make-go.py picks the prebuilt pair only with LIQUIDGOV_PREBUILT=1")
check('"deploy/prebuilt-remote.sh"' in rd("naming.toml"), "naming.toml allows deploy/prebuilt-remote.sh to restart")
check("liquidgov-platform/.prebuilt/" in rd(".gitignore"), ".gitignore keeps liquidgov-platform/.prebuilt/ out of git")

print("prebuilt_check: %d checks, %d failed" % (checks, failed))
sys.exit(1 if failed else 0)
