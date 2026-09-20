#!/usr/bin/env python3
"""Make www.liquidgov.us a redirect to the apex (BUILD www apex), as a pure, idempotent edit of the live nginx site config.

    nginx-www-apex.py FILE            print the edited config to stdout (FILE untouched)
    nginx-www-apex.py --write FILE    edit FILE in place
    nginx-www-apex.py --check FILE    exit 0 if the redirect is already there, 1 if not (no output)

Why: `www.liquidgov.us` resolves to the same droplet and the certificate covers it, but nginx served the ENTIRE site on it as a
second origin (separate cookies and localStorage, duplicate URLs, http://www went to https://www). One canonical host is the fix.

What it does, and only this (every certbot-managed line and every `location` is left byte-for-byte alone; `server_name` keeps
both names so the certificate match and certbot's own handling are unchanged):

  1. in the 443 server block, right after its `server_name liquidgov.us www.liquidgov.us;` line:
         if ($host = www.liquidgov.us) { return 301 https://liquidgov.us$request_uri; }
     (`return` inside `if` at server level is the one safe use of `if`; it runs in the rewrite phase, before any location, so every
     path on www, /internal/ included, is redirected; $request_uri keeps the path and the query string; $host is lower-cased by nginx)
  2. in the port-80 block, certbot's `if ($host = www.liquidgov.us) { return 301 https://$host$request_uri; }` becomes a redirect
     straight to https://liquidgov.us$request_uri (one hop instead of www -> https://www -> apex).

It REFUSES (exit 2) rather than guess if the file is not the shape it knows: it needs exactly one 443 server block naming both hosts
and exactly one certbot www redirect in a port-80 block, or the finished result of both edits. Idempotent: running it twice is the same as once.
"""
import re, sys

HOST_LINE = "server_name liquidgov.us www.liquidgov.us;"
NEW_443 = """
    # Canonical host (BUILD www apex): www is not a second copy of the site. One origin means one set of cookies, one set of URLs.
    # `return` inside `if` at server level runs before any location, so every path on www is redirected; $request_uri keeps path + query.
    if ($host = www.liquidgov.us) {
        return 301 https://liquidgov.us$request_uri;
    }
"""
OLD_80 = """    if ($host = www.liquidgov.us) {
        return 301 https://$host$request_uri;
    } # managed by Certbot
"""
NEW_80 = """    # BUILD www apex: straight to the canonical apex host (one hop, not https://www first).
    if ($host = www.liquidgov.us) {
        return 301 https://liquidgov.us$request_uri;
    }
"""
APPLIED_443 = "return 301 https://liquidgov.us$request_uri;"

class Refuse(Exception): pass

def servers(text):
    """(start, end) of each top-level `server { ... }` block, by brace matching."""
    out, i = [], 0
    while True:
        m = re.compile(r"^server\s*\{", re.M).search(text, i)
        if not m: return out
        depth, j = 0, m.start()
        while j < len(text):
            c = text[j]
            if c == "{": depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0: break
            j += 1
        else:
            raise Refuse("an unclosed server block")
        out.append((m.start(), j + 1)); i = j + 1

def applied(text):
    return text.count(APPLIED_443) == 2 and "if ($host = www.liquidgov.us)" in text and OLD_80 not in text

def transform(text):
    if applied(text): return text
    blocks = servers(text)
    b443 = [b for b in blocks if re.search(r"listen\s+(\[::\]:)?443\b", text[b[0]:b[1]]) and HOST_LINE in text[b[0]:b[1]]]
    b80 = [b for b in blocks if re.search(r"listen\s+(\[::\]:)?80\b", text[b[0]:b[1]]) and OLD_80 in text[b[0]:b[1]]]
    if len(b443) != 1: raise Refuse("expected exactly one 443 server block naming both hosts, found %d" % len(b443))
    if len(b80) != 1: raise Refuse("expected exactly one port-80 block with certbot's www redirect, found %d" % len(b80))
    if text.count(OLD_80) != 1: raise Refuse("certbot's www redirect text appears %d times" % text.count(OLD_80))
    if APPLIED_443 in text: raise Refuse("half-applied: one redirect is already there and the other is not; fix by hand")
    # edit the later block first so the earlier offsets stay valid
    (s80, e80), (s443, e443) = b80[0], b443[0]
    if s80 < s443: raise Refuse("the port-80 block comes before the 443 block: not the known shape")
    blk80 = text[s80:e80].replace(OLD_80, NEW_80, 1)
    # the comment and the `if` go on their own lines directly under server_name
    blk443 = text[s443:e443].replace(HOST_LINE, HOST_LINE + "\n" + NEW_443.strip("\n"), 1)
    out = text[:s443] + blk443 + text[e443:s80] + blk80 + text[e80:]
    if not applied(out): raise Refuse("internal error: the result does not look applied")
    return out

def main(argv):
    mode, args = "print", argv[1:]
    if args and args[0] in ("--write", "--check"): mode, args = args[0][2:], args[1:]
    if len(args) != 1: print(__doc__); return 2
    text = open(args[0], encoding="utf-8").read()
    try:
        if mode == "check": return 0 if applied(text) else 1
        out = transform(text)
    except Refuse as e:
        print("REFUSED: " + str(e), file=sys.stderr); return 2
    if mode == "write":
        if out != text: open(args[0], "w", encoding="utf-8").write(out)
        print("already applied" if out == text else "written")
    else:
        sys.stdout.write(out)
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv))
