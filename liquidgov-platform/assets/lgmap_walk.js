/* Walking on the map (Brandon 2026-11-13, 12:36 AM): the walker stands on a box. The touchpad or WASD/arrows picks the
   connector (the line to a neighboring box) that points most that way and walks along it to the next box; A or Enter
   opens the box you stand on (the same as tapping it), B or Esc goes back (closes the open page, else the map's Back).
   Uses lgmap.js's own drawing as it is: the boxes (#lgm-nodes .lgn[data-id]) and connectors (#lgm-edges .lge[data-a]
   [data-b]) on the screen, wherever its ring layout put them; this file lays nothing out. The Drop button, pad and keys are
   walker.js's (window.LG_WALKER, loaded before this file). Dropped in, the camera (lgmap.js's window.LG_MAP) zooms
   to LG_WALKER.CONTROLS.mapZoom and follows the walker; the boxes get depth (lgmap.css .lgm-dropping). */
(function () {
  'use strict';
  var root = document.getElementById('lgmap');
  if (!root || !window.LG_WALKER) return;
  var nodeLayer = document.getElementById('lgm-nodes'), edgeLayer = document.getElementById('lgm-edges');
  var at = null, walker = null, follow = 0, dropping = false, later = [], settle = 0, dropPortrait;
  var ZOOM = LG_WALKER.CONTROLS.mapZoom;
  var MIN_COS = 0.2;   // a connector counts for a direction when it points within ~78 degrees of it

  function box(id) { return id ? nodeLayer.querySelector('.lgn[data-id="' + (window.CSS && CSS.escape ? CSS.escape(id) : id) + '"]') : null; }
  function middle(el) { var r = el.getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + r.height / 2 }; }
  function shown(el) { return el.style.display !== 'none' && !el.classList.contains('lge-hiddenbox'); }
  // where to stand when nothing is chosen yet: the box last tapped, else the center of the map (you at the start)
  function startBox() {
    var sel = nodeLayer.querySelector('.lgn.lgn-sel') || box('you') || nodeLayer.querySelector('.lgn');
    return sel ? sel.getAttribute('data-id') : null;
  }
  // the boxes a connector from `id` leads to
  function neighbors(id) {
    var out = [];
    Array.prototype.forEach.call(edgeLayer.querySelectorAll('.lge'), function (e) {
      if (!shown(e)) return;
      var a = e.getAttribute('data-a'), b = e.getAttribute('data-b'), o = a === id ? b : (b === id ? a : null);
      if (o && box(o) && out.indexOf(o) < 0) out.push(o);
    });
    return out;
  }
  var WAY = { up: [0, -1], down: [0, 1], left: [-1, 0], right: [1, 0] };
  // the neighbor whose connector points most along the unit vector w (ties: the nearer one)
  function pick(w) {
    var here = box(at);
    if (!here) return null;
    var p = middle(here), best = null, bestScore = -Infinity;
    neighbors(at).forEach(function (id) {
      var q = middle(box(id)), dx = q.x - p.x, dy = q.y - p.y, len = Math.sqrt(dx * dx + dy * dy) || 1;
      var cos = (dx * w[0] + dy * w[1]) / len;
      if (cos < MIN_COS) return;
      var score = cos - len / 100000;
      if (score > bestScore) { bestScore = score; best = id; }
    });
    return best;
  }
  function show() {
    var el = box(at);
    Array.prototype.forEach.call(nodeLayer.querySelectorAll('.lgn.lgn-walk'), function (o) { if (o !== el) o.classList.remove('lgn-walk'); });
    if (!el) { walker.hidden = true; return; }
    el.classList.add('lgn-walk');
    var r = el.getBoundingClientRect();
    walker.hidden = false;
    walker.style.left = Math.round(r.left + r.width / 2) + 'px';
    walker.style.top = Math.round(r.top) + 'px';
    walker.setAttribute('data-at', at);
  }
  // the camera on the box you stand on; again a little later after the map moves by itself (a box opened, Back: the
  // map recenters and fits itself once its connections load)
  function camera() { if (dropping && window.LG_MAP && box(at)) LG_MAP.focus(at, ZOOM); }
  function cameraSoon() {
    later.forEach(clearTimeout);
    later = [350, 900, 1700].map(function (ms) { return setTimeout(camera, ms); });
  }
  function step(w, speed) {
    if (!box(at)) at = startBox();
    var next = pick(w);
    if (next) at = next;
    show();
    if (next) {
      camera();
      LG_WALKER.shape(dropPortrait, w[0] * (speed || 1), w[1] * (speed || 1));
      clearTimeout(settle);
      settle = setTimeout(function () { LG_WALKER.shape(dropPortrait, 0, 0); }, LG_WALKER.CONTROLS.shape.mapMoveMs);
    }
  }

  walker = document.createElement('div');
  walker.id = 'lgm-walker';
  dropPortrait = LG_WALKER.portrait(root.getAttribute('data-me'));
  walker.appendChild(dropPortrait);
  walker.hidden = true;
  walker.setAttribute('aria-hidden', 'true');
  document.body.appendChild(walker);

  window.addEventListener('resize', function () { if (dropping) cameraSoon(); });   // lgmap.js fits on a resize

  // The touchpad: a push past the dead zone walks one connector its way; held, it walks on, sooner the harder it is
  // pulled (every CONTROLS.touchpad.mapRepeatMs at full pull).
  var pullNow = null, again = 0;
  function padWalk() {
    clearTimeout(again);
    if (!pullNow || !dropping) return;
    var len = Math.sqrt(pullNow.x * pullNow.x + pullNow.y * pullNow.y);
    step([pullNow.x / len, pullNow.y / len], len);
    again = setTimeout(padWalk, LG_WALKER.CONTROLS.touchpad.mapRepeatMs / len);
  }
  function pullTo(x, y) {
    var was = pullNow;
    pullNow = x || y ? { x: x, y: y } : null;
    if (!pullNow) clearTimeout(again);
    else if (!was) padWalk();
  }

  var arrival = LG_WALKER.takeHandoff();   // came through the portal: Drop is on, standing at that page's box
  var drop = LG_WALKER.mount({
    // the switch sits in the map's own row (Back, the trail, Zoom out), so the map keeps its place on the page
    dropSlot: document.getElementById('lgm-zoomout'),
    dir: function (dir, down) { if (down) step(WAY[dir]); },
    pull: pullTo,
    // A / Enter: the same as tapping the box you stand on
    select: function () {
      var el = box(at);
      if (!el) return false;
      el.dispatchEvent(new MouseEvent('click', { bubbles: true, cancelable: true, view: window }));
      cameraSoon();
      return true;
    },
    // B / Esc: close the open page, else the map's own Back
    back: function () {
      var panel = document.getElementById('lgm-panel'), close = document.getElementById('lgm-close');
      var backBtn = document.getElementById('lgm-back');
      if (panel && !panel.hidden && close) close.click();
      else if (backBtn && !backBtn.disabled) backBtn.click();
      cameraSoon();
    },
    dropping: function (on) {
      dropping = on;
      root.classList.toggle('lgm-dropping', on);
      clearInterval(follow);
      pullTo(0, 0);
      clearTimeout(settle);
      LG_WALKER.shape(dropPortrait, 0, 0);
      later.forEach(clearTimeout);
      if (on) {
        if (!box(at)) at = startBox();
        show();
        camera();
        follow = setInterval(function () { if (!box(at)) at = startBox(); show(); }, 120);   // the map glides and recenters
      } else {
        walker.hidden = true;
        if (window.LG_MAP) LG_MAP.fit();
        Array.prototype.forEach.call(nodeLayer.querySelectorAll('.lgn.lgn-walk'), function (o) { o.classList.remove('lgn-walk'); });
      }
    }
  });
  if (arrival && window.LG_MAP && LG_MAP.ready) {
    LG_MAP.ready.then(function () { return arrival.box ? LG_MAP.goTo(arrival.box) : null; })
      .then(function () { if (arrival.box && box(arrival.box)) at = arrival.box; },
            function () { /* that box is not on your map (privacy): arrive at you */ })
      .then(function () { drop.start(); cameraSoon(); });
  }
})();
