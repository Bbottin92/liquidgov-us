// The extra world props (commons/js/world.js SCATTER + scatterRegions): they must fill plain ground and nothing else, and they must
// never make the world worse to walk.
//   node world_scatter_unit.js <path to the commons/ directory>
// world.js is the first script the page loads and needs only a stubbed DOM, so this loads just that file, as the browser does, and reads
// the real tile and collision arrays it built. Checks: every prop landed (none refused), each is inside its own region, none sits on what
// must stay clear (the path spine, the bridge jog, every region's edge tiles = the seams, the strip where a region's name is stamped, one
// tile around every door / sign / interactable / the lab and mine doors, the spawn, the passerby's waypoints), and a flood fill from the
// spawn reaches EVERY walkable tile (no pocket sealed off) and every interactable's neighbour. Prints 'world_scatter_unit: N checks, F failed'.
"use strict";
const fs = require("fs"), path = require("path"), vm = require("vm");

const dir = process.argv[2];
let checks = 0, failed = 0;
function check(ok, what) { checks++; if (!ok) { failed++; console.log("  FAIL " + what); } }

function deepWith(over) {
  return new Proxy(function () {}, {
    get: (t, k) => k in over ? over[k] : k === Symbol.toPrimitive ? () => 0 : k === "length" ? 0 : k === "then" ? undefined : deepWith({}),
    apply: () => deepWith({}), construct: () => deepWith({}), set: () => true, has: () => true,
  });
}
function loadWorld(src) {
  const ctx = deepWith({});
  const g = {
    document: deepWith({ createElement: () => deepWith({ getContext: () => ctx, style: {} }), getElementById: () => deepWith({}) }),
    Image: function () { return {}; }, console, Uint8Array, Math, parseInt, Object, Array,
  };
  g.window = g; vm.createContext(g);
  vm.runInContext(src, g, { filename: "js/world.js" });
  return g;
}
const src = fs.readFileSync(path.join(dir, "js/world.js"), "utf8");
const g = loadWorld(src);
const ev = (expr) => vm.runInContext(expr, g);
const W = ev("WORLD_W"), H = ev("WORLD_H"), TILE_ = ev("TILE");

const SCATTER = JSON.parse(ev("JSON.stringify(SCATTER)"));
const total = SCATTER.reduce((n, grp) => n + grp.at.length, 0);
check(ev("SCATTER_REFUSED.length") === 0, "no prop was refused (a refused one would have overwritten a door, the path or a wall): " + ev("JSON.stringify(SCATTER_REFUSED)"));
check(ev("SCATTER_PLACED") === total, "every listed prop was placed (" + ev("SCATTER_PLACED") + " of " + total + ")");

// ---- caps, per region and tile: commons-daemon/tools/scatter_author.js's own RECIPES table, asserted
// here independently rather than trusted -- the generator caps are what make the Square/Hall Lawn/
// Orchard NOT double-dressed (they get zero SCATTER entries; dressRegions() already gave each its own
// set piece) and what stop North Woods, Garden, Quarry Ridge etc. from silently drifting past a sane
// count if the generator or its seed ever changes. Deliberately includes the zeros.
const T_ = (n) => ev(n);
function countOf(region, tile) {
  return SCATTER.filter((g) => g.region === region && g.tile === T_(tile)).reduce((n, g) => n + g.at.length, 0);
}
function inRange(n, lo, hi) { return n >= lo && n <= hi; }
const CAPS = [
  ["North Woods", "T_TREE", 4, 8], ["North Woods", "T_BENCH", 0, 0], ["North Woods", "T_FLOWERBED", 0, 0], ["North Woods", "T_CRATE", 0, 0], ["North Woods", "T_LOWWALL", 0, 0],
  ["Lookout", "T_TREE", 2, 2], ["Lookout", "T_BENCH", 0, 0], ["Lookout", "T_FLOWERBED", 0, 0], ["Lookout", "T_CRATE", 0, 0], ["Lookout", "T_LOWWALL", 0, 0],
  ["Orchard", "T_TREE", 0, 0], ["Orchard", "T_BENCH", 0, 0], ["Orchard", "T_FLOWERBED", 0, 0], ["Orchard", "T_CRATE", 0, 0], ["Orchard", "T_LOWWALL", 0, 0],
  ["the Square", "T_TREE", 0, 0], ["the Square", "T_BENCH", 0, 0], ["the Square", "T_FLOWERBED", 0, 0], ["the Square", "T_CRATE", 0, 0], ["the Square", "T_LOWWALL", 0, 0],
  ["Hall Lawn", "T_TREE", 0, 0], ["Hall Lawn", "T_BENCH", 0, 0], ["Hall Lawn", "T_FLOWERBED", 0, 0], ["Hall Lawn", "T_CRATE", 0, 0], ["Hall Lawn", "T_LOWWALL", 0, 0], ["Hall Lawn", "T_HEDGE", 0, 0],
  ["Workshop Lane", "T_CRATE", 0, 4], ["Workshop Lane", "T_TREE", 0, 2], ["Workshop Lane", "T_BENCH", 0, 0], ["Workshop Lane", "T_FLOWERBED", 0, 0], ["Workshop Lane", "T_LOWWALL", 0, 0],
  ["Lab Yard", "T_CRATE", 0, 3], ["Lab Yard", "T_BENCH", 0, 1], ["Lab Yard", "T_TREE", 0, 2], ["Lab Yard", "T_FLOWERBED", 0, 0],
  // Raised 0-4/0-2 to 4-7/1-2 (BUILD property coherence follow-up): walking the map found Homestead Row
  // reading as "a gate, one sign, and three trees" -- the old caps let the region's own random roll come
  // up nearly empty, and its stale candidate-space restriction (from the torn-down 8-door facade) made it
  // worse. Both fixed in scatter_author.js; this is that fix's new floor, not a loosened ceiling.
  ["Homestead Row", "T_TREE", 4, 7], ["Homestead Row", "T_BENCH", 1, 2], ["Homestead Row", "T_FLOWERBED", 0, 0],
  ["Garden", "T_FLOWERBED", 4, 8], ["Garden", "T_TREE", 0, 2], ["Garden", "T_BENCH", 0, 2], ["Garden", "T_LOWWALL", 0, 0],
  ["Quarry Ridge", "T_LOWWALL", 4, 8], ["Quarry Ridge", "T_CRATE", 0, 2], ["Quarry Ridge", "T_TREE", 0, 2], ["Quarry Ridge", "T_FLOWERBED", 0, 0], ["Quarry Ridge", "T_BENCH", 0, 0],
  ["Creek", "T_TREE", 4, 6], ["Creek", "T_BENCH", 0, 1], ["Creek", "T_FLOWERBED", 0, 0], ["Creek", "T_CRATE", 0, 0], ["Creek", "T_LOWWALL", 0, 0],
  ["Shore", "T_TREE", 4, 6], ["Shore", "T_BENCH", 0, 1], ["Shore", "T_FLOWERBED", 0, 2], ["Shore", "T_CRATE", 0, 1], ["Shore", "T_LOWWALL", 0, 0],
];
const capViolations = [];
for (const [region_, tile, lo, hi] of CAPS) {
  const n = countOf(region_, tile);
  if (!inRange(n, lo, hi)) capViolations.push(region_ + "/" + tile + ": " + n + " not in [" + lo + "," + hi + "]");
}
check(capViolations.length === 0, "every region/tile count is within scatter_author.js's own caps: " + capViolations.join("; "));

// ---- lane sentinels: a scatter prop must never land ON a lane tile. The universal legality rule in
// scatter_author.js already forbids this (a final T_PATH/T_BRIDGE tile fails groundOk outright), so
// this is a check the OLD hand-list-based test literally could not have -- one sentinel per lane,
// asserted directly against the tile array rather than trusting the generator's own logic to be right.
const LANE_SENTINELS = {
  Workshop: [25, 32], Lab: [25, 48], Hall: [50, 30], mine: [20, 69], Lookout: [35, 18],
  Orchard: [50, 9], woods: [4, 24], Garden: [45, 48], ochre: [10, 71],
  madder: [49, 45], weld: [60, 46], iron: [14, 28],
};
const T_PATH_ = T_("T_PATH"), T_BRIDGE_ = T_("T_BRIDGE");
const laneBroken = [];
for (const [name, [x, y]] of Object.entries(LANE_SENTINELS)) {
  const t = ev("tiles[" + (y * W + x) + "]");
  if (t !== T_PATH_ && t !== T_BRIDGE_) laneBroken.push(name + " (" + x + "," + y + ") is tile " + t + ", not a path/bridge");
}
check(laneBroken.length === 0, "no scatter prop (or anything else) has paved over a lane sentinel: " + laneBroken.join("; "));

// ---- the generator is the source of truth: a fresh run must reproduce this file's array exactly
const { execFileSync } = require("child_process");
try {
  const out = execFileSync(process.execPath, [path.join(dir, "..", "commons-daemon", "tools", "scatter_author.js"), "--check"], { encoding: "utf8" });
  check(/^scatter_author --check: ok/.test(out.trim()), "scatter_author.js --check confirms this SCATTER array is exactly what the seed produces: " + out.trim());
} catch (e) {
  check(false, "scatter_author.js --check failed (SCATTER has drifted from a fresh generator run): " + (e.stdout || e.message));
}

// no coordinate is listed twice
const seen = new Set(); const dups = [];
for (const grp of SCATTER) for (const [x, y] of grp.at) { const k = x + "," + y; if (seen.has(k)) dups.push(k); seen.add(k); }
check(dups.length === 0, "no tile is listed twice: " + dups.join(" "));

// ---- what must stay clear
const region = (name) => JSON.parse(ev("JSON.stringify(regionAABB(findRegion(" + JSON.stringify(name) + ")))"));
const regions = JSON.parse(ev("JSON.stringify(REGIONS)"));
const clear = new Map();                      // "x,y" -> why
const mark = (x, y, why) => { if (x >= 0 && y >= 0 && x < W && y < H && !clear.has(x + "," + y)) clear.set(x + "," + y, why); };
for (const r of regions) {
  const a = region(r.name);
  for (let x = a.x0; x <= a.x1; x++) { mark(x, a.y0, "seam: " + r.name + " edge"); mark(x, a.y1, "seam: " + r.name + " edge"); }
  for (let y = a.y0; y <= a.y1; y++) { mark(a.x0, y, "seam: " + r.name + " edge"); mark(a.x1, y, "seam: " + r.name + " edge"); }
  // the region's name is stamped at its centre: 8px monospace, about 4 tiles wide, straddling two tile rows
  for (let x = a.x0 + 9; x <= a.x0 + 14; x++) for (let y = a.y0 + 9; y <= a.y0 + 10; y++) mark(x, y, "label: " + r.name);
}
for (let y = 20; y < H; y++) for (let x = 33; x <= 36; x++) mark(x, y, "the path spine");
for (let x = 33; x <= 66; x++) for (let y = 68; y <= 71; y++) mark(x, y, "the bridge jog");
const ring = (x, y, r, why) => { for (let dy = -r; dy <= r; dy++) for (let dx = -r; dx <= r; dx++) mark(x + dx, y + dy, why); };
for (const it of JSON.parse(ev("JSON.stringify(interactables)"))) ring(it.x, it.y, 1, "next to a " + it.type);
const lab = JSON.parse(ev("JSON.stringify(LAB_DOOR_TILE)")), mine = JSON.parse(ev("JSON.stringify(MINE_DOOR_TILE)"));
ring(lab.x, lab.y, 1, "next to the lab door"); ring(mine.x, mine.y, 1, "next to the mine door");
const sq = region("the Square"), spawnX = Math.floor((sq.x0 + sq.x1 + 1) / 2), spawnY = Math.floor((sq.y0 + sq.y1 + 1) / 2);
ring(spawnX, spawnY, 2, "the spawn");
for (const [x, y] of [[35, sq.y0 + 2], [35, 49], [35, 69], [65, 69]]) ring(x, y, 1, "a passerby waypoint");

const bad = [];
for (const grp of SCATTER) {
  const a = region(grp.region);
  for (const [x, y] of grp.at) {
    if (!(x >= a.x0 && x <= a.x1 && y >= a.y0 && y <= a.y1)) bad.push(grp.region + " (" + x + "," + y + ") is outside its region");
    else if (clear.has(x + "," + y)) bad.push(grp.region + " (" + x + "," + y + ") is on " + clear.get(x + "," + y));
  }
}
check(bad.length === 0, "no prop is outside its region or on a tile that must stay clear: " + bad.join("; "));

// only the six existing prop tiles are used (no new art, no new collision rule)
const allowed = new Set(["T_TREE", "T_BENCH", "T_CRATE", "T_FLOWERBED", "T_LOWWALL", "T_HEDGE"].map((n) => ev(n)));
check(SCATTER.every((grp) => allowed.has(grp.tile)), "the scatter uses only tree, bench, crate, flower bed, low wall and hedge tiles");
check(SCATTER.every((grp) => ev("SOLID_TILE[" + grp.tile + "]") === 1), "every scatter tile is solid, like the same props already in the world");

// ---- walkability: 4-connected flood fill from the spawn over the real collision array
const solid = ev("Array.from(solid)");
const walk = (x, y) => x >= 0 && y >= 0 && x < W && y < H && !solid[y * W + x];
check(walk(spawnX, spawnY), "the spawn tile is walkable");
const seenW = new Uint8Array(W * H), q = [[spawnX, spawnY]]; seenW[spawnY * W + spawnX] = 1; let reached = 0;
while (q.length) {
  const [x, y] = q.pop(); reached++;
  for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
    const nx = x + dx, ny = y + dy;
    if (walk(nx, ny) && !seenW[ny * W + nx]) { seenW[ny * W + nx] = 1; q.push([nx, ny]); }
  }
}
let walkable = 0; const sealed = [];
for (let y = 0; y < H; y++) for (let x = 0; x < W; x++) if (walk(x, y)) { walkable++; if (!seenW[y * W + x]) sealed.push(x + "," + y); }
check(sealed.length === 0, "every walkable tile is reachable from the spawn (" + reached + " of " + walkable + "); sealed off: " + sealed.slice(0, 12).join(" "));
check(walkable > 2000, "and there is a lot of ground to walk (" + walkable + " tiles)");
// every interactable has at least one reachable neighbour to stand on
const noStand = [];
for (const it of JSON.parse(ev("JSON.stringify(interactables)"))) {
  if (![[1, 0], [-1, 0], [0, 1], [0, -1]].some(([dx, dy]) => seenW[(it.y + dy) * W + it.x + dx])) noStand.push(it.type + " (" + it.x + "," + it.y + ")");
}
check(noStand.length === 0, "every door, sign and interactable can be reached from a walkable neighbour: " + noStand.join(", "));
check(seenW[(lab.y + 1) * W + lab.x] === 1 && seenW[lab.y * W + lab.x] === 1, "the lab door is walkable and reachable");
check(seenW[(mine.y + 1) * W + mine.x] === 1, "the mine door's front tile is reachable");

// ---- the Workshop (BUILD workshop A): the Workshop Lane door is real, and the room behind it is sound
const ws = JSON.parse(ev("JSON.stringify(WORKSHOP_DOOR_TILE)"));
const wsIts = JSON.parse(ev("JSON.stringify(interactables)")).filter((i) => i.x === ws.x && i.y === ws.y);
check(wsIts.length === 1 && wsIts[0].type === "workshopDoor" && wsIts[0].prompt === "Enter", "the Workshop Lane door tile carries exactly one interactable, a workshopDoor with the Enter prompt");
check(ev("interactables.some(function (i) { return i.type === 'shutter'; })") === false, "no closed-shutter interactable is left on Workshop Lane");
check(ev("tiles[" + (ws.y * W + ws.x) + "]") === ev("T_DOOR_DEMO") && solid[ws.y * W + ws.x] === 1, "the workshop door is the existing solid door tile (no new art), so it is Enter, not walk-in");
check(seenW[(ws.y + 1) * W + ws.x] === 1, "the workshop door's front tile is reachable from the spawn");
check(ws.x >= 8 && ws.x <= 14 && ws.y >= 24 && ws.y <= 27, "the door is on the Workshop Lane facade");

// interiors.js runs on top of world.js in the same page scope, exactly as the browser loads it
vm.runInContext(fs.readFileSync(path.join(dir, "js/interiors.js"), "utf8"), g, { filename: "js/interiors.js" });
const wsp = JSON.parse(ev("JSON.stringify({ w: workshopSpace.w, h: workshopSpace.h, solid: Array.from(workshopSpace.solid), tiles: Array.from(workshopSpace.tiles), its: workshopSpace.interactables, entry: WORKSHOP_ENTRY, door: WORKSHOP_DOOR })"));
const wOpen = (x, y) => x >= 0 && y >= 0 && x < wsp.w && y < wsp.h && !wsp.solid[y * wsp.w + x];
let wsFloor = 0; for (let i = 0; i < wsp.solid.length; i++) if (!wsp.solid[i]) wsFloor++;
check(wsp.w === 14 && wsp.h === 10 && wsFloor > 60, "the workshop is 14x10 with real floor to walk (" + wsFloor + " open tiles) -- a room of nothing would pass every check below vacuously");
check(wOpen(wsp.entry.x, wsp.entry.y) && wOpen(wsp.door.x, wsp.door.y) && wsp.door.y === wsp.h - 1 && wsp.entry.y === wsp.door.y - 1, "the entry tile and the door tile are open, the door on the bottom wall with the entry right above it");
check(wsp.tiles[wsp.door.y * wsp.w + wsp.door.x] === ev("T_IDOOR"), "the interior door is the walk-through doorway tile");
{
  const seenI = new Uint8Array(wsp.w * wsp.h), qi = [[wsp.entry.x, wsp.entry.y]]; seenI[wsp.entry.y * wsp.w + wsp.entry.x] = 1; let reachedI = 0;
  while (qi.length) {
    const [x, y] = qi.pop(); reachedI++;
    for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) { const nx = x + dx, ny = y + dy; if (wOpen(nx, ny) && !seenI[ny * wsp.w + nx]) { seenI[ny * wsp.w + nx] = 1; qi.push([nx, ny]); } }
  }
  check(reachedI === wsFloor, "every open tile in the workshop is reachable from the entry (" + reachedI + " of " + wsFloor + ")");
  check(seenI[wsp.door.y * wsp.w + wsp.door.x] === 1, "the doorway can be walked onto from inside");
  // BUILD studio 3: the workshop grew from a bare sign (P2) to sign + easel (slice 2) + bench + notebook
  // (slice 3) -- this check was never updated for the easel and has been vacuously wrong since studio 2;
  // fixed here to name all four instead of asserting a stale count of one.
  const sign = wsp.its.find((it) => it.type === "sign");
  check(!!sign && sign.text.length > 10, "the workshop has a sign with real text");
  check(wsp.its.some((it) => it.type === "easel" && it.x === 7 && it.y === 4), "the workshop has the easel table");
  check(wsp.its.some((it) => it.type === "bench" && it.x === 3 && it.y === 2 && it.prompt === "Make"), "the workshop has the bench, prompt Make");
  check(wsp.its.some((it) => it.type === "notebook" && it.x === 10 && it.y === 2 && it.prompt === "Read"), "the workshop has the notebook, prompt Read");
  check(wsp.its.length === 4, "and nothing else is in the workshop (" + wsp.its.length + " interactables)");
  check(wsp.its.every((it) => [[1, 0], [-1, 0], [0, 1], [0, -1]].some(([dx, dy]) => seenI[(it.y + dy) * wsp.w + it.x + dx])), "the sign can be read from a reachable neighbour");
}
check(/INTERIOR_INSTANCE_KEY\s*=\s*\{[^}]*workshop:\s*"instance:workshop"/.test(fs.readFileSync(path.join(dir, "js/net.js"), "utf8")), "net.js maps the workshop mode to the shared instance:workshop key");
// each region has walkable ground on every side it should connect to (the seams stay open)
const seams = [];
for (const r of regions) {
  const a = region(r.name);
  for (const [n, dx, dy] of [["north", 0, -1], ["south", 0, 1], ["west", -1, 0], ["east", 1, 0]]) {
    const nb = regions.find((o) => o.col === r.col + dx && o.row === r.row + dy);
    if (!nb) continue;
    let open = 0;
    const xs = dx === 0 ? [a.x0, a.x1] : [dx > 0 ? a.x1 : a.x0], ys = dy === 0 ? [a.y0, a.y1] : [dy > 0 ? a.y1 : a.y0];
    for (let x = xs[0]; x <= xs[xs.length - 1]; x++) for (let y = ys[0]; y <= ys[ys.length - 1]; y++) if (walk(x, y) && walk(x + dx, y + dy)) open++;
    if (open === 0 && r.name !== "Creek" && r.name !== "Shore" && nb.name !== "Creek" && nb.name !== "Shore" && !(r.name === "Quarry Ridge" || nb.name === "Quarry Ridge")) seams.push(r.name + " -> " + nb.name);
  }
}
check(seams.length === 0, "no seam between land regions is blocked: " + seams.join(", "));

console.log("world_scatter_unit: " + checks + " checks, " + failed + " failed");
process.exit(failed ? 1 : 0);
