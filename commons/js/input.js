"use strict";
// ---- Input: keyboard --------------------------------------------------
// A/D = left/right on screen. Hard requirement, do not invert.
var keys = Object.create(null);
var KEY_TO_DIR = {
  ArrowUp: "up", ArrowDown: "down", ArrowLeft: "left", ArrowRight: "right",
  w: "up", s: "down", a: "left", d: "right",
  W: "up", S: "down", A: "left", D: "right"
};
// Typing a name must win over movement keys: while the name overlay is up,
// or while any text field has focus, don't touch these events at all -- no
// preventDefault, no keys[] tracking. "Brandon" has an a and a d in it.
function typingInField(e) {
  if (document.getElementById("nameOverlay").style.display !== "none") return true;
  var easel = document.getElementById("easel");
  if (easel && easel.classList.contains("open")) return true;
  var t = e.target;
  return !!(t && (t.tagName === "INPUT" || t.tagName === "TEXTAREA"));
}
window.addEventListener("keydown", function (e) {
  if (typingInField(e)) return;
  if (e.key === "Enter" || e.key === " ") { tryInteract(); e.preventDefault(); return; }
  var dir = KEY_TO_DIR[e.key];
  if (dir) { keys[dir] = true; e.preventDefault(); }
});
window.addEventListener("keyup", function (e) {
  if (typingInField(e)) return;
  var dir = KEY_TO_DIR[e.key];
  if (dir) { keys[dir] = false; e.preventDefault(); }
});

// ---- Input: on-screen d-pad (touch + mouse via pointer events) --------
var padState = { up: false, down: false, left: false, right: false };
function wirePad(id, dir) {
  var el = document.getElementById(id);
  var set = function (v) { return function (e) { padState[dir] = v; e.preventDefault(); }; };
  el.addEventListener("pointerdown", set(true));
  el.addEventListener("pointerup", set(false));
  el.addEventListener("pointercancel", set(false));
  el.addEventListener("pointerleave", set(false));
}
wirePad("dUp", "up");
wirePad("dDown", "down");
wirePad("dLeft", "left");
wirePad("dRight", "right");

var interactBtn = document.getElementById("interactBtn");
interactBtn.addEventListener("pointerdown", function (e) { e.preventDefault(); tryInteract(); });

function currentDX() {
  var l = keys.left || padState.left, r = keys.right || padState.right;
  return (r ? 1 : 0) - (l ? 1 : 0);
}
function currentDY() {
  var u = keys.up || padState.up, d = keys.down || padState.down;
  return (d ? 1 : 0) - (u ? 1 : 0);
}

