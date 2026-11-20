"""Next free e2e port for a new liquidgov test suite (Brandon OK'd 2026-11-08: "Ports + worktree tidy tool").
On 2026-11-08 city_moderation.sh took 18098, the last free port in naming.toml's 18080-18099, found by hand.
Reads the same rules naming_check.py enforces (its parse_toml and port_of, so they cannot drift apart): the first port
in [port_min, port_max] that no *.sh under liquidgov-platform/tests (e2e/, rollback/, ...) mentions (PORT=, FAKE=, any number in the range). When the range is full it says so (exit 3); with --widen it raises
port_max by 20 in naming.toml (the one line, whole-token match, verified by reading it back) and prints the new port.
Usage: python scripts/next_port.py [--widen]"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tests" / "repo"))
import naming_check as nc  # noqa: E402


def state():
    cfg = nc.load(str(ROOT))  # naming_check.load(<repo root>) reads <root>/naming.toml
    e2e = cfg["e2e"]
    lo, hi = int(e2e["port_min"]), int(e2e["port_max"])
    # every suite under liquidgov-platform/tests (e2e/, rollback/, ...): on 2026-11-19 reading e2e/ only offered 18143,
    # which tests/rollback/portraits_undo.sh already used
    d = (ROOT / e2e["root"]).parent
    used = set()
    for p in d.rglob("*.sh"):
        used |= ports_in(p, lo, hi)
    return lo, hi, used


def ports_in(path, lo, hi):
    """Every number in [lo, hi] a suite mentions, not only its PORT= line: on 2026-11-20 reading PORT= alone offered
    18152, which e2e/outside_wikipedia.sh already binds as FAKE=18152 (its stand-in server). A number in a comment
    counts too; skipping a port that is merely mentioned is cheaper than two suites binding the same one."""
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return set()
    found = {int(n) for n in re.findall(r"(?<![0-9])[0-9]+(?![0-9])", text) if lo <= int(n) <= hi}
    own = nc.port_of(str(path))  # PORT= stays counted whatever the rule above finds (naming_check's own reading)
    return found | ({own} if own is not None else set())


def main(argv):
    lo, hi, used = state()
    free = [p for p in range(lo, hi + 1) if p not in used]
    if free:
        print(free[0])
        return 0
    if "--widen" not in argv:
        print("next_port: every port in %d-%d is taken; rerun with --widen to raise port_max by 20" % (lo, hi), file=sys.stderr)
        return 3
    toml = ROOT / "naming.toml"
    # raw bytes in and out: read_text/write_text turned the LF file into CRLF on Windows, and make-go.py's line-anchored
    # regex then stopped matching (caught by make_go_check at commit, 2026-11-08)
    with open(toml, encoding="utf-8", newline="") as f:
        text = f.read()
    new, n = re.subn(r"(?m)^port_max = %d(?=\r?$)" % hi, "port_max = %d" % (hi + 20), text)
    if n != 1:
        print("next_port: found %d 'port_max = %d' lines in naming.toml (want 1); nothing changed" % (n, hi), file=sys.stderr)
        return 1
    with open(toml, "w", encoding="utf-8", newline="") as f:
        f.write(new)
    if state()[1] != hi + 20:
        print("next_port: naming.toml did not read back with port_max = %d" % (hi + 20), file=sys.stderr)
        return 1
    print(hi + 1)
    print("next_port: widened port_max %d -> %d in naming.toml (commit it with the new suite)" % (hi, hi + 20), file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
