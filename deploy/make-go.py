"""Make deploy/<to>-go.sh + <to>-remote.sh from a proven pair (<from>-go.sh / <from>-remote.sh), with the allow-list set
to the files a dry run showed, and register the remote half in naming.toml restart_allowed.
Brandon OK'd a reusable deploy script (2026-11-07); this replaces hand-copying each pair (three times so far).
Whole-token renames only (lesson: a substring rename turned MISMATCH into MISIPHONE); prints every changed line, and
runs bash -n on both outputs.
Usage: python deploy/make-go.py <from|auto> <to> "<one-line purpose>" <file> [<file> ...]
  <file> paths are relative to liquidgov-platform/, exactly as the dry run lists them.
  auto (2026-11-08): picks the pair by what changes. Only assets/ and tests/ files: the no-restart pair (jbshell:
  sync, chown, same-PID and live-file checks; the running server reads assets from disk per request). Anything else:
  the build-and-restart pair (care). An explicit <from> that does not fit the change is refused (an assets-only
  change on a restarting pair would drop every listener and the shared station for nothing; a code change on the
  no-restart pair would never build).
Every generated *-go.sh also takes "dry": preflight, dry run and allow-list check, then it stops before the backup
and changes nothing (open item, 2026-11-08)."""
import os
import re
import subprocess
import sys

AUTO_NO_RESTART, AUTO_RESTART = "jbshell", "care"
NO_RESTART_DIRS = ("assets/", "tests/")
DRY_LINE = 'if [ "${1:-}" = "dry" ]; then echo "DRY_ONLY_DONE: nothing on the server was changed. Run with go to deploy."; exit 0; fi'

if len(sys.argv) < 5:
    sys.exit(__doc__)
src_name, dst_name, purpose, files = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]
for f in files:
    if f.startswith("/") or "\\" in f or f.startswith("liquidgov-platform/"):
        sys.exit("file paths are relative to liquidgov-platform/ with forward slashes, as the dry run lists them: %r" % f)
needs_restart = any(not f.startswith(NO_RESTART_DIRS) for f in files)
if src_name == "auto":
    src_name = AUTO_RESTART if needs_restart else AUTO_NO_RESTART
    print("auto: %s pair (%s)" % (src_name, "a file outside assets/ and tests/ changes, so build + restart"
                                  if needs_restart else "assets/tests only, so no build and no restart"))
for n in (src_name, dst_name):
    if not re.fullmatch(r"[a-z][a-z0-9]*", n):
        sys.exit("names must be lowercase letters/digits: %r" % n)
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
d = os.path.join(root, "deploy")


def rd(p):
    with open(p, encoding="utf-8", newline="") as f:
        return f.read()


def wr(p, s):
    with open(p + ".tmp", "w", encoding="utf-8", newline="") as f:
        f.write(s)
    os.replace(p + ".tmp", p)
    if rd(p) != s:
        sys.exit("write check failed: " + p)


def rename(s):
    s = re.sub(r"(?<![A-Za-z])%s(?![a-z])" % src_name, dst_name, s)
    return re.sub(r"(?<![A-Za-z])%s(?![A-Z])" % src_name.upper(), dst_name.upper(), s)


src_go = os.path.join(d, "%s-go.sh" % src_name)
if not os.path.exists(src_go):
    sys.exit("no such pair: " + src_go)
restarts = re.search(r"^remote restart\b", rd(src_go), re.M) is not None
if restarts and not needs_restart:
    sys.exit("refusing: %s-go.sh restarts liquidgov.us, but every file is under assets/ or tests/, which the running "
             "server picks up without a restart. Use: make-go.py auto (or %s)" % (src_name, AUTO_NO_RESTART))
if needs_restart and not restarts:
    sys.exit("refusing: %s-go.sh never builds or restarts, but %s is outside assets/ and tests/, so it would not take "
             "effect. Use: make-go.py auto (or %s)" % (src_name, ", ".join(f for f in files if not f.startswith(NO_RESTART_DIRS)),
                                                        AUTO_RESTART))

made = []
for part in ("go", "remote"):
    src_path = os.path.join(d, "%s-%s.sh" % (src_name, part))
    dst_path = os.path.join(d, "%s-%s.sh" % (dst_name, part))
    if os.path.exists(dst_path):
        sys.exit("refusing to overwrite " + dst_path)
    src = rd(src_path)
    out = rename(src)
    nl = "\r\n" if "\r\n" in out else "\n"
    if part == "go":
        m = re.search(r"allow = \{.*?\n\}", out, re.S)
        if not m:
            sys.exit("no allow-list block in " + src_path)
        out = out[:m.start()] + "allow = {" + nl + "".join('    "%s",%s' % (f, nl) for f in files) + "}" + out[m.end():]
        out = re.sub(r"^# Put .*? on liquidgov\.us\.", lambda _m: "# Put " + purpose + " on liquidgov.us.", out, count=1, flags=re.M)
        # dry mode: accept "dry" as well as "go", and stop right after the allow-list check passes
        if DRY_LINE not in out:
            gate = 'if [ "${1:-}" != "go" ]; then'
            if out.count(gate) != 1:
                sys.exit("cannot add dry mode: the argument check %r appears %d times in %s" % (gate, out.count(gate), src_path))
            out = out.replace(gate, 'if [ "${1:-}" != "go" ] && [ "${1:-}" != "dry" ]; then')
            end = 'print("DRY_OK")' + nl + "PY" + nl
            if out.count(end) != 1:
                sys.exit("cannot add dry mode: the allow-list check end appears %d times in %s" % (out.count(end), src_path))
            out = out.replace(end, end + nl + "# dry: stop here, before the backup (added by make-go.py)" + nl + DRY_LINE + nl)
        if out.count(DRY_LINE) != 1 or out.index(DRY_LINE) > out.find(nl + "remote backup"):
            sys.exit("dry mode check failed for %s: the dry exit must appear once, before 'remote backup'" % dst_path)
    for a, b in zip(src.splitlines(), out.splitlines()):
        if a != b:
            print("%s-%s: %s" % (dst_name, part, b.strip()[:140]))
    wr(dst_path, out)
    made.append(dst_path)

t = os.path.join(root, "naming.toml")
s = rd(t)
m = re.search(r'^restart_allowed = \[(.*)\]$', s, re.M)
if not m:
    sys.exit("no restart_allowed line in naming.toml")
entry = '"deploy/%s-remote.sh"' % dst_name
if entry not in m.group(1):
    s = s[:m.end(1)] + ", " + entry + s[m.end(1):]
    wr(t, s)
for p in made:
    # relative, forward-slash path from the repo root: a Windows path handed to bash loses its backslashes
    rel = os.path.relpath(p, root).replace("\\", "/")
    r = subprocess.run(["bash", "-n", rel], capture_output=True, text=True, cwd=root)
    if r.returncode != 0:
        sys.exit("bash -n failed for %s: %s" % (p, r.stderr.strip()))
print("made and syntax-checked deploy/%s-go.sh and %s-remote.sh; registered in naming.toml" % (dst_name, dst_name))
