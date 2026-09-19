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

function buildInterior(w, h, fillFn) {
  var t = new Uint8Array(w * h);
  var s = new Uint8Array(w * h);
  for (var y = 0; y < h; y++) for (var x = 0; x < w; x++) t[idxIn(w, x, y)] = T_FLOOR;
  for (var x2 = 0; x2 < w; x2++) { t[idxIn(w, x2, 0)] = T_IWALL; t[idxIn(w, x2, h - 1)] = T_IWALL; }
  for (var y2 = 0; y2 < h; y2++) { t[idxIn(w, 0, y2)] = T_IWALL; t[idxIn(w, w - 1, y2)] = T_IWALL; }
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
var ownHouseSpace = buildInterior(16, 16, function (t, w) {
  fillHouse(t, w);
  t[idxIn(w, HOUSE_LATCH.x, HOUSE_LATCH.y)] = T_SIGN;
});
ownHouseSpace.interactables = [
  { type: "wallSlot", x: 7, y: 0, prompt: "Place" },
  HOUSE_LATCH
];

// Lab -- 20x16. Three lookup stations (pushback.md's chalk-only recipe
// #3, no lime) plus a workbench visual. Output goes to session RAM only
// (v1.md) -- there is no SQLite in this preview and no reason to invent
// one for three toasts. Door is a walk-in trigger both ways, matching
// its outdoor half (atlas.md), not the demo house's Enter pattern.
var LAB_ENTRY = { x: 10, y: 14 };
var LAB_DOOR = { x: 10, y: 15 };
// item is the real catalog slug (schema.sql/seed.sql's items.slug), not
// a display string -- V2 lab placeables (below) sends it straight to
// POST /placed/<instance>, which validates against that same column.
// Nothing renders `item` as text (the toast uses `name`), so there was
// no display-string reason for these to ever have been spaced out.
var LAB_RECIPES = {
  copper: { name: "Copper + acetic acid → copper acetate", item: "copper_acetate_bottle" },
  iron:   { name: "Iron + tannin → iron-gall ink", item: "iron_gall_ink_page" },
  chalk:  { name: "Calcium carbonate, ground and washed → chalk", item: "chalk_block" }
};
// V2 stretch: how a placed slug renders in the wall-slot -- a flat
// color + short label, matching this game's existing no-sprite-assets
// look for anything that isn't the player/passerby figure. Keyed by the
// same real catalog slugs LAB_RECIPES.item now uses.
var PLACED_ITEM_INFO = {
  chalk_block:            { color: "#e8e2d0", label: "chalk" },
  iron_gall_ink_page:     { color: "#2a2320", label: "ink" },
  copper_acetate_bottle:  { color: "#4a8a6a", label: "acetate" }
};
var labSpace = buildInterior(20, 16, function (t, w) {
  t[idxIn(w, LAB_DOOR.x, LAB_DOOR.y)] = T_IDOOR;
  t[idxIn(w, 8, 1)]  = T_WORKBENCH;
  t[idxIn(w, 9, 1)]  = T_WORKBENCH;
  t[idxIn(w, 10, 1)] = T_WORKBENCH;
  t[idxIn(w, 11, 1)] = T_WORKBENCH;
  t[idxIn(w, 5, 7)]  = T_STATION_COPPER;
  t[idxIn(w, 10, 7)] = T_STATION_IRON;
  t[idxIn(w, 14, 7)] = T_STATION_CHALK;
});
labSpace.interactables = [
  { type: "labStation", recipe: "copper", x: 5, y: 7, prompt: "Look" },
  { type: "labStation", recipe: "iron", x: 10, y: 7, prompt: "Look" },
  { type: "labStation", recipe: "chalk", x: 14, y: 7, prompt: "Look" }
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

// Pickups. A guest's are session-only (v1.md: RAM, vanish on refresh --
// "Do not invent guest persistence"). A bound player's are also saved by the
// daemon (GROK_V6_ANSWER.md, bound inventory) and restored on load, so
// `inventory` and `lastPickedItem` survive a refresh or a daemon restart.
// V2: lastPickedItem is what the wall-slot places -- simplest possible
// "which item" choice given there's no inventory menu to pick from; the most
// recent lookup is what a player would expect to be holding.
var inventory = {};
var lastPickedItem = null;

