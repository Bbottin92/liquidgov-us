"""make_go_dry_label_check.py: a generated *-go.sh run with "dry" says DRY RUN, not "RESTARTING liquidgov.us"
(2026-11-08: citymod-go.sh dry printed RESTARTING while changing nothing). Runs make-go.py in a throwaway copy, then
runs only the generated banner line in bash for "dry" and "go" (no network).
Usage: python tests/repo/make_go_dry_label_check.py <repo root>"""
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
    passes += bool(ok)
    fails += not ok


tmp = tempfile.mkdtemp(prefix="make-go-dry-label-")
try:
    os.makedirs(os.path.join(tmp, "deploy"))
    for rel in ["deploy/make-go.py", "naming.toml"] + ["deploy/%s-%s.sh" % (n, p) for n in ("jbshell", "care") for p in ("go", "remote")]:
        shutil.copy2(os.path.join(ROOT, rel), os.path.join(tmp, rel))
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
    for name, f, live_word in (("dlone", "assets/x.js", "UPDATING"), ("dltwo", "src/x.cpp", "RESTARTING")):
        p = subprocess.run([sys.executable, os.path.join(tmp, "deploy", "make-go.py"), "auto", name, "p", f],
                           capture_output=True, text=True, cwd=tmp, env=env, timeout=120)
        check("%s: pair made" % name, p.returncode == 0, p.stderr[-200:])
        g = open(os.path.join(tmp, "deploy", "%s-go.sh" % name), encoding="utf-8").read()
        banner = [l for l in g.splitlines() if re.search(r"(RESTARTING|UPDATING|DRY RUN) liquidgov\.us", l)]
        check("%s: one banner line" % name, len(banner) == 1, banner)
        if len(banner) != 1:
            continue
        for arg, want, not_want in (("dry", "DRY RUN", live_word), ("go", live_word, "DRY RUN")):
            r = subprocess.run([BASH, "-c", 'set -- %s; ROOT=/x; %s' % (arg, banner[0])], capture_output=True, text=True, timeout=30)
            check("%s %s: says %s, not %s" % (name, arg, want, not_want), want in r.stdout and not_want not in r.stdout,
                  r.stdout + r.stderr)
finally:
    shutil.rmtree(tmp, ignore_errors=True)
print("make_go_dry_label_check: passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
