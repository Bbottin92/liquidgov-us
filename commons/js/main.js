"use strict";
// ---- Main loop ----------------------------------------------------------
var running = false;
var lastT = 0;
var lastTickAt = 0; // performance.now() of the last frame() call, any source
var forceRedraw = true;
var prevPlayerX = player.x, prevPlayerY = player.y;
var prevPlayerDir = player.dir, prevPlayerAnim = player.animFrame;
var prevPbVisible = false, prevPbX = passerby.x, prevPbY = passerby.y;
var prevPbDir = passerby.dir, prevPbAnim = passerby.animFrame;
var prevToastActive = false;

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
  // The hint and toast sit at the bottom of the viewport. In a room shorter
  // than the viewport that is the black margin, where ink would vanish, so
  // they switch to paper there.
  var hintInk = (activeSpace.ph - camY) < crop.vh - 30 ? "#f7f1e4" : INK;

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
        ctx.fillRect(6 * TILE - camX + 3 * TILE - 11, wy + 2 * TILE - 11, 9, 9);
        ctx.fillStyle = INK;
        ctx.font = "7px monospace";
        ctx.textAlign = "center";
        ctx.textBaseline = "middle";
        ctx.fillText(info.label, 6 * TILE - camX + 1.5 * TILE, wy + 2 * TILE + 6);
      } else {
        ctx.fillRect(wx + 2, wy + 2, TILE - 4, TILE - 4);
        ctx.fillStyle = INK;
        ctx.font = "7px monospace";
        ctx.textAlign = "center";
        ctx.textBaseline = "middle";
        ctx.fillText(info.label, wx + TILE / 2, wy + TILE + 6);
      }
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
      ctx.fillStyle = "#d8b46a";
      ctx.fillRect(lsx + 2, lsy + 2, TILE - 4, TILE - 4);
      var lname = lot.friend.length > 9 ? lot.friend.slice(0, 8) + "…" : lot.friend;
      ctx.fillStyle = INK;
      ctx.font = "7px monospace";
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";
      ctx.fillText(lname, lsx + TILE / 2, lsy - 5);
    }
  }

  if (mode === "outdoor" && passerby.visible) {
    var pbsx = passerby.x - camX, pbsy = passerby.y - camY;
    var pbDestY = spriteDestY(pbsy, passerby.half);
    ctx.drawImage(spriteAtlas, spriteSrcX(passerby.dir, passerby.animFrame), 0, SPRITE_W, SPRITE_H,
      spriteDestX(pbsx), pbDestY, SPRITE_W, SPRITE_H);
    ctx.fillStyle = INK;
    ctx.font = "8px monospace";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillText("passerby", pbsx, pbDestY - 4);
  }

  // V2: renders for either outdoor or a shared interior -- otherOccupants
  // is populated by whichever poll last ran (netPollRegions outdoor,
  // netPollCurrentRegion indoors) and regionLocalToWorld already handles
  // both an outdoor REGIONS entry and an interior instance-key string.
  if (netReady && netRegion) {
    for (var oi = 0; oi < otherOccupants.length; oi++) {
      var o = otherOccupants[oi];
      var owp = regionLocalToWorld(o.region, o.x, o.y);
      var osx = owp.x - camX, osy = owp.y - camY;
      var oDir = (typeof o.facing === "number" && o.facing >= 0 && o.facing <= 3) ? o.facing : DIR_DOWN;
      // No animFrame in the occupant feed (2Hz snapshots, not a walk
      // cycle) -- a static standing frame per poll is enough to prove
      // real multiplayer works; smoothing/interpolating between polls is
      // a later nice-to-have, not attempted here.
      var oDestY = spriteDestY(osy, 6);
      ctx.drawImage(spriteAtlas, spriteSrcX(oDir, 0), 0, SPRITE_W, SPRITE_H,
        spriteDestX(osx), oDestY, SPRITE_W, SPRITE_H);
      ctx.fillStyle = INK;
      ctx.font = "8px monospace";
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";
      ctx.fillText(o.display_name || o.id, osx, oDestY - 4);
    }
  }

  var sx = player.x - camX, sy = player.y - camY;
  var sDestY = spriteDestY(sy, player.half);
  ctx.drawImage(spriteAtlas, spriteSrcX(player.dir, player.animFrame), 0, SPRITE_W, SPRITE_H,
    spriteDestX(sx), sDestY, SPRITE_W, SPRITE_H);
  // Self-nameplate (GROK_V2_ANSWER.md stretch item): only once the
  // daemon has actually accepted a name -- before the first join
  // response lands, or if the daemon was never reachable at all, no
  // nameplate draws rather than showing the raw typed input that might
  // not match what other clients see (GROK_NAME_REVIEW.md).
  if (acceptedDisplayName) {
    ctx.fillStyle = INK;
    ctx.font = "8px monospace";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillText(acceptedDisplayName, sx, sDestY - 4);
  }

  if (nearestInteractable) {
    var ix = (nearestInteractable.x * TILE + 8) - camX;
    var iy = (nearestInteractable.y * TILE + 8) - camY - 14;
    ctx.fillStyle = INK;
    ctx.font = "8px monospace";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    // Keep the prompt inside the viewport: the house latch sits on the wall at
    // the left edge, where a centred label would be cut off.
    var promptText = "[" + nearestInteractable.prompt + "]";
    var promptW = ctx.measureText(promptText).width;
    ix = Math.max(promptW / 2 + 1, Math.min(crop.vw - promptW / 2 - 1, ix));
    ctx.fillText(promptText, ix, iy);
  } else if (mode !== "outdoor") {
    // Demo house has no interactables at all (the wall slot is decoration,
    // not a station) and the lab's walk-in exit needs no station nearby
    // either -- without this, losing P1's static "[Enter] to step back
    // outside" stub text would leave keyboard users with no on-screen cue
    // that Enter still leaves (the interact button covers touch). Drawn
    // higher than the toast line so a lab-station toast can't overlap it.
    ctx.fillStyle = hintInk;
    ctx.font = "8px monospace";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillText("[Enter] to step back outside", crop.vw / 2, crop.vh - 26);
  }

  if (performance.now() < toast.until) {
    ctx.fillStyle = hintInk;
    ctx.font = "8px monospace";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillText(toast.msg, crop.vw / 2, crop.vh - 14);
  }
}

function frame(t) {
  if (!running) return;
  var dt = Math.min((t - lastT) / 1000, 1 / 20) || 0;
  lastT = t;
  lastTickAt = t;
  netStatusTick(t);   // the "Reconnecting…" badge: observe-only, every frame (net.js)

  var playerMoved = movePlayer(currentDX(), currentDY(), dt);

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
  // Parked since P2: the button always said "Enter" even at a Try/Read/
  // Look station -- harmless (nearest interactable still won on tap), but
  // the caption was lying about which key it stood in for.
  interactBtn.textContent = nearestInteractable
    ? (nearestInteractable.type === "wallSlot" && mode === "visit" ? "Look" : nearestInteractable.prompt)
    : "Enter";

  var toastActive = performance.now() < toast.until;

  // canvas.md #2: skip the blit entirely if the scene truly hasn't changed
  // -- rAF still runs (vsync), we just don't touch the GPU for nothing.
  // toastActive alone isn't enough: it covers every frame *while* the
  // toast is showing, but the frame where it flips back to false (the
  // off-transition) also needs to redraw, or the last "Closed." bitmap
  // just sits there forever since nothing else necessarily changed on
  // that exact frame. Found by testing the actual expiry, not by reading
  // the code.
  //
  // Must-fix from sprites review: dir/animFrame can change with the walk
  // blocked (bump a wall, tap a different direction -- updateFacingAnim
  // still turns the sprite, but playerMoved is false), so "did x,y move"
  // alone isn't a complete dirty check anymore now that there's a facing
  // cell to redraw. Track prevPlayerDir/Anim (and passerby's) the same
  // way position is tracked.
  var playerFacingChanged = player.dir !== prevPlayerDir || player.animFrame !== prevPlayerAnim;
  var changed = forceRedraw || cropChangedThisFrame || playerMoved || playerFacingChanged ||
    toastActive || (toastActive !== prevToastActive);
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
  prevToastActive = toastActive;
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
input.focus();
