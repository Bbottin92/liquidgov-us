"""No-rollback guard for liquidgov.us deploys (Brandon 2026-11-09: static check; live commit from the server).

Rolling back a site-code deploy means restoring the previous server program (liquidgov_http.pre-<name>). That program
must still open the store the NEW one has been writing, or the rollback takes the site down: the loader throws
"LGP1: unknown record type" for a type it does not know, and "LGP1: bad <type> record" when a row has a field count its
check rejects (2026-11-07 rollback rule: the pre-flow binary could not read a Flowed pain row).

This compares liquidgov_file_store's file_store.cpp at the LIVE commit (what a rollback restores) with HEAD:
  1. every record type HEAD writes (`<< "type\t`) must be one the live loader reads (`tag == "type"`);
  2. for each such type, the live loader's field-count check must still accept what HEAD writes. Read from the two
     loaders' first `f.size()` check for the type: an exact check (!= N / == N) must be unchanged; a lower bound
     (< N / >= N, the forward-compatible types that keep extra fields) may only rise, never fall.
Refuses with the list and what a rollback would need (strip those records first, as deploy/city-undo.py does), unless
LIQUIDGOV_NO_ROLLBACK_OK="<reason + undo plan>" is set: only with Brandon's explicit OK for that deploy; logged.
Static, so it can miss a format change that keeps the same field count (a reordered field); it says what it covered.

Usage: python3 deploy/rollback_guard.py <live commit> [repo root]   -> ROLLBACK_GUARD_OK / _OVERRIDE (exit 0),
       ROLLBACK_GUARD_REFUSED (exit 1), usage/git errors (exit 2, which callers treat as a refusal).
Tests: commons-daemon/tests/rollback_guard_check.py."""
import os
import re
import shutil
import subprocess
import sys

STORE = "liquidgov-platform/libs/liquidgov_file_store/src/file_store.cpp"


def git(repo, *args):
    """git, then git.exe through WSL interop (a worktree's .git can hold a Windows path WSL git cannot read)."""
    tries = [["git", "-C", repo] + list(args)]
    if shutil.which("git.exe") and shutil.which("wslpath"):
        try:
            wp = subprocess.run(["wslpath", "-w", repo], capture_output=True, text=True, stdin=subprocess.DEVNULL).stdout.strip()
            tries.append(["git.exe", "-C", wp] + list(args))
        except OSError:  # err-ok: no wslpath means no second try; the error below names the git failure
            pass
    last = ""
    for cmd in tries:
        try:
            r = subprocess.run(cmd, capture_output=True, text=True, stdin=subprocess.DEVNULL,
                               encoding="utf-8", errors="replace")
        except OSError as e:
            last = str(e)
            continue
        if r.returncode == 0:
            return r.stdout.replace("\r\n", "\n")
        last = r.stderr.strip()
    raise RuntimeError("git %s failed: %s" % (" ".join(args), last[:300]))


def loader_checks(cpp):
    """{type: first f.size() check in its loader branch, normalised like '!=3' / '<8', or None}"""
    out = {}
    marks = [(m.start(), m.group(1)) for m in re.finditer(r'tag == "([a-z]+)"', cpp)]
    for i, (pos, tag) in enumerate(marks):
        end = marks[i + 1][0] if i + 1 < len(marks) else len(cpp)
        m = re.search(r"f\.size\(\)\s*([!<>=]=?)\s*(\d+)", cpp[pos:end])
        if tag not in out:
            out[tag] = (m.group(1) + m.group(2)) if m else None
    return out


def written(cpp):
    return set(re.findall(r'<< "([a-z]+)\\t', cpp))


def compare(old_cpp, new_cpp):
    """[(type, why)] for every record HEAD writes that the live loader would refuse."""
    old, new = loader_checks(old_cpp), loader_checks(new_cpp)
    problems = []
    for t in sorted(written(new_cpp)):
        if t not in old:
            problems.append((t, "new record type: the live loader throws 'unknown record type'"))
            continue
        a, b = old[t], new.get(t)
        if a == b:
            continue
        if a and b and a[:1] in "<>" and b[:1] in "<>" and a[:-len(re.sub(r"\D", "", a))] == b[:-len(re.sub(r"\D", "", b))]:
            na, nb = int(re.sub(r"\D", "", a)), int(re.sub(r"\D", "", b))
            if nb >= na:
                continue  # a lower bound that rose: the live loader still accepts the longer rows
            problems.append((t, "minimum field count fell (live loader %s, new %s): new short rows are refused" % (a, b)))
            continue
        problems.append((t, "field-count check changed (live loader f.size() %s, new %s): the live loader may refuse "
                            "the new rows" % (a or "none", b or "none")))
    return problems


def main(argv):
    if len(argv) < 2 or not re.fullmatch(r"[0-9a-f]{7,40}", argv[1]):
        print("usage: rollback_guard.py <live commit, 7-40 hex> [repo root]")
        return 2
    live = argv[1]
    repo = argv[2] if len(argv) > 2 else os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    try:
        same = git(repo, "rev-parse", "%s:%s" % (live, STORE)).strip() == git(repo, "rev-parse", "HEAD:" + STORE).strip()
        if same:
            print("ROLLBACK_GUARD_OK live=%s: %s is unchanged since the live commit (nothing to compare)" % (live[:7], STORE))
            return 0
        old_cpp = git(repo, "show", "%s:%s" % (live, STORE))
        new_cpp = git(repo, "show", "HEAD:" + STORE)
    except RuntimeError as e:
        override = os.environ.get("LIQUIDGOV_NO_ROLLBACK_OK", "").strip()
        if override:  # an explicit, logged human decision also covers "cannot compare"
            print("ROLLBACK_GUARD_OVERRIDE: %s (could not compare against %s: %s)" % (override, live[:7], e))
            return 0
        print("ROLLBACK_GUARD_REFUSED (cannot read the store code at %s or HEAD: %s; fetch, or check the live commit)" % (live[:7], e))
        return 2
    problems = compare(old_cpp, new_cpp)
    covered = "checked %d written record types against the live loader's %d (types + first field-count check each)" % (
        len(written(new_cpp)), len(loader_checks(old_cpp)))
    if not problems:
        print("ROLLBACK_GUARD_OK live=%s: %s" % (live[:7], covered))
        return 0
    override = os.environ.get("LIQUIDGOV_NO_ROLLBACK_OK", "").strip()
    head = "ROLLBACK_GUARD_OVERRIDE: %s" % override if override else "ROLLBACK_GUARD_REFUSED"
    print("%s live=%s: restoring the live program after this deploy would fail to open the store (%s):" % (head, live[:7], covered))
    for t, why in problems:
        print("  %s: %s" % (t, why))
    if override:
        return 0
    print("A rollback would first have to strip or rewrite those records (as deploy/city-undo.py does for the city types).")
    print('Deploy anyway only with Brandon\'s OK and an undo plan: LIQUIDGOV_NO_ROLLBACK_OK="<reason + undo plan>".')
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
