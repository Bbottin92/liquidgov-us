"""Second pass: follows helper functions (depth 2), builds the nested page list, the 'does something' list, and the link audit.
Usage: python inventory2.py <src dir> <out dir>      writes <out dir>/pages.md, does.md, audit.md, resolved.json"""
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import inventory as inv  # noqa: E402

FUNC = re.compile(r'^(?:static\s+)?(?:inline\s+)?(?:std::string|HttpResponse|http_response|void|auto)\s+([a-z][a-z0-9_]*)\s*\([^;{)]*\)\s*(?:const\s*)?\{', re.M)
SKIP_HELPERS = {"wrap", "page_shell"}
# The router has no global gate (router.cpp:85-107 hands every request to each feature in turn); each route block gates
# itself with the router's own primitives: `if(!acc) return need_login();` (or a 401 JSON) for members, and
# `if(!is_operator(user)) return admin_404();` (or a 403) for operators. Read those, not a guess.
# also the combined form `if (!acc || !is_operator(user)) return ...` (system_routes.cpp /logs)
OPERATOR = re.compile(r"!\s*is_operator\s*\(\s*user\s*\)\s*\)\s*\{?\s*return")
MEMBER = re.compile(r"if\s*\(\s*!\s*acc\s*(?:\|\|[^)]*)?\)\s*\{?\s*return|if\s*\(\s*user\.empty\(\)\s*\)\s*\{?\s*return")
RANK = {"public": 0, "members": 1, "operators": 2}


def access(blk):
    if OPERATOR.search(blk):
        return "operators"
    if MEMBER.search(blk):
        return "members"
    return "public"


def main(src, out):
    funcs, routes, file_routes = {}, [], {}
    for root, _d, files in os.walk(src):
        for fn in sorted(files):
            if not fn.endswith(".cpp"):
                continue
            path = os.path.join(root, fn)
            text = open(path, encoding="utf-8", errors="replace").read()
            rel = os.path.relpath(path, src).replace("\\", "/")
            for m in FUNC.finditer(text):
                blk = text[m.start():inv.block_end(text, m.end() - 1)]
                a = inv.analyse(blk)
                a["file"], a["line"] = rel, text.count("\n", 0, m.start()) + 1
                funcs.setdefault(m.group(1), a)
            # static files served from one table (ui/static_files.cpp, cleanup slice 1 / q57, live since 2026-11-17):
            # each {"/path", ...} row is a public GET with no route block of its own
            if rel.endswith("ui/static_files.cpp"):
                for sm in re.finditer(r'^\s*\{\s*"(/[^"]+)"', text, re.M):
                    routes.append({"method": "GET", "url": sm.group(1), "file": rel,
                                   "line": text.count("\n", 0, sm.start()) + 1, "groups": [], "helpers": [],
                                   "redirects": [], "reads": [], "login_gate": False, "html_chars": 0,
                                   "access": "public", "static": True})
            # every `if (...)` whose condition names a path: ALL path literals in it are URLs of this block (grouped conditions
            # like (path=="/install"||path=="/apps") were missed by the first pass)
            for m in re.finditer(r'^[ \t]*(?:else\s+)?if\s*\((?P<cond>[^\n{]*)', text, re.M):
                cond = m.group("cond")
                if "path" not in cond:
                    continue
                urls = re.findall(r'path\s*==\s*"(/[^"]*)"', cond) + \
                    [p + "*" for p in re.findall(r'path\.rfind\("(/[^"]*)"\s*,\s*0\)\s*==\s*0', cond)]
                if not urls:
                    continue
                meth = re.search(r'method\s*==\s*"([A-Z]+)"', cond)
                end = inv.block_end(text, m.end())
                blk = text[m.start():end]
                a = inv.analyse(blk)
                for u in urls:
                    r = {"method": meth.group(1) if meth else "ANY", "url": u, "file": rel,
                         "line": text.count("\n", 0, m.start()) + 1, "_span": (m.start(), end)}
                    r.update(a)
                    r["access"] = access(blk)
                    file_routes.setdefault(rel, []).append((r, text))
                    routes.append(r)  # the same object: the nesting pass below updates what gets written

    # an inner `if (path == "/x")` inside a bigger route block (dispatch inside a shared handler) takes the enclosing
    # block's gate and method: the gate there runs first (map_routes.cpp /problems/*, admin_routes.cpp /admin/styles/*)
    def indent(text, pos):
        """leading whitespace of the line holding pos (route matches start at the line start, before the indent)"""
        ls = text.rfind("\n", 0, pos) + 1
        le = text.find("\n", ls)
        line = text[ls:le if le >= 0 else len(text)]
        return len(line) - len(line.lstrip())

    for rel, pairs in file_routes.items():
        for r, text in pairs:
            s, e = r["_span"]
            # real nesting is also deeper indentation: a block-end miscount (jukebox_routes.cpp:215 seemed to run past
            # :261) must not make a sibling route look nested
            outer = [o for o, _t in pairs if o is not r and o["_span"][0] < s and o["_span"][1] >= e
                     and indent(text, s) > indent(text, o["_span"][0])]
            for o in outer:
                gate_text = text[o["_span"][0]:s]  # only what runs before this inner check
                g = access(gate_text)
                if RANK[g] > RANK[r["access"]]:
                    r["access"] = g
                if r["method"] == "ANY" and o["method"] != "ANY":
                    r["method"] = o["method"]
            r["nested_in"] = min(outer, key=lambda o: o["_span"][1] - o["_span"][0])["line"] if outer else None
    for r in routes:
        r.pop("_span", None)

    def expand(names, depth, seen):
        """groups/redirects/reads from the helpers named, recursively"""
        groups, reds, reads, used = [], [], [], []
        for h in names:
            if h in SKIP_HELPERS or h in seen or h not in funcs:
                continue
            seen.add(h)
            f = funcs[h]
            used.append("%s (%s:%d)" % (h, f["file"], f["line"]))
            for g in f["groups"]:
                groups.append({"label": g["label"] + "  <via %s>" % h, "objects": g["objects"]})
            reds += f["redirects"]
            reads += f["reads"]
            if depth > 1:
                g2, r2, d2, u2 = expand(f["helpers"], depth - 1, seen)
                groups += g2
                reds += r2
                reads += d2
                used += u2
        return groups, reds, reads, used

    for r in routes:
        g, rd, rf, used = expand(r["helpers"], 2, set())
        r["groups"] = r["groups"] + g
        r["redirects"] = sorted(set(r["redirects"] + rd))[:10]
        r["reads"] = sorted(set(r["reads"] + rf))[:25]
        r["resolved_helpers"] = used
        r["unresolved_helpers"] = [h for h in r["helpers"] if h not in funcs and h not in SKIP_HELPERS]
    routes.sort(key=lambda r: (r["url"], r["method"]))

    # link audit: every internal href seen anywhere vs the route table
    exact = {r["url"] for r in routes if not r["url"].endswith("*")}
    prefixes = [r["url"][:-1] for r in routes if r["url"].endswith("*")]

    def known(u):
        u = u.split("?")[0].split("#")[0]
        return u in exact or any(u.startswith(p) for p in prefixes) or u.startswith("/assets/")

    links = {}
    for r in list(routes) + list(funcs.values()):
        for g in r["groups"]:
            for o in g["objects"]:
                m = re.match(r"link (/[^\s]*)", o)
                if m and "+" not in m.group(1) and "\\" not in m.group(1):
                    links.setdefault(m.group(1).split("?")[0].split("#")[0], set()).add(r.get("url") or r["file"])
    dead = sorted(u for u in links if not known(u))
    linked = set(links)
    # referenced = the URL appears as a quoted string anywhere in the C++ or the scripts/styles other than its own route line
    # (forms post to it, fetch() calls it, a redirect names it). Unreferenced = nothing in the code points at it.
    corpus = ""
    for base in (src, os.path.join(os.path.dirname(src), "assets")):
        for root, _d, files in os.walk(base):
            for fn in files:
                if fn.endswith((".cpp", ".hpp", ".js", ".css")):
                    corpus += open(os.path.join(root, fn), encoding="utf-8", errors="replace").read() + "\n"
    def refs(u):
        # a C++ literal reads action=\"/x\" so a backslash can follow the URL (first run missed it: 67 false orphans)
        return len(re.findall(r'["\'`(]' + re.escape(u) + r'(?:["\'`?#/)\\]|$)', corpus))
    orphans = sorted(u for u in exact if refs(u) <= 1 and not u.startswith("/assets/") and u not in ("/", "/robots.txt", "/favicon.ico"))
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "resolved.json"), "w", encoding="utf-8") as f:
        json.dump(routes, f, indent=1)

    by = {}
    for r in routes:
        by.setdefault(r["url"], []).append(r)
    with open(os.path.join(out, "pages.md"), "w", encoding="utf-8", newline="\n") as f:
        for url, rs in by.items():
            f.write("### `%s`\n" % url)
            for r in rs:
                f.write("- **%s** %s - source `%s:%d`%s\n" % (r["method"], "(read the page)" if r["method"] in ("GET", "ANY") else "(a form posts here)",
                        r["file"], r["line"], "" if r["access"] == "public" else " - %s only" % r["access"]))
                if r["redirects"]:
                    f.write("  - sends you on to: %s\n" % "; ".join(r["redirects"]))
                if r["reads"]:
                    f.write("  - reads form fields: %s\n" % ", ".join(r["reads"]))
                for g in r["groups"]:
                    objs = g["objects"]
                    f.write("  - **group** %s%s\n" % (g["label"], "" if objs else " (heading only)"))
                    for o in objs[:30]:
                        f.write("    - %s\n" % o)
                    if len(objs) > 30:
                        f.write("    - ... %d more\n" % (len(objs) - 30))
                if r["unresolved_helpers"]:
                    f.write("  - builds more via (not followed): %s\n" % ", ".join(r["unresolved_helpers"]))
            f.write("\n")

    # what does something: forms, buttons with ids, scripts, redirects
    with open(os.path.join(out, "does.md"), "w", encoding="utf-8", newline="\n") as f:
        rows = []
        for r in routes:
            for g in r["groups"]:
                for o in g["objects"]:
                    if o.startswith(("form ", "button ", "script ")):
                        rows.append((r["url"], r["method"], o))
        seen_rows = set()
        for url, meth, o in rows:
            if (url, o) in seen_rows:
                continue
            seen_rows.add((url, o))
            f.write("- `%s` %s: %s\n" % (url, meth, o))
    with open(os.path.join(out, "audit.md"), "w", encoding="utf-8", newline="\n") as f:
        f.write("routes=%d distinct urls=%d helper functions indexed=%d\n" % (len(routes), len(by), len(funcs)))
        f.write("internal links found in the HTML: %d distinct targets\n" % len(links))
        f.write("DEAD (linked from a page but no route matches): %d\n" % len(dead))
        for u in dead:
            f.write("  - %s  <- %s\n" % (u, ", ".join(sorted(links[u]))[:100]))
        f.write("ORPHAN (a route that no page links to, excluding /, assets, robots, favicon): %d\n" % len(orphans))
        for u in orphans:
            f.write("  - %s\n" % u)
        unres = sorted({h for r in routes for h in r["unresolved_helpers"]})
        f.write("helper names called but not found as a function definition (HTML not followed): %d\n  %s\n" % (len(unres), ", ".join(unres)))
    print("routes=%d urls=%d funcs=%d dead=%d orphans=%d" % (len(routes), len(by), len(funcs), len(dead), len(orphans)))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
