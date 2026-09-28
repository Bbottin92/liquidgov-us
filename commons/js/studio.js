"use strict";
// The Studio, slice 3 (BUILD studio 3, CLAUDE_STUDIO_3.md): the client for the daemon-only gather/
// bench/mix/collect built in slice 1. A bound player's state is a server-authoritative cache
// (studioState), refreshed only at the three points CLAUDE_STUDIO_3.md names -- a bound join, entering
// the Workshop, and at most every 30s while the bench overlay is open -- never from the 2Hz presence
// tick. A guest never calls any of these routes; gather is a local, session-only RAM simulation.

// The fixed gather-spot table, mirroring commons-daemon/src/studio.cpp's gather_spots() exactly (item,
// qty, cooldown). Not client-authored data any more than the daemon's own copy is: both are fixed
// tables, and a spot slug here that didn't also exist there would just always come back unknown_spot.
var STUDIO_GATHER_SPOTS = {
  north_woods_galls: { item: "oak_gall",       qty: 3, cooldownMs: 600000,   text: "Oak galls on this tree." },
  north_woods_twigs: { item: "willow_twigs",   qty: 3, cooldownMs: 600000,   text: "Willow twigs." },
  orchard_keg:       { item: "vinegar",        qty: 3, cooldownMs: 86400000, text: "The keg." },
  shore_shells:      { item: "seashell",       qty: 3, cooldownMs: 600000,   text: "Seashells, calcium carbonate." },
  quarry_ochre:      { item: "ochre_earth",    qty: 3, cooldownMs: 600000,   text: "Ochre earth." },
  quarry_alum:       { item: "alum_stone",     qty: 3, cooldownMs: 600000,   text: "Alum stone." },
  garden_madder:     { item: "madder_root",    qty: 3, cooldownMs: 600000,   text: "Madder root." },
  garden_weld:       { item: "weld",           qty: 3, cooldownMs: 600000,   text: "Weld." },
  garden_woad:       { item: "woad_leaves",    qty: 3, cooldownMs: 600000,   text: "Woad leaves." },
  workshop_iron:     { item: "scrap_iron",     qty: 3, cooldownMs: 600000,   text: "Scrap iron." },
  workshop_copper:   { item: "copper_sheet",   qty: 3, cooldownMs: 600000,   text: "Copper sheet." },
  lab_gum:           { item: "gum_arabic_raw", qty: 3, cooldownMs: 600000,   text: "Gum arabic." },
  lab_soot:          { item: "soot",           qty: 3, cooldownMs: 600000,   text: "Soot." }
};

// ---- Gather -------------------------------------------------------------
// A guest's cooldown is local, session-only RAM (v1.md's "do not invent guest persistence" -- a
// cooldown that survived a refresh would be exactly that). Date.now() is fine here: nothing about it
// needs to be frozen for a test the way the daemon's own now_ms() seam does, and it does not need to
// survive a refresh at all.
var guestGatherCooldowns = {};

function studioGatherGuest(spot) {
  var g = STUDIO_GATHER_SPOTS[spot];
  if (!g) return;
  var now = Date.now();
  if (now < (guestGatherCooldowns[spot] || 0)) { showToast("Not yet."); return; }
  guestGatherCooldowns[spot] = now + g.cooldownMs;
  inventory[g.item] = Math.min(99, (inventory[g.item] || 0) + g.qty);
  showToast(g.text);
  renderInvHud();
}

function studioGatherBound(spot) {
  var g = STUDIO_GATHER_SPOTS[spot];
  fetch(DAEMON + "/presence/gather", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ spot: spot })
  }).then(function (res) { return res.json().then(function (j) { return { res: res, j: j }; }); })
    .then(function (got) {
      if (!got.res.ok) {
        showToast(got.j && got.j.reason === "on_cooldown" ? "Not yet." : "Couldn't gather that.");
        return;
      }
      inventory[got.j.item] = Math.min(99, (inventory[got.j.item] || 0) + got.j.qty);
      renderInvHud();
      showToast((g && g.text) || (got.j.item + "."));
    }).catch(function () { showToast("Couldn't gather that."); });
}

// The one entry point interact.js calls. Guests never reach the daemon (CLAUDE_STUDIO_3.md "Do not":
// "Let a guest call gather, bench, mix, collect, or studio").
function netGather(spot) {
  if (isBound) studioGatherBound(spot);
  else studioGatherGuest(spot);
}

// ---- Studio state cache ---------------------------------------------------
// Refreshed only at the three points CLAUDE_STUDIO_3.md names. studioState stays null for a guest
// (and for a bound player before the first fetch lands) -- every renderer below treats null as "nothing
// known yet", not as an error.
var studioState = null;
var studioFetchBusy = false;

function applyStudioInventory(list) {
  // "Trust the server inventory on the next studio GET" (CLAUDE_STUDIO_3.md): a full replace, the same
  // trust model netLoadInventory() already uses for the pickup-sourced half of this same shared table.
  var next = {};
  for (var i = 0; i < (list || []).length; i++) next[list[i].item] = list[i].qty;
  inventory = next;
  renderInvHud();
}

function netFetchStudio() {
  if (!isBound || studioFetchBusy) return;
  studioFetchBusy = true;
  fetch(DAEMON + "/presence/studio")
    .then(function (res) { return res.ok ? res.json() : null; })
    .then(function (j) {
      if (!j || !j.ok) return;
      studioState = j;
      applyStudioInventory(j.inventory);
      if (benchOpen()) renderBench();
      if (notebookOpen()) renderNotebook();
    })
    .catch(function () {})
    .finally(function () { studioFetchBusy = false; });
}

// "At most every 30 seconds" while the bench overlay is open (CLAUDE_STUDIO_3.md "When to fetch").
// Never ticks for a guest (netFetchStudio no-ops) and never runs off the 2Hz presence tick -- this is
// main.js's rAF frame(), throttled here to a much slower cadence of its own.
var lastBenchFetchAt = 0;
function tickStudioOverlays(t) {
  if (!benchOpen()) return;
  if (t - lastBenchFetchAt < 30000) return;
  lastBenchFetchAt = t;
  netFetchStudio();
}

// "A batch is ready.": one line off the canvas, same family as #placeName/#connStatus, driven only by
// the cached studioState (never its own fetch -- CLAUDE_STUDIO_3.md "Collect still only works from the
// bench").
function tickBatchReady() {
  var el = document.getElementById("batchReady");
  if (!el) return;
  var ready = false;
  if (studioState && studioState.batches) {
    var now = Date.now();
    for (var i = 0; i < studioState.batches.length; i++) {
      if (studioState.batches[i].ready_ms <= now) { ready = true; break; }
    }
  }
  el.textContent = ready ? "A batch is ready." : "";
}

// ---- BUILD backpack decision 6: the bench, visually --------------------------------------------------
// Two small fixed tables mirroring catalog.sql's own recipes/recipe_outputs rows, the same "client
// mirrors a daemon-fixed table" shape STUDIO_GATHER_SPOTS above already uses. Neither /presence/studio's
// batches nor its makeable list carries an output-item slug or a started_ms (main.cpp only sends
// batch_id/recipe/ready_ms), so a batch's start time is derived here as ready_ms - the recipe's own known
// duration -- no route change, no new field, just what catalog.sql already fixes for every recipe.
var RECIPE_OUTPUT_ITEM = {
  charcoal:                "charcoal_black",
  shell_white:             "shell_white",
  yellow_ochre:            "yellow_ochre",
  red_ochre:               "red_ochre",
  gum_solution:            "binder",
  oak_gall_tannin_studio:  "tannin",
  carbon_ink:              "carbon_ink_black",
  iron_acetate:            "iron_acetate",
  iron_gall_ink_studio:    "iron_gall_blue_black",
  verdigris:               "verdigris_green",
  madder_lake:             "madder_red",
  weld_lake:               "weld_yellow",
  woad_blue:               "woad_blue"
};
var RECIPE_DURATION_MS = {
  charcoal:                28800000,
  shell_white:             86400000,
  yellow_ochre:            86400000,
  red_ochre:               14400000,
  gum_solution:            28800000,
  oak_gall_tannin_studio:  86400000,
  carbon_ink:              1800000,
  iron_acetate:            259200000,
  iron_gall_ink_studio:    43200000,
  verdigris:               604800000,
  madder_lake:             172800000,
  weld_lake:               86400000,
  woad_blue:               259200000
};

function benchIcon(recipeSlug) {
  var item = RECIPE_OUTPUT_ITEM[recipeSlug];
  if (!item) return null;
  var c = document.createElement("canvas");
  c.width = 18; c.height = 18;
  c.className = "bpIcon";
  var ctx = c.getContext("2d");
  ctx.imageSmoothingEnabled = false;
  if (typeof drawItemGlyph !== "function" || !drawItemGlyph(ctx, item, 0, 0, 18)) return null;
  return c;
}

function benchProgressPct(b) {
  var durMs = RECIPE_DURATION_MS[b.recipe];
  if (!durMs) return 100;
  var startedMs = b.ready_ms - durMs;
  var pct = (Date.now() - startedMs) / durMs * 100;
  return Math.max(0, Math.min(100, Math.round(pct)));
}

// ---- The bench ------------------------------------------------------------
function benchOpen() {
  var el = document.getElementById("bench");
  return !!(el && el.classList.contains("open"));
}

function benchCountdown(readyMs) {
  var left = readyMs - Date.now();
  if (left <= 0) return "Ready";
  var m = Math.ceil(left / 60000);
  if (m < 60) return m + " min";
  var h = Math.floor(m / 60);
  if (h < 24) return h + " h";
  return Math.floor(h / 24) + " d";
}

function benchCollect(batchId) {
  fetch(DAEMON + "/presence/collect", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ batch_id: batchId })
  }).then(function (res) { return res.json().then(function (j) { return { res: res, j: j }; }); })
    .then(function (got) {
      if (!got.res.ok) { showToast("Not ready yet."); return; }
      showToast(got.j.first_discovery ? "Discovered!" : "Collected.");
      netFetchStudio();
    }).catch(function () { showToast("Couldn't collect."); });
}

function benchStart(recipe) {
  fetch(DAEMON + "/presence/bench", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ recipe: recipe })
  }).then(function (res) { return res.json().then(function (j) { return { res: res, j: j }; }); })
    .then(function (got) {
      if (!got.res.ok) {
        var why = got.j && got.j.reason;
        showToast(why === "slots_full" ? "The bench is full." : why === "missing_inputs" ? "Not enough held." : "Couldn't start that.");
        return;
      }
      showToast("Started.");
      netFetchStudio();
    }).catch(function () { showToast("Couldn't start that."); });
}

// Up to 4 held slugs, duplicates allowed by clicking the same chip again (the daemon's own canonical
// key keeps duplicates and only ignores order). Cleared after every Try, same as a fresh side-mix
// attempt should be.
var benchMixSelected = [];

function benchMixToggle(slug) {
  var i = benchMixSelected.indexOf(slug);
  if (i >= 0) { benchMixSelected.splice(i, 1); renderBench(); return; }
  if (benchMixSelected.length >= 4) return;
  benchMixSelected.push(slug);
  renderBench();
}

function benchMixTry() {
  if (!benchMixSelected.length) return;
  fetch(DAEMON + "/presence/mix", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ slugs: benchMixSelected })
  }).then(function (res) { return res.json().then(function (j) { return { res: res, j: j }; }); })
    .then(function (got) {
      showToast(got.res.ok ? got.j.result : (got.j && got.j.reason === "too_fast" ? "Wait a moment." : "Nothing useful comes of it."));
      benchMixSelected = [];
      renderBench();
    }).catch(function () { showToast("Nothing useful comes of it."); benchMixSelected = []; renderBench(); });
}

function renderBench() {
  var batchesEl = document.getElementById("benchBatches");
  var recipesEl = document.getElementById("benchRecipes");
  var mixEl = document.getElementById("benchMix");
  var guestEl = document.getElementById("benchGuestNote");
  if (!batchesEl || !recipesEl || !mixEl || !guestEl) return;
  batchesEl.textContent = "";
  recipesEl.textContent = "";
  mixEl.textContent = "";
  guestEl.textContent = "";

  if (!isBound) {
    guestEl.textContent = "Sign in on LiquidGov to start a batch.";
    return;
  }

  var s = studioState;

  var batchesHead = document.createElement("p");
  batchesHead.className = "benchSection";
  batchesHead.textContent = "Open batches";
  batchesEl.appendChild(batchesHead);

  var batches = (s && s.batches) || [];
  if (!batches.length) {
    var none = document.createElement("p");
    none.className = "benchEmpty";
    none.textContent = "No batches started.";
    batchesEl.appendChild(none);
  }
  for (var i = 0; i < batches.length; i++) {
    (function (b) {
      var row = document.createElement("div");
      row.className = "benchRow";
      var icon = benchIcon(b.recipe);
      if (icon) row.appendChild(icon);
      var label = document.createElement("span");
      label.textContent = b.recipe;
      row.appendChild(label);
      var bar = document.createElement("div");
      bar.className = "benchProgress";
      var fill = document.createElement("i");
      fill.style.width = benchProgressPct(b) + "%";
      bar.appendChild(fill);
      row.appendChild(bar);
      var time = document.createElement("span");
      time.textContent = benchCountdown(b.ready_ms);
      row.appendChild(time);
      if (b.ready_ms <= Date.now()) {
        var btn = document.createElement("button");
        btn.type = "button";
        btn.textContent = "Collect";
        btn.addEventListener("click", function () { benchCollect(b.batch_id); });
        row.appendChild(btn);
      }
      batchesEl.appendChild(row);
    })(batches[i]);
  }

  var recipesHead = document.createElement("p");
  recipesHead.className = "benchSection";
  recipesHead.textContent = "You can make";
  recipesEl.appendChild(recipesHead);

  var makeable = (s && s.makeable) || [];
  if (!makeable.length) {
    var noneR = document.createElement("p");
    noneR.className = "benchEmpty";
    noneR.textContent = "Nothing makeable with what you hold.";
    recipesEl.appendChild(noneR);
  }
  for (var r = 0; r < makeable.length; r++) {
    (function (m) {
      var row = document.createElement("div");
      row.className = "benchRow";
      var icon = benchIcon(m.recipe);
      if (icon) row.appendChild(icon);
      var label = document.createElement("span");
      label.textContent = m.display_name;
      row.appendChild(label);
      var btn = document.createElement("button");
      btn.type = "button";
      btn.textContent = "Start";
      btn.addEventListener("click", function () { benchStart(m.recipe); });
      row.appendChild(btn);
      recipesEl.appendChild(row);
    })(makeable[r]);
  }

  var mixHead = document.createElement("p");
  mixHead.className = "benchSection";
  mixHead.textContent = "Try something";
  mixEl.appendChild(mixHead);

  var held = (s && s.inventory) || [];
  var chips = document.createElement("div");
  chips.className = "benchMixChips";
  for (var h = 0; h < held.length; h++) {
    (function (item) {
      if (item.qty <= 0) return;
      var chip = document.createElement("button");
      chip.type = "button";
      chip.className = "benchChip" + (benchMixSelected.indexOf(item.item) >= 0 ? " on" : "");
      chip.textContent = item.item;
      chip.addEventListener("click", function () { benchMixToggle(item.item); });
      chips.appendChild(chip);
    })(held[h]);
  }
  mixEl.appendChild(chips);
  var tryBtn = document.createElement("button");
  tryBtn.type = "button";
  tryBtn.id = "benchMixTry";
  tryBtn.textContent = "Try (" + benchMixSelected.length + "/4)";
  tryBtn.disabled = benchMixSelected.length === 0;
  tryBtn.addEventListener("click", benchMixTry);
  mixEl.appendChild(tryBtn);
}

function openBench() {
  var el = document.getElementById("bench");
  if (!el) return;
  el.hidden = false;
  el.classList.add("open");
  if (el.tabIndex < 0) el.tabIndex = -1;
  if (el.focus) el.focus();
  benchMixSelected = [];
  renderBench();
}

function closeBench() {
  var el = document.getElementById("bench");
  if (!el) return;
  el.classList.remove("open");
  el.hidden = true;
}

// ---- The notebook -----------------------------------------------------
function notebookOpen() {
  var el = document.getElementById("notebook");
  return !!(el && el.classList.contains("open"));
}

function renderNotebook() {
  var el = document.getElementById("notebookLines");
  var guestEl = document.getElementById("notebookGuestNote");
  if (!el || !guestEl) return;
  el.textContent = "";
  guestEl.textContent = "";
  if (!isBound) {
    guestEl.textContent = "Sign in on LiquidGov to keep a notebook.";
    return;
  }
  var s = studioState;
  var discoveries = (s && s.discoveries) || [];
  for (var i = 0; i < discoveries.length; i++) {
    var d = discoveries[i];
    var line = document.createElement("p");
    line.className = "notebookLine";
    line.textContent = d.display_name + " -- " + d.real_range_note + " " + d.history_note;
    el.appendChild(line);
  }
  var undiscovered = Math.max(0, (s && s.recipe_count || 0) - discoveries.length);
  for (var u = 0; u < undiscovered; u++) {
    var q = document.createElement("p");
    q.className = "notebookLine notebookUnknown";
    q.textContent = "?";
    el.appendChild(q);
  }
}

function openNotebook() {
  var el = document.getElementById("notebook");
  if (!el) return;
  el.hidden = false;
  el.classList.add("open");
  if (el.tabIndex < 0) el.tabIndex = -1;
  if (el.focus) el.focus();
  renderNotebook();
}

function closeNotebook() {
  var el = document.getElementById("notebook");
  if (!el) return;
  el.classList.remove("open");
  el.hidden = true;
}

(function wireStudioOverlays() {
  var benchClose = document.getElementById("benchClose");
  if (benchClose) benchClose.addEventListener("click", closeBench);
  var notebookClose = document.getElementById("notebookClose");
  if (notebookClose) notebookClose.addEventListener("click", closeNotebook);
  window.addEventListener("keydown", function (ev) {
    if (ev.key !== "Escape") return;
    if (benchOpen()) { closeBench(); ev.preventDefault(); }
    else if (notebookOpen()) { closeNotebook(); ev.preventDefault(); }
  });
})();
