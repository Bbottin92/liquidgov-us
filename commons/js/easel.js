"use strict";
// The easel (BUILD studio 2, studio.md). Charcoal only. A bound player saves
// palette indices to the daemon. A guest keeps a session stub in this page and
// never calls the painting endpoints. The wall bitmap is separate from the
// editor, so looking at a friend's portrait cannot become a save of your own.
var EASEL_N = 32;
var EASEL_PAPER = [239, 230, 212, 255];
var CHARCOAL = [
  { index: 1, hex: "#1a1a1a", name: "Black" },
  { index: 2, hex: "#4d4d4d", name: "Grey" },
  { index: 3, hex: "#7a7a7a", name: "Light" },
  { index: 4, hex: "#adadad", name: "Pale" }
];
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
var wallBuf = new Uint8Array(EASEL_N * EASEL_N);
var wallShown = "";
var wallGen = 0;
var studioFrame = null;

function easelOpen() {
  var el = document.getElementById("easel");
  return !!(el && el.classList.contains("open"));
}

function budget() { return easelBound ? easelHave : guestLeft; }

function copyLeft(src) { return { 1: src[1] || 0, 2: src[2] || 0, 3: src[3] || 0, 4: src[4] || 0 }; }

function easelHex(index) {
  for (var i = 0; i < CHARCOAL.length; i++) if (CHARCOAL[i].index === index) return CHARCOAL[i].hex;
  return null;
}

function easelRgb(hex) {
  return [parseInt(hex.slice(1, 3), 16), parseInt(hex.slice(3, 5), 16), parseInt(hex.slice(5, 7), 16), 255];
}

function fillIndexed(ctx, src) {
  var img = ctx.createImageData(EASEL_N, EASEL_N);
  for (var i = 0; i < src.length; i++) {
    var rgb = src[i] ? easelRgb(easelHex(src[i]) || "#1a1a1a") : EASEL_PAPER;
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

function paintWall() {
  if (!studioFrame) {
    studioFrame = document.createElement("canvas");
    studioFrame.width = EASEL_N;
    studioFrame.height = EASEL_N;
  }
  fillIndexed(studioFrame.getContext("2d"), wallBuf);
  if (typeof forceRedraw !== "undefined") forceRedraw = true;
}

function drawStudioPortrait(camX, camY) {
  if (mode !== "house" && mode !== "visit") return;
  if (typeof STUDIO_PORTRAIT === "undefined" || typeof TILE === "undefined") return;
  loadStudioPortrait();
  if (!studioFrame) paintWall();
  var x = STUDIO_PORTRAIT.x * TILE - camX;
  var y = STUDIO_PORTRAIT.y * TILE - camY;
  var prev = ctx.imageSmoothingEnabled;
  ctx.imageSmoothingEnabled = false;
  ctx.drawImage(studioFrame, x, y, STUDIO_PORTRAIT.w * TILE, STUDIO_PORTRAIT.h * TILE);
  ctx.imageSmoothingEnabled = prev;
  if (typeof drawNameplate === "function") {
    drawNameplate(ctx, "a small portrait", x + TILE, y + STUDIO_PORTRAIT.h * TILE + 8);
  }
}

function easelNote(text) {
  var n = document.getElementById("easelNote");
  if (n) n.textContent = text || "";
}

function easelBudgetNote() {
  var n = budget()[1] || 0;
  if (!easelBound) {
    easelNote(n ? ("This visit only. Black left: " + n + ".") : "No black left. Sign in on LiquidGov to keep a portrait.");
  } else {
    easelNote(n ? ("Charcoal left: about " + n + " pixels.") : "No charcoal left.");
  }
}

function renderEaselPalette() {
  var box = document.getElementById("easelPalette");
  if (!box) return;
  var left = budget();
  box.textContent = "";
  for (var i = 0; i < CHARCOAL.length; i++) {
    var c = CHARCOAL[i];
    var b = document.createElement("button");
    b.type = "button";
    b.className = "swatch" + (easelColour === c.index ? " on" : "");
    b.disabled = (left[c.index] || 0) <= 0;
    b.innerHTML = "<i style=\"background:" + c.hex + "\"></i>" + c.name;
    b.setAttribute("data-index", String(c.index));
    b.addEventListener("click", function (ev) {
      var idx = Number(ev.currentTarget.getAttribute("data-index"));
      if ((budget()[idx] || 0) <= 0) return;
      easelColour = idx;
      renderEaselPalette();
    });
    box.appendChild(b);
  }
}

function qtyToPixels(list) {
  var qty = { 1: 0, 2: 0, 3: 0, 4: 0 };
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
  easelHave = qtyToPixels(j && j.palette);
  var save = document.getElementById("easelSave");
  if (save) save.textContent = "Save";
  renderEaselPalette();
  paintEaselCanvas();
  easelBudgetNote();
}

function loadStudioPortrait() {
  var key = mode === "visit" ? ("visit:" + (visitingOwner || "")) : (mode === "house" && isBound ? "self" : "");
  if (wallShown === key) return;
  wallShown = key;
  if (!key) return;
  var url = key === "self"
    ? DAEMON + "/presence/painting/portrait"
    : DAEMON + "/presence/painting/" + encodeURIComponent(visitingOwner) + "/portrait";
  var gen = ++wallGen;
  fetch(url).then(function (res) { return res.ok ? res.json() : null; }).then(function (j) {
    if (gen !== wallGen || !j || !j.pixels) return;
    readPixels(wallBuf, j.pixels);
    paintWall();
  }).catch(function () {});
}

function openEasel() {
  var el = document.getElementById("easel");
  if (!el) return;
  var gen = ++easelGen;
  el.hidden = false;
  el.classList.add("open");
  if (el.tabIndex < 0) el.tabIndex = -1;
  if (el.focus) el.focus();
  easelSetTool("pen");
  if (isBound) {
    fetch(DAEMON + "/presence/painting/portrait").then(function (res) {
      if (!res.ok) throw new Error("no");
      return res.json();
    }).then(function (j) {
      if (gen !== easelGen || !easelOpen()) return;
      fillEditor(j);
    }).catch(function () {
      if (gen !== easelGen || !easelOpen()) return;
      easelNote("Could not open your portrait.");
    });
  } else {
    easelBound = false;
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
    if ((left[value] || 0) <= 0) { easelNote("No more of that charcoal."); return false; }
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
  if (easelTool === "smudge") value = smudgeValue(cell.x, cell.y, easelColour);
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
  var left = budget();
  left[1] = prev.left[1]; left[2] = prev.left[2]; left[3] = prev.left[3]; left[4] = prev.left[4];
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
    fetch(DAEMON + "/presence/painting/portrait", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body)
    }).then(function (res) { return res.json().then(function (j) { return { res: res, j: j }; }); }).then(function (got) {
      if (!got.res.ok) {
        var why = got.j && got.j.reason;
        easelNote(why === "insufficient" ? "Not enough charcoal." : why === "too_fast" ? "Wait a moment." : "Not saved.");
        return;
      }
      if (got.j && got.j.palette) easelHave = qtyToPixels(got.j.palette);
      readPixels(wallBuf, easelBuf);
      if (mode === "house") wallShown = "self";
      paintWall();
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
