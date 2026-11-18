"""make_go_postcheck_check.py: the no-restart go script's post-deploy live check covers the files the deploy ships (q88,
2026-11-17). Why: the jbshell template compared the live juketube.js/css with the local ones, whatever the deploy shipped;
mapclarity1 shipped lgmap.css/js and lgmap_walk.js and the hashes had to be checked by hand.
Runs the real deploy/make-go.py on copies of the jbshell/care pairs in a throwaway folder (nothing is sent anywhere) and
reads the generated script:
  a pair for assets/lgmap.css + assets/lgmap.js + a test file: the live loop lists lgmap.css lgmap.js (the test file is not
      served from /assets/ and is not in it) and no longer juketube.js/css as its only subjects
  another pair, for assets/site.css and assets/sub/x.js: its own list, sub-folder path kept (served at /assets/sub/x.js)
  a tests-only pair: the loop is left as the template has it (nothing served changes), and the script is still valid bash
  a pair that restarts (care, src/ file): the generator does not break on a template without the loop
Usage: python tests/repo/make_go_postcheck_check.py <repo root>   -> last line passes=N fails=M; exit 1 on any FAIL."""
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
passes = fails = 0


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:400]))
    passes += bool(ok)
    fails += not ok


def rd(p):
    with open(p, encoding="utf-8", newline="") as f:
        return f.read()


LOOP = re.compile(r"^for f in ([A-Za-z0-9._/ -]+); do$", re.M)

tmp = tempfile.mkdtemp(prefix="make-go-postcheck-")
try:
    os.makedirs(os.path.join(tmp, "deploy"))
    need = ["deploy/make-go.py", "naming.toml"] + ["deploy/%s-%s.sh" % (n, p) for n in ("jbshell", "care") for p in ("go", "remote")]
    missing = [r for r in need if not os.path.exists(os.path.join(ROOT, r))]
    for rel in need:
        if os.path.exists(os.path.join(ROOT, rel)):
            shutil.copy2(os.path.join(ROOT, rel), os.path.join(tmp, rel))
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}

    def make(*args):
        p = subprocess.run([sys.executable, os.path.join(tmp, "deploy", "make-go.py")] + list(args), capture_output=True,
                           text=True, cwd=tmp, env=env, timeout=120)
        return p.returncode, p.stdout + p.stderr

    def loop_of(name):
        m = LOOP.search(rd(os.path.join(tmp, "deploy", "%s-go.sh" % name)))
        return m.group(1).split() if m else None

    base = loop_of("jbshell")
    check("precondition: the template files exist and the jbshell template has a live-hash loop (instrument works)",
          not missing and base == ["juketube.js", "juketube.css"], (missing, base))

    rc, out = make("auto", "pone", "x", "assets/lgmap.css", "assets/lgmap.js", "tests/browser/walker_x_check.py")
    got = loop_of("pone") if rc == 0 else None
    check("an assets deploy's live check lists the shipped asset files (and not the test file)",
          rc == 0 and got == ["lgmap.css", "lgmap.js"], (rc, got, out[-200:]))

    rc, out = make("auto", "ptwo", "x", "assets/site.css", "assets/sub/x.js")
    got = loop_of("ptwo") if rc == 0 else None
    check("another deploy gets its own list, including a file in a sub-folder of assets/ (served at /assets/sub/x.js)",
          rc == 0 and got == ["site.css", "sub/x.js"], (rc, got, out[-200:]))

    rc, out = make("auto", "pthree", "x", "tests/e2e/a_b.sh")
    got = loop_of("pthree") if rc == 0 else None
    check("a tests-only deploy leaves the template's loop as it is (nothing served changes)",
          rc == 0 and got == base, (rc, got, out[-200:]))

    rc, out = make("auto", "pfour", "x", "src/http_server.cpp")
    check("a restarting deploy (care template) is generated without error", rc == 0, out[-300:])
finally:
    shutil.rmtree(tmp, ignore_errors=True)

print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
