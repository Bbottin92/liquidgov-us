"""American spelling in what the site shows (ctx tried-and-failed: "centre" spread to 32 places; "checked
automatically, not by memory"). Run 4's canvas toolbar added aria-label "Size, opacity and colour", which a screen
reader reads out; the toolbar already had "Text background colour".
Reads the string literals of src/http_server.cpp and assets/*.js (lines that are only a comment are skipped) and
fails on a British spelling as a WHOLE word (colour, colours, centre, cancelled...), never a substring.
Static: no browser and no server. Lives with the browser checks because it reads the same page sources they cut.
Usage: python3 site_spelling_check.py [<liquidgov-platform dir>]
Prints PASS/FAIL lines and "VERDICT site-spelling: ALL-PASS" or the failures; exit 1 on a FAIL."""
import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
BRITISH = re.compile(r"\b(colours?|coloured|centres?|centred|cancell(?:ed|ing)|behaviours?|favourites?|"
                     r"organis(?:e|ed|es|ing|ation|ations)|recognis(?:e|ed|es|ing)|licences?|neighbours?|honours?)\b",
                     re.IGNORECASE)
LIT = {".cpp": re.compile(r'"((?:[^"\\\n]|\\.)*)"'), ".hpp": re.compile(r'"((?:[^"\\\n]|\\.)*)"'), ".js":re.compile(r"'((?:[^'\\\n]|\\.)*)'|\"((?:[^\"\\\n]|\\.)*)\"")}


def hits(path):
    out = []
    lit = LIT[os.path.splitext(path)[1]]
    with open(path, encoding="utf-8") as f:
        for no, line in enumerate(f, 1):
            s = line.strip()
            if s.startswith(("//", "/*", "*")):
                continue
            for m in lit.finditer(line):
                text = next(g for g in m.groups() if g is not None) if m.groups() else m.group(1)
                for w in BRITISH.finditer(text or ""):
                    out.append("%s:%d %s in %r" % (os.path.basename(path), no, w.group(0), text[:80]))
    return out


def self_test():
    """The matcher both ways (critical rule 5): whole words hit, longer words and American spellings do not."""
    bad = ["Size, opacity and colour", "the centre", "Cancelled.", "Colours"]
    good = ["Size, opacity and color", "center", "canceled", "colorful", "decentred", "Colourful"]
    return all(BRITISH.search(s) for s in bad) and not any(BRITISH.search(s) for s in good)


def main(argv):
    platform = os.path.abspath(argv[0]) if argv else os.path.dirname(os.path.dirname(HERE))
    files = [os.path.join(platform, "src", p) for p in ["http_server.cpp"] + __import__("server_source").pieces(platform)] + sorted(glob.glob(os.path.join(platform, "assets", "*.js")))
    fails = []

    def check(name, ok, detail=""):
        print(("PASS " if ok else "FAIL ") + name + ("" if ok else "  :: " + str(detail)[:900]), flush=True)
        if not ok:
            fails.append(name)

    check("S0 the matcher hits whole British words only", self_test())
    check("S0 the page sources are there", all(os.path.exists(p) for p in files), files)
    for p in files:
        if os.path.exists(p):
            h = hits(p)
            check("S1 no British spelling in %s's strings" % os.path.basename(p), not h, h)
    n = 2 + len(files)
    print("VERDICT site-spelling: %s  (%d checks)" % ("ALL-PASS" if not fails else "FAILS: " + ", ".join(fails), n), flush=True)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
