"""make_go_check.py: deploy/make-go.py picks the right pair and gives every script a dry mode (2026-11-08).

Why: the Juketube shell deploy copied a pair that always builds and restarts, for a change to two asset files that the
running server reads from disk per request; a side agent caught it before the go (lessons 2026-11-08). The open item
"give generated *-go.sh a 'dry' mode" is the other half. This check runs make-go.py in a throwaway copy of the files it
reads (never the real deploy/ folder, never the server) and checks:
  auto + assets/tests only -> no-restart pair; auto + src -> build-and-restart pair; a mismatched explicit pair is
  refused both ways; bad paths are refused; the generated go.sh accepts "dry" (stopping before the backup) and still
  refuses anything else; the remote half is registered in naming.toml.
Usage: python commons-daemon/tests/make_go_check.py <repo root>   (exit 0 only when every case passed)"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

BASH = shutil.which("bash") or "bash"
ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
passes = fails = 0


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:300]))
    if ok:
        passes += 1
    else:
        fails += 1


def rd(p):
    with open(p, encoding="utf-8", newline="") as f:
        return f.read()


tmp = tempfile.mkdtemp(prefix="make-go-check-")
try:
    os.makedirs(os.path.join(tmp, "deploy"))
    need = ["deploy/make-go.py", "naming.toml"] + ["deploy/%s-%s.sh" % (n, p) for n in ("jbshell", "care") for p in ("go", "remote")]
    for rel in need:
        src = os.path.join(ROOT, rel)
        if not os.path.exists(src):
            check("precondition: %s exists" % rel, False, src)
            continue
        shutil.copy2(src, os.path.join(tmp, rel))
    check("precondition: the no-restart template never restarts",
          not re.search(r"^remote restart\b", rd(os.path.join(tmp, "deploy/jbshell-go.sh")), re.M))
    check("precondition: the bash used is not Windows' System32 WSL launcher",
          "system32" not in BASH.lower(), BASH)
    check("precondition: the restart template restarts",
          bool(re.search(r"^remote restart\b", rd(os.path.join(tmp, "deploy/care-go.sh")), re.M)))
    # the generator is run from Windows Python under Git Bash or from WSL; it calls bash -n itself
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}

    def make(*args):
        p = subprocess.run([sys.executable, os.path.join(tmp, "deploy", "make-go.py")] + list(args), capture_output=True,
                           text=True, cwd=tmp, env=env, timeout=120)
        return p.returncode, p.stdout + p.stderr

    def go(name, arg):
        # PATH's bash, never Windows' System32 WSL bash.exe (a bare "bash" found that one first and failed at random)
        p = subprocess.run([BASH, "deploy/%s-go.sh" % name] + ([arg] if arg else []), capture_output=True, text=True,
                           cwd=tmp, env=dict(env, LIQUIDGOV_DEPLOY_KEY=""), timeout=60)
        return p.returncode, p.stdout + p.stderr

    # 1. auto, assets only -> no restart
    rc, out = make("auto", "aone", "an assets change", "assets/juketube.js", "assets/juketube.css")
    g = rd(os.path.join(tmp, "deploy/aone-go.sh")) if rc == 0 else ""
    check("auto + assets only makes a pair", rc == 0 and "auto: jbshell pair" in out, out[-300:])
    check("...that never builds or restarts", g and not re.search(r"^remote (build|restart)\b", g, re.M), g[-200:])
    check("...with exactly the given allow-list",
          g and re.search(r'allow = \{\s*"assets/juketube\.js",\s*"assets/juketube\.css",\s*\}', g) is not None)
    # 2. auto, a source file -> build + restart
    rc, out = make("auto", "atwo", "a code change", "src/http_server.cpp", "tests/e2e/x_y.sh")
    g2 = rd(os.path.join(tmp, "deploy/atwo-go.sh")) if rc == 0 else ""
    check("auto + a src file makes the build-and-restart pair", rc == 0 and "auto: care pair" in out, out[-300:])
    check("...that builds and restarts", g2 and re.search(r"^remote build\b", g2, re.M) and re.search(r"^remote restart\b", g2, re.M))
    # 3. explicit mismatches are refused, both ways
    rc, out = make("care", "athree", "x", "assets/a.js")
    check("an assets-only change on the restarting pair is refused", rc != 0 and "refusing" in out and
          not os.path.exists(os.path.join(tmp, "deploy/athree-go.sh")), out[-300:])
    rc, out = make("jbshell", "afour", "x", "src/http_server.cpp")
    check("a code change on the no-restart pair is refused", rc != 0 and "refusing" in out and
          not os.path.exists(os.path.join(tmp, "deploy/afour-go.sh")), out[-300:])
    rc, out = make("jbshell", "afive", "x", "tests/e2e/a_b.sh", "assets/x.css")
    check("tests + assets on the no-restart pair is allowed", rc == 0, out[-300:])
    # 4. bad paths
    for bad in ("liquidgov-platform/assets/a.js", "/opt/liquidgov/assets/a.js", "assets\\a.js"):
        rc, out = make("auto", "asix", "x", bad)
        check("a bad path is refused: %r" % bad, rc != 0 and not os.path.exists(os.path.join(tmp, "deploy/asix-go.sh")), out[-200:])
    # 5. dry mode, both ways, without any network: the gate accepts dry and the script then stops for the missing key
    for name, text in (("aone", g), ("atwo", g2)):
        if not text:
            check("%s: dry mode (no script to check)" % name, False)
            continue
        dry_at = text.find("DRY_ONLY_DONE")
        backup_at = text.find("\nremote backup")
        check("%s: the dry exit comes once, after the allow-list check and before the backup" % name,
              text.count("DRY_ONLY_DONE") == 1 and text.find('print("DRY_OK")') < dry_at < backup_at, (dry_at, backup_at))
        rc, out = go(name, "dry")
        check("%s: 'dry' passes the argument gate (stops at the missing key, exit 2)" % name,
              rc == 2 and "LIQUIDGOV_DEPLOY_KEY" in out and "Refusing" not in out, out[-200:])
        for arg in ("", "Go", "dryrun", "go dry"):
            rc, out = go(name, arg)
            check("%s: %r is refused at the gate" % (name, arg), rc == 2 and "Refusing" in out, out[-200:])
    # 6. registered for the restart-bypass check
    t = rd(os.path.join(tmp, "naming.toml"))
    check("the new remote halves are registered in naming.toml",
          '"deploy/aone-remote.sh"' in t and '"deploy/atwo-remote.sh"' in t, re.search(r"restart_allowed = .*", t))
finally:
    shutil.rmtree(tmp, ignore_errors=True)
print("make_go_check: passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
