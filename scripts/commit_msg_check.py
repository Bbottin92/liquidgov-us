#!/usr/bin/env python3
"""commit-msg check (Brandon 2026-11-11: "figure out why these things are able to be overlooked then correct that").
A commit that changes site code (liquidgov-platform src/, libs/, assets/, or deploy/) must say, in its message, on
lines of their own:
  Read: <what was read line by line, and what rests only on tests>
  Attacker: <for a security-related change: can the attacker in the threat case use this too? else "n/a: why">
  Undo: <how to undo it, and whether shipping it with another change ties their rollbacks>
Why: on 2026-11-10/11 side agents caught six gaps none of these questions had been asked about (a "reviewed" branch
only partly read; undo links in memory; an undo that let the hijacker log back in; a 3-day key for an old mailbox; a
"cancel" button a hijacker could press; two changes whose rollbacks were tied), and a login hole had been live since
2026-09-15. A rule alone was forgotten; this makes the commit itself refuse.
Merge commits and commits touching no site code pass. A line counts only at the start (a mention mid-sentence does
not). Prints what it checked. Usage (from .githooks/commit-msg): commit_msg_check.py <message file>"""
import fnmatch
import re
import subprocess
import sys

CODE = ["liquidgov-platform/src/*", "liquidgov-platform/libs/*", "liquidgov-platform/assets/*", "deploy/*"]
NEEDED = ["Read", "Attacker", "Undo"]


def staged_files():
    out = subprocess.run(["git", "diff", "--cached", "--name-only"], capture_output=True, text=True)
    if out.returncode != 0:
        raise RuntimeError("git diff --cached failed: " + out.stderr.strip())
    return [l for l in out.stdout.splitlines() if l.strip()]


def missing_lines(message):
    text = "\n".join(l for l in message.splitlines() if not l.startswith("#"))  # git's comment lines don't count
    return [k for k in NEEDED if not re.search(r"(?mi)^%s:[ \t]*\S" % k, text)]


def main(argv):
    if len(argv) != 1:
        print(__doc__)
        return 2
    with open(argv[0], encoding="utf-8", errors="replace") as f:
        message = f.read()
    if message.startswith("Merge "):
        print("commit_msg_check: merge commit, not checked")
        return 0
    try:
        files = staged_files()
    except (OSError, RuntimeError) as e:
        print("commit_msg_check: could not list staged files (%s); refusing" % e, file=sys.stderr)
        return 1
    code = [p for p in files if any(fnmatch.fnmatch(p, g) for g in CODE)]
    if not code:
        print("commit_msg_check: %d staged file(s), none is site code; Read/Attacker/Undo not required" % len(files))
        return 0
    miss = missing_lines(message)
    if miss:
        print("commit_msg_check: this commit changes site code (%s%s) but its message has no line starting %s.\n"
              "Add, each on its own line:\n  Read: what was read line by line vs what rests only on tests\n"
              "  Attacker: can the attacker in the threat case use this too? (or 'n/a: <why>')\n"
              "  Undo: how to undo it; does bundling tie its rollback to another change?"
              % (", ".join(code[:3]), " ..." if len(code) > 3 else "", ", ".join(m + ":" for m in miss)), file=sys.stderr)
        return 1
    print("commit_msg_check: %d site-code file(s); Read/Attacker/Undo present" % len(code))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
