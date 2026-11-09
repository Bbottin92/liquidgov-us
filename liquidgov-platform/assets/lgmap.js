// LiquidGov interactive map (/map). Brandon 2026-11-06: click a box and its
// connections expand out of it; if the box is something people engage with,
// its page shows in the field on the right; the field can fill the window, and
// the mAInstream button at the top shrinks it back.
// Data: GET /map/node?n=<id> -> {node:{id,label,kind,page}, links:[{node, edge}]}.
// The server decides what this viewer may see (privacy settings, blocks); this
// file only draws it.
(function () {
  'use strict';
  var root = document.getElementById('lgmap');
  if (!root) return;
  var SVGNS = 'http://www.w3.org/2000/svg';
  var svg = document.getElementById('lgm-svg');
  var view = document.getElementById('lgm-view');
  var edgeLayer = document.getElementById('lgm-edges');
  var nodeLayer = document.getElementById('lgm-nodes');
  var panel = document.getElementById('lgm-panel');
  var frame = document.getElementById('lgm-frame');
  var ptitle = document.getElementById('lgm-ptitle');
  var openLink = document.getElementById('lgm-open');
  var statusEl = document.getElementById('lgm-status');
  var closeBtn = document.getElementById('lgm-close');
  function narrow() { return window.matchMedia ? matchMedia('(max-width: 760px)').matches : innerWidth <= 760; }
  var W = 200, H = 36, COLW = 270, ROWH = 46;
  var nodes = {};   // id -> {id,label,kind,page,rel,x,y,el,expanded}
  var edges = {};   // "a|b" -> {a,b,kind,el}
  var selected = null;
  var tx = 40, ty = 0, scale = 1;

  // Visits (Brandon 2026-11-06: "connections that you visit frequently become
  // thicker"). Counted per box in this browser's localStorage only; never sent
  // to the server. "Forget my visits" in the bar clears them.
  var VKEY = 'lgmap.visits.' + (root.getAttribute('data-me') || '');
  function edgeKey(a, b) { return a < b ? a + '|' + b : b + '|' + a; }
  var visits = {};
  try { visits = JSON.parse(localStorage.getItem(VKEY) || '{}') || {}; } catch (e) { visits = {}; }
  function saveVisits() {
    var ids = Object.keys(visits);
    if (ids.length > 2000) { // keep the most visited
      ids.sort(function (a, b) { return visits[b] - visits[a]; });
      var keep = {}; ids.slice(0, 1500).forEach(function (k) { keep[k] = visits[k]; }); visits = keep;
    }
    try { localStorage.setItem(VKEY, JSON.stringify(visits)); } catch (e) { /* private mode: thickness just won't persist */ }
  }
  // A line's use count ("e:<a>|<b>") goes up each time you go along it: you
  // click a box that is connected to the box you were on, or to the box it
  // grew from.
  function edgeWidth(e) {
    var v = visits['e:' + edgeKey(e.a, e.b)] || 0;
    return 1.6 + Math.min(4.5, 1.5 * Math.log2(1 + v));
  }
  // Brandon 2026-11-06: "if you've never actually connected with a box, then
  // maybe there's no distinct line, only a translucent one". A box counts as
  // touched once you've visited it here, or when it's yours (relevance 1:
  // your city, groups, proposals...). The mAInstream always counts.
  function touched(id) {
    var n = nodes[id];
    return (visits[id] || 0) > 0 || (n && (n.kind === 'root' || (typeof n.rel === 'number' && n.rel >= 0.95)));
  }
  function sizeEdge(e) {
    e.el.style.setProperty('--w', edgeWidth(e).toFixed(2) + 'px');
    e.el.classList.toggle('lge-faint', !(touched(e.a) && touched(e.b)));
  }
  // Relevance shading: red (far from you) -> yellow -> green (yours). The server
  // scores it from your own involvement; frequent visits nudge it a little.
  function shade(n) {
    if (typeof n.rel !== 'number' || n.rel < 0) return;
    var r = Math.min(1, n.rel + 0.05 * Math.log2(1 + (visits[n.id] || 0)));
    var hue = Math.round(r * 120);
    n.el.querySelector('.lgs').style.fill = 'hsla(' + hue + ',72%,48%,.24)';
    n.el.setAttribute('data-rel', r.toFixed(2));
  }

  // Shapes by kind (Brandon 2026-11-07: "color and shape coding"): people round, groups square, cities oval,
  // proposals hexagon, offices octagon, problems a tag, posts a note. Colour stays relevance (shade).
  function shapeEl(kind, w, h) {
    var c = 10, poly = function (pts) { return el('polygon', { points: pts.map(function (p) { return p.join(','); }).join(' '), 'class': 'lgs' }); };
    switch (kind) {
      case 'person': return el('rect', { width: w, height: h, rx: h / 2, 'class': 'lgs' });
      case 'group': return el('rect', { width: w, height: h, rx: 3, 'class': 'lgs' });
      case 'city': return el('ellipse', { cx: w / 2, cy: h / 2, rx: w / 2, ry: h / 2, 'class': 'lgs' });
      case 'prop': case 'sitewide': return poly([[c, 0], [w - c, 0], [w, h / 2], [w - c, h], [c, h], [0, h / 2]]);
      case 'dept': return poly([[7, 0], [w - 7, 0], [w, 7], [w, h - 7], [w - 7, h], [7, h], [0, h - 7], [0, 7]]);
      case 'pain': return poly([[0, 0], [w - 14, 0], [w, h / 2], [w - 14, h], [0, h]]);
      default: return el('rect', { width: w, height: h, rx: 9, 'class': 'lgs' });
    }
  }
  // Size from relevance: your own things are bigger (0.85x far .. 1.25x yours); the mAInstream stays 1x.
  function sizeOf(n) {
    return (typeof n.rel === 'number' && n.rel >= 0) ? 0.85 + 0.4 * Math.min(1, n.rel) : 1;
  }
  function leftX(n) { return n.x + W / 2 * (1 - sizeOf(n)); }
  function rightX(n) { return n.x + W / 2 * (1 + sizeOf(n)); }

  function status(msg) { statusEl.textContent = msg || ''; }
  function el(tag, attrs) {
    var e = document.createElementNS(SVGNS, tag);
    for (var k in attrs) e.setAttribute(k, attrs[k]);
    return e;
  }
  // CSS transform (not the SVG attribute) so moves can glide; dragging turns the glide off.
  function apply(glide) {
    view.classList.toggle('lgm-glide', !!glide);
    view.style.transform = 'translate(' + tx + 'px,' + ty + 'px) scale(' + scale + ')';
  }
  // Bring a box and the column of connections to its right into view.
  function focusOn(n) {
    var box = svg.getBoundingClientRect();
    if (!box.width) return;
    if (narrow()) {
      // Phone: the box at the left edge and its column of connections beside it, scaled to fit the width,
      // never under 0.75 (13 px labels stay readable; advisor 2026-11-07).
      scale = Math.max(0.75, Math.min(1, (box.width - 16) / (COLW + W * 1.25)));
      // Sweep 2026-11-09: at the 0.75 floor the column of connections ran off the right edge ("Tower District
      // Garde..."), and centring on the box left a screenful of empty map above it. So: the connections (the boxes
      // you tap next) stay fully in view - right-aligned when box + column are wider than the phone - and the top
      // of that column starts near the top of the frame instead of being centred in it.
      var right = leftX(n) + COLW + W * 1.25, top = n.y;
      for (var k in edges) {
        var o = edges[k].a === n.id ? nodes[edges[k].b] : (edges[k].b === n.id ? nodes[edges[k].a] : null);
        if (o && o.x > n.x) top = Math.min(top, o.y);
      }
      tx = Math.round(Math.min(8 - leftX(n) * scale, box.width - 8 - right * scale));
      ty = Math.round(Math.min(box.height / 2 - (n.y + H / 2) * scale, 12 - top * scale));
      apply(true);
      return;
    }
    tx = Math.round(Math.min(box.width * 0.3, box.width - (COLW + W + 30) * scale) - n.x * scale);
    ty = Math.round(box.height / 2 - (n.y + H / 2) * scale);
    apply(true);
  }
  function short(s, n) { s = String(s); return s.length > n ? s.slice(0, n - 1) + '…' : s; }

  function free(x, y) {
    for (var id in nodes) {
      var n = nodes[id];
      if (Math.abs(n.x - x) < COLW / 2 && Math.abs(n.y - y) < ROWH - 2) return false;
    }
    return true;
  }
  function slot(x, y) {
    for (var k = 0; k < 600; k++) {
      var d = (k % 2 ? 1 : -1) * Math.ceil(k / 2) * ROWH;
      if (free(x, y + d)) return y + d;
    }
    return y;
  }

  function edgePath(e) {
    var A = nodes[e.a], B = nodes[e.b];
    if (A.x > B.x) { var t = A; A = B; B = t; }
    var x1, y1 = A.y + H / 2, x2, y2 = B.y + H / 2;
    if (Math.abs(A.x - B.x) < 1) { // same column: bow out to the right
      x1 = rightX(A); x2 = rightX(B);
      return 'M' + x1 + ' ' + y1 + ' C' + (x1 + 60) + ' ' + y1 + ' ' + (x2 + 60) + ' ' + y2 + ' ' + x2 + ' ' + y2;
    }
    x1 = rightX(A); x2 = leftX(B);   // lines meet each box's real edge, whatever its size
    var dx = Math.max(30, (x2 - x1) / 2);
    return 'M' + x1 + ' ' + y1 + ' C' + (x1 + dx) + ' ' + y1 + ' ' + (x2 - dx) + ' ' + y2 + ' ' + x2 + ' ' + y2;
  }
  function addEdge(a, b, kind) {
    var key = edgeKey(a, b);
    if (edges[key] || !nodes[a] || !nodes[b]) return;
    var e = { a: a, b: b, kind: kind };
    e.el = el('path', { 'class': 'lge lge-' + kind, d: edgePath(e), 'data-a': a, 'data-b': b });
    var t = el('title', {}); t.textContent = kind; e.el.appendChild(t);
    edgeLayer.appendChild(e.el);
    edges[key] = e;
    sizeEdge(e);
  }

  function addNode(d, x, y) {
    if (nodes[d.id]) return nodes[d.id];
    var n = { id: d.id, label: d.label, kind: d.kind, page: d.page, rel: d.rel, x: x, y: slot(x, y), expanded: false };
    var s = sizeOf(n);
    var g = el('g', { 'class': 'lgn lgn-' + n.kind + ' lgn-new', tabindex: '0', role: 'button',
      transform: 'translate(' + (n.x + W / 2 * (1 - s)) + ',' + (n.y + H / 2 * (1 - s)) + ') scale(' + s + ')' });
    g.appendChild(shapeEl(n.kind, W, H));
    var text = el('text', { x: W / 2, y: H / 2 + 5 });
    text.textContent = short(n.label, 26);
    g.appendChild(text);
    var title = el('title', {}); title.textContent = n.label; g.appendChild(title);
    g.addEventListener('click', function (ev) {
      ev.stopPropagation();
      if (Date.now() < suppressClickUntil) return; // the press below already handled this touch
      select(n, !narrow());
    });
    // Enter opens the page (keyboard and screen readers, which cannot long-press); Space only grows connections.
    g.addEventListener('keydown', function (ev) {
      if (ev.key === 'Enter' || ev.key === ' ') { ev.preventDefault(); select(n, ev.key === 'Enter'); }
    });
    // Phones (Brandon 2026-11-09): a quick tap only grows the box's connections; holding it makes the box swell
    // the longer you hold (like Messenger's thumbs-up) and letting go after HOLD_MS opens its page. Moving the
    // finger cancels, so dragging the map still works.
    g.addEventListener('pointerdown', function (ev) {
      if (ev.pointerType === 'mouse' || !narrow()) return;
      startHold(n, ev);
    });
    g.addEventListener('contextmenu', function (ev) { if (narrow()) ev.preventDefault(); });
    g.addEventListener('mouseenter', function () { light(n.id); });
    g.addEventListener('mouseleave', function () { light(selected ? selected.id : null); });
    nodeLayer.appendChild(g);
    setTimeout(function () { g.classList.remove('lgn-new'); }, 20);
    n.el = g;
    nodes[n.id] = n;
    shade(n);
    return n;
  }

  function light(id) {
    svg.classList.toggle('lgm-focus', !!id);
    for (var k in edges) {
      var e = edges[k];
      e.el.classList.toggle('hi', !!id && (e.a === id || e.b === id));
    }
  }

  function load(id) {
    return fetch('/map/node?n=' + encodeURIComponent(id), { credentials: 'same-origin', headers: { Accept: 'application/json' } }) // err-ok: every caller catches this promise and shows the message in the map's status line
      .then(function (r) {
        if (r.status === 401) { location.href = '/login'; throw new Error('signed out'); }
        if (r.status === 404) throw new Error('That is no longer on the map.');
        if (!r.ok) throw new Error('The map could not load (' + r.status + ').');
        return r.json();
      });
  }

  function expand(n) {
    if (n.expanded) return Promise.resolve(n);
    status('Loading…');
    return load(n.id).then(function (data) {
      var fresh = data.links.filter(function (l) { return !nodes[l.node.id]; });
      var x = n.x + COLW;
      var start = n.y - (fresh.length - 1) * ROWH / 2;
      fresh.forEach(function (l, i) { addNode(l.node, x, start + i * ROWH).from = n.id; });
      data.links.forEach(function (l) { addEdge(n.id, l.node.id, l.edge); });
      n.expanded = true;
      n.el.classList.add('lgn-open');
      status(data.links.length ? '' : 'Nothing else connects here yet.');
      return n;
    }).catch(function (err) { status(err.message); return n; });
  }

  // ---- press and hold (phones) ----
  var HOLD_MS = 450, GROW_MAX = 1.6, GROW_MS = 900, suppressClickUntil = 0, hold = null;
  var DOUBLE_MS = 350, GLIDE_WAIT_MS = 320, lastTap = { id: null, t: 0 };
  function boxTransform(n, k) {
    var s = sizeOf(n) * k;
    return 'translate(' + (n.x + W / 2 * (1 - s)) + ',' + (n.y + H / 2 * (1 - s)) + ') scale(' + s + ')';
  }
  function endHold(open) {
    if (!hold) return;
    var h = hold;
    hold = null;
    cancelAnimationFrame(h.raf);
    h.n.el.classList.remove('lgn-hold', 'lgn-ready');
    h.n.el.setAttribute('transform', boxTransform(h.n, 1));
    document.removeEventListener('pointermove', h.move, true);
    document.removeEventListener('pointerup', h.up, true);
    document.removeEventListener('pointercancel', h.cancel, true);
    if (open === null) return;              // cancelled (the finger moved: a drag, not a tap)
    suppressClickUntil = Date.now() + 600;  // the browser's click for this touch must not run select() again
    select(h.n, open);
  }
  function startHold(n, ev) {
    endHold(null);
    var t0 = performance.now(), x0 = ev.clientX, y0 = ev.clientY;
    hold = { n: n, raf: 0, ready: false };
    n.el.classList.add('lgn-hold');
    // grow on top of its neighbours, not behind the next box down (seen in the 2026-11-09 screenshot); the release
    // is caught on the document, so moving the element does not lose the touch
    nodeLayer.appendChild(n.el);
    function grow(now) {
      if (!hold) return;
      var t = Math.min(1, (now - t0) / GROW_MS);
      n.el.setAttribute('transform', boxTransform(n, 1 + (GROW_MAX - 1) * t));
      if (!hold.ready && now - t0 >= HOLD_MS) {
        hold.ready = true;
        n.el.classList.add('lgn-ready');
        try { if (navigator.vibrate) navigator.vibrate(12); } catch (e) {} // err-ok: haptics are a nicety; iOS has none
      }
      hold.raf = requestAnimationFrame(grow);
    }
    hold.move = function (e) { if (Math.abs(e.clientX - x0) > 10 || Math.abs(e.clientY - y0) > 10) endHold(null); };
    // Tap or hold is decided from the input events' own timestamps (when the finger went down and up), not from
    // when this page got round to handling them: a busy page delays handlers by hundreds of ms (measured in
    // headless Edge: a 60 ms tap handled 453 ms apart), which would turn a tap into a hold.
    var down = ev.timeStamp;
    // a hold opens; so does a second quick tap on the same box within DOUBLE_MS (Brandon 2026-11-09: "maybe a
    // double tap can open a box?"), also by event timestamps
    hold.up = function (e) {
      if (e.timeStamp - down >= HOLD_MS) { lastTap = { id: null, t: 0 }; endHold(true); return; }
      var dbl = lastTap.id === n.id && e.timeStamp - lastTap.t < DOUBLE_MS;
      lastTap = dbl ? { id: null, t: 0 } : { id: n.id, t: e.timeStamp };
      endHold(dbl);
    };
    hold.cancel = function () { endHold(null); };
    document.addEventListener('pointermove', hold.move, true);
    document.addEventListener('pointerup', hold.up, true);
    document.addEventListener('pointercancel', hold.cancel, true);
    hold.raf = requestAnimationFrame(grow);
  }

  // open: show the box's page (desktop click, Enter, a phone long-press). Without it - a quick tap on a phone -
  // only the connections grow.
  function select(n, open) {
    var prev = selected;
    if (selected) selected.el.classList.remove('lgn-sel');
    selected = n;
    n.el.classList.add('lgn-sel');
    visits[n.id] = (visits[n.id] || 0) + 1;
    var from = prev && prev !== n && edges[edgeKey(prev.id, n.id)] ? prev.id
             : (n.from && edges[edgeKey(n.from, n.id)] ? n.from : null);
    if (from) { var ek = 'e:' + edgeKey(from, n.id); visits[ek] = (visits[ek] || 0) + 1; }
    saveVisits();
    shade(n);
    for (var k in edges) if (edges[k].a === n.id || edges[k].b === n.id) sizeEdge(edges[k]);
    light(n.id);
    if (n.page && open) openPanel(n);
    else if (!n.page || !narrow()) closePanel();
    // After the field opens (the map narrows), glide to the box and what grew out of it.
    // Phone, a tap that did not open: wait before gliding, so a second tap (double-tap to open) lands on the same box
    // instead of wherever the glide moved it.
    var wait = narrow() && !open ? GLIDE_WAIT_MS : 0;
    expand(n).then(function () {
      light(n.id);
      setTimeout(function () { if (selected === n) focusOn(n); }, wait);
    });
  }

  function openPanel(n) {
    panel.hidden = false;
    root.classList.add('lgm-has-panel');
    ptitle.textContent = n.label;
    openLink.href = n.page;
    // Same page, framed: the server drops its header for framed loads.
    if (frame.getAttribute('src') !== n.page) frame.setAttribute('src', n.page);
    // Phone: the page is a full-screen sheet over the map (CSS); the page behind must not scroll, and the close
    // button says where it goes back to (Brandon 2026-11-07: "full-screen sheet over the map").
    if (narrow()) {
      document.documentElement.classList.add('lgm-sheet-open');
      closeBtn.textContent = '‹ mAInstream';
    } else {
      closeBtn.innerHTML = '&times;';
    }
  }
  function closePanel() {
    panel.hidden = true;
    panel.classList.remove('full');
    root.classList.remove('lgm-has-panel');
    document.documentElement.classList.remove('lgm-sheet-open');
    if (selected) focusOn(selected);   // back exactly where you were
  }
  // A link inside the framed page that goes to the map would open a map inside the map (advisor catch,
  // 2026-11-07): take it at the top level instead, focusing the box when it is on the map already.
  frame.addEventListener('load', function () {
    var doc;
    try { doc = frame.contentDocument; } catch (e) { return; } // err-ok: a cross-origin page has no map links to fix
    if (!doc) return;
    doc.addEventListener('click', function (ev) {
      var a = ev.target.closest && ev.target.closest('a[href]');
      if (!a) return;
      var u;
      try { u = new URL(a.getAttribute('href'), doc.baseURI); } catch (e) { return; } // err-ok: not a URL, leave it
      if (u.origin !== location.origin || u.pathname !== '/map') return;
      ev.preventDefault();
      var f = u.searchParams.get('focus');
      if (f && nodes[f]) { closePanel(); select(nodes[f], !narrow()); }
      else location.href = u.href;
    });
  });

  document.getElementById('lgm-expand').addEventListener('click', function () {
    var full = panel.classList.toggle('full');
    this.textContent = full ? 'Side' : 'Expand';
  });
  closeBtn.addEventListener('click', closePanel);
  var forget = document.getElementById('lgm-forget');
  if (forget) forget.addEventListener('click', function () {
    visits = {};
    try { localStorage.removeItem(VKEY); } catch (e) {} // err-ok: private-mode storage throws; the in-memory visits are still cleared and the status line says so
    for (var k in edges) sizeEdge(edges[k]);
    for (var id in nodes) shade(nodes[id]);
    status('Visits forgotten on this device.');
  });
  document.getElementById('lgm-main').addEventListener('click', function () {
    // mAInstream: shrink the field back to the side and bring the mAInstream into view.
    panel.classList.remove('full');
    document.getElementById('lgm-expand').textContent = 'Expand';
    var r = nodes.root;
    var box = svg.getBoundingClientRect();
    scale = 1;
    tx = 40;
    ty = r ? box.height / 2 - (r.y + H / 2) : 0;
    apply(true);
  });

  // Pan by dragging the empty map; zoom with the wheel (around the pointer).
  var drag = null;
  svg.addEventListener('pointerdown', function (ev) {
    if (ev.target.closest && ev.target.closest('.lgn')) return;
    drag = { x: ev.clientX, y: ev.clientY, tx: tx, ty: ty };
    svg.setPointerCapture(ev.pointerId);
    svg.classList.add('lgm-drag');
  });
  svg.addEventListener('pointermove', function (ev) {
    if (!drag) return;
    tx = drag.tx + ev.clientX - drag.x;
    ty = drag.ty + ev.clientY - drag.y;
    apply();
  });
  function endDrag() { drag = null; svg.classList.remove('lgm-drag'); }
  svg.addEventListener('pointerup', endDrag);
  svg.addEventListener('pointercancel', endDrag);
  svg.addEventListener('wheel', function (ev) {
    ev.preventDefault();
    var box = svg.getBoundingClientRect();
    var px = ev.clientX - box.left, py = ev.clientY - box.top;
    var next = Math.min(2.5, Math.max(0.3, scale * (ev.deltaY < 0 ? 1.1 : 1 / 1.1)));
    tx = px - (px - tx) * next / scale;
    ty = py - (py - ty) * next / scale;
    scale = next;
    apply();
  }, { passive: false });

  // Start: the mAInstream and its Currents. ?focus=<id> also opens that box.
  var box = svg.getBoundingClientRect();
  ty = box.height / 2 - H / 2;
  apply();
  // Fit every box on the map into view (not under 0.6x; a phone uses focusOn's own rule instead).
  function fitAll() {
    var b = svg.getBoundingClientRect(), ids = Object.keys(nodes);
    if (!b.width || !ids.length) return;
    var x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
    ids.forEach(function (id) { var n = nodes[id]; x0 = Math.min(x0, leftX(n)); x1 = Math.max(x1, rightX(n)); y0 = Math.min(y0, n.y); y1 = Math.max(y1, n.y + H); });
    scale = Math.max(0.6, Math.min(1.1, (b.width - 40) / (x1 - x0), (b.height - 40) / (y1 - y0)));
    tx = Math.round((b.width - (x1 - x0) * scale) / 2 - x0 * scale);
    ty = Math.round((b.height - (y1 - y0) * scale) / 2 - y0 * scale);
    apply(true);
  }
  // Brandon 2026-11-07: the map is the way in, "with your most relevant boxes enlarged and in scope". So the start
  // opens the mAInstream, then the most relevant of its boxes (your own city or group when you have one), and
  // brings that branch into view.
  function mostRelevant(exceptIds) {
    var best = null;
    for (var id in nodes) {
      var n = nodes[id];
      if (exceptIds[id] || typeof n.rel !== 'number' || n.rel < 0.5) continue;
      if (!best || n.rel > best.rel) best = n;
    }
    return best;
  }
  load('root').then(function (data) {
    var r = addNode(data.node, 0, 0);
    selected = null;
    return expand(r);
  }).then(function () {
    var focus = root.getAttribute('data-focus');
    if (!focus) {
      var top = mostRelevant({ root: 1 });
      if (!top) { if (!narrow()) fitAll(); return; }
      return expand(top).then(function () { if (narrow()) focusOn(top); else fitAll(); });
    }
    if (nodes[focus]) { select(nodes[focus], !narrow()); return; }
    return load(focus).then(function (data) {
      var n = addNode(data.node, COLW * 2, 0);
      data.links.forEach(function (l) { if (nodes[l.node.id]) addEdge(n.id, l.node.id, l.edge); });
      select(n, !narrow());
    });
  }).catch(function (err) { status(err.message); });
})();
