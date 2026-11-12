"""server_source.py: the site server's full source text, as one string.

liquidgov-platform/src/http_server.cpp is split into feature pieces under src/site/ (Brandon 2026-11-12, "make it
modular"). http_server.cpp keeps its #include lines for those pieces, in order; the compiler builds them as one unit.
Checks that read the server's literals (CSS, markup, routes) call read(platform_dir) to get the whole text back, with
every `#include "site/..."` line replaced by that file's contents, so they see exactly what the compiler sees.

A plain http_server.cpp with no site/ includes (old trees, the fake roots some self-tests build) is returned as is.

  python server_source.py <platform_dir>                 print the expanded source
  python server_source.py <platform_dir> --same-as FILE  exit 0 if the expanded source is byte-identical to FILE
"""
import os
import re
import sys

PIECE = re.compile(r'^#include "(site/[A-Za-z0-9_./-]+\.inc)"\r?\n?$')


def _expand(path, src_dir, seen):
    """The file's text with each `#include "site/..."` line replaced by that piece (recursively)."""
    if path in seen:
        raise ValueError("server_source: %s includes itself" % path)
    seen = seen | {path}
    out = []
    with open(path, encoding="utf-8", newline="") as f:
        for line in f.read().splitlines(keepends=True):
            m = PIECE.match(line)
            if m:
                piece = os.path.join(src_dir, m.group(1).replace("/", os.sep))
                if not os.path.isfile(piece):
                    raise FileNotFoundError("server_source: %s includes %s, which does not exist" % (path, piece))
                out.append(_expand(piece, src_dir, seen))
            else:
                out.append(line)
    return "".join(out)


def read(platform_dir):
    """Expanded source of <platform_dir>/src/http_server.cpp."""
    src_dir = os.path.join(platform_dir, "src")
    return _expand(os.path.join(src_dir, "http_server.cpp"), src_dir, frozenset())


def pieces(platform_dir):
    """The site/ piece paths http_server.cpp includes, in order (relative to src/)."""
    with open(os.path.join(platform_dir, "src", "http_server.cpp"), encoding="utf-8", newline="") as f:
        return [m.group(1) for m in (PIECE.match(l) for l in f.read().splitlines(keepends=True)) if m]


def main(argv):
    if not argv or len(argv) not in (1, 3) or (len(argv) == 3 and argv[1] != "--same-as"):
        print(__doc__)
        return 2
    try:
        text = read(argv[0])
    except (OSError, ValueError) as e:
        print("FAIL %s" % e)
        return 1
    if len(argv) == 1:
        sys.stdout.write(text)
        return 0
    try:
        with open(argv[2], encoding="utf-8", newline="") as f:
            want = f.read()
    except OSError as e:
        print("FAIL cannot read %s: %s" % (argv[2], e))
        return 1
    if text == want:
        print("SAME %d bytes, %d pieces" % (len(text.encode("utf-8")), len(pieces(argv[0]))))
        return 0
    a, b = text.splitlines(), want.splitlines()
    for i, (x, y) in enumerate(zip(a, b), 1):
        if x != y:
            print("DIFFERENT first at line %d:\n  got:  %r\n  want: %r" % (i, x[:120], y[:120]))
            return 1
    print("DIFFERENT lengths: got %d lines, want %d lines" % (len(a), len(b)))
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
