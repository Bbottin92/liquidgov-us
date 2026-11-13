#!/usr/bin/env python3
"""Still-true home-screen file checks after the one-app decision (Brandon 2026-11-07).

Keeps two groups from the retired pwa_check.py:
  the Commons manifest and the generated icons
  the Mail, Social and Civic manifests and icons, which still answer for phones that already installed them
Leaves out the /apps Commons tile and the three-app picker. Those are no longer true; one_app_check.py
covers the one-app wiring.
    python3 pwa_files_check.py <repo root>
Prints 'pwa_files_check: N checks, F failed'; exit 1 if any failed.
"""
import fnmatch, json, os, re, struct, subprocess, sys, tempfile, zlib

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

def png_pixels(path):
    b = open(path, "rb").read()
    w, h = struct.unpack(">II", b[16:24])
    i = 8
    idat = b""
    while i < len(b):
        n = struct.unpack(">I", b[i:i + 4])[0]
        t = b[i + 4:i + 8]
        if t == b"IDAT":
            idat += b[i + 8:i + 8 + n]
        i += 12 + n
    raw = zlib.decompress(idat)
    stride = 1 + 3 * w
    return [[tuple(raw[y * stride + 1 + 3 * x:y * stride + 4 + 3 * x]) for x in range(w)] for y in range(h)]

CM = os.path.join(ROOT, "commons")
html = rd("commons/index.html")
man = json.loads(rd("commons/manifest.webmanifest"))
check(man.get("name") == "The Commons" and man.get("short_name") == "Commons", "the Commons manifest is named 'The Commons' / 'Commons'")
check(man.get("start_url") == "/commons/" and man.get("scope") == "/commons/", "start_url and scope are /commons/")
check(man.get("display") == "standalone", "display is standalone")
theme = (re.search(r'<meta name="theme-color" content="(#[0-9a-fA-F]{6})"', html) or [None, None])[1]
check(theme is not None and man.get("theme_color") == theme, "the manifest theme_color is the page's own theme-color (%s)" % theme)
check(re.fullmatch(r"#[0-9a-fA-F]{6}", man.get("background_color", "")) is not None, "background_color is a hex colour")
icons = man.get("icons", [])
def has(size, purpose):
    return any(i.get("sizes") == "%dx%d" % (size, size) and i.get("purpose") == purpose and i.get("type") == "image/png" for i in icons)
check(has(192, "any") and has(512, "any"), "icons: 192 and 512 'any'")
check(has(512, "maskable"), "icons: a 512 'maskable'")
for i in icons:
    p = os.path.join(CM, i["src"])
    want = int(i["sizes"].split("x")[0])
    info = png_info(p) if os.path.exists(p) else None
    check(info is not None and info[0] == want and info[1] == want, "icon %s exists, is a real PNG of the declared %s (got %s)" % (i["src"], i["sizes"], info))
    check(info is not None and info[2] == 2, "icon %s is opaque RGB" % i["src"])
tag = re.findall(r'<link rel="manifest" href="([^"]+)">', html)
check(tag == ["/commons/manifest.webmanifest"], "index.html links the manifest exactly once: %s" % tag)
at = re.findall(r'<link rel="apple-touch-icon" href="([^"]+)">', html)
check(len(at) == 1 and at[0].startswith("/commons/") and png_info(os.path.join(CM, at[0][len("/commons/"):])) == (180, 180, 2),
      "index.html has one apple-touch-icon and it is a 180x180 opaque PNG: %s" % at)
check('<meta name="apple-mobile-web-app-capable" content="yes">' in html and '<meta name="mobile-web-app-capable" content="yes">' in html
      and '<meta name="apple-mobile-web-app-title" content="The Commons">' in html, "the standalone meta tags are there, titled 'The Commons'")
with tempfile.TemporaryDirectory() as td:
    subprocess.run([sys.executable, os.path.join(ROOT, "Persistence/TheCommons/art/make_icons.py"), td], check=True, stdout=subprocess.DEVNULL)
    for n in ("apple-touch-icon.png", "icon-192.png", "icon-512.png"):
        check(open(os.path.join(td, n), "rb").read() == open(os.path.join(CM, n), "rb").read(),
              "commons/%s is byte-identical to what make_icons.py generates" % n)
px = png_pixels(os.path.join(CM, "icon-192.png"))
check(len({c for row in px for c in row}) >= 8, "the icon is a real picture (many colours), not a flat fill")
check((0xf7, 0xf1, 0xe4) in {c for row in px for c in row}, "the notice-board paper colour is in it")
edge = {px[y][x] for y in range(len(px)) for x in range(len(px)) if x < 20 or y < 20 or x >= len(px) - 20 or y >= len(px) - 20}
check((0xf7, 0xf1, 0xe4) not in edge and (0x4a, 0x3a, 0x28) not in edge, "the notice-board and post stay out of the outer margin")
lib = rd("commons-daemon/deploy/lib.sh")
arr = re.search(r"^CLIENT_EXCLUDES=\((.*?)\)", lib, re.S | re.M)
check(arr is not None, "lib.sh's CLIENT_EXCLUDES array was found")
ex = re.findall(r"--exclude[= ]'?([^' \n)]+)'?", arr.group(1)) if arr else []
check(len(ex) >= 1, "and it lists its excludes (%s)" % ex)
for n in ("manifest.webmanifest", "apple-touch-icon.png", "icon-192.png", "icon-512.png"):
    check(not any(fnmatch.fnmatch(n, e.strip("/")) for e in ex), "the client sync does not exclude %s" % n)

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
