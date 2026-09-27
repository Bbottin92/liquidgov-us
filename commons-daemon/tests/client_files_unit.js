// The game client's file structure and LOAD ORDER (commons/index.html + style.css + js/*.js).
//   node client_files_unit.js <path to the commons/ directory>
// Why this exists: the client used to be one inline <script>, where every function is visible from the first line (function
// declarations hoist across the whole script). Split into files, a file can only see what has ALREADY loaded, so a top-level call
// to something declared in a later file throws at load and blanks the game. Concatenating the sources proves nothing about that;
// this test loads the real files IN TAG ORDER, each as its own classic script in one shared global scope (what the browser does),
// under a stubbed DOM, and fails on any ReferenceError. Wrong orders must throw (so the check can fail), and the load-time
// code (the IIFEs, paintBake, applyCrop) must have actually RUN, by its effects.
// Prints 'client_files_unit: N checks, F failed'; exit 1 if any failed.
"use strict";
const fs = require("fs"), path = require("path"), vm = require("vm");

const dir = process.argv[2];
const read = (p) => fs.readFileSync(path.join(dir, p), "utf8");
let checks = 0, failed = 0;
function check(ok, what) { checks++; if (!ok) { failed++; console.log("  FAIL " + what); } }

const html = read("index.html");
const ORDER = ["js/world.js", "js/interiors.js", "js/sprites.js", "js/actors.js", "js/net.js", "js/input.js", "js/interact.js", "js/main.js", "js/easel.js", "js/studio.js"];

// ---- the page is a shell: markup + one stylesheet + the ten tags, in the documented order
const scripts = [...html.matchAll(/<script\b([^>]*)>([\s\S]*?)<\/script>/g)];
const srcs = scripts.map((m) => (m[1].match(/\bsrc="([^"]*)"/) || [])[1]);
check(JSON.stringify(srcs) === JSON.stringify(ORDER), "the <script src> tags are exactly the ten files in the documented order: " + JSON.stringify(srcs));
check(scripts.every((m) => m[2].trim() === ""), "no inline script is left in index.html");
check(scripts.every((m) => !/\b(defer|async|type\s*=|nomodule)\b/i.test(m[1])), "no defer / async / type=module on any tag (the order must match the old inline script)");
check(!/<style\b/i.test(html), "no inline <style> is left in index.html");
const css = read("style.css");
check(html.includes('id="easelCanvas" width="32" height="32"') && html.includes('data-tool="smudge"') && html.includes('id="easelUndo"'), "the easel overlay is a 32x32 canvas with smudge and undo");
check(html.includes('id="bench"') && html.includes('id="benchBatches"') && html.includes('id="benchRecipes"') && html.includes('id="benchMix"'), "the bench overlay has batches, recipes and a mix row");
check(html.includes('id="notebook"') && html.includes('id="notebookLines"'), "the notebook overlay has a lines container");
check(/<div id="batchReady"[^>]*role="status"[^>]*><\/div>/.test(html), "index.html has the empty #batchReady line");
check(css.includes("#easel") && css.includes("min-height:44px") && css.includes("100dvh - 16.5rem") && /@media \(max-width:\s*480px\)/.test(css), "the easel has a portrait-phone layout: 44px targets and a canvas that leaves room for the tools");
const links = [...html.matchAll(/<link\b([^>]*)>/g)].filter((m) => /rel="stylesheet"/.test(m[1])).map((m) => (m[1].match(/\bhref="([^"]*)"/) || [])[1]);
check(JSON.stringify(links) === JSON.stringify(["style.css"]), "one stylesheet, style.css: " + JSON.stringify(links));
const lastTag = html.lastIndexOf("<script"), bodyEnd = html.indexOf("</body>");
check(lastTag > 0 && lastTag < bodyEnd && html.indexOf("<div id=\"nameOverlay\">") < html.indexOf("<script"), "the scripts come after all the markup they use (the old script sat at the end of <body>)");

// ---- the files
const jsFiles = fs.readdirSync(path.join(dir, "js")).filter((f) => f.endsWith(".js")).map((f) => "js/" + f).sort();
check(JSON.stringify(jsFiles) === JSON.stringify([...ORDER].sort()), "js/ holds exactly those ten files, none unreferenced: " + JSON.stringify(jsFiles));
for (const f of ORDER) {
  const t = read(f);
  check(t.startsWith('"use strict";\n'), f + " starts with \"use strict\"; (strict mode is per script: only the first file would get it otherwise)");
  check(t.length > 500 && t.endsWith("\n"), f + " is a real file that ends with a newline");
}
check(read("style.css").length > 500, "style.css is not empty");
check(read("js/net.js").includes("// ---- netMint:begin") && read("js/net.js").includes("// ---- netMint:end"), "the netMint test markers are in js/net.js");
check(read("js/net.js").includes("// ---- netStatus:begin") && read("js/net.js").includes("// ---- netStatus:end"), "the netStatus (connection badge) test markers are in js/net.js");
check(/<div id="connStatus"[^>]*role="status"[^>]*aria-live="polite"[^>]*><\/div>/.test(html), "index.html has the empty #connStatus live region (the badge)");
check(/<div id="invHud"[^>]*><\/div>/.test(html), "index.html has the empty #invHud strip (what you hold)");
check(/<div id="toast"[^>]*role="status"[^>]*><\/div>/.test(html), "index.html has the empty #toast pill");
check(/<div id="placeName"><\/div>/.test(html), "index.html has the empty #placeName banner");
check(/<title>The Commons<\/title>/.test(html), "the tab title is The Commons, not a leftover P3");
check(/id="siteLink" href="\/"/.test(html), "the in-game LiquidGov link goes to the civic site, not a new URL");
check(/class="toSite"/.test(html), "the welcome card has a way back to LiquidGov");

// ---- no name is declared in two files (a redeclaration would silently change which one wins)
const declared = new Map(); const dups = [];
for (const f of ORDER) for (const m of read(f).matchAll(/^(?:var|function)\s+([A-Za-z_$][\w$]*)/gm)) {
  if (declared.has(m[1]) && declared.get(m[1]) !== f) dups.push(m[1] + " (" + declared.get(m[1]) + " and " + f + ")");
  declared.set(m[1], f);
}
check(dups.length === 0, "no top-level var/function is declared in two different files: " + dups.join(", "));
check(declared.size > 150, "the files declare their names at the top level as before (" + declared.size + " names)");

// ---- load them in tag order, the way the browser does, under a stubbed DOM
function deepWith(over) {
  const self = new Proxy(function () {}, {
    get: (t, k) => k in over ? over[k] : k === Symbol.toPrimitive ? () => 0 : k === "length" ? 0 : k === "then" ? undefined : deepWith({}),
    apply: () => deepWith({}), construct: () => deepWith({}), set: () => true, has: () => true,
  });
  return self;
}
function makeEnv() {
  // rec.byEl: canvas element -> { draws } (draw calls made on THAT canvas's 2d context), so the effects can be attributed:
  // "the bake drew" must mean the `baked` canvas, not any canvas. rec.byId: getElementById results, with every property set on them.
  const rec = { images: [], errors: [], listeners: [], byEl: new WeakMap(), byId: {} };
  const DRAW = new Set(["fillRect", "drawImage", "putImageData", "fill", "stroke", "fillText", "strokeRect"]);
  const mkCtx = (r) => new Proxy(function () {}, {
    get: (t, k) => k === Symbol.toPrimitive ? () => 0 : k === "then" ? undefined : (...a) => { if (DRAW.has(k)) r.draws++; return deepWith({}); },
    set: () => true,
  });
  const recorded = (base) => new Proxy(base, {                       // an element that remembers what the game sets on it
    get: (t, k) => k in t ? t[k] : k === Symbol.toPrimitive ? () => 0 : k === "length" ? 0 : k === "then" ? undefined : deepWith({}),
    set: (t, k, v) => { t[k] = v; return true; }, has: () => true,
  });
  const canvasEl = () => {
    const r = { draws: 0 }, ctx = mkCtx(r);
    const el = recorded({ getContext: () => ctx, addEventListener() {}, style: {} });
    rec.byEl.set(el, r); return el;
  };
  const el = (id) => {
    if (id === "game") { const c = canvasEl(); rec.byId[id] = c; return c; }
    return (rec.byId[id] = recorded({ addEventListener: (n) => rec.listeners.push(n), style: {}, focus() {}, classList: deepWith({}) }));
  };
  const g = {
    location: { hostname: "localhost", origin: "http://localhost:8090", search: "", href: "http://localhost:8090/" },
    navigator: { userAgent: "probe", maxTouchPoints: 0 }, innerWidth: 1280, innerHeight: 800, devicePixelRatio: 1,
    document: deepWith({ getElementById: (id) => rec.byId[id] || el(id), createElement: (t) => t === "canvas" ? canvasEl() : el("_" + t), addEventListener() {}, hidden: false }),
    requestAnimationFrame: () => 0, setTimeout: () => 0, setInterval: () => 0, clearTimeout() {}, clearInterval() {},
    performance: { now: () => 0 }, fetch: () => new Promise(() => {}),
    // (a decoded image reports the size the game checks for, so its onload does its real work when fired)
    Image: function () {
      const o = new Proxy({}, { set: (t, k, v) => { t[k] = v; if (k === "src") { const s = /tiles\.png/.test(v) ? [256, 64] : /sprites\.png/.test(v) ? [128, 24] : [0, 0]; t.naturalWidth = s[0]; t.naturalHeight = s[1]; } return true; } });
      rec.images.push(o); return o;
    },
    addEventListener(n) { rec.listeners.push(n); }, removeEventListener() {},
    console: { log() {}, warn() {}, error: (...a) => rec.errors.push(a.join(" ")) },
    URLSearchParams, URL, encodeURIComponent, Promise, Date, screen: { width: 1280, height: 800 },
    localStorage: { getItem: () => null, setItem() {} },
    sessionStorage: { getItem: () => null, setItem() {} },
  };
  g.window = g; vm.createContext(g); return { g, rec };
}
function load(order) {
  const env = makeEnv(); const done = [];
  for (const f of order) {
    try { vm.runInContext(read(f), env.g, { filename: f }); done.push(f); }
    catch (e) { return { ok: false, file: f, error: e.name + ": " + e.message, done, ...env }; }
  }
  return { ok: true, done, ...env };
}
const ev = (env, expr) => vm.runInContext(expr, env.g);

const ok = load(ORDER);
check(ok.ok, "all nine files load in tag order" + (ok.ok ? "" : ": " + ok.file + " THREW " + ok.error));
if (ok.ok) {
  // the load-time code must have RUN (a silent skip would look like success), judged by its effects
  check(ev(ok, "typeof spriteAtlasCtx !== 'undefined' && spriteAtlasCtx !== null"), "paintSpriteAtlas ran (the sprite atlas context is set)");
  check(ev(ok, "(function () { var c = regionAABB(findRegion('Creek')); return tiles[66 * WORLD_W + (c.x0 + 2)] === T_WATER && tiles[c.y0 * WORLD_W + (c.x0 + 2)] !== T_WATER; })()"), "bakeTileIds ran (Creek has a water ribbon and a grass bank)");
  const bakeDraws = ok.rec.byEl.get(ev(ok, "baked")).draws, worldCells = ev(ok, "WORLD_W * WORLD_H");
  check(bakeDraws >= worldCells, "paintBake ran: the `baked` canvas got at least one draw per world cell (" + bakeDraws + " >= " + worldCells + ")");
  check(ok.rec.byEl.get(ev(ok, "spriteAtlas")).draws > 0, "paintSpriteAtlas drew onto the sprite atlas canvas");
  const urls = ok.rec.images.map((i) => i.src);
  check(urls.includes(ev(ok, "TILE_SHEET_URL")) && urls.includes(ev(ok, "SPRITE_SHEET_URL")), "loadSheets ran (it requested both sheets: " + JSON.stringify(urls) + ")");
  check(ok.rec.byId.game.width === 240 && ok.rec.byId.game.height === 160 && ok.rec.byId.game.style.width === "720px",
    "applyCrop ran: it sized the #game canvas from the crop (240x160, shown at 720px for a landscape window): " + ok.rec.byId.game.width + "x" + ok.rec.byId.game.height);
  check(!ok.rec.errors.some((e) => /spawn tile is blocked/.test(e)), "assertSpawnIsFree ran and found the spawn free");
  check(ev(ok, "typeof player === 'object' && player.x > 0 && player.y > 0"), "the player exists at a spawn");
  check(ok.rec.listeners.includes("keydown") || ok.rec.listeners.includes("resize"), "the input and resize listeners were registered");
  for (const fn of ["netMint", "netHandleAuthFailure", "enter", "tryInteract", "applyCrop", "paintBake", "netStatusTick", "netStatusReport", "renderInvHud", "tickToast", "tickPlaceName", "drawNameplate", "ingestOccupants", "stepOccupants", "drawItemGlyph", "drawSortedActors", "blitTallTile", "blitCanopy"]) check(ev(ok, "typeof " + fn) === "function", fn + " is a function after load");
  check(ev(ok, "netStatusShown") === false && ev(ok, "netStatusFails") === 0, "the badge starts hidden with no failures counted");
  check(ok.rec.errors.length === 0, "nothing was reported through console.error during load: " + JSON.stringify(ok.rec.errors));
  check(ev(ok, "typeof openEasel === 'function' && typeof closeEasel === 'function' && typeof drawStudioPortrait === 'function'"), "the easel entry points exist after load");
  check(ev(ok, "STUDIO_PORTRAIT.x === 10 && STUDIO_PORTRAIT.y === 0 && STUDIO_PORTRAIT.w === 2 && STUDIO_PORTRAIT.h === 2"), "the studio portrait is the pinned 2x2 at tile (10, 0)");
  check(ev(ok, "ownHouseSpace.interactables.some(function (it) { return it.type === 'studioPortrait' && it.x === 10 && it.y === 0; })"), "the owner's house has the portrait frame on that tile");
  check(ev(ok, "workshopSpace.interactables.some(function (it) { return it.type === 'easel' && it.x === 7 && it.y === 4; })"), "the workshop table is the easel");
  check(ev(ok, "demoHouseSpace.interactables.every(function (it) { return it.type !== 'studioPortrait' && it.type !== 'easel'; })"), "the shared demo house does not grow an easel or a portrait frame");
  var guestFetches = 0, guestThrew = "";
  ok.g.fetch = function () { guestFetches++; return Promise.resolve({ ok: false, json: function () { return Promise.resolve({}); } }); };
  try { ev(ok, "isBound = false; mode = 'workshop'; openEasel();"); }
  catch (e) { guestThrew = e.name + ": " + e.message; }
  check(!guestThrew && guestFetches === 0, "a guest opening the easel does not call the painting endpoints" + (guestThrew ? " (" + guestThrew + ")" : " (" + guestFetches + " fetches)"));

  // ---- BUILD studio 3: the bench, the notebook, and the map/lab wiring
  check(ev(ok, "typeof openBench === 'function' && typeof closeBench === 'function' && typeof openNotebook === 'function' && typeof closeNotebook === 'function' && typeof netGather === 'function' && typeof netFetchStudio === 'function'"), "the studio-3 entry points exist after load");
  check(ev(ok, "workshopSpace.interactables.some(function (it) { return it.type === 'bench' && it.x === 3 && it.y === 2 && it.prompt === 'Make'; })"), "the workshop has the bench, prompt Make");
  check(ev(ok, "workshopSpace.interactables.some(function (it) { return it.type === 'notebook' && it.x === 10 && it.y === 2 && it.prompt === 'Read'; })"), "the workshop has the notebook, prompt Read");
  check(ev(ok, "typeof WORLD_PICKUPS === 'undefined'"), "WORLD_PICKUPS is gone, not left dead");
  check(ev(ok, "interactables.every(function (it) { return it.type !== 'worldPickup'; })"), "no worldPickup interactable survives in the outdoor world");
  check(ev(ok, "['north_woods_galls','north_woods_twigs','orchard_keg','shore_shells','quarry_ochre','quarry_alum','garden_madder','garden_weld','garden_woad','workshop_iron','workshop_copper'].every(function (s) { return interactables.some(function (it) { return it.type === 'studioGather' && it.spot === s; }); })"), "all eleven outdoor studioGather spots are present");
  check(ev(ok, "labSpace.interactables.filter(function (it) { return it.type === 'studioGather'; }).map(function (it) { return it.spot; }).sort().join(',') === 'lab_gum,lab_soot'"), "the lab's gum and soot stations are studio gather spots");
  check(ev(ok, "labSpace.interactables.filter(function (it) { return it.type === 'labStock'; }).map(function (it) { return it.stock; }).sort().join(',') === 'alum,tannin,vinegar'"), "the lab's vinegar, tannin and alum stations toast instead of granting");
  check(ev(ok, "labSpace.interactables.filter(function (it) { return it.type === 'labStation'; }).map(function (it) { return it.recipe; }).sort().join(',') === 'chalk,copper,iron'"), "copper, iron and chalk stay unconverted trophy lookups");
  var gFetches = 0, gThrew = "";
  ok.g.fetch = function () { gFetches++; return Promise.resolve({ ok: false, json: function () { return Promise.resolve({}); } }); };
  try { ev(ok, "isBound = false; openBench(); openNotebook(); netGather('north_woods_galls');"); }
  catch (e) { gThrew = e.name + ": " + e.message; }
  check(!gThrew && gFetches === 0, "a guest opening the bench/notebook or gathering never calls the studio endpoints" + (gThrew ? " (" + gThrew + ")" : " (" + gFetches + " fetches)"));

  // ---- BUILD studio 4: the palette ladder and the second frame
  check(ev(ok, "typeof drawStudioPainting === 'function'"), "drawStudioPainting exists after load");
  check(ev(ok, "STUDIO_PAINTING.x === 4 && STUDIO_PAINTING.y === 0 && STUDIO_PAINTING.w === 2 && STUDIO_PAINTING.h === 2"), "the second frame is pinned at tile (4, 0)");
  check(ev(ok, "ownHouseSpace.interactables.some(function (it) { return it.type === 'studioPainting' && it.x === 4 && it.y === 0 && it.prompt === 'Draw'; })"), "the owner's house has the painting frame on that tile");
  check(ev(ok, "demoHouseSpace.interactables.every(function (it) { return it.type !== 'studioPainting'; })"), "the shared demo house does not grow a second frame");
  check(ev(ok, "ownHouseSpace.solid[idxIn(16, 4, 1)] === 0 && ownHouseSpace.solid[idxIn(16, 5, 1)] === 0"), "(4,1) and (5,1) stay plain walkable floor -- the frame is an overlay, not a tile change");
  check(ev(ok, "STUDIO_PIGMENTS.length === 13 && STUDIO_PIGMENTS[0].index === 1 && STUDIO_PIGMENTS[0].hex === '#1a1a1a' && STUDIO_PIGMENTS[12].index === 13 && STUDIO_PIGMENTS[12].hex === '#2e4c72'"), "the client's 13-pigment table starts at charcoal black and ends at woad blue, same numbers as the daemon's");
  check(ev(ok, "GUEST_PALETTE.length === 4 && GUEST_PALETTE.every(function (p) { return p.index >= 1 && p.index <= 4; })"), "a guest's palette is exactly the four charcoal indices");
  var pFetches = [];
  ok.g.fetch = function (url) { pFetches.push(url); return Promise.resolve({ ok: false, json: function () { return Promise.resolve({}); } }); };
  ev(ok, "isBound = true; mode = 'workshop'; openEasel();");
  ev(ok, "closeEasel(); mode = 'house'; openEasel('painting');");
  check(pFetches.length === 2 && /\/painting\/portrait$/.test(pFetches[0]) && /\/painting\/painting$/.test(pFetches[1]),
    "opening from the workshop table fetches the portrait URL, opening the new frame fetches the painting URL: " + JSON.stringify(pFetches));
  var pgFetches = 0, pgThrew = "";
  ok.g.fetch = function () { pgFetches++; return Promise.resolve({ ok: false, json: function () { return Promise.resolve({}); } }); };
  try { ev(ok, "isBound = false; openEasel('painting');"); }
  catch (e) { pgThrew = e.name + ": " + e.message; }
  check(!pgThrew && pgFetches === 0, "a guest never enters painting mode (no fetch even if asked)" + (pgThrew ? " (" + pgThrew + ")" : ""));
}

// ---- controls: the check must be able to FAIL (a wrong order has to throw a ReferenceError)
const ctl = [["sprites.js before world.js", ["js/sprites.js", "js/world.js", "js/interiors.js", "js/actors.js", "js/net.js", "js/input.js", "js/interact.js", "js/main.js"]],
  ["main.js first", ["js/main.js", "js/world.js", "js/interiors.js", "js/sprites.js", "js/actors.js", "js/net.js", "js/input.js", "js/interact.js"]],
  ["actors.js before sprites.js", ["js/world.js", "js/interiors.js", "js/actors.js", "js/sprites.js", "js/net.js", "js/input.js", "js/interact.js", "js/main.js"]]];
for (const [label, order] of ctl) {
  const r = load(order);
  check(!r.ok && /^ReferenceError/.test(r.error), "control: " + label + " throws a ReferenceError (" + (r.ok ? "it did NOT throw" : r.error) + ")");
}
// ---- async callbacks between scripts: the browser runs the <script> tags one after another with the event loop free in between, so an
// image that is already cached can fire its onload after ANY tag. A handler in an early file that touches a name declared in a LATER file
// throws a ReferenceError (strict mode) -- the live client did exactly that with forceRedraw (assigned in sprites.js, declared in main.js).
function asyncBetweenScripts(order) {
  const env = makeEnv(); const errs = [];
  for (const f of order) {
    vm.runInContext(read(f), env.g, { filename: f });
    for (const img of env.rec.images) {
      if (typeof img.onload !== "function") continue;
      try { img.onload(); } catch (e) { errs.push("after " + f + ": " + img.src + " onload threw " + e.name + ": " + e.message); }
    }
  }
  return errs;
}
const asyncErrs = asyncBetweenScripts(ORDER);
check(asyncErrs.length === 0, "an image finishing its load between any two scripts causes no ReferenceError: " + asyncErrs.slice(0, 3).join(" | "));
check(load(ORDER).rec.images.filter((i) => typeof i.onload === "function").length >= 2, "and the two sheets' onload handlers are registered, so that check really fires them");

const missing = load(ORDER.slice(0, 4).concat(ORDER.slice(5)));            // a file left out of the tags (a missing net.js)
check(!missing.ok || ev(missing, "typeof netMint") === "undefined", "control: with net.js missing, netMint is not defined (so a missing file cannot pass unnoticed)");

console.log("client_files_unit: " + checks + " checks, " + failed + " failed");
process.exit(failed ? 1 : 0);
