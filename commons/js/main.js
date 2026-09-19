"use strict";
// ---- Main loop ----------------------------------------------------------
var running = false;
var lastT = 0;
var lastTickAt = 0; // performance.now() of the last frame() call, any source
// (forceRedraw is declared in sprites.js, the first file whose async handlers assign it -- see there.)
var prevPlayerX = player.x, prevPlayerY = player.y;
var prevPlayerDir = player.dir, prevPlayerAnim = player.animFrame;
var prevPbVisible = false, prevPbX = passerby.x, prevPbY = passerby.y;
var prevPbDir = passerby.dir, prevPbAnim = passerby.animFrame;

// Paper-backed 8px labels so names read on grass and on paper walls.
function drawNameplate(c, text, x, y) {
  if (!text) return;
  c.font = "8px monospace";
  c.textAlign = "center";
  c.textBaseline = "middle";
  var w = c.measureText(text).width;
  c.fillStyle = "rgba(247,241,228,0.88)";
  c.fillRect(Math.round(x - w / 2 - 2), Math.round(y - 5), Math.round(w + 4), 10);
  c.fillStyle = INK;
  c.fillText(text, x, y);
}

function drawFootShadow(c, worldX, worldY, camX, camY) {
  var x = Math.round(worldX - camX);
  var y = Math.round(worldY - camY);
  c.fillStyle = "rgba(26,24,20,0.28)";
  c.fillRect(x - 5, y - 1, 10, 3);
}

function blitTallTile(c, id, tx, ty, camX, camY) {
  var dx = Math.round(tx * TILE - camX), dy = Math.round(ty * TILE - camY);
  if (tileSheet) {
    c.drawImage(tileSheet, (id % 16) * TILE, Math.floor(id / 16) * TILE, TILE, TILE, dx, dy, TILE, TILE);
  } else {
    c.fillStyle = COLOR[id] || "#3a5a28";
    c.fillRect(dx, dy, TILE, TILE);
  }
}

// Second storey of a tree: foliage on the walkable tile north of the trunk.
// Not baked (would overwrite grass). Sorted with actors so you tuck under it.
function blitCanopy(c, tx, ty, camX, camY) {
  var dx = Math.round(tx * TILE - camX), dy = Math.round(ty * TILE - camY);
  var base = COLOR[T_TREE];
  c.fillStyle = shadeHex(base, -0.18);
  c.fillRect(dx + 1, dy + 6, 14, 9);
  c.fillStyle = base;
  c.fillRect(dx + 3, dy + 2, 10, 11);
  c.fillStyle = shadeHex(base, 0.14);
  c.fillRect(dx + 5, dy + 1, 6, 6);
}

function drawOneActor(c, a, camX, camY) {
  drawFootShadow(c, a.x, a.y, camX, camY);
  var destY = spriteDestY(a.y - camY, a.half);
  c.drawImage(spriteAtlas, spriteSrcX(a.dir, a.anim), 0, SPRITE_W, SPRITE_H,
    spriteDestX(a.x - camX), destY, SPRITE_W, SPRITE_H);
  if (a.name) drawNameplate(c, a.name, a.x - camX, destY - 4);
}

function drawSortedActors(c, camX, camY) {
  var list = [];
  if (mode === "outdoor" && passerby.visible) {
    list.push({ kind: "actor", x: passerby.x, y: passerby.y, dir: passerby.dir, anim: passerby.animFrame,
      name: "passerby", half: passerby.half });
  }
  if (netReady && netRegion) {
    for (var oid in occupantViz) {
      var o = occupantViz[oid];
      list.push({ kind: "actor", x: o.x, y: o.y, dir: o.dir, anim: o.animFrame,
        name: o.display_name || o.id, half: 6 });
    }
  }
  list.push({ kind: "actor", x: player.x, y: player.y, dir: player.dir, anim: player.animFrame,
    name: acceptedDisplayName, half: player.half });

  var x0 = Math.max(0, Math.floor(camX / TILE) - 1);
  var y0 = Math.max(0, Math.floor(camY / TILE) - 1);
  var x1 = Math.min(activeSpace.w - 1, Math.ceil((camX + crop.vw) / TILE));
  var y1 = Math.min(activeSpace.h - 1, Math.ceil((camY + crop.vh) / TILE) + 1);
  for (var ty = y0; ty <= y1; ty++) {
    for (var tx = x0; tx <= x1; tx++) {
      var id = activeSpace.tiles[idxIn(activeSpace.w, tx, ty)];
      if (!TALL_TILE[id]) continue;
      list.push({ kind: "tile", id: id, tx: tx, ty: ty, y: (ty + 1) * TILE });
      if (id === T_TREE && ty > 0) {
        var nid = activeSpace.tiles[idxIn(activeSpace.w, tx, ty - 1)];
        if (!TALL_TILE[nid] && nid !== T_WATER) {
          list.push({ kind: "canopy", tx: tx, ty: ty - 1, y: ty * TILE });
        }
      }
    }
  }

  list.sort(function (a, b) {
    if (a.y !== b.y) return a.y - b.y;
    return (a.kind === "actor" ? 1 : 0) - (b.kind === "actor" ? 1 : 0);
  });
  for (var i = 0; i < list.length; i++) {
    var it = list[i];
    if (it.kind === "tile") blitTallTile(c, it.id, it.tx, it.ty, camX, camY);
    else if (it.kind === "canopy") blitCanopy(c, it.tx, it.ty, camX, camY);
    else drawOneActor(c, it, camX, camY);
  }
}

// One draw path for outdoor and both interiors -- they already share the
// same bake/blit/collision shape, so the only per-space differences are
// "is there a passerby" (outdoor only) and what's in `interactables`.
function drawSpace(camX, camY) {
  // A room smaller than the viewport (the 16x12 mine, the lab) only covers
  // part of the canvas; without this the rest keeps whatever the previous
  // frame -- usually the outdoor world -- left there. Fill it with the page's
  // own black first. Only when the space really is smaller, so the outdoor
  // frame (which always covers everything) pays nothing.
  if (activeSpace.pw < crop.vw || activeSpace.ph < crop.vh) {
    ctx.fillStyle = "#0c0b09";
    ctx.fillRect(0, 0, crop.vw, crop.vh);
  }
  ctx.drawImage(activeSpace.baked, camX, camY, crop.vw, crop.vh, 0, 0, crop.vw, crop.vh);

  // V2 stretch: the placed lab item, drawn over the baked wall-slot tile
  // itself -- a flat color square + label, not a sprite (this game has
  // no item-sprite assets, see art.md's locked figure-only atlas).
  // The house owner's real profile canvas, hung in the wall slot
  // (glue.md "Wall-slot bitmap"): the latest saved frame from civic, only in a
  // house (yours, or a friend's you're visiting) -- the shared demo house and
  // the lab have no owner, so they keep the empty slot.
  var wallOwner = (mode === "house" || mode === "visit") ? wallOwnerFor(INTERIOR_INSTANCE_KEY[mode]) : null;
  loadWallPortrait(wallOwner);
  var portraitOn = !!(wallOwner && wallPortrait.ok);
  if (portraitOn) {
    var pfx = 6 * TILE - camX, pfy = 0 * TILE - camY;         // 3 tiles wide, 2 tall, centered on the slot
    ctx.fillStyle = "#d8cdb0";
    ctx.fillRect(pfx, pfy, 3 * TILE, 2 * TILE);
    var iw = wallPortrait.img.naturalWidth, ih = wallPortrait.img.naturalHeight;
    var pad = 3, boxW = 3 * TILE - pad * 2, boxH = 2 * TILE - pad * 2;
    var sc = Math.min(boxW / iw, boxH / ih);
    var dw = iw * sc, dh = ih * sc;
    ctx.drawImage(wallPortrait.img, pfx + (3 * TILE - dw) / 2, pfy + (2 * TILE - dh) / 2, dw, dh);
  }

  // V2 stretch: the placed lab item. With no portrait it is drawn over the
  // baked wall-slot tile itself as before -- a flat color square + label, not
  // a sprite (this game has no item-sprite assets, see art.md's locked
  // figure-only atlas). With a portrait hung there it shrinks to a badge on
  // the frame's corner, so a placed item and the real portrait can share the
  // one slot instead of one hiding the other.
  if ((mode === "demoHouse" || mode === "house" || mode === "visit") && placedItem) {
    var info = PLACED_ITEM_INFO[placedItem];
    if (info) {
      var wx = 7 * TILE - camX, wy = 0 * TILE - camY;
      ctx.fillStyle = info.color;
      if (portraitOn) {
        drawItemGlyph(ctx, placedItem, 6 * TILE - camX + 3 * TILE - 12, wy + 2 * TILE - 12, 10);
        drawNameplate(ctx, info.label, 6 * TILE - camX + 1.5 * TILE, wy + 2 * TILE + 6);
      } else {
        drawItemGlyph(ctx, placedItem, wx, wy);
        drawNameplate(ctx, info.label, wx + TILE / 2, wy + TILE + 6);
      }
    }
  }

  if ((mode === "house" || mode === "visit") && placedFloor.length) {
    for (var fi = 0; fi < placedFloor.length; fi++) {
      var f = placedFloor[fi], finfo = PLACED_ITEM_INFO[f.item];
      if (!finfo) continue;
      var fx = f.x * TILE - camX, fy = f.y * TILE - camY;
      drawItemGlyph(ctx, f.item, fx, fy);
      drawNameplate(ctx, finfo.label, fx + TILE / 2, fy + TILE + 5);
    }
  }

  // V6: a lot door whose friend is in the game right now reads as lit, with
  // their name over it. Drawn over the baked (still boarded) tile like the
  // placed item above -- the bake stays static, only the overlay changes.
  if (mode === "outdoor") {
    for (var fd = 0; fd < FRIEND_LOT_DOORS.length; fd++) {
      var lot = FRIEND_LOT_DOORS[fd];
      if (!lot.friend) continue;
      var lsx = lot.x * TILE - camX, lsy = lot.y * TILE - camY;
      if (lsx < -TILE || lsx > crop.vw || lsy < -TILE || lsy > crop.vh) continue;
      if (tileSheet) {
        ctx.drawImage(tileSheet, (T_DOOR_HOUSE % 16) * TILE, Math.floor(T_DOOR_HOUSE / 16) * TILE,
          TILE, TILE, lsx, lsy, TILE, TILE);
      } else {
        ctx.fillStyle = "#6b4a28";
        ctx.fillRect(lsx + 2, lsy + 1, 12, 14);
        ctx.fillStyle = "#d8b46a";
        ctx.fillRect(lsx + 11, lsy + 8, 2, 2);
      }
      var lname = lot.friend.length > 9 ? lot.friend.slice(0, 8) + "…" : lot.friend;
      drawNameplate(ctx, lname, lsx + TILE / 2, lsy - 5);
    }
  }

  if (mode === "outdoor") {
    for (var wi = 0; wi < interactables.length; wi++) {
      var wpi = interactables[wi];
      if (wpi.type !== "worldPickup") continue;
      var wp = WORLD_PICKUPS[wpi.pickup];
      if (!wp) continue;
      var gx = wpi.x * TILE - camX, gy = wpi.y * TILE - camY;
      if (gx < -TILE || gx > crop.vw || gy < -TILE || gy > crop.vh) continue;
      drawItemGlyph(ctx, wp.item, gx, gy + 6);
    }
  }

  drawSortedActors(ctx, camX, camY);
}

function frame(t) {
  if (!running) return;
  var dt = Math.min((t - lastT) / 1000, 1 / 20) || 0;
  lastT = t;
  lastTickAt = t;
  netStatusTick(t);   // the "Reconnecting…" badge: observe-only, every frame (net.js)
  tickToast(t);
  tickPlaceName(t);

  var playerMoved = movePlayer(currentDX(), currentDY(), dt);
  var occupantsMoved = stepOccupants(dt);

  // Hoisted above the outdoor block (not just above drawSpace, where it
  // used to live) so the presence networking tick can compute which
  // regions the camera currently overlaps -- same values, reused for the
  // actual draw below instead of recomputing them.
  var camX = Math.round(player.x - crop.vw / 2);
  var camY = Math.round(player.y - crop.vh / 2);
  camX = Math.max(0, Math.min(camX, activeSpace.pw - crop.vw));
  camY = Math.max(0, Math.min(camY, activeSpace.ph - crop.vh));

  var pbMoved = false;
  if (mode === "outdoor") {
    pbMoved = updatePasserby(dt);
    // Lab door is a walk-in trigger (atlas.md), not Enter-to-confirm --
    // fires the instant the player's own tile matches it.
    var ptx = Math.floor(player.x / TILE), pty = Math.floor(player.y / TILE);
    if (LAB_DOOR_TILE && ptx === LAB_DOOR_TILE.x && pty === LAB_DOOR_TILE.y) {
      enterInterior("lab", labSpace, LAB_ENTRY);
    }

    // Presence networking tick. Join happens the first outdoor frame after
    // netMint() resolves (netRegion still null); crossing an outdoor AABB
    // rekeys instead of a raw join/leave pair (presence.md: "do not leave
    // a ghost in the old list"); otherwise move+poll are throttled to
    // NET_INTERVAL_MS, not every frame -- this is a ~60Hz render loop, the
    // daemon's own tick-rate target is ~2Hz per region.
    if (netReady) {
      var curRegion = regionAt(ptx, pty);
      if (curRegion && curRegion !== netRegion) {
        if (netRegion === null) netJoin(curRegion);
        else netRekey(netRegion, curRegion);
      } else if (curRegion && t - lastNetSendAt > NET_INTERVAL_MS) {
        var camRegions = regionsIntersectingCamera(camX, camY);
        netMove();
        netPollRegions(camRegions);
        netPollLots(camRegions, t);
        lastNetSendAt = t;
      }
    }
  } else {
    // V2 (GROK_V2_ANSWER.md): "indoors is a single-player cutscene" was
    // the actual hole real presence left behind -- V1 called netLeave()
    // on every interior entry, so two guests standing in the same demo
    // house never saw each other. Both interiors are now one **shared**
    // instance each (instance:demo / instance:lab), not per-player
    // houses (that needs real accounts, post-glue) -- a fixed key is
    // correct here, not a shortcut standing in for something bigger.
    // Same join-on-null/rekey-on-mismatch pattern as outdoor, just
    // against a single fixed region instead of whichever grid cell the
    // player's tile is in; same throttled move+poll, but a plain
    // single-region poll (netPollCurrentRegion), not the camera-neighbor
    // union outdoor uses -- there's only ever one region to be in here.
    var instanceKey = INTERIOR_INSTANCE_KEY[mode];
    if (netReady && instanceKey) {
      if (instanceKey !== netRegion) {
        if (netRegion === null) netJoin(instanceKey);
        else netRekey(netRegion, instanceKey);
      } else if (t - lastNetSendAt > NET_INTERVAL_MS) {
        netMove();
        netPollCurrentRegion();
        if (mode !== "mine") netPollPlaced(); // nothing can be placed in the mine
        lastNetSendAt = t;
      }
    }
    if (mode === "lab") {
      var ltx = Math.floor(player.x / TILE), lty = Math.floor(player.y / TILE);
      if (ltx === LAB_DOOR.x && lty === LAB_DOOR.y) {
        exitToOutdoor({ x: LAB_DOOR_TILE.x, y: LAB_DOOR_TILE.y + 1 });
      }
    }
  }

  nearestInteractable = findNearestInteractable();
  interactBtn.classList.toggle("show", !!nearestInteractable || mode !== "outdoor");
  interactBtn.textContent = nearestInteractable
    ? (nearestInteractable.type === "wallSlot" && mode === "visit" ? "Look" : nearestInteractable.prompt)
    : "Leave";

  // canvas.md #2: skip the blit entirely if the scene truly hasn't changed
  // -- rAF still runs (vsync), we just don't touch the GPU for nothing.
  // Toasts live in a DOM pill now, so they no longer dirty the canvas.
  //
  // Must-fix from sprites review: dir/animFrame can change with the walk
  // blocked (bump a wall, tap a different direction -- updateFacingAnim
  // still turns the sprite, but playerMoved is false), so "did x,y move"
  // alone isn't a complete dirty check anymore now that there's a facing
  // cell to redraw. Track prevPlayerDir/Anim (and passerby's) the same
  // way position is tracked.
  var playerFacingChanged = player.dir !== prevPlayerDir || player.animFrame !== prevPlayerAnim;
  var changed = forceRedraw || cropChangedThisFrame || playerMoved || playerFacingChanged || occupantsMoved;
  if (mode === "outdoor") {
    var pbFacingChanged = passerby.dir !== prevPbDir || passerby.animFrame !== prevPbAnim;
    changed = changed ||
      (passerby.visible && (pbMoved || pbFacingChanged || passerby.x !== prevPbX || passerby.y !== prevPbY)) ||
      (passerby.visible !== prevPbVisible);
  }
  // V2: a fresh poll (outdoor or a shared interior) can update
  // otherOccupants between frames with everything local otherwise idle --
  // without this, a remote player's new position wouldn't redraw until
  // something local also happened to change. Not folded into the
  // outdoor-only block above since interiors need it too now.
  changed = changed || netOccupantsDirty;

  if (!changed) return;

  drawSpace(camX, camY);

  forceRedraw = false;
  cropChangedThisFrame = false;
  netOccupantsDirty = false;
  prevPlayerX = player.x; prevPlayerY = player.y;
  prevPlayerDir = player.dir; prevPlayerAnim = player.animFrame;
  prevPbVisible = passerby.visible; prevPbX = passerby.x; prevPbY = passerby.y;
  prevPbDir = passerby.dir; prevPbAnim = passerby.animFrame;
}

// requestAnimationFrame is the loop -- real browsers (including phones)
// fire it reliably and it pauses correctly in a background tab, which a
// timer-only loop wouldn't. setInterval only exists as a watchdog fallback
// for the rare view that never calls rAF at all. The watchdog checks every
// ~300ms whether a frame has actually landed recently; it only starts the
// fallback interval if rAF has gone completely silent, tears it back down
// if rAF resumes, and never engages at all while the tab is hidden (a
// backgrounded tab correctly doing nothing is not "rAF went silent").
var WATCHDOG_MS = 300;
var usingFallback = false;
var fallbackHandle = null;

function rafLoop(t) {
  if (!running) return;
  frame(t);
  requestAnimationFrame(rafLoop);
}

function watchdogTick() {
  if (!running) return;
  if (document.hidden) {
    if (usingFallback) {
      usingFallback = false;
      clearInterval(fallbackHandle);
      fallbackHandle = null;
    }
    return;
  }
  var stale = performance.now() - lastTickAt > WATCHDOG_MS;
  if (stale && !usingFallback) {
    usingFallback = true;
    fallbackHandle = setInterval(function () { frame(performance.now()); }, 1000 / 60);
  } else if (!stale && usingFallback) {
    usingFallback = false;
    clearInterval(fallbackHandle);
    fallbackHandle = null;
  }
}

function start() {
  if (running) return;
  running = true;
  lastT = performance.now();
  lastTickAt = lastT;
  requestAnimationFrame(rafLoop);
  setInterval(watchdogTick, WATCHDOG_MS);
}

// ---- Name prompt ----------------------------------------------------------
var overlay = document.getElementById("nameOverlay");
var input = document.getElementById("nameInput");
var go = document.getElementById("nameGo");

function enter() {
  playerName = (input.value || "Guest").trim().slice(0, 20) || "Guest";
  try { sessionStorage.setItem("commons-name", playerName); } catch (e) {}
  overlay.style.display = "none";
  input.blur(); // else document.activeElement is still the input and the
                // typingInField() guard would keep eating movement keys
  start();
  // Fire-and-forget: if the daemon is unreachable (or refuses, or is restarting), netReady stays false and the
  // game runs as a local simulation, while netMint() keeps retrying with backoff and presence starts the moment
  // it gets a credential.
  netMint();
}
go.addEventListener("click", enter);
input.addEventListener("keydown", function (e) {
  if (e.key === "Enter") enter();
});
(function resumeName() {
  var saved = "";
  try { saved = sessionStorage.getItem("commons-name") || ""; } catch (e) {}
  if (saved) {
    input.value = saved;
    enter();
  } else {
    input.focus();
  }
})();
