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
  var closeBtn = document.getElementById('lgm-close');
  function narrow() { return window.matchMedia ? matchMedia('(max-width: 760px)').matches : innerWidth <= 760; }
  // ROWH 56 (was 46): a box can grow to 1.25x (45 px tall), so 46 px rows made the boxes closest to you overlap their
  // neighbours (polish 2026-11-09); 56 leaves ~10 px between even the biggest
  var W = 200, H = 36, COLW = 270, ROWH = 56, YOU_R = 58;
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
  // your city, groups, proposals...). The MAINstream always counts.
  function touched(id) {
    var n = nodes[id];
    return (visits[id] || 0) > 0 || (n && (n.kind === 'root' || n.kind === 'you' || (typeof n.rel === 'number' && n.rel >= 0.95)));
  }
  function sizeEdge(e) {
    e.el.style.setProperty('--w', edgeWidth(e).toFixed(2) + 'px');
    e.el.classList.toggle('lge-faint', !(touched(e.a) && touched(e.b)));
  }
  // Relevance shading: red (far from you) -> yellow -> green (yours). The server
  // scores it from your own involvement; frequent visits nudge it a little.
  function shade(n) {
    if (typeof n.rel !== 'number' || n.rel < 0) return;
    if (n.kind === 'mine' || n.kind === 'you') return; // the ring keeps the mockup's dark cards (Brandon 2026-11-11)
    var r = Math.min(1, n.rel + 0.05 * Math.log2(1 + (visits[n.id] || 0)));
    var hue = Math.round(r * 120);
    // the same relevance hue, with more color: .24 alpha read as washed out ("lifeless", polish 2026-11-09)
    n.el.querySelector('.lgs').style.fill = 'hsla(' + hue + ',78%,58%,.42)';
    n.el.setAttribute('data-rel', r.toFixed(2));
  }

  // Shapes by kind (Brandon 2026-11-07: "color and shape coding"): people round, groups square, cities oval,
  // proposals hexagon, offices octagon, problems a tag, posts a note. Colour stays relevance (shade).
  function shapeEl(kind, w, h) {
    var c = 10, poly = function (pts) { return el('polygon', { points: pts.map(function (p) { return p.join(','); }).join(' '), 'class': 'lgs' }); };
    switch (kind) {
      // M1.1 look (Grok Bot's mockup 1): you are a gold-ringed circle; your kinds are dark cards with a gold edge
      case 'you': return el('circle', { cx: w / 2, cy: h / 2, r: YOU_R, 'class': 'lgs' });
      case 'mine': return el('rect', { width: w, height: h + 10, y: -5, rx: 7, 'class': 'lgs' });
      case 'person': return el('rect', { width: w, height: h, rx: h / 2, 'class': 'lgs' });
      case 'group': return el('rect', { width: w, height: h, rx: 3, 'class': 'lgs' });
      case 'city': return el('ellipse', { cx: w / 2, cy: h / 2, rx: w / 2, ry: h / 2, 'class': 'lgs' });
      case 'prop': case 'sitewide': return poly([[c, 0], [w - c, 0], [w, h / 2], [w - c, h], [c, h], [0, h / 2]]);
      case 'dept': return poly([[7, 0], [w - 7, 0], [w, 7], [w, h - 7], [w - 7, h], [7, h], [0, h - 7], [0, 7]]);
      case 'pain': return poly([[0, 0], [w - 14, 0], [w, h / 2], [w - 14, h], [0, h]]);
      default: return el('rect', { width: w, height: h, rx: 9, 'class': 'lgs' });
    }
  }
  // Size from relevance: your own things are bigger (0.85x far .. 1.25x yours); the MAINstream stays 1x.
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
    if (A.kind === 'you' || B.kind === 'you') { // the ring around you: straight spokes, centre to centre
      return 'M' + (A.x + W / 2) + ' ' + y1 + ' L' + (B.x + W / 2) + ' ' + y2;
    }
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
    var n = { id: d.id, label: d.label, kind: d.kind, page: d.page, rel: d.rel, sub: d.sub || '', x: x,
      y: d.kind === 'mine' ? y : slot(x, y), expanded: false };  // ring places are exact (see ringPlace)
    var s = sizeOf(n);
    var g = el('g', { 'class': 'lgn lgn-' + n.kind + ' lgn-new', tabindex: '0', role: 'button',
      transform: 'translate(' + (n.x + W / 2 * (1 - s)) + ',' + (n.y + H / 2 * (1 - s)) + ') scale(' + s + ')' });
    g.appendChild(shapeEl(n.kind, W, H));
    if (n.kind === 'you') {
      // M1.1 look (Grok Bot's mockup 1): a gold disc with your initial, your handle, "YOU", all inside the ringed circle
      g.appendChild(el('circle', { cx: W / 2, cy: H / 2 - 16, r: 20, 'class': 'lgn-face' }));
      var ini = el('text', { x: W / 2, y: H / 2 - 9, 'class': 'lgn-initial' });
      ini.textContent = String(n.label || '?').charAt(0).toUpperCase();
      g.appendChild(ini);
      var nm = el('text', { x: W / 2, y: H / 2 + 22, 'class': 'lgn-youname' });
      nm.textContent = short(n.label, 18);
      g.appendChild(nm);
      var yl = el('text', { x: W / 2, y: H / 2 + 37, 'class': 'lgn-subt lgn-youtag' });
      yl.textContent = 'YOU';
      g.appendChild(yl);
    } else if (n.kind === 'mine') {
      // a dark card: title, and a short line under it ("3 people", "Fresno, then the rest of the site"), left-aligned
      var tt = el('text', { x: 14, y: H / 2 - 2, 'class': 'lgn-cardtitle' });
      tt.textContent = short(n.label, 22);
      g.appendChild(tt);
      var st = el('text', { x: 14, y: H / 2 + 13, 'class': 'lgn-subt lgn-cardsub' });
      st.textContent = short(n.sub, 32);
      g.appendChild(st);
    } else {
      var text = el('text', { x: W / 2, y: H / 2 + 5 });
      text.textContent = short(n.label, 26);
      g.appendChild(text);
    }
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
    n.el = g;
    addExtras(n);
    setTimeout(function () { g.classList.remove('lgn-new'); }, 20);
    nodes[n.id] = n;
    shade(n);
    return n;
  }
  // The elements that sit beside a box at its position; recentre() calls this again after moving a box to the middle.
  function addExtras(n) {
    // Screen readers (Brandon 2026-11-09): VoiceOver and TalkBack turn a double tap into a plain click, which on a
    // phone only grows connections, and they cannot press and hold. So a box with a page gets an "Open <name>" button
    // right after it in reading order: 1x1, transparent, no pointer events, not a Tab stop (Enter on the box opens).
    if (n.page) {
      var sr = el('g', { 'class': 'lgn-sr', role: 'button', 'aria-label': 'Open ' + n.label });
      sr.appendChild(el('rect', { x: n.x + W - 1, y: n.y + H - 1, width: 1, height: 1 }));
      sr.addEventListener('click', function (ev) { ev.stopPropagation(); select(n, true); });
      nodeLayer.insertBefore(sr, n.el.nextSibling);
      n.sr = sr;
    }
    if (n.kind === 'you') {
      // "Open your page" under the you box, as its OWN element (not inside the box's group: a group that held it had its
      // centre between box and button, so a press on that centre missed the box; CI phone_check 4/4b, 2026-11-11).
      // aria-hidden: screen readers already get "Open <you>" from the box's own 1px button (one per box, check 6c).
      var ob = el('g', { 'class': 'lgn-openbtn', 'aria-hidden': 'true', transform: 'translate(' + n.x + ',' + n.y + ')' });
      ob.appendChild(el('rect', { x: W / 2 - 62, y: H / 2 + YOU_R + 8, width: 124, height: 24, rx: 12 }));
      var obt = el('text', { x: W / 2, y: H / 2 + YOU_R + 24 }); obt.textContent = 'Open your page'; ob.appendChild(obt);
      ob.addEventListener('click', function (ev) { ev.stopPropagation(); select(n, true); });
      nodeLayer.appendChild(ob);
    }
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

  // Box i of k on an ellipse around box c, starting at the top and going clockwise. A phone gets a narrower, taller
  // ring so the whole ring fits the width at a readable scale.
  function ringPlace(c, i, k) {
    // An even count is turned half a step, so no box sits level with you (left/right boxes overlapped the you box on
    // a phone, 2026-11-11 screenshot with 4 kinds).
    var step = 2 * Math.PI / Math.max(1, k), turn = k % 2 === 0 ? step / 2 : 0;
    // kind boxes draw at 1.25x (sizeOf), so left/right neighbours need centres >= 250 px apart: rx 200 on a phone
    var r = ringRadii(), a = -Math.PI / 2 + turn + step * i;
    return { x: Math.round(c.x + r.rx * Math.cos(a)), y: Math.round(c.y + r.ry * Math.sin(a)) };
  }
  function ringRadii() { return narrow() ? { rx: 200, ry: 190 } : { rx: 300, ry: 165 }; }

  // Collapse (Brandon 2026-11-11: "if i select a higher level box, shouldnt the previous branches collapse?"): when a box
  // is selected, keep only you, your kind ring, the path from you to this box, and what this box opens onto; branches
  // off that path close. Going back up the trail closes what was open below.
  // The centre (Grok Bot's mockup 1: "tap a kind, its items branch out; tap an item, it becomes the centre"): you or one
  // of your kinds sits in the middle with its connections on a ring; centrePath is the trail back (You > Friends).
  var centrePath = [];
  function centreId() { return centrePath.length ? centrePath[centrePath.length - 1].id : 'you'; }
  function dataOf(n) { return { id: n.id, label: n.label, kind: n.kind, page: n.page, rel: n.rel, sub: n.sub }; }
  function isCentreKind(n) { return n.kind === 'you' || n.kind === 'mine'; }
  // m: the tapped box (its own element is kept and moved to the middle: a replaced element broke anything holding it,
  // CI phone_check 3, 2026-11-11), or a trail entry's data when that box is no longer drawn.
  function recentre(m) {
    var keep = m.el && nodes[m.id] === m ? m : null;
    for (var k in edges) if (edges[k].el.parentNode) edges[k].el.parentNode.removeChild(edges[k].el);
    [].slice.call(nodeLayer.childNodes).forEach(function (c) { if (!keep || c !== keep.el) nodeLayer.removeChild(c); });
    var oldRing = document.getElementById('lgm-ring');
    if (oldRing && oldRing.parentNode) oldRing.parentNode.removeChild(oldRing);
    nodes = {};
    edges = {};
    var d = dataOf(m), at = -1;
    for (var i = 0; i < centrePath.length; i++) if (centrePath[i].id === d.id) at = i;
    if (at >= 0) centrePath = centrePath.slice(0, at + 1); else centrePath.push(d);
    if (!keep) return addNode(d, 0, 0);
    keep.x = 0; keep.y = 0; keep.expanded = false; delete keep.from;
    keep.el.classList.remove('lgn-open');
    keep.el.setAttribute('transform', boxTransform(keep, 1));
    nodes[keep.id] = keep;
    addExtras(keep);
    return keep;
  }
  function collapseTo(n) {
    var keep = {}, cid = centreId();
    keep[cid] = 1;
    for (var id in nodes) if (nodes[id].from === cid) keep[id] = 1;
    for (var c = n, guard = 0; c && guard < 50; c = c.from ? nodes[c.from] : null, guard++) keep[c.id] = 1;
    for (var id2 in nodes) if (nodes[id2].from === n.id) keep[id2] = 1;
    var gone = {};
    for (var id3 in nodes) {
      if (keep[id3]) continue;
      var x = nodes[id3];
      if (x.el && x.el.parentNode) x.el.parentNode.removeChild(x.el);
      if (x.sr && x.sr.parentNode) x.sr.parentNode.removeChild(x.sr);
      gone[id3] = 1;
      delete nodes[id3];
    }
    for (var k in edges) {
      var e = edges[k];
      if (gone[e.a] || gone[e.b]) { if (e.el.parentNode) e.el.parentNode.removeChild(e.el); delete edges[k]; }
    }
    for (var id4 in nodes) { // a box whose children were closed can open again
      var m = nodes[id4], kids = false;
      for (var j in nodes) if (nodes[j].from === id4) { kids = true; break; }
      if (m.expanded && !kids && id4 !== n.id && id4 !== cid) { m.expanded = false; m.el.classList.remove('lgn-open'); }
    }
  }
  function expand(n) {
    if (n.expanded) return Promise.resolve(n);
    status('Loading…');
    return load(n.id).then(function (data) {
      // around a kind, "you" is the way back (the trail), not one of its items
      var links = n.kind === 'mine' ? data.links.filter(function (l) { return l.node.id !== 'you'; }) : data.links;
      var fresh = links.filter(function (l) { return !nodes[l.node.id]; });
      if (isCentreKind(n) && n.id === centreId()) { // M1.1 look (Grok Bot's mockup 1): the centre's connections on a ring
        // the faint dotted ring behind them (mockup), drawn once under the lines
        if (!document.getElementById('lgm-ring')) {
          var ring = el('ellipse', { id: 'lgm-ring', 'class': 'lgm-ring', cx: n.x + W / 2, cy: n.y + H / 2,
            rx: ringRadii().rx, ry: ringRadii().ry });
          edgeLayer.insertBefore(ring, edgeLayer.firstChild);
        }
        fresh.forEach(function (l, i) { var p = ringPlace(n, i, fresh.length); addNode(l.node, p.x, p.y).from = n.id; });
      } else {
        var x = n.x + COLW;
        var start = n.y - (fresh.length - 1) * ROWH / 2;
        fresh.forEach(function (l, i) { addNode(l.node, x, start + i * ROWH).from = n.id; });
      }
      links.forEach(function (l) { addEdge(n.id, l.node.id, l.edge); });
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
    if (n.sr) nodeLayer.appendChild(n.sr);  // its screen-reader button stays right after it
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
  // The trail (M1.1 look, Grok Bot's mockup 1): how you got to box n, from You, each step a button that goes back there.
  var trailEl = document.getElementById('lgm-trail');
  function trail(n) {
    if (!trailEl) return;
    // the centres you stepped through (You > Friends), then the boxes from the centre out to n
    var tail = [], seen = {}, cid = centreId();
    for (var c = n; c && c.id !== cid && !seen[c.id] && tail.length < 12; c = c.from ? nodes[c.from] : null) { seen[c.id] = 1; tail.unshift(c); }
    var chain = centrePath.concat(tail);
    trailEl.textContent = '';
    chain.forEach(function (c, i) {
      if (i) { var sep = document.createElement('span'); sep.className = 'lgm-trail-sep'; sep.textContent = '›'; trailEl.appendChild(sep); }
      var b = document.createElement('button');
      b.type = 'button';
      b.className = 'lgm-trail-step' + (i === chain.length - 1 ? ' here' : '');
      b.textContent = c.kind === 'you' ? 'You' : short(c.label, 22);
      if (i === chain.length - 1) b.setAttribute('aria-current', 'location');
      else b.addEventListener('click', function () { select(nodes[c.id] || c, false); });
      trailEl.appendChild(b);
    });
  }
  function select(n, open) {
    // you or one of your kinds becomes the centre, with its connections on a ring (Brandon 2026-11-11)
    if (isCentreKind(n) && n.id !== centreId()) {
      // phone, a tap that did not open: wait the double-tap window first, so a second tap still lands on the box and
      // opens its page instead of hitting the spot it moved away from (CI phone_check 4c, 2026-11-11)
      if (narrow() && !open && n.el) {
        if (selected && selected.el) selected.el.classList.remove('lgn-sel');
        selected = n;
        n.el.classList.add('lgn-sel');
        setTimeout(function () { if (selected === n && centreId() !== n.id) select(n, 'now'); }, GLIDE_WAIT_MS);
        return;
      }
      n = recentre(n);
      if (open === 'now') open = false;
    }
    var prev = selected;
    if (selected) selected.el.classList.remove('lgn-sel');
    selected = n;
    n.el.classList.add('lgn-sel');
    visits[n.id] = (visits[n.id] || 0) + 1;
    var from = prev && prev !== n && edges[edgeKey(prev.id, n.id)] ? prev.id
             : (n.from && edges[edgeKey(n.from, n.id)] ? n.from : null);
    if (from) { var ek = 'e:' + edgeKey(from, n.id); visits[ek] = (visits[ek] || 0) + 1; }
    saveVisits();
    collapseTo(n);
    shade(n);
    trail(n);
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
      // on the ring (you or one of your kinds) keep the whole open map in view instead of sliding the tapped box to the
      // edge (2026-11-11 CI screenshot: tapping Friends cut the cards off at the left)
      setTimeout(function () { if (selected === n) { if (isCentreKind(n)) fitAll(true); else focusOn(n); } }, wait);
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
      closeBtn.textContent = '‹ MAINstream';
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
    // MAINstream: shrink the field back to the side and bring the MAINstream into view.
    panel.classList.remove('full');
    document.getElementById('lgm-expand').textContent = 'Expand';
    var r = nodes.root || nodes.you; // the map starts at you; the MAINstream is loaded once Places is opened
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

  // Start: the MAINstream and its Currents. ?focus=<id> also opens that box.
  var box = svg.getBoundingClientRect();
  ty = box.height / 2 - H / 2;
  apply();
  // Fit every box on the map into view (not under 0.6x; a phone uses focusOn's own rule instead).
  // top: place the boxes near the top of the frame instead of centring them (the first view: centred, the Juketube tip
  // that first-time visitors see covered the you box on a phone, phone_check 4/4b + #jbhint probe 2026-11-11).
  function fitAll(top) {
    var b = svg.getBoundingClientRect(), ids = Object.keys(nodes);
    if (!b.width || !ids.length) return;
    var x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
    ids.forEach(function (id) { var n = nodes[id]; x0 = Math.min(x0, leftX(n)); x1 = Math.max(x1, rightX(n)); y0 = Math.min(y0, n.y); y1 = Math.max(y1, n.y + H); });
    y1 += 40; // room for "Open your page" under the you box
    scale = Math.max(0.6, Math.min(1.1, (b.width - 40) / (x1 - x0), (b.height - 40) / (y1 - y0)));
    tx = Math.round((b.width - (x1 - x0) * scale) / 2 - x0 * scale);
    ty = top ? Math.round(16 - y0 * scale) : Math.round((b.height - (y1 - y0) * scale) / 2 - y0 * scale);
    apply(!top); // the first view is placed, not glided in
  }
  // M1.1 (Brandon 2026-11-10): the map starts at you, with your kinds of connection (Friends, Groups, Places,
  // Politics, Problems) as the first ring. The whole site (MAINstream) is under Places, and its button still works.
  load('you').then(function (data) {
    var r = addNode(data.node, 0, 0);
    centrePath = [dataOf(r)];
    selected = null;
    trail(r); // "You" until you step out
    return expand(r);
  }).then(function () {
    var focus = root.getAttribute('data-focus');
    if (!focus) { fitAll(true); return; } // the whole ring in view near the top, phone included (nothing cut off or covered)
    if (nodes[focus]) { select(nodes[focus], !narrow()); return; }
    return load(focus).then(function (data) {
      var n = addNode(data.node, COLW * 2, 0);
      data.links.forEach(function (l) { if (nodes[l.node.id]) addEdge(n.id, l.node.id, l.edge); });
      select(n, !narrow());
    });
  }).catch(function (err) { status(err.message); });
})();
