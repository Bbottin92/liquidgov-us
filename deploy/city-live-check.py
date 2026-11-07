#!/usr/bin/env python3
"""Signed-out checks for the city deploy. Creates no accounts.

Run with Python on either Windows or WSL. It does not shell out to curl, so a
WSL path and a Windows curl.exe never have to agree.
"""
import sys
import urllib.error
import urllib.request

BASE = "https://liquidgov.us"


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


OPENER = urllib.request.build_opener(NoRedirect)
FAILS = []


def fetch(path, dest=None, method="GET"):
    data = b"" if method == "POST" else None
    req = urllib.request.Request(BASE + path, data=data, method=method)
    if dest:
        req.add_header("Sec-Fetch-Dest", dest)
    try:
        resp = OPENER.open(req, timeout=25)
        return resp.status, resp.headers, resp.read()
    except urllib.error.HTTPError as err:
        return err.code, err.headers, err.read()


def header(headers, name):
    return headers.get(name) or ""


def framed(headers):
    opts = header(headers, "X-Frame-Options")
    csp = header(headers, "Content-Security-Policy")
    return opts == "SAMEORIGIN" and "frame-ancestors 'self'" in csp


def check(name, ok, detail):
    print(("OK  " if ok else "FAIL ") + name + " " + detail)
    if not ok:
        FAILS.append(name)


def location_is(headers, path):
    loc = header(headers, "Location")
    return loc == path or loc.endswith(path)


def main():
    status, headers, _body = fetch("/map")
    check("map", status == 303 and location_is(headers, "/login?next=%2Fmap") and not framed(headers),
          "status=%s loc=%s" % (status, header(headers, "Location")))

    status, headers, _body = fetch("/map", dest="iframe")
    check("map-frame", status == 303 and framed(headers),
          "status=%s frame=%s" % (status, header(headers, "X-Frame-Options")))

    status, headers, _body = fetch("/problems")
    check("problems", status == 303 and "/login" in header(headers, "Location"),
          "status=%s loc=%s" % (status, header(headers, "Location")))

    status, headers, body = fetch("/pain/1")
    check("pain", status == 303 and "/login" in header(headers, "Location") and b"Flowed by" not in body,
          "status=%s loc=%s" % (status, header(headers, "Location")))

    status, headers, _body = fetch("/city/portland")
    check("city", status == 303 and "/login" in header(headers, "Location"),
          "status=%s loc=%s" % (status, header(headers, "Location")))

    status, _headers, body = fetch("/assets/lgmap.js")
    check("lgmap.js", status == 200 and len(body) > 1000, "status=%s bytes=%s" % (status, len(body)))

    status, _headers, body = fetch("/assets/lgmap.css")
    check("lgmap.css", status == 200 and len(body) > 100, "status=%s bytes=%s" % (status, len(body)))

    status, _headers, body = fetch("/install")
    text = body.decode("utf-8", "replace")
    check("install", status == 200 and text.count("<h2") == 1 and "One LiquidGov icon" in text
          and "<h2>LiquidGov</h2>" in text, "status=%s h2=%s" % (status, text.count("<h2")))

    for name, path, dest in (
        ("app-social", "/app/social", "/feed"),
        ("app-mail", "/app/mail", "/mail"),
        ("app-civic", "/app/civic", "/proposals"),
    ):
        status, headers, _body = fetch(path)
        check(name, status == 303 and location_is(headers, dest),
              "status=%s loc=%s" % (status, header(headers, "Location")))

    status, _headers, body = fetch("/manifest.webmanifest")
    check("manifest", status == 200 and b"LiquidGov" in body, "status=%s bytes=%s" % (status, len(body)))

    status, _headers, body = fetch("/manifest-social.webmanifest")
    check("manifest-social", status == 200 and len(body) > 50, "status=%s bytes=%s" % (status, len(body)))

    status, headers, body = fetch("/")
    text = body.decode("utf-8", "replace")
    check("home", status == 200 and not framed(headers) and "Walk in" not in text and 'href="/commons' not in text,
          "status=%s" % status)

    status, headers, _body = fetch("/", dest="iframe")
    check("home-frame", status == 200 and framed(headers),
          "status=%s frame=%s" % (status, header(headers, "X-Frame-Options")))

    status, _headers, body = fetch("/commons/")
    text = body.decode("utf-8", "replace")
    check("commons", status == 200 and "The Commons is closed" in text, "status=%s" % status)

    status, _headers, body = fetch("/commons/presence/guest", method="POST")
    text = body.decode("utf-8", "replace")
    check("commons-post", status == 200 and "The Commons is closed" in text, "status=%s" % status)

    print("FAILS", len(FAILS))
    return 1 if FAILS else 0


if __name__ == "__main__":
    sys.exit(main())
