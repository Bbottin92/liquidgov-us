"""server_source.py: the site server's full source text, as one string.

The site server was one file, liquidgov-platform/src/http_server.cpp. It is now split into modules (Brandon
2026-11-12, "make it modular"): src/http_server.cpp is main(), and src/site/sources.cmake lists the module .cpp files
(each with its .hpp beside it), in the original order. CMakeLists.txt builds from that same list. Checks that read the
server's literals (CSS, markup, routes) call read(platform_dir) to get every module's text: site/common.hpp, then each
module's .hpp and .cpp in sources.cmake order, then http_server.cpp.

Older layouts still read correctly: stage A (http_server.cpp #includes "site/....inc" pieces, expanded in place) and
the single http_server.cpp of old trees and of the fake roots some self-tests build (returned as is).

  python server_source.py <platform_dir>                 print the full source
  python server_source.py <platform_dir> --same-as FILE  exit 0 if the source is byte-identical to FILE (stage A)
  python server_source.py <platform_dir> --files         list the files read, in order
"""
import os
import re
import sys

PIECE = re.compile(r'^#include "(site/[A-Za-z0-9_./-]+\.inc)"\r?\n?$')
SOURCE = re.compile(r"^\s*src/(site/[A-Za-z0-9_./-]+\.cpp)\s*$", re.M)
# a split read must contain these, or it silently missed modules (a check reading half the server passes on nothing)
MUST_HAVE = ("std::string handle(", "int main(", "std::string css()")


def _read(path):
    with open(path, encoding="utf-8", newline="") as f:
        return f.read()


def _expand(path, src_dir, seen):
    """Stage A: the file's text with each `#include "site/....inc"` line replaced by that piece (recursively)."""
    if path in seen:
        raise ValueError("server_source: %s includes itself" % path)
    seen = seen | {path}
    out = []
    for line in _read(path).splitlines(keepends=True):
        m = PIECE.match(line)
        if m:
            piece = os.path.join(src_dir, m.group(1).replace("/", os.sep))
            if not os.path.isfile(piece):
                raise FileNotFoundError("server_source: %s includes %s, which does not exist" % (path, piece))
            out.append(_expand(piece, src_dir, seen))
        else:
            out.append(line)
    return "".join(out)


def pieces(platform_dir):
    """The module files read besides http_server.cpp, relative to src/, in order ([] for a one-file server)."""
    src_dir = os.path.join(platform_dir, "src")
    listing = os.path.join(src_dir, "site", "sources.cmake")
    if os.path.isfile(listing):
        cpps = SOURCE.findall(_read(listing))
        if not cpps:
            raise ValueError("server_source: %s lists no src/site/*.cpp files" % listing)
        out = ["site/common.hpp"] if os.path.isfile(os.path.join(src_dir, "site", "common.hpp")) else []
        for c in cpps:
            h = c[:-4] + ".hpp"
            if os.path.isfile(os.path.join(src_dir, h.replace("/", os.sep))):
                out.append(h)
            if not os.path.isfile(os.path.join(src_dir, c.replace("/", os.sep))):
                raise FileNotFoundError("server_source: sources.cmake lists %s, which does not exist" % c)
            out.append(c)
        return out
    return [m.group(1) for m in (PIECE.match(l) for l in _read(os.path.join(src_dir, "http_server.cpp"))
                                 .splitlines(keepends=True)) if m]


def read(platform_dir):
    """Full source of the site server under <platform_dir>/src."""
    src_dir = os.path.join(platform_dir, "src")
    if os.path.isfile(os.path.join(src_dir, "site", "sources.cmake")):
        text = "".join(_read(os.path.join(src_dir, p.replace("/", os.sep))) for p in pieces(platform_dir))
        text += _read(os.path.join(src_dir, "http_server.cpp"))
        missing = [s for s in MUST_HAVE if s not in text]
        if missing:
            raise ValueError("server_source: the split source has no %s; a module is missing from sources.cmake"
                             % ", ".join(missing))
        return text
    return _expand(os.path.join(src_dir, "http_server.cpp"), src_dir, frozenset())


def main(argv):
    if not argv or len(argv) not in (1, 2, 3) or (len(argv) == 3 and argv[1] != "--same-as") or (
            len(argv) == 2 and argv[1] != "--files"):
        print(__doc__)
        return 2
    try:
        if len(argv) == 2:
            print("\n".join(pieces(argv[0]) + ["http_server.cpp"]))
            return 0
        text = read(argv[0])
    except (OSError, ValueError) as e:
        print("FAIL %s" % e)
        return 1
    if len(argv) == 1:
        sys.stdout.write(text)
        return 0
    try:
        want = _read(argv[2])
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
