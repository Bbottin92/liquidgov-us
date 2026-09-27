"use strict";
// ---- Interact system ----------------------------------------------------
// One prompt at doors/set pieces, no RPG menu (locked, art.md). Range is
// "on or next to the tile" -- Chebyshev distance <= 1.
// mode: "outdoor" | "demoHouse" | "lab" | "mine" | "workshop" | "house" (V4) | "visit" (a friend's house). The demo house keeps its P1
// Enter-to-enter/Enter-to-exit pattern unchanged. The Lab is a walk-in
// trigger both ways (atlas.md is explicit that this door, unlike Hall or
// the demo house, isn't an Enter-to-confirm interaction) -- Enter also
// works as a fallback exit from it, for anyone who doesn't think to walk
// back onto the doorway tile.
var mode = "outdoor";
var nearestInteractable = null;
var toast = { msg: "", until: 0 };
// BUILD studio 3: three lab stations stop granting a jar/crystal (their real ingredient is gathered and
// benched instead) and just point at where the real thing now lives, once per station per session --
// a Look every few seconds while standing there shouldn't spam the same line over and over.
var labStockToasted = {};
var savedOutdoorX = player.x, savedOutdoorY = player.y;

function showToast(msg, ms) {
  toast.msg = msg;
  toast.until = performance.now() + (ms || 1800);
  var el = document.getElementById("toast");
  if (el) el.textContent = msg;
}

function tickToast(now) {
  var el = document.getElementById("toast");
  if (!el) return;
  if (now < toast.until) {
    if (el.textContent !== toast.msg) el.textContent = toast.msg;
  } else if (el.textContent) el.textContent = "";
}

var lastAnnouncedPlace = "";
var placeUntil = 0;
function interiorPlaceName() {
  if (mode === "lab") return "the Lab";
  if (mode === "mine") return "the mine";
  if (mode === "workshop") return "the Workshop";
  if (mode === "demoHouse") return "the demo house";
  if (mode === "house") return "your house";
  if (mode === "visit") return visitingOwner ? (visitingOwner + "'s house") : "a house";
  return "";
}
function announcePlace(name) {
  if (!name || name === lastAnnouncedPlace) return;
  lastAnnouncedPlace = name;
  placeUntil = performance.now() + 2200;
  var el = document.getElementById("placeName");
  if (el) el.textContent = name;
}
function tickPlaceName(now) {
  var name = "";
  if (mode === "outdoor") {
    var r = regionAt(Math.floor(player.x / TILE), Math.floor(player.y / TILE));
    name = r ? r.name : "";
  } else {
    name = interiorPlaceName();
  }
  announcePlace(name);
  var el = document.getElementById("placeName");
  if (el && now >= placeUntil && el.textContent) el.textContent = "";
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
  if (mode === "house" || mode === "visit") {
    for (var f = 0; f < placedFloor.length; f++) {
      var fl = placedFloor[f];
      var fd = Math.max(Math.abs(ptx - fl.x), Math.abs(pty - fl.y));
      if (fd <= 1 && fd < bestDist) {
        best = { type: "floorItem", x: fl.x, y: fl.y, item: fl.item, prompt: mode === "house" ? "Take" : "Look" };
        bestDist = fd;
      }
    }
  }
  return best;
}

function tileInFront() {
  var tx = Math.floor(player.x / TILE), ty = Math.floor(player.y / TILE);
  if (player.dir === DIR_UP) ty -= 1;
  else if (player.dir === DIR_DOWN) ty += 1;
  else if (player.dir === DIR_LEFT) tx -= 1;
  else tx += 1;
  return { x: tx, y: ty };
}

function tryFloorPlace() {
  var t = tileInFront();
  if (t.x < 1 || t.y < 1 || t.x > 14 || t.y > 14) return false;
  if (t.x === DEMO_DOOR.x && (t.y === DEMO_DOOR.y || t.y === DEMO_ENTRY.y)) return false;
  if (activeSpace.solid[idxIn(activeSpace.w, t.x, t.y)]) return false;
  for (var i = 0; i < placedFloor.length; i++) {
    if (placedFloor[i].x === t.x && placedFloor[i].y === t.y) {
      showToast("Already something there.");
      return true;
    }
  }
  if (placedFloor.length >= 8) {
    showToast("No more room.");
    return true;
  }
  netPlaceFloor(lastPickedItem, t.x, t.y);
  showToast("Set down.");
  return true;
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
      var ok = window.confirm("Open proposals on LiquidGov? You'll need an account to vote.");
      if (ok) window.open("https://liquidgov.us/proposals", "_blank", "noopener");
    } else if (it.type === "latch") {
      netSetHousePolicy(HOUSE_POLICY_NEXT[housePolicy]);
    } else if (it.type === "mineDoor") {
      enterInterior("mine", mineSpace, MINE_ENTRY);
    } else if (it.type === "workshopDoor") {
      enterInterior("workshop", workshopSpace, WORKSHOP_ENTRY);
      // "Once on entering the workshop" (CLAUDE_STUDIO_3.md "When to fetch"). Guests never call
      // /presence/studio at all -- netFetchStudio() itself no-ops unless isBound.
      netFetchStudio();
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
      renderInvHud();
    } else if (it.type === "studioGather") {
      // BUILD studio 3: a bound player's gather is server-authoritative (cooldown + grant, one
      // daemon transaction); a guest gets a local, session-only version of the same cooldown and
      // never reaches the daemon at all (studio.md's "do not invent guest persistence", CLAUDE_STUDIO_3.md
      // "Let a guest call gather... " under Do not). Neither path sets lastPickedItem: these are
      // crafting materials, not wall/floor-placeable trophies (catalog.sql: wall_slot_ok = 0).
      netGather(it.spot);
    } else if (it.type === "labStock") {
      // The station's real ingredient moved to a gather spot or the Workshop bench; the station
      // itself grants nothing anymore, just points at where it went, once per session.
      if (!labStockToasted[it.stock]) {
        labStockToasted[it.stock] = true;
        showToast(it.toast);
      }
    } else if (it.type === "bench") {
      openBench();
    } else if (it.type === "notebook") {
      openNotebook();
    } else if (it.type === "easel" || it.type === "studioPortrait" || it.type === "studioPainting") {
      // BUILD studio 4 (CLAUDE_STUDIO_4.md decision 4): where you opened it is the canvas -- the
      // workshop table and the portrait frame both edit the portrait; the new frame edits the painting.
      // A visit still only ever toasts, same split the portrait already had.
      if (it.type === "studioPortrait" && mode === "visit") {
        showToast("A small portrait.");
      } else if (it.type === "studioPainting" && mode === "visit") {
        showToast("A small painting.");
      } else if (typeof openEasel === "function") {
        openEasel(it.type === "studioPainting" ? "painting" : "portrait");
      }
    } else if (it.type === "sign") {
      showToast(it.text || "Closed.");
    } else if (it.type === "wallSlot") {
      if (mode === "visit") {
        // A visitor looks; only the owner places (the daemon enforces it
        // too, but a refused POST would read as a dead credential).
        showToast("That's " + visitingOwner + "'s wall.");
      } else if (!lastPickedItem) {
        showToast("Nothing to place yet -- pick something up first.");
      } else {
        netPlace(lastPickedItem);
        showToast("Placed.");
      }
    } else if (it.type === "floorItem") {
      if (mode === "visit") {
        var finfo = PLACED_ITEM_INFO[it.item];
        showToast(finfo ? finfo.label : it.item);
      } else {
        netTakeFloor(it.x, it.y);
        lastPickedItem = it.item;
        showToast("Picked up.");
        renderInvHud();
      }
    }
    return;
  }

  // C2: in your own house, empty Enter facing a clear floor tile puts the
  // last pickup down (cap 8). Facing the door / a wall still leaves, same
  // as before -- place refuses those tiles first.
  if (mode === "house" && lastPickedItem && tryFloorPlace()) return;

  // Nothing in range: in an interior, Enter is the fallback exit. The lab
  // spawn point is one tile south of the outdoor door -- the same spot
  // the walk-in trigger uses -- so there's no re-entry oscillation.
  if (mode === "demoHouse" || mode === "house" || mode === "visit") { exitToOutdoor(null); return; }
  if (mode === "lab") { exitToOutdoor({ x: LAB_DOOR_TILE.x, y: LAB_DOOR_TILE.y + 1 }); return; }
  if (mode === "mine") { exitToOutdoor({ x: MINE_DOOR_TILE.x, y: MINE_DOOR_TILE.y + 1 }); return; }
  if (mode === "workshop") { exitToOutdoor({ x: WORKSHOP_DOOR_TILE.x, y: WORKSHOP_DOOR_TILE.y + 1 }); return; }
}

