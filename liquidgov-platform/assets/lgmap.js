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
  // The side panel is view-only until Edit (Brandon 2026-11-11 4:30 PM decision 2; live map finding #11: Interests
  // opened the whole profile editor, The Gov the composer and Leave, Politics New proposal). Every form that changes
  // something (method=post) is hidden in the panel's page; Edit shows them, Done hides them. Opening another box
  // starts view-only again; a reload in the panel (a form was sent) keeps editing.
  var editBtn = document.getElementById('lgm-edit'), editing = false;
  var EDIT_FORMS = 'form[method="post" i]';
  function applyView() {
    var doc;
    try { doc = frame.contentDocument; } catch (e) { doc = null; } // err-ok: a page from another site is left as it is
    var has = !!(doc && doc.querySelector && doc.querySelector(EDIT_FORMS));
    if (editBtn) {
      editBtn.hidden = !has;
      editBtn.textContent = editing ? 'Done' : 'Edit';
      editBtn.setAttribute('aria-pressed', editing ? 'true' : 'false');
    }
    if (!doc || !doc.documentElement) return;
    var st = doc.getElementById('lgm-viewonly');
    if (!editing && !st) {
      st = doc.createElement('style');
      st.id = 'lgm-viewonly';
      st.textContent = EDIT_FORMS + '{display:none!important}';
      (doc.head || doc.documentElement).appendChild(st);
    } else if (editing && st) st.parentNode.removeChild(st);
  }
  if (editBtn) editBtn.addEventListener('click', function () { editing = !editing; applyView(); });
  function narrow() { return window.matchMedia ? matchMedia('(max-width: 760px)').matches : innerWidth <= 760; }
  var W = 200, H = 36, YOU_R = 58;
  // Pools (Brandon's drawing 2026-11-15, "bigger and not square"): with the walker on the map (data-pools on #lgmap),
  // every box is a big oval pool, about a screen wide for the one you are in, and every connection a wide channel.
  // poolR: a pool's half width in map units (its half height is POOL_Y of that); the label's size follows the pool.
  var POOLS = root.getAttribute('data-pools') === '1';
  var POOL_Y = 0.9, POOL_GAP = 240;
  function poolR(d) { return d.R || (d.kind === 'you' ? 220 : d.kind === 'mine' ? 175 : d.kind === 'more' ? 120 : 145); }
  // Room on a pool's shore: each channel's mouth takes MOUTH map units of it (its 98-wide shore and some land either
  // side), so a pool with many channels grows (poolFit) until they all fit, and the channels leaving it are spread at
  // least mouthAngle apart (worldPlace): mouths never run into each other.
  var MOUTH = 140;
  function mouthAngle(R) { return 2 * Math.asin(Math.min(1, MOUTH / 2 / R)); }
  // A pool grows at most to POOL_GROW times its own size, and never into another pool (POOL_LAND of land kept between):
  // a pool that grew without a limit spread over its neighbours (map_world_check O4, 2026-11-15). What does not fit
  // goes behind "+N more" (capRing: poolCap).
  var POOL_GROW = 2, POOL_LAND = 60;
  function poolBase(d) { var R = d.R; d.R = 0; var b = poolR(d); d.R = R; return b; }
  function poolMax(d) { return poolBase(d) * POOL_GROW; }
  function setPoolR(c, R) {
    R = Math.min(poolMax(c), R);
    if (R <= poolR(c)) return false;
    var cc = c.region, ok = Object.keys(nodes).every(function (id) {
      if (id === c.id) return true;
      var o = nodes[id].region;
      return Math.hypot(o.cx - cc.cx, o.cy - cc.cy) >= R + Math.max(o.hw, o.hh) + POOL_LAND;
    });
    if (!ok) return false;
    c.R = R;
    var sh = c.el && c.el.querySelector('.lgs');
    if (sh) { sh.setAttribute('rx', R); sh.setAttribute('ry', R * POOL_Y); }
    return true;
  }
  function poolFit(c, k) {
    var R = poolR(c);
    while (k * mouthAngle(R) > 2 * Math.PI * 0.9 && R < poolMax(c)) R += 10;
    setPoolR(c, R);
  }
  // how many new channels a pool can take: what fits round it at its largest, less the way back
  function poolCap(c) { return Math.max(3, Math.floor(2 * Math.PI * 0.9 / mouthAngle(poolMax(c))) - 1); }
  function poolFont(d) { return Math.round(poolR(d) * (d.kind === 'you' ? 0.3 : 0.2)); }
  function poolLabel(d) { return d.kind === 'you' ? 'Main' : d.label; }
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
    if (POOLS) return 1;   // a pool's size is its radius (poolR), not a scale
    if (n.kind === 'more') return 1.1; // the "+N more" box: as big as the boxes around it, so it never sets the zoom floor
    return (typeof n.rel === 'number' && n.rel >= 0) ? 0.85 + 0.4 * Math.min(1, n.rel) : 1;
  }
  // Readable names (Brandon's Brief 5 item 2; Grok Bot's audit: the 0.4x floor drew 14 px names at about 5.6 px): the map
  // never zooms below the scale that draws every box's name at MIN_PX on screen. Names are 14 px (lgmap.css) times the
  // box's size; the small grey second lines are not names.
  var NAME_PX = 14, MIN_PX = 11;
  // The map's zoom (Brandon 2026-11-13 5:01 AM: no Zoom out button; in Scroll mode the mouse wheel zooms, a pinch on a
  // touch screen): the one place its limits live. min/max: the scale the map may reach (and never below the scale that
  // keeps names MIN_PX, floorScale); wheel: one notch's factor. The walker's camera stays under max too.
  var ZOOM = { min: 0.3, max: 2.5, wheel: 1.1 };
  function floorFor(list) {
    var f = 0;
    list.forEach(function (d) { f = Math.max(f, MIN_PX / (POOLS ? poolFont(d) : NAME_PX * sizeOf(d))); });
    return Math.min(1, f + 0.002); // + rounding slack
  }
  function floorScale() { return floorFor(Object.keys(nodes).map(function (id) { return nodes[id]; })); }
  // Box widths from their labels (live look #4: "Genesis Executive Officer..." ellipsized in a 200 px box with room to
  // spare). On a desktop a box grows to fit its label, up to MAX_W; a phone keeps W (its two columns are sized for it).
  // Labels are measured, not counted, so one never spills out of its box. A box stays centered where W would sit.
  var MAX_W = 420, meas, measFamily;
  // a phone's box width and column offset, set per level by capRing so two columns fit at a readable zoom
  var phoneW = W, phoneDX = 141;
  var F_LABEL = '600 14px', F_TITLE = '700 14px', F_SUB = '400 10.5px';
  function textW(s, font) {
    if (meas === undefined) {
      try { meas = document.createElement('canvas').getContext('2d'); } catch (e) { meas = null; } // err-ok: no canvas: estimate below
      measFamily = (window.getComputedStyle && getComputedStyle(svg).fontFamily) || 'sans-serif';
    }
    s = String(s || '');
    if (!meas) return s.length * 9;
    meas.font = font + ' ' + measFamily;
    return meas.measureText(s).width * 1.03; // + the labels' .01em letter spacing, and a little slack
  }
  function fitText(s, font, room) {
    s = String(s || '');
    if (textW(s, font) <= room) return s;
    var lo = 0, hi = s.length;
    while (lo < hi) { var mid = (lo + hi + 1) >> 1; if (textW(s.slice(0, mid) + '…', font) <= room) lo = mid; else hi = mid - 1; }
    return s.slice(0, lo).replace(/\s+$/, '') + '…';
  }
  // room for the pointed ends of proposals, Sitewide and problems
  function padOf(kind) { return kind === 'prop' || kind === 'sitewide' || kind === 'pain' ? 44 : 28; }
  function widthOf(d) {
    if (d.kind === 'you' || POOLS) return W;
    if (narrow()) return phoneW;
    var need = d.kind === 'mine' ? Math.max(textW(d.label, F_TITLE), textW(d.sub, F_SUB)) + 28 : textW(d.label, F_LABEL) + padOf(d.kind);
    return Math.ceil(Math.min(MAX_W, Math.max(W, need)));
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
  function short(s, n) { s = String(s); return s.length > n ? s.slice(0, n - 1) + '…' : s; }

  function centerOf(n) { return { x: n.x + W / 2, y: n.y + H / 2 }; }
  // Where the line from n's middle toward (x, y) leaves n's own shape, plus a 4 px gap (live look #4: lines ran
  // through the labels; a box's fill is see-through, so a line under it still showed).
  function edgeEnd(n, x, y) {
    var c = centerOf(n), dx = x - c.x, dy = y - c.y, len = Math.sqrt(dx * dx + dy * dy), s = sizeOf(n), t;
    if (len < 1 || POOLS) return c;   // pools: channels run middle to middle, under the pools
    if (n.kind === 'you') t = YOU_R * s / len;
    else if (n.kind === 'city') { var rx = (n.w || W) / 2 * s, ry = H / 2 * s; t = 1 / Math.sqrt(dx * dx / (rx * rx) + dy * dy / (ry * ry)); }
    else {
      var hw = (n.w || W) / 2 * s, hh = (n.kind === 'mine' ? H + 10 : H) / 2 * s;
      t = Math.min(dx ? hw / Math.abs(dx) : Infinity, dy ? hh / Math.abs(dy) : Infinity);
    }
    t = Math.min(1, t + 4 / len);
    return { x: c.x + dx * t, y: c.y + dy * t };
  }
  // Every line is a spoke from the center to a box on its ring, edge to edge. On a phone the ring is two columns, and
  // a straight spoke to the second row crossed the first row's cards (live look #9): there it runs down the gap
  // between the columns and turns into the card's inner edge.
  function edgePath(e) {
    var A = nodes[e.a], B = nodes[e.b];
    if (B.id === centerId()) { var t0 = A; A = B; B = t0; }
    var ca = centerOf(A), cb = centerOf(B);
    var p = edgeEnd(A, cb.x, cb.y), q = edgeEnd(B, ca.x, ca.y);
    return 'M' + p.x.toFixed(1) + ' ' + p.y.toFixed(1) + ' L' + q.x.toFixed(1) + ' ' + q.y.toFixed(1);
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

  function copyValve(v) { return { type: v.type, owner: v.owner || '', invite: (v.invite || []).slice() }; }
  function addNode(d, x, y, parent) {
    if (nodes[d.id]) return nodes[d.id];
    var n = { id: d.id, label: d.label, kind: d.kind, page: d.page, rel: d.rel, sub: d.sub || '', x: x, y: y,
      expanded: false, more: d.more, css: d.css || '',   // ring places are exact (see ringPlace); css: spatial step 10, an admin's CSS for this instance
      // spatial steps 6-7: who may enter. The box's own valve if its data has one, else a copy of its parent's (the box
      // it was opened from), else Open. A valve is { type: open|conduit|invite|private|request|closed, owner, invite[] }.
      valve: d.valve && d.valve.type ? d.valve : parent && parent.valve ? copyValve(parent.valve) : { type: 'open' } };
    n.w = widthOf(n);
    // spatial step 2: the box's region (center + half extents, map units), always current; nothing reads it yet
    Object.defineProperty(n, 'region', { get: function () { var k = sizeOf(n);
      if (POOLS) { var R = poolR(n); return { cx: n.x + W / 2, cy: n.y + H / 2, hw: R, hh: R * POOL_Y, pool: true }; }
      return { cx: n.x + W / 2, cy: n.y + H / 2, hw: (n.w || W) * k / 2, hh: H * k / 2 }; } });
    var s = sizeOf(n), ox = (W - n.w) / 2;
    var g = el('g', { 'class': 'lgn lgn-' + n.kind + ' lgn-new', tabindex: '0', role: 'button', 'data-id': n.id, 'data-valve': n.valve.type,
      'data-valve-owner': n.valve.owner || '', 'data-valve-invite': (n.valve.invite || []).join(','),
      transform: 'translate(' + (n.x + W / 2 * (1 - s)) + ',' + (n.y + H / 2 * (1 - s)) + ') scale(' + s + ')' });
    var shape = POOLS ? el('ellipse', { cx: W / 2, cy: H / 2, rx: poolR(n), ry: poolR(n) * POOL_Y, 'class': 'lgs' }) : shapeEl(n.kind, n.w, H);
    if (ox) shape.setAttribute('transform', 'translate(' + ox + ',0)');
    g.appendChild(shape);
    if (POOLS) {
      // the pool's name, big in its middle, with a line of waves under it
      var R = poolR(n), fs = poolFont(n), pt = el('text', { x: W / 2, y: H / 2 + fs * 0.3, 'class': 'lgn-pool-name' });
      pt.style.fontSize = fs + 'px';
      pt.textContent = fitText(poolLabel(n), '700 ' + fs + 'px', R * 1.6);
      g.appendChild(pt);
      var wy = H / 2 + fs * 0.75, ww = Math.min(R * 1.2, textW(pt.textContent, '700 ' + fs + 'px') * 1.15), wx = W / 2 - ww / 2, amp = fs * 0.12, waves = 4, d = 'M' + wx.toFixed(1) + ' ' + wy.toFixed(1);
      for (var wi = 0; wi < waves; wi++) d += ' q' + (ww / waves / 2).toFixed(1) + ' ' + (wi % 2 ? amp : -amp).toFixed(1) + ' ' + (ww / waves).toFixed(1) + ' 0';
      g.appendChild(el('path', { d: d, 'class': 'lgn-pool-wave' }));
    } else if (n.kind === 'you') {
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
      var tt = el('text', { x: ox + 14, y: H / 2 - 2, 'class': 'lgn-cardtitle' });
      tt.textContent = fitText(n.label, F_TITLE, n.w - 28);
      g.appendChild(tt);
      var st = el('text', { x: ox + 14, y: H / 2 + 13, 'class': 'lgn-subt lgn-cardsub' });
      st.textContent = fitText(n.sub, F_SUB, n.w - 28);
      g.appendChild(st);
    } else {
      var text = el('text', { x: W / 2, y: H / 2 + 5 });
      text.textContent = fitText(n.label, F_LABEL, n.w - padOf(n.kind));
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
      if (ev.pointerType === 'mouse' || !narrow() || pinch) return;   // a second finger is a pinch, not a hold
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
  // The elements that sit beside a box at its position.
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
      // center between box and button, so a press on that center missed the box; CI phone_check 4/4b, 2026-11-11).
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
  function ringPlace(c, i, k, r) {
    if (POOLS) {
      // pools: half the ring above the pool, half below, fanned across 120 degrees (none level with it), far enough that
      // the channels between them are long and wide (Brandon's drawing: four channels leaving diagonally)
      var up = Math.ceil(k / 2), j = i < up ? i : i - up, m = i < up ? up : k - up, span = Math.PI * 2 / 3;
      var t = m === 1 ? 0.5 : m === 2 ? (j ? 0.75 : 0.25) : j / (m - 1);   // left to right
      var ang = i < up ? -Math.PI * 5 / 6 + span * t : Math.PI * 5 / 6 - span * t;
      var D = poolR(c) + 175 + POOL_GAP + Math.max(0, m - 2) * 60;
      return { x: Math.round(c.x + D * Math.cos(ang)), y: Math.round(c.y + D * POOL_Y * 1.15 * Math.sin(ang)) };
    }
    // Phone (map SPEC phase 1 added Feeds, Interests, Events: 5 or more kinds ran off the edges and overlapped, CI
    // phone_check 2, 2026-11-11): the look Brandon saw with 4 kinds - two columns, a row above you and a row below - kept
    // and grown downward and upward, two cards a row, in reading order; an odd last card sits centered under you.
    if (narrow()) {
      // the extra row (odd row count) goes below you, so you stay near the top of the frame (2026-11-11 CI shot: two
      // rows above pushed you and the last card down under the Juketube tip)
      var rows = Math.ceil(k / 2), above = Math.max(1, Math.floor(rows / 2)), row = Math.floor(i / 2), col = i % 2;
      var y = row < above ? -(PHONE_BASE + (above - 1 - row) * PHONE_GAP) : PHONE_BASE + 40 + (row - above) * PHONE_GAP;
      var alone = i === k - 1 && k % 2 === 1;
      return { x: Math.round(c.x + (alone ? 0 : (col ? 1 : -1) * phoneDX)), y: Math.round(c.y + y) };
    }
    // An even count is turned half a step, so no box sits level with you (left/right boxes overlapped the you box on
    // a phone, 2026-11-11 screenshot with 4 kinds).
    var step = 2 * Math.PI / Math.max(1, k), turn = k % 2 === 0 ? step / 2 : 0;
    // kind boxes draw at 1.25x (sizeOf), so left/right neighbours need centers >= 250 px apart: rx 200 on a phone
    var a = -Math.PI / 2 + turn + step * i;
    r = r || ringRadii();
    return { x: Math.round(c.x + r.rx * Math.cos(a)), y: Math.round(c.y + r.ry * Math.sin(a)) };
  }
  // phone rows: the first row clears your circle (58) + half a card; the first row below also clears "Open your page";
  // columns phoneDX either side (capRing sets it per level: half the widest card plus a 6 px gap, 131 for your kinds)
  // PHONE_GAP 80: a card is ~45 px tall at 1.25x plus its second line; 64 left them touching (2026-11-11 CI shot)
  var PHONE_BASE = 120, PHONE_GAP = 80;
  // k: how many boxes go round the center. On a phone the dotted ring is as tall as the rows it encloses.
  function ringRadii(k) {
    if (!narrow()) return { rx: 300, ry: 165 };
    var rows = Math.ceil((k || 4) / 2), above = Math.max(1, Math.floor(rows / 2)), below = rows - above;
    return { rx: Math.round(phoneDX * 200 / 141), ry: Math.max(150, PHONE_BASE + 40 + Math.max(above, below) * PHONE_GAP - PHONE_GAP + 30) };
  }
  // A ring roomy enough that no two of its boxes (nor a box and the center) overlap: any box can be a center now
  // (Brandon 2026-11-11 live look #2), so a ring can hold a dozen boxes, not just your 5-8 kinds. Desktop only: the phone
  // keeps its two columns. list: the boxes' data, in ring order.
  function ringFit(c, list) {
    var r = ringRadii(list.length);
    if (narrow() || list.length < 2) return r;
    var half = function (d) { var s = sizeOf(d); return { w: widthOf(d) / 2 * s + 8, h: (d.kind === 'mine' ? H + 10 : H) / 2 * s + 8 }; };
    for (var tries = 0; tries < 40; tries++) {
      var pts = list.map(function (d, i) { var p = ringPlace(c, i, list.length, r), h = half(d); return { x: p.x, y: p.y, w: h.w, h: h.h }; });
      var hc = c.kind === 'you' ? { w: YOU_R * sizeOf(c) + 8, h: YOU_R * sizeOf(c) + 8 } : half(c);
      pts.push({ x: c.x, y: c.y, w: hc.w, h: hc.h });
      var clash = false;
      for (var i = 0; i < pts.length && !clash; i++)
        for (var j = i + 1; j < pts.length; j++)
          if (Math.abs(pts[i].x - pts[j].x) < pts[i].w + pts[j].w && Math.abs(pts[i].y - pts[j].y) < pts[i].h + pts[j].h) { clash = true; break; }
      if (!clash) break;
      r = { rx: Math.round(r.rx * 1.08), ry: Math.round(r.ry * 1.08) };
    }
    return clash ? null : r; // null: no ring without overlaps in 40 tries
  }

  // Big levels (Brief 5 item 2; Grok Bot's audit: a group can have 40+ members, and the ring grew until boxes ran off the
  // frame): a ring holds as many boxes as fit the frame at a readable zoom (floorFor); the rest go behind a "+N more" box,
  // which becomes the center with the rest around it when tapped (capped the same way). Boxes keep the server's order.
  // CAP_MIN (Brandon 2026-11-12, tablet at 640 px: the You ring showed Politics and "+6 more"): the most boxes the You
  // ring can have (src/http_server.cpp map links for "you": the seven kinds and Problems). A level of CAP_MIN boxes or
  // fewer is never capped, and a cap never shows fewer than CAP_MIN boxes (the last of them "+N more").
  var CAP_MAX = 24, CAP_MIN = 8, FIT_M = 40, FIT_M_PHONE = 16;
  function fitMargin() { return narrow() ? FIT_M_PHONE : FIT_M; }
  // The frame a level is laid out for. On a desktop that is the frame with the side panel open (the smaller one), even
  // while it is closed, so a level shows the same boxes with the panel open or shut (Back closes it) and fits both.
  function layoutFrame() {
    var b = svg.getBoundingClientRect(), w = b.width;
    if (!narrow() && panel.hidden) { // measured, not computed from the CSS: shown and hidden again before any paint
      panel.hidden = false; root.classList.add('lgm-has-panel');
      w = svg.getBoundingClientRect().width;
      panel.hidden = true; root.classList.remove('lgm-has-panel');
    }
    return { width: w, height: b.height };
  }
  // the extent (map units) of center c with items placed by ringPlace on ring r: the same measure as extent() below
  function layoutExtent(c, items, r) {
    var boxes = items.map(function (d, i) { var p = ringPlace(c, i, items.length, r); return { x: p.x, y: p.y, kind: d.kind, w: widthOf(d), s: sizeOf(d) }; });
    boxes.push({ x: c.x, y: c.y, kind: c.kind, w: c.w || widthOf(c), s: sizeOf(c) });
    return extentOf(boxes, { cx: c.x + W / 2, cy: c.y + H / 2, rx: r.rx, ry: r.ry });
  }
  function phoneLayout(c, items) {
    var f = floorFor(items.concat([c])), fr = layoutFrame(), smax = 0;
    items.forEach(function (d) { smax = Math.max(smax, sizeOf(d)); });
    // two columns of width pw * smax, 12 px apart, inside the frame at zoom f (Brief 5: names >= 11 px)
    var room = (fr.width - FIT_M_PHONE) / f - 12;
    phoneW = Math.max(110, Math.min(W, Math.floor(room / (2 * Math.max(1, smax)))));
    phoneDX = Math.round(phoneW * Math.max(1, smax) / 2 + 6);
  }
  // every box of a level on its ring: two columns on a phone (phoneLayout), and fitAll scales the whole ring to the frame
  function ringAll(c, items) {
    if (narrow()) phoneLayout(c, items);
    return { items: items, r: ringFit(c, items) || ringRadii(items.length) };
  }
  function capRing(c, list) {
    var fr = layoutFrame(), m = fitMargin(), total = list.length;
    // your own kinds are never capped (rule: all of them always show), nor is a small level
    if (c.kind === 'you' || (total <= CAP_MIN && !(POOLS && total > poolCap(c)))) return ringAll(c, list.slice());
    // id: more:<the level's own box>:<how many are left>, the same for every "+N more" page of that level
    var base = c.kind === 'more' ? c.id.slice(5, c.id.lastIndexOf(':')) : c.id;
    function more(rest) {
      return { id: 'more:' + base + ':' + rest.length, label: '+' + rest.length + ' more', kind: 'more', page: '',
        rel: -1, sub: '', more: rest };
    }
    // pools: as many as fit round the pool at its largest (poolCap), the rest behind "+N more"
    if (POOLS) {
      var pc = poolCap(c);
      return { items: total <= pc ? list.slice() : list.slice(0, pc - 1).concat([more(list.slice(pc - 1))]), r: null };
    }
    for (var k = Math.min(total, CAP_MAX); k >= CAP_MIN; k--) {
      var items = k >= total ? list.slice() : list.slice(0, k - 1).concat([more(list.slice(k - 1))]);
      if (narrow()) phoneLayout(c, items);
      var r = ringFit(c, items);
      if (!r) continue;
      var e = layoutExtent(c, items, r), f = floorFor(items.concat([c]));
      if (Math.min((fr.width - m) / (e.x1 - e.x0), (fr.height - m) / (e.y1 - e.y0)) >= f) return { items: items, r: r };
    }
    // nothing fits (a tiny frame): CAP_MIN boxes, the last "+N more"; this is the only case that can run off
    return ringAll(c, list.slice(0, CAP_MIN - 1).concat([more(list.slice(CAP_MIN - 1))]));
  }

  // Collapse (Brandon 2026-11-11: "if i select a higher level box, shouldnt the previous branches collapse?"): when a box
  // is selected, keep only you, your kind ring, the path from you to this box, and what this box opens onto; branches
  // off that path close. Going back up the trail closes what was open below.
  // The center (Grok Bot's mockup 1: "tap a kind, its items branch out; tap an item, it becomes the center"): you or one
  // of your kinds sits in the middle with its connections on a ring; centerPath is the trail back (You > Friends).
  var centerPath = [];
  function centerId() { return centerPath.length ? centerPath[centerPath.length - 1].id : 'you'; }

  // The filter bar and Hidden mode are gone (Brandon 2026-11-14: "i want to get rid of the hidden objects on the
  // mainstream map, that was a bad idea and its causing clutter"): nothing on the map is hidden by hand. What the map
  // shows is the walker's job (lgmap_walk.js: zoomed in, only the box you are at and its connections). #lgm-filter and
  // its note stay in the page's markup, empty and not shown (lgmap.css), until the page itself is next changed.
  // Back: history entries made by focusOn(); popstate returns to that box without pushing again.
  // Live look #5 (Brandon 2026-11-11): the first click was sometimes ignored, the second skipped a level, and Back was
  // enabled at You. Now:
  // - each entry holds its place (i: 0 is the start view) and the whole trail (path), so going back draws exactly that
  //   level with exactly that trail, and Back is disabled at i 0;
  // - the side panel's page loads with location.replace (openPanel), so it adds no history entry. A link followed
  //   INSIDE the panel still does, and history.back() then only steps that page back (the "ignored" click), so the
  //   button keeps stepping back (up to 10 times) until the map's own entry arrives;
  // - a second click while one is under way goes one more step, never zero.
  // (Brandon 2026-11-13 5:01 AM: no Back button; you go back by tapping the box you came from, the trail's step before
  // where you are, marked lgm-trail-prev. The phone's back gesture and the walker's B / Esc (LG_MAP.back) still step
  // back through these entries.)
  var restoring = false, histAt = 0, backWant = -1, backTimer = 0, backTries = 0;
  function updateBack() {}
  function backStep() {
    clearTimeout(backTimer);
    if (backWant < 0 || histAt <= backWant || ++backTries > 10) { backWant = -1; return; }
    history.back();
    backTimer = setTimeout(backStep, 350);
  }
  function goBack() {
    if (histAt <= 0) return false;
    backWant = backWant >= 0 ? Math.max(0, backWant - 1) : histAt - 1;
    backTries = 0;
    backStep();
    return true;
  }
  window.addEventListener('popstate', function (ev) {
    // each entry carries the center's own box data, so it can be drawn again even after the trail moved past it
    var st = ev.state, d = st && st.d;
    if (!st || !st.lgc || !d) return; // not one of the map's entries
    histAt = typeof st.i === 'number' ? st.i : Math.max(0, histAt - 1);
    restoring = true;
    try {
      selected = null; // a phone tap still waiting (GLIDE_WAIT) must not move the map after this
      select(focusOn(nodes[d.id] || d), 'now');
    } finally { restoring = false; }
    updateBack();
    if (backWant >= 0) { if (histAt > backWant) { backTries = 0; backStep(); } else { clearTimeout(backTimer); backWant = -1; } }
  });
  // The MAINstream view, from the MAINstream button at the top, at any level (live look #6:
  // the button only panned the map, and did nothing at deep levels). The MAINstream becomes the center: the ring clears,
  // its cities and Sitewide go round it; the trail says You > MAINstream, whatever level you came from.
  function toMainstream() {
    if (centerId() === 'root') { fitAll(narrow()); return; }
    var go = function (d) { centerPath = centerPath.slice(0, 1); select(nodes.root || d, 'now'); };
    if (nodes.root) { go(nodes.root); return; }
    status('Loading…');
    load('root').then(function (data) { go(data.node); }).catch(function (err) { status(err.message); });
  }
  // ---- one world of pools (Brandon 2026-11-15: pools and gates, "lets just get it all built now"; rings outward from
  // wherever you are, each thing once). Opening a box no longer clears the map and lays it out again around that box:
  // the box stays where it is, its connections are added around it in free space on its far side (away from the box it
  // was opened from), and a connection to a box already on the map is drawn as one more channel to it. The trail is
  // the path from you to the box. What shows near you is the map walker's (lgmap_walk.js: fading layers).
  function pathTo(n) {
    var out = [], seen = {};
    for (var c = n; c && !seen[c.id] && out.length < 40; c = c.from ? nodes[c.from] : null) { seen[c.id] = 1; out.unshift(dataOf(c)); }
    if (!out.length || out[0].id !== 'you') { if (nodes.you) out.unshift(dataOf(nodes.you)); }
    return out;
  }
  function focusOn(m) {
    var n = nodes[m.id];
    if (!n) {   // not drawn yet (a trail step, Back, the MAINstream): put it next to the box you are at
      var near = selected && nodes[selected.id] ? selected : nodes.you;
      var at = near ? worldPlace(near, [m])[0] : { x: 0, y: 0 };
      n = addNode(dataOf(m), at.x, at.y, near);
      if (near) { n.from = near.id; addEdge(near.id, n.id, 'in'); }
    }
    centerPath = pathTo(n);
    if (!restoring) {
      try { history.pushState({ lgc: n.id, d: dataOf(n), path: centerPath.slice(), i: histAt + 1 }, ''); histAt++; } catch (e) {} // err-ok: no history API: Back stays disabled
    }
    updateBack();
    return n;
  }
  // the camera to box n at the zoom it has now (no walker: the map walker moves the camera itself)
  function lookAt(n) {
    var b = svg.getBoundingClientRect(), c = centerOf(n);
    if (!b.width) return;
    tx = Math.round(b.width / 2 - c.x * scale);
    ty = Math.round(b.height / 2 - c.y * scale);
    apply(true);
  }
  // places for new boxes around box c: on its far side from the box it was opened from (all round for you), at the
  // first spot on growing circles where a box would not overlap any box already on the map (GAP apart). Map units.
  var GAP = 34;
  function halfOf(d) { if (POOLS) return { w: poolR(d), h: poolR(d) * POOL_Y }; var k = sizeOf(d); return { w: (widthOf(d) * k) / 2, h: ((d.kind === 'mine' ? H + 10 : H) * k) / 2 }; }
  function worldPlace(c, items) {
    var cc = centerOf(c), p = c.from && nodes[c.from] ? centerOf(nodes[c.from]) : null;
    var out0 = p ? Math.atan2(cc.y - p.y, cc.x - p.x) : -Math.PI / 2, all = !p;
    if (POOLS) return poolPlace(c, items, cc, out0, all);
    var taken = Object.keys(nodes).map(function (id) { var r = nodes[id].region; return { x: r.cx, y: r.cy, w: r.hw, h: r.hh }; });
    var hc = POOLS ? halfOf(c) : c.kind === 'you' ? { w: YOU_R * sizeOf(c), h: YOU_R * sizeOf(c) } : halfOf(c);
    var far = POOLS ? POOL_GAP : 120, step = POOLS ? 160 : 90, gap = POOLS ? 90 : GAP;
    var placed = [];
    items.forEach(function (d) {
      var h = halfOf(d), R = Math.max(hc.w, hc.h) + Math.max(h.w, h.h) + far, spot = null;
      for (var ring = 0; ring < 30 && !spot; ring++, R += step) {
        var steps = all ? 16 : 11, span = all ? Math.PI : Math.PI * 0.6;
        for (var j = 0; j <= steps * 2 && !spot; j++) {
          var off = (j % 2 ? 1 : -1) * Math.ceil(j / 2) * span / steps, a = out0 + off;
          var x = cc.x + R * Math.cos(a), y = cc.y + R * Math.sin(a);
          var free = taken.every(function (t) { return Math.abs(t.x - x) >= t.w + h.w + gap || Math.abs(t.y - y) >= t.h + h.h + gap; });
          if (free) spot = { x: x, y: y };
        }
      }
      if (!spot) spot = { x: cc.x + R * Math.cos(out0), y: cc.y + R * Math.sin(out0) };
      taken.push({ x: spot.x, y: spot.y, w: h.w, h: h.h });
      placed.push({ x: Math.round(spot.x - W / 2), y: Math.round(spot.y - H / 2) });
    });
    return placed;
  }
  // pools: the new pools go round pool c on its far side, each channel at least a mouth's width from the next (the
  // pool grows first if they do not fit), leaving the way back clear; each as near as it can be without touching another
  // pool (POOL_GAP of land at least, so channels stay long enough to read).
  // how far pool o's middle is from the straight channel between p and (x, y)
  function segAway(p, x, y, o) {
    var dx = x - p.x, dy = y - p.y, L = dx * dx + dy * dy, t = L ? Math.max(0, Math.min(1, ((o.x - p.x) * dx + (o.y - p.y) * dy) / L)) : 0;
    return Math.hypot(p.x + t * dx - o.x, p.y + t * dy - o.y);
  }
  function poolPlace(c, items, cc, out0, all) {
    if (!items.length) return [];
    poolFit(c, items.length + (all ? 0 : 1));
    var R0 = poolR(c), step = mouthAngle(R0) * 1.05, k = items.length;
    var span = all ? 2 * Math.PI : 2 * Math.PI - 2 * step;   // the way back keeps a mouth's width either side
    var gapA = Math.min(span / k, Math.max(step * 1.5, Math.PI / 4));   // close together, never closer than a mouth
    var start = all ? out0 : out0 - gapA * (k - 1) / 2;
    var taken = Object.keys(nodes).map(function (id) { var r = nodes[id].region; return { x: r.cx, y: r.cy, w: r.hw, h: r.hh }; });
    var LAND = 120;
    // the directions channels already leave c by (to boxes on the map linked to it): a new one keeps a mouth away
    var used = [];
    Object.keys(edges).forEach(function (k) {
      var e = edges[k], o = e.a === c.id ? nodes[e.b] : e.b === c.id ? nodes[e.a] : null;
      if (o) { var q = centerOf(o); used.push(Math.atan2(q.y - cc.y, q.x - cc.x)); }
    });
    function roomAt(a) { return used.every(function (u) { var d = Math.abs(Math.atan2(Math.sin(a - u), Math.cos(a - u))); return d >= step; }); }
    // a shore with no room left for one more mouth: the pool grows until there is (its mouths then sit closer)
    // a shore with no room left for one more mouth: the pool grows (within its limit) until there is
    function makeRoom() {
      for (var g = 0; g < 120; g++) {
        for (var q = 0; q < 72; q++) if (roomAt(q * Math.PI / 36)) return;
        if (!grow(10)) return;   // at its largest: mouths sit closer
      }
    }
    function grow(by) {
      // the pools placed in this round are not on the map yet: the pool must not grow into them either
      var R = Math.min(poolMax(c), poolR(c) + by);
      if (!taken.every(function (o) { return (o.x === cc.x && o.y === cc.y) || Math.hypot(o.x - cc.x, o.y - cc.y) >= R + Math.max(o.w, o.h) + POOL_LAND; })) return false;
      if (!setPoolR(c, R)) return false;
      R0 = c.R; step = mouthAngle(R0) * 1.05;
      return true;
    }
    return items.map(function (d, i) {
      makeRoom();
      var h = halfOf(d), a0 = start + gapA * i, x = 0, y = 0, found = false;
      // nearest first; at each distance its own direction, then turned further either way. The spot must not touch
      // another pool, its channel must not run through one, and its mouth needs room on the shore. Nothing within 8
      // steps out: the pool grows a little and it looks again; still nothing: the first spot not touching a pool.
      function look(rings, needRoom) {
        var D = Math.max(R0, R0 * POOL_Y) + h.w + POOL_GAP;
        for (var t = 0; t < rings; t++, D += 90) {
          for (var j = 0; j < 25; j++) {
            var a = a0 + (j % 2 ? 1 : -1) * Math.ceil(j / 2) * Math.PI / 12;
            x = cc.x + D * Math.cos(a); y = cc.y + D * Math.sin(a);
            if ((!needRoom || roomAt(a)) && taken.every(function (o) {
              if (o.x === cc.x && o.y === cc.y) return true;   // the pool it leaves
              if (Math.abs(o.x - x) < o.w + h.w + LAND && Math.abs(o.y - y) < o.h + h.h + LAND) return false;
              return !needRoom || segAway(cc, x, y, o) >= Math.max(o.w, o.h) + 60;
            })) return true;
          }
        }
        return false;
      }
      for (var tries = 0; tries < 12 && !found; tries++) { found = look(8, true); if (!found && !grow(20)) break; }
      if (!found) look(60, false);
      taken.push({ x: x, y: y, w: h.w, h: h.h });
      used.push(Math.atan2(y - cc.y, x - cc.x));
      return { x: Math.round(x - W / 2), y: Math.round(y - H / 2) };
    });
  }
  // a "+N more" box keeps its rest, so Back and the trail can draw it again
  function dataOf(n) { return { id: n.id, label: n.label, kind: n.kind, page: n.page, rel: n.rel, sub: n.sub, more: n.more }; }
  function expand(n) {
    if (n.expanded) return Promise.resolve(n);
    status('Loading…');
    // a "+N more" box carries the rest of its level itself (capRing); everything else asks the server
    var got = n.kind === 'more' ? Promise.resolve({ links: (n.more || []).map(function (d) { return { node: d, edge: 'more' }; }) })
                                : load(n.id);
    return got.then(function (data) {
      // the center moved on while this loaded (a quick second tap, Back): drawing it now would mix two levels
      if (nodes[n.id] !== n || n.id !== centerId()) return n;
      // one world: every link is drawn, to a new box or as one more channel to a box already on the map
      var links = data.links;
      var fresh = links.filter(function (l) { return !nodes[l.node.id]; });
      var lay = capRing(n, fresh.map(function (l) { return l.node; }));
      if (n.kind === 'you' && !n.from) {
        // your own kinds round you: the first ring keeps its even look (M1.1, Grok Bot's mockup 1)
        var rr = lay.r;
        lay.items.forEach(function (d, i) { var p = ringPlace(n, i, lay.items.length, rr); addNode(d, p.x, p.y, n).from = n.id; });
      } else {
        var spots = worldPlace(n, lay.items);
        lay.items.forEach(function (d, i) { addNode(d, spots[i].x, spots[i].y, n).from = n.id; });
      }
      links.forEach(function (l) { addEdge(n.id, l.node.id, l.edge); }); // the rest (behind "+N more") are not drawn: skipped
      lay.items.forEach(function (d) { if (d.kind === 'more') addEdge(n.id, d.id, 'more'); });
      n.expanded = true;
      n.el.classList.add('lgn-open');
      // Events is always on the You ring, even before any exist (Brandon 2026-11-11 decision 1): a friendly empty state
      if (n.id === 'mine:events' && !links.some(function (l) { return l.node.id !== 'you'; }))
        status('No events yet. When there are events for you, they will show here.');
      else status(data.links.length ? '' : 'Nothing else connects here yet.');
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
    // the centers you stepped through (You > Friends), then the boxes from the center out to n
    var tail = [], seen = {}, cid = centerId();
    for (var c = n; c && c.id !== cid && !seen[c.id] && tail.length < 12; c = c.from ? nodes[c.from] : null) { seen[c.id] = 1; tail.unshift(c); }
    var chain = centerPath.concat(tail);
    trailEl.textContent = '';
    chain.forEach(function (c, i) {
      if (i) { var sep = document.createElement('span'); sep.className = 'lgm-trail-sep'; sep.textContent = '›'; trailEl.appendChild(sep); }
      var b = document.createElement('button');
      b.type = 'button';
      b.className = 'lgm-trail-step' + (i === chain.length - 1 ? ' here' : i === chain.length - 2 ? ' lgm-trail-prev' : '');
      b.textContent = c.kind === 'you' ? 'You' : short(c.label, 22);
      if (i === chain.length - 1) b.setAttribute('aria-current', 'location');
      else {
        // the box you came from is Back (Brandon 5:01 AM: no Back button; marked lgm-trail-prev in lgmap.css)
        // Back moves straight away like the old Back button ('now': no phone double-tap wait, it is not a box tap)
        var back = i === chain.length - 2;
        if (back) b.setAttribute('aria-label', 'Back to ' + b.textContent);
        b.addEventListener('click', function () { select(nodes[c.id] || c, back ? 'now' : false); });
      }
      trailEl.appendChild(b);
    });
    trailEl.scrollLeft = trailEl.scrollWidth; // a phone's one-line trail shows where you are (lgmap.css)
  }
  function select(n, open) {
    // 'now': move without the phone's double-tap wait and without opening a page (the wait's own second call, Back,
    // Zoom out). Normalised here so it can never reach openPanel below.
    var now = open === 'now';
    if (now) open = false;
    // the tapped box becomes the center, with its connections on a ring (Brandon 2026-11-11; live look #2: a child box
    // only extended the trail and its connections grew beside the old view)
    if (n.id !== centerId()) {
      // phone, a tap that did not open: wait the double-tap window first, so a second tap still lands on the box and
      // opens its page instead of hitting the spot it moved away from (CI phone_check 4c, 2026-11-11)
      if (narrow() && !open && !now && n.el) {
        if (selected && selected.el) selected.el.classList.remove('lgn-sel');
        selected = n;
        n.el.classList.add('lgn-sel');
        setTimeout(function () { if (selected === n && centerId() !== n.id) select(n, 'now'); }, GLIDE_WAIT_MS);
        return;
      }
      n = focusOn(n);
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
    shade(n);
    trail(n);
    for (var k in edges) if (edges[k].a === n.id || edges[k].b === n.id) sizeEdge(edges[k]);
    light(n.id);
    if (n.page && open) openPage(n);
    // After the field opens (the map narrows), glide to the box and what grew out of it.
    // Phone, a tap that did not open: wait before gliding, so a second tap (double-tap to open) lands on the same box
    // instead of wherever the glide moved it.
    var wait = narrow() && !open ? GLIDE_WAIT_MS : 0;
    expand(n).then(function () {
      light(n.id);
      // one world (2026-11-15): the camera goes to the box (the map walker carries your drop there and follows it);
      // nothing else moves
      setTimeout(function () {
        if (selected !== n) return;
        var M = window.LG_MAP;
        if (M && typeof M.onFocus === 'function' && M.onFocus(n.id)) return;
        lookAt(n);
      }, wait);
    });
  }

  // a box's page opens as the page itself (through the walker's portal when it is on the map), not in a side panel
  function openPage(n) {
    var M = window.LG_MAP;
    if (M && typeof M.onOpen === 'function' && M.onOpen(n.id, n.page)) return;
    location.assign(n.page);
  }
  function closePanel() {
    panel.hidden = true;
    editing = false;
    applyView();
    panel.classList.remove('full');
    root.classList.remove('lgm-has-panel');
    document.documentElement.classList.remove('lgm-sheet-open');
    if (selected) fitAll(narrow());   // the map gets its width back: fit the ring to it again
  }
  // A link inside the framed page that goes to the map would open a map inside the map (advisor catch,
  // 2026-11-07): take it at the top level instead, focusing the box when it is on the map already.
  frame.addEventListener('load', function () {
    applyView();
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
    // MAINstream: shrink the field back to the side and open the MAINstream view
    panel.classList.remove('full');
    document.getElementById('lgm-expand').textContent = 'Expand';
    toMainstream();
  });

  // Pan by dragging the empty map; zoom with the wheel or a pinch (around the pointer / the fingers).
  var drag = null;
  function zoomTo(next, px, py) {   // px, py: the frame point that stays put
    next = Math.min(ZOOM.max, Math.max(ZOOM.min, floorScale(), next));
    tx = px - (px - tx) * next / scale;
    ty = py - (py - ty) * next / scale;
    scale = next;
  }
  // Walking is always on now (no Drop/Scroll switch, Brandon 2026-11-14), and zooming is yours everywhere: a pinch on
  // the map, the wheel, a trackpad pinch. (It used to be Scroll mode only.)
  function scrollMode() { return true; }
  // Pinch (Scroll mode, touch): two fingers on the map zoom it around their middle and pan with it; one finger works
  // as before (drag the empty map, tap or hold a box).
  var fingers = {}, pinch = null;
  function two() { var k = Object.keys(fingers); return k.length === 2 ? [fingers[k[0]], fingers[k[1]]] : null; }
  function spread(f) { return Math.max(1, Math.hypot(f[0].x - f[1].x, f[0].y - f[1].y)); }
  svg.addEventListener('pointerdown', function (ev) {
    if (ev.pointerType !== 'touch') return;
    fingers[ev.pointerId] = { x: ev.clientX, y: ev.clientY };
    var f = two();
    if (!f || !scrollMode()) return;
    endHold(null);                              // a finger on a box was a pinch, not a tap or a hold
    suppressClickUntil = Date.now() + 600;
    drag = null;
    svg.classList.remove('lgm-drag');
    var b = svg.getBoundingClientRect();
    pinch = { d: spread(f), s: scale, mx: (f[0].x + f[1].x) / 2 - b.left, my: (f[0].y + f[1].y) / 2 - b.top, tx: tx, ty: ty };
  }, true);
  svg.addEventListener('pointermove', function (ev) {
    if (!fingers[ev.pointerId]) return;
    fingers[ev.pointerId] = { x: ev.clientX, y: ev.clientY };
    var f = two();
    if (!pinch || !f) return;
    var b = svg.getBoundingClientRect(), mx = (f[0].x + f[1].x) / 2 - b.left, my = (f[0].y + f[1].y) / 2 - b.top;
    tx = pinch.tx; ty = pinch.ty; scale = pinch.s;
    zoomTo(pinch.s * spread(f) / pinch.d, pinch.mx, pinch.my);
    tx += mx - pinch.mx; ty += my - pinch.my;   // and the map follows the fingers' middle
    apply();
  }, true);
  function lift(ev) {
    delete fingers[ev.pointerId];
    if (pinch && !two()) { pinch = null; keepInside(svg.getBoundingClientRect()); apply(); }
  }
  svg.addEventListener('pointerup', lift, true);
  svg.addEventListener('pointercancel', lift, true);
  svg.addEventListener('pointerdown', function (ev) {
    if (pinch) return;
    if (ev.target.closest && ev.target.closest('.lgn')) return;
    drag = { x: ev.clientX, y: ev.clientY, tx: tx, ty: ty };
    svg.setPointerCapture(ev.pointerId);
    svg.classList.add('lgm-drag');
  });
  svg.addEventListener('pointermove', function (ev) {
    if (!drag || pinch) return;
    tx = drag.tx + ev.clientX - drag.x;
    ty = drag.ty + ev.clientY - drag.y;
    apply();
  });
  function endDrag() { drag = null; svg.classList.remove('lgm-drag'); }
  svg.addEventListener('pointerup', endDrag);
  svg.addEventListener('pointercancel', endDrag);
  // The wheel (Brandon 2026-11-13 5:01 AM, replacing live look #9's "a plain wheel scrolls the page"): in Scroll mode
  // the wheel over the map zooms it toward the pointer and the page does not scroll; in Drop mode only ctrl+wheel (a
  // trackpad pinch, ctrl+scroll) does, as before. After a zoom, a map that fits the frame is kept inside it.
  svg.addEventListener('wheel', function (ev) {
    if (!scrollMode() && !ev.ctrlKey && !ev.metaKey) return;
    ev.preventDefault();
    if (!ev.deltaY) return;
    var box = svg.getBoundingClientRect();
    zoomTo(scale * (ev.deltaY < 0 ? ZOOM.wheel : 1 / ZOOM.wheel), ev.clientX - box.left, ev.clientY - box.top);
    keepInside(box);
    apply();
  }, { passive: false });
  // each way the map fits the frame at this scale, slide it back inside (8 px margin); a way it doesn't fit is left alone
  function keepInside(box) {
    var e = extent();
    if (!e) return;
    var l = tx + e.x0 * scale, r = tx + e.x1 * scale, t = ty + e.y0 * scale, b = ty + e.y1 * scale;
    if (r - l <= box.width - 16) tx += Math.max(0, 8 - l) - Math.max(0, r - (box.width - 8));
    if (b - t <= box.height - 16) ty += Math.max(0, 8 - t) - Math.max(0, b - (box.height - 8));
  }

  // Start: the MAINstream and its Currents. ?focus=<id> also opens that box.
  var box = svg.getBoundingClientRect();
  ty = box.height / 2 - H / 2;
  apply();
  // Fit every box on the map into the frame, centered (live look #3 and #8: boxes ran off the left edge with the side
  // panel open, and the ring sat in the top half with the bottom empty). Not over 1x; not under the readable floor
  // (floorScale: every name >= 11 px; it was 0.4x on a desktop, 0.6x on a phone). capRing sizes each ring to fit at it.
  // top: place the boxes near the top of the frame instead (phones: the Juketube tip that first-time visitors see
  // covered the you box when centered, phone_check 4/4b + #jbhint probe 2026-11-11).
  // the map's own extent: every box at its real size and the dotted ring, in map units
  function extent() {
    var ids = Object.keys(nodes);
    if (!ids.length) return null;
    if (POOLS) {   // pools: their own ovals
      var e = { x0: Infinity, y0: Infinity, x1: -Infinity, y1: -Infinity };
      ids.forEach(function (id) { var r = nodes[id].region; e.x0 = Math.min(e.x0, r.cx - r.hw); e.x1 = Math.max(e.x1, r.cx + r.hw); e.y0 = Math.min(e.y0, r.cy - r.hh); e.y1 = Math.max(e.y1, r.cy + r.hh); });
      return e;
    }
    var ring = document.getElementById('lgm-ring'); // the whole dotted ring stays in view too (a 1-item ring zoomed in
    return extentOf(ids.map(function (id) {          // and cut it off, CI screenshot 2026-11-11)
      var n = nodes[id]; return { x: n.x, y: n.y, kind: n.kind, w: n.w || W, s: sizeOf(n) };
    }), ring && { cx: +ring.getAttribute('cx'), cy: +ring.getAttribute('cy'), rx: +ring.getAttribute('rx'), ry: +ring.getAttribute('ry') });
  }
  // boxes: {x, y, kind, w, s} (a box's place, its width and size); ring: the dotted ellipse or null. capRing measures a
  // level with this before drawing it.
  function extentOf(boxes, ring) {
    var x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
    boxes.forEach(function (n) {
      // each box's real height at its size: the you circle reaches well above and below its row, and "Open your page" under it
      var s = n.s, c = n.y + H / 2, half = n.w / 2 * s;
      var up = n.kind === 'you' ? YOU_R * s : (n.kind === 'mine' ? H + 10 : H) / 2 * s, down = n.kind === 'you' ? YOU_R + 34 : up;
      x0 = Math.min(x0, n.x + W / 2 - half); x1 = Math.max(x1, n.x + W / 2 + half); y0 = Math.min(y0, c - up); y1 = Math.max(y1, c + down);
    });
    if (ring) {
      x0 = Math.min(x0, ring.cx - ring.rx); x1 = Math.max(x1, ring.cx + ring.rx); y0 = Math.min(y0, ring.cy - ring.ry); y1 = Math.max(y1, ring.cy + ring.ry);
    }
    return { x0: x0, y0: y0, x1: x1, y1: y1 };
  }
  // the scale at which the whole map fits the frame (fitAll's; the map walker's zoom is a multiple of it)
  function fitScale() {
    var b = svg.getBoundingClientRect(), e = extent();
    if (!b.width || !e) return scale;
    var m = fitMargin();
    return Math.max(floorScale(), Math.min(1, (b.width - m) / (e.x1 - e.x0), (b.height - m) / (e.y1 - e.y0)));
  }
  function fitAll(top) {
    var b = svg.getBoundingClientRect(), e = extent();
    if (!b.width || !e) return;
    var x0 = e.x0, y0 = e.y0, x1 = e.x1, y1 = e.y1;
    var m = fitMargin();
    scale = fitScale();
    tx = Math.round((b.width - (x1 - x0) * scale) / 2 - x0 * scale);
    ty = top ? Math.round(m / 2 - y0 * scale) : Math.round((b.height - (y1 - y0) * scale) / 2 - y0 * scale);
    apply(false); // placed, not glided in
  }
  // the map's width changed (window resized, side panel): fit again. Height alone is a phone's address bar coming and
  // going while the page scrolls; that must not move the map.
  var fitW = 0, fitTimer = 0;
  window.addEventListener('resize', function () {
    clearTimeout(fitTimer);
    fitTimer = setTimeout(function () {
      var w = svg.getBoundingClientRect().width;
      if (w && Math.abs(w - fitW) > 1) { fitW = w; fitAll(narrow()); }
    }, 150);
  });
  // The map walker's camera (assets/lgmap_walk.js; Brandon 2026-11-13, "when you Drop in on the map, it should look
  // bigger"): focus(id, zoom) puts box id in the middle of the frame at zoom times the fitted scale (never past the
  // map's ZOOM.max), gliding; fit() is the normal view again. The zoom is walker.js's (LG_WALKER.CONTROLS.mapZoom).
  window.LG_MAP = {
    // spatial step 8: a box is an instance; this is its record (region and valve, as built), or null if it is not on the map
    instance: function (id) { var n = nodes[id]; return n ? { id: n.id, region: n.region, valve: n.valve, css: n.css, page: n.page || '' } : null; },
    // The portal (2026-11-15, Brandon: the side panel is abandoned; a page is entered through its portal): opening a
    // box's page calls onOpen(id, page) when the map walker set it (it grows the peek into the page); with no walker
    // (the droplet hidden in Settings) the page simply opens.
    onOpen: null,
    // the box at the middle of the map now (the trail's last step): the map walker opens a pool you swim into
    center: function () { return centerId(); },
    focus: function (id, zoom) {
      var n = nodes[id], b = svg.getBoundingClientRect();
      if (!n || !b.width) return false;
      fitAll(narrow());
      var c = centerOf(n);
      scale = Math.min(ZOOM.max, scale * zoom);
      tx = Math.round(b.width / 2 - c.x * scale);
      ty = Math.round(b.height / 2 - c.y * scale);
      apply(true);
      return true;
    },
    fit: function () { fitAll(narrow()); },
    back: goBack,       // one step back, as the phone's back gesture (the walker's B / Esc); false at the start
    ZOOM: ZOOM,
    scale: function () { return scale; },
    // The free-walking map walker (2026-11-14) works in map units and moves the camera itself:
    // view(): where the camera is going (map point p is at frame point tx + p.x * scale); setView puts it there
    // (glide: eased); fitScale(): the scale that shows the whole map; frame(): the map's frame on the screen;
    // toScreen(x, y): where a map point is on the screen right now, mid-glide included; extent(): the map's bounds.
    pools: POOLS ? { R: poolR, Y: POOL_Y, label: poolLabel } : null,   // the map walker's water (lgmap_walk.js)
    view: function () { return { tx: tx, ty: ty, scale: scale }; },
    setView: function (x, y, k, glide) {
      scale = Math.min(ZOOM.max, Math.max(ZOOM.min, k));
      tx = x; ty = y;
      apply(!!glide);
    },
    fitScale: fitScale,
    frame: function () { return svg.getBoundingClientRect(); },
    toScreen: function (x, y) {
      var b = svg.getBoundingClientRect(), t = getComputedStyle(view).transform, m = t && t !== 'none' ? new DOMMatrix(t) : null;
      return m ? { x: b.left + m.e + x * m.a, y: b.top + m.f + y * m.d } : { x: b.left + tx + x * scale, y: b.top + ty + y * scale };
    },
    extent: extent
  };
  // M1.1 (Brandon 2026-11-10): the map starts at you, with your kinds of connection (Friends, Groups, Places,
  // Politics, Problems) as the first ring. The whole site (MAINstream) is under Places, and its button still works.
  // Show box id: select it if it is drawn, else load it and its links to drawn boxes first. open: as select's.
  // ?focus=<id> uses it at the start, and the map walker's portal arrival (LG_MAP.goTo) uses it after.
  function focusBox(id, open) {
    if (nodes[id]) { select(nodes[id], open); return Promise.resolve(nodes[id]); }
    return load(id).then(function (data) {
      var n = focusOn(data.node);   // one world: placed next to the box you are at, never on top of another
      data.links.forEach(function (l) { if (nodes[l.node.id]) addEdge(n.id, l.node.id, l.edge); });
      select(n, open);
      return n;
    });
  }
  window.LG_MAP.goTo = function (id) { return focusBox(id, 'now'); };   // moves there without opening its page
  // Search (Brandon 2026-11-15: "a search bar at the top where you can search for stuff"): as you type (two letters),
  // GET /map/search lists what your map may show; picking one (tap, or Enter for the first) takes you there: it is put
  // next to where you are and your drop goes to it (LG_MAP.goTo). Esc or an empty box closes the list.
  (function () {
    var form = document.getElementById('lgm-search'), q = document.getElementById('lgm-q'), list = document.getElementById('lgm-hits');
    if (!form || !q || !list) return;
    var KIND = { person: 'Person', group: 'Group', city: 'Place', prop: 'Proposal', post: 'Post', dept: 'Topic', comment: 'Comment', message: 'Message' };
    // when something was written, short: "3:05 PM" today, "Nov 12" this year, "Nov 12, 2025" before
    function when(ts) {
      if (!ts) return '';
      var d = new Date(ts * 1000), now = new Date();
      if (d.toDateString() === now.toDateString()) return d.toLocaleTimeString([], { hour: 'numeric', minute: '2-digit' });
      return d.toLocaleDateString([], d.getFullYear() === now.getFullYear() ? { month: 'short', day: 'numeric' } : { month: 'short', day: 'numeric', year: 'numeric' });
    }
    var timer = 0, asked = '', hits = [];
    function close() { list.hidden = true; list.textContent = ''; hits = []; }
    // a message has no place on the map: its conversation opens; anything else is swum to
    function go(d) {
      close(); q.value = ''; q.blur();
      if (!d.id) { if (d.page) location.assign(d.page); return; }
      LG_MAP.goTo(d.id).catch(function () { status('That is not on your map.'); });
    }
    function show(results) {
      list.textContent = '';
      hits = results;
      if (!results.length) {
        var none = document.createElement('li'); none.className = 'lgm-hit-none'; none.textContent = 'Nothing found';
        list.appendChild(none);
      }
      results.forEach(function (d) {
        var li = document.createElement('li'), b = document.createElement('button'), k = document.createElement('span');
        li.setAttribute('role', 'option');
        b.type = 'button'; b.className = 'lgm-hit'; b.setAttribute('data-id', d.id);
        var main = document.createElement('span'), t = document.createElement('span');
        main.className = 'lgm-hit-main';
        t.className = 'lgm-hit-text'; t.textContent = short(d.label, 90);
        main.appendChild(t);
        if (d.who || d.ts) {
          var by = document.createElement('span');
          by.className = 'lgm-hit-by';
          by.textContent = [d.who, when(d.ts)].filter(Boolean).join(' \u00b7 ');
          main.appendChild(by);
        }
        k.className = 'lgm-hit-kind'; k.textContent = KIND[d.kind] || '';
        b.appendChild(main); b.appendChild(k);
        b.addEventListener('click', function () { go(d); });
        li.appendChild(b); list.appendChild(li);
      });
      list.hidden = false;
    }
    function ask() {
      var term = q.value.trim();
      if (term.length < 2) { asked = ''; close(); return; }
      if (term === asked) return;
      asked = term;
      fetch('/map/search?q=' + encodeURIComponent(term) + '&tz=' + (-new Date().getTimezoneOffset()), { credentials: 'same-origin' })
        .then(function (r) { return r.json(); })
        .then(function (data) { if (asked === term && q.value.trim() === term) show(data.results || []); })
        .catch(function () { /* err-ok: a failed search leaves the list as it was */ });
    }
    q.addEventListener('input', function () { clearTimeout(timer); timer = setTimeout(ask, 180); });
    q.addEventListener('keydown', function (e) {
      if (e.key === 'Escape') { close(); q.blur(); }
      e.stopPropagation();   // typing never walks your drop
    });
    form.addEventListener('submit', function (e) { e.preventDefault(); if (hits.length) go(hits[0]); else ask(); });
    document.addEventListener('pointerdown', function (e) { if (!form.contains(e.target)) close(); });
  })();
  window.LG_MAP.ready = load('you').then(function (data) {
    var r = addNode(data.node, 0, 0);
    centerPath = [dataOf(r)];
    // the first view is the history entry Back returns to (map SPEC phase 1)
    try { history.replaceState({ lgc: r.id, d: dataOf(r), path: centerPath.slice(), i: 0 }, ''); } catch (e) {} // err-ok: no history API: Back stays disabled
    selected = null;
    trail(r); // "You" until you step out
    return expand(r);
  }).then(function () {
    var focus = root.getAttribute('data-focus');
    if (!focus) { fitAll(narrow()); return; } // the whole ring in view near the top, phone included (nothing cut off or covered)
    return focusBox(focus, !narrow()).then(function () {});
  });
  window.LG_MAP.ready.catch(function (err) { status(err.message); });
})();
