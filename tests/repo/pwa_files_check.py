#!/usr/bin/env python3
"""Still-true home-screen file checks after the one-app decision (Brandon 2026-11-07).

Keeps one group from the retired pwa_check.py: the Mail, Social and Civic manifests and icons, which still answer for
phones that already installed them. (Its Commons manifest and icon checks went with the Commons, 2026-11-13.) Leaves
out the three-app picker, no longer true; one_app_check.py covers the one-app wiring.
    python3 pwa_files_check.py <repo root>
Prints 'pwa_files_check: N checks, F failed'; exit 1 if any failed.
"""
import json, os, re, struct, sys

ROOT = sys.argv[1]
checks = failed = 0
def check(ok, what):
    global checks, failed
    checks += 1
    if not ok:
        failed += 1
        print("  FAIL " + what)
def rd(p):
    return open(os.path.join(ROOT, p), encoding="utf-8").read()

def png_info(path):
    b = open(path, "rb").read()
    if b[:8] != b"\x89PNG\r\n\x1a\n" or b[12:16] != b"IHDR":
        return None
    w, h, depth, ctype = struct.unpack(">IIBB", b[16:26])
    return (w, h, ctype)

src = __import__("runpy").run_path(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "liquidgov-platform", "tests", "browser", "server_source.py"))["read"](os.path.join(ROOT, "liquidgov-platform"))  # http_server.cpp + its src/site pieces
def manifest_of(name):
    i = src.find('path=="/manifest-%s.webmanifest"' % name)
    if i < 0:
        return None
    j = src.find("}\n", i)
    block = src[i:j]
    lits = re.findall(r'"((?:[^"\\]|\\.)*)"', block.split("http_raw(", 1)[1])
    lits = lits[2:]
    text = "".join(bytes(l, "utf-8").decode("unicode_escape") for l in lits)
    try:
        return json.loads(text)
    except Exception as e:
        return "BAD JSON: %s | %s" % (e, text[:80])

# Still served for icons already on phones. Nothing on a new page links them (one_app_check.py).
kept = {
    "mail": ("Messages", "/mail"),
    "social": ("Social", "/feed"),
    "civic": ("Civic", "/proposals"),
}
for name, (title, start) in kept.items():
    m = manifest_of(name)
    check(isinstance(m, dict), "the %s manifest route exists and its JSON parses" % name)
    if not isinstance(m, dict):
        continue
    check(m.get("name") == title and m.get("short_name") == title, "%s: name/short_name are '%s'" % (name, title))
    check(m.get("display") == "standalone", "%s: display standalone" % name)
    check(re.fullmatch(r"#[0-9a-fA-F]{6}", m.get("background_color", "")) is not None, "%s: background_color is hex" % name)
    check(re.fullmatch(r"#[0-9a-fA-F]{6}", m.get("theme_color", "")) is not None, "%s: theme_color is hex" % name)
    check(m.get("start_url") == start, "%s: start_url is %s" % (name, start))
    srcs = {i["sizes"]: i["src"] for i in m.get("icons", [])}
    check(set(srcs) == {"192x192", "512x512"}, "%s: icons are 192 and 512" % name)
    for sz, s in srcs.items():
        n = int(sz.split("x")[0])
        f = os.path.join(ROOT, "liquidgov-platform", "assets", s.lstrip("/").replace("/", os.sep))
        info = png_info(f) if os.path.exists(f) else None
        check(info is not None and info[0] == n and info[1] == n, "%s: %s exists as a %s PNG (got %s)" % (name, s, sz, info))
        check(('path=="%s"' % s) in src or ('"%s"' % s) in src, "%s: civic still serves %s" % (name, s))

print("pwa_files_check: %d checks, %d failed" % (checks, failed))
sys.exit(1 if failed else 0)
