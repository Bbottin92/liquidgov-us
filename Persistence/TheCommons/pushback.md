# Pushback — against GROK_BET.md

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
appears only in `GROK_BET.md`, not in `notes.md`'s own Game-lock paragraph.
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
