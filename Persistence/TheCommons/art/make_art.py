#!/usr/bin/env python3
"""Generates commons/tiles.png and commons/sprites.png (BUILD tileset PNG + player sheet).

Deterministic, stdlib only (no Pillow in this environment): re-running it produces
byte-identical PNGs. This is the *source* of the art; the PNGs are what ships.

    python3 Persistence/TheCommons/art/make_art.py

tiles.png   256 x 64   16x16 cells, 16 per row, row-major.  Cell N is tile id N
            (the T_* enum in commons/js/world.js) for N = 0..41.  Cells 48..51 are
            grass variants (A phase 1,2 = 48,49 ; B phase 1,2 = 50,51) so the
            bake can keep breaking up the lattice with a position hash.
sprites.png 128 x 24   8 cells of 16x24 with alpha: (dir*2 + frame), dir order
            DOWN, UP, LEFT, RIGHT (matches spriteSrcX in index.html).

Palette: every colour is the tile's own COLOR from index.html mixed toward black or
white (plus ink/paper from art.md). The script parses those COLOR lines and checks
each tile's mean colour stays near its COLOR, so the sheet cannot drift off-palette
or off-order without failing here.
"""
import os, re, struct, zlib, random, sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
CLIENT = os.path.join(REPO, "commons", "js", "world.js")
T = 16

# ---------------------------------------------------------------- pixel helpers
def hexrgb(h):
    h = h.lstrip("#")
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), 255)

def shade(c, f):
    """Mix toward black (f<0) or white (f>0), like shadeHex() in index.html."""
    t = 0 if f < 0 else 255
    a = abs(f)
    return tuple(int(round(c[i] + (t - c[i]) * a)) for i in range(3)) + (255,)

class Img:
    def __init__(self, w, h, fill=(0, 0, 0, 0)):
        self.w, self.h = w, h
        self.p = [fill] * (w * h)
    def px(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.p[y * self.w + x] = c
    def rect(self, x, y, w, h, c):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.px(xx, yy, c)
    def blit(self, o, ox, oy):
        for y in range(o.h):
            for x in range(o.w):
                self.px(ox + x, oy + y, o.p[y * o.w + x])
    def png(self):
        raw = bytearray()
        for y in range(self.h):
            raw.append(0)
            for x in range(self.w):
                raw += bytes(self.p[y * self.w + x])
        def chunk(t, d):
            c = struct.pack(">I", len(d)) + t + d
            return c + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
        return (b"\x89PNG\r\n\x1a\n"
                + chunk(b"IHDR", struct.pack(">IIBBBBB", self.w, self.h, 8, 6, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
                + chunk(b"IEND", b""))

# ------------------------------------------------------- palette from the client
src = open(CLIENT, encoding="utf-8").read()
ids = {}
for m in re.finditer(r"\b(T_[A-Z_0-9]+)\s*=\s*(\d+)", src):
    ids[m.group(1)] = int(m.group(2))
COLOR = {}
for m in re.finditer(r'COLOR\[(T_[A-Z_0-9]+)\]\s*=\s*"(#[0-9a-fA-F]{6})"', src):
    COLOR[ids[m.group(1)]] = hexrgb(m.group(2))
INK = hexrgb(re.search(r'var INK = "(#[0-9a-fA-F]{6})"', src).group(1))
BODY, SKIN = hexrgb("#3a5a7a"), hexrgb("#c9a97a")   # the existing player colours
NTILES = 42  # BUILD property phase 4: ids 0..41 (T_SAND added)
assert len(COLOR) == NTILES and sorted(COLOR) == list(range(NTILES)), sorted(set(range(NTILES)) - set(COLOR))

def C(name): return COLOR[ids[name]]

# ------------------------------------------------------------------ tile drawing
def fill(c):
    t = Img(T, T, c); return t

def grass(base, phase):
    t = fill(base)
    dark, light = shade(base, -0.10), shade(base, 0.07)
    for gy in range(4):
        for gx in range(4):
            if ((gx + gy + phase) & 1) != 0: continue
            col = light if ((gx * 3 + gy + phase) % 4 == 0) else dark
            t.rect(gx * 4 + (phase & 1), gy * 4 + ((phase >> 1) & 1), 2, 2, col)
    return t

def speckle(base, seed, n, dark=-0.12, light=0.10):
    t = fill(base); r = random.Random(seed)
    for i in range(n):
        x, y = r.randrange(1, 15), r.randrange(1, 15)
        t.px(x, y, shade(base, dark if i % 3 else light))
    return t

def path():
    t = speckle(C("T_PATH"), 11, 9, -0.13, 0.09)
    t.rect(3, 4, 2, 1, shade(C("T_PATH"), -0.20)); t.rect(10, 11, 2, 1, shade(C("T_PATH"), -0.20))
    return t

def water():
    b = C("T_WATER"); t = fill(b)
    hi = shade(b, 0.16)
    t.rect(2, 4, 5, 1, hi); t.rect(9, 5, 3, 1, hi)
    t.rect(6, 10, 5, 1, hi); t.rect(1, 12, 2, 1, hi)
    return t

def dirt():
    return speckle(C("T_DIRT"), 23, 12, -0.15, 0.08)

def planks_v(base, seed):
    """Vertical boards: building wall."""
    t = fill(base); seam = shade(base, -0.28); hi = shade(base, 0.10)
    for x in (0, 5, 10):
        t.rect(x, 0, 1, 16, seam)
    t.rect(0, 0, 16, 1, hi); t.rect(0, 15, 16, 1, shade(base, -0.20))
    r = random.Random(seed)
    for _ in range(5):
        t.px(r.randrange(1, 15), r.randrange(2, 14), shade(base, -0.18))
    return t

def planks_h(base, off=0):
    """Horizontal boards: floors / bridge."""
    t = fill(base); seam = shade(base, -0.24); hi = shade(base, 0.08)
    for y in (3, 7, 11, 15):
        t.rect(0, y, 16, 1, seam)
    # staggered butt joints
    for i, y in enumerate((0, 4, 8, 12)):
        jx = (5 + off + i * 6) % 16
        t.rect(jx, y, 1, 3, seam)
        t.rect(0, y, 16, 1, hi)
    return t

def door(panel, kind):
    frame = shade(C("T_WALL"), -0.30)
    t = fill(frame)
    t.rect(1, 1, 14, 15, shade(panel, -0.25))     # inner reveal
    t.rect(2, 2, 12, 14, panel)
    t.rect(2, 2, 12, 1, shade(panel, 0.14))        # lit top
    if kind == "boarded":
        plank = shade(C("T_WALL"), -0.05)
        for i in range(12):                        # two crossing planks
            t.rect(2 + i, 3 + i, 2, 2, plank)
            t.rect(13 - i, 3 + i, 2, 2, shade(plank, -0.15))
        t.rect(2, 8, 12, 1, shade(panel, -0.35))
    elif kind == "lab":
        t.rect(5, 4, 6, 5, shade(panel, 0.35)); t.rect(5, 4, 6, 1, INK); t.rect(7, 4, 1, 5, INK)
        t.rect(11, 10, 1, 2, C("T_HALL_DOOR"))
    elif kind == "hall":
        t.rect(3, 4, 4, 8, shade(panel, 0.12)); t.rect(9, 4, 4, 8, shade(panel, 0.12))
        t.rect(3, 4, 4, 1, INK); t.rect(9, 4, 4, 1, INK)
        t.rect(7, 8, 1, 2, INK); t.rect(8, 8, 1, 2, INK)
    else:  # plain / house / demo
        t.rect(4, 4, 8, 4, shade(panel, -0.10)); t.rect(4, 9, 8, 5, shade(panel, -0.10))
        t.rect(11, 9, 1, 2, C("T_HALL_DOOR"))
    return t

def mine_door():
    t = fill(C("T_MINE_DOOR"))
    beam = shade(C("T_WALL"), -0.15)
    t.rect(0, 0, 2, 16, beam); t.rect(14, 0, 2, 16, beam); t.rect(0, 0, 16, 2, beam)
    t.rect(2, 2, 12, 1, shade(C("T_MINE_DOOR"), 0.25))
    t.rect(6, 6, 4, 8, shade(C("T_MINE_DOOR"), -0.5))
    return t

def hedge():
    b = C("T_HEDGE"); t = fill(b); r = random.Random(31)
    for _ in range(26):
        t.rect(r.randrange(0, 14), r.randrange(0, 14), 2, 2, shade(b, 0.14 if r.random() < 0.5 else -0.22))
    t.rect(0, 14, 16, 2, shade(b, -0.30))
    return t

def tree():
    t = grass(C("T_GRASS_A"), 0)
    canopy = C("T_TREE")
    t.rect(7, 11, 2, 4, shade(C("T_WALL"), -0.15))          # trunk
    t.rect(6, 15, 4, 1, shade(C("T_GRASS_A"), -0.35))        # shadow
    for y in range(16):
        for x in range(16):
            d = (x - 7.5) ** 2 + (y - 6.5) ** 2
            if d <= 30:
                col = canopy
                if d > 22: col = shade(canopy, -0.32)        # rim
                elif (x + y) < 12 and d < 16: col = shade(canopy, 0.20)   # sun side
                elif (x - y) > 3: col = shade(canopy, -0.14)
                t.px(x, y, col)
    return t

def notice_post():
    t = grass(C("T_GRASS_B"), 0)
    wood = C("T_NOTICE_POST")
    t.rect(7, 8, 2, 8, wood)
    t.rect(2, 2, 12, 8, wood); t.rect(3, 3, 10, 6, hexrgb("#f7f1e4"))
    t.rect(4, 4, 6, 1, INK); t.rect(4, 6, 8, 1, INK); t.rect(4, 8, 4, 1, INK)
    return t

def sign():
    # A full-tile board (no ground drawn) so the same tile reads on grass
    # outdoors and on dirt in the mine.
    wood = C("T_SIGN"); t = fill(wood)
    t.rect(0, 0, 16, 1, shade(wood, 0.30)); t.rect(0, 15, 16, 1, shade(wood, -0.35))
    t.rect(0, 0, 1, 16, shade(wood, 0.12)); t.rect(15, 0, 1, 16, shade(wood, -0.25))
    t.rect(3, 4, 10, 1, shade(wood, 0.60)); t.rect(3, 7, 8, 1, shade(wood, 0.60)); t.rect(3, 10, 6, 1, shade(wood, 0.60))
    return t

def bench():
    b = C("T_BENCH"); t = fill(b)
    t.rect(0, 0, 16, 1, shade(b, 0.14))
    g = shade(b, -0.22)
    t.rect(1, 5, 9, 1, g); t.rect(12, 5, 3, 1, g); t.rect(1, 10, 4, 1, g); t.rect(7, 10, 8, 1, g)
    t.rect(0, 15, 16, 1, g)
    return t

def flowerbed():
    b = C("T_FLOWERBED"); t = fill(shade(b, -0.35)); r = random.Random(5)
    leaf = shade(C("T_GRASS_A"), -0.05)
    for _ in range(9): t.rect(r.randrange(1, 14), r.randrange(1, 14), 2, 1, leaf)
    for i in range(9):
        x, y = r.randrange(1, 14), r.randrange(1, 14)
        t.rect(x, y, 2, 2, b if i % 3 else shade(b, 0.4))
        t.px(x, y, shade(b, 0.55))
    return t

def railing():
    t = grass(C("T_GRASS_A"), 1)
    r = C("T_RAILING")
    t.rect(0, 5, 16, 2, r); t.rect(0, 10, 16, 2, r)
    t.rect(0, 5, 16, 1, shade(r, 0.2)); t.rect(0, 10, 16, 1, shade(r, 0.2))
    for x in (1, 7, 13): t.rect(x, 3, 2, 12, shade(r, -0.2))
    return t

def plateau():
    return speckle(C("T_PLATEAU"), 41, 10, -0.08, 0.10)

def lowwall():
    b = C("T_LOWWALL"); t = fill(b); m = shade(b, -0.28)
    for y in (0, 5, 10):
        t.rect(0, y, 16, 1, m)
    for (x, y) in ((4, 1), (11, 1), (8, 6), (2, 6), (13, 6), (5, 11), (12, 11)):
        t.rect(x, y, 1, 4, m)
    t.rect(0, 15, 16, 1, m)
    return t

def crate():
    b = C("T_CRATE"); t = fill(shade(b, -0.30)); t.rect(1, 1, 14, 14, b)
    d = shade(b, -0.32)
    for i in range(12):
        t.px(2 + i, 2 + i, d); t.px(13 - i, 2 + i, d)
    t.rect(1, 1, 14, 1, shade(b, 0.14))
    return t

def adit():
    b = C("T_ADIT"); t = speckle(b, 7, 14, -0.22, 0.10)
    t.rect(2, 3, 5, 1, shade(b, -0.3)); t.rect(9, 9, 4, 1, shade(b, -0.3)); t.rect(6, 12, 1, 3, shade(b, -0.3))
    return t

# ---- interior
def floor_bg(): return planks_h(C("T_FLOOR"))

def iwall():
    b = C("T_IWALL"); t = speckle(b, 3, 5, -0.07, 0.0)
    t.rect(0, 15, 16, 1, shade(b, -0.12))
    return t

def table():
    t = floor_bg(); w = C("T_TABLE")
    t.rect(1, 3, 14, 9, w); t.rect(1, 3, 14, 1, shade(w, 0.18)); t.rect(1, 11, 14, 1, shade(w, -0.25))
    t.rect(2, 12, 2, 3, shade(w, -0.2)); t.rect(12, 12, 2, 3, shade(w, -0.2))
    return t

def chair():
    t = floor_bg(); c = C("T_CHAIR")
    t.rect(4, 2, 8, 6, shade(c, -0.15)); t.rect(4, 2, 8, 1, shade(c, 0.15))    # back
    t.rect(3, 8, 10, 4, c); t.rect(3, 8, 10, 1, shade(c, 0.15))                # seat
    t.rect(4, 12, 2, 3, shade(c, -0.25)); t.rect(10, 12, 2, 3, shade(c, -0.25))
    return t

def rug():
    b = C("T_RUG"); t = fill(shade(b, -0.22)); t.rect(1, 0, 14, 16, b)
    t.rect(3, 0, 10, 16, shade(b, 0.10))
    for y in range(0, 16, 4): t.rect(5, y + 1, 6, 1, shade(b, -0.15))
    return t

def wallslot():
    b = C("T_WALLSLOT"); t = fill(b)
    fr = shade(C("T_WORKBENCH"), 0.05)
    t.rect(0, 0, 16, 1, fr); t.rect(0, 15, 16, 1, fr); t.rect(0, 0, 1, 16, fr); t.rect(15, 0, 1, 16, fr)
    t.rect(1, 1, 14, 1, shade(b, -0.14)); t.rect(1, 1, 1, 14, shade(b, -0.10))
    return t

def workbench():
    t = floor_bg(); w = C("T_WORKBENCH")
    t.rect(0, 4, 16, 8, w); t.rect(0, 4, 16, 1, shade(w, 0.18)); t.rect(0, 11, 16, 1, shade(w, -0.25))
    t.rect(1, 12, 2, 4, shade(w, -0.22)); t.rect(13, 12, 2, 4, shade(w, -0.22))
    t.rect(5, 6, 6, 2, shade(w, -0.18))
    return t

def station(base, kind):
    """A lookup station: a block in the station's own colour with a lit top,
    a shaded foot, and a small emblem of what it makes (flask / ink page / chalk)."""
    t = fill(base)
    dark = shade(base, -0.22 if sum(base[:3]) > 300 else -0.35)
    lite = shade(base, 0.22 if sum(base[:3]) < 400 else 0.0)
    em = shade(base, 0.45) if sum(base[:3]) < 300 else shade(base, -0.38)
    t.rect(0, 0, 16, 2, lite if lite != base else shade(base, -0.06))
    t.rect(0, 13, 16, 3, dark); t.rect(0, 0, 1, 16, shade(base, -0.10)); t.rect(15, 0, 1, 16, shade(base, -0.10))
    if kind == "flask":
        t.rect(6, 4, 4, 1, em); t.rect(7, 5, 2, 2, em); t.rect(5, 7, 6, 4, em)
    elif kind == "block":
        t.rect(4, 4, 8, 7, em); t.rect(4, 4, 8, 1, shade(em, 0.2)); t.rect(5, 6, 6, 1, base)
    else:
        t.rect(5, 6, 6, 4, em); t.rect(5, 6, 6, 1, shade(em, 0.1)); t.rect(7, 4, 3, 2, em)
    return t

def idoor():
    t = floor_bg(); s = shade(C("T_IDOOR"), -0.28)
    t.rect(0, 0, 16, 2, s); t.rect(0, 14, 16, 2, shade(C("T_IDOOR"), 0.15))
    return t

def lamp():
    b = C("T_LAMP"); t = fill(shade(b, -0.35))
    for y in range(16):
        for x in range(16):
            d = (x - 7.5) ** 2 + (y - 7.5) ** 2
            if d <= 42: t.px(x, y, shade(b, -0.15))
            if d <= 24: t.px(x, y, b)
            if d <= 9: t.px(x, y, shade(b, 0.55))
    return t

# ---- interior-only tiles that are plain colour blocks
def solid_tex(name, seed=None, n=6):
    return speckle(C(name), seed or 1, n, -0.10, 0.08)

DRAW = {
    "T_GRASS_A": lambda: grass(C("T_GRASS_A"), 0),
    "T_GRASS_B": lambda: grass(C("T_GRASS_B"), 0),
    "T_PATH": path, "T_WATER": water, "T_DIRT": dirt,
    "T_WALL": lambda: planks_v(C("T_WALL"), 2),
    "T_DOOR_DEMO": lambda: door(C("T_DOOR_DEMO"), "plain"),
    "T_DOOR_BOARDED": lambda: door(C("T_DOOR_BOARDED"), "boarded"),
    "T_HEDGE": hedge,
    "T_HALL_DOOR": lambda: door(C("T_HALL_DOOR"), "hall"),
    "T_NOTICE_POST": notice_post, "T_BENCH": bench,
    "T_BRIDGE": lambda: planks_h(C("T_BRIDGE"), 3),
    "T_LAB_DOOR": lambda: door(C("T_LAB_DOOR"), "lab"),
    "T_SIGN": sign,
    "T_FLOOR": floor_bg, "T_IWALL": iwall, "T_TABLE": table, "T_CHAIR": chair,
    "T_RUG": rug, "T_WALLSLOT": wallslot, "T_WORKBENCH": workbench,
    "T_STATION_COPPER": lambda: station(C("T_STATION_COPPER"), "flask"),
    "T_STATION_IRON": lambda: station(C("T_STATION_IRON"), "block"),
    "T_STATION_CHALK": lambda: station(C("T_STATION_CHALK"), "chalk"),
    "T_STATION_VINEGAR": lambda: station(C("T_STATION_VINEGAR"), "flask"),
    "T_STATION_TANNIN": lambda: station(C("T_STATION_TANNIN"), "block"),
    "T_STATION_LAMPBLACK": lambda: station(C("T_STATION_LAMPBLACK"), "chalk"),
    "T_STATION_ALUM": lambda: station(C("T_STATION_ALUM"), "block"),
    "T_STATION_GUM": lambda: station(C("T_STATION_GUM"), "flask"),
    "T_IDOOR": idoor, "T_TREE": tree, "T_RAILING": railing, "T_PLATEAU": plateau,
    "T_FLOWERBED": flowerbed, "T_LOWWALL": lowwall, "T_CRATE": crate, "T_ADIT": adit,
    "T_DOOR_HOUSE": lambda: door(C("T_DOOR_HOUSE"), "plain"),
    "T_MINE_DOOR": mine_door, "T_LAMP": lamp,
    # BUILD property phase 4: reuses grass()'s own speckle technique, sand's own colour -- no new drawing.
    "T_SAND": lambda: grass(C("T_SAND"), 0),
}
assert {ids[n] for n in DRAW} == set(range(NTILES)), "every tile id needs a drawing"

def build_tiles():
    sheet = Img(16 * T, 4 * T, (0, 0, 0, 255))
    for name, fn in DRAW.items():
        i = ids[name]
        sheet.blit(fn(), (i % 16) * T, (i // 16) * T)
    # grass variants: A phase 1,2 -> cells 48,49 ; B phase 1,2 -> cells 50,51
    for cell, base, ph in ((48, "T_GRASS_A", 1), (49, "T_GRASS_A", 2), (50, "T_GRASS_B", 1), (51, "T_GRASS_B", 2)):
        sheet.blit(grass(C(base), ph), (cell % 16) * T, (cell // 16) * T)
    return sheet

# ------------------------------------------------------------------ player sheet
DIR_DOWN, DIR_UP, DIR_LEFT, DIR_RIGHT = 0, 1, 2, 3
HAIR = hexrgb("#3a3226")

def figure(dir, frame):
    s = Img(16, 24)
    body_hi, body_lo = shade(BODY, 0.18), shade(BODY, -0.25)
    skin_lo = shade(SKIN, -0.18)
    # shadow under the feet
    s.rect(4, 23, 8, 1, (26, 24, 20, 90))
    # legs (INK trousers), alternating stride; the forward leg is one px longer
    fwd = 6 if frame == 0 else 5
    s.rect(5, 18, 2, fwd, INK)
    s.rect(9, 18, 2, 11 - fwd, INK)
    s.rect(5, 18 + fwd - 1, 2, 1, shade(INK, 0.25)); s.rect(9, 18 + (11 - fwd) - 1, 2, 1, shade(INK, 0.25))
    # body + shading
    s.rect(4, 8, 8, 10, BODY)
    s.rect(4, 8, 8, 1, body_hi)
    s.rect(4, 17, 8, 1, body_lo)
    s.rect(11, 9, 1, 8, body_lo)
    # arms swing opposite the legs
    if dir in (DIR_DOWN, DIR_UP):
        a = 1 if frame == 0 else 0
        s.rect(3, 9 + a, 1, 6, body_lo); s.rect(12, 9 + (1 - a), 1, 6, body_lo)
        s.px(3, 15 + a, SKIN); s.px(12, 15 + (1 - a), SKIN)
    else:
        s.rect(6 + frame, 10, 3, 5, body_lo)
        s.px(7 + frame, 15, SKIN)
    # head
    s.rect(5, 2, 6, 6, SKIN)
    s.rect(5, 7, 6, 1, skin_lo)
    if dir == DIR_DOWN:
        s.rect(5, 1, 6, 2, HAIR)
        s.px(5, 3, HAIR); s.px(10, 3, HAIR)
        s.px(6, 4, INK); s.px(9, 4, INK)
        s.px(7, 6, skin_lo); s.px(8, 6, skin_lo)
    elif dir == DIR_UP:
        s.rect(5, 1, 6, 5, HAIR); s.rect(6, 6, 4, 1, HAIR)
    elif dir == DIR_LEFT:
        s.rect(5, 1, 6, 2, HAIR); s.rect(8, 3, 3, 3, HAIR); s.px(5, 3, HAIR)
        s.px(6, 4, INK); s.px(5, 5, skin_lo)
    else:
        s.rect(5, 1, 6, 2, HAIR); s.rect(5, 3, 3, 3, HAIR); s.px(10, 3, HAIR)
        s.px(9, 4, INK); s.px(10, 5, skin_lo)
    return s

def build_sprites():
    sheet = Img(8 * 16, 24)
    for d in range(4):
        for f in range(2):
            sheet.blit(figure(d, f), (d * 2 + f) * 16, 0)
    return sheet

# ------------------------------------------------------------------- QA + write
def mean(img, x0, y0, w, h):
    r = g = b = 0
    for y in range(y0, y0 + h):
        for x in range(x0, x0 + w):
            p = img.p[y * img.w + x]; r += p[0]; g += p[1]; b += p[2]
    n = w * h
    return (r / n, g / n, b / n)

def main():
    tiles, sprites = build_tiles(), build_sprites()
    worst = []
    for name, fn in DRAW.items():
        i = ids[name]
        m = mean(tiles, (i % 16) * T, (i // 16) * T, T, T)
        c = COLOR[i]
        dev = sum((m[k] - c[k]) ** 2 for k in range(3)) ** 0.5
        worst.append((dev, name))
    worst.sort(reverse=True)
    print("largest mean-colour distance from each tile's COLOR (0-441 scale):")
    for dev, name in worst[:6]:
        print("  %-18s %.1f" % (name, dev))
    tol = {"T_NOTICE_POST": 85, "T_RAILING": 70}   # props drawn on a grass ground
    bad = [(d, n) for d, n in worst if d > tol.get(n, 60)]
    if bad:
        print("OFF-PALETTE:", bad); sys.exit(1)
    out = os.path.join(REPO, "commons")
    for fname, img in (("tiles.png", tiles), ("sprites.png", sprites)):
        data = img.png()
        open(os.path.join(out, fname), "wb").write(data)
        print("wrote commons/%s  %dx%d  %d bytes" % (fname, img.w, img.h, len(data)))

if __name__ == "__main__":
    main()
