"use strict";
// ---- Interact system ----------------------------------------------------
// One prompt at doors/set pieces, no RPG menu (locked, art.md). Range is
// "on or next to the tile" -- Chebyshev distance <= 1.
// mode: "outdoor" | "demoHouse" | "lab" | "mine" | "house" (V4) | "visit" (a friend's house). The demo house keeps its P1
// Enter-to-enter/Enter-to-exit pattern unchanged. The Lab is a walk-in
// trigger both ways (atlas.md is explicit that this door, unlike Hall or
// the demo house, isn't an Enter-to-confirm interaction) -- Enter also
// works as a fallback exit from it, for anyone who doesn't think to walk
// back onto the doorway tile.
var mode = "outdoor";
var nearestInteractable = null;
var toast = { msg: "", until: 0 };
var savedOutdoorX = player.x, savedOutdoorY = player.y;

function showToast(msg, ms) {
  toast.msg = msg;
  toast.until = performance.now() + (ms || 1500);
}

function findNearestInteractable() {
  var ptx = Math.floor(player.x / TILE), pty = Math.floor(player.y / TILE);
  var best = null, bestDist = Infinity;
  var list = activeSpace.interactables;
  for (var i = 0; i < list.length; i++) {
    var it = list[i];
    var d = Math.max(Math.abs(ptx - it.x), Math.abs(pty - it.y));
    if (d <= 1 && d < bestDist) { best = it; bestDist = d; }
  }
  return best;
}

function enterInterior(key, space, entry) {
  savedOutdoorX = player.x; savedOutdoorY = player.y;
  activeSpace = space;
  player.x = entry.x * TILE + 8;
  player.y = entry.y * TILE + 8;
  mode = key;
  forceRedraw = true;
  // V2: no explicit network call needed here (or in exitToOutdoor) --
  // frame()'s network tick runs every frame regardless of mode and
  // already detects "current region !== netRegion" generically, so the
  // very next tick rekeys from wherever the player just was (outdoor
  // region or interior instance) into wherever they just arrived. Same
  // mechanism that already handled outdoor-to-outdoor crossings; this
  // just stopped special-casing interiors out of it.
}

function exitToOutdoor(spawnTile) {
  activeSpace = outdoorSpace;
  if (spawnTile) {
    player.x = spawnTile.x * TILE + 8;
    player.y = spawnTile.y * TILE + 8;
  } else {
    player.x = savedOutdoorX;
    player.y = savedOutdoorY;
  }
  mode = "outdoor";
  forceRedraw = true;
}

function tryInteract() {
  // A real interactable in range always wins -- checked first. Without
  // this order, standing right next to a lab station and pressing Enter
  // would just exit the lab (mode === "lab" caught it first) and a
  // station could never actually be reached (found while testing this
  // build, not by inspection: the toast never appeared no matter how
  // close to a station the player stood).
  var it = nearestInteractable;
  if (it) {
    if (it.type === "hall") {
      var ok = window.confirm("Open liquidgov.us/proposals in a new tab?");
      if (ok) window.open("https://liquidgov.us/proposals", "_blank", "noopener");
    } else if (it.type === "latch") {
      netSetHousePolicy(HOUSE_POLICY_NEXT[housePolicy]);
    } else if (it.type === "mineDoor") {
      enterInterior("mine", mineSpace, MINE_ENTRY);
    } else if (it.type === "demoDoor") {
      enterInterior("demoHouse", demoHouseSpace, DEMO_ENTRY);
    } else if (it.type === "friendDoor") {
      // Visiting (friends policy). A lit door only exists for a mutual
      // friend who is in the game, but the daemon is what actually decides
      // -- a refusal comes back through netHandleJoinResponse and walks the
      // visitor back out. Same interior template as every other house; what
      // makes it theirs is the instance key.
      if (it.friend && isBound) {
        visitingOwner = it.friend;
        INTERIOR_INSTANCE_KEY.visit = "instance:house:" + it.friend;
        enterInterior("visit", demoHouseSpace, DEMO_ENTRY);
      } else {
        showToast("Closed.");
      }
    } else if (it.type === "houseDoor") {
      // Same interior template as the demo house (V4: mechanically just
      // another shared-shape instance, like lab/demo already are) --
      // what makes it "yours" is the instance key, not different art.
      if (isBound) enterInterior("house", ownHouseSpace, DEMO_ENTRY);
      else showToast("Log in on LiquidGov to open your own house.");
    } else if (it.type === "labStation") {
      var r = LAB_RECIPES[it.recipe];
      inventory[r.item] = (inventory[r.item] || 0) + 1;
      lastPickedItem = r.item;
      netPickup(r.item);
      showToast(r.name + ".");
    } else if (it.type === "worldPickup") {
      // D: same loop as a lab station (look up, +1, never consumed). The
      // marker is never removed from the map.
      var wp = WORLD_PICKUPS[it.pickup];
      inventory[wp.item] = (inventory[wp.item] || 0) + 1;
      lastPickedItem = wp.item;
      netPickup(wp.item);
      showToast(wp.name + ".");
    } else if (it.type === "shutter") {
      showToast("Closed for now.");
    } else if (it.type === "sign") {
      showToast(it.text || "Closed.");
    } else if (it.type === "wallSlot") {
      if (mode === "visit") {
        // A visitor looks; only the owner places (the daemon enforces it
        // too, but a refused POST would read as a dead credential).
        showToast("That's " + visitingOwner + "'s wall.");
      } else if (!lastPickedItem) {
        showToast("Nothing to place yet -- visit the lab first.");
      } else {
        netPlace(lastPickedItem);
        showToast("Placed.");
      }
    }
    return;
  }

  // Nothing in range: in an interior, Enter is the fallback exit. The lab
  // spawn point is one tile south of the outdoor door -- the same spot
  // the walk-in trigger uses -- so there's no re-entry oscillation.
  if (mode === "demoHouse" || mode === "house" || mode === "visit") { exitToOutdoor(null); return; }
  if (mode === "lab") { exitToOutdoor({ x: LAB_DOOR_TILE.x, y: LAB_DOOR_TILE.y + 1 }); return; }
  if (mode === "mine") { exitToOutdoor({ x: MINE_DOOR_TILE.x, y: MINE_DOOR_TILE.y + 1 }); return; }
}

