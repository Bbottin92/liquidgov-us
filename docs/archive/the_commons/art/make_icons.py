#!/usr/bin/env python3
"""Generates the Commons home-screen icons: commons/apple-touch-icon.png (180), commons/icon-192.png, commons/icon-512.png.

Deterministic, stdlib only (no Pillow here): re-running it produces byte-identical PNGs. This is the *source* of the
icons; the PNGs are what ships.

    python3 docs/archive/the_commons/art/make_icons.py

The icon is the plaza seen from above, drawn on a 24x24 grid of "pixels" and scaled up with nearest-neighbour (crisp, like
the game): grass, the path crossing in the middle, the notice-post with its paper, a bench and a small figure. Colours are
the game's own (js/world.js COLOR table, plus the player's body/skin from art/make_art.py), so it reads as the same place.
Full-bleed (no transparency, no rounded corners: the phone rounds it), and everything that matters sits inside the central
~66% so the same file is safe as a "maskable" icon.
"""
import os, struct, sys, zlib

HERE = os.path.dirname(os.path.abspath(__file__))
# default: the repo's commons/ ; an optional argument redirects the output (commons-daemon/tests/pwa_files_check.py regenerates into a temp dir to
# prove the committed PNGs are exactly what this script makes)
OUT = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.abspath(os.path.join(HERE, "..", "..", "..", "commons"))
N = 24   # the icon is an N x N grid of pixels

def hexrgb(h):
    h = h.lstrip("#"); return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16))

GRASS_A, GRASS_B = hexrgb("#4a7c59"), hexrgb("#3d6b4f")
PATH = hexrgb("#c4b49a")
POST = hexrgb("#4a3a28")          # the notice-post's own dark ink-brown
PAPER = hexrgb("#f7f1e4")         # the notice pinned to it
INK = hexrgb("#1a1814")
BENCH = hexrgb("#6b4a32")
BODY, SKIN = hexrgb("#3a5a7a"), hexrgb("#c9a97a")   # the player figure
BRASS = hexrgb("#c4a574")

def shade(c, f):
    t = 0 if f < 0 else 255; a = abs(f)
    return tuple(int(round(c[i] + (t - c[i]) * a)) for i in range(3))

def scene():
    g = [[GRASS_A if ((x // 2) + (y // 2)) % 2 == 0 else GRASS_B for x in range(N)] for y in range(N)]
    def rect(x, y, w, h, c):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                g[yy][xx] = c
    # the path crossing: two bands through the middle, edged a shade darker so it reads as laid stone
    edge = shade(PATH, -0.18)
    rect(0, 10, N, 4, PATH); rect(10, 0, 4, N, PATH)
    rect(0, 10, N, 1, edge); rect(0, 13, N, 1, edge)
    rect(10, 0, 1, N, edge); rect(13, 0, 1, N, edge)
    rect(10, 10, 4, 4, PATH)                      # the junction is plain again (no edge lines through the middle)
    # the notice-post: a dark post with a paper notice on it (an "invitation", not a fountain), centred on the junction
    rect(11, 9, 2, 8, POST)                       # post
    rect(8, 5, 8, 6, POST)                        # the board's frame
    rect(9, 6, 6, 4, PAPER)                       # the notice
    rect(10, 6, 4, 1, INK)                        # two "lines of text" with paper between them
    rect(10, 8, 3, 1, shade(INK, 0.35))
    rect(10, 17, 4, 1, shade(POST, -0.35))        # its foot on the ground
    # a bench on the grass, lower left
    rect(3, 18, 5, 2, BENCH); rect(3, 18, 5, 1, shade(BENCH, 0.18)); rect(3, 20, 1, 1, shade(BENCH, -0.3)); rect(7, 20, 1, 1, shade(BENCH, -0.3))
    # a walker on the path, lower right (the same colours as the player sprite)
    rect(16, 15, 2, 1, INK)                       # hair
    rect(16, 16, 2, 1, SKIN)                      # face
    rect(16, 17, 2, 3, BODY)                      # coat
    rect(16, 20, 1, 1, INK); rect(17, 20, 1, 1, INK)   # feet
    # a brass step at the top of the path, a nod to the Hall's door colour
    rect(11, 1, 2, 1, BRASS)
    return g

def render(size):
    g = scene(); s = size / N
    rows = []
    for y in range(size):
        gy = min(N - 1, int(y / s))
        rows.append(b"\x00" + b"".join(bytes(g[gy][min(N - 1, int(x / s))]) for x in range(size)))
    raw = b"".join(rows)
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))

def main():
    for name, size in (("apple-touch-icon.png", 180), ("icon-192.png", 192), ("icon-512.png", 512)):
        data = render(size)
        open(os.path.join(OUT, name), "wb").write(data)
        print("wrote commons/%s  %dx%d  %d bytes" % (name, size, size, len(data)))

if __name__ == "__main__":
    main()
