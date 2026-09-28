"use strict";
// ---- Camera / dual crop (art.md) ----------------------------------------
// Landscape (width >= height): 240x160 logical @ 3x.
// Portrait  (width <  height): 160x240 logical @ 2x.
var crop = { vw: 240, vh: 160, scale: 3 };

function computeCrop() {
  var landscape = window.innerWidth >= window.innerHeight;
  return landscape
    ? { vw: 240, vh: 160, scale: 3 }
    : { vw: 160, vh: 240, scale: 2 };
}

var canvas = document.getElementById("game");
var ctx = canvas.getContext("2d", { alpha: false });
var cropChangedThisFrame = true; // force one draw after any crop swap

function applyCrop() {
  crop = computeCrop();
  canvas.width = crop.vw;
  canvas.height = crop.vh;
  canvas.style.width = (crop.vw * crop.scale) + "px";
  canvas.style.height = (crop.vh * crop.scale) + "px";
  ctx.imageSmoothingEnabled = false;
  cropChangedThisFrame = true;
}
applyCrop();
window.addEventListener("resize", applyCrop);
window.addEventListener("orientationchange", applyCrop);

// ---- Player ---------------------------------------------------------------
var spawn = regionAABB(findRegion("the Square"));
var player = {
  x: ((spawn.x0 + spawn.x1 + 1) / 2) * TILE,
  y: ((spawn.y0 + spawn.y1 + 1) / 2) * TILE,
  half: 6,
  speed: 76, // px/sec
  dir: DIR_DOWN, animFrame: 0, animTimer: 0
};
var playerName = "";

// collision.md's spawn rule: a solid prop must never land on the spawn
// tile (the actual fountain bug from the last round). Asserted here,
// after dressRegions() has already run, so a future prop placement that
// breaks it fails loudly instead of silently trapping a fresh arrival.
(function assertSpawnIsFree() {
  if (boxBlockedAt(player.x, player.y, player.half)) {
    console.error("Commons: spawn tile is blocked -- move the prop, not the player.");
  }
})();

// The proposals page's Hall door lands here: /commons/?at=hall. Stand on the
// path tile directly north of the Hall door, facing it, so Enter still reaches
// the prompt. One shot -- the query is removed so a later refresh does not
// drag you back. The Square spawn above is what a normal visit still uses,
// and assertSpawnIsFree has already checked that tile.
function applyHallReturn() {
  var search = "";
  try { search = (window.location && window.location.search) || ""; } catch (e1) { return; }
  if (typeof search !== "string" || !/(?:^|[?&])at=hall(?:&|$)/.test(search)) return;
  var hall = null, i;
  for (i = 0; i < interactables.length; i++) {
    if (interactables[i].type === "hall") { hall = interactables[i]; break; }
  }
  if (!hall) return;
  var tx = hall.x, ty = hall.y - 1;
  if (ty < 0 || solid[ty * WORLD_W + tx]) return;
  player.x = tx * TILE + 8;
  player.y = ty * TILE + 8;
  player.dir = DIR_DOWN;
  try {
    if (window.history && window.history.replaceState && window.URL) {
      var u = new URL(window.location.href);
      u.searchParams.delete("at");
      window.history.replaceState(null, "", u.pathname + (u.search || "") + (u.hash || ""));
    }
  } catch (e2) {}
}
applyHallReturn();

// P2: these three read `activeSpace` instead of the outdoor globals
// directly, so the exact same collision.md rules (12x12 box, 4 corners,
// axis split, no tile-snap, no CCD) apply unchanged inside an interior --
// only which tile/solid array and which pixel bounds are "the world"
// changes. collision.md's dt-clamp reasoning is untouched either way.
function tileBlockedAtPixel(px, py) {
  var tx = Math.floor(px / TILE), ty = Math.floor(py / TILE);
  if (tx < 0 || ty < 0 || tx >= activeSpace.w || ty >= activeSpace.h) return true;
  return activeSpace.solid[idxIn(activeSpace.w, tx, ty)] === 1;
}

function boxBlockedAt(cx, cy, half) {
  return tileBlockedAtPixel(cx - half, cy - half) ||
         tileBlockedAtPixel(cx + half, cy - half) ||
         tileBlockedAtPixel(cx - half, cy + half) ||
         tileBlockedAtPixel(cx + half, cy + half);
}

function movePlayer(dx, dy, dt) {
  if (dx === 0 && dy === 0) { updateFacingAnim(player, 0, 0, dt, false); return false; }
  var len = Math.sqrt(dx * dx + dy * dy) || 1;
  var vx = (dx / len) * player.speed * dt;
  var vy = (dy / len) * player.speed * dt;

  var moved = false;
  var nx = player.x + vx;
  if (!boxBlockedAt(nx, player.y, player.half)) { player.x = nx; moved = true; }
  var ny = player.y + vy;
  if (!boxBlockedAt(player.x, ny, player.half)) { player.y = ny; moved = true; }

  // Edge of the current space: clamp, don't wall-tile it.
  var h = player.half;
  if (player.x < h) player.x = h;
  if (player.y < h) player.y = h;
  if (player.x > activeSpace.pw - h) player.x = activeSpace.pw - h;
  if (player.y > activeSpace.ph - h) player.y = activeSpace.ph - h;

  // Facing/animation from the *input* direction, not the post-collision
  // delta -- so bumping a wall still turns the sprite to face it instead
  // of freezing on whatever direction last succeeded.
  updateFacingAnim(player, dx, dy, dt, moved);
  return moved;
}

// ---- The passerby (locked: nameplate exactly "passerby", is_ambient
// internally, hides while a real guest shares its region, Square ->
// Homestead -> Creek -> Shore, presence-shaped/RAM/~2Hz-scale motion,
// no dialogue, not a friend door, not an NPC). ---------------------------
// collision.md: the passerby does not collide, so every waypoint must sit
// on an actual path/bridge tile or it walks on water. The last waypoint
// used to be a plain Shore coordinate off the ribbon entirely -- fixed by
// keeping every point on the exact tiles bakeTileIds() carved: the
// vertical spine at x=35, then the horizontal jog at y=69 out to x=65.
var passerbyWaypoints = (function () {
  var sq = regionAABB(findRegion("the Square"));
  var hs = regionAABB(findRegion("Homestead Row"));
  function px(tx, ty) { return { x: tx * TILE + 8, y: ty * TILE + 8 }; }
  return [
    px(35, sq.y0 + 2),                       // the Square, near the top of the spine
    px(35, Math.floor((hs.y0 + hs.y1) / 2)), // Homestead Row, still on the vertical spine
    px(35, 69),                              // Creek, exactly the jog's start row
    px(65, 69)                               // Shore, exactly the jog's end -- still bridge
  ];
})();
var passerby = {
  x: passerbyWaypoints[0].x,
  y: passerbyWaypoints[0].y,
  half: 6,
  speed: 50,
  wp: 0,
  wpDir: 1, // patrol ping-pong direction along passerbyWaypoints -- not sprite facing
  dir: DIR_DOWN, animFrame: 0, animTimer: 0, // sprite facing/walk-cycle (same atlas as player)
  isAmbient: true, // internal marker -- never string-match the nameplate
  visible: true
};

function updatePasserby(dt) {
  var target = passerbyWaypoints[passerby.wp];
  var dx = target.x - passerby.x, dy = target.y - passerby.y;
  var dist = Math.sqrt(dx * dx + dy * dy);
  var moved = false;
  if (dist < 2) {
    passerby.wp += passerby.wpDir;
    if (passerby.wp >= passerbyWaypoints.length - 1) { passerby.wp = passerbyWaypoints.length - 1; passerby.wpDir = -1; }
    if (passerby.wp <= 0) { passerby.wp = 0; passerby.wpDir = 1; }
  } else {
    passerby.x += (dx / dist) * passerby.speed * dt;
    passerby.y += (dy / dist) * passerby.speed * dt;
    moved = true;
  }
  updateFacingAnim(passerby, dx, dy, dt, moved);

  // Locked rule is "hide only if a second *real* occupant is already
  // there" -- not "hide whenever the local player is." The local player
  // is who this crutch exists for, not a redundant second body: hiding it
  // the moment they're in the same region as their own passerby left the
  // Square empty from the very first frame, which is the exact
  // waiting-room look this was built to avoid (found in review, not by
  // testing -- the spawn case never showed anything *else* to compare
  // against). otherRealOccupantsInRegion() is 0 today because this
  // preview has no multiplayer transport at all; it stays a named seam so
  // wiring in real presence later is additive, not a rewrite.
  var pbTile = { x: Math.floor(passerby.x / TILE), y: Math.floor(passerby.y / TILE) };
  var pbRegion = regionAt(pbTile.x, pbTile.y);
  passerby.visible = otherRealOccupantsInRegion(pbRegion) === 0 ? true : false;

  return moved;
}

