"""make_go_record_length_check.py: the `RECORD ctx log:` line a generated *-go.sh prints must be accepted by ctx log,
whose target is at most 120 characters. On 2026-11-09 the sweep1 deploy's line was refused (target 124 chars: the
whole purpose went into it) and the log had to be written by hand. The notes.md bullet keeps the full purpose.
Runs make-go.py in a throwaway copy with a long purpose, runs only the generated record block with a fake sync log
and a fake `remote` (no network), and measures the target it prints. Usage: python make_go_record_length_check.py <root>"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

BASH = shutil.which("bash") or "bash"  # PATH's bash, never System32's WSL launcher (lesson 2026-11-08)
ROOT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", ".."))
LIMIT = 120  # ctx.py log: "target is N chars; keep it under 120" (measured 2026-11-09: 124 refused)
passes = fails = 0


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:300]))
    passes += bool(ok)
    fails += not ok


LONG = ("Site sweep batch 1: fixes, friendlier pages, chapter narration, map hold/double-tap/screen-reader open "
        "and a few more words to be sure")
tmp = tempfile.mkdtemp(prefix="make-go-reclen-")
try:
    os.makedirs(os.path.join(tmp, "deploy"))
    for rel in ["deploy/make-go.py", "naming.toml"] + ["deploy/care-%s.sh" % p for p in ("go", "remote")]:
        shutil.copy2(os.path.join(ROOT, rel), os.path.join(tmp, rel))
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
    for name, purpose in (("rlong", LONG), ("rshort", "Short purpose")):
        p = subprocess.run([sys.executable, os.path.join(tmp, "deploy", "make-go.py"), "auto", name, purpose, "src/http_server.cpp"],
                           capture_output=True, text=True, cwd=tmp, env=env, timeout=120)
        check("%s: make-go.py made the pair" % name, p.returncode == 0, p.stdout[-300:] + p.stderr[-300:])
        if p.returncode != 0:
            continue
        g = open(os.path.join(tmp, "deploy", "%s-go.sh" % name), encoding="utf-8").read()
        block = g[g.find("# records (added by make-go.py)"):]
        sync = os.path.join(tmp, "sync-%s.txt" % name)
        open(sync, "w").write("Deploying commit abc1234\n")
        script = ('remote() { [ "$1" = preflight ] && { echo ACTIVE=active; echo PID=4242; }; }\n'
                  'SYNCLOG="%s"\n' % sync.replace("\\", "/")) + block
        out = subprocess.run([BASH, "-c", script], capture_output=True, text=True, timeout=60).stdout
        m = re.search(r'^RECORD ctx log: .* deploy "([^"]*)" --result ok --ref abc1234$', out, re.M)
        check("%s: precondition: the block printed a RECORD ctx log line" % name, m is not None, out[-300:])
        if not m:
            continue
        target = m.group(1)
        check("%s: the ctx log target fits ctx's %d-char limit (%d)" % (name, LIMIT, len(target)), len(target) <= LIMIT, target)
        check("%s: the target still names the site and the pair" % name,
              target.startswith("liquidgov.us ") and target.endswith("(%s)" % name), target)
        notes = re.search(r"^RECORD notes\.md: .*$", out, re.M)
        check("%s: the notes.md bullet keeps the whole purpose" % name, notes is not None and purpose in notes.group(0),
              notes.group(0) if notes else out[-300:])
    check("precondition: the long purpose really is over the limit untrimmed",
          len("liquidgov.us " + LONG + " (rlong)") > LIMIT, len("liquidgov.us " + LONG + " (rlong)"))
finally:
    shutil.rmtree(tmp, ignore_errors=True)
print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
