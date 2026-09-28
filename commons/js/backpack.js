"use strict";
// The backpack (BUILD backpack, CLAUDE_BACKPACK.md): a fuller view of the same `inventory` object the
// strip (#invHud) already reads -- not a second inventory, no new state, no fetch, no route. The strip
// stays exactly as it is; this is a second, bigger view over the same data, opened by the Bag button
// next to it or the `I` key. Also owns the one shared tooltip bubble (#itemTip), used by both the strip
// and this grid -- one DOM node, not one per item (decision 4).

// Sort bucket order for the grid (decision 5: "sorted by category then alphabetically" -- not insertion
// order, which is what the strip does and reads as random past a handful of items). Every slug lands in
// exactly one bucket: the ~24 generic-square items via ITEM_CATEGORY (interiors.js), the handful with
// their own bespoke atlas art via the checks below, mirroring sprites.js's own paintItemAtlas() branches.
var CATEGORY_ORDER = ["twig", "leaf", "stone", "powder", "bundle", "swatch", "jar", "ink", "shell", "other"];

function categoryOf(slug) {
  if (ITEM_CATEGORY[slug]) return ITEM_CATEGORY[slug];
  if (slug === "copper_acetate_bottle" || slug === "vinegar_jar" || slug === "vinegar") return "jar";
  if (slug === "iron_gall_ink_page" || slug === "lampblack_ink" || slug === "carbon_ink_black" || slug === "iron_gall_blue_black") return "ink";
  if (slug === "seashell") return "shell";
  if (slug === "oak_gall" || slug === "oak_gall_tannin" || slug === "gum_arabic") return "bundle";
  if (slug === "alum_crystal") return "stone";
  if (slug === "chalk_block") return "powder";
  return "other";
}

function backpackOpen() {
  var el = document.getElementById("backpack");
  return !!(el && el.classList.contains("open"));
}

function openBackpack() {
  var el = document.getElementById("backpack");
  if (!el) return;
  el.hidden = false;
  el.classList.add("open");
  if (el.tabIndex < 0) el.tabIndex = -1;
  if (el.focus) el.focus();
  renderBackpack();
}

function closeBackpack() {
  var el = document.getElementById("backpack");
  if (!el) return;
  el.classList.remove("open");
  el.hidden = true;
  hideItemTip();
}

function backpackChip(slug) {
  var info = PLACED_ITEM_INFO[slug] || { color: "#888888", label: slug };
  var b = document.createElement("button");
  b.type = "button";
  b.className = "bpChip" + (slug === lastPickedItem ? " held" : "");
  b.setAttribute("data-item", slug);
  var sw = document.createElement("canvas");
  sw.width = 24; sw.height = 24;
  var sctx = sw.getContext("2d");
  sctx.imageSmoothingEnabled = false;
  if (typeof drawItemGlyph !== "function" || !drawItemGlyph(sctx, slug, 0, 0, 24)) {
    sctx.fillStyle = info.color;
    sctx.fillRect(0, 0, 24, 24);
  }
  var lab = document.createElement("span");
  lab.textContent = info.label;
  var qty = document.createElement("em");
  qty.textContent = "x" + inventory[slug];
  b.appendChild(sw); b.appendChild(lab); b.appendChild(qty);
  return b;
}

function renderBackpack() {
  var grid = document.getElementById("backpackGrid");
  var empty = document.getElementById("backpackEmpty");
  if (!grid || !empty) return;
  grid.textContent = "";
  var keys = [];
  for (var k in inventory) if (Object.prototype.hasOwnProperty.call(inventory, k) && inventory[k] > 0) keys.push(k);
  empty.textContent = keys.length ? "" : "Nothing held yet.";
  keys.sort(function (a, b) {
    var ca = CATEGORY_ORDER.indexOf(categoryOf(a)), cb = CATEGORY_ORDER.indexOf(categoryOf(b));
    if (ca !== cb) return ca - cb;
    var la = (PLACED_ITEM_INFO[a] || {}).label || a, lb = (PLACED_ITEM_INFO[b] || {}).label || b;
    return la < lb ? -1 : la > lb ? 1 : 0;
  });
  for (var i = 0; i < keys.length; i++) grid.appendChild(backpackChip(keys[i]));
}

// ---- the shared tooltip bubble (decision 4) -- one #itemTip node, reused by the strip and this grid.
function itemTipText(slug) {
  return ITEM_DESC[slug] || (PLACED_ITEM_INFO[slug] && PLACED_ITEM_INFO[slug].label) || slug;
}

var itemTipSlug = null;

function showItemTip(slug, anchorEl) {
  var tip = document.getElementById("itemTip");
  if (!tip || !anchorEl) return;
  tip.textContent = itemTipText(slug);
  tip.hidden = false;
  itemTipSlug = slug;
  var r = anchorEl.getBoundingClientRect();
  var tw = tip.offsetWidth, th = tip.offsetHeight;
  var left = r.left + r.width / 2 - tw / 2;
  left = Math.max(6, Math.min(left, window.innerWidth - tw - 6));
  var top = r.top - th - 8;
  if (top < 6) top = r.bottom + 8;
  tip.style.left = Math.round(left) + "px";
  tip.style.top = Math.round(top) + "px";
}

function hideItemTip() {
  var tip = document.getElementById("itemTip");
  if (tip) tip.hidden = true;
  itemTipSlug = null;
}

function findItemChip(el, root) {
  var t = el;
  while (t && t !== root && !(t.getAttribute && t.getAttribute("data-item"))) t = t.parentNode;
  return (t && t !== root) ? t : null;
}

// pointerover/pointerout for a mouse (desktop hover, no separate per-item listeners); click/tap covers
// touch, which has no hover, and doubles as a toggle -- tapping the same chip again hides it.
function wireItemTips(containerId) {
  var el = document.getElementById(containerId);
  if (!el || !el.addEventListener) return;
  el.addEventListener("pointerover", function (e) {
    var chip = findItemChip(e.target, el);
    if (chip) showItemTip(chip.getAttribute("data-item"), chip);
  });
  el.addEventListener("pointerout", function (e) {
    if (findItemChip(e.target, el)) hideItemTip();
  });
  el.addEventListener("click", function (e) {
    var chip = findItemChip(e.target, el);
    if (!chip) return;
    var slug = chip.getAttribute("data-item");
    var tip = document.getElementById("itemTip");
    if (tip && !tip.hidden && itemTipSlug === slug) { hideItemTip(); return; }
    showItemTip(slug, chip);
  });
}
wireItemTips("invHud");
wireItemTips("backpackGrid");

(function wireBackpack() {
  document.addEventListener("click", function (e) {
    var tip = document.getElementById("itemTip");
    if (!tip || tip.hidden) return;
    var overChip = e.target && e.target.closest ? e.target.closest("[data-item]") : null;
    if (!overChip) hideItemTip();
  });

  var grid = document.getElementById("backpackGrid");
  if (grid && grid.addEventListener) {
    grid.addEventListener("click", function (e) {
      var t = e.target;
      while (t && t !== grid && !(t.getAttribute && t.getAttribute("data-item"))) t = t.parentNode;
      if (!t || t === grid) return;
      lastPickedItem = t.getAttribute("data-item");
      renderInvHud();
      renderBackpack();
    });
  }

  var closeBtn = document.getElementById("backpackClose");
  if (closeBtn) closeBtn.addEventListener("click", closeBackpack);

  var bagBtn = document.getElementById("bagBtn");
  if (bagBtn) bagBtn.addEventListener("click", function () {
    if (backpackOpen()) closeBackpack(); else openBackpack();
  });

  // Its own keydown listener, same shape as studio.js's wireStudioOverlays() -- Escape and `I` are not
  // handled by input.js's movement listener at all, so there is nothing to gate behind typingInField()
  // here (that function instead gained a #backpack.open check, so IT suppresses movement while this is
  // up -- see input.js).
  window.addEventListener("keydown", function (ev) {
    if (ev.key === "Escape" && backpackOpen()) { closeBackpack(); ev.preventDefault(); return; }
    if (ev.key !== "i" && ev.key !== "I") return;
    var t = ev.target;
    if (t && (t.tagName === "INPUT" || t.tagName === "TEXTAREA")) return;
    if (backpackOpen()) { closeBackpack(); ev.preventDefault(); return; }
    if (document.getElementById("nameOverlay").style.display !== "none") return;
    var easel = document.getElementById("easel");
    if (easel && easel.classList.contains("open")) return;
    var bench = document.getElementById("bench");
    if (bench && bench.classList.contains("open")) return;
    var notebook = document.getElementById("notebook");
    if (notebook && notebook.classList.contains("open")) return;
    openBackpack();
    ev.preventDefault();
  });
})();
