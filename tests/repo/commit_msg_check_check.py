#!/usr/bin/env python3
"""Self-test of scripts/commit_msg_check.py (2026-11-11). Builds a throwaway git repo, stages files and runs the check
on messages: site code without the three lines is refused; with them it passes; a mention mid-sentence does not count;
docs-only and merge commits pass; a comment line (#) does not count. Usage: commit_msg_check_check.py [repo root]"""
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SCRIPT = os.path.join(ROOT, "scripts", "commit_msg_check.py")
passes = fails = 0
# Inside a git hook, git exports GIT_INDEX_FILE / GIT_DIR / ...: without dropping them the throwaway repo's git calls
# read the OUTER repo's index (2026-11-11: the pre-commit run of this test saw the real commit's staged files).
ENV = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}


def check(name, ok, detail=""):
    global passes, fails
    print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:300]))
    if ok:
        passes += 1
    else:
        fails += 1


def run(repo, msg):
    path = os.path.join(repo, "MSG")
    with open(path, "w", encoding="utf-8") as f:
        f.write(msg)
    p = subprocess.run([sys.executable, SCRIPT, path], cwd=repo, capture_output=True, text=True, env=ENV)
    return p.returncode, p.stdout + p.stderr


def git(repo, *a):
    subprocess.run(["git", "-C", repo] + list(a), capture_output=True, check=True, env=ENV)


GOOD = "Fix a thing\n\nRead: all of src/x.cpp\nAttacker: n/a: no security surface\nUndo: revert this commit\n"
tmp = tempfile.mkdtemp(prefix="commit-msg-check-")
try:
    check("precondition: the check script exists", os.path.isfile(SCRIPT), SCRIPT)
    git(tmp, "init", "-q")
    os.makedirs(os.path.join(tmp, "liquidgov-platform", "src"))
    with open(os.path.join(tmp, "liquidgov-platform", "src", "x.cpp"), "w") as f:
        f.write("int x;\n")
    with open(os.path.join(tmp, "README.md"), "w") as f:
        f.write("docs\n")
    git(tmp, "add", "README.md")
    rc, out = run(tmp, "docs only\n")
    check("docs-only commit passes without the lines", rc == 0, out)
    git(tmp, "add", "liquidgov-platform/src/x.cpp")
    rc, out = run(tmp, "Fix a thing\n")
    check("site code without Read/Attacker/Undo is refused", rc == 1 and "Read:" in out, out)
    rc, out = run(tmp, GOOD)
    check("site code with all three lines passes", rc == 0, out)
    rc, out = run(tmp, "Fix\n\nRead: all\nUndo: revert\nWe thought about the Attacker: none.\n")
    check("a mid-sentence 'Attacker:' does not count", rc == 1 and "Attacker:" in out, out)
    rc, out = run(tmp, "Fix\n\nRead: all\nUndo: revert\n# Attacker: n/a\n")
    check("a git comment line does not count", rc == 1, out)
    rc, out = run(tmp, "Fix\n\nRead:\nAttacker: n/a: x\nUndo: revert\n")
    check("an empty 'Read:' does not count", rc == 1 and "Read:" in out, out)
    rc, out = run(tmp, "Merge branch 'wip/x'\n")
    check("a merge commit passes", rc == 0, out)
finally:
    shutil.rmtree(tmp, ignore_errors=True)

print("passes=%d fails=%d" % (passes, fails))
sys.exit(1 if fails else 0)
