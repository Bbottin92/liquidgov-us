#!/usr/bin/env python3
"""One installable app (Brandon 2026-11-06: "lets get rid of the multiple app icons thing") and the
closed Commons (280bf60), checked from the shipped files, no server.
    python3 one_app_check.py <repo root>
A new file because committed tests stay frozen (#6847b, #1bcfd); pwa_check.py is the original.
Prints 'one_app_check: N checks, F failed'; exit 1 if any failed.
"""
import os, re, sys

ROOT = sys.argv[1]
checks = failed = 0
def check(ok, what):
    global checks, failed
    checks += 1
    if not ok:
        failed += 1; print("  FAIL " + what)
def rd(p): return open(os.path.join(ROOT, p), encoding="utf-8").read()

apps = rd("liquidgov-platform/assets/apps.html")
# The Commons is closed until it is built (Brandon, 280bf60): /apps must not offer a walk-in.
check("The Commons" not in apps and "/commons/" not in apps, "the /apps page does not offer the closed Commons")

# ================================================================= one app (Brandon 2026-11-06:
# "lets get rid of the multiple app icons thing. that was a bad idea")
src = __import__("runpy").run_path(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "liquidgov-platform", "tests", "browser", "server_source.py"))["read"](os.path.join(ROOT, "liquidgov-platform"))  # http_server.cpp + its src/site pieces
check('"<link rel=\\"manifest\\" href=\\"/manifest.webmanifest\\">"' in src, "page_wrap links the one LiquidGov manifest on every page")
check('apple-touch-icon\\" href=\\"/apple-touch-icon.png\\"' in src, "page_wrap links the one LiquidGov icon on every page")
check('apple-mobile-web-app-title\\" content=\\"LiquidGov\\"' in src, "the home-screen name is LiquidGov on every page")
check(re.search(r'title=="(?:Messages|Social|Civic)"\?"/(?:manifest-|icon-)', src) is None, "no page picks a separate Messages/Social/Civic manifest or icon")
for path, dest in (("/app/social", "/feed"), ("/app/civic", "/proposals"), ("/app/mail", "/mail")):
    check(re.search(r'path=="%s"\)\s*return http_redir\("%s"\)' % (re.escape(path), re.escape(dest)), src) is not None,
          "%s sends an old install to %s" % (path, dest))
check(not any(s in apps for s in ("Get Social", "Get Civic", "Get Messages")), "/apps offers one app, not three")
for f in ("app-social.html", "app-civic.html", "app-mail.html"):
    check(not os.path.exists(os.path.join(ROOT, "liquidgov-platform/assets", f)), "the old %s install shell is gone" % f)
# Kept on purpose: icons already on phones, and deploy/nginx-www-apex.sh checks manifest-social.
for name in ("mail", "social", "civic"):
    check(('path=="/manifest-%s.webmanifest"' % name) in src, "the old %s manifest still answers for icons already installed" % name)

print("one_app_check: %d checks, %d failed" % (checks, failed))
sys.exit(1 if failed else 0)
