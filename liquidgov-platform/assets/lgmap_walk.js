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
  var POOLS = MAP.pools;   // big oval pools and wide channels (Brandon's drawing, 2026-11-15); null: the box map

  function box(id) { return id ? nodeLayer.querySelector('.lgn[data-id="' + (window.CSS && CSS.escape ? CSS.escape(id) : id) + '"]') : null; }
  function shown(el) { return el.style.display !== 'none'; }
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
  function isInside(p, r) {
    if (r.pool) { var ex = (p.x - r.cx) / r.hw, ey = (p.y - r.cy) / r.hh; return ex * ex + ey * ey <= 1; }   // an oval pool
    return Math.abs(p.x - r.cx) <= r.hw && Math.abs(p.y - r.cy) <= r.hh;
  }
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
  // Pools: the pool you are in (else the main one) fills the width of what you see, or its height on a wide screen,
  // so the channels leave it at the screen's edges, as in Brandon's drawing.
  function walkScale() {
    if (POOLS) {
      var r = region(at || last || 'you') || region('you'), s = seen();
      if (r) return Math.max(MAP.ZOOM.min, Math.min(MAP.ZOOM.max, (s.x1 - s.x0) / (2 * r.hw * 1.04), (s.y1 - s.y0) / (2 * r.hh * 1.12)));
    }
    return Math.min(MAP.ZOOM.max, MAP.fitScale() * C.mapZoom);
  }
  function zoomedIn() {
    var walk = walkScale(), fit = POOLS ? Math.min(MAP.fitScale(), walk * 0.6) : MAP.fitScale();
    return MAP.view().scale >= fit + (walk - fit) / 2 - 0.001;
  }
  // Layers (Brandon 2026-11-15: "3 or 4 layers of pools and streams"; rings outward from wherever you are): zoomed in,
  // the box you are at (the one you are in, else the one you were last in, else the nearest) and the boxes one channel
  // away are clear, two away faint, three away barely there, and anything further is mist (not shown); a channel shows
  // as faint as its fainter end. They re-form around you as you swim. Zoomed out, everything shows.
  var LAYER_ALPHA = [1, 1, 0.5, 0.22];
  function layersFrom(ref) {
    var adj = {}, dist = {};
    Array.prototype.forEach.call(edgeLayer.querySelectorAll('.lge'), function (e) {
      var a = e.getAttribute('data-a'), b = e.getAttribute('data-b');
      (adj[a] = adj[a] || []).push(b); (adj[b] = adj[b] || []).push(a);
    });
    dist[ref] = 0;
    var q = [ref];
    while (q.length) {
      var id = q.shift();
      if (dist[id] >= LAYER_ALPHA.length - 1) continue;
      (adj[id] || []).forEach(function (o) { if (!(o in dist)) { dist[o] = dist[id] + 1; q.push(o); } });
    }
    return dist;
  }
  function alphaOf(dist, id) { return id in dist ? LAYER_ALPHA[dist[id]] : 0; }
  function proximity(all) {
    var ref = at || (box(last) ? last : nearestBox(all, me)), dist = zoomedIn() && ref ? layersFrom(ref) : null;
    walker.setAttribute('data-near', dist ? 'on' : 'off');
    Array.prototype.forEach.call(nodeLayer.querySelectorAll('.lgn'), function (o) {
      var a = dist ? alphaOf(dist, o.getAttribute('data-id')) : 1;
      o.style.visibility = a ? '' : 'hidden';
      o.style.opacity = a && a < 1 ? String(a) : '';
      o.setAttribute('data-layer', dist && a ? String(dist[o.getAttribute('data-id')]) : '');
    });
    Array.prototype.forEach.call(edgeLayer.querySelectorAll('.lge'), function (e) {
      var a = dist ? Math.min(alphaOf(dist, e.getAttribute('data-a')), alphaOf(dist, e.getAttribute('data-b'))) : 1;
      e.style.visibility = a ? '' : 'hidden';
      e.style.opacity = a && a < 1 ? String(a) : '';
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
  // Pools and gates (Brandon 2026-11-15: boxes are pools, "you cant swim" across dry land, connections are channels
  // with floodgates that open when relevant): your drop is only ever in water, inside a pool (a box's region) or in the
  // channel along a connection that is showing (an edge whose two boxes are on the map and not hidden), CHANNEL map
  // units either side of the line between their middles. First version: a connection is relevant when it is yours, and
  // every connection on your map is, so every showing channel is open; a box you may not enter still stops you at its
  // edge (its valve). If a new layout ever leaves you on dry land you may still move (never trapped).
  var CHANNEL = POOLS ? 42 : 17;   // pools: a channel 84 map units wide (lgmap.css .lge-fill)
  function nearSeg(p, a, b, w) {
    var dx = b.cx - a.cx, dy = b.cy - a.cy, L = dx * dx + dy * dy, t = L ? ((p.x - a.cx) * dx + (p.y - a.cy) * dy) / L : 0;
    t = Math.max(0, Math.min(1, t));
    var qx = a.cx + t * dx - p.x, qy = a.cy + t * dy - p.y;
    return qx * qx + qy * qy <= w * w;
  }
  function inWater(all, p) {
    if (holding(all, p).length) return true;
    var ok = false;
    Array.prototype.forEach.call(edgeLayer.querySelectorAll('.lge'), function (e) {
      if (ok || !shown(e) || e.style.visibility === 'hidden') return;
      var a = region(e.getAttribute('data-a')), b = region(e.getAttribute('data-b'));
      if (a && b && nearSeg(p, a, b, CHANNEL)) ok = true;
    });
    return ok;
  }
  // may the drop be at p? In water (above), not inside a box it may not enter (unless it is already inside it: a box
  // that moved onto you never traps you), and not off the map (150 map units past its outermost boxes).
  function allowed(all, p) {
    var e = MAP.extent();
    if (e && (p.x < e.x0 - 150 || p.x > e.x1 + 150 || p.y < e.y0 - 150 || p.y > e.y1 + 150)) return false;
    if (!inWater(all, p) && inWater(all, me)) return false;   // the shore: dry land stops you
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
    // the banks guide you: in a channel, a push that is blocked slides you along the channel the way you pushed (keys only
    // go straight across or up and down; channels run any way)
    var best = null;
    Array.prototype.forEach.call(edgeLayer.querySelectorAll('.lge'), function (e) {
      if (e.style.visibility === 'hidden' || !shown(e)) return;
      var a = region(e.getAttribute('data-a')), b = region(e.getAttribute('data-b'));
      if (!a || !b || !nearSeg(me, a, b, CHANNEL + 4)) return;
      var dx = b.cx - a.cx, dy = b.cy - a.cy, L = Math.sqrt(dx * dx + dy * dy) || 1, ux = dx / L, uy = dy / L;
      var along = v.x * ux + v.y * uy;
      if (Math.abs(along) > 0.15 && (!best || Math.abs(along) > Math.abs(best.along))) best = { ux: ux, uy: uy, along: along };
    });
    if (best) {
      var sl = { x: me.x + best.ux * best.along * step, y: me.y + best.uy * best.along * step };
      if (allowed(all, sl)) { me = sl; return true; }
    }
    return false;
  }
  // the map moved the box you stand in (a box opened, the ring was laid out again): you go with it
  function carry() {
    var r = at ? region(at) : null;
    var moved = false;
    if (r && anchor && anchor.id === at && (anchor.cx !== r.cx || anchor.cy !== r.cy)) {
      me = { x: me.x + r.cx - anchor.cx, y: me.y + r.cy - anchor.cy };
      moved = true;
    }
    anchor = r ? { id: at, cx: r.cx, cy: r.cy } : null;
    return moved;
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
    var carried = carry();   // a pool that opened moves you with it: the screen follows, as it does when you swim
    var all = boxes(), v = W.velocity(held, pull), moved = false;
    W.shape(dropPortrait, v.x, v.y);
    if (v.x || v.y) moved = move(all, v);
    if (moved) lastMove = Date.now();
    var now = insideOf(all, me);
    if (now !== at) {
      at = now; anchor = null; carry();
      // pools: the pool you swam into fills the screen, as the main one does
      if (POOLS && at && !userOut) centerView(walkScale(), true);
    }
    if (at) last = at;
    instanceId = at && at !== 'you' ? at : MAINSTREAM;
    Array.prototype.forEach.call(nodeLayer.querySelectorAll('.lgn.lgn-walk'), function (o) { if (o.getAttribute('data-id') !== at) o.classList.remove('lgn-walk'); });
    if (at && box(at)) box(at).classList.add('lgn-walk');
    scopeCss();
    if (moved || carried) follow();
    watchZoom();
    proximity(all);
    if (POOLS) water();   // the water follows the fading at once
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
    peekTick();
    poolTick();
  }

  var water = function () {};   // the channels' water, mouths and signs (set up below)
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
  root.classList.add('lgm-dropping');   // water (lgmap.css): the boxes are pools, the connections streams
  // the water's colours (lgmap.css fills pools with #lgm-water): deeper in the middle,
  // lighter at the shore; and a slow shimmer along each stream (.lge-flow), drawn over it, that never takes a tap
  (function () {
    var NS = 'http://www.w3.org/2000/svg', defs = document.createElementNS(NS, 'defs');
    defs.innerHTML = '<radialGradient id="lgm-water" cx="50%" cy="45%" r="65%"><stop offset="0" stop-color="#1b5d80"/>' +
      '<stop offset=".7" stop-color="#2a7ea6"/><stop offset="1" stop-color="#5fb4d6"/></radialGradient>';
    svg.insertBefore(defs, svg.firstChild);
    // Pools: each channel is its shore (the .lge line, drawn wide) under its water (.lge-fill), so where channels cross
    // the water joins; the pools sit over the channels, and at each mouth a patch of water (.lge-mouth) opens the pool's
    // shore so the channel runs into it. The name of where a channel goes is written inside the pool at its mouth, along
    // the shore (.lge-sign). All copy the channel's own visibility and fading (proximity), every tick.
    var fillLayer = null, flowLayer = edgeLayer;
    if (POOLS) {
      fillLayer = document.createElementNS(NS, 'g'); fillLayer.id = 'lgm-fills';
      flowLayer = document.createElementNS(NS, 'g'); flowLayer.id = 'lgm-flows';
      edgeLayer.parentNode.insertBefore(fillLayer, edgeLayer.nextSibling);
      edgeLayer.parentNode.insertBefore(flowLayer, fillLayer.nextSibling);
      defs.innerHTML += '<radialGradient id="lgm-pool" cx="50%" cy="45%" r="60%"><stop offset="0" stop-color="#1b5d80"/>' +
        '<stop offset="1" stop-color="#2a7ea6"/></radialGradient>';
    }
    function same(x, e) {
      if (x.style.visibility !== e.style.visibility) x.style.visibility = e.style.visibility;
      if (x.style.opacity !== e.style.opacity) x.style.opacity = e.style.opacity;
    }
    function mirror(layer, cls, e, key) {
      var f = layer.querySelector('.' + cls + '[data-for="' + key + '"]');
      if (!f) { f = document.createElementNS(NS, 'path'); f.setAttribute('class', cls); f.setAttribute('data-for', key); layer.appendChild(f); }
      if (f.getAttribute('d') !== e.getAttribute('d')) f.setAttribute('d', e.getAttribute('d'));
      same(f, e);
    }
    function nameOf(id) {
      if (id === 'you') return POOLS.label({ kind: 'you' });
      var b = box(id), t = b && b.querySelector('title');
      return t ? t.textContent : '';
    }
    // one end of a channel: where it meets pool `from`'s shore (heading to `to`), its patch and its sign
    function mouth(key, from, to, e) {
      var a = region(from), b = region(to);
      if (!a || !b) return;
      var dx = b.cx - a.cx, dy = b.cy - a.cy, L = Math.sqrt(dx * dx + dy * dy) || 1, ux = dx / L, uy = dy / L;
      var t = 1 / Math.sqrt(ux * ux / (a.hw * a.hw) + uy * uy / (a.hh * a.hh)), px = a.cx + ux * t, py = a.cy + uy * t;
      // drawn inside the pool's own drawing (its <g>, in its own units), so a pool laid over another covers its mouths
      var g = box(from), sh = g && g.querySelector('.lgs');
      if (!sh) return;
      var ox = a.cx - +sh.getAttribute('cx'), oy = a.cy - +sh.getAttribute('cy');
      px -= ox; py -= oy;
      var mk = key + '>' + from;
      var m = g.querySelector('.lge-mouth[data-for="' + mk + '"]');
      if (!m) { m = document.createElementNS(NS, 'path'); m.setAttribute('class', 'lge-mouth'); m.setAttribute('data-for', mk); sh.parentNode.insertBefore(m, sh.nextSibling); }
      var d = 'M' + (px - ux * 26).toFixed(1) + ' ' + (py - uy * 26).toFixed(1) + ' L' + (px + ux * 22).toFixed(1) + ' ' + (py + uy * 22).toFixed(1);
      if (m.getAttribute('d') !== d) m.setAttribute('d', d);
      var sg = g.querySelector('.lge-sign[data-for="' + mk + '"]');
      if (!sg) { sg = document.createElementNS(NS, 'text'); sg.setAttribute('class', 'lge-sign'); sg.setAttribute('data-for', mk); g.appendChild(sg); }
      var fs = Math.round(Math.min(a.hw * 0.11, 22)), inset = fs * 1.4, sx = px - ux * inset, sy = py - uy * inset;
      var deg = Math.atan2(uy, ux) * 180 / Math.PI + 90;
      if (deg > 90) deg -= 180; else if (deg <= -90) deg += 180;   // never upside down
      var tr = 'rotate(' + deg.toFixed(1) + ' ' + sx.toFixed(1) + ' ' + sy.toFixed(1) + ')', name = nameOf(to);
      var most = Math.max(4, Math.floor(Math.min(a.hw * 0.85, 150) / (fs * 0.6)));   // a sign stays inside its pool and its mouth's stretch of shore
      if (name.length > most) name = name.slice(0, most - 1).replace(/\s+$/, '') + '\u2026';
      if (sg.getAttribute('transform') !== tr) { sg.setAttribute('x', sx.toFixed(1)); sg.setAttribute('y', sy.toFixed(1)); sg.setAttribute('transform', tr); sg.style.fontSize = fs + 'px'; }
      if (sg.textContent !== name) sg.textContent = name;
      same(m, e); same(sg, e);
      m.setAttribute('data-keep', '1'); sg.setAttribute('data-keep', '1');
    }
    water = function () {
      var keep = {};
      Array.prototype.forEach.call(edgeLayer.querySelectorAll('.lge'), function (e) {
        var a = e.getAttribute('data-a'), b = e.getAttribute('data-b'), key = a + '|' + b;
        keep[key] = 1;
        mirror(flowLayer, 'lge-flow', e, key);
        if (!POOLS) return;
        mirror(fillLayer, 'lge-fill', e, key);
        mouth(key, a, b, e); mouth(key, b, a, e);
      });
      [flowLayer, fillLayer].forEach(function (layer) {
        if (layer) Array.prototype.slice.call(layer.querySelectorAll('.lge-flow,.lge-fill')).forEach(function (f) { if (!keep[f.getAttribute('data-for')]) f.remove(); });
      });
      if (POOLS) Array.prototype.slice.call(nodeLayer.querySelectorAll('.lge-mouth,.lge-sign')).forEach(function (x) {
        if (x.getAttribute('data-keep')) x.removeAttribute('data-keep'); else x.remove();
      });
    };
    setInterval(water, 400);
  })();

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
  // ---- the portal (Brandon 2026-11-14: "walk up to the portal of whatever page youre curious about. maybe youd see a
  // small semi-translucent window blow up from the center of the portal when you get within entry range. if you click A
  // within that range, youd see that window expand to fill the screen while simultaneously becoming opaque until it
  // effectively becomes the page youre occupying"; the side panel is abandoned). Standing in or next to a box with a
  // page for dwellMs, its page (live, only looked at: lg_peek=1, walker.js) grows out of the box's middle in a
  // see-through window; walking away shrinks it back. A there, or opening the box (a double tap, a hold, Enter), grows
  // it over the whole screen as it turns solid, and then you are on that page.
  // Swimming into a pool opens it (Brandon 2026-11-15, pools and gates): stay in a box that is not the middle of the map
  // for openMs and its connections spread around it, as a tap on it does (this replaces A making a box the center; A is
  // now the portal). B / Esc, or the trail, goes back.
  var OPEN_MS = 700, inSince = 0, inFor = '', poolOpened = '';
  function poolTick() {
    if (entering || !at || !MAP.center || at === MAP.center()) { inFor = ''; return; }
    if (inFor !== at) { inFor = at; inSince = Date.now(); return; }
    if (Date.now() - inSince < OPEN_MS) return;
    inFor = '';
    // opened without its page (a click on a desktop goes into the page: swimming in must not leave the map)
    if (box(at) && MAP.goTo) { poolOpened = at; MAP.goTo(at); }
  }
  // Pools (Brandon 2026-11-15, "you pick something"): the pool you are in fills the screen, so the peek waits until your
  // drop has been still for stillMs, is smaller, and sits at the top of the map on the side away from your drop; moving
  // hides it at once. The box map keeps its own place for it (over or under the box).
  var PEEK = { dwellMs: 250, stillMs: 550, w: 520, h: 380, poolW: 0.46, poolH: 0.24, alpha: 0.6, ms: 320, growMs: 420, cls: 'lgm-peek' };
  var lastMove = 0;
  var peekEl = null, peekFrame = null, peekFor = '', peekPage = '', peekSince = 0, peekOpen = false, entering = false;
  function peekUrl(page) { return page + (page.indexOf('?') < 0 ? '?' : '&') + 'lg_peek=1'; }
  function peekBuild() {
    peekEl = document.createElement('div');
    peekEl.className = PEEK.cls;
    peekEl.setAttribute('aria-hidden', 'true');
    peekFrame = document.createElement('iframe');
    peekFrame.setAttribute('tabindex', '-1');
    peekFrame.title = 'Peek';
    peekEl.appendChild(peekFrame);
    peekEl.hidden = true;
    document.body.appendChild(peekEl);
  }
  // the window's place: above the box when there is room, else below, inside the screen; it grows from the box's middle
  function peekPlace(el, page) {
    var b = el.getBoundingClientRect(), vw = window.innerWidth, vh = window.innerHeight;
    var w = Math.min(PEEK.w, Math.round(vw * 0.62)), h = Math.min(PEEK.h, Math.round(vh * 0.3));   // a glimpse, not a cover
    var cx = (b.left + b.right) / 2, top = b.top - h - 12 >= 8 ? b.top - h - 12 : Math.min(vh - h - 8, b.bottom + 12);
    var left = Math.max(12, Math.min(vw - w - 12, cx - w / 2));
    top = Math.max(8, top);
    if (POOLS) {
      var s = seen(), f = MAP.frame();
      w = Math.min(PEEK.w, Math.round(vw * PEEK.poolW)); h = Math.min(PEEK.h, Math.round(vh * PEEK.poolH));
      top = Math.max(8, Math.round(f.top + s.y0 + 10));
      left = spot.x < vw / 2 ? vw - w - 12 : 12;   // the side away from your drop
    }
    peekEl.style.left = left + 'px'; peekEl.style.top = top + 'px';
    peekEl.style.width = w + 'px'; peekEl.style.height = h + 'px';
    peekEl.style.transformOrigin = (cx - left) + 'px ' + ((b.top + b.bottom) / 2 - top) + 'px';
    // the page drawn at the screen's size, shrunk into the window, so the peek looks like the page itself
    var k = w / vw;
    peekFrame.style.width = vw + 'px'; peekFrame.style.height = Math.round(h / k) + 'px';
    peekFrame.style.transform = 'scale(' + k + ')';
    if (peekPage !== page) {
      // a frame that already shows a page would add a step to the browser's history if given a new address (then the
      // phone's back gesture went back inside the peek, not on the map): its own location is replaced instead
      var had = !!peekPage;
      peekPage = page;
      try { if (had && peekFrame.contentWindow) { peekFrame.contentWindow.location.replace(peekUrl(page)); return; } }
      catch (e) { /* err-ok: not reachable: set the address below */ }
      peekFrame.src = peekUrl(page);
    }
  }
  function peekShow(id, el, page) {
    if (!peekEl) peekBuild();
    peekFor = id;
    peekPlace(el, page);
    peekEl.hidden = false;
    peekEl.classList.remove('lgm-peek-full');
    void peekEl.offsetWidth;   // start the grow from the small state
    peekEl.classList.add('lgm-peek-on');
    peekOpen = true;
    walker.setAttribute('data-peek', id);
  }
  function peekHide() {
    if (!peekEl || !peekOpen || entering) return;
    peekOpen = false;
    peekFor = '';
    walker.removeAttribute('data-peek');
    peekEl.classList.remove('lgm-peek-on');
    setTimeout(function () { if (!peekOpen && !entering) peekEl.hidden = true; }, PEEK.ms);
  }
  // A: the window fills the screen and turns solid, then you are on the page
  function enter(id, page) {
    if (entering || !page) return false;
    var el = box(id);
    if (!peekOpen || peekFor !== id) { if (!el) return false; peekShow(id, el, page); }
    entering = true;
    walker.setAttribute('data-entering', id);
    var go = function () { location.assign(page); };
    if (W.reducedMotion()) { go(); return true; }
    requestAnimationFrame(function () { peekEl.classList.add('lgm-peek-full'); });
    setTimeout(go, PEEK.growMs);
    return true;
  }
  MAP.onOpen = function (id, page) { return enter(id, page); };
  // a box tapped (or reached by the trail or Back) somewhere else on the map: your drop goes to it, the screen follows
  MAP.onFocus = function (id) {
    var r = region(id);
    if (!r || !me) return false;
    // a pool that opened because you swam into it: you may have swum on by now, and must not be pulled back to it
    if (poolOpened === id) { poolOpened = ''; return true; }
    if (at !== id) { me = { x: r.cx, y: r.cy }; anchor = null; }
    centerView(MAP.view().scale, true);
    return true;
  };
  // each tick: the box you are at (in it, or right next to it) and its page
  function peekTick() {
    if (entering) return;
    if (POOLS && Date.now() - lastMove < PEEK.stillMs) { peekSince = 0; peekHide(); return; }   // swimming: no peek
    var t = me ? target() : null, id = t ? t.getAttribute('data-id') : '', rec = id ? MAP.instance(id) : null;
    var page = rec && rec.page ? rec.page : '';
    if (!page) { peekSince = 0; peekHide(); return; }
    if (peekOpen && peekFor === id) { peekPlace(t, page); return; }
    if (!peekSince || walker.getAttribute('data-peek-wait') !== id) { peekSince = Date.now(); walker.setAttribute('data-peek-wait', id); return; }
    if (Date.now() - peekSince >= PEEK.dwellMs) peekShow(id, t, page);
  }
  window.addEventListener('pageshow', function (e) { if (e.persisted && entering) { entering = false; peekHide(); } });   // Back to the map

  var want = '', ready = false;
  var arrival = W.takeHandoff();   // came through the portal: you start at that page's box
  W.mount({
    dir: function (dir, down) { held[dir] = down; },
    pull: function (x, y) { pull = { x: x, y: y }; },
    // A / Enter: the same as tapping the box you stand on (or right next to); nothing there: squish and bounce
    select: function () {
      var el = me ? target() : null;
      if (!el) { W.hop(dropPortrait); return true; }
      // the box you are at has a page: go in through its portal
      var rec = MAP.instance(el.getAttribute('data-id'));
      if (rec && rec.page && enter(rec.id, rec.page)) return true;
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
