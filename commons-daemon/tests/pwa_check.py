#!/usr/bin/env python3
"""The home-screen apps (BUILD pwa): the Commons manifest + icons, and civic's Social / Civic manifests (same pattern as Mail).
    python3 pwa_check.py <repo root>
Everything is checked from the files: the Commons ones as shipped, and civic's from the C++ source (its manifests are string
literals in liquidgov-platform/src/http_server.cpp, extracted and parsed as JSON, the same way for Mail as a control so a broken
extractor cannot pass by finding nothing). Prints 'pwa_check: N checks, F failed'; exit 1 if any failed.
"""
import fnmatch, json, os, re, struct, subprocess, sys, tempfile, zlib

ROOT = sys.argv[1]
checks = failed = 0
def check(ok, what):
    global checks, failed
    checks += 1
    if not ok:
        failed += 1; print("  FAIL " + what)
def rd(p): return open(os.path.join(ROOT, p), encoding="utf-8").read()

def png_info(path):   # (width, height, color_type) from the IHDR, or None if it is not a PNG
    b = open(path, "rb").read()
    if b[:8] != b"\x89PNG\r\n\x1a\n" or b[12:16] != b"IHDR": return None
    w, h, depth, ctype = struct.unpack(">IIBB", b[16:26])
    return (w, h, ctype)

def png_pixels(path):  # RGB rows, for the tiny sanity checks (our own generator's output: color type 2, 8-bit, filter 0)
    b = open(path, "rb").read(); w, h = struct.unpack(">II", b[16:24]); i = 8; idat = b""
    while i < len(b):
        n = struct.unpack(">I", b[i:i + 4])[0]; t = b[i + 4:i + 8]
        if t == b"IDAT": idat += b[i + 8:i + 8 + n]
        i += 12 + n
    raw = zlib.decompress(idat); stride = 1 + 3 * w
    return [[tuple(raw[y * stride + 1 + 3 * x:y * stride + 4 + 3 * x]) for x in range(w)] for y in range(h)]

# ================================================================= The Commons
CM = os.path.join(ROOT, "commons")
html = rd("commons/index.html")
man = json.loads(rd("commons/manifest.webmanifest"))
check(man.get("name") == "The Commons" and man.get("short_name") == "Commons", "the Commons manifest is named 'The Commons' / 'Commons'")
check(man.get("start_url") == "/commons/" and man.get("scope") == "/commons/", "start_url and scope are /commons/ (installing from the plaza opens the plaza)")
check(man.get("display") == "standalone", "display is standalone")
theme = (re.search(r'<meta name="theme-color" content="(#[0-9a-fA-F]{6})"', html) or [None, None])[1]
check(theme is not None and man.get("theme_color") == theme, "the manifest theme_color is the page's own theme-color (%s)" % theme)
check(re.fullmatch(r"#[0-9a-fA-F]{6}", man.get("background_color", "")) is not None, "background_color is a hex colour")
icons = man.get("icons", [])
def has(size, purpose): return any(i.get("sizes") == "%dx%d" % (size, size) and i.get("purpose") == purpose and i.get("type") == "image/png" for i in icons)
check(has(192, "any") and has(512, "any"), "icons: 192 and 512 'any'")
check(has(512, "maskable"), "icons: a 512 'maskable' (Android adaptive icons)")
for i in icons:
    p = os.path.join(CM, i["src"]); want = int(i["sizes"].split("x")[0])
    info = png_info(p) if os.path.exists(p) else None
    check(info is not None and info[0] == want and info[1] == want, "icon %s exists, is a real PNG of the declared %s (got %s)" % (i["src"], i["sizes"], info))
    check(info is not None and info[2] == 2, "icon %s is opaque RGB (no alpha: a phone rounds it, transparency shows black)" % i["src"])
tag = re.findall(r'<link rel="manifest" href="([^"]+)">', html)
check(tag == ["/commons/manifest.webmanifest"], "index.html links the manifest exactly once, by its absolute path: %s" % tag)
at = re.findall(r'<link rel="apple-touch-icon" href="([^"]+)">', html)
check(len(at) == 1 and at[0].startswith("/commons/") and png_info(os.path.join(CM, at[0][len("/commons/"):])) == (180, 180, 2), "index.html has one apple-touch-icon and it is a 180x180 opaque PNG in commons/: %s" % at)
check('<meta name="apple-mobile-web-app-capable" content="yes">' in html and '<meta name="mobile-web-app-capable" content="yes">' in html
      and '<meta name="apple-mobile-web-app-title" content="The Commons">' in html, "the standalone meta tags are there, titled 'The Commons'")
# the icons are exactly what the generator makes (no hand-edited or stale PNG), and the generator is deterministic
with tempfile.TemporaryDirectory() as td:
    subprocess.run([sys.executable, os.path.join(ROOT, "Persistence/TheCommons/art/make_icons.py"), td], check=True, stdout=subprocess.DEVNULL)
    for n in ("apple-touch-icon.png", "icon-192.png", "icon-512.png"):
        check(open(os.path.join(td, n), "rb").read() == open(os.path.join(CM, n), "rb").read(), "commons/%s is byte-identical to what make_icons.py generates" % n)
px = png_pixels(os.path.join(CM, "icon-192.png"))
check(len({c for row in px for c in row}) >= 8, "the icon is a real picture (many colours), not a flat fill")
check((0xf7, 0xf1, 0xe4) in {c for row in px for c in row}, "the notice-board paper colour is in it")
# nothing near the edge but grass and path (the central ~66% is where the subject sits, so the icon survives a maskable crop)
edge = {px[y][x] for y in range(len(px)) for x in range(len(px)) if x < 20 or y < 20 or x >= len(px) - 20 or y >= len(px) - 20}
check((0xf7, 0xf1, 0xe4) not in edge and (0x4a, 0x3a, 0x28) not in edge, "the notice-board and post stay out of the outer margin (maskable-safe)")
# it ships: the client sync must not exclude these files
lib = rd("commons-daemon/deploy/lib.sh")
arr = re.search(r"^CLIENT_EXCLUDES=\((.*?)\)", lib, re.S | re.M)
check(arr is not None, "lib.sh's CLIENT_EXCLUDES array (what the client sync leaves out) was found")
ex = re.findall(r"--exclude[= ]'?([^' \n)]+)'?", arr.group(1)) if arr else []
check(len(ex) >= 1, "and it lists its excludes (%s)" % ex)
for n in ("manifest.webmanifest", "apple-touch-icon.png", "icon-192.png", "icon-512.png"):
    check(not any(fnmatch.fnmatch(n, e.strip("/")) for e in ex), "the client sync does not exclude %s (client excludes: %s)" % (n, ex))
apps = rd("liquidgov-platform/assets/apps.html")
m = re.search(r'<img class="mark" src="(/commons/[^"]+)"[^>]*>\s*<h2[^>]*>The Commons</h2>', apps)
check(m is not None and png_info(os.path.join(CM, m.group(1)[len("/commons/"):])) is not None, "the /apps Commons tile shows the Commons icon (not the Genesis one): %s" % (m and m.group(1)))

# ================================================================= civic: Social / Civic (and Mail, as the control)
src = rd("liquidgov-platform/src/http_server.cpp")
def manifest_of(name):
    i = src.find('path=="/manifest-%s.webmanifest"' % name)
    if i < 0: return None
    j = src.find("}\n", i)              # the route's closing brace
    block = src[i:j]
    lits = re.findall(r'"((?:[^"\\]|\\.)*)"', block.split("http_raw(", 1)[1])
    lits = lits[2:]                     # drop the status ("200 OK") and content-type literals: what is left is the JSON
    text = "".join(bytes(l, "utf-8").decode("unicode_escape") for l in lits)
    try: return json.loads(text)
    except Exception as e: return "BAD JSON: %s | %s" % (e, text[:80])
def theme_for(title): return (re.search(r'title=="%s"\?"(#[0-9a-fA-F]{6})"' % title, src) or [None, None])[1]
def icon_route_ok(path): return ('path=="%s"' % path) in src
shell = {"social": ("app-social.html", "Social", "/icon-social"), "civic": ("app-civic.html", "Civic", "/icon-civic"), "mail": (None, "Messages", "/icon-mail")}
picker = re.search(r'<link rel=\\"manifest\\" href=\\""\+\((.*?)\)\+"\\">', src)
check(picker is not None, "page_wrap's manifest picker was found in the source")
for name, (shellfile, title, iconbase) in shell.items():
    m = manifest_of(name)
    check(isinstance(m, dict), "the %s manifest route exists and its JSON parses: %s" % (name, m if not isinstance(m, dict) else "ok"))
    if not isinstance(m, dict): continue
    check(m.get("name") == title and m.get("short_name") == title, "%s: name/short_name are '%s'" % (name, title))
    check(m.get("display") == "standalone", "%s: display standalone" % name)
    check(re.fullmatch(r"#[0-9a-fA-F]{6}", m.get("background_color", "")) is not None, "%s: background_color is hex" % name)
    check(m.get("theme_color") == theme_for(title) and theme_for(title) is not None, "%s: theme_color %s is the colour page_wrap already uses for '%s' (%s)" % (name, m.get("theme_color"), title, theme_for(title)))
    if shellfile:   # social/civic: start_url is exactly where the install shell sends a standalone launch
        target = re.search(r"location\.replace\('([^']+)'\)", rd("liquidgov-platform/assets/" + shellfile)).group(1)
        check(m.get("start_url") == target, "%s: start_url %s is where %s redirects a standalone launch (%s)" % (name, m.get("start_url"), shellfile, target))
    else:
        check(m.get("start_url") == "/mail", "mail (control): start_url /mail")
    srcs = {i["sizes"]: i["src"] for i in m.get("icons", [])}
    check(set(srcs) == {"192x192", "512x512"}, "%s: icons are 192 and 512" % name)
    for sz, s in srcs.items():
        n = int(sz.split("x")[0]); f = os.path.join(ROOT, "liquidgov-platform/assets", s.lstrip("/"))
        info = png_info(f) if os.path.exists(f) else None
        check(info is not None and info[0] == n and info[1] == n, "%s: %s exists as a %s PNG (got %s)" % (name, s, sz, info))
        check(icon_route_ok(s), "%s: civic actually serves %s (it is in the icon route's allow-list)" % (name, s))
    if picker:
        want = "/manifest-%s.webmanifest" % ("mail" if name == "mail" else name)
        check(('title=="%s"?"%s"' % (title, want)) in picker.group(1), "page_wrap links %s for a page titled '%s'" % (want, title))
check(picker is not None and picker.group(1).rstrip().endswith('"/manifest.webmanifest"'), "every other page still gets the Genesis manifest (the default)")
check(len(re.findall(r'title=="(?:Messages|Social|Civic)"', picker.group(1))) == 3 if picker else False, "the picker has exactly the three app branches")
# the titles the shells render with are the ones the picker keys on
for path, title in (("/app/social", "Social"), ("/app/civic", "Civic"), ("/app/mail", "Messages")):
    check(re.search(r'path=="%s"\)\s*\{[^}]*pg\("%s"' % (re.escape(path), title), src, re.S) is not None, "%s renders with the title '%s' the picker keys on" % (path, title))

print("pwa_check: %d checks, %d failed" % (checks, failed))
sys.exit(1 if failed else 0)
