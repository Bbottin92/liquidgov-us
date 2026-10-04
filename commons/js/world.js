"use strict";

/* ---------------------------------------------------------------------
 * The Commons -- P3: fill. All 12 regions are dressed now.
 * P1's spine and P2's Lab Yard / demo house / lab interior stay as
 * shipped. New this round: the six quiet regions -- North Woods, Lookout,
 * Orchard, Workshop Lane, Garden, Quarry Ridge -- each with the one set
 * piece atlas.md names for it. Same tileset throughout; no new doors, no
 * new interiors, no new interact behavior beyond two toasts (Workshop's
 * shutter, Quarry Ridge's sign). This is the full v1.md definition of
 * done for the outdoor world.
 * See BUILD_ORDER.md (P3), atlas.md, art.md.
 * ------------------------------------------------------------------- */

// ---- World constants (locked in atlas.md) -----------------------------
var TILE = 16;
var REGION_W = 24, REGION_H = 20;   // tiles per region
// BUILD property phase 4 (CLAUDE_PROPERTY_PHASE4.md): 4 -> 5 rows for the Dunes. The grid math below
// (regionAABB/regionAt/regionsIntersectingCamera in net.js) is parameterized off these two constants and
// REGIONS, not hardcoded to 12 cells anywhere -- checked directly before this change, not assumed.
var GRID_COLS = 4, GRID_ROWS = 5;   // supermap layout
var WORLD_W = GRID_COLS * REGION_W; // 96
var WORLD_H = GRID_ROWS * REGION_H; // 100
var WORLD_PW = WORLD_W * TILE;      // 1536
var WORLD_PH = WORLD_H * TILE;      // 1600

// Region table: name, grid col/row -- matches atlas.md exactly.
var REGIONS = [
  { name: "North Woods",    col: 0, row: 0 },
  { name: "Lookout",        col: 1, row: 0 },
  { name: "Orchard",        col: 2, row: 0 },
  { name: "the Grove",      col: 3, row: 0 },
  { name: "Workshop Lane",  col: 0, row: 1 },
  { name: "the Square",     col: 1, row: 1 },
  { name: "Hall Lawn",      col: 2, row: 1 },
  { name: "the Meadow",     col: 3, row: 1 },
  { name: "Lab Yard",       col: 0, row: 2 },
  { name: "Homestead Row",  col: 1, row: 2 },
  { name: "Garden",         col: 2, row: 2 },
  { name: "the Hedge",      col: 3, row: 2 },
  { name: "Quarry Ridge",   col: 0, row: 3 },
  { name: "Creek",          col: 1, row: 3 },
  { name: "Shore",          col: 2, row: 3 },
  { name: "the Point",      col: 3, row: 3 },
  // The rest of this row is the sea (see bakeTileIds). It is not a region.
  { name: "the Dunes",      col: 0, row: 4 }
];

function regionAABB(r) {
  var x0 = r.col * REGION_W, y0 = r.row * REGION_H;
  return { x0: x0, y0: y0, x1: x0 + REGION_W - 1, y1: y0 + REGION_H - 1 };
}

function findRegion(name) {
  for (var i = 0; i < REGIONS.length; i++) if (REGIONS[i].name === name) return REGIONS[i];
  return null;
}

// Region a tile belongs to, from grid math only (tilemap.md: never stored
// per tile). Used for the passerby's hide-if-a-real-guest-is-here rule.
function regionAt(tx, ty) {
  var col = Math.floor(tx / REGION_W), row = Math.floor(ty / REGION_H);
  for (var i = 0; i < REGIONS.length; i++) {
    if (REGIONS[i].col === col && REGIONS[i].row === row) return REGIONS[i];
  }
  return null;
}

// The six Homestead lot doors (lots 2-7), in slot order -- filled in by
// dressRegions(), read by the lot poll and drawSpace(). Declared up here,
// not beside dressRegions(): the world is built during script load, before
// a `var` further down would have been assigned.
// The six friend-lot doors used to live here. BUILD property coherence moved that list to the
// south gate, so this stays an empty array: the outdoor draw still walks it, and a name in it
// would be a door again.
var FRIEND_LOT_DOORS = [];
var WHERE_GATE = null;

// ---- Tile ids -----------------------------------------------------------
var T_GRASS_A = 0, T_GRASS_B = 1, T_PATH = 2, T_WATER = 3, T_DIRT = 4,
    T_WALL = 5, T_DOOR_DEMO = 6, T_DOOR_BOARDED = 7, T_HEDGE = 8,
    T_HALL_DOOR = 9, T_NOTICE_POST = 10, T_BENCH = 11, T_BRIDGE = 12,
    // P2: Lab Yard exterior. The Lab door is deliberately NOT in the same
    // family as demo/boarded/Hall -- atlas.md calls it a "walk-in
    // trigger," not an Enter-to-confirm door, so it stays non-solid.
    T_LAB_DOOR = 13, T_SIGN = 14,
    // P2: interior-only tiles (demo house + lab), never placed outdoors.
    T_FLOOR = 15, T_IWALL = 16, T_TABLE = 17, T_CHAIR = 18, T_RUG = 19,
    T_WALLSLOT = 20, T_WORKBENCH = 21,
    T_STATION_COPPER = 22, T_STATION_IRON = 23, T_STATION_CHALK = 24,
    T_IDOOR = 25, // interior side of a walk-in doorway (entrance/exit)
    // P3: the six quiet regions. art.md's own drawable list names these as
    // distinct props (fence/railing, flower bed, boarded-adit face) rather
    // than reuses of an unrelated tile -- reuse happens *within* this set
    // (the same T_TREE dresses North Woods and Orchard, T_BENCH is every
    // sit-log/sit-rock/sitting-wall) not by collapsing them into one id.
    T_TREE = 26, T_RAILING = 27, T_PLATEAU = 28, T_FLOWERBED = 29,
    T_LOWWALL = 30, T_CRATE = 31, T_ADIT = 32,
    // V4 (GROK_V4_ANSWER.md): the one Homestead door that unboards for
    // the bound self. Its own tile, not a reuse of T_DOOR_DEMO -- a
    // guest and a bound player see the same wall from a different door.
    T_DOOR_HOUSE = 33,
    // BUILD mine A (GROK_MINE_ANSWER.md): the adit's one door tile, and the
    // lamp inside. Same flat-colour tileset as everything else -- two more
    // ids, not a new sheet.
    T_MINE_DOOR = 34, T_LAMP = 35,
    // BUILD lab A (GROK_OBJECTS_ANSWER.md): five more lookup stations.
    // Cells 36-47 were free; 48-51 stay grass variants. No lime.
    T_STATION_VINEGAR = 36, T_STATION_TANNIN = 37, T_STATION_LAMPBLACK = 38,
    T_STATION_ALUM = 39, T_STATION_GUM = 40,
    // BUILD property phase 4: the Dunes' ground. Cells 36-47 were free; this is the next one.
    T_SAND = 41,
    // Short buildings. A roof, a wall with a window, a counter and a shelf.
    // Drawn in the tileset the same way every other id is. Not a new sheet
    // format: the next free cells, under the grass variants at 48.
    T_ROOF = 42, T_WINDOW = 43, T_COUNTER = 44, T_SHELF = 45;

// Palette, from art.md -- outdoor stays grass/dirt/water, never parchment.
// Brass stays "sparingly" for the Hall door only, per art.md -- the
// notice-post gets its own dark ink-brown, not a fountain, not brass.
// Interior walls run warm/paper (art.md) so the wall-slot reads as the
// civic site hung in a house, not more outdoor palette indoors.
var COLOR = {};
COLOR[T_GRASS_A]      = "#4a7c59";
COLOR[T_GRASS_B]      = "#3d6b4f";
COLOR[T_PATH]         = "#c4b49a";
COLOR[T_WATER]        = "#3d5a73";
COLOR[T_DIRT]         = "#8a8378";
COLOR[T_WALL]         = "#6b4a32";
COLOR[T_DOOR_DEMO]    = "#8a6a46";
COLOR[T_DOOR_BOARDED] = "#5d564c";
COLOR[T_HEDGE]        = "#2f4a37";
COLOR[T_HALL_DOOR]    = "#c4a574";
COLOR[T_NOTICE_POST]  = "#4a3a28";
COLOR[T_BENCH]        = "#6b4a32";
COLOR[T_BRIDGE]       = "#7a5a3a";
COLOR[T_LAB_DOOR]     = "#9a7a4a";
COLOR[T_SIGN]         = "#3a3226";
COLOR[T_FLOOR]        = "#b08a5a";
COLOR[T_IWALL]        = "#f7f1e4";
COLOR[T_TABLE]        = "#6b4a32";
COLOR[T_CHAIR]        = "#8a6a46";
COLOR[T_RUG]          = "#7a4a4a";
COLOR[T_WALLSLOT]     = "#d8cdb0";
COLOR[T_WORKBENCH]    = "#6b4a32";
COLOR[T_STATION_COPPER] = "#4a8a6a";
COLOR[T_STATION_IRON]   = "#2a2a3a";
COLOR[T_STATION_CHALK]  = "#e8e0d0";
COLOR[T_STATION_VINEGAR]   = "#7a5a32";
COLOR[T_STATION_TANNIN]    = "#4a3020";
COLOR[T_STATION_LAMPBLACK] = "#2a2824";
COLOR[T_STATION_ALUM]      = "#c5c8c2";
COLOR[T_STATION_GUM]       = "#a07a48";
COLOR[T_IDOOR]        = "#b08a5a";
COLOR[T_TREE]         = "#2f5a3f";
COLOR[T_RAILING]      = "#9a9488";
COLOR[T_PLATEAU]      = "#5a8f68";
COLOR[T_FLOWERBED]    = "#8a5a6a";
COLOR[T_LOWWALL]      = "#7a7468";
COLOR[T_CRATE]        = "#8a6a46";
COLOR[T_ADIT]         = "#5d564c";
COLOR[T_DOOR_HOUSE]   = "#5a7a8a";
COLOR[T_MINE_DOOR]    = "#25211c";
COLOR[T_LAMP]         = "#e6b957";
COLOR[T_SAND]         = "#d8bd84";
COLOR[T_ROOF]         = "#8b3d32";
COLOR[T_WINDOW]       = "#6b4a32";
COLOR[T_COUNTER]      = "#6b4a32";
COLOR[T_SHELF]        = "#6b4a32";
var INK = "#1a1814";

// ---- Tile painter (BUILD tile dither) --------------------------------------
// The bake used to be one fillRect per tile. It is still one bake, one blit,
// the same COLOR palette and the same 16x16 ids -- this only adds a little
// detail inside a tile so flat fields stop reading as a spreadsheet:
//   * grass A/B: a sparse 2x2px checker of slightly darker/lighter dots,
//     offset per tile by a fixed hash so the lattice doesn't tile visibly
//   * water: a darker 2px lip on any edge that meets a non-water tile
//   * bench / sit-rock / sit-log (T_BENCH): two lines of wood grain and a
//     lighter top edge
// Every shade is derived from the tile's own COLOR (mixed toward black or
// white), so there is no new palette entry. No new asset, no new draw path.
function shadeHex(hex, f) {
  var n = parseInt(hex.slice(1), 16);
  var r = (n >> 16) & 255, g = (n >> 8) & 255, b = n & 255;
  var t = f < 0 ? 0 : 255, a = Math.abs(f);
  r = Math.round(r + (t - r) * a); g = Math.round(g + (t - g) * a); b = Math.round(b + (t - b) * a);
  return "rgb(" + r + "," + g + "," + b + ")";
}

// ---- Tileset PNG (BUILD tileset PNG + player sheet) --------------------------
// tiles.png is a 16x16 grid, row-major, cell N = tile id N (the T_* enum),
// plus grass variants in cells 48..51. Persistence/TheCommons/art/make_art.py
// generates it deterministically from this file's own COLOR table. The bake
// stamps from it ONCE per space, exactly as it stamped paintTile's fillRects --
// the camera still blits the baked crop. The sheet arrives asynchronously, so
// the world first bakes with paintTile (the fallback) and is re-baked from the
// sheet the moment it loads; if it never loads (offline, 404, wrong size) the
// paintTile look simply stays. No runtime packer, no per-frame sheet blits.
var TILE_SHEET_URL = "tiles.png?v=3", SPRITE_SHEET_URL = "sprites.png?v=1";
var TILE_SHEET_W = 256, TILE_SHEET_H = 64;
var tileSheet = null;                  // an <img>, set only once it loaded at the expected size
var GRASS_VARIANT_CELLS = {};          // grass id -> [phase1 cell, phase2 cell]
GRASS_VARIANT_CELLS[T_GRASS_A] = [48, 49];
GRASS_VARIANT_CELLS[T_GRASS_B] = [50, 51];
var spaceRebakers = [];                // one closure per baked space (outdoor + each interior)

function drawTile(c, id, tx, ty, nb) {
  if (!tileSheet) { paintTile(c, id, tx, ty, nb); return; }
  var cell = id;
  var gv = GRASS_VARIANT_CELLS[id];
  if (gv) {
    var phase = (tx * 7 + ty * 13) % 3;      // same hash paintTile's dots used
    if (phase > 0) cell = gv[phase - 1];
  }
  c.drawImage(tileSheet, (cell % 16) * TILE, Math.floor(cell / 16) * TILE, TILE, TILE, tx * TILE, ty * TILE, TILE, TILE);
  if (id === T_WATER) {
    // The lip depends on the neighbours, so it stays a small overlay rather
    // than 16 edge variants in the sheet.
    var lip = shadeHex(COLOR[T_WATER], -0.28);
    var px = tx * TILE, py = ty * TILE;
    c.fillStyle = lip;
    if (nb(0, -1) !== T_WATER && nb(0, -1) !== -1) c.fillRect(px, py, TILE, 2);
    if (nb(0, 1) !== T_WATER && nb(0, 1) !== -1) c.fillRect(px, py + TILE - 2, TILE, 2);
    if (nb(-1, 0) !== T_WATER && nb(-1, 0) !== -1) c.fillRect(px, py, 2, TILE);
    if (nb(1, 0) !== T_WATER && nb(1, 0) !== -1) c.fillRect(px + TILE - 2, py, 2, TILE);
  }
}

// nb(dx, dy) returns the tile id next to (tx, ty), or -1 off the map.
function paintTile(c, id, tx, ty, nb) {
  var px = tx * TILE, py = ty * TILE, base = COLOR[id];
  c.fillStyle = base;
  c.fillRect(px, py, TILE, TILE);

  if (id === T_GRASS_A || id === T_GRASS_B || id === T_SAND) {
    // two dot lattices on a 4px pitch, checkered; which phase a tile gets
    // comes from a fixed hash of its position, and A/B use opposite tones.
    // BUILD property phase 4: T_SAND reuses this exact technique (same speckle, sand's own colour) --
    // no new art, no new drawing path, per CLAUDE_PROPERTY_PHASE4.md decision 3.
    var phase = (tx * 7 + ty * 13) % 3;
    var dark = shadeHex(base, -0.10), light = shadeHex(base, 0.07);
    for (var gy = 0; gy < 4; gy++) {
      for (var gx = 0; gx < 4; gx++) {
        if (((gx + gy + phase) & 1) !== 0) continue;
        c.fillStyle = ((gx * 3 + gy + phase) % 4 === 0) ? light : dark;
        c.fillRect(px + gx * 4 + (phase & 1), py + gy * 4 + ((phase >> 1) & 1), 2, 2);
      }
    }
  } else if (id === T_WATER) {
    var lip = shadeHex(base, -0.28);
    c.fillStyle = lip;
    if (nb(0, -1) !== T_WATER && nb(0, -1) !== -1) c.fillRect(px, py, TILE, 2);
    if (nb(0, 1) !== T_WATER && nb(0, 1) !== -1) c.fillRect(px, py + TILE - 2, TILE, 2);
    if (nb(-1, 0) !== T_WATER && nb(-1, 0) !== -1) c.fillRect(px, py, 2, TILE);
    if (nb(1, 0) !== T_WATER && nb(1, 0) !== -1) c.fillRect(px + TILE - 2, py, 2, TILE);
  } else if (id === T_BENCH) {
    c.fillStyle = shadeHex(base, 0.14);
    c.fillRect(px, py, TILE, 1);                       // lit top edge
    c.fillStyle = shadeHex(base, -0.22);
    c.fillRect(px + 1, py + 5, 9, 1);                  // grain, broken so it isn't a ruled line
    c.fillRect(px + 12, py + 5, 3, 1);
    c.fillRect(px + 1, py + 10, 4, 1);
    c.fillRect(px + 7, py + 10, 8, 1);
    c.fillRect(px, py + TILE - 1, TILE, 1);            // shaded underside
  } else if (id === T_ROOF) {
    c.fillStyle = shadeHex(base, -0.28);
    c.fillRect(px, py + 12, TILE, 4);
    c.fillStyle = shadeHex(base, 0.16);
    c.fillRect(px, py + 2, TILE, 2);
    c.fillStyle = shadeHex(base, -0.18);
    c.fillRect(px, py + 7, TILE, 1);
  } else if (id === T_WINDOW) {
    c.fillStyle = shadeHex(base, -0.2);
    c.fillRect(px, py, TILE, TILE);
    c.fillStyle = "#9eb4c0";
    c.fillRect(px + 4, py + 4, 8, 6);
    c.fillStyle = shadeHex(base, -0.45);
    c.fillRect(px + 7, py + 4, 1, 6);
    c.fillRect(px + 4, py + 6, 8, 1);
  } else if (id === T_COUNTER || id === T_SHELF) {
    c.fillStyle = shadeHex(base, id === T_SHELF ? -0.15 : 0.05);
    c.fillRect(px, py + (id === T_SHELF ? 6 : 8), TILE, id === T_SHELF ? 3 : 6);
    c.fillStyle = "#c45a3a";
    c.fillRect(px + 2, py + 3, 3, 3);
    c.fillStyle = "#d8c070";
    c.fillRect(px + 7, py + 2, 3, 4);
    c.fillStyle = "#3d6a4a";
    c.fillRect(px + 11, py + 3, 3, 3);
  } else if (id === T_TABLE) {
    c.fillStyle = "#d8c070";
    c.fillRect(px + 6, py + 4, 4, 3);
  }
}

// collision.md: doors are solid -- "stand next to them, press Enter," not
// walk-through. A boarded door reads as a wall with a prompt, same as any
// other closed door; Hall's confirm() is the interaction, not stepping in.
// The Lab door and any interior doorway tile are the one deliberate
// exception (walk-in trigger, per atlas.md) and stay off this table.
var SOLID_TILE = {};
SOLID_TILE[T_WATER] = 1;
SOLID_TILE[T_WALL] = 1;
SOLID_TILE[T_HEDGE] = 1;
SOLID_TILE[T_NOTICE_POST] = 1;
SOLID_TILE[T_BENCH] = 1;
SOLID_TILE[T_DOOR_DEMO] = 1;
SOLID_TILE[T_DOOR_BOARDED] = 1;
SOLID_TILE[T_DOOR_HOUSE] = 1;
SOLID_TILE[T_HALL_DOOR] = 1;
SOLID_TILE[T_SIGN] = 1;
SOLID_TILE[T_IWALL] = 1;
SOLID_TILE[T_TABLE] = 1;
SOLID_TILE[T_CHAIR] = 1;
SOLID_TILE[T_WALLSLOT] = 1;
SOLID_TILE[T_WORKBENCH] = 1;
SOLID_TILE[T_STATION_COPPER] = 1;
SOLID_TILE[T_STATION_IRON] = 1;
SOLID_TILE[T_STATION_CHALK] = 1;
SOLID_TILE[T_STATION_VINEGAR] = 1;
SOLID_TILE[T_STATION_TANNIN] = 1;
SOLID_TILE[T_STATION_LAMPBLACK] = 1;
SOLID_TILE[T_STATION_ALUM] = 1;
SOLID_TILE[T_STATION_GUM] = 1;
// P3: T_PLATEAU is walkable ground (raised visual layer only, per
// atlas.md), not a wall -- everything else here is a real obstacle.
SOLID_TILE[T_TREE] = 1;
SOLID_TILE[T_RAILING] = 1;
SOLID_TILE[T_FLOWERBED] = 1;
SOLID_TILE[T_LOWWALL] = 1;
SOLID_TILE[T_CRATE] = 1;
SOLID_TILE[T_ADIT] = 1;
SOLID_TILE[T_MINE_DOOR] = 1;
SOLID_TILE[T_LAMP] = 1;
SOLID_TILE[T_ROOF] = 1;
SOLID_TILE[T_WINDOW] = 1;
SOLID_TILE[T_COUNTER] = 1;
SOLID_TILE[T_SHELF] = 1;

// Tall props redraw in Y-order with actors so a 24px sprite can pass
// behind a tree/hedge/wall instead of always stamping on top of the bake.
var TALL_TILE = {};
TALL_TILE[T_TREE] = 1;
TALL_TILE[T_HEDGE] = 1;
TALL_TILE[T_ADIT] = 1;
TALL_TILE[T_NOTICE_POST] = 1;
TALL_TILE[T_SIGN] = 1;
TALL_TILE[T_WALL] = 1;
TALL_TILE[T_IWALL] = 1;
TALL_TILE[T_ROOF] = 1;
TALL_TILE[T_WINDOW] = 1;

// ---- Scatter: the extra props (BUILD objects B) ------------------------------
// Data, not code: [x, y] tile coords per region, drawn with tiles that already exist (tree, sit-log,
// crate, flower bed, low wall, hedge), so no new art and no new collision rule. scatterRegions() (called
// last in dressRegions) refuses any spot that is not plain ground, so a wrong coordinate can never
// overwrite a door, the path or a wall: it is listed in SCATTER_REFUSED instead, and
// tests/world_scatter_unit.js fails on that. The same test enforces what the coordinates below keep clear:
// the path spine and the bridge jog, every region's edge tiles (the seams between regions), the strip
// where a region's name is stamped, one tile around every door / sign / interactable, the spawn, and the
// passerby's waypoints; and it flood-fills the result so no walkable tile is ever sealed off.
// Declared up here, not beside dressRegions(): the world is built during script load (see FRIEND_LOT_DOORS).
var SCATTER = [
  // Generated by commons-daemon/tools/scatter_author.js, seed 20260927. Not hand-edited --
  // `node commons-daemon/tools/scatter_author.js --check` fails if this drifts from a fresh run.
  // Change a rule in that file, or the seed, to move a prop.
  { region: "North Woods", tile: T_TREE, at: [[1,9],[1,11],[9,4],[18,18],[20,16],[22,9]] },
  { region: "Lookout", tile: T_TREE, at: [[26,3],[29,3]] },
  { region: "Workshop Lane", tile: T_CRATE, at: [[17,22]] },
  { region: "Workshop Lane", tile: T_TREE, at: [[20,22]] },
  { region: "Lab Yard", tile: T_CRATE, at: [[17,55]] },
  { region: "Lab Yard", tile: T_BENCH, at: [[15,56]] },
  { region: "Lab Yard", tile: T_TREE, at: [[3,49],[16,41]] },
  { region: "Homestead Row", tile: T_TREE, at: [[26,41],[27,58],[30,50],[31,56],[39,54],[46,50],[46,55]] },
  { region: "Homestead Row", tile: T_BENCH, at: [[45,47]] },
  { region: "Garden", tile: T_FLOWERBED, at: [[51,43],[52,50],[54,50],[57,43],[59,43],[64,43],[68,43]] },
  { region: "Garden", tile: T_TREE, at: [[53,44]] },
  { region: "Garden", tile: T_BENCH, at: [[61,54]] },
  { region: "Quarry Ridge", tile: T_LOWWALL, at: [[6,64],[12,77],[20,77],[21,64]] },
  { region: "Quarry Ridge", tile: T_CRATE, at: [[6,71],[22,61]] },
  { region: "Quarry Ridge", tile: T_TREE, at: [[4,66],[15,62]] },
  { region: "Creek", tile: T_TREE, at: [[27,63],[29,63],[32,63],[38,63],[42,63],[45,63]] },
  { region: "Creek", tile: T_BENCH, at: [[40,77]] },
  { region: "Shore", tile: T_TREE, at: [[51,63],[54,63],[58,63],[68,63],[70,63]] }
];
var SCATTER_PLACED = 0, SCATTER_REFUSED = [];   // read by tests/world_scatter_unit.js

// ---- Build the tile + collision arrays (once) ---------------------------
var tiles = new Uint8Array(WORLD_W * WORLD_H);
var solid = new Uint8Array(WORLD_W * WORLD_H);

function idx(x, y) { return y * WORLD_W + x; }

// Interactables: filled in by dressRegions() below with real tile coords.
var interactables = [];
// The Lab door's outdoor tile coords -- filled in by dressRegions(). Read
// by frame()'s outdoor branch since it's a walk-in trigger, not something
// in `interactables` (which is the Enter-to-confirm family).
var LAB_DOOR_TILE = null;
// The adit door. Declared here (not beside the interior) for the same reason as
// FRIEND_LOT_DOORS: the outdoor world is built during script load.
var MINE_DOOR_TILE = null;
// The Workshop Lane door (BUILD workshop A). Same reason.
var WORKSHOP_DOOR_TILE = null;

(function bakeTileIds() {
  var creek = regionAABB(findRegion("Creek"));
  var shore = regionAABB(findRegion("Shore"));
  var quarry = regionAABB(findRegion("Quarry Ridge"));
  var dunes = regionAABB(findRegion("the Dunes"));

  for (var y = 0; y < WORLD_H; y++) {
    for (var x = 0; x < WORLD_W; x++) {
      var t;
      if (x >= quarry.x0 && x <= quarry.x1 && y >= quarry.y0 && y <= quarry.y1) {
        t = T_DIRT;
      } else if (x >= dunes.x0 && x <= dunes.x1 && y >= dunes.y0 && y <= dunes.y1) {
        // BUILD property phase 4: the Dunes' own ground.
        t = T_SAND;
      } else if (y >= dunes.y0 && x > dunes.x1) {
        // The south of the map used to be a 48 by 20 field of the same grass as the Square, with
        // no name and no path. A Link to the Past, Link's Awakening, Stardew Valley, Pokémon Red,
        // EarthBound, Super Mario World, Hollow Knight, Animal Crossing, Undertale, and Breath of
        // the Wild all stop the walkable world at a terrain edge (water, a cliff, a wall, or the
        // path simply ending). This water is that edge. The Dunes stay sand and meet it on the east.
        t = T_WATER;
      } else {
        // Was a 1-tile checker of the two greens. At 16px that is a spreadsheet,
        // and it sat under the lanes so the road had no edge. One field colour;
        // the darker green is only the shoulder (vergePaths).
        t = T_GRASS_A;
      }
      tiles[idx(x, y)] = t;
    }
  }

  // Creek + Shore: a water ribbon with grass/sand banks, not a flooded
  // region (atlas.md: "one water ribbon + one bridge; rest open bank").
  // P0 painted the whole 24×20 as water; this is the remaining map fill.
  var waterY0 = 66, waterY1 = 72;
  for (var wy = waterY0; wy <= waterY1; wy++) {
    for (var wx = creek.x0; wx <= shore.x1; wx++) {
      tiles[idx(wx, wy)] = T_WATER;
    }
  }
  for (var sy = 73; sy <= shore.y1; sy++) {
    for (var sx = shore.x0; sx <= shore.x1; sx++) {
      tiles[idx(sx, sy)] = T_DIRT;
    }
  }
  // The Point is the new east column's coast. The creek's water used to stop
  // at the old map edge and turn into grass. It keeps going, then the same
  // dirt bank the Shore already has, so the coast is one shore and not a seam.
  var point = regionAABB(findRegion("the Point"));
  for (var py = waterY0; py <= waterY1; py++) {
    for (var px = point.x0; px < WORLD_W; px++) tiles[idx(px, py)] = T_WATER;
  }
  for (var pdy = waterY1 + 1; pdy <= point.y1; pdy++) {
    for (var pdx = point.x0; pdx < WORLD_W; pdx++) tiles[idx(pdx, pdy)] = T_DIRT;
  }

  // Path ribbon, terrain only: the Square -> Homestead Row -> Creek -> Shore.
  // Vertical spine at x=34..35 threads all three (they share grid col 1),
  // then a jog at y=69..70 carries it east into Shore.
  // BUILD property phase 4: this used to run to WORLD_H - 1 (the world's own bottom edge), which was
  // exactly Shore's own south edge until this build grew the grid by a row -- pinned to shore.y1
  // explicitly now, so the spine still ends at Shore and doesn't drift into the new, intentionally
  // undressed column 1-2 of row 4 underneath it.
  var px0 = 34, px1 = 35;
  for (var vy = 20; vy <= shore.y1; vy++) {
    tiles[idx(px0, vy)] = T_PATH;
    tiles[idx(px1, vy)] = T_PATH;
  }
  var jy0 = 69, jy1 = 70;
  for (var jx = px0; jx <= 65; jx++) {
    tiles[idx(jx, jy0)] = T_PATH;
    tiles[idx(jx, jy1)] = T_PATH;
  }

  // Recolor the path where it crosses water (Creek/Shore) into a bridge --
  // still just a walkable tile id, same collision rule as path. Purely
  // cosmetic so P0's "path paves the water" behavior isn't a surprise.
  for (var i = 0; i < tiles.length; i++) {
    var x2 = i % WORLD_W, y2 = (i - x2) / WORLD_W;
    var inCreek = x2 >= creek.x0 && x2 <= creek.x1 && y2 >= creek.y0 && y2 <= creek.y1;
    var inShore = x2 >= shore.x0 && x2 <= shore.x1 && y2 >= shore.y0 && y2 <= shore.y1;
    if ((inCreek || inShore) && tiles[i] === T_PATH) tiles[i] = T_BRIDGE;
  }

  // BUILD property phase 4: a path connecting Quarry Ridge into the Dunes, laid before dressRegions()
  // so a scatter prop can never land on it. Brandon's own standing rule, already applied to every other
  // region this session: nothing gets built without a way to reach it. x=14 is clear of Quarry Ridge's
  // own scatter (T_LOWWALL/T_CRATE sit at x=2,5,12,17,18,21) and south of its sign (y=68).
  var dunesPathX = 14;
  for (var dpy = quarry.y1 - 4; dpy <= dunes.y0 + 4; dpy++) {
    tiles[idx(dunesPathX, dpy)] = T_PATH;
  }

  dressRegions();
  addStudioGatherMarkers();
  // After the building branches (those stay as routed) and after every prop.
  // A lane here only takes grass or dirt, so it cannot erase a door, a crate,
  // or the bridge recolour.
  layApproachPaths();
  vergePaths();

  // Collision, computed last from the final tile ids.
  for (var i2 = 0; i2 < tiles.length; i2++) {
    solid[i2] = SOLID_TILE[tiles[i2]] ? 1 : 0;
  }
})();

// P1 + P2 + P3: every region is dressed now (set piece + collision land
// together, per region). Same tileset throughout -- T_TREE/T_BENCH/etc.
// reappear across regions; only the arrangement differs (art.md).
// A building is three tiles tall, like a Pokemon mart: roof, windows, door.
// It is wider than it is tall, and it is not a filled square. The door is
// the south edge so the path in front of it is where you stand.
function placeFront(doorX, doorY, half, doorTile, shop) {
  var dx;
  for (dx = -half; dx <= half; dx++) {
    tiles[idx(doorX + dx, doorY - 2)] = T_ROOF;
    tiles[idx(doorX + dx, doorY - 1)] = T_WINDOW;
    if (dx !== 0) tiles[idx(doorX + dx, doorY)] = T_WINDOW;
  }
  tiles[idx(doorX, doorY)] = doorTile;
  if (shop) interactables.push({ type: "shopDoor", x: doorX, y: doorY, prompt: "Enter", shop: shop });
}
function placeFrontWest(doorX, doorY, extra, doorTile) {
  var x;
  for (x = doorX - extra; x <= doorX; x++) {
    tiles[idx(x, doorY - 2)] = T_ROOF;
    tiles[idx(x, doorY - 1)] = T_WINDOW;
    if (x !== doorX) tiles[idx(x, doorY)] = T_WINDOW;
  }
  tiles[idx(doorX, doorY)] = doorTile;
}

function dressRegions() {
  var square = regionAABB(findRegion("the Square"));
  var hall = regionAABB(findRegion("Hall Lawn"));
  var homestead = regionAABB(findRegion("Homestead Row"));
  var creek = regionAABB(findRegion("Creek"));
  var shore = regionAABB(findRegion("Shore"));

  // the Square: a civic notice-post (not a fountain), near center, plus
  // two benches. Offset 3 tiles south of the exact center -- the player
  // also spawns at that exact center tile, and a solid prop sitting on
  // the spawn tile traps a fresh arrival (collision.md: every corner of
  // the player's own box was already inside the "solid" tile it spawned
  // on, so every candidate move was blocked -- found while testing P1).
  var sqCx = Math.floor((square.x0 + square.x1 + 1) / 2);
  var sqCy = Math.floor((square.y0 + square.y1 + 1) / 2) + 3;
  tiles[idx(sqCx, sqCy)] = T_NOTICE_POST;
  tiles[idx(square.x0 + 2, sqCy)] = T_BENCH;
  tiles[idx(square.x1 - 2, sqCy)] = T_BENCH;

  // Hall Lawn: the door, framed by two hedges. Interact opens /proposals.
  var hallCx = Math.floor((hall.x0 + hall.x1 + 1) / 2);
  var hallCy = Math.floor((hall.y0 + hall.y1 + 1) / 2);
  tiles[idx(hallCx, hallCy)] = T_HALL_DOOR;
  tiles[idx(hallCx - 2, hallCy)] = T_HEDGE;
  tiles[idx(hallCx + 2, hallCy)] = T_HEDGE;
  // Short roof beside the walk up to the door. The tile directly north of
  // the door is the path, so the roof does not sit on it.
  tiles[idx(hallCx - 2, hallCy - 2)] = T_ROOF;
  tiles[idx(hallCx - 1, hallCy - 2)] = T_ROOF;
  tiles[idx(hallCx + 1, hallCy - 2)] = T_ROOF;
  tiles[idx(hallCx + 2, hallCy - 2)] = T_ROOF;
  tiles[idx(hallCx - 1, hallCy - 1)] = T_WINDOW;
  tiles[idx(hallCx + 1, hallCy - 1)] = T_WINDOW;
  tiles[idx(hallCx - 1, hallCy)] = T_WINDOW;
  tiles[idx(hallCx + 1, hallCy)] = T_WINDOW;
  interactables.push({
    type: "hall", x: hallCx, y: hallCy,
    prompt: "Hall"
  });

  // Homestead Row is the first region south of the Square, on the road you are already walking.
  // The long wall used to be a row of doors. The doors left, and the wall stayed as a facade that
  // went nowhere. It is grass now. The gate sits on the west half of the spine (x=34). The east
  // half (x=35) stays path, so the road to Creek is not blocked by your own front door. The sign
  // is one tile east of that road, off the path.
  WHERE_GATE = { x: 34, y: homestead.y0 + 3 }; // (34, 43), inside Homestead, north of the lab junction
  tiles[idx(WHERE_GATE.x, WHERE_GATE.y)] = T_DOOR_DEMO;
  interactables.push({ type: "whereGate", x: WHERE_GATE.x, y: WHERE_GATE.y, prompt: "Where" });
  tiles[idx(WHERE_GATE.x + 2, WHERE_GATE.y)] = T_SIGN;
  interactables.push({
    type: "sign", x: WHERE_GATE.x + 2, y: WHERE_GATE.y, prompt: "Read",
    text: "Plots. The gate is on the road."
  });

  // Creek: a bench directly beside the bridge -- collision.md flagged the
  // previous spot as stranded out in flooded water, disconnected from the
  // one walkable line through the region. One tile east of the vertical
  // bridge column, still inside Creek.
  tiles[idx(35 + 1, creek.y0 + 5)] = T_BENCH;

  // Shore: a driftwood log right where the bridge's jog lands in Shore
  // (reuse the bench tile/color -- same "small sittable prop" language as
  // atlas.md, no new art needed), not off alone in open water.
  tiles[idx(60, 71)] = T_BENCH;

  // D: a short sand tongue off the Shore bridge (walkable dirt, not a new
  // tile), shells as a crate pile. No limestone. Connects to the bridge so
  // the beach is not a sealed pocket (world_scatter_unit flood-fill).
  tiles[idx(62, 71)] = T_DIRT;
  tiles[idx(63, 71)] = T_DIRT;
  tiles[idx(62, 72)] = T_DIRT;
  tiles[idx(63, 72)] = T_DIRT;
  tiles[idx(62, 73)] = T_DIRT;
  tiles[idx(63, 73)] = T_DIRT;
  tiles[idx(62, 73)] = T_CRATE;
  // BUILD studio 3: this tile stops being the old unlimited worldPickup and becomes a studio gather
  // spot (studio.md: "reuses those exact spots; no second marker on the same tile"). Same item slug,
  // now on the daemon's per-player 10-minute cooldown for a bound player; a guest still gets a local,
  // session-only version of the same cooldown (frozen lab table in CLAUDE_STUDIO_3.md).
  interactables.push({ type: "studioGather", spot: "shore_shells", x: 62, y: 73, prompt: "Look" });
  // Atlas Shore set piece: a short pier into the water, 2 tiles wide,
  // meeting the existing east-west bridge jog so it is not a sealed dock.
  for (var pierY = 67; pierY <= 72; pierY++) {
    tiles[idx(62, pierY)] = T_BRIDGE;
    tiles[idx(63, pierY)] = T_BRIDGE;
  }

  // Lab Yard (P2): the building exterior + a small sign. The door is a
  // walk-in trigger (atlas.md), not an Enter-to-confirm door like Hall or
  // the demo house -- it's deliberately not solid and not pushed into
  // `interactables`. LAB_DOOR_TILE is read by frame()'s outdoor branch to
  // detect the crossing.
  var lab = regionAABB(findRegion("Lab Yard"));
  var labDoorX = lab.x0 + 11, labDoorY = lab.y0 + 8; // (11,48)
  // The old lab was a filled square. It is a short front now: roof and
  // windows, the door at the east end so the path can arrive beside it.
  // The door itself is still the walk-in tile, painted after the path.
  tiles[idx(labDoorX + 3, labDoorY + 1)] = T_SIGN;
  LAB_DOOR_TILE = { x: labDoorX, y: labDoorY };

  // ---- P3: the six quiet regions. One sitting each (BUILD_ORDER.md), same
  // tileset, no doors, no new interiors -- Workshop Lane's shutter and
  // Quarry Ridge's sign are the only two interactables, and both just
  // toast; nothing here changes state or warps.

  // North Woods: tree line along the world's north edge (a hard wall,
  // matches the world boundary honestly instead of an invisible clamp),
  // scattered solitary trees, one cluster with a sit-log.
  var northWoods = regionAABB(findRegion("North Woods"));
  for (var nwx = northWoods.x0 + 1; nwx <= northWoods.x1 - 1; nwx++) {
    tiles[idx(nwx, northWoods.y0)] = T_TREE;
  }
  tiles[idx(5, 14)] = T_TREE;
  tiles[idx(17, 6)] = T_TREE;
  tiles[idx(6, 5)] = T_TREE;
  tiles[idx(10, 10)] = T_TREE;
  tiles[idx(11, 10)] = T_TREE;
  tiles[idx(10, 11)] = T_TREE;
  tiles[idx(11, 11)] = T_BENCH; // the sit-log
  // D: one oak with galls. A solitary tree already in this region, not the
  // sit-log cluster and not on the name stamp.
  // BUILD studio 3: same tile, now a studio gather spot (10-minute per-player cooldown), not the old
  // unlimited worldPickup.
  interactables.push({ type: "studioGather", spot: "north_woods_galls", x: 17, y: 6, prompt: "Look" });

  // Lookout: a raised plateau (walkable, +1 visual layer), a railing along
  // the south edge facing the Square (the sightline the P1 patch fixed --
  // Shore is off-camera from here, don't reintroduce that promise), one
  // bench, one tree just off the plateau.
  // Railing at y=16, not y=10 (parked after P3 review): the landscape crop
  // is only 10 tiles tall, so a camera centered on a player standing at
  // the railing shows roughly +-5 rows -- from y=10 that tops out at y=15,
  // never reaching the Square (which starts at y=20). From y=16 it reaches
  // to y=21, actually showing a sliver of it, matching the "facing south
  // over the Square" framing instead of just asserting it in a comment.
  for (var lkx = 30; lkx <= 41; lkx++) {
    for (var lky = 10; lky <= 15; lky++) tiles[idx(lkx, lky)] = T_PLATEAU;
    tiles[idx(lkx, 16)] = T_RAILING;
  }
  tiles[idx(35, 14)] = T_BENCH;
  tiles[idx(28, 13)] = T_TREE;

  // Orchard: 4-6 decorative fruit trees in loose rows, wide walk lanes
  // between them -- no farming, no harvest interaction, just trees.
  var orchardTrees = [[54, 8], [58, 8], [62, 8], [54, 12], [58, 12], [62, 12]];
  for (var ot = 0; ot < orchardTrees.length; ot++) {
    tiles[idx(orchardTrees[ot][0], orchardTrees[ot][1])] = T_TREE;
  }
  // D: fallen apples on the grass between rows, not a harvest on the trees
  // (atlas.md: no farming). Flowerbed as a coloured pile, existing tile.
  // BUILD studio 3: the same tile becomes the Orchard keg -- studio.md cut homemade vinegar from
  // apples in favor of a keg with a 1-day cooldown that yields vinegar directly. No second marker;
  // cider_apple is no longer granted from here (people who already hold one keep it and can still
  // place it -- the item row itself is untouched).
  tiles[idx(56, 10)] = T_FLOWERBED;
  interactables.push({ type: "studioGather", spot: "orchard_keg", x: 56, y: 10, prompt: "Look" });

  // Workshop Lane: the facade + a bench + light street scatter. The shutter
  // was a closed board through P3-V6 ("no interior without Brandon reopening
  // the pin"); he reopened it 2026-09-20 (BUILD workshop, option A: thin,
  // like the Hall). The same tile is now a real door into instance:workshop
  // -- a plain room with a bench, no crafting and nothing to pick up.
  var wsFacadeX0 = 8, wsFacadeY1 = 27;
  // Three tiles tall, not a block. Door stays (11, 27), which is the tile
  // the workshop test already names.
  placeFront(11, wsFacadeY1, 2, T_DOOR_DEMO, null);
  WORKSHOP_DOOR_TILE = { x: 11, y: wsFacadeY1 };
  interactables.push({ type: "workshopDoor", x: 11, y: wsFacadeY1, prompt: "Enter" });
  tiles[idx(9, 29)] = T_BENCH;
  tiles[idx(16, 28)] = T_CRATE;
  tiles[idx(17, 30)] = T_CRATE;
  // BUILD studio 3: the two crates were purely decorative until now. CLAUDE_STUDIO_3.md's frozen map
  // table turns them into the iron and copper gather spots -- no new tile, no new art, no second marker.
  interactables.push({ type: "studioGather", spot: "workshop_iron", x: 16, y: 28, prompt: "Look" });
  interactables.push({ type: "studioGather", spot: "workshop_copper", x: 17, y: 30, prompt: "Look" });

  // Garden: flower-bed cluster + a low sitting wall, Homestead's front
  // yard -- calmer than Orchard, still just two props.
  var garden = regionAABB(findRegion("Garden"));
  for (var gfx = garden.x0 + 8; gfx <= garden.x0 + 10; gfx++) {
    for (var gfy = garden.y0 + 8; gfy <= garden.y0 + 9; gfy++) tiles[idx(gfx, gfy)] = T_FLOWERBED;
  }
  for (var glx = garden.x0 + 13; glx <= garden.x0 + 15; glx++) {
    tiles[idx(glx, garden.y0 + 9)] = T_LOWWALL;
  }

  // Quarry Ridge: the mine adit + sit-rock + a small sign. BUILD mine A
  // (GROK_MINE_ANSWER.md): the bottom-centre tile of the adit is a real door
  // (solid, Enter) into instance:mine -- an honest cave, no ore (that lock
  // stays). The rest of the face is still rock.
  var quarry = regionAABB(findRegion("Quarry Ridge"));
  for (var qax = quarry.x0 + 8; qax <= quarry.x0 + 12; qax++) {
    for (var qay = quarry.y0 + 4; qay <= quarry.y0 + 7; qay++) tiles[idx(qax, qay)] = T_ADIT;
  }
  MINE_DOOR_TILE = { x: quarry.x0 + 10, y: quarry.y0 + 7 };
  tiles[idx(MINE_DOOR_TILE.x, MINE_DOOR_TILE.y)] = T_MINE_DOOR;
  interactables.push({ type: "mineDoor", x: MINE_DOOR_TILE.x, y: MINE_DOOR_TILE.y, prompt: "Enter" });
  tiles[idx(quarry.x0 + 9, quarry.y0 + 10)] = T_BENCH; // the sit-rock
  var signX = quarry.x0 + 14, signY = quarry.y0 + 8;
  tiles[idx(signX, signY)] = T_SIGN;
  interactables.push({ type: "sign", x: signX, y: signY, prompt: "Read", text: "Watch your step." });

  // Branch paths: the only path in the whole map was the north-south spine through the middle column
  // (the Square -> Homestead Row -> Creek -> Shore) plus its jog into Shore -- every actual building
  // (a door into an interior, or the Hall's confirm) sat in open grass with nothing leading to it. One
  // tile wide (the spine is two, so the hierarchy reads main road vs. side path), routed by hand around
  // every existing prop rather than through it -- verified against the flood-fill/collision test, not
  // just eyeballed. T_DIRT under the walk-in Lab door already reads as a threshold; these tie into it.
  for (var wby = 28; wby <= 32; wby++) tiles[idx(11, wby)] = T_PATH; // Workshop door (11,27) south to the crossbar
  for (var wbh = 11; wbh <= 34; wbh++) tiles[idx(wbh, 32)] = T_PATH; // crossbar east into the Square, meeting the spine at x=34.
  // Straight now, not a dogleg: the two scatter crates this used to duck around (19,32) were part of the
  // old hand-authored list. scatter_author.js's own legality rule (no scatter prop is an orthogonal
  // neighbour of a path, benches excepted) means a freshly generated crate can never land here again.

  for (var lbh = 12; lbh <= 34; lbh++) tiles[idx(lbh, 48)] = T_PATH; // Lab door (11,48) east through Homestead Row to the spine.
  // After the path, so the lane does not erase the front. The door is the
  // east end of a short roof, and it stays the walk-in tile.
  placeFrontWest(11, 48, 4, T_LAB_DOOR);
  // Straight now, not a dogleg: same reasoning -- the two crates at (16,48)/(17,48) were the old hand
  // list; scatter_author.js cannot regenerate a prop onto this lane or its shoulder.

  for (var mbx = 10; mbx <= 10; mbx++) for (var mby = 68; mby <= 69; mby++) tiles[idx(mbx, mby)] = T_PATH; // mine door (10,67) south to the crossbar
  // Stops at x=33, not 34: (34,69) is already the pre-existing spine, and it sits over water there
  // (Creek's water band), so bakeTileIds() already recoloured it to a bridge -- painting plain T_PATH
  // over it here would silently undo that recolour on one tile.
  for (var mbh = 10; mbh <= 33; mbh++) tiles[idx(mbh, 69)] = T_PATH; // crossbar east through Creek, meeting the existing Shore jog

  for (var hbh = 34; hbh <= 57; hbh++) tiles[idx(hbh, 30)] = T_PATH; // spine, across the Square and Hall Lawn, to just short of the Hall's own flanking hedges at x=58/62

  // The map grew one column to the east (72 -> 96). These four regions are
  // that column. The lane is x=84. The orchard road (y=9) and the garden
  // street (y=46) are extended to meet it, so the new land is the same walk
  // and not a second map you have to already know about. The water crossing
  // at the Point is a bridge, same as the creek. Trees on the north of the
  // Grove and the east edge of the grass are the stop, the same reason the
  // North Woods has a tree line instead of an invisible wall.
  var east = 84;
  for (var ox = 66; ox <= east; ox++) tiles[idx(ox, 9)] = T_PATH;
  for (var ey = 10; ey <= 65; ey++) tiles[idx(east, ey)] = T_PATH;
  for (var by = 66; by <= 72; by++) tiles[idx(east, by)] = T_BRIDGE;
  for (var beachY = 73; beachY <= 76; beachY++) tiles[idx(east, beachY)] = T_PATH;
  for (var gx = 70; gx < east; gx++) tiles[idx(gx, 46)] = T_PATH;
  var groveTrees = [[76, 4], [90, 4], [76, 14], [90, 14]];
  for (var gt = 0; gt < groveTrees.length; gt++) tiles[idx(groveTrees[gt][0], groveTrees[gt][1])] = T_TREE;
  for (var gnx = 72; gnx <= 94; gnx++) tiles[idx(gnx, 0)] = T_TREE;
  tiles[idx(WORLD_W - 1, 0)] = T_TREE; // the corner, or it is a one-tile pocket behind the tree line
  for (var edgeY = 1; edgeY <= 65; edgeY++) tiles[idx(WORLD_W - 1, edgeY)] = T_TREE;
  tiles[idx(78, 52)] = T_HEDGE;
  tiles[idx(80, 52)] = T_HEDGE;
  tiles[idx(82, 52)] = T_HEDGE;

  // Every one of these roads already came this far and stopped at grass.
  // The door is the thing at the end. Each is three tiles tall: roof,
  // windows, door. The path tile south of the door is where you stand.
  placeFront(82, 8, 2, T_DOOR_DEMO, "grove");
  for (var mxs = 85; mxs <= 89; mxs++) tiles[idx(mxs, 32)] = T_PATH;
  placeFront(89, 31, 2, T_DOOR_DEMO, "meadow");
  placeFront(80, 45, 2, T_DOOR_DEMO, "hedge");
  for (var pxs = 85; pxs <= 88; pxs++) tiles[idx(pxs, 76)] = T_PATH;
  placeFront(88, 75, 2, T_DOOR_DEMO, "point");
  tiles[idx(14, 85)] = T_PATH;
  for (var dxs = 15; dxs <= 18; dxs++) tiles[idx(dxs, 85)] = T_PATH;
  placeFront(18, 84, 2, T_DOOR_DEMO, "dunes");

  scatterRegions();
}

// BUILD studio 3 (CLAUDE_STUDIO_3.md): the rest of the frozen map table. A separate step, called after
// dressRegions() (below), not folded into it -- test_world_scatter.sh pins scatterRegions() as the last
// thing dressRegions() itself does, and that invariant is about scatterRegions()'s OWN safety property
// (it only ever paints plain ground), which this code doesn't touch, so it earns its own name rather
// than being smuggled onto the end of that function. New outdoor piles use T_FLOWERBED, the same
// coloured-pile tile the Orchard's apple pile already used (solid, like every other prop tile). Quarry
// stays on dirt -- studio.md: "do not lay flowerbed on the quarry. The Look is enough" -- so those two
// add only an interactable, no tile change at all.
//
// world_scatter_unit.js keeps a 1-tile clearance around every interactable (so a marker never sits in
// the middle of decorative clutter). CLAUDE_STUDIO_3.md's own literal coordinates for these six land
// inside that clearance around several pre-existing P3 scatter props (a flowerbed, a bench, a low
// wall), so each is nudged a few tiles onto the nearest clear plain ground in the same region, per that
// file's own fallback rule ("move one tile onto grass or dirt and say so in the writeup") -- noted here
// and in studio-3-built.md.
function addStudioGatherMarkers() {
  tiles[idx(5, 9)] = T_FLOWERBED; // spec: (14,8) -- inside the scatter bench's clearance at (15,7)
  interactables.push({ type: "studioGather", spot: "north_woods_twigs", x: 5, y: 9, prompt: "Look" });
  tiles[idx(49, 44)] = T_FLOWERBED; // spec: (52,44) -- that tile IS a scatter flowerbed itself
  interactables.push({ type: "studioGather", spot: "garden_madder", x: 49, y: 44, prompt: "Look" });
  tiles[idx(69, 47)] = T_FLOWERBED; // spec: (66,44) -- also a scatter flowerbed tile itself
  interactables.push({ type: "studioGather", spot: "garden_weld", x: 69, y: 47, prompt: "Look" });
  tiles[idx(49, 54)] = T_FLOWERBED; // spec: (52,54) -- inside the scatter low-wall's clearance
  interactables.push({ type: "studioGather", spot: "garden_woad", x: 49, y: 54, prompt: "Look" });
  // Quarry: no tile change (dirt stays dirt). spec: (4,74)/(18,74) -- both inside the scatter low
  // walls' clearance that fence the quarry pit; moved a few tiles onto open dirt, ochre west / alum
  // east same as the frozen table intended.
  interactables.push({ type: "studioGather", spot: "quarry_ochre", x: 8, y: 72, prompt: "Look" });
  interactables.push({ type: "studioGather", spot: "quarry_alum", x: 14, y: 72, prompt: "Look" });
}

// The building branches (Workshop, Lab, Hall, mine) are already in dressRegions.
// What they do not reach: the Lookout, the Orchard, the woods, the east doors,
// the Garden, and the two quarry piles. Same rule as those branches -- one tile,
// grass or dirt only, stop on the tile you stand on. Their tiles are left alone.
function pave(x, y) {
  var t = tiles[idx(x, y)];
  if (t === T_GRASS_A || t === T_GRASS_B || t === T_DIRT) tiles[idx(x, y)] = T_PATH;
}
function paveH(x0, x1, y) { for (var x = x0; x <= x1; x++) pave(x, y); }
function paveV(x, y0, y1) { for (var y = y0; y <= y1; y++) pave(x, y); }

function layApproachPaths() {
  // Spine stops at the Square. Continue it to the grass under the Lookout
  // railing, then east and north to the Orchard lane just above the keg.
  paveH(34, 35, 17); paveH(34, 35, 18); paveH(34, 35, 19);
  paveH(36, 42, 17);
  paveV(42, 9, 16);
  paveH(43, 65, 9);
  // Down the middle of Hall Lawn, stopping on the tile above the door. The
  // existing Hall branch arrives from the west on y=30; this one comes from
  // the Orchard and does not paint the door.
  paveV(60, 10, 29);

  // West of the Workshop facade (it owns x=8-14), down to that branch's
  // crossbar at y=32. The tree at (4,17) stays; the lane steps around it.
  // y=6 reaches toward the gall oak. The track at x=4 passes beside the twigs.
  paveH(4, 16, 6);
  paveV(4, 6, 16);
  pave(5, 17);
  paveV(4, 18, 32);
  paveH(5, 10, 32);

  // The Lab branch already runs along the south side of the west doors and
  // stops at the spine. Continue that same row to the Garden beds, and step
  // south to the tile above the woad. Madder is four tiles north of that
  // street and weld is the east edge, past the bed row, so neither was on a
  // path. Step north off the street, then east along the open grass above
  // the beds. The beds themselves stay; the lane goes around them.
  paveH(35, 55, 48);
  paveV(49, 45, 53);
  paveH(50, 69, 46);

  // Iron and copper are crates in the yard, not on the street. The door's
  // path is x=11. One step east along y=28 reaches the tile beside the iron,
  // and the next tile south is beside the copper too.
  paveH(12, 15, 28);
  pave(16, 29);

  // The mine branch already reaches the door and runs east at y=69. Ochre and
  // alum are still bare dirt south of that. One spur, not a second crossing.
  paveV(10, 70, 71);
  paveH(8, 14, 71);
}

// Darker grass only where it touches a lane or the bridge. Copied first so
// the shoulder does not spread across the field.
function vergePaths() {
  var next = new Uint8Array(tiles.length);
  var i, x, y, d, nx, ny, n, shoulder;
  var dirs = [[1, 0], [-1, 0], [0, 1], [0, -1]];
  for (i = 0; i < tiles.length; i++) next[i] = tiles[i];
  for (y = 0; y < WORLD_H; y++) {
    for (x = 0; x < WORLD_W; x++) {
      if (tiles[idx(x, y)] !== T_GRASS_A) continue;
      shoulder = false;
      for (d = 0; d < 4; d++) {
        nx = x + dirs[d][0]; ny = y + dirs[d][1];
        if (nx < 0 || ny < 0 || nx >= WORLD_W || ny >= WORLD_H) continue;
        n = tiles[idx(nx, ny)];
        if (n === T_PATH || n === T_BRIDGE) shoulder = true;
      }
      if (shoulder) next[idx(x, y)] = T_GRASS_B;
    }
  }
  for (i = 0; i < tiles.length; i++) tiles[i] = next[i];
}

// The extra props (SCATTER above), placed after everything else so they can only fill plain ground.
// Plain ground = grass, dirt or the Lookout plateau. A driftwood bench may sit on water (the Creek and Shore
// ones already do); nothing else may.
function scatterRegions() {
  for (var g = 0; g < SCATTER.length; g++) {
    var grp = SCATTER[g], box = regionAABB(findRegion(grp.region));
    for (var k = 0; k < grp.at.length; k++) {
      var x = grp.at[k][0], y = grp.at[k][1];
      var inside = x >= box.x0 && x <= box.x1 && y >= box.y0 && y <= box.y1;
      var base = inside ? tiles[idx(x, y)] : -1;
      var ground = base === T_GRASS_A || base === T_GRASS_B || base === T_DIRT || base === T_PLATEAU || (base === T_WATER && grp.tile === T_BENCH);
      if (!ground) { SCATTER_REFUSED.push(grp.region + " (" + x + "," + y + ")"); continue; }
      tiles[idx(x, y)] = grp.tile;
      SCATTER_PLACED++;
    }
  }
}

// ---- Bake the world once to an offscreen canvas (tilemap.md: bake once,
// blit the crop -- never loop 72x80 per frame). ---------------------------
var baked = document.createElement("canvas");
baked.width = WORLD_PW; baked.height = WORLD_PH;
// alpha:false on the bake too -- it's an opaque world, no reason to pay for
// a compositing channel nothing ever uses.
var bctx = baked.getContext("2d", { alpha: false });
bctx.imageSmoothingEnabled = false;
function paintBake() {
  for (var y = 0; y < WORLD_H; y++) {
    for (var x = 0; x < WORLD_W; x++) {
      drawTile(bctx, tiles[idx(x, y)], x, y, function (dx, dy) {
        var nx = x + dx, ny = y + dy;
        return (nx < 0 || ny < 0 || nx >= WORLD_W || ny >= WORLD_H) ? -1 : tiles[idx(nx, ny)];
      });
    }
  }
}
paintBake();
spaceRebakers.push(paintBake);

