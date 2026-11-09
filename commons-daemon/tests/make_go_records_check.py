"""make_go_records_check.py: every generated *-go.sh ends by printing its own records from the run's real output
(Brandon 2026-11-08, "Deploy writes its records"): the ctx log line and the notes.md bullet, with the deployed commit
(from the sync log) and the new PID (a fresh preflight read). On 2026-11-08 a log line written from memory claimed
work that had not happened. The block is only reached after the public checks pass (set -e; city-live-check exits 1).
Runs make-go.py in a throwaway copy, then runs ONLY the generated record block in bash with a fake sync log and a fake
`remote` (no network). Usage: python commons-daemon/tests/make_go_records_check.py <repo root>"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

BASH = shutil.which("bash") or "bash"  # PATH's bash, never System32's WSL launcher (lesson 2026-11-08)
ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
passes = fails = 0


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:300]))
    passes += bool(ok)
    fails += not ok


tmp = tempfile.mkdtemp(prefix="make-go-records-")
try:
    os.makedirs(os.path.join(tmp, "deploy"))
    for rel in ["deploy/make-go.py", "naming.toml"] + ["deploy/%s-%s.sh" % (n, p) for n in ("jbshell", "care") for p in ("go", "remote")]:
        shutil.copy2(os.path.join(ROOT, rel), os.path.join(tmp, rel))
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
    for name, files in (("rone", ["assets/x.js"]), ("rtwo", ["src/http_server.cpp"])):
        p = subprocess.run([sys.executable, os.path.join(tmp, "deploy", "make-go.py"), "auto", name, "Purpose with \"quotes\" for " + name]
                           + files, capture_output=True, text=True, cwd=tmp, env=env, timeout=120)
        check("%s: make-go.py made the pair" % name, p.returncode == 0, p.stdout[-300:] + p.stderr[-300:])
        if p.returncode != 0:
            continue
        g = open(os.path.join(tmp, "deploy", "%s-go.sh" % name), encoding="utf-8").read()
        i, j = g.find("# records (added by make-go.py)"), g.find("echo CHECK_DONE")
        check("%s: the record block comes after the public checks" % name, 0 <= j < i, (i, j))
        block = g[i:]
        sync = os.path.join(tmp, "sync-%s.txt" % name)
        open(sync, "w").write("Deploying commit abc1234\nSyncing ...\n")
        script = ('remote() { [ "$1" = preflight ] && { echo ACTIVE=active; echo PID=4242; }; }\n'
                  'SYNCLOG="%s"\n' % sync.replace("\\", "/")) + block
        r = subprocess.run([BASH, "-c", script], capture_output=True, text=True, timeout=60)
        out = r.stdout
        check("%s: the block runs" % name, r.returncode == 0, r.stderr[-300:])
        check("%s: ctx log line carries the real commit" % name,
              re.search(r'^RECORD ctx log: python \S*ctx\.py log <who> deploy "liquidgov\.us .*%s.*" --result ok --ref abc1234$' % name,
                        out, re.M) is not None, out)
        check("%s: notes bullet carries commit, PID and the checks" % name,
              re.search(r"^RECORD notes\.md: - \*\*\d{4}-\d\d-\d\d \d\d:\d\d PT -- .*%s.* LIVE \(commit `abc1234`, PID 4242, public checks FAILS 0" % name,
                        out, re.M) is not None, out)
        check("%s: quotes in the purpose cannot break the shell line" % name, '\\"quotes\\"' not in out and "quotes" in out, out)
        open(sync, "w").write("no commit line here\n")
        r = subprocess.run([BASH, "-c", script], capture_output=True, text=True, timeout=60)
        check("%s: a sync log without a commit says so instead of printing an empty ref" % name,
              "RECORD_FAILED" in r.stdout and "--ref " not in r.stdout, r.stdout)
finally:
    shutil.rmtree(tmp, ignore_errors=True)
print("make_go_records_check: passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
