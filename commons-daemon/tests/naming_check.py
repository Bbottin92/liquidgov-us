#!/usr/bin/env python3
"""Naming standard for new LiquidGov files (Brandon 2026-11-07). Reads naming.toml at the repo root.

    python3 naming_check.py <repo root>
    python3 naming_check.py --self-test

A new file that breaks the pattern fails. Paths listed as grandfather keep the names they already
had. Prints 'naming_check: N checks, F failed' and exits 1 if any failed.
--self-test builds a tiny tree with a badly named file and a shared port, and passes only if both fail.
"""
import os, re, subprocess, sys, tempfile

def parse_toml(text):
    """The small subset this file uses: [section], key = "str", key = N, key = ["a", "b"]."""
    data, section = {}, ""
    for raw in text.splitlines():
        line = raw.split("#", 1)[0].strip()
        if not line:
            continue
        if line.startswith("[") and line.endswith("]"):
            section = line[1:-1].strip()
            data.setdefault(section, {})
            continue
        key, val = [p.strip() for p in line.split("=", 1)]
        if val.startswith("["):
            inner = val[1:-1].strip()
            items = [] if not inner else [s.strip().strip('"') for s in inner.split(",")]
            parsed = items
        elif val.startswith('"'):
            parsed = val.strip()[1:-1].replace('\\\\', '\\')
        else:
            parsed = int(val)
        data.setdefault(section, {})[key] = parsed
    return data

def load(root):
    path = os.path.join(root, "naming.toml")
    with open(path, encoding="utf-8") as f:
        return parse_toml(f.read())

class Tally:
    def __init__(self):
        self.checks = self.failed = 0
        self.lines = []
    def check(self, ok, what):
        self.checks += 1
        if not ok:
            self.failed += 1
            self.lines.append("  FAIL " + what)

def files_named(directory, end):
    if not os.path.isdir(directory):
        return []
    return sorted(n for n in os.listdir(directory) if n.endswith(end) and os.path.isfile(os.path.join(directory, n)))

def port_of(path):
    try:
        text = open(path, encoding="utf-8", errors="replace").read()
    except OSError:
        return None
    m = re.search(r"^PORT=(\d+)", text, re.M)
    return int(m.group(1)) if m else None

def restarts(path):
    try:
        lines = open(path, encoding="utf-8", errors="replace").read().splitlines()
    except OSError:
        return False
    for line in lines:
        s = line.strip()
        if s.startswith("#"):
            continue
        if re.match(r"systemctl restart\b", s) or re.search(r"[;&|]\s*systemctl restart\b", s):
            return True
    return False

def branches_of(root):
    r = subprocess.run(["git", "-C", root, "for-each-ref", "--format=%(refname:short)", "refs/heads"],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return None
    return [b for b in r.stdout.split() if b]

def scan(root, cfg, branches):
    t = Tally()
    g = cfg.get("grandfather", {})

    def pattern_scan(section, end):
        spec = cfg[section]
        directory = os.path.join(root, spec["root"].replace("/", os.sep))
        pat = re.compile(spec["pattern"])
        names = files_named(directory, end)
        allowed = set(g.get(section, []))
        for name in names:
            if name in allowed:
                t.check(True, "%s/%s is grandfathered" % (spec["root"], name))
            else:
                t.check(pat.match(name) is not None, "%s/%s matches %s" % (spec["root"], name, spec["pattern"]))
        for name in allowed:
            t.check(os.path.isfile(os.path.join(directory, name)),
                    "grandfathered %s/%s still exists (drop it from naming.toml if it was removed)" % (spec["root"], name))
        return names

    e2e_names = pattern_scan("e2e", ".sh")
    pattern_scan("cpp", ".cpp")
    pattern_scan("ci_sh", ".sh")
    pattern_scan("ci_py", ".py")
    deploy_names = pattern_scan("deploy", ".sh")

    e2e_dir = os.path.join(root, cfg["e2e"]["root"].replace("/", os.sep))
    seen = {}
    lo, hi = int(cfg["e2e"]["port_min"]), int(cfg["e2e"]["port_max"])
    for name in e2e_names:
        port = port_of(os.path.join(e2e_dir, name))
        t.check(port is not None, "%s has a PORT= line" % name)
        if port is None:
            continue
        t.check(lo <= port <= hi, "%s port %s is inside %s-%s" % (name, port, lo, hi))
        seen.setdefault(port, []).append(name)
    for port, names in sorted(seen.items()):
        t.check(len(names) == 1, "port %s belongs to one e2e script (got %s)" % (port, ", ".join(names)))

    restart_re = re.compile(cfg["deploy"]["restart_pattern"])
    areas = {}
    deploy_dir = os.path.join(root, cfg["deploy"]["root"].replace("/", os.sep))
    for name in deploy_names:
        if not restarts(os.path.join(deploy_dir, name)):
            continue
        m = restart_re.match(name)
        t.check(m is not None, "%s is the one live-restart script for its area (%s)" % (name, cfg["deploy"]["restart_pattern"]))
        if m:
            areas.setdefault(m.group(1), []).append(name)
    for area, names in sorted(areas.items()):
        t.check(len(names) == 1, "area %s has one live-restart script (got %s)" % (area, ", ".join(names)))

    frozen = cfg.get("frozen", {})
    server = ""
    server_path = os.path.join(root, "liquidgov-platform", "src", "http_server.cpp")
    if os.path.isfile(server_path):
        server = open(server_path, encoding="utf-8", errors="replace").read()
    t.check(frozen.get("button", "") in server, "the MAINstream button text is unchanged")
    for url in frozen.get("assets", []):
        t.check(url in server, "shipped asset URL %s is still served" % url)
    tags = frozen.get("store_tags_check", "")
    t.check(bool(tags) and os.path.isfile(os.path.join(root, tags.replace("/", os.sep))),
            "LGP1 store tags stay with %s" % tags)
    hook = os.path.join(root, ".githooks", "pre-push")
    hook_text = open(hook, encoding="utf-8", errors="replace").read() if os.path.isfile(hook) else ""
    t.check("lgp1_consistency.py" in hook_text and "naming_check.py" in hook_text,
            "pre-push still runs the store-tag check and this naming check")

    pat = re.compile(cfg["branches"]["pattern"])
    allowed_b = set(g.get("branches", []))
    if branches is None:
        t.check(False, "git could not list local branches")
    else:
        for name in branches:
            if name in allowed_b:
                t.check(True, "branch %s is grandfathered" % name)
            else:
                t.check(pat.match(name) is not None, "branch %s matches %s" % (name, cfg["branches"]["pattern"]))
    return t

def report(t):
    print("\n".join(t.lines))
    print("naming_check: %d checks, %d failed" % (t.checks, t.failed))
    return 1 if t.failed else 0

def self_test():
    """A badly named new file, and two scripts on one port, must each fail. A clean tiny tree must pass."""
    with tempfile.TemporaryDirectory() as td:
        def tree(bad_name, ports, branches):
            for rel in ("liquidgov-platform/tests/e2e", "liquidgov-platform/tests", "liquidgov-platform/src",
                        "commons-daemon/tests", "deploy", ".githooks"):
                os.makedirs(os.path.join(td, rel.replace("/", os.sep)), exist_ok=True)
            e2e = os.path.join(td, "liquidgov-platform", "tests", "e2e")
            for old in os.listdir(e2e):
                os.remove(os.path.join(e2e, old))
            open(os.path.join(td, "naming.toml"), "w", encoding="utf-8", newline="\n").write(
                '[e2e]\nroot = "liquidgov-platform/tests/e2e"\npattern = "^[a-z0-9]+_[a-z0-9_]+\\\\.sh$"\nport_min = 18080\nport_max = 18099\n'
                '[cpp]\nroot = "liquidgov-platform/tests"\npattern = "^[a-z0-9_]+_test\\\\.cpp$"\n'
                '[ci_sh]\nroot = "commons-daemon/tests"\npattern = "^test_[a-z0-9_]+\\\\.sh$"\n'
                '[ci_py]\nroot = "commons-daemon/tests"\npattern = "^[a-z0-9_]+_check\\\\.py$"\n'
                '[deploy]\nroot = "deploy"\npattern = "^[a-z0-9]+(?:-[a-z0-9]+)+\\\\.sh$"\nrestart_pattern = "^([a-z0-9]+)-remote\\\\.sh$"\n'
                '[branches]\npattern = "^wip/[0-9]+-[a-z0-9-]+$"\n'
                '[grandfather]\ne2e = []\ncpp = []\nci_sh = []\nci_py = ["lgp1_consistency.py"]\ndeploy = []\nbranches = ["master"]\n'
                '[frozen]\nbutton = "MAINstream</button>"\nstore_tags_check = "commons-daemon/tests/lgp1_consistency.py"\n'
                'assets = ["/manifest.webmanifest"]\n')
            open(os.path.join(td, "liquidgov-platform", "src", "http_server.cpp"), "w", encoding="utf-8", newline="\n").write(
                'MAINstream</button> "/manifest.webmanifest"\n')
            open(os.path.join(td, "commons-daemon", "tests", "lgp1_consistency.py"), "w", encoding="utf-8", newline="\n").write("# tags\n")
            open(os.path.join(td, ".githooks", "pre-push"), "w", encoding="utf-8", newline="\n").write(
                "lgp1_consistency.py naming_check.py\n")
            open(os.path.join(td, "liquidgov-platform", "tests", "e2e", "city_flow.sh"), "w", encoding="utf-8", newline="\n").write(
                "PORT=%d\n" % ports[0])
            open(os.path.join(td, "liquidgov-platform", "tests", "e2e", bad_name), "w", encoding="utf-8", newline="\n").write(
                "PORT=%d\n" % ports[1])
            open(os.path.join(td, "deploy", "city-remote.sh"), "w", encoding="utf-8", newline="\n").write(
                "systemctl restart liquidgov\n")
            return scan(td, load(td), branches)
        bad = tree("badly.sh", (18093, 18094), ["master"])
        name_failed = any("badly.sh" in line for line in bad.lines)
        clash = tree("city_keep.sh", (18093, 18093), ["master"])
        port_failed = any("belongs to one e2e script" in line for line in clash.lines)
        clean = tree("city_keep.sh", (18093, 18094), ["master", "wip/2-naming"])
        print("naming_check self-test: bad-name fail=%s port-clash fail=%s clean failed=%d" % (
            name_failed, port_failed, clean.failed))
        if name_failed and port_failed and clean.failed == 0:
            print("naming_check self-test: passed")
            return 0
        print("\n".join(clean.lines))
        return 1

def main(argv):
    if "--self-test" in argv:
        return self_test()
    if len(argv) != 2:
        print("usage: naming_check.py <repo root> | --self-test", file=sys.stderr)
        return 2
    root = argv[1]
    t = scan(root, load(root), branches_of(root))
    return report(t)

if __name__ == "__main__":
    sys.exit(main(sys.argv))
