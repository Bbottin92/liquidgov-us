// commons-daemon/tools/scatter_author.js
// Deterministic authoring of commons/js/world.js's SCATTER array. The old array was a hand list
// padded past world_scatter_unit.js's own "total >= 150" floor, which is exactly why the Square,
// Hall Lawn and the Orchard ended up dressed twice (once by dressRegions()'s own set piece, once
// again by SCATTER). This script REPLACES that hand list with the output of a fixed-seed generator.
// The game still just reads the SCATTER array at load time -- nothing is rolled in the browser.
//
//   node commons-daemon/tools/scatter_author.js          # regenerate and write commons/js/world.js
//   node commons-daemon/tools/scatter_author.js --check  # exit 0 only if a fresh run matches exactly
//
// Run with node from the repo (or anywhere without a "type":"module" ancestor package.json --
// world_scatter_unit.js has the same constraint and the same reason: this is CommonJS, no deps.
"use strict";
const fs = require("fs");
const path = require("path");
const vm = require("vm");

const SEED = 20260927; // fixed. Change this (or a rule below) to move a prop -- never hand-patch a coordinate.
const WORLD_JS = path.join(__dirname, "..", "..", "commons", "js", "world.js");

// ---- mulberry32: a small seedable PRNG (public-domain shape, credited in every copy of it online).
// Good enough for "shuffle a few hundred candidate tiles" -- this is prop placement, not cryptography.
function mulberry32(seed) {
  let a = seed >>> 0;
  return function () {
    a |= 0; a = (a + 0x6D2B79F5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
function shuffle(rng, arr) {
  const out = arr.slice();
  for (let i = out.length - 1; i > 0; i--) {
    const j = Math.floor(rng() * (i + 1));
    const tmp = out[i]; out[i] = out[j]; out[j] = tmp;
  }
  return out;
}
function randInt(rng, min, max) { return min + Math.floor(rng() * (max - min + 1)); } // inclusive

// ---- load world.js in a vm with SCATTER forced to [], the same shape tests/world_scatter_unit.js
// already loads it in, so "what does the real, undressed ground look like" is asked the identical way
// the test asks it. dressRegions(), addStudioGatherMarkers(), layApproachPaths() and vergePaths() all
// still run (they're unconditional in the file's own load-time IIFE) -- scatterRegions() just has
// nothing to place. This *is* "run with nothing planted, so every lane is actually paved."
function deepWith(over) {
  return new Proxy(function () {}, {
    get: (t, k) => k in over ? over[k] : k === Symbol.toPrimitive ? () => 0 : k === "length" ? 0 : k === "then" ? undefined : deepWith({}),
    apply: () => deepWith({}), construct: () => deepWith({}), set: () => true, has: () => true,
  });
}
function loadEmptiedWorld(src) {
  const emptied = src.replace(/var SCATTER = \[[\s\S]*?\];\n/, "var SCATTER = [];\n");
  if (emptied === src) throw new Error("scatter_author: could not find `var SCATTER = [...]` to empty");
  const ctx = deepWith({});
  const g = {
    document: deepWith({ createElement: () => deepWith({ getContext: () => ctx, style: {} }), getElementById: () => deepWith({}) }),
    Image: function () { return {}; }, console, Uint8Array, Math, parseInt, Object, Array,
  };
  g.window = g; vm.createContext(g);
  vm.runInContext(emptied, g, { filename: "js/world.js" });
  return g;
}

function buildModel(src) {
  const g = loadEmptiedWorld(src);
  const ev = (expr) => vm.runInContext(expr, g);
  const W = ev("WORLD_W"), H = ev("WORLD_H");
  const T = {};
  for (const n of ["T_GRASS_A", "T_GRASS_B", "T_PATH", "T_WATER", "T_DIRT", "T_PLATEAU", "T_BRIDGE",
                    "T_TREE", "T_BENCH", "T_CRATE", "T_FLOWERBED", "T_LOWWALL", "T_HEDGE"]) {
    T[n] = ev(n);
  }
  const tiles = ev("Array.from(tiles)");
  const solid = ev("Array.from(solid)");
  const interactables = JSON.parse(ev("JSON.stringify(interactables)"));
  const labDoor = JSON.parse(ev("JSON.stringify(LAB_DOOR_TILE)"));
  const mineDoor = JSON.parse(ev("JSON.stringify(MINE_DOOR_TILE)"));
  const regions = JSON.parse(ev("JSON.stringify(REGIONS)"));
  const region = (name) => JSON.parse(ev("JSON.stringify(regionAABB(findRegion(" + JSON.stringify(name) + ")))"));
  const idx = (x, y) => y * W + x;

  // ---- the "clear" map: identical in shape to tests/world_scatter_unit.js's own mark(), on purpose --
  // a generated prop must never be a tile that test would independently flag. Any future drift between
  // the two is a bug in whichever one didn't get updated.
  const clear = new Map();
  const mark = (x, y) => { if (x >= 0 && y >= 0 && x < W && y < H) clear.set(x + "," + y, 1); };
  for (const r of regions) {
    const a = region(r.name);
    for (let x = a.x0; x <= a.x1; x++) { mark(x, a.y0); mark(x, a.y1); }
    for (let y = a.y0; y <= a.y1; y++) { mark(a.x0, y); mark(a.x1, y); }
    for (let x = a.x0 + 9; x <= a.x0 + 14; x++) for (let y = a.y0 + 9; y <= a.y0 + 10; y++) mark(x, y);
  }
  for (let y = 20; y < H; y++) for (let x = 33; x <= 36; x++) mark(x, y);
  for (let x = 33; x <= 66; x++) for (let y = 68; y <= 71; y++) mark(x, y);
  const ring = (x, y, r) => { for (let dy = -r; dy <= r; dy++) for (let dx = -r; dx <= r; dx++) mark(x + dx, y + dy); };
  for (const it of interactables) ring(it.x, it.y, 1);
  ring(labDoor.x, labDoor.y, 1);
  ring(mineDoor.x, mineDoor.y, 1);
  const sq = region("the Square"), spawnX = Math.floor((sq.x0 + sq.x1 + 1) / 2), spawnY = Math.floor((sq.y0 + sq.y1 + 1) / 2);
  ring(spawnX, spawnY, 2);
  for (const [x, y] of [[35, sq.y0 + 2], [35, 49], [35, 69], [65, 69]]) ring(x, y, 1);

  return { W, H, T, tiles, solid, region, idx, clear };
}

function makeLegality(model) {
  const { W, H, T, tiles, solid, idx, clear } = model;
  function groundOk(x, y, tileType, regionName) {
    const base = tiles[idx(x, y)];
    if (base === T.T_GRASS_A || base === T.T_GRASS_B || base === T.T_DIRT || base === T.T_PLATEAU) return true;
    if (base === T.T_WATER && tileType === T.T_BENCH && (regionName === "Creek" || regionName === "Shore")) return true;
    return false;
  }
  function pathAdjacent(x, y) {
    for (const [dx, dy] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
      const nx = x + dx, ny = y + dy;
      if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
      const n = tiles[idx(nx, ny)];
      if (n === T.T_PATH || n === T.T_BRIDGE) return true;
    }
    return false;
  }
  function clearOfSolid(x, y) {
    for (let dy = -1; dy <= 1; dy++) for (let dx = -1; dx <= 1; dx++) {
      const nx = x + dx, ny = y + dy;
      if (nx < 0 || ny < 0 || nx >= W || ny >= H) continue;
      if (solid[idx(nx, ny)]) return false;
    }
    return true;
  }
  function legal(x, y, tileType, regionName, aabb) {
    if (x < aabb.x0 || x > aabb.x1 || y < aabb.y0 || y > aabb.y1) return false;
    if (!groundOk(x, y, tileType, regionName)) return false;
    if (clear.has(x + "," + y)) return false;
    if (pathAdjacent(x, y) && tileType !== T.T_BENCH) return false; // a bench alone may sit on the shoulder
    if (!clearOfSolid(x, y)) return false; // Chebyshev >= 2 from every solid tile already in the emptied world
    return true;
  }
  return { legal };
}

// ---- per-region recipes -------------------------------------------------------------------------
// [typeName, min, max] tuples, in the priority order a shuffled candidate is offered them. Orchard,
// the Square and Hall Lawn get nothing here -- dressRegions() already gave each its own set piece
// (six trees, two benches, two hedges respectively), and doubling that dressing on top was the whole
// "why does everything look padded" complaint. A region with a "row/line/run" requirement gets its own
// restricted candidate space for that one type (built by `rowCandidates` below) instead of the
// region's general open ground -- a single flat shuffle cannot produce a *line*, only a scatter.
const RECIPES = {
  "North Woods": [["T_TREE", 4, 8]],
  "Lookout": [["T_TREE", 2, 2]], // exactly 2, x <= 29, off the plateau -- see restrictLookout
  "Orchard": [],
  "the Square": [],
  "Hall Lawn": [],
  "Workshop Lane": [["T_CRATE", 0, 4], ["T_TREE", 0, 2]], // corner stamp excluded, see restrictWorkshop
  "Lab Yard": [["T_CRATE", 0, 3], ["T_BENCH", 0, 1], ["T_TREE", 0, 2]],
  "Homestead Row": [["T_TREE", 0, 4], ["T_BENCH", 0, 2]], // door fronts excluded, see restrictHomestead
  "Garden": [["T_TREE", 0, 2], ["T_BENCH", 0, 2]], // flowerbeds are the row-restricted type, handled separately
  "Quarry Ridge": [["T_CRATE", 0, 2], ["T_TREE", 0, 2]], // low walls are the run-restricted type, handled separately
  "Creek": [["T_BENCH", 0, 1]], // trees are the line-restricted type, handled separately
  "Shore": [["T_BENCH", 0, 1], ["T_FLOWERBED", 0, 2], ["T_CRATE", 0, 1]], // trees are the line-restricted type
};

function restrictWorkshop(x, y) { return !(x <= 3 || x >= 21 || y >= 36); } // the corner stamp
function restrictLookout(x, y, aabb) { return x <= 29 && !(x >= 30 && x <= 41 && y >= 10 && y <= 15); } // off the plateau
const HOMESTEAD_DOOR_XS = [25, 26, 29, 32, 37, 40, 43, 46]; // houseDoorX + the 7 facade doors
function restrictHomestead(x, y) {
  if (y !== 46 && y !== 48) return true;
  return HOMESTEAD_DOOR_XS.indexOf(x) === -1;
}

function main() {
  const checkOnly = process.argv.includes("--check");
  const src = fs.readFileSync(WORLD_JS, "utf8");
  const model = buildModel(src);
  const { T, region } = model;
  const { legal } = makeLegality(model);
  const rng = mulberry32(SEED);

  const accepted = []; // { region, tile, x, y }, growing as we go -- the cross-region spacing rule
  function farFromAccepted(x, y) {
    for (const a of accepted) if (Math.max(Math.abs(a.x - x), Math.abs(a.y - y)) < 2) return false;
    return true;
  }
  function tryPlace(regionName, tileName, x, y, aabb) {
    if (!legal(x, y, T[tileName], regionName, aabb)) return false;
    if (!farFromAccepted(x, y)) return false;
    accepted.push({ region: regionName, tile: tileName, x, y });
    return true;
  }

  function allTilesIn(aabb) {
    const out = [];
    for (let y = aabb.y0; y <= aabb.y1; y++) for (let x = aabb.x0; x <= aabb.x1; x++) out.push([x, y]);
    return out;
  }

  // A region's general pass: one shuffle of every remaining-legal-for-something tile, then a greedy
  // walk offering each candidate to the first type (in the recipe's own listed order) that still wants
  // more and finds the tile legal. No attempt budget: a target that the single pass cannot fill is left
  // short, not retried.
  function runRegionGeneral(regionName, recipe) {
    const aabb = region(regionName);
    const targets = recipe.map(([name, min, max]) => ({ name, need: randInt(rng, min, max) }));
    if (targets.every((t) => t.need === 0)) return;
    const candidates = shuffle(rng, allTilesIn(aabb));
    for (const [x, y] of candidates) {
      for (const t of targets) {
        if (t.need <= 0) continue;
        if (tryPlace(regionName, t.name, x, y, aabb)) { t.need--; break; }
      }
    }
  }

  // A restricted pass for a "row/line/run" type: candidates come from `spaceFn` (already filtered to
  // the intended shape), not the whole region, so the result reads as a line even though which cells
  // along it are actually used is still the seeded shuffle's choice.
  function runRestricted(regionName, tileName, min, max, spaceFn) {
    const aabb = region(regionName);
    let need = randInt(rng, min, max);
    if (need <= 0) return;
    const space = shuffle(rng, spaceFn());
    for (const [x, y] of space) {
      if (need <= 0) break;
      if (tryPlace(regionName, tileName, x, y, aabb)) need--;
    }
  }

  // ---- North Woods / Lookout (restricted: off the plateau, x<=29) ----
  runRegionGeneral("North Woods", RECIPES["North Woods"]);
  runRestricted("Lookout", "T_TREE", 2, 2, () => {
    const a = region("Lookout");
    return allTilesIn(a).filter(([x, y]) => restrictLookout(x, y, a));
  });

  // ---- Workshop Lane (restricted: off the corner stamp) ----
  runRegionGeneral("Workshop Lane", []); // placeholder kept for symmetry; real pass is restricted below
  {
    const a = region("Workshop Lane");
    const targets = RECIPES["Workshop Lane"].map(([name, min, max]) => ({ name, need: randInt(rng, min, max) }));
    const candidates = shuffle(rng, allTilesIn(a).filter(([x, y]) => restrictWorkshop(x, y)));
    for (const [x, y] of candidates) {
      for (const t of targets) {
        if (t.need <= 0) continue;
        if (tryPlace("Workshop Lane", t.name, x, y, a)) { t.need--; break; }
      }
    }
  }

  runRegionGeneral("Lab Yard", RECIPES["Lab Yard"]);

  // ---- Homestead Row (restricted: door fronts stay clear) ----
  {
    const a = region("Homestead Row");
    const targets = RECIPES["Homestead Row"].map(([name, min, max]) => ({ name, need: randInt(rng, min, max) }));
    const candidates = shuffle(rng, allTilesIn(a).filter(([x, y]) => restrictHomestead(x, y)));
    for (const [x, y] of candidates) {
      for (const t of targets) {
        if (t.need <= 0) continue;
        if (tryPlace("Homestead Row", t.name, x, y, a)) { t.need--; break; }
      }
    }
  }

  // ---- Garden: flowerbeds in 1-2 horizontal rows (restricted), trees + benches general ----
  {
    const a = region("Garden");
    const rowCount = randInt(rng, 1, 2);
    const candidateYs = [];
    for (let y = a.y0 + 2; y <= a.y1 - 2; y++) candidateYs.push(y);
    const rowYs = shuffle(rng, candidateYs).slice(0, rowCount);
    runRestricted("Garden", "T_FLOWERBED", 4, 8, () =>
      allTilesIn(a).filter(([x, y]) => rowYs.indexOf(y) !== -1));
  }
  runRegionGeneral("Garden", RECIPES["Garden"]);

  // ---- Quarry Ridge: low walls in one broken run near the pit (restricted), crates + trees general ----
  {
    const a = region("Quarry Ridge");
    // The candidate space is a band tracing the pit's rough perimeter, not the whole 24x20 region --
    // "one broken run," not a scatter. The universal path-adjacency rule (legal() above) is what
    // actually carves the gap where the mine lane crosses this band; nothing region-specific does that.
    const band = [];
    for (let x = a.x0 + 2; x <= a.x1 - 2; x++) { band.push([x, a.y0 + 4]); band.push([x, a.y0 + 17]); }
    for (let y = a.y0 + 4; y <= a.y0 + 17; y++) { band.push([a.x0 + 2, y]); band.push([a.x1 - 2, y]); }
    runRestricted("Quarry Ridge", "T_LOWWALL", 4, 8, () => band);
  }
  runRegionGeneral("Quarry Ridge", RECIPES["Quarry Ridge"]);

  // ---- Creek / Shore: trees in one line (restricted), the rest general ----
  // Both regions' water band starts at their own y0+6 (waterY0=66, both AABBs have y0=60), and water is
  // solid -- a line drawn at y0+5 sits directly against it, and the Chebyshev>=2-from-solid rule then
  // rejects the WHOLE row (every tile in it has a solid water neighbour directly south). Found by
  // running the generator, not by inspection: Shore came back with zero trees on the first pass. y0+1
  // to y0+3 keeps the line's own southern neighbour (as far down as y0+4) solidly on dry ground.
  runRestricted("Creek", "T_TREE", 4, 6, () => {
    const a = region("Creek");
    const y = a.y0 + randInt(rng, 1, 3);
    return allTilesIn(a).filter(([, ty]) => ty === y);
  });
  runRegionGeneral("Creek", RECIPES["Creek"]);
  runRestricted("Shore", "T_TREE", 4, 6, () => {
    const a = region("Shore");
    const y = a.y0 + randInt(rng, 1, 3);
    return allTilesIn(a).filter(([, ty]) => ty === y);
  });
  runRegionGeneral("Shore", RECIPES["Shore"]);

  // ---- serialize: group by (region, tile), same shape the old hand list used ----
  const groups = [];
  const groupIndex = new Map();
  for (const p of accepted) {
    const key = p.region + "|" + p.tile;
    if (!groupIndex.has(key)) {
      groupIndex.set(key, groups.length);
      groups.push({ region: p.region, tile: p.tile, at: [] });
    }
    groups[groupIndex.get(key)].at.push([p.x, p.y]);
  }
  for (const g of groups) g.at.sort((a, b) => a[0] - b[0] || a[1] - b[1]);

  const lines = groups.map((g) =>
    "  { region: " + JSON.stringify(g.region) + ", tile: " + g.tile + ", at: [" +
    g.at.map(([x, y]) => "[" + x + "," + y + "]").join(",") + "] }"
  );
  const arrayText = "var SCATTER = [\n" +
    "  // Generated by commons-daemon/tools/scatter_author.js, seed " + SEED + ". Not hand-edited --\n" +
    "  // `node commons-daemon/tools/scatter_author.js --check` fails if this drifts from a fresh run.\n" +
    "  // Change a rule in that file, or the seed, to move a prop.\n" +
    lines.join(",\n") + "\n" +
    "];\n";

  if (checkOnly) {
    const current = src.match(/var SCATTER = \[[\s\S]*?\];\n/);
    if (!current || current[0] !== arrayText) {
      console.log("scatter_author --check: DRIFT (a fresh run does not match commons/js/world.js)");
      process.exit(1);
    }
    console.log("scatter_author --check: ok (" + accepted.length + " props, matches world.js exactly)");
    process.exit(0);
  }

  const updated = src.replace(/var SCATTER = \[[\s\S]*?\];\n/, arrayText);
  fs.writeFileSync(WORLD_JS, updated);
  console.log("scatter_author: wrote " + accepted.length + " props to " + WORLD_JS);
}

main();
