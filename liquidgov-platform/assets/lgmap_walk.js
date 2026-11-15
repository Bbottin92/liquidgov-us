/* Walking on the map (Brandon 2026-11-13; free walking 2026-11-14: "it should behave just like any where else as far
   as the droplet movement goes"). Your drop moves freely over the map, the way a dot moves over a profile page: the
   touchpad or WASD/arrows walk it at walker.js's speed. A box is an area: step onto it and you are in it (its
   instance); A or Enter opens the box you stand on, or the one right next to you (the same as tapping it); B or Esc
   goes back (closes the open page, else the map's Back).
   Walking is always on: there is no Drop/Scroll switch. The view starts zoomed in around your drop
   (LG_WALKER.CONTROLS.mapZoom times the fitted view) and, zoomed in, only the box you are at and the boxes connected to
   it are shown. Zoom out yourself (a pinch on the map, the wheel, a trackpad pinch) and the whole map is shown; zoom
   back in and it narrows again. The camera always keeps your drop in the middle of the part of
   the map you can see. Tap or click your drop for its menu (drip speed).
   Uses lgmap.js's own drawing as it is: the boxes (#lgm-nodes .lgn[data-id]), their regions (LG_MAP.instance(id)
   .region, map units), the connectors (#lgm-edges .lge[data-a][data-b]) and the camera (LG_MAP.view / setView /
   toScreen); this file lays nothing out. The pad, keys and drop menu are walker.js's (window.LG_WALKER). */
(function () {
  'use strict';
  var root = document.getElementById('lgmap');
  if (!root || !window.LG_WALKER || !window.LG_MAP || !LG_MAP.setView) return;
  var W = LG_WALKER, C = W.CONTROLS, MAP = LG_MAP;
  var nodeLayer = document.getElementById('lgm-nodes'), edgeLayer = document.getElementById('lgm-edges');
  var svg = document.getElementById('lgm-svg');
  var MAINSTREAM = 'mainstream';
  var me = null;                              // where the drop is, in map units; null until the map has drawn
  var at = '', last = '', instanceId = MAINSTREAM;   // the box you are in ('' = none), the last one you were in, the instance
  var held = {}, pull = { x: 0, y: 0 }, anchor = null, asked = '';
  var myName = root.getAttribute('data-me') || '';
  var userZoom = 0, userOut = false, rezoom = 0;

  function box(id) { return id ? nodeLayer.querySelector('.lgn[data-id="' + (window.CSS && CSS.escape ? CSS.escape(id) : id) + '"]') : null; }
  function shown(el) { return el.style.display !== 'none' && !el.classList.contains('lge-hiddenbox'); }
  function region(id) { var r = MAP.instance(id); return r && r.region ? r.region : null; }
  // every box drawn on the map, with its region
  function boxes() {
    var out = [];
    Array.prototype.forEach.call(nodeLayer.querySelectorAll('.lgn[data-id]'), function (el) {
      if (el.style.display === 'none') return;
      var id = el.getAttribute('data-id'), r = region(id);
      if (r) out.push({ id: id, el: el, r: r });
    });
    return out;
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
  // spatial steps 6-7: the entry path. A box is entered only through its valve (data-valve, -owner, -invite on the box):
  //   open: anyone; closed: no one; private: only its owner; invite: only people on its list;
  //   request: turned away at its edge, and a request is recorded for the owner (data-requested-by, and an
  //   'lg-valve-request' event), once per approach; conduit: you walk across it but are never in it.
  // Returns 'in', 'through' or 'no'.
  function entry(id, record) {
    var el = box(id);
    if (!el) return 'no';
    var type = el.getAttribute('data-valve') || 'open';
    if (type === 'open') return 'in';
    if (type === 'conduit') return 'through';
    if (type === 'private') return myName && el.getAttribute('data-valve-owner') === myName ? 'in' : 'no';
    if (type === 'invite') return myName && (el.getAttribute('data-valve-invite') || '').split(',').indexOf(myName) >= 0 ? 'in' : 'no';
    if (type === 'request' && record && asked !== id) {
      asked = id;
      el.setAttribute('data-requested-by', myName);
      document.dispatchEvent(new CustomEvent('lg-valve-request', { detail: { box: id, by: myName } }));
    }
    return 'no';   // closed, request, and any type this file does not know
  }
  // is the point in the region? The same rule as assets/spatial.js isInside (spatial step 2, the edge counts as
  // inside); that file is not served to pages, so the one line is repeated here.
  function isInside(p, r) { return Math.abs(p.x - r.cx) <= r.hw && Math.abs(p.y - r.cy) <= r.hh; }
  function holding(all, p) { return all.filter(function (b) { return isInside(p, b.r); }); }
  // the box you are in: the smallest enterable box whose region holds the point
  function insideOf(all, p) {
    var best = null;
    holding(all, p).forEach(function (b) {
      if (entry(b.id, false) !== 'in') return;
      if (!best || b.r.hw * b.r.hh < best.r.hw * best.r.hh) best = b;
    });
    return best ? best.id : '';
  }
  function nearestBox(all, p) {
    var best = null, bestD = Infinity;
    all.forEach(function (b) {
      var d = Math.hypot(b.r.cx - p.x, b.r.cy - p.y);
      if (d < bestD) { bestD = d; best = b; }
    });
    return best ? best.id : '';
  }

  // spatial step 10: an admin's CSS for the instance you are Inside, as text in one <style> (never markup), nested under
  // #lgm-panel so it styles only what is inside the side panel, with the panel itself held in a paint-contained box.
  // The server only delivers CSS that passed sanitize_author_css. Gone again when you leave the instance.
  var cssFor = null;
  function scopeCss() {
    if (cssFor === instanceId) return;
    cssFor = instanceId;
    var rec = instanceId !== MAINSTREAM ? MAP.instance(instanceId) : null;
    var panel = document.getElementById('lgm-panel'), st = document.getElementById('lgm-instance-css');
    if (!rec || !rec.css || !panel) { if (st) st.parentNode.removeChild(st); if (panel) panel.removeAttribute('data-instance'); return; }
    if (!st) { st = document.createElement('style'); st.id = 'lgm-instance-css'; document.head.appendChild(st); }
    st.textContent = '#lgm-panel *{' + rec.css + '}#lgm-panel[data-instance]{contain:paint;isolation:isolate}';
    panel.setAttribute('data-instance', instanceId);
  }

  // The zoom you walk at, and the scale under which you count as zoomed out (halfway back to the fitted view).
  function walkScale() { return Math.min(MAP.ZOOM.max, MAP.fitScale() * C.mapZoom); }
  function zoomedIn() {
    var fit = MAP.fitScale(), walk = walkScale();
    return MAP.view().scale >= fit + (walk - fit) / 2 - 0.001;
  }
  // spatial step 4: zoomed in, only the box you are at (the one you are in, else the one you were last in, else the
  // nearest) and the boxes connected to it are shown; the rest and their connectors are hidden. Zoomed out, everything
  // is shown.
  function proximity(all) {
    var keep = null, ref = at || (box(last) ? last : nearestBox(all, me));
    if (zoomedIn() && ref) { keep = neighbors(ref); keep.push(ref); }
    walker.setAttribute('data-near', keep ? 'on' : 'off');
    Array.prototype.forEach.call(nodeLayer.querySelectorAll('.lgn'), function (o) {
      o.style.visibility = !keep || keep.indexOf(o.getAttribute('data-id')) >= 0 ? '' : 'hidden';
    });
    Array.prototype.forEach.call(edgeLayer.querySelectorAll('.lge'), function (e) {
      e.style.visibility = !keep || (keep.indexOf(e.getAttribute('data-a')) >= 0 && keep.indexOf(e.getAttribute('data-b')) >= 0) ? '' : 'hidden';
    });
  }

  // Presence (2026-11-14): the walker is in the presence server's map room (/here/ws?page=/map, the socket and message
  // shape of the dots on profile pages, here.js): every position update sends {x, y, instanceId}. The server's
  // snapshot then holds only the walkers in the same instance (spatial step 5); who that is right now is kept on
  // #lgm-walker[data-here] (handles, comma separated) and drawn as small round portraits around the walker (hereDraw).
  // x, y: where the walker is on the screen, as fractions of it (the map has no page coordinates of its own).
  var hws = null, hsent = '', spot = { x: 0, y: 0 };
  // The other droplets in your instance: one small circle each (#lgm-here .lgm-here-dot[data-u], lgmap.css), on a ring
  // around the walker, in handle order so they keep their places. Their portrait, else their first letter. Not
  // tappable. At most HERE_MAX are drawn. Gone when they leave the snapshot (another instance, left the map).
  var hereLayer = null, hereDots = {}, HERE_MAX = 12, HERE_R = 34;
  function hereDot(u) {
    var d = document.createElement('div'), img = document.createElement('img');
    d.className = 'lgm-here-dot';
    d.setAttribute('data-u', u);
    d.textContent = u.charAt(0).toUpperCase();
    img.alt = '';
    img.onerror = function () { if (img.parentNode) img.parentNode.removeChild(img); };
    img.src = '/portrait/' + encodeURIComponent(u);
    d.appendChild(img);
    hereLayer.appendChild(d);
    return d;
  }
  function herePlace() {
    var names = Object.keys(hereDots).sort(), n = Math.max(names.length, 6);
    names.forEach(function (u, i) {
      var a = -Math.PI / 2 + (i + 1) * 2 * Math.PI / (n + 1);
      hereDots[u].style.left = Math.round(spot.x + HERE_R * Math.cos(a)) + 'px';
      hereDots[u].style.top = Math.round(spot.y + HERE_R * Math.sin(a)) + 'px';
    });
    if (hereLayer) hereLayer.hidden = walker.hidden;
  }
  function hereDraw(list) {
    if (list.length) walker.setAttribute('data-here', list.join(','));
    else if (hws) walker.setAttribute('data-here', '');
    else walker.removeAttribute('data-here');
    if (!hereLayer) {
      if (!list.length) return;
      hereLayer = document.createElement('div');
      hereLayer.id = 'lgm-here';
      hereLayer.setAttribute('aria-hidden', 'true');
      document.body.appendChild(hereLayer);
    }
    var keep = {};
    list.slice().sort().slice(0, HERE_MAX).forEach(function (u) { keep[u] = true; if (!hereDots[u]) hereDots[u] = hereDot(u); });
    Object.keys(hereDots).forEach(function (u) { if (!keep[u]) { hereLayer.removeChild(hereDots[u]); delete hereDots[u]; } });
    herePlace();
  }
  function unit(v) { return Math.round(Math.max(0, Math.min(1, v)) * 1000) / 1000; }
  function presence() {
    if (!hws || hws.readyState !== 1 || !me) return;
    var msg = JSON.stringify({ x: unit(spot.x / (window.innerWidth || 1)), y: unit(spot.y / (window.innerHeight || 1)), instanceId: instanceId });
    if (msg === hsent) return;
    hsent = msg;
    hws.send(msg);
  }
  function presenceOn() {
    if (hws || !myName || !window.WebSocket) return;
    var s;
    try { s = new WebSocket((location.protocol === 'https:' ? 'wss:' : 'ws:') + '//' + location.host + '/here/ws?page=%2Fmap'); }
    catch (e) { return; }
    hws = s;
    hsent = '';
    s.onopen = presence;
    s.onmessage = function (ev) {
      if (s !== hws) return;
      try {
        var d = JSON.parse(ev.data);
        if (!d.people) return;
        hereDraw(d.people.map(function (q) { return String(q.u); }).filter(function (u) { return u && u !== myName; }));
      } catch (e) { /* a bad frame is skipped */ }
    };
    s.onclose = function () { if (s === hws) { hws = null; hereDraw([]); } };   // refused (chose No) or dropped: not retried on this page view
  }
  // Leaving the map (2026-11-15): a browser can keep the page you left in memory (its back/forward cache) with this
  // socket still open, and then the others in your instance kept seeing your drop after you had gone. So the socket is
  // closed on the way out, and opened again if that kept page is shown again (Back).
  window.addEventListener('pagehide', function () {
    if (!hws) return;
    var s = hws;
    hws = null;
    try { s.close(); } catch (e) { /* err-ok: already closing */ }
    hereDraw([]);
  });
  window.addEventListener('pageshow', function (e) { if (e.persisted && ready) presenceOn(); });

  // ---- the camera
  // The part of the map's frame you can see: on the screen, and above the pad (on a phone the frame runs under the
  // touchpad and past the bottom of the screen). In frame px.
  function seen() {
    var f = MAP.frame(), top = Math.max(f.top, 0), bottom = Math.min(f.bottom, window.innerHeight);
    var left = Math.max(f.left, 0), right = Math.min(f.right, window.innerWidth);
    var k = document.querySelector('#lg-here-pad .lg-here-keys'), kb = k ? k.getBoundingClientRect() : null;
    var padTop = kb && kb.height ? kb.top : Infinity;
    if (padTop - 8 > top + 80) bottom = Math.min(bottom, padTop - 8);
    if (bottom <= top || right <= left) return { x0: 0, y0: 0, x1: f.width, y1: f.height };
    return { x0: left - f.left, y0: top - f.top, x1: right - f.left, y1: bottom - f.top };
  }
  function centerView(k, glide) {
    var r = seen();
    MAP.setView(Math.round((r.x0 + r.x1) / 2 - me.x * k), Math.round((r.y0 + r.y1) / 2 - me.y * k), k, glide);
  }
  // the walking zoom, around your drop
  function zoomIn() {
    rezoom = 0;
    if (me && !userOut && !zoomedIn()) centerView(walkScale(), true);
  }
  // after a move: the drop back in the middle of the part of the map you can see (always, Brandon 2026-11-14)
  function follow() { centerView(MAP.view().scale, false); }
  // You zoomed (the wheel, a pinch): remembered for a moment, so a view that ends up zoomed out is known to be yours.
  svg.addEventListener('wheel', function () { userZoom = Date.now(); }, { passive: true });
  var touches = {};
  svg.addEventListener('pointerdown', function (e) { if (e.pointerType === 'touch') { touches[e.pointerId] = 1; if (Object.keys(touches).length > 1) userZoom = Date.now(); } }, true);
  svg.addEventListener('pointermove', function (e) { if (touches[e.pointerId] && Object.keys(touches).length > 1) userZoom = Date.now(); }, true);
  ['pointerup', 'pointercancel'].forEach(function (t) { svg.addEventListener(t, function (e) { delete touches[e.pointerId]; }, true); });
  // Zoomed out by you: left alone (the whole map shows). Zoomed out by the map itself (a box opened, Back, a resize:
  // lgmap.js fits the whole map again): back to the walking zoom shortly after.
  function watchZoom() {
    if (zoomedIn()) { userOut = false; return; }
    if (Date.now() - userZoom < 1500) userOut = true;
    if (!userOut && !rezoom) rezoom = setTimeout(zoomIn, 400);
  }

  // ---- walking
  // may the drop stand at p? Not inside a box it may not enter (unless it is already inside it: a box that moved onto
  // you never traps you), and not off the map (150 map units past its outermost boxes).
  function allowed(all, p) {
    var e = MAP.extent();
    if (e && (p.x < e.x0 - 150 || p.x > e.x1 + 150 || p.y < e.y0 - 150 || p.y > e.y1 + 150)) return false;
    var was = holding(all, me).map(function (b) { return b.id; }), ok = true;
    holding(all, p).forEach(function (b) {
      if (was.indexOf(b.id) < 0 && entry(b.id, true) === 'no') ok = false;
    });
    return ok;
  }
  function move(all, v) {
    var step = C.maxSpeed / MAP.view().scale;   // the same speed on the screen at any zoom
    var nx = me.x + v.x * step, ny = me.y + v.y * step;
    var tries = [{ x: nx, y: ny }, { x: nx, y: me.y }, { x: me.x, y: ny }];   // blocked: slide along the edge
    for (var i = 0; i < tries.length; i++) {
      if ((tries[i].x !== me.x || tries[i].y !== me.y) && allowed(all, tries[i])) {
        me = tries[i];
        if (i === 0) asked = '';   // walking freely again: the next time a request box turns you away, it is asked again
        return true;
      }
    }
    return false;
  }
  // the map moved the box you stand in (a box opened, the ring was laid out again): you go with it
  function carry() {
    var r = at ? region(at) : null;
    if (r && anchor && anchor.id === at && (anchor.cx !== r.cx || anchor.cy !== r.cy)) {
      me = { x: me.x + r.cx - anchor.cx, y: me.y + r.cy - anchor.cy };
    }
    anchor = r ? { id: at, cx: r.cx, cy: r.cy } : null;
  }
  function start() {
    var id = box(want) ? want : (box('you') ? 'you' : ''), r = id ? region(id) : null;
    if (!r) { var first = boxes()[0]; r = first ? first.r : null; }
    if (!r) return false;
    me = { x: r.cx, y: r.cy };
    return true;
  }
  function tick() {
    if (!me) {
      if (!ready || !start()) return;
      userOut = false;
      centerView(walkScale(), false);
    }
    carry();
    var all = boxes(), v = W.velocity(held, pull), moved = false;
    W.shape(dropPortrait, v.x, v.y);
    if (v.x || v.y) moved = move(all, v);
    var now = insideOf(all, me);
    if (now !== at) { at = now; anchor = null; carry(); }
    if (at) last = at;
    instanceId = at && at !== 'you' ? at : MAINSTREAM;
    Array.prototype.forEach.call(nodeLayer.querySelectorAll('.lgn.lgn-walk'), function (o) { if (o.getAttribute('data-id') !== at) o.classList.remove('lgn-walk'); });
    if (at && box(at)) box(at).classList.add('lgn-walk');
    scopeCss();
    if (moved) follow();
    watchZoom();
    proximity(all);
    var p = MAP.toScreen(me.x, me.y);
    spot = { x: Math.round(p.x), y: Math.round(p.y) };
    walker.hidden = false;
    walker.style.left = spot.x + 'px';
    walker.style.top = spot.y + 'px';
    walker.setAttribute('data-at', at);
    walker.setAttribute('data-instance', instanceId);
    walker.setAttribute('data-x', String(Math.round(me.x * 10) / 10));
    walker.setAttribute('data-y', String(Math.round(me.y * 10) / 10));
    herePlace();
    presence();
  }

  var walker = document.createElement('div');
  walker.id = 'lgm-walker';
  var dropPortrait = W.portrait(myName);
  walker.appendChild(dropPortrait);
  W.menu(dropPortrait);   // tap or click your drop: microphone, camera, drip speed
  // the drop takes taps now (its menu), so a wheel over it would scroll the page: it is handed to the map, which zooms
  dropPortrait.addEventListener('wheel', function (e) {
    e.preventDefault();
    svg.dispatchEvent(new WheelEvent('wheel', { deltaX: e.deltaX, deltaY: e.deltaY, deltaMode: e.deltaMode, clientX: e.clientX,
      clientY: e.clientY, ctrlKey: e.ctrlKey, metaKey: e.metaKey, bubbles: true, cancelable: true }));
  }, { passive: false });
  walker.hidden = true;
  walker.setAttribute('aria-hidden', 'true');
  walker.setAttribute('data-at', '');
  walker.setAttribute('data-instance', MAINSTREAM);
  document.body.appendChild(walker);
  root.classList.add('lgm-dropping');   // the boxes' depth (lgmap.css): they read as solid things you walk between

  // the box A acts on: the one you are in, else one whose edge is within CONTROLS.near px of your drop
  function target() {
    if (at && box(at)) return box(at);
    var best = null, bestD = C.near;
    boxes().forEach(function (b) {
      if (b.el.style.visibility === 'hidden') return;
      var r = b.el.getBoundingClientRect();
      var dx = Math.max(r.left - spot.x, 0, spot.x - r.right), dy = Math.max(r.top - spot.y, 0, spot.y - r.bottom);
      var d = Math.hypot(dx, dy);
      if (d <= bestD) { bestD = d; best = b.el; }
    });
    return best;
  }
  var want = '', ready = false;
  var arrival = W.takeHandoff();   // came through the portal: you start at that page's box
  W.mount({
    dir: function (dir, down) { held[dir] = down; },
    pull: function (x, y) { pull = { x: x, y: y }; },
    // A / Enter: the same as tapping the box you stand on (or right next to); nothing there: squish and bounce
    select: function () {
      var el = me ? target() : null;
      if (!el) { W.hop(dropPortrait); return true; }
      el.dispatchEvent(new MouseEvent('click', { bubbles: true, cancelable: true, view: window }));
      return true;
    },
    // B / Esc: close the open page, else one step back (LG_MAP.back, as the phone's back gesture)
    back: function () {
      var panel = document.getElementById('lgm-panel'), close = document.getElementById('lgm-close');
      if (panel && !panel.hidden && close) close.click();
      else if (MAP.back) MAP.back();
    },
  });
  (MAP.ready || Promise.resolve())
    .then(function () { return arrival && arrival.box ? MAP.goTo(arrival.box) : null; })
    .then(function () { if (arrival && arrival.box) want = arrival.box; },
          function () { /* that box is not on your map (privacy): start at you */ })
    .then(function () { ready = true; presenceOn(); });
  setInterval(tick, 40);
})();
