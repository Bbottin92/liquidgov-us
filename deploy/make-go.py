"""Make deploy/<to>-go.sh + <to>-remote.sh from a proven pair (<from>-go.sh / <from>-remote.sh), with the allow-list set
to the files a dry run showed, and register the remote half in naming.toml restart_allowed.
Brandon OK'd a reusable deploy script (2026-11-07); this replaces hand-copying each pair (three times so far).
Whole-token renames only (lesson: a substring rename turned MISMATCH into MISIPHONE); prints every changed line, and
runs bash -n on both outputs.
Usage: python deploy/make-go.py <from> <to> "<one-line purpose>" <file> [<file> ...]"""
import os
import re
import subprocess
import sys

if len(sys.argv) < 5:
    sys.exit(__doc__)
src_name, dst_name, purpose, files = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]
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
