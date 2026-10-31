# BUILD backpack — proposed (awaiting Brandon's sign-off)

Brandon: a real backpack screen for all items, each with its own icon, a hover/tap description bubble,
and a much more organized/visually appealing bench. Asked to clarify the "currency" line first: no
currency system exists anywhere in this game (no schema column, no daemon concept, and the whole design
has deliberately avoided anything economy-shaped) — confirmed to drop it entirely, not build one.

This is not part of `studio.md`'s own numbered slices (the backpack covers every item in the game, not
just Studio ones), so it gets its own name rather than being squeezed into "studio 6." **Client only.**
No schema, no daemon route change, no new external art files — same house rule the map/scatter work
already followed ("no new art, no new collision rule"), applied here as "no new *image* assets, only more
procedural canvas drawing," matching what `sprites.js` already does for the handful of items it bothers
to draw specially today.

## Scope, plainly

- A real backpack overlay listing **every** item currently held, not just the abbreviated top-left strip.
- Every item gets a distinguishable icon — not the flat "generic textured square" ~30 of the 38 catalog
  items fall back to today.
- Hovering (mouse) or tapping (touch) an item shows a short description in a bubble above it.
- The bench overlay gets an actual visual pass: sections, icons on recipes, a real progress readout for
  batches instead of a plain "Xh" text line.
- No currency, no capacity bar, no new mechanic of any kind — this is a pure information/legibility pass
  over data that already exists (the same shared `inventory` the top-left strip already reads).

## Decision 1 — opening the backpack

**Recommended: a new persistent "Bag" button, always visible next to `#invHud`, plus a keyboard
shortcut.** The strip stays exactly as it is (it's the "what's in your hand" quick-reference, and
`lastPickedItem`/wall-placing still needs it) — the backpack is a *second*, fuller view on top of the
same `inventory` object, not a replacement.

- A small button rendered next to the strip (`#invHud`'s own row), always there once the strip has
  anything to show, labelled "Bag."
- Keyboard: `I` opens/closes it (checked the same way `easelOpen()`/`benchOpen()`/`notebookOpen()`
  already gate movement — `typingInField()` grows one more `#backpack.open` check).
- Rejected: putting it behind a world interactable (a "chest" tile, say). Your held items aren't
  somewhere in the world; gating access to *your own inventory* behind a tile you have to walk to would
  be adding friction to something that should just always be reachable.

## Decision 2 — icons, without new art files

`sprites.js`'s `paintItemAtlas()` currently special-cases about eight slugs (chalk, ink bottles, a jar,
a shell, a gum blob, an oak gall) and falls through to one generic 8×8 highlighted square for everything
else — which is most of the Studio catalog. Hand-drawing 38 fully bespoke pixel glyphs is real effort for
low payoff (most raw materials don't have a natural "shape" the way a bottle or a shell does). Proposed
instead: **a small set of shape *categories*, each item mapped to the category that fits its real-world
form, still tinted with that item's own colour** (already assigned, and already fixed by the inventory-
chip-collision work from a few builds ago):

| Category | Shape (16×16, procedural) | Example items |
|---|---|---|
| twig/stick | two crossed diagonal bars | willow_twigs |
| leaf/plant | a simple pointed-leaf outline | woad_leaves, madder_root, weld |
| stone/ore | an irregular hexagon-ish blob | ochre_earth, alum_stone, scrap_iron, copper_sheet |
| shell | *(already bespoke — unchanged)* | seashell |
| jar/bottle | *(already bespoke — unchanged)* | vinegar_jar, copper_acetate_bottle, and the new `vinegar` |
| powder/pile | a small triangular heap | soot, chalk_block *(chalk stays its own bespoke glyph)* |
| swatch | a filled rounded square with a lighter corner highlight | every pigment with a `pigment_hex` (charcoal_black..woad_blue) |
| ink | *(already bespoke — unchanged)* | carbon_ink_black, iron_gall_blue_black extend the existing ink glyph |
| bundle | a tied-looking double-lump | oak_gall, oak_gall_tannin, tannin, binder, iron_acetate |

Each category is one new small drawing function in `sprites.js`'s existing `paintItemAtlas()` (same
mechanism, same bake-once atlas, same `drawItemGlyph()` call sites — the backpack, the bench, and the
existing strip all get the better icon for free, automatically, with no separate icon system to build).
The mapping (slug → category) lives next to `PLACED_ITEM_INFO`, not a second colour table.

## Decision 3 — descriptions

No `description` column exists on `items`, and adding one means a schema change, a catalog edit, and a
new field on every inventory-serving response — for text that is purely client-side flavour and never
read by the daemon. Proposed: **a client-side `ITEM_DESC` table** (one line per slug, same file as
`PLACED_ITEM_INFO`), one short sentence each, in the same voice the recipe `history_note`s already use —
several can lean on text that already exists there rather than inventing new copy from scratch (e.g.
`willow_twigs`: "Young willow, left to dry before the charcoal pit."). A slug with no entry falls back to
its own `display_name` — never a blank bubble, and never a hard failure for a slug someone forgets to
add text for later.

## Decision 4 — the tooltip bubble

One shared DOM element (`#itemTip`), reused for every chip in both the strip and the backpack grid —
not one tooltip node per item. Shown on `pointerenter` (desktop hover) and on `click`/`tap` (so it also
works with no mouse), positioned above the hovered/tapped chip using its own bounding rect, hidden on
`pointerleave` (hover) or a second tap / tapping elsewhere (touch). Text is `ITEM_DESC[slug]`. This
already covers "hover with a mouse" and "tap on mobile" as one implementation, not two.

## Decision 5 — the backpack layout

Full-screen overlay, same `#easel`/`#bench`/`#notebook` shell. A grid of chips (icon + label + qty, no
different from the strip's own chip shape, just bigger and organized as a real grid instead of a
wrapping row), sorted by category then alphabetically — not insertion order, which is what the strip
does today and reads as random once you're holding thirty different things. Clicking a chip sets
`lastPickedItem`, same as the strip already does (so the backpack is a superset of the strip's own
behaviour, not a separate parallel state).

## Decision 6 — the bench, visually

Same data, same routes, no behaviour change — purely CSS/DOM structure:

- A header row per section ("Open batches", "You can make", "Try something") instead of three bare
  `<div>`s running together with only whitespace between them.
- Each recipe/batch row gets the same icon its *output* item would show in the backpack (via the same
  category table), so "what does starting this actually make" is visible before you've ever collected it
  once.
- An open batch's countdown becomes a small progress bar (elapsed vs. `ready_ms - started_ms`) alongside
  the existing text, not instead of it — the exact minutes/hours text stays, since that is real
  information the spec cares about ("Names and waiting," not a mystery meter).
- The mix chip row gets its own visually distinct strip (a different background tint) so it doesn't read
  as "more of the same list" — CLAUDE_STUDIO_3.md was already explicit that side-mixing is a different,
  lesser action from starting a real batch, and the visual weight should say so too.

## Out of scope, said plainly

No currency (confirmed). No drag-and-drop reordering. No stacking/sorting preferences saved between
sessions (there is no per-viewer persistence in this client at all — `localStorage` is not part of this
game's architecture and this proposal isn't introducing it). No new interactable in the world. No change
to what a guest can or can't hold — the backpack shows whatever `inventory` already has, guest or bound,
exactly like the strip does today.

## Tests

`client_files_unit.js` grows: the backpack entry points exist (`openBackpack`/`closeBackpack`), the Bag
button and `#itemTip` markup exist, `ITEM_DESC`/the category table cover every `PLACED_ITEM_INFO` slug
(so nothing silently falls through to an undefined icon or a blank tooltip), a guest opening the backpack
never fetches (it's pure client-side `inventory`, same as the strip). No daemon test changes — nothing
here touches a route.

## Deploy

Client-only, same as most of the recent work. Holds for a literal `deploy`, same as always.

---

**Say `BUILD backpack` to build this as written, or tell me what to change first.**
