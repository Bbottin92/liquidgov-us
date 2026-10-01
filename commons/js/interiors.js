"use strict";
// ---- Interiors (P2): demo house + lab. Same bake-once/blit shape as
// outdoor, swapped in via `activeSpace` so movement/collision/rendering
// don't need a second copy of that logic. Warp instances only -- the
// outdoor camera/crop rules are unchanged (BUILD_ORDER.md P2).
var outdoorSpace = {
  tiles: tiles, solid: solid, w: WORLD_W, h: WORLD_H,
  pw: WORLD_PW, ph: WORLD_PH, baked: baked, interactables: interactables
};
var activeSpace = outdoorSpace;

function idxIn(w, x, y) { return y * w + x; }

// BUILD property phase 1: baseTile/borderTile are optional (default T_FLOOR/T_IWALL, every existing
// caller's exact old behavior) -- the owner's own property is the one space that passes grass/hedge
// instead, so it reads as a plot of land, not a room.
function buildInterior(w, h, fillFn, baseTile, borderTile) {
  if (baseTile === undefined) baseTile = T_FLOOR;
  if (borderTile === undefined) borderTile = T_IWALL;
  var t = new Uint8Array(w * h);
  var s = new Uint8Array(w * h);
  for (var y = 0; y < h; y++) for (var x = 0; x < w; x++) t[idxIn(w, x, y)] = baseTile;
  for (var x2 = 0; x2 < w; x2++) { t[idxIn(w, x2, 0)] = borderTile; t[idxIn(w, x2, h - 1)] = borderTile; }
  for (var y2 = 0; y2 < h; y2++) { t[idxIn(w, 0, y2)] = borderTile; t[idxIn(w, w - 1, y2)] = borderTile; }
  fillFn(t, w, h);
  for (var i = 0; i < t.length; i++) s[i] = SOLID_TILE[t[i]] ? 1 : 0;

  var bake = document.createElement("canvas");
  bake.width = w * TILE; bake.height = h * TILE;
  var bc = bake.getContext("2d", { alpha: false });
  bc.imageSmoothingEnabled = false;
  function bakeInterior() {
    for (var yy = 0; yy < h; yy++) {
      for (var xx = 0; xx < w; xx++) {
        drawTile(bc, t[idxIn(w, xx, yy)], xx, yy, function (dx, dy) {
          var nx = xx + dx, ny = yy + dy;
          return (nx < 0 || ny < 0 || nx >= w || ny >= h) ? -1 : t[idxIn(w, nx, ny)];
        });
      }
    }
  }
  bakeInterior();
  spaceRebakers.push(bakeInterior);
  return { tiles: t, solid: s, w: w, h: h, pw: w * TILE, ph: h * TILE, baked: bake, interactables: [] };
}

// Demo house -- 16x16. Table, two chairs, rug, empty wall-slot frame
// (locked: stays empty until glue exists -- do not fake a bitmap here).
// Entrance/exit stays Enter-based, unchanged from P1; only the black
// placeholder is replaced with real content this round.
var DEMO_ENTRY = { x: 7, y: 14 };
var DEMO_DOOR = { x: 7, y: 15 };
function fillHouse(t, w) {
  t[idxIn(w, DEMO_DOOR.x, DEMO_DOOR.y)] = T_IDOOR;
  t[idxIn(w, 7, 0)] = T_WALLSLOT; // on the wall itself, not standing on the floor
  t[idxIn(w, 7, 7)] = T_TABLE;
  t[idxIn(w, 6, 7)] = T_CHAIR;
  t[idxIn(w, 8, 7)] = T_CHAIR;
  t[idxIn(w, 7, 8)] = T_RUG;
  t[idxIn(w, 7, 9)] = T_RUG;
  // C1: extra fixed decor, same in every house, existing tiles, no new art.
  t[idxIn(w, 2, 1)] = T_WORKBENCH; // shelf
  t[idxIn(w, 3, 1)] = T_WORKBENCH;
  t[idxIn(w, 14, 1)] = T_LAMP;
  t[idxIn(w, 1, 4)] = T_CRATE;     // bookcase
  t[idxIn(w, 1, 5)] = T_CRATE;
  t[idxIn(w, 13, 12)] = T_FLOWERBED; // plant
  t[idxIn(w, 4, 11)] = T_RUG;      // second rug
  t[idxIn(w, 5, 11)] = T_RUG;
}
var demoHouseSpace = buildInterior(16, 16, fillHouse);
// V2 stretch (GROK_V2_ANSWER.md): the wall-slot is interactable now, but
// only to place a lab item as a game tile -- explicitly "not a civic
// PNG." The real profile-canvas wall slot (glue.md/canvas-frame.md) is a
// different feature entirely and stays parked on civic's /latest route;
// this reuses the same physical tile for a simpler, local-only purpose.
demoHouseSpace.interactables = [
  { type: "wallSlot", x: 7, y: 0, prompt: "Place" }
];

// House policy (BUILD house friends/locked A, GROK_HOUSE_POLICY_ANSWER.md):
// the owner's own house is the same room plus one latch, a sign tile on the
// left wall, well away from the entry so the Enter-to-leave fallback never
// lands on it. Only mode "house" uses this space -- the demo house and a
// friend's house (mode "visit") keep the plain one, so nobody sees or
// touches a latch in a room that isn't theirs. `housePolicy` mirrors the
// daemon's players.visit_policy ('friends' default, 'open', 'locked'); the
// daemon sends it on the join/rekey that lands me in my own house.
var HOUSE_LATCH = { type: "latch", x: 0, y: 13, prompt: "Door: friends" };
var housePolicy = "friends";
// friends -> open -> locked -> friends. The prompt names the door's current
// state; Enter moves to the next one.
var HOUSE_POLICY_NEXT = { friends: "open", open: "locked", locked: "friends" };
var HOUSE_POLICY_TOAST = {
  friends: "Door: friends can visit.",
  open: "Door open: anyone can visit.",
  locked: "Door locked: only you."
};
function setHousePolicy(p) {
  housePolicy = HOUSE_POLICY_NEXT[p] ? p : "friends";
  HOUSE_LATCH.prompt = "Door: " + housePolicy;
}
// BUILD property phase 1 (CLAUDE_PROPERTY.md): the owner's own space is no longer fillHouse's fixed
// furnished room -- the demo house keeps that exactly as it was (still fillHouse, still T_FLOOR/T_IWALL,
// a "here's a lived-in example" reference for a fresh guest). This is an empty grass-and-hedge plot: the
// door and the wall slot stay (functional, not decoration), everything fixed-and-unchangeable (table,
// chairs, both rug pairs, both workbench "shelves", both crate "bookcases", the lamp, the flowerbed) is
// gone. The existing floor-item system (house.cpp's place_floor, already fully built) is the actual
// "design it yourself" tool now that there's room to use it in.
function fillProperty(t, w) {
  t[idxIn(w, DEMO_DOOR.x, DEMO_DOOR.y)] = T_IDOOR;
  t[idxIn(w, 7, 0)] = T_WALLSLOT; // on the wall itself, not standing on the floor
}
var ownHouseSpace = buildInterior(16, 16, function (t, w) {
  fillProperty(t, w);
  t[idxIn(w, HOUSE_LATCH.x, HOUSE_LATCH.y)] = T_SIGN;
}, T_GRASS_A, T_HEDGE);
ownHouseSpace.interactables = [
  { type: "wallSlot", x: 7, y: 0, prompt: "Place" },
  HOUSE_LATCH,
  { type: "studioPortrait", x: 10, y: 0, prompt: "Draw" },
  { type: "studioPainting", x: 4, y: 0, prompt: "Draw" }
];

// BUILD property coherence: a visit used to enter demoHouseSpace, so a friend saw the furnished
// room with your things drawn on top of it. This is the same grass and hedge as your own plot,
// without the latch and without an easel. The frames are here so Enter still only toasts
// (interact.js already refuses to open the easel when mode is "visit"). The drawing itself
// keys off mode, not off this list.
var visitSpace = buildInterior(16, 16, fillProperty, T_GRASS_A, T_HEDGE);
visitSpace.interactables = [
  { type: "wallSlot", x: 7, y: 0, prompt: "Look" },
  { type: "studioPortrait", x: 10, y: 0, prompt: "Draw" },
  { type: "studioPainting", x: 4, y: 0, prompt: "Draw" }
];

// Studio portrait frame (BUILD studio 2). Pinned against this 16x16 house, not the
// unverified candidate in studio.md. y=0 is the solid top wall. The civic profile
// drawing is painted 3 tiles wide and 2 tall, centered on the wall slot at (7,0),
// so it covers x 6-8 and y 0-1. Shelves are the workbenches at (2,1) and (3,1).
// The lamp is (14,1). The owner's latch is (0,13). The door is (7,15).
// (BUILD property phase 1: the shelves/lamp this paragraph pins against are gone from
// the owner's own house now -- fillHouse, and so these landmarks, still describe the
// demo house unchanged. The coordinates picked relative to them are untouched either way.)
// x 10-11 are plain wall, one tile clear of the civic frame and three clear of
// the lamp. The portrait is 32x32, which is exactly 2x2 tiles of 16px: (10,0) and
// (11,0) on the wall, (10,1) and (11,1) over the floor the same way the civic
// frame already overlays y=1. Those floor tiles stay walkable. The demo house
// does not get the interactable; a visit sees the image but does not open the easel.
var STUDIO_PORTRAIT = { x: 10, y: 0, w: 2, h: 2 };

// Second painting frame (BUILD studio 4, CLAUDE_STUDIO_4.md decision 2). x 4-5 is the old unverified
// candidate from studio.md before slice 2 -- checked for real against this house now: the shelves end
// at x=3, the civic frame starts at x=6, so x 4-5 is flush against both, not gapped like the portrait's
// own one-tile gap at x=9. The only other clear 2-wide slot is x 12-13, but that sits flush against
// BOTH the portrait and the lamp, which would read as one wide picture -- x 4-5 was preferred. y=0 is
// still the solid wall (overlay only, same as the portrait); (4,1) and (5,1) stay plain floor, walkable,
// no tile changed here either. Owner's house only; the demo house does not get this interactable, and a
// visit draws the image without opening the easel (same split the portrait already has).
var STUDIO_PAINTING = { x: 4, y: 0, w: 2, h: 2 };

// Lab -- 20x16. Eight lookup stations: the original three (pushback.md's
// chalk-only recipe, no lime) plus five more (GROK_OBJECTS_ANSWER.md) in
// a second row. Same loop: look up, +1, never consumed. Door is a walk-in
// trigger both ways, matching its outdoor half (atlas.md).
var LAB_ENTRY = { x: 10, y: 14 };
var LAB_DOOR = { x: 10, y: 15 };
// item is the real catalog slug (schema.sql/seed.sql/catalog.sql items.slug). Only the three
// unconverted lookups are left here (BUILD studio 3, CLAUDE_STUDIO_3.md's frozen lab table): vinegar,
// tannin, lampblack and alum, gum moved to labStock / studioGather below.
var LAB_RECIPES = {
  copper: { name: "Copper + acetic acid → copper acetate", item: "copper_acetate_bottle" },
  iron:   { name: "Iron + tannin → iron-gall ink", item: "iron_gall_ink_page" },
  chalk:  { name: "Calcium carbonate, ground and washed → chalk", item: "chalk_block" }
};
var PLACED_ITEM_INFO = {
  chalk_block:            { color: "#e8e2d0", label: "chalk" },
  iron_gall_ink_page:     { color: "#2a2320", label: "ink" },
  copper_acetate_bottle:  { color: "#4a8a6a", label: "acetate" },
  vinegar_jar:            { color: "#7a5a32", label: "vinegar" },
  oak_gall_tannin:        { color: "#4a3020", label: "tannin" },
  lampblack_ink:          { color: "#2a2824", label: "soot" },
  alum_crystal:           { color: "#c5c8c2", label: "alum" },
  gum_arabic:             { color: "#a07a48", label: "gum" },
  oak_gall:               { color: "#4a3020", label: "galls" },
  seashell:               { color: "#c5c0b4", label: "shell" },
  cider_apple:            { color: "#8a3a32", label: "apple" },
  // BUILD studio 3: every Studio item that can now actually reach a bound player's inventory this
  // slice (gathered raw materials, plus every bench output -- the bench overlay this slice can start
  // and collect all 13 recipes). Colour is the catalog's own pigment_hex where the item has one
  // (catalog.sql); the rest are a plain earth/ink tone. No new bake-time art: these fall through to
  // the generic textured square in sprites.js's paintItemAtlas, same as any slug not special-cased there.
  // Live-caught bug (2026-09-27, playing on liquidgov.us after deploying slices 1-4): four of these
  // five real-world substances also exist as an OLD Lab-A trophy (alum_crystal, gum_arabic,
  // vinegar_jar, lampblack_ink) with the identical color+label already used above -- copying that
  // exact styling here made the inventory strip show two indistinguishable "alum"/"gum"/"vinegar"/
  // "soot" chips with only their quantities different. Each gets its own label and color instead.
  willow_twigs:           { color: "#8a7a4a", label: "twigs" },
  ochre_earth:            { color: "#b8823a", label: "ochre" },
  alum_stone:             { color: "#a8ab9e", label: "alum stone" },
  madder_root:            { color: "#7a3a2a", label: "madder" },
  weld:                   { color: "#8a9a3a", label: "weld" },
  woad_leaves:            { color: "#4a6a4a", label: "woad" },
  scrap_iron:             { color: "#6a6a6a", label: "iron" },
  copper_sheet:           { color: "#9a6a3a", label: "copper" },
  gum_arabic_raw:         { color: "#c9954f", label: "raw gum" },
  soot:                   { color: "#454138", label: "raw soot" },
  vinegar:                { color: "#9c7a42", label: "keg vinegar" },
  charcoal_black:         { color: "#1a1a1a", label: "black" },
  charcoal_grey_1:        { color: "#4d4d4d", label: "grey" },
  charcoal_grey_2:        { color: "#7a7a7a", label: "light grey" },
  charcoal_grey_3:        { color: "#adadad", label: "pale grey" },
  shell_white:            { color: "#f2ede4", label: "white" },
  yellow_ochre:           { color: "#c9963a", label: "yellow ochre" },
  red_ochre:              { color: "#9c4a2e", label: "red ochre" },
  binder:                 { color: "#c9b06a", label: "binder" },
  tannin:                 { color: "#5a3a26", label: "bench tannin" }, // same live bug: was identical to oak_gall_tannin's "tannin"
  carbon_ink_black:       { color: "#0d0d0d", label: "carbon ink" },
  iron_acetate:           { color: "#6a4a32", label: "iron acetate" },
  iron_gall_blue_black:   { color: "#1c1c24", label: "iron-gall ink" },
  verdigris_green:        { color: "#3f8f5f", label: "verdigris" },
  madder_red:             { color: "#a72a2a", label: "madder lake" },
  weld_yellow:            { color: "#e0c04a", label: "weld lake" },
  woad_blue:              { color: "#2e4c72", label: "woad blue" },
  // BUILD property phase 2+3 (CLAUDE_PROPERTY_PHASE2.md): a tool (held, never consumed) and the
  // structural item it unlocks. Colour is plain wood/iron tone -- neither is paint, no pigment_hex.
  hammer:                 { color: "#8a8a8a", label: "hammer" },
  fence_post:             { color: "#8a7a4a", label: "fence post" }
};

// BUILD backpack (CLAUDE_BACKPACK.md decision 2): a small set of procedural shape categories for the
// ~24 slugs that otherwise fall through to sprites.js's generic textured square. Lives next to
// PLACED_ITEM_INFO, not folded into it -- colour/label and shape are two separate concerns. A slug not
// listed here either has bespoke art already (sprites.js's own slug checks) or is meant to keep the
// plain square. sprites.js reads this table to pick a shape; backpack.js's categoryOf() reads it too,
// to group the backpack grid (falling back to the bespoke slugs' own real category there).
var ITEM_CATEGORY = {
  willow_twigs:     "twig",
  ochre_earth:      "stone",
  alum_stone:       "stone",
  scrap_iron:       "stone",
  copper_sheet:     "stone",
  madder_root:      "leaf",
  weld:             "leaf",
  woad_leaves:      "leaf",
  soot:             "powder",
  gum_arabic_raw:   "bundle",
  binder:           "bundle",
  tannin:           "bundle",
  iron_acetate:     "bundle",
  charcoal_black:   "swatch",
  charcoal_grey_1:  "swatch",
  charcoal_grey_2:  "swatch",
  charcoal_grey_3:  "swatch",
  shell_white:      "swatch",
  yellow_ochre:     "swatch",
  red_ochre:        "swatch",
  verdigris_green:  "swatch",
  madder_red:       "swatch",
  weld_yellow:      "swatch",
  woad_blue:        "swatch",
  // BUILD property phase 2+3: fence_post is wood, same category as the twigs it's made from. hammer
  // gets the one new category this build adds (sprites.js's "tool") -- nothing existing reads as a tool.
  fence_post:       "twig",
  hammer:           "tool"
};

// BUILD backpack (decision 3): one short sentence per slug, client-side only (no schema/catalog change --
// this text is never read by the daemon). Several lean on the same real-world grounding the recipe
// history_notes already use (catalog.sql). A slug missing here falls back to its own PLACED_ITEM_INFO
// label in backpack.js's itemTipText() -- never a blank tooltip.
var ITEM_DESC = {
  chalk_block:            "Calcium carbonate, ground and washed.",
  iron_gall_ink_page:     "A page already written in iron-gall ink.",
  copper_acetate_bottle:  "Copper, dissolved in acetic acid.",
  vinegar_jar:            "Acetic acid from fermented cider. Old stock -- the Orchard keg makes it now.",
  oak_gall_tannin:        "Oak galls, crushed and steeped. Old stock -- the bench makes it now.",
  lampblack_ink:          "Soot bound with gum arabic.",
  alum_crystal:           "Potassium aluminium sulfate, crystallised.",
  gum_arabic:             "Acacia resin, dissolved in water.",
  oak_gall:               "A gall from an oak, full of tannin.",
  seashell:               "Calcium carbonate, from the shore.",
  cider_apple:            "A fallen apple from the Orchard.",
  willow_twigs:           "Young willow, left to dry before the charcoal pit.",
  ochre_earth:            "Iron-rich earth, the source of yellow ochre.",
  alum_stone:             "The mordant that fixes a lake pigment to its base.",
  madder_root:            "The root behind madder lake's red.",
  weld:                   "A dye-plant, the source of weld lake's yellow.",
  woad_leaves:            "Fermented for a blue dye since antiquity.",
  scrap_iron:             "Old nails, steeped in vinegar for iron acetate.",
  copper_sheet:           "Rests in vinegar fumes, then scraped, for verdigris.",
  gum_arabic_raw:         "Acacia resin, not yet dissolved into a binder.",
  soot:                   "Lampblack, the base of carbon ink.",
  vinegar:                "Acetic acid from the Orchard keg.",
  charcoal_black:         "Willow, burned slow and left overnight.",
  charcoal_grey_1:        "Charcoal, worn lighter.",
  charcoal_grey_2:        "Charcoal, lighter still.",
  charcoal_grey_3:        "The palest charcoal grey.",
  shell_white:            "Ground shell, an old white pigment.",
  yellow_ochre:           "Earth pigment, washed and settled.",
  red_ochre:              "Yellow ochre, heated.",
  binder:                 "Gum arabic, dissolved in water.",
  tannin:                 "Oak galls, steeped for the base of iron-gall ink.",
  carbon_ink_black:       "Soot bound with gum arabic -- the fast path.",
  iron_acetate:           "Iron steeped in vinegar. Folk method, unsourced.",
  iron_gall_blue_black:   "Tannin, iron and gum. Ages toward brown.",
  verdigris_green:        "Copper's green -- the slow, expensive one.",
  madder_red:             "A lake pigment, later and northern-European.",
  weld_yellow:            "A lake pigment, from a Bolognese method.",
  woad_blue:              "A simplified vat, settled onto shell white.",
  hammer:                 "A hammer head set into a willow haft. Yours to keep; it never wears out.",
  fence_post:             "Willow, trimmed and pointed. Plant it, and it stays."
};

// BUILD property phase 2+3 (CLAUDE_PROPERTY_PHASE2.md decisions 2/3): which placed-floor items are
// structural -- block movement, gated on holding a hammer -- rather than purely decorative. A real table,
// not one hardcoded slug check, so a second structure type later is one more line, not a new branch.
var STRUCTURAL_ITEMS = { fence_post: true };
var labSpace = buildInterior(20, 16, function (t, w) {
  t[idxIn(w, LAB_DOOR.x, LAB_DOOR.y)] = T_IDOOR;
  t[idxIn(w, 8, 1)]  = T_WORKBENCH;
  t[idxIn(w, 9, 1)]  = T_WORKBENCH;
  t[idxIn(w, 10, 1)] = T_WORKBENCH;
  t[idxIn(w, 11, 1)] = T_WORKBENCH;
  t[idxIn(w, 5, 7)]  = T_STATION_COPPER;
  t[idxIn(w, 10, 7)] = T_STATION_IRON;
  t[idxIn(w, 14, 7)] = T_STATION_CHALK;
  t[idxIn(w, 3, 10)]  = T_STATION_VINEGAR;
  t[idxIn(w, 6, 10)]  = T_STATION_TANNIN;
  t[idxIn(w, 9, 10)]  = T_STATION_LAMPBLACK;
  t[idxIn(w, 12, 10)] = T_STATION_ALUM;
  t[idxIn(w, 15, 10)] = T_STATION_GUM;
  // The door and the stations were in the same room and nothing connected
  // them. A rug down the open column between the stations (iron is x=10,y=7;
  // the y=10 stations sit at 9 and 12) is the aisle, not a new prop.
  t[idxIn(w, 10, 8)] = T_RUG;
  t[idxIn(w, 10, 9)] = T_RUG;
  t[idxIn(w, 10, 11)] = T_RUG;
  t[idxIn(w, 10, 12)] = T_RUG;
  t[idxIn(w, 10, 13)] = T_RUG;
  t[idxIn(w, 10, 14)] = T_RUG;
});
// BUILD studio 3 (CLAUDE_STUDIO_3.md's frozen lab table). Prompt stays Look on every one of these --
// only what happens on Look changes. copper/iron/chalk are untouched trophy lookups. vinegar/tannin/
// alum grant nothing anymore and just toast once, pointing at where the real thing now lives. lampblack
// and gum become studio gather spots (spots "lab_soot" / "lab_gum", already in the daemon's fixed
// table since slice 1) -- a bound Look posts to the daemon like any outdoor gather spot; a guest gets
// the same local session-only cooldown.
labSpace.interactables = [
  { type: "labStation", recipe: "copper", x: 5,  y: 7,  prompt: "Look" },
  { type: "labStation", recipe: "iron",   x: 10, y: 7,  prompt: "Look" },
  { type: "labStation", recipe: "chalk",  x: 14, y: 7,  prompt: "Look" },
  { type: "labStock", stock: "vinegar", x: 3,  y: 10, prompt: "Look",
    toast: "Stock. Vinegar is the keg in the Orchard." },
  { type: "labStock", stock: "tannin", x: 6,  y: 10, prompt: "Look",
    toast: "Stock. Make tannin at the Workshop." },
  { type: "studioGather", spot: "lab_soot", x: 9, y: 10, prompt: "Look" },
  { type: "labStock", stock: "alum", x: 12, y: 10, prompt: "Look",
    toast: "Stock. Alum stone is on Quarry Ridge." },
  { type: "studioGather", spot: "lab_gum", x: 15, y: 10, prompt: "Look" }
];

// The mine (BUILD mine A, GROK_MINE_ANSWER.md) -- 16x12, an honest cave: dirt
// floor, rock walls, two sit-rocks, one lit lamp tile, one sign. No ore, no
// stations, nothing to place or pick up. Shared instance:mine like demo/lab,
// so guests and bound players both walk in; Enter leaves (same as the demo
// house), back onto the tile in front of the adit.
var MINE_ENTRY = { x: 7, y: 10 };
var MINE_DOOR = { x: 7, y: 11 };
var mineSpace = buildInterior(16, 12, function (t, w, h) {
  for (var my = 0; my < h; my++) for (var mx = 0; mx < w; mx++) t[idxIn(w, mx, my)] = T_DIRT;
  for (var mx2 = 0; mx2 < w; mx2++) { t[idxIn(w, mx2, 0)] = T_LOWWALL; t[idxIn(w, mx2, h - 1)] = T_LOWWALL; }
  for (var my2 = 0; my2 < h; my2++) { t[idxIn(w, 0, my2)] = T_LOWWALL; t[idxIn(w, w - 1, my2)] = T_LOWWALL; }
  t[idxIn(w, MINE_DOOR.x, MINE_DOOR.y)] = T_IDOOR;
  t[idxIn(w, 7, 0)] = T_LAMP;   // set into the back wall, a lit tile
  t[idxIn(w, 3, 6)] = T_BENCH;  // the two sit-rocks
  t[idxIn(w, 12, 6)] = T_BENCH;
  t[idxIn(w, 10, 2)] = T_SIGN;
});
mineSpace.interactables = [
  { type: "sign", x: 10, y: 2, prompt: "Read", text: "No ore. That's honest." }
];

// The Workshop (BUILD workshop A) -- 14x10, thin like the Hall: a plain room
// behind the Workshop Lane door. Two benches along the back wall, a lamp, a
// worktable with two stools, crates, a rug, one sign. All existing tiles. No
// crafting, no stations, nothing to place or pick up (V1's "no crafting"
// stands). Shared instance:workshop like demo/lab/mine: guests and bound
// players both walk in, no rows, no snapshot. Enter with nothing in range
// leaves, onto the tile in front of the door (no re-entry oscillation).
var WORKSHOP_ENTRY = { x: 7, y: 8 };
var WORKSHOP_DOOR = { x: 7, y: 9 };
var workshopSpace = buildInterior(14, 10, function (t, w) {
  t[idxIn(w, WORKSHOP_DOOR.x, WORKSHOP_DOOR.y)] = T_IDOOR;
  t[idxIn(w, 7, 0)] = T_LAMP;   // set into the back wall, a lit tile
  for (var bx = 2; bx <= 4; bx++) t[idxIn(w, bx, 1)] = T_WORKBENCH;
  for (var bx2 = 9; bx2 <= 11; bx2++) t[idxIn(w, bx2, 1)] = T_WORKBENCH;
  t[idxIn(w, 6, 4)] = T_TABLE;  // the worktable and its two stools
  t[idxIn(w, 7, 4)] = T_TABLE;
  t[idxIn(w, 5, 4)] = T_CHAIR;
  t[idxIn(w, 8, 4)] = T_CHAIR;
  t[idxIn(w, 1, 3)] = T_CRATE;
  t[idxIn(w, 1, 4)] = T_CRATE;
  t[idxIn(w, 12, 6)] = T_CRATE;
  t[idxIn(w, 12, 7)] = T_CRATE;
  t[idxIn(w, 6, 6)] = T_RUG;
  t[idxIn(w, 7, 6)] = T_RUG;
  // The rug used to be a patch with no relation to the door or the table.
  // Same tile, extended one step toward each, so the walk from the door to
  // the easel is the thing the floor is pointing at. Entry tile stays floor.
  t[idxIn(w, 7, 5)] = T_RUG;
  t[idxIn(w, 7, 7)] = T_RUG;
  t[idxIn(w, 12, 3)] = T_SIGN;
  // BUILD studio 3: the bench and the notebook, on the two existing workbench tiles from the P2
  // set-dressing pass (no new tile, no new art) rather than inventing a crafting-minigame prop.
  t[idxIn(w, 3, 2)] = T_WORKBENCH;
  t[idxIn(w, 10, 2)] = T_WORKBENCH;
});
workshopSpace.interactables = [
  { type: "sign", x: 12, y: 3, prompt: "Read", text: "The bench is on the left. The book is on the right." },
  { type: "easel", x: 7, y: 4, prompt: "Draw" },
  { type: "bench", x: 3, y: 2, prompt: "Make" },
  { type: "notebook", x: 10, y: 2, prompt: "Read" }
];

// Pickups. A guest's are session-only (v1.md: RAM, vanish on refresh --
// "Do not invent guest persistence"). A bound player's are also saved by the
// daemon (GROK_V6_ANSWER.md, bound inventory) and restored on load, so
// `inventory` and `lastPickedItem` survive a refresh or a daemon restart.
// lastPickedItem is what the wall and the floor place. The counts used to
// be painted again as chips on the grass (#invHud). The backpack is that
// list, so the strip stays empty -- a second copy of the same items.
var inventory = {};
var lastPickedItem = null;

function renderInvHud() {
  var el = document.getElementById("invHud");
  var bag = document.getElementById("bagBtn");
  if (el) el.innerHTML = "";
  // The Bag button is the widget's toggle, like the juketube button: it stays
  // even when nothing is held, so an empty pack can still be opened.
  if (bag) bag.hidden = false;
  if (typeof backpackOpen === "function" && backpackOpen() && typeof renderBackpack === "function") renderBackpack();
}

