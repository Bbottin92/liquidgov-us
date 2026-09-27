"use strict";
// The easel (BUILD studio 2, studio.md). Charcoal only. A bound player saves
// palette indices to the daemon. A guest keeps a session stub in this page and
// never calls the painting endpoints. The wall bitmap is separate from the
// editor, so looking at a friend's portrait cannot become a save of your own.
var EASEL_N = 32;
var EASEL_PAPER = [239, 230, 212, 255];
// BUILD studio 4 (CLAUDE_STUDIO_4.md decision 1): the fixed index table, same numbers as the daemon's
// PIGMENTS[] in commons-daemon/src/studio.cpp. Never a per-player map -- a visited bitmap has to mean
// the same hex in every house. Indices 1-4 are exactly the old CHARCOAL table; a stored portrait's
// bytes still mean the same thing they always did.
var STUDIO_PIGMENTS = [
  { index: 1, hex: "#1a1a1a", name: "Black" },
  { index: 2, hex: "#4d4d4d", name: "Grey" },
  { index: 3, hex: "#7a7a7a", name: "Light" },
  { index: 4, hex: "#adadad", name: "Pale" },
  { index: 5, hex: "#f2ede4", name: "Shell white" },
  { index: 6, hex: "#c9963a", name: "Yellow ochre" },
  { index: 7, hex: "#9c4a2e", name: "Red ochre" },
  { index: 8, hex: "#0d0d0d", name: "Carbon ink" },
  { index: 9, hex: "#1c1c24", name: "Iron-gall ink" },
  { index: 10, hex: "#3f8f5f", name: "Verdigris" },
  { index: 11, hex: "#a72a2a", name: "Madder lake" },
  { index: 12, hex: "#e0c04a", name: "Weld lake" },
  { index: 13, hex: "#2e4c72", name: "Woad blue" }
];
// A guest never fetches, so their palette is always exactly the four charcoal indices -- the same
// slice of the table decision 1 already reserves for "always legal, never discovery-gated".
var GUEST_PALETTE = STUDIO_PIGMENTS.slice(0, 4);
// 4x4 Bayer. A smudge stroke dithers the chosen charcoal against the next
// lighter grey when that grey is actually held, otherwise against bare paper,
// which is how a stub of black reads as a grey at all.
var BAYER4 = [
  [0, 8, 2, 10],
  [12, 4, 14, 6],
  [3, 11, 1, 9],
  [15, 7, 13, 5]
];
var easelTool = "pen";
var easelColour = 1;
var easelBuf = new Uint8Array(EASEL_N * EASEL_N);
var easelUndo = [];
var easelDrawing = false;
var easelBound = false;
var easelGen = 0;
var guestLeft = { 1: 400, 2: 0, 3: 0, 4: 0 };
var guestBuf = new Uint8Array(EASEL_N * EASEL_N);
var easelHave = { 1: 0, 2: 0, 3: 0, 4: 0 };
// BUILD studio 4: which pigments the own-GET/POST actually offered (index order), the swatch source
// per decision 4 -- not the fixed STUDIO_PIGMENTS table, which exists for rendering ANY stored byte
// (including a friend's undiscovered pigment) and never for what a save may legally paint with.
var easelPalette = GUEST_PALETTE;
// BUILD studio 4: which canvas is open ("portrait" or "painting") -- decision 4, "where you opened it
// is the canvas." Set by openEasel(kind); read by the save handler and the wall-buffer picker below.
var easelKind = "portrait";

function easelOpen() {
  var el = document.getElementById("easel");
  return !!(el && el.classList.contains("open"));
}

function budget() { return easelBound ? easelHave : guestLeft; }

// A shallow copy of whatever keys are actually present -- not hardcoded to four, now that a bound
// player's budget can hold up to thirteen.
function copyLeft(src) {
  var out = {};
  for (var k in src) if (Object.prototype.hasOwnProperty.call(src, k)) out[k] = src[k];
  return out;
}

function easelHex(index) {
  for (var i = 0; i < STUDIO_PIGMENTS.length; i++) if (STUDIO_PIGMENTS[i].index === index) return STUDIO_PIGMENTS[i].hex;
  return null;
}

// Button labels: 1-4 stay Black/Grey/Light/Pale, the rest use the item's own display name
// (CLAUDE_STUDIO_4.md decision 4).
function pigmentName(index) {
  for (var i = 0; i < STUDIO_PIGMENTS.length; i++) if (STUDIO_PIGMENTS[i].index === index) return STUDIO_PIGMENTS[i].name;
  return "?";
}

function easelRgb(hex) {
  return [parseInt(hex.slice(1, 3), 16), parseInt(hex.slice(3, 5), 16), parseInt(hex.slice(5, 7), 16), 255];
}

function fillIndexed(ctx, src) {
  var img = ctx.createImageData(EASEL_N, EASEL_N);
  for (var i = 0; i < src.length; i++) {
    // BUILD studio 4: an index the client table does not know (14 and up -- never legal to save, but a
    // stray byte could still exist) draws as paper, not black. The old fallback ("#1a1a1a" when
    // easelHex found nothing) was fine while 1-4 were the only legal indices; it is wrong now.
    var hex = src[i] ? easelHex(src[i]) : null;
    var rgb = hex ? easelRgb(hex) : EASEL_PAPER;
    img.data[i * 4] = rgb[0];
    img.data[i * 4 + 1] = rgb[1];
    img.data[i * 4 + 2] = rgb[2];
    img.data[i * 4 + 3] = rgb[3];
  }
  ctx.putImageData(img, 0, 0);
}

function paintEaselCanvas() {
  var canvas = document.getElementById("easelCanvas");
  if (!canvas) return;
  fillIndexed(canvas.getContext("2d"), easelBuf);
}

// BUILD studio 4: the two wall bitmaps are never the same object (CLAUDE_STUDIO_4.md decision 2:
// "Do not reuse the portrait's wall buffer") -- each canvas keeps its own buf/shown/gen/frame, and
// wallStateFor(kind) is the only place that picks between them.
var portraitWall = { buf: new Uint8Array(EASEL_N * EASEL_N), shown: "", gen: 0, frame: null };
var paintingWall = { buf: new Uint8Array(EASEL_N * EASEL_N), shown: "", gen: 0, frame: null };
function wallStateFor(kind) { return kind === "painting" ? paintingWall : portraitWall; }

function paintWallState(state) {
  if (!state.frame) {
    state.frame = document.createElement("canvas");
    state.frame.width = EASEL_N;
    state.frame.height = EASEL_N;
  }
  fillIndexed(state.frame.getContext("2d"), state.buf);
  if (typeof forceRedraw !== "undefined") forceRedraw = true;
}

// Fetches either canvas's wall bitmap: the owner's own (mode "house") or a visited one (mode "visit",
// using visitingOwner). A 404 (no row yet, or a friend who never opened that canvas) just leaves the
// buffer at its default all-zero blank -- same as before, now per kind.
function loadStudioWall(kind) {
  var state = wallStateFor(kind);
  var key = mode === "visit" ? ("visit:" + (visitingOwner || "")) : (mode === "house" && isBound ? "self" : "");
  if (state.shown === key) return;
  state.shown = key;
  if (!key) return;
  var url = key === "self"
    ? DAEMON + "/presence/painting/" + kind
    : DAEMON + "/presence/painting/" + encodeURIComponent(visitingOwner) + "/" + kind;
  var gen = ++state.gen;
  fetch(url).then(function (res) { return res.ok ? res.json() : null; }).then(function (j) {
    if (gen !== state.gen || !j || !j.pixels) return;
    readPixels(state.buf, j.pixels);
    paintWallState(state);
  }).catch(function () {});
}

function drawStudioPortrait(camX, camY) {
  if (mode !== "house" && mode !== "visit") return;
  if (typeof STUDIO_PORTRAIT === "undefined" || typeof TILE === "undefined") return;
  loadStudioWall("portrait");
  var state = portraitWall;
  if (!state.frame) paintWallState(state);
  var x = STUDIO_PORTRAIT.x * TILE - camX;
  var y = STUDIO_PORTRAIT.y * TILE - camY;
  var prev = ctx.imageSmoothingEnabled;
  ctx.imageSmoothingEnabled = false;
  ctx.drawImage(state.frame, x, y, STUDIO_PORTRAIT.w * TILE, STUDIO_PORTRAIT.h * TILE);
  ctx.imageSmoothingEnabled = prev;
  if (typeof drawNameplate === "function") {
    drawNameplate(ctx, "a small portrait", x + TILE, y + STUDIO_PORTRAIT.h * TILE + 8);
  }
}

// BUILD studio 4 (CLAUDE_STUDIO_4.md decision 2): the second frame, drawn from its own bitmap, never
// the portrait's. Same nameplate call the portrait uses.
function drawStudioPainting(camX, camY) {
  if (mode !== "house" && mode !== "visit") return;
  if (typeof STUDIO_PAINTING === "undefined" || typeof TILE === "undefined") return;
  loadStudioWall("painting");
  var state = paintingWall;
  if (!state.frame) paintWallState(state);
  var x = STUDIO_PAINTING.x * TILE - camX;
  var y = STUDIO_PAINTING.y * TILE - camY;
  var prev = ctx.imageSmoothingEnabled;
  ctx.imageSmoothingEnabled = false;
  ctx.drawImage(state.frame, x, y, STUDIO_PAINTING.w * TILE, STUDIO_PAINTING.h * TILE);
  ctx.imageSmoothingEnabled = prev;
  if (typeof drawNameplate === "function") {
    drawNameplate(ctx, "a small painting", x + TILE, y + STUDIO_PAINTING.h * TILE + 8);
  }
}

function easelNote(text) {
  var n = document.getElementById("easelNote");
  if (n) n.textContent = text || "";
}

function easelBudgetNote() {
  if (!easelBound) {
    var n0 = budget()[1] || 0;
    easelNote(n0 ? ("This visit only. Black left: " + n0 + ".") : "No black left. Sign in on LiquidGov to keep a portrait.");
    return;
  }
  // BUILD studio 4: "names pixels of the selected colour, not 'Charcoal left', once the open palette
  // can hold more than charcoal" -- while it's still charcoal-only, keep the original collective line.
  if (easelPalette.length <= 4) {
    var n = budget()[1] || 0;
    easelNote(n ? ("Charcoal left: about " + n + " pixels.") : "No charcoal left.");
    return;
  }
  var n2 = budget()[easelColour] || 0;
  var name = pigmentName(easelColour);
  easelNote(n2 ? (name + " left: about " + n2 + " pixels.") : ("No more " + name.toLowerCase() + "."));
}

function renderEaselPalette() {
  var box = document.getElementById("easelPalette");
  if (!box) return;
  var left = budget();
  box.textContent = "";
  // BUILD studio 4 (decision 4): swatches come from the own-GET/POST palette, in index order -- not
  // the fixed STUDIO_PIGMENTS table, which would show every undiscovered pigment too.
  var list = easelBound ? easelPalette : GUEST_PALETTE;
  for (var i = 0; i < list.length; i++) {
    var idx = list[i].index;
    var hex = easelHex(idx) || "#1a1a1a";
    var name = pigmentName(idx);
    var b = document.createElement("button");
    b.type = "button";
    b.className = "swatch" + (easelColour === idx ? " on" : "");
    b.disabled = (left[idx] || 0) <= 0;
    b.innerHTML = "<i style=\"background:" + hex + "\"></i>" + name;
    b.setAttribute("data-index", String(idx));
    b.addEventListener("click", function (ev) {
      var idx2 = Number(ev.currentTarget.getAttribute("data-index"));
      if ((budget()[idx2] || 0) <= 0) return;
      easelColour = idx2;
      renderEaselPalette();
    });
    box.appendChild(b);
  }
}

function qtyToPixels(list) {
  var qty = {};
  if (!list) return qty;
  for (var i = 0; i < list.length; i++) qty[list[i].index] = (list[i].qty || 0) * 200;
  return qty;
}

function readPixels(dst, src) {
  for (var i = 0; i < dst.length; i++) dst[i] = src && src.length === dst.length ? (src[i] & 255) : 0;
}

function fillEditor(j) {
  easelBuf = new Uint8Array(EASEL_N * EASEL_N);
  readPixels(easelBuf, j && j.pixels);
  easelUndo = [];
  easelBound = true;
  easelPalette = (j && j.palette) || [];
  easelHave = qtyToPixels(easelPalette);
  var save = document.getElementById("easelSave");
  if (save) save.textContent = "Save";
  renderEaselPalette();
  paintEaselCanvas();
  easelBudgetNote();
}

function openEasel(kind) {
  // BUILD studio 4 (decision 4): where you opened it is the canvas. openEasel() with no argument
  // (the workshop table's own call site) still means the portrait, same as every slice-2 call site.
  easelKind = kind === "painting" ? "painting" : "portrait";
  var titleEl = document.getElementById("easelTitle");
  if (titleEl) titleEl.textContent = easelKind === "painting" ? "A small painting" : "A small portrait";
  var el = document.getElementById("easel");
  if (!el) return;
  var gen = ++easelGen;
  el.hidden = false;
  el.classList.add("open");
  if (el.tabIndex < 0) el.tabIndex = -1;
  if (el.focus) el.focus();
  easelSetTool("pen");
  if (isBound) {
    fetch(DAEMON + "/presence/painting/" + easelKind).then(function (res) {
      if (!res.ok) throw new Error("no");
      return res.json();
    }).then(function (j) {
      if (gen !== easelGen || !easelOpen()) return;
      fillEditor(j);
    }).catch(function () {
      if (gen !== easelGen || !easelOpen()) return;
      easelNote("Could not open your " + easelKind + ".");
    });
  } else {
    // BUILD studio 4: guests never enter painting mode -- they have no own house, so the (4,0)
    // interactable is never reachable as its owner. This branch stays exactly the portrait it always was.
    easelBound = false;
    easelPalette = GUEST_PALETTE;
    easelBuf = new Uint8Array(guestBuf);
    easelUndo = [];
    var save = document.getElementById("easelSave");
    if (save) save.textContent = "Keep";
    renderEaselPalette();
    paintEaselCanvas();
    easelBudgetNote();
  }
}

function closeEasel() {
  var el = document.getElementById("easel");
  if (!el) return;
  easelGen++;
  easelDrawing = false;
  if (!easelBound) guestBuf = new Uint8Array(easelBuf);
  el.classList.remove("open");
  el.hidden = true;
}

function easelCell(ev) {
  var canvas = document.getElementById("easelCanvas");
  var rect = canvas.getBoundingClientRect();
  var x = Math.floor((ev.clientX - rect.left) / rect.width * EASEL_N);
  var y = Math.floor((ev.clientY - rect.top) / rect.height * EASEL_N);
  if (x < 0 || y < 0 || x >= EASEL_N || y >= EASEL_N) return null;
  return { i: y * EASEL_N + x, x: x, y: y };
}

function easelPut(i, value) {
  if (i == null || i < 0 || i >= easelBuf.length) return false;
  if (easelBuf[i] === value) return true;
  if (value !== 0) {
    var left = budget();
    if ((left[value] || 0) <= 0) { easelNote("No more of that."); return false; }
    left[value] = left[value] - 1;
  }
  // Erasing, or covering a pixel with a different colour, does not put the old
  // colour back. Same rule as the server: nothing is refunded.
  easelBuf[i] = value;
  return true;
}

function smudgeValue(x, y, colour) {
  var t = BAYER4[y & 3][x & 3];
  var lighter = 0;
  if (colour >= 1 && colour < 4 && (budget()[colour + 1] || 0) > 0) lighter = colour + 1;
  return t < 8 ? colour : lighter;
}

function easelStamp(cell) {
  if (easelTool === "erase") return easelPut(cell.i, 0);
  var value = easelColour;
  // BUILD studio 4 (decision 4): "smudge stays the dither among charcoal 1-4. A selected index of 5 or
  // more stamps solid, same as the pen." smudgeValue's own dither logic already only lightens 1-3, but
  // without this guard a lake or an ink selected with the smudge tool would still run through it and
  // come out half-erased on every dark cell of the Bayer pattern (its `lighter` stays 0 for colour>=4).
  if (easelTool === "smudge" && easelColour < 5) value = smudgeValue(cell.x, cell.y, easelColour);
  return easelPut(cell.i, value);
}

function easelPointer(ev) {
  if (!easelDrawing) return;
  var cell = easelCell(ev);
  if (!cell) return;
  if (easelStamp(cell)) paintEaselCanvas();
  ev.preventDefault();
}

function easelSetTool(name) {
  easelTool = name;
  var tools = document.getElementById("easelTools");
  if (!tools) return;
  var buttons = tools.children;
  for (var i = 0; i < buttons.length; i++) {
    var tool = buttons[i].getAttribute && buttons[i].getAttribute("data-tool");
    if (tool) buttons[i].classList.toggle("on", tool === name);
  }
}

function easelUndoStroke() {
  var prev = easelUndo.pop();
  if (!prev) return;
  easelBuf = prev.buf;
  // Whichever module var budget() reads from gets replaced wholesale -- simpler and equally correct
  // now that a snapshot can hold up to thirteen keys instead of a fixed four.
  if (easelBound) easelHave = prev.left; else guestLeft = prev.left;
  paintEaselCanvas();
  renderEaselPalette();
  easelBudgetNote();
}

(function wireEasel() {
  var canvas = document.getElementById("easelCanvas");
  var el = document.getElementById("easel");
  if (!canvas || !el) return;
  canvas.addEventListener("pointerdown", function (ev) {
    easelUndo.push({ buf: new Uint8Array(easelBuf), left: copyLeft(budget()) });
    if (easelUndo.length > 16) easelUndo.shift();
    easelDrawing = true;
    if (canvas.setPointerCapture) canvas.setPointerCapture(ev.pointerId);
    easelPointer(ev);
  });
  canvas.addEventListener("pointermove", easelPointer);
  function endStroke() {
    if (!easelDrawing) return;
    easelDrawing = false;
    renderEaselPalette();
    easelBudgetNote();
  }
  canvas.addEventListener("pointerup", endStroke);
  canvas.addEventListener("pointercancel", endStroke);
  var tools = document.getElementById("easelTools");
  if (tools) tools.addEventListener("click", function (ev) {
    var t = ev.target.getAttribute && ev.target.getAttribute("data-tool");
    if (!t && ev.target.parentNode) t = ev.target.parentNode.getAttribute && ev.target.parentNode.getAttribute("data-tool");
    if (t) easelSetTool(t);
  });
  var undo = document.getElementById("easelUndo");
  if (undo) undo.addEventListener("click", easelUndoStroke);
  var save = document.getElementById("easelSave");
  if (save) save.addEventListener("click", function () {
    if (!easelBound) { closeEasel(); return; }
    var body = { pixels: Array.prototype.slice.call(easelBuf) };
    fetch(DAEMON + "/presence/painting/" + easelKind, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body)
    }).then(function (res) { return res.json().then(function (j) { return { res: res, j: j }; }); }).then(function (got) {
      if (!got.res.ok) {
        var why = got.j && got.j.reason;
        easelNote(why === "insufficient" ? "Not enough of that." : why === "too_fast" ? "Wait a moment." : "Not saved.");
        return;
      }
      if (got.j && got.j.palette) { easelPalette = got.j.palette; easelHave = qtyToPixels(got.j.palette); }
      // BUILD studio 4: write into THIS canvas's own wall buffer, never the other one's.
      var wall = wallStateFor(easelKind);
      readPixels(wall.buf, easelBuf);
      if (mode === "house") wall.shown = "self";
      paintWallState(wall);
      renderEaselPalette();
      easelNote("Saved.");
    }).catch(function () { easelNote("Not saved."); });
  });
  var close = document.getElementById("easelClose");
  if (close) close.addEventListener("click", closeEasel);
  window.addEventListener("keydown", function (ev) {
    if (!easelOpen()) return;
    if (ev.key === "Escape") { closeEasel(); ev.preventDefault(); }
    else if (ev.key === "e" || ev.key === "E") { easelSetTool("erase"); ev.preventDefault(); }
    else if (ev.key === "m" || ev.key === "M") { easelSetTool("smudge"); ev.preventDefault(); }
    else if (ev.key === "b" || ev.key === "B" || ev.key === "p" || ev.key === "P") { easelSetTool("pen"); ev.preventDefault(); }
    else if ((ev.key === "z" || ev.key === "Z") && !ev.ctrlKey && !ev.metaKey) { easelUndoStroke(); ev.preventDefault(); }
    else if (ev.key >= "1" && ev.key <= "4") {
      var idx = Number(ev.key);
      if ((budget()[idx] || 0) > 0) { easelColour = idx; renderEaselPalette(); }
      ev.preventDefault();
    }
  });
})();
