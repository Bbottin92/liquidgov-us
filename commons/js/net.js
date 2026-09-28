"use strict";
// ---- Presence networking (commons-daemon, loopback) --------------------
// Wires the client to the real daemon for the first time (previously this
// was a named seam only -- see the comment above updatePasserby()).
// Server-issued guest ids (GROK_DAEMON_REVIEW.md should-fix #3): the
// daemon mints (id, secret) via POST /presence/guest and every
// join/move/rekey/leave call must present both. `id` is safe to show (the
// GET feed broadcasts it so other clients can render this occupant);
// `secret` never leaves this tab except in those authenticated calls.
//
// V1 scope, deliberately: outdoor only. Interiors use "instance:<id>"
// region keys that need a map_instances row, and presence.md is explicit
// that V1 doesn't wire SQLite for guests at all -- so entering a house or
// the lab calls netLeave() rather than pretending to track a region that
// doesn't exist yet.
//
// Should-fix (GROK_PRESENCE_REVIEW.md): polling now covers every region
// whose AABB intersects the camera, matching presence.md's tick-rate
// section literally ("A client subscribes to every region whose AABB
// intersects the camera... Not 'one tile-row of the neighbor'"), not just
// the single region the player's own occupant lives in. `netRegion` still
// means exactly one thing -- which region the player's own row is
// joined/rekeyed into on the daemon, the presence.md "per-region tick"
// membership -- polling breadth is a separate, wider concern layered on
// top of it.
// Should-fix (GROK_PRESENCE_REVIEW.md): "NET host is loopback :8765.
// Grok preview and liquidgov.us will not see WSL." True either way this
// constant is spelled -- neither a remote preview sandbox nor a real
// visitor's browser can reach a developer's own WSL loopback, no matter
// what URL the client asks for. The actual fix is "the nginx round"
// (relative path, same-origin proxy), explicitly out of scope here (no
// nginx, no deploy). What's cheap and worth doing now: stop forcing a
// source edit to point this at a different reachable daemon (a LAN IP,
// a tunnel, another dev box) -- an optional ?daemon= query param
// overrides the default without touching code. Doesn't fix reachability
// by itself; just removes the friction until the real proxy exists.
//
// Should-fix (GROK_DAEMON_URL_REVIEW.md): a shared link with
// ?daemon=https://evil.example would make this page mint a guest secret
// and POST it straight to the attacker -- harmless today only because
// this page is loopback-only. Gated *before* this file could ever be
// on a public origin, not after: honor ?daemon= only while the page
// itself is being viewed from loopback (on a real deploy, ignore the
// param outright and use the eventual relative proxy instead), and even
// then only accept an http: URL whose host is loopback or RFC1918
// private -- no https:, no javascript:, no arbitrary DNS name.
function isLoopbackPage() {
  var h = location.hostname;
  return h === "127.0.0.1" || h === "localhost" || h === "::1" || h === "[::1]";
}

function isTrustedDaemonHost(hostname) {
  if (hostname === "localhost" || hostname === "127.0.0.1" || hostname === "::1" || hostname === "[::1]") return true;
  var m = /^(\d{1,3})\.(\d{1,3})\.\d{1,3}\.\d{1,3}$/.exec(hostname);
  if (!m) return false;
  var a = Number(m[1]), b = Number(m[2]);
  return a === 127 || a === 10 || (a === 172 && b >= 16 && b <= 31) || (a === 192 && b === 168);
}

// Must-fix (GROK_DEPLOY_REVIEW.md / CLAUDE_DEPLOY_PREFLIGHT.md): a
// visitor loading this page from a real deploy is not on loopback, and
// the old unconditional "http://127.0.0.1:8765" default would have sent
// their browser to *their own* machine's loopback, not the droplet --
// curl on the droplet could prove nginx works, but no browser ever
// could have, until this branch existed. Off loopback, DAEMON becomes
// same-origin (`location.origin + "/commons"`); every existing call
// site already builds its own URL as `DAEMON + "/presence/..."`, so
// this composes correctly with nginx-commons.conf's
// `location /commons/presence/` with zero other changes needed -- e.g.
// `https://liquidgov.us/commons` + `/presence/join` is exactly the path
// that location matches. On loopback, behavior is unchanged: the
// validated `?daemon=` override (4e9bfbd) or the local dev default.
var DAEMON = (function () {
  var fallback = "http://127.0.0.1:8765";
  if (!isLoopbackPage()) return location.origin + "/commons";
  var raw = new URLSearchParams(location.search).get("daemon");
  if (!raw) return fallback;
  try {
    var u = new URL(raw);
    if (u.protocol !== "http:" || !isTrustedDaemonHost(u.hostname)) return fallback;
    return u.origin;
  } catch (e) {
    return fallback;
  }
})();
var NET_INTERVAL_MS = 500; // presence.md: per-region ~2Hz
// V2 (GROK_V2_ANSWER.md): demoHouse/lab are fixed, shared instances.
// V4 (GROK_V4_ANSWER.md) adds "house" -- per-bound-player, so its key
// isn't known at load time; netHandleJoinResponse fills it in once a
// cookie actually resolves. Absent (guest, or not bound yet), the
// house branch in frame()'s network tick just never fires -- the same
// "if (netReady && instanceKey)" guard already used for demoHouse/lab.
// "visit" (friends policy) is filled in per door -- whichever friend's house
// the player just walked into.
var INTERIOR_INSTANCE_KEY = { demoHouse: "instance:demo", lab: "instance:lab", mine: "instance:mine", workshop: "instance:workshop" };
var netId = null, netSecret = null, netReady = false;
// A bound player has two ids: the guest credential above (what this client
// authenticates with) and "u:<username>", the id the daemon actually files
// their occupant row under once a civic cookie resolves (cookie-bind.md).
// Every poll returns the caller's own row too; comparing only against netId
// never matched a bound player's, so they were drawn a second time as a
// lagging copy of themselves. Set from the join/rekey answers below.
var netBoundId = null;
function isSelfId(id) { return id === netId || (netBoundId !== null && id === netBoundId); }
var netRegion = null;        // an outdoor REGIONS entry, an interior instance key string, or null
var otherOccupants = [];     // merged poll across every camera-intersecting region, self filtered out
var occupantViz = Object.create(null); // id -> {x,y,tx,ty,dir,...} world pixels, lerped between 2Hz polls
var netOccupantsDirty = false;
var netPollBusy = false;     // one batch of camera-region GETs in flight at a time
var lastNetSendAt = -Infinity;

// Every REGIONS entry whose outdoor tile AABB intersects the camera's
// current viewport (in world tile coordinates) -- typically 1-4 regions
// on a 24x20 region grid per presence.md's own worked example.
function regionsIntersectingCamera(camX, camY) {
  var vx0 = Math.floor(camX / TILE), vy0 = Math.floor(camY / TILE);
  var vx1 = Math.floor((camX + crop.vw - 1) / TILE), vy1 = Math.floor((camY + crop.vh - 1) / TILE);
  var out = [];
  for (var i = 0; i < REGIONS.length; i++) {
    var ab = regionAABB(REGIONS[i]);
    if (ab.x0 <= vx1 && ab.x1 >= vx0 && ab.y0 <= vy1 && ab.y1 >= vy0) out.push(REGIONS[i]);
  }
  return out;
}

// V2 (GROK_V2_ANSWER.md): region is either an outdoor REGIONS entry
// (object) or a shared interior instance key (plain string, e.g.
// "instance:demo") -- presence.md always intended interior region ids
// to look like this, V1 just never joined one. A string region has no
// supermap AABB to offset by: the instance's own local tile space IS
// the whole region, same shape as an outdoor region's own local space,
// just with no (x0,y0) origin to add.
function regionKeyFor(region) { return typeof region === "string" ? region : (region.col + "," + region.row); }

// GROK_HOUSE_REVIEW.md / CLAUDE_HOUSE_KEY.md: a house key is
// "instance:house:" + a real lg_username, which civic already restricts
// to a URL-safe charset (valid_username(): alnum/dot/underscore/hyphen)
// -- nothing here would ever actually need escaping today. Still,
// anything landing in a URL *path* (not a JSON body field, which needs
// no encoding at all) should be encoded on principle rather than relying
// on that charset never changing underneath this code.
function regionPathSegment(region) { return encodeURIComponent(regionKeyFor(region)); }

function worldToRegionLocal(region, worldPx, worldPy) {
  if (typeof region === "string") return { x: worldPx / TILE, y: worldPy / TILE };
  var aabb = regionAABB(region);
  return { x: (worldPx / TILE) - aabb.x0, y: (worldPy / TILE) - aabb.y0 };
}

function regionLocalToWorld(region, lx, ly) {
  if (typeof region === "string") return { x: lx * TILE, y: ly * TILE };
  var aabb = regionAABB(region);
  return { x: (aabb.x0 + lx) * TILE, y: (aabb.y0 + ly) * TILE };
}

function clearOccupantViz() {
  occupantViz = Object.create(null);
}

// Turn a 2Hz snapshot list into viz targets. Same-region updates lerp;
// a region change (or first sight) snaps. Draw reads occupantViz.
function ingestOccupants(list) {
  var seen = Object.create(null);
  for (var i = 0; i < list.length; i++) {
    var o = list[i];
    seen[o.id] = true;
    var wp = regionLocalToWorld(o.region, o.x, o.y);
    var face = (typeof o.facing === "number" && o.facing >= 0 && o.facing <= 3) ? o.facing : DIR_DOWN;
    var key = regionKeyFor(o.region);
    var v = occupantViz[o.id];
    if (!v || v.regionKey !== key) {
      occupantViz[o.id] = {
        id: o.id, region: o.region, regionKey: key,
        x: wp.x, y: wp.y, tx: wp.x, ty: wp.y,
        dir: face, animFrame: 0, animTimer: 0,
        display_name: o.display_name,
        shirt_hex: o.shirt || ""
      };
    } else {
      v.tx = wp.x; v.ty = wp.y;
      v.dir = face;
      v.display_name = o.display_name;
      v.region = o.region;
      // BUILD wear a pigment: kept live-updated on every poll, not just at first sight, so a nearby
      // friend's colour change shows up within one 2Hz tick, not only on next region entry.
      v.shirt_hex = o.shirt || "";
    }
  }
  for (var id in occupantViz) if (!seen[id]) delete occupantViz[id];
  otherOccupants = list;
  netOccupantsDirty = true;
}

function stepOccupants(dt) {
  var moved = false;
  var speed = player.speed;
  for (var id in occupantViz) {
    var v = occupantViz[id];
    var dx = v.tx - v.x, dy = v.ty - v.y;
    var dist = Math.sqrt(dx * dx + dy * dy);
    if (dist < 0.4) {
      if (v.x !== v.tx || v.y !== v.ty) { v.x = v.tx; v.y = v.ty; moved = true; }
      v.animFrame = 0;
      continue;
    }
    moved = true;
    if (dist > 96) { v.x = v.tx; v.y = v.ty; v.animFrame = 0; continue; }
    var step = speed * dt;
    if (step >= dist) { v.x = v.tx; v.y = v.ty; }
    else { v.x += (dx / dist) * step; v.y += (dy / dist) * step; }
    if (Math.abs(dx) > Math.abs(dy)) v.dir = dx < 0 ? DIR_LEFT : DIR_RIGHT;
    else v.dir = dy < 0 ? DIR_UP : DIR_DOWN;
    v.animTimer += dt;
    if (v.animTimer > 0.18) { v.animTimer = 0; v.animFrame = v.animFrame ? 0 : 1; }
  }
  return moved;
}

function netBody(extra) {
  var body = { id: netId, secret: netSecret };
  for (var k in extra) body[k] = extra[k];
  return JSON.stringify(body);
}

// ---- netMint:begin (tests/netmint_unit.js runs THIS block, extracted verbatim: keep it self-contained)
// The guest credential mint. A credential is accepted ONLY from a 200 answer whose body has a non-empty string
// `id` and `secret`: the old version never looked at the status and read whatever JSON came back as
// credentials (a JSON error body would have set netReady with id undefined and looped on 403s), and any failure
// (a 429 from nginx's /guest limit, a 502 while the daemon restarts, a network blip) left the player local-only
// until they reloaded the page. Now a failure retries with capped exponential backoff, forever but quietly: the
// game keeps running local-only meanwhile, and the next frame() tick joins as soon as a credential arrives (the
// same path as a late first mint). A Retry-After header, if a server ever sends one, is honoured (capped).
// One mint loop at a time: startup and netHandleAuthFailure() share it instead of stacking.
var NET_MINT_BASE_MS = 2000, NET_MINT_CAP_MS = 30000, NET_MINT_RETRY_AFTER_CAP_MS = 120000;
var netMintAttempt = 0;      // consecutive failures so far; a success resets it
var netMintPending = null;   // the mint loop in flight (a promise that resolves once a credential is in hand)
function netMintBackoffMs(attempt, retryAfterSec) {
  var ms = Math.min(NET_MINT_CAP_MS, NET_MINT_BASE_MS * Math.pow(2, attempt));
  ms *= 0.8 + Math.random() * 0.4;   // +/-20%: a room of clients behind one address must not retry in lockstep
  if (retryAfterSec > 0) ms = Math.max(ms, Math.min(retryAfterSec * 1000, NET_MINT_RETRY_AFTER_CAP_MS));
  return ms;
}
function netMint() {
  if (netMintPending) return netMintPending;
  netMintPending = new Promise(function (resolve) {
    function attempt() {
      var retryAfter = 0;
      Promise.resolve().then(function () { return fetch(DAEMON + "/presence/guest", { method: "POST" }); })
        .then(function (r) {
          if (!r.ok) {
            var ra = r.headers && r.headers.get ? parseInt(r.headers.get("Retry-After"), 10) : 0;
            retryAfter = ra > 0 ? ra : 0;
            throw new Error("presence/guest answered " + r.status);
          }
          return r.json();
        })
        .then(function (j) {
          if (!j || typeof j.id !== "string" || !j.id || typeof j.secret !== "string" || !j.secret)
            throw new Error("presence/guest answered 200 without a credential");
          netId = j.id; netSecret = j.secret; netReady = true;
          netMintAttempt = 0; netMintPending = null;
          resolve();
        })
        .catch(function (e) {
          var wait = netMintBackoffMs(netMintAttempt, retryAfter);
          netMintAttempt++;
          if (netMintAttempt === 1 || netMintAttempt % 10 === 0)
            console.warn("Commons: presence/guest failed (attempt " + netMintAttempt + ") -- local-only until it answers; retrying in " +
              Math.round(wait / 1000) + "s", e);
          setTimeout(attempt, wait);
        });
    }
    attempt();
  });
  return netMintPending;
}

// Must-fix (GROK_PRESENCE_REVIEW.md / CLAUDE_PRESENCE_PATCH.md): the
// daemon's guest credential window is now generous (guest.hpp, 30min),
// but a credential can still go bad -- most likely the daemon itself
// restarted mid-session, wiping every in-RAM GuestRegistry entry. Rather
// than getting stuck silently 403ing forever, mint a fresh credential and
// rejoin from wherever the player currently is. Guarded by
// netReauthInFlight so one bad response doesn't fire several remints at
// once (join/rekey/move are mutually exclusive per tick, but a slow
// in-flight request racing a fresh one is still possible).
var netReauthInFlight = false;
function netHandleAuthFailure() {
  if (netReauthInFlight) return;
  netReauthInFlight = true;
  netReady = false;
  netRegion = null;
  otherOccupants = [];
  netMint().then(function () {
    // netMint() resolves only once a credential is in hand (it retries until then), so this flag stays set
    // for as long as the remint takes and one outage cannot start several overlapping remints.
    netReauthInFlight = false;
    // No mode-specific rejoin here on purpose: the next frame() network
    // tick (outdoor or interior branch, whichever `mode` currently is)
    // sees netRegion === null and rejoins on its own -- the exact same
    // path used at initial login, so this doesn't need to know whether
    // the player is outdoors or in a shared interior right now.
  });
}
// ---- netMint:end

// ---- netStatus:begin (tests/netstatus_unit.js runs THIS block, extracted verbatim: keep it self-contained)
// The "Reconnecting…" badge. It only OBSERVES: it sends no request and changes no retry or backoff. Before this, a visitor got no
// signal at all when the client could not reach the daemon (moves failed silently, the player kept walking a local copy of the
// world and vanished for everyone else). The badge shows when EITHER
//   - a credential has been missing for more than 4s: a mint loop (startup, or a remint after a 403) pending that long, or
//   - 6 heartbeats in a row have failed: the move POST is the heartbeat (exactly one per tick, so 6 = 3s at 2Hz; the 1-4 polls per
//     tick are NOT counted, or an ordinary daemon restart would show the badge after about a second).
// It hides on the next success. There is deliberately no "stay clear" timer: the two thresholds are the anti-flap. A daemon that
// answers (200, or a 403 that the remint path handles) counts as reached; a network error, a 5xx from nginx, or a 429 does not.
var NET_STATUS_MINT_MS = 4000, NET_STATUS_FAILS = 6;
var netStatusFails = 0;        // consecutive failed heartbeats; any success resets it
var netMintSeenAt = null;      // when the current mint loop was first seen pending (frame time), or null
var netStatusShown = false;
function netStatusReport(reached) { if (reached) netStatusFails = 0; else netStatusFails++; }
function netStatusTick(t) {    // once per frame, with the frame time
  if (netMintPending) { if (netMintSeenAt === null) netMintSeenAt = t; } else netMintSeenAt = null;
  var noCredential = netMintSeenAt !== null && t - netMintSeenAt > NET_STATUS_MINT_MS;
  var show = noCredential || netStatusFails >= NET_STATUS_FAILS;
  if (show === netStatusShown) return;
  netStatusShown = show;
  var el = document.getElementById("connStatus");
  if (el) el.textContent = show ? "Reconnecting…" : "";     // the element is :empty when clear (display:none), and a live region
}
// ---- netStatus:end

// V2 (GROK_V2_ANSWER.md): join/rekey/move are no longer outdoor-only --
// `region` can be an outdoor REGIONS entry or a shared interior instance
// key ("instance:demo" / "instance:lab"). worldToRegionLocal/regionKeyFor
// already handle both, so one set of functions covers both cases.
// GROK_NAME_REVIEW.md: draw the *accepted* name (post-disambiguation),
// never the raw typed input -- the daemon may have appended " (2)" etc.
// Falls back to playerName until the first join response lands.
var acceptedDisplayName = null;
// V4 (GROK_V4_ANSWER.md): "bound: true" on join/rekey, so the client
// doesn't have to guess bound state from the nameplate string. Set from
// every response (GROK_HOUSE_REVIEW.md should-fix), not latched true --
// a stale isBound from an earlier session state should not survive a
// fresh guest reauth.
var isBound = false;
// Whose house I'm standing in while mode === "visit" (friends policy).
var visitingOwner = null;
// BUILD wear a pigment: this client's OWN worn colour, "" for the default blue. Set from every
// join/rekey response (never latched true the way isBound explicitly isn't either) and from a
// successful /presence/wear call. Read by the actor draw path to recolour my own sprite the same way
// otherOccupants' own `shirt` field recolours everyone else's.
var myShirtHex = "";

// The wall-slot portrait: civic's `/canvas-frame/<owner>/latest` (same
// origin as this page on liquidgov.us; the browser sends the viewer's own
// cookie, which is what lets civic apply its block check). Any failure --
// 404 for no drawing, a block, an unknown user, or no civic at all on a local
// dev page -- simply means no portrait: the slot stays as it always was.
// Never probed by index, never fetched by the daemon (nginx.md).
var wallPortrait = { owner: null, img: null, ok: false };

function wallOwnerFor(region) {
  var prefix = "instance:house:";
  if (typeof region !== "string" || region.indexOf(prefix) !== 0) return null;
  return region.slice(prefix.length) || null;
}

function loadWallPortrait(owner) {
  if (wallPortrait.owner === owner) return;   // called every frame; only acts on a change
  var img = null;
  wallPortrait = { owner: owner, img: null, ok: false };
  if (owner) {
    img = new Image();
    wallPortrait.img = img;
    img.onload = function () { if (wallPortrait.img === img) { wallPortrait.ok = true; forceRedraw = true; } };
    img.src = "/canvas-frame/" + encodeURIComponent(owner) + "/latest";
  }
  forceRedraw = true;
}

// Bound inventory. The daemon answers from my own session cookie (no name
// is ever sent), most recent pickup first. Failure is silent: the local
// pickup still works, it just won't be remembered.
var inventoryLoaded = false;
var studioLoaded = false;  // BUILD studio 3: mirrors inventoryLoaded, but for GET /presence/studio (studio.js)

function netLoadInventory() {
  fetch(DAEMON + "/presence/inventory")
    .then(function (r) { return r.json(); })
    .then(function (j) {
      var items = (j && j.items) || [];
      for (var i = 0; i < items.length; i++) inventory[items[i].item] = items[i].qty;
      // Only fill the "what does the wall place" default if nothing was
      // picked up yet this session -- a lookup made before this answer
      // landed is the more recent one.
      if (items.length && !lastPickedItem) lastPickedItem = items[0].item;
      renderInvHud();
    })
    .catch(function () { inventoryLoaded = false; });
}

function netPickup(item) {
  if (!isBound || !netReady) return;
  fetch(DAEMON + "/presence/pickup", { method: "POST", body: netBody({ item: item }) }).catch(function () {});
}

// House policy: lock or unlock my own door. The daemon takes identity from my
// session cookie only (a guest is refused, and there is no username to send),
// and answers with the policy it stored -- the latch and the toast follow that
// answer, not my guess, so a failed request never leaves the label lying.
var housePolicyBusy = false;
function netSetHousePolicy(policy) {
  if (!isBound || !netReady || mode !== "house" || housePolicyBusy) return;
  housePolicyBusy = true;
  fetch(DAEMON + "/presence/house-policy", { method: "POST", body: netBody({ policy: policy }) })
    .then(function (r) { return r.json(); })
    .then(function (j) {
      if (j && j.ok && HOUSE_POLICY_NEXT[j.policy]) {
        setHousePolicy(j.policy);
        showToast(HOUSE_POLICY_TOAST[housePolicy], 2200);
        forceRedraw = true;
      } else {
        showToast("Couldn't change the door.");
      }
    })
    .catch(function () { showToast("Couldn't change the door."); })
    .finally(function () { housePolicyBusy = false; });
}

// A refused or ended visit walks the visitor back outside. The next frame's
// ordinary region-mismatch tick rekeys them out of the optimistic visit key
// -- the same mechanism every other door already relies on.
function netVisitEnded(msg) {
  if (mode !== "visit") return;
  showToast(msg, 2200);
  exitToOutdoor(null);
}

function netHandleJoinResponse(r) {
  if (r.status === 403) {
    // 403 means two different things: a dead credential (remint), or the
    // daemon declining a *visit*. Only the reason tells them apart -- treating
    // a declined visit as a dead credential would remint and retry forever.
    r.json().then(function (j) {
      if (j && j.reason === "not_your_house" && mode === "visit") netVisitEnded("They aren't taking visitors right now.");
      else netHandleAuthFailure();
    }).catch(function () { netHandleAuthFailure(); });
    return;
  }
  r.json().then(netApplyJoinJson).catch(function () {});
}

// Everything a join/rekey answer teaches the client about itself. Also used for
// a move answer that says `promoted` (a mid-session login the daemon has
// relabelled in place), which carries the same bound/username/display_name.
function netApplyJoinJson(j) {
  {
    if (j && j.display_name && j.display_name !== acceptedDisplayName) {
      acceptedDisplayName = j.display_name;
      forceRedraw = true; // the name can change (or first appear) with nothing else moving that frame
    }
    if (j) {
      isBound = !!j.bound;
      // My own occupant row is "u:<username>" once bound. Drop any copy of it
      // already sitting in the last poll instead of drawing it until the next
      // one lands.
      netBoundId = (j.bound && j.username) ? "u:" + j.username : null;
      if (netBoundId) {
        otherOccupants = otherOccupants.filter(function (o) { return o.id !== netBoundId; });
        delete occupantViz[netBoundId];
      }
      // Bound: pull my saved pickups once per bind. Guests never do.
      if (j.bound && !inventoryLoaded) { inventoryLoaded = true; netLoadInventory(); }
      if (!j.bound) inventoryLoaded = false;
      // BUILD studio 3 "When to fetch": once after a bound join (studio.js). netFetchStudio() itself
      // guards on isBound too, so this is never reached for a guest join.
      if (j.bound && !studioLoaded) { studioLoaded = true; netFetchStudio(); }
      if (!j.bound) studioLoaded = false;
      // GROK_HOUSE_REVIEW.md must-fix: display_name is the *disambiguated*
      // Square nameplate ("brandon (2)" if a guest already typed
      // "brandon") -- never build the house key from it. "username" is
      // the raw, unsuffixed lg_username the daemon only ever sends when
      // actually bound; that's the one safe to key a house off of.
      if (j.bound && j.username) INTERIOR_INSTANCE_KEY.house = "instance:house:" + j.username;
      if (typeof j.house_policy === "string") setHousePolicy(j.house_policy);
      // BUILD wear a pigment: absent (guest, or a bound player wearing nothing) means the default blue,
      // same "missing field = default" rule every other sparse field on this response already follows.
      myShirtHex = typeof j.shirt === "string" ? j.shirt : "";
      // V5 (GROK_V5_ANSWER.md): "walking back in later resumes there, not
      // the fixed entry tile every time." The daemon only ever sends
      // resume_* when this join/rekey actually landed in the caller's own
      // house with an existing snapshot -- guarded on mode === "house" too
      // so a resume for a request the player has since walked away from
      // (a fast door-in-door-out) can't reposition them somewhere else.
      if (j.resume_x !== undefined && j.resume_y !== undefined && mode === "house") {
        var world = regionLocalToWorld(netRegion, j.resume_x, j.resume_y);
        player.x = world.x + 8; player.y = world.y + 8;
        if (j.resume_facing !== undefined) player.dir = j.resume_facing;
        forceRedraw = true;
      }
    }
  }
}

function netJoin(region) {
  if (!netReady) return;
  var lp = worldToRegionLocal(region, player.x, player.y);
  netRegion = region; // optimistic -- matches the existing join_locked/rekey de-dupe on the daemon side either way
  otherOccupants = []; // stale data from whatever region we were just in shouldn't render for a frame in the new one
  clearOccupantViz();
  placedItem = null;   // likewise -- a different room's placed item must not bleed into this one's render
  placedFloor = [];
  fetch(DAEMON + "/presence/join", {
    method: "POST",
    body: netBody({ region: regionKeyFor(region), x: lp.x, y: lp.y, facing: player.dir, display_name: playerName })
  }).then(netHandleJoinResponse).catch(function () {});
}

function netRekey(fromRegion, toRegion) {
  if (!netReady) return;
  var lp = worldToRegionLocal(toRegion, player.x, player.y);
  netRegion = toRegion;
  otherOccupants = [];
  clearOccupantViz();
  placedItem = null;
  placedFloor = [];
  fetch(DAEMON + "/presence/rekey", {
    method: "POST",
    body: netBody({
      from_region: regionKeyFor(fromRegion), to_region: regionKeyFor(toRegion),
      x: lp.x, y: lp.y, facing: player.dir, display_name: playerName
    })
  }).then(netHandleJoinResponse).catch(function () {});
}

function netMove() {
  if (!netReady || !netRegion) return;
  var lp = worldToRegionLocal(netRegion, player.x, player.y);
  fetch(DAEMON + "/presence/move", {
    method: "POST",
    body: netBody({ region: regionKeyFor(netRegion), x: lp.x, y: lp.y, facing: player.dir })
  }).then(function (r) {
    netStatusReport(r.ok || r.status === 403);   // the heartbeat for the connection badge: did the daemon answer?
    if (r.status === 403) { netHandleAuthFailure(); return null; }
    return r.json();
  }, function (e) { netStatusReport(false); throw e; }).then(function (j) {   // (a network error: count it, then fall to the same silent catch as before)
    // Answered 200 on purpose (a 403 would look like a dead credential): the
    // owner unfriended or blocked me, and the daemon has already removed me.
    if (j && j.reason === "not_welcome") netVisitEnded("You're no longer welcome there.");
    // Logged in mid-session: the daemon relabelled my row `u:<name>` in place.
    if (j && j.promoted) netApplyJoinJson(j);
  }).catch(function () {});
}

// Single-region poll for a shared interior instance -- there's exactly
// one instance:demo and one instance:lab (V2, not per-player houses), so
// unlike outdoor there's no camera-neighbor set to union across.
function netPollCurrentRegion() {
  if (!netReady || !netRegion || netPollBusy) return;
  netPollBusy = true;
  fetch(DAEMON + "/presence/" + regionPathSegment(netRegion))
    .then(function (r) { return r.json(); })
    .then(function (list) {
      var region = netRegion;
      ingestOccupants(list.filter(function (o) { return !isSelfId(o.id); }).map(function (o) {
        return { region: region, id: o.id, display_name: o.display_name, x: o.x, y: o.y, facing: o.facing, shirt: o.shirt };
      }));
    })
    .catch(function () {})
    .finally(function () { netPollBusy = false; });
}

// V2 stretch: lab output placed in a shared interior's wall slot as a
// game tile (GROK_V2_ANSWER.md). One placed item per instance, RAM only
// on the daemon side (PlacedStore) -- polled the same throttled cadence
// as presence, not its own faster loop.
var placedItem = null;    // last polled slug for the CURRENT interior instance, or null
var placedFloor = [];     // C2: [{item, x, y}, ...] in the current house; empty elsewhere
var netPlacedPollBusy = false;

function netPlace(item) {
  if (!netReady || !netRegion || typeof netRegion !== "string") return;
  placedItem = item; // optimistic, same shape as netJoin/netRekey's own region assignment
  forceRedraw = true;
  fetch(DAEMON + "/placed/" + regionPathSegment(netRegion), {
    method: "POST",
    body: netBody({ item: item })
  }).then(function (r) { if (r.status === 403) netHandleAuthFailure(); }).catch(function () {});
}

function netPlaceFloor(item, x, y) {
  if (!netReady || !netRegion || typeof netRegion !== "string") return;
  placedFloor.push({ item: item, x: x, y: y });
  forceRedraw = true;
  fetch(DAEMON + "/placed/" + regionPathSegment(netRegion), {
    method: "POST",
    body: netBody({ item: item, x: x, y: y })
  }).then(function (r) {
    if (r.status === 403) netHandleAuthFailure();
    if (r.status === 409) showToast("No more room.");
  }).catch(function () {});
}

function netTakeFloor(x, y) {
  if (!netReady || !netRegion || typeof netRegion !== "string") return;
  placedFloor = placedFloor.filter(function (f) { return !(f.x === x && f.y === y); });
  forceRedraw = true;
  fetch(DAEMON + "/placed/" + regionPathSegment(netRegion), {
    method: "POST",
    body: netBody({ item: "", x: x, y: y, pick: 1 })
  }).then(function (r) { if (r.status === 403) netHandleAuthFailure(); }).catch(function () {});
}

function netPollPlaced() {
  if (!netReady || !netRegion || typeof netRegion !== "string" || netPlacedPollBusy) return;
  netPlacedPollBusy = true;
  fetch(DAEMON + "/placed/" + regionPathSegment(netRegion))
    .then(function (r) { return r.json(); })
    .then(function (j) {
      var item = (j && j.item) ? j.item : null;
      var floor = (j && j.floor) ? j.floor : [];
      var floorDirty = JSON.stringify(floor) !== JSON.stringify(placedFloor);
      if (item !== placedItem || floorDirty) {
        placedItem = item;
        placedFloor = floor;
        forceRedraw = true;
      }
    })
    .catch(function () {})
    .finally(function () { netPlacedPollBusy = false; });
}

// V6 friends query (GROK_V6_ANSWER.md): which of my mutual friends are in
// the game right now, for Homestead lots 2-7. The daemon answers from my
// own session cookie (never a name I send), so this only means anything
// for a bound player -- a guest has no friends to ask about and never
// polls. Only polled while the Homestead facade is actually on screen, and
// throttled well under the daemon's own short TTL cache.
var LOT_POLL_MS = 5000;
var lastLotPollAt = -Infinity;
var netLotsBusy = false;

function netPollLots(regions, t) {
  if (!netReady || !isBound || netLotsBusy) return;
  var near = regions.some(function (r) { return r.name === "Homestead Row"; });
  if (!near || t - lastLotPollAt < LOT_POLL_MS) return;
  lastLotPollAt = t;
  netLotsBusy = true;
  fetch(DAEMON + "/presence/lots")
    .then(function (r) { return r.json(); })
    .then(function (j) {
      var lots = (j && j.lots) || [];
      for (var i = 0; i < FRIEND_LOT_DOORS.length; i++) {
        var door = FRIEND_LOT_DOORS[i], friend = lots[i] || null;
        if (door.friend !== friend) {
          door.friend = friend;
          door.prompt = friend ? "Visit" : "Away";
          forceRedraw = true;
        }
      }
    })
    .catch(function () {})
    .finally(function () { netLotsBusy = false; });
}

function netPollRegions(regions) {
  if (!netReady || !regions.length || netPollBusy) return;
  netPollBusy = true;
  Promise.all(regions.map(function (region) {
    return fetch(DAEMON + "/presence/" + regionPathSegment(region))
      .then(function (r) { return r.json(); })
      .then(function (list) {
        return list
          .filter(function (o) { return !isSelfId(o.id); })
          .map(function (o) {
            return { region: region, id: o.id, display_name: o.display_name, x: o.x, y: o.y, facing: o.facing, shirt: o.shirt };
          });
      })
      .catch(function () { return []; }); // one region's GET failing shouldn't drop every other region's occupants
  })).then(function (perRegion) {
    ingestOccupants(perRegion.reduce(function (acc, list) { return acc.concat(list); }, []));
  }).finally(function () { netPollBusy = false; });
}

// Tab close, reload, or navigating away, plus entering an interior.
// sendBeacon looked like the standard choice for the unload case, but its
// JSON Blob body makes this a cross-origin *preflighted* request, and
// testing against this daemon live showed the actual POST failing
// (net::ERR_FAILED) after a successful OPTIONS -- while sendBeacon()
// still returned true, since that return only means "queued," not
// "delivered." A silent, undetectable failure on the one call meant to
// guarantee cleanup is worse than not trying sendBeacon at all. Plain
// fetch with keepalive is already proven working against this daemon
// (every join/move/rekey call uses it) and survives page unload the same
// way beacon is meant to -- though nothing guarantees delivery on an
// abrupt close either way, which is exactly what PresenceStore's own
// idle reap (30s) exists to backstop.
function netLeave() {
  if (!netReady || !netRegion) return;
  var body = netBody({ region: regionKeyFor(netRegion) });
  fetch(DAEMON + "/presence/leave", { method: "POST", body: body, keepalive: true }).catch(function () {});
  netRegion = null;
  otherOccupants = [];
  clearOccupantViz();
}
window.addEventListener("pagehide", netLeave);

function otherRealOccupantsInRegion(region) {
  if (!region || !netReady) return 0;
  var n = 0;
  for (var i = 0; i < otherOccupants.length; i++) {
    if (otherOccupants[i].region === region) n++;
  }
  return n;
}

