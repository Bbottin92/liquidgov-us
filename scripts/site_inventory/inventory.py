"""Generates a raw site inventory from the C++ route blocks (read-only; writes only the output file).
Usage: python inventory.py <src dir> <out.md> [<out.json>]
For each route block `if (method=="GET" && path=="/x")` (also prefix routes, either order, or no method) it records: method, URL,
source file:line, the string literals of the block (the HTML it emits), then:
  groups  = headings (h1/h2/h3/summary/legend/fieldset) in order; each group's objects are the tags between headings
  objects = forms (action, method), inputs/selects/textareas (name, type), buttons (id, text), links (href, text), images,
            scripts (src), iframes, svg, video/audio, plus helper calls that build more HTML elsewhere (listed by name)
  does    = redirects out (http_redir), login gate (heuristic), POST handlers' field reads (form_value/param names)
Limits (printed in the output header): HTML built inside helper functions is not followed here (the helper is named); a block
is cut at its matching brace counting braces outside string literals."""
import json
import os
import re
import sys

ROUTE = [
    re.compile(r'if\s*\(\s*method\s*==\s*"(?P<m>[A-Z]+)"\s*&&\s*(?:path\s*==\s*"(?P<p>[^"]+)"|path\.rfind\("(?P<pre>[^"]+)"\s*,\s*0\)\s*==\s*0)'),
    re.compile(r'if\s*\(\s*(?:path\s*==\s*"(?P<p>[^"]+)"|path\.rfind\("(?P<pre>[^"]+)"\s*,\s*0\)\s*==\s*0)\s*&&\s*method\s*==\s*"(?P<m>[A-Z]+)"'),
    re.compile(r'if\s*\(\s*(?:path\s*==\s*"(?P<p>[^"]+)"|path\.rfind\("(?P<pre>[^"]+)"\s*,\s*0\)\s*==\s*0)\s*\)'),
]
LIT = re.compile(r'"((?:[^"\\]|\\.)*)"')
TAG = re.compile(r'<(h1|h2|h3|summary|legend|form|input|select|textarea|button|a|img|iframe|video|audio|script|svg|canvas|table|nav)\b([^>]*)>', re.I)
ATTR = lambda a, n: (re.search(r'\b%s\s*=\s*\\?"([^"\\]*)' % n, a) or [None, ""])[1]
HELP = re.compile(r'\b([a-z][a-z0-9_]*_(?:html|body|page|section|card|list|panel|form|banner|bar|snippet)|wrap|page_shell)\s*\(')
REDIR = re.compile(r'http_redir\(\s*([^;]*?)\)\s*;')
FORMV = re.compile(r'(?:form_value|form_get|param|query_value|get_param)\s*\(\s*[a-z_]*\s*,?\s*"([a-z_0-9.-]+)"')


def unesc(s):
    return s.replace('\\"', '"').replace("\\n", " ").replace("\\\\", "\\")


def block_end(text, i):
    """index just past the block that starts at the first '{' after i, or the end of the single statement"""
    j = text.find("{", i)
    k = text.find(";", i)
    if j < 0 or (0 <= k < j):
        return (k + 1) if k >= 0 else len(text)
    depth, n, instr, esc = 0, len(text), False, False
    for p in range(j, n):
        c = text[p]
        if instr:
            if esc:
                esc = False
            elif c == "\\":
                esc = True
            elif c == '"':
                instr = False
            continue
        if c == '"':
            instr = True
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return p + 1
    return n


def analyse(blk):
    html = unesc("".join(LIT.findall(blk)))
    groups, cur = [], {"label": "(top of page)", "objects": []}
    for m in TAG.finditer(html):
        tag, attrs = m.group(1).lower(), m.group(2)
        tail = re.sub(r"<[^>]+>", " ", html[m.end():m.end() + 160]).strip()
        tail = re.sub(r"\s+", " ", tail)[:60]
        if tag in ("h1", "h2", "h3", "summary", "legend"):
            if cur["objects"] or cur["label"] != "(top of page)":
                groups.append(cur)
            cur = {"label": "%s: %s" % (tag, tail), "objects": []}
            continue
        if tag == "form":
            o = "form action=%s method=%s" % (ATTR(attrs, "action") or "(same page)", (ATTR(attrs, "method") or "get").lower())
        elif tag in ("input", "select", "textarea"):
            o = "%s name=%s type=%s" % (tag, ATTR(attrs, "name") or "-", ATTR(attrs, "type") or ("text" if tag == "input" else tag))
        elif tag == "button":
            o = "button id=%s '%s'%s" % (ATTR(attrs, "id") or "-", tail, " [submit]" if ATTR(attrs, "type") == "submit" else "")
        elif tag == "a":
            o = "link %s '%s'" % (ATTR(attrs, "href") or "-", tail)
        elif tag == "script":
            o = "script %s" % (ATTR(attrs, "src") or "(inline)")
        elif tag in ("img", "iframe", "video", "audio"):
            o = "%s %s" % (tag, ATTR(attrs, "src") or ATTR(attrs, "id") or "")
        else:
            o = tag + (" id=" + ATTR(attrs, "id") if ATTR(attrs, "id") else "")
        cur["objects"].append(o.strip())
    groups.append(cur)
    return {
        "groups": [g for g in groups if g["objects"] or g["label"] != "(top of page)"],
        "helpers": sorted(set(HELP.findall(blk))),
        "redirects": sorted(set(x.strip()[:70] for x in REDIR.findall(blk)))[:8],
        "reads": sorted(set(FORMV.findall(blk)))[:20],
        "login_gate": bool(re.search(r"user\.empty\(\)|require_login|http_redir\(\s*\"/login", blk)),
        "html_chars": len(html),
    }


def main(src, out_md, out_json):
    routes = []
    for root, _d, files in os.walk(src):
        for fn in sorted(files):
            if not fn.endswith(".cpp"):
                continue
            path = os.path.join(root, fn)
            text = open(path, encoding="utf-8", errors="replace").read()
            seen = set()
            for rx in ROUTE:
                for m in rx.finditer(text):
                    if m.start() in seen:
                        continue
                    seen.add(m.start())
                    d = m.groupdict()
                    url = d.get("p") or (d.get("pre") + "*")
                    blk = text[m.start():block_end(text, m.end())]
                    r = {"method": d.get("m") or "ANY", "url": url, "file": os.path.relpath(path, src).replace("\\", "/"),
                         "line": text.count("\n", 0, m.start()) + 1}
                    r.update(analyse(blk))
                    routes.append(r)
    routes.sort(key=lambda r: (r["url"], r["method"]))
    with open(out_json, "w", encoding="utf-8") as f:
        json.dump(routes, f, indent=1)
    by = {}
    for r in routes:
        by.setdefault(r["url"], []).append(r)
    with open(out_md, "w", encoding="utf-8", newline="\n") as f:
        f.write("# Raw route inventory (generated)\n\n%d route blocks, %d distinct URLs. HTML built in helper functions is NOT followed "
                "(helpers are named). Groups are the headings found in the block.\n\n" % (len(routes), len(by)))
        for url, rs in by.items():
            f.write("## %s\n" % url)
            for r in rs:
                f.write("- %s  (%s:%d)%s html=%d chars\n" % (r["method"], r["file"], r["line"], " [login gate]" if r["login_gate"] else "", r["html_chars"]))
                if r["helpers"]:
                    f.write("  - helpers: %s\n" % ", ".join(r["helpers"]))
                if r["redirects"]:
                    f.write("  - redirects: %s\n" % "; ".join(r["redirects"]))
                if r["reads"]:
                    f.write("  - reads fields: %s\n" % ", ".join(r["reads"]))
                for g in r["groups"]:
                    f.write("  - group %s\n" % g["label"])
                    for o in g["objects"][:25]:
                        f.write("    - %s\n" % o)
            f.write("\n")
    print("routes=%d urls=%d" % (len(routes), len(by)))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else sys.argv[2] + ".json")
