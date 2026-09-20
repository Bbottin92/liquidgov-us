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
check(total >= 150, "there is a real amount of scatter (" + total + " props)");
check(ev("SCATTER_REFUSED.length") === 0, "no prop was refused (a refused one would have overwritten a door, the path or a wall): " + ev("JSON.stringify(SCATTER_REFUSED)"));
check(ev("SCATTER_PLACED") === total, "every listed prop was placed (" + ev("SCATTER_PLACED") + " of " + total + ")");

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
  check(wsp.its.length === 1 && wsp.its[0].type === "sign" && wsp.its[0].text.length > 10, "the workshop has exactly one interactable, a sign with text (nothing to pick up or place)");
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
