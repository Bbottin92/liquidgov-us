// LiquidGov interactive map (/map). Brandon 2026-11-06: click a box and its
// connections expand out of it; if the box is something people engage with,
// its page shows in the field on the right; the field can fill the window, and
// the MAINstream button at the top shrinks it back.
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
  // your city, groups, proposals...). The Main Stream always counts.
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
    n.el.querySelector('rect').style.fill = 'hsla(' + hue + ',72%,48%,.24)';
    n.el.setAttribute('data-rel', r.toFixed(2));
  }

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
      x1 = A.x + W; x2 = B.x + W;
      return 'M' + x1 + ' ' + y1 + ' C' + (x1 + 60) + ' ' + y1 + ' ' + (x2 + 60) + ' ' + y2 + ' ' + x2 + ' ' + y2;
    }
    x1 = A.x + W; x2 = B.x;
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
    var g = el('g', { 'class': 'lgn lgn-' + n.kind + ' lgn-new', transform: 'translate(' + n.x + ',' + n.y + ')', tabindex: '0', role: 'button' });
    g.appendChild(el('rect', { width: W, height: H, rx: 9 }));
    var text = el('text', { x: W / 2, y: H / 2 + 5 });
    text.textContent = short(n.label, 26);
    g.appendChild(text);
    var title = el('title', {}); title.textContent = n.label; g.appendChild(title);
    g.addEventListener('click', function (ev) { ev.stopPropagation(); select(n); });
    g.addEventListener('keydown', function (ev) { if (ev.key === 'Enter' || ev.key === ' ') { ev.preventDefault(); select(n); } });
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
    return fetch('/map/node?n=' + encodeURIComponent(id), { credentials: 'same-origin', headers: { Accept: 'application/json' } })
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

  function select(n) {
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
    if (n.page) openPanel(n); else closePanel();
    // After the field opens (the map narrows), glide to the box and what grew out of it.
    expand(n).then(function () { light(n.id); focusOn(n); });
  }

  function openPanel(n) {
    panel.hidden = false;
    root.classList.add('lgm-has-panel');
    ptitle.textContent = n.label;
    openLink.href = n.page;
    // Same page, framed: the server drops its header for framed loads.
    if (frame.getAttribute('src') !== n.page) frame.setAttribute('src', n.page);
  }
  function closePanel() {
    panel.hidden = true;
    panel.classList.remove('full');
    root.classList.remove('lgm-has-panel');
  }

  document.getElementById('lgm-expand').addEventListener('click', function () {
    var full = panel.classList.toggle('full');
    this.textContent = full ? 'Side' : 'Expand';
  });
  document.getElementById('lgm-close').addEventListener('click', closePanel);
  var forget = document.getElementById('lgm-forget');
  if (forget) forget.addEventListener('click', function () {
    visits = {};
    try { localStorage.removeItem(VKEY); } catch (e) {}
    for (var k in edges) sizeEdge(edges[k]);
    for (var id in nodes) shade(nodes[id]);
    status('Visits forgotten on this device.');
  });
  document.getElementById('lgm-main').addEventListener('click', function () {
    // MAINstream: shrink the field back to the side and bring the Main Stream into view.
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

  // Start: the Main Stream and its Currents. ?focus=<id> also opens that box.
  var box = svg.getBoundingClientRect();
  ty = box.height / 2 - H / 2;
  apply();
  load('root').then(function (data) {
    var r = addNode(data.node, 0, 0);
    selected = null;
    return expand(r);
  }).then(function () {
    var focus = root.getAttribute('data-focus');
    if (!focus) return;
    if (nodes[focus]) { select(nodes[focus]); return; }
    return load(focus).then(function (data) {
      var n = addNode(data.node, COLW * 2, 0);
      data.links.forEach(function (l) { if (nodes[l.node.id]) addEdge(n.id, l.node.id, l.edge); });
      select(n);
    });
  }).catch(function (err) { status(err.message); });
})();
