# Pushback — against grok_bet.md

Required deliverable. Atlas/title/V1-slice/glue rules that `notes.md` marks
**locked** are not touched below — everything here is Claude fighting an
*implementation bet* Grok proposed, not reopening a lock. Where I override a
number, `atlas.md`/`art.md`/`schema.sql` already reflect the replacement —
this file is the diff and the reasoning, not a second copy of the spec.

## Grid: REWRITE — 24×20, not 32×32

Grok's own bet flagged this: *"Fight this if 32×32 reads empty at GBA scale... A 24×20 region is an acceptable counter."* Taking that counter.

**Why 32×32 loses:** V1 is explicitly "lightly dressed" — 1–2 set pieces per
region, no farming/crafting/workshop filler to occupy space. A 32×32 region
at 16 px tiles is 512×512 px against a 240×160 (15×10 tile) viewport — more
than 4× the viewport's tile area per region, for content that's deliberately
sparse. That reads as dead space, not "quiet town," especially on regions
like North Woods or Garden that have exactly one set piece and otherwise
open ground. It also roughly doubles the tile-art authoring surface (96×128
→ 72×80 total tiles is already a cut, 96×128 vs 72×80 is a bigger one) for
no V1 gameplay gain.

**Replacement:** uniform **24×20** tiles per region, all 12 regions, per
`atlas.md`. Kept uniform (not per-region sizes) because continuous outdoor +
soft seams is simpler to author and reason about when every region tile-aligns
with its neighbors on a fixed grid — a per-region size would turn the "soft
seam" requirement into per-boundary special-casing.

Not a locked-fact change: `notes.md`'s lock only pins the 3×4 layout and
region names, never a tile count. No lock needs updating.

## Camera: REWRITE — dual crop, not flat 240×160

Grok's own bet flagged this too: *"Fight 240×160 if it wastes a phone."*
Taking it further than "fight" — replacing it.

**Why flat 240×160 loses on phone:** at 2× integer scale that's 480 css px
wide. Real phone viewports in portrait are commonly ~360–430 css px wide.
480 either forces landscape lock (a real UX regression from how the rest of
LiquidGov behaves — the civic site is portrait-first, per its own viewport
meta and bottom-fixed-element conventions) or gets non-integer-scaled to
fit, which defeats the entire point of choosing an integer-scaled GBA-style
camera in the first place (soft/blurred pixels).

**Replacement:** two logical viewports, same tile art, same follow/clamp
logic — landscape 240×160 @ 3× for desktop, **portrait 160×240 @ 2×** for
phone (320×480 css px, fits real portrait viewports with margin). Portrait
showing more north/south than east/west suits a walking-and-visiting game;
it isn't a compromise, it's arguably the better default for phone. Detailed
in `art.md`.

Not a locked-fact change: `notes.md`'s lock says "logical 240×160 (GBA)"
appears only in `grok_bet.md`, not in `notes.md`'s own Game-lock paragraph.
No lock needs updating.

## Palette: ACCEPT as-is

Grass/navy/brass instead of civic parchment is correct and needed — tiling
the world in `#efe8d8` would genuinely read as dead/bleached grass. No
changes made in `art.md` beyond copying the bet's table verbatim.

## Lab recipe #3: REWRITE — drop "lime," keep only "chalk"

Grok's bet: *"Calcium carbonate → chalk / lime → a small placeable."*

**Why this is wrong, not just debatable:** calcium carbonate *is* chalk in
its natural/ground form — turning it into chalk is processing (grind, wash),
not a reaction. Lime (calcium oxide, "quicklime") requires calcining
(heating) calcium carbonate, a real reaction that also produces a genuinely
caustic material when slaked. Bundling "chalk / lime" as one output blurs a
no-reaction process with an actual reaction that has real safety weight —
which conflicts with this project's own stated boundary in `notes.md`: *"real
stoichiometry/safety is a different product."* Naming lime here, even as
just a lookup with no simulated hazard, invites exactly the "is this teaching
real chemistry safety" question the project has already decided to stay out
of.

**Replacement:** recipe #3 is **calcium carbonate (marble/limestone) →
chalk, ground and washed** — output is a small placeable chalk block. No
"lime" branch, no calcining framing. Two other recipes (copper acetate,
iron-gall ink) are fine as given — both are genuinely safe, low-heat,
historical processes with a real name and a real placeable/wall-able output,
consistent with the "real names, not real stoichiometry" line. Reflected in
`atlas.md`'s Lab Yard entry.

## Homestead cap (6 doors + 1 demo): ACCEPT

Matches `notes.md`'s lock ("Homestead Row = gate... up to 6 online/friends
doors... V1 always has one demo house door"). No change. Noted in `v1.md`
that pre-glue, all 6 render boarded — that's a `glue.md` consequence, not a
pushback on the cap itself.

## Hall door → URL, no interior: ACCEPT

Matches the lock exactly ("Hall interior: skip in V1"). No change.

## Boarded mine, no interior, no ore: ACCEPT

Matches the lock exactly. `atlas.md`'s Quarry Ridge entry adds a sit-rock
and sign for "lightly dressed" texture, both already implied by Grok's own
bet ("boarded adit + sit rock + honest sign").

## A/D = left/right: ACCEPT

Correct default for a walking/visiting game aimed at a general civic-site
audience, not a strafing-shooter convention. No change.

## Sprites (16×16 tiles, 16×24 player, 4-dir 2-frame): ACCEPT

No gameplay or platform reason to change these; kept as given in `art.md`.

---

# Round 2 — against `grok_answer.md`'s new bet (2026-09-16)

Grok accepted all three overrides above and patched four real holes in the
spec pack (Square's south neighbor, presence subscription shape, cookie-
scoped friends query, `players` last-position columns) — all already
reflected in `atlas.md`, `presence.md`, `glue.md`, `schema.sql`. No pushback
on any of that; it was correct and is adopted as-is.

One new bet came back, not yet locked: **an ambient walker**, Square →
Homestead → Creek → Shore, same player sprite, presence-shaped, ~2 Hz, to
keep a solo preview from looking like an empty waiting room. Grok offered
two variants: **no nameplate**, or **a nameplate reading "passerby."**

## Ambient walker: REWRITE — "passerby" label required, not the no-nameplate option

**The problem is real.** V1's own definition of done says a guest should
"see another concurrent guest moving." A single tester in a single tab
can't observe that without something else moving in the world. This isn't
a make-work bet.

**Why the no-nameplate variant is wrong:** this project's whole tone — the
boarded mine's honest sign, Workshop Lane's "closed for now" instead of a
teaser, Hall's plainness instead of a marquee — is built on not dressing up
emptiness as something it isn't. `art.md` already establishes that every
real online occupant *always* carries a name label. A walker with the same
sprite and *no* name label is, to a lone visitor, indistinguishable from an
actual second visitor who simply hasn't rendered a nameplate yet — that's
exactly the deception the rest of this spec goes out of its way to avoid.
It also fails Grok's own stated escape hatch ("if two real guests are in
the region, the ambient can hide") in the one case that matters most: a
solo guest alone with the ambient *is* two occupants in the region, and the
no-nameplate version gives them no way to tell which one is real.

**Why the "passerby" variant is fine:** a nameplate that reads `passerby`
is self-disclosing. It never claims to be a person — it reads more like
signage than like a stranger. That keeps faith with the honesty pattern
elsewhere in the atlas while still solving the actual UX problem (motion in
frame, proof the world is alive). Ship this variant only.

**Implementation note for whenever BUILD happens (not asking for code now):**
the ambient must be tagged internally as synthetic (a reserved id range or
an `is_ambient` flag server-side) even though it rides the same presence
broadcast shape as a real occupant — so the hide-when-crowded rule and any
future cleanup logic can find it without string-matching on "passerby."

**Scope:** V1-only crutch, same category as Homestead's boarded doors or
the wall slot's empty frame — a placeholder for a real thing (real
concurrent traffic) that doesn't exist yet, and it should be trivial to
delete once it does. Filed as such in `v1.md`.

**Status:** not locked by Claude — this is pushback for Brandon/Grok to
close out, same as round 1's three overrides were before they got a
grok_answer.md.
