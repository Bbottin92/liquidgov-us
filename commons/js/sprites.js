"use strict";
// ---- Player/passerby sprite atlas (art.md: 16x24, 4-dir, 2-frame walk,
// one look, no character creator; canvas.md: one atlas, one drawImage per
// actor -- no per-frame fillRect, no save/restore per sprite, no <img>
// per direction). Baked once, same bake-then-blit shape as everything
// else in this file. Player and passerby share this exact atlas (locked:
// "online others: same sprite, name in ink above head").
var SPRITE_W = 16, SPRITE_H = 24;
var DIR_DOWN = 0, DIR_UP = 1, DIR_LEFT = 2, DIR_RIGHT = 3;
var spriteAtlasCtx = null;
var spriteAtlas = document.createElement("canvas");
spriteAtlas.width = SPRITE_W * 8; spriteAtlas.height = SPRITE_H; // 4 dirs x 2 frames
(function paintSpriteAtlas() {
  // alpha:true here on purpose -- canvas.md's alpha:false rule is for the
  // bake/display contexts, which are opaque full-frame layers. This one is
  // a sprite composited over the world; without alpha it would blit as an
  // opaque black 16x24 box around the figure instead of just the figure.
  var sc = spriteAtlas.getContext("2d", { alpha: true });
  sc.imageSmoothingEnabled = false;
  spriteAtlasCtx = sc;
  var BODY = "#3a5a7a", SKIN = "#c9a97a";
  function drawFigure(ox, dir, frame) {
    sc.fillStyle = SKIN;
    sc.fillRect(ox + 5, 2, 6, 6); // head
    sc.fillStyle = BODY;
    sc.fillRect(ox + 4, 8, 8, 10); // body
    sc.fillStyle = INK;
    // Legs alternate which one is forward -- the whole walk animation.
    if (frame === 0) {
      sc.fillRect(ox + 5, 18, 2, 5);
      sc.fillRect(ox + 9, 18, 2, 6);
    } else {
      sc.fillRect(ox + 5, 18, 2, 6);
      sc.fillRect(ox + 9, 18, 2, 5);
    }
    // Facing mark: both eyes down, one eye left/right, none up (back of
    // the head) -- cheap enough to read as a direction without needing
    // a real side-profile silhouette.
    if (dir === DIR_DOWN) { sc.fillRect(ox + 6, 4, 1, 1); sc.fillRect(ox + 9, 4, 1, 1); }
    else if (dir === DIR_LEFT) { sc.fillRect(ox + 6, 4, 1, 1); }
    else if (dir === DIR_RIGHT) { sc.fillRect(ox + 9, 4, 1, 1); }
  }
  for (var d = 0; d < 4; d++) {
    for (var f = 0; f < 2; f++) drawFigure((d * 2 + f) * SPRITE_W, d, f);
  }
})();

// Anchors the sprite so its feet (bottom edge) sit at the entity's
// collision point, not its vertical center -- otherwise a 24px-tall
// sprite on a 12px box would float or sink relative to doors/props sized
// to the box.
function spriteDestX(worldX) { return worldX - SPRITE_W / 2; }
function spriteDestY(worldY, half) { return worldY - (SPRITE_H - half); }
function spriteSrcX(dir, frame) { return (dir * 2 + frame) * SPRITE_W; }

// 16×16 item glyphs, one cell per PLACED_ITEM_INFO slug. Same bake-once
// blit as the figure atlas. Floor, wall, and the inventory chip all
// drawImage from here instead of a flat color square.
var ITEM_GLYPH = 16;
var itemAtlas = document.createElement("canvas");
var itemGlyphIndex = {};
(function paintItemAtlas() {
  var slugs = [];
  for (var k in PLACED_ITEM_INFO) slugs.push(k);
  slugs.sort();
  itemAtlas.width = Math.max(ITEM_GLYPH, ITEM_GLYPH * slugs.length);
  itemAtlas.height = ITEM_GLYPH;
  var c = itemAtlas.getContext("2d", { alpha: true });
  c.imageSmoothingEnabled = false;
  function px(ox, x, y, w, h, col) { c.fillStyle = col; c.fillRect(ox + x, y, w, h); }
  for (var i = 0; i < slugs.length; i++) {
    var slug = slugs[i], ox = i * ITEM_GLYPH, col = PLACED_ITEM_INFO[slug].color;
    itemGlyphIndex[slug] = i;
    if (slug === "chalk_block") {
      px(ox, 4, 3, 8, 10, col);
      px(ox, 4, 3, 8, 1, "#f4efe4");
      px(ox, 4, 12, 8, 1, "#b8ae94");
    } else if (slug === "iron_gall_ink_page" || slug === "lampblack_ink") {
      px(ox, 3, 2, 10, 12, "#efe6d4");
      px(ox, 5, 4, 6, 1, col);
      px(ox, 5, 7, 6, 1, col);
      px(ox, 5, 10, 4, 1, col);
    } else if (slug === "copper_acetate_bottle" || slug === "vinegar_jar") {
      px(ox, 6, 1, 4, 3, col);
      px(ox, 4, 4, 8, 10, col);
      px(ox, 5, 6, 6, 4, "#f4efe4");
    } else if (slug === "cider_apple") {
      px(ox, 4, 5, 8, 8, col);
      px(ox, 7, 2, 2, 3, "#3a5a28");
      px(ox, 9, 3, 3, 2, "#3a5a28");
    } else if (slug === "seashell") {
      px(ox, 3, 8, 10, 5, col);
      px(ox, 5, 5, 6, 4, col);
      px(ox, 7, 3, 2, 3, "#f4efe4");
    } else if (slug === "alum_crystal") {
      px(ox, 7, 2, 2, 12, col);
      px(ox, 4, 6, 8, 4, col);
      px(ox, 6, 4, 4, 8, "#f4efe4");
    } else if (slug === "oak_gall" || slug === "oak_gall_tannin") {
      px(ox, 3, 5, 6, 6, col);
      px(ox, 8, 7, 5, 5, "#3a2418");
    } else if (slug === "gum_arabic") {
      px(ox, 4, 6, 8, 6, col);
      px(ox, 6, 4, 4, 4, col);
      px(ox, 6, 7, 4, 2, "#f4efe4");
    } else {
      px(ox, 4, 4, 8, 8, col);
      px(ox, 5, 5, 6, 2, "#f4efe4");
    }
  }
})();

function drawItemGlyph(ctx, slug, dx, dy, size) {
  var i = itemGlyphIndex[slug];
  if (i === undefined) return false;
  size = size || ITEM_GLYPH;
  ctx.drawImage(itemAtlas, i * ITEM_GLYPH, 0, ITEM_GLYPH, ITEM_GLYPH,
    Math.round(dx), Math.round(dy), size, size);
  return true;
}

// "Redraw next frame" flag, read and reset by frame() in main.js. It is declared HERE, not there: the sheets'
// onload handlers below assign it, and a cached image can fire onload between two <script> tags, before
// main.js has run; in strict mode assigning an undeclared name then throws. Declare a name in the first file
// that can touch it from an async callback (tests/client_files_unit.js fires the handlers between scripts).
var forceRedraw = true;

// Load tiles.png and sprites.png (same folder as this page). Each is used only
// if it decodes at exactly the size the generator writes; anything else leaves
// the painted look in place. Re-baking is a one-off: every space re-stamps from
// the sheet, then the next frame redraws.
(function loadSheets() {
  var ti = new Image();
  ti.onload = function () {
    if (ti.naturalWidth !== TILE_SHEET_W || ti.naturalHeight !== TILE_SHEET_H) return;
    tileSheet = ti;
    for (var i = 0; i < spaceRebakers.length; i++) spaceRebakers[i]();
    forceRedraw = true;
  };
  ti.src = TILE_SHEET_URL;

  var si = new Image();
  si.onload = function () {
    if (si.naturalWidth !== SPRITE_W * 8 || si.naturalHeight !== SPRITE_H) return;
    spriteAtlasCtx.clearRect(0, 0, spriteAtlas.width, spriteAtlas.height);
    spriteAtlasCtx.drawImage(si, 0, 0);
    forceRedraw = true;
  };
  si.src = SPRITE_SHEET_URL;
})();

function updateFacingAnim(actor, dx, dy, dt, moved) {
  // Facing follows input, not the post-collision result -- bumping a wall
  // should still turn the sprite to face it. Only the walk-cycle (leg
  // frame) depends on whether the step actually landed.
  if (dy > 0) actor.dir = DIR_DOWN;
  else if (dy < 0) actor.dir = DIR_UP;
  else if (dx > 0) actor.dir = DIR_RIGHT;
  else if (dx < 0) actor.dir = DIR_LEFT;
  if (!moved) { actor.animFrame = 0; actor.animTimer = 0; return; }
  actor.animTimer += dt;
  if (actor.animTimer > 0.15) { actor.animTimer = 0; actor.animFrame = 1 - actor.animFrame; }
}

