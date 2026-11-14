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
  // spatial step 3: the instance the walker is in. Standing on a box (always inside its region: the walker only
  // stands on boxes) it is that box's id; at the you box, or in Scroll mode, it is the parent, mainstream.
  var MAINSTREAM = 'mainstream', instanceId = MAINSTREAM;
  var me = root.getAttribute('data-me') || '';
  // spatial step 10: an admin's CSS for the instance you are Inside, as text in one <style> (never markup), nested under
  // #lgm-panel so it styles only what is inside the side panel, with the panel itself held in a paint-contained box.
  // The server only delivers CSS that passed sanitize_author_css. Gone again when you leave the instance.
  var cssFor = null;
  function scopeCss() {
    if (cssFor === instanceId) return;
    cssFor = instanceId;
    var rec = instanceId !== MAINSTREAM && window.LG_MAP && LG_MAP.instance ? LG_MAP.instance(instanceId) : null;
    var panel = document.getElementById('lgm-panel'), st = document.getElementById('lgm-instance-css');
    if (!rec || !rec.css || !panel) { if (st) st.parentNode.removeChild(st); if (panel) panel.removeAttribute('data-instance'); return; }
    if (!st) { st = document.createElement('style'); st.id = 'lgm-instance-css'; document.head.appendChild(st); }
    st.textContent = '#lgm-panel *{' + rec.css + '}#lgm-panel[data-instance]{contain:paint;isolation:isolate}';
    panel.setAttribute('data-instance', instanceId);
  }
  function instance() {
    instanceId = dropping && at && at !== 'you' && box(at) ? at : MAINSTREAM;
    walker.setAttribute('data-instance', instanceId);
    scopeCss();
  }
  // spatial step 4: while Drop is on, only the box you stand on (Inside) and the boxes connected to it (Near) are
  // shown; the rest (Far) and their connectors are hidden. Scroll mode shows everything again.
  function proximity() {
    var keep = null;
    if (dropping && box(at)) { keep = neighbors(at); keep.push(at); }
    Array.prototype.forEach.call(nodeLayer.querySelectorAll('.lgn'), function (o) {
      o.style.visibility = !keep || keep.indexOf(o.getAttribute('data-id')) >= 0 ? '' : 'hidden';
    });
    Array.prototype.forEach.call(edgeLayer.querySelectorAll('.lge'), function (e) {
      e.style.visibility = !keep || (keep.indexOf(e.getAttribute('data-a')) >= 0 && keep.indexOf(e.getAttribute('data-b')) >= 0) ? '' : 'hidden';
    });
  }
  // Presence (2026-11-14): while Drop is on, the walker is in the presence server's map room (/here/ws?page=/map, the
  // socket and message shape of the dots on profile pages, here.js): every position update sends {x, y, instanceId}.
  // The server's snapshot then holds only the walkers in the same instance (spatial step 5); who that is right now
  // is kept on #lgm-walker[data-here] (handles, comma separated) and drawn as small round portraits around the walker
  // (hereDraw). Scroll mode closes the socket and removes them.
  // x, y: where the walker is on the screen, as fractions of it (the map has no page coordinates of its own).
  var hws = null, hsent = '';
  // The other droplets in your instance: one small circle each (#lgm-here .lgm-here-dot[data-u], lgmap.css), on a ring
  // around the walker, in handle order so they keep their places. Their portrait, else their first letter. Not
  // tappable. At most HERE_MAX are drawn. Gone when they leave the snapshot (another instance, Scroll, left the map).
  var hereLayer = null, hereDots = {}, HERE_MAX = 12, HERE_R = 30;
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
    var cx = parseFloat(walker.style.left) || 0, cy = (parseFloat(walker.style.top) || 0) - 15;   // the walker's own circle sits just above the box
    names.forEach(function (u, i) {
      var a = -Math.PI / 2 + (i + 1) * 2 * Math.PI / (n + 1);
      hereDots[u].style.left = Math.round(cx + HERE_R * Math.cos(a)) + 'px';
      hereDots[u].style.top = Math.round(cy + HERE_R * Math.sin(a)) + 'px';
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
    if (!hws || hws.readyState !== 1 || !box(at)) return;
    var p = middle(box(at));
    var msg = JSON.stringify({ x: unit(p.x / (window.innerWidth || 1)), y: unit(p.y / (window.innerHeight || 1)), instanceId: instanceId });
    if (msg === hsent) return;
    hsent = msg;
    hws.send(msg);
  }
  function presenceOn() {
    if (hws || !me || !window.WebSocket) return;
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
        hereDraw(d.people.map(function (q) { return String(q.u); }).filter(function (u) { return u && u !== me; }));
      } catch (e) { /* a bad frame is skipped */ }
    };
    s.onclose = function () { if (s === hws) { hws = null; hereDraw([]); } };   // refused (chose No) or dropped: not retried until the next Drop
  }
  function presenceOff() {
    var s = hws;
    hws = null;
    hereDraw([]);
    if (s) s.close();
  }
  function show() {
    instance();
    proximity();
    presence();
    var el = box(at);
    Array.prototype.forEach.call(nodeLayer.querySelectorAll('.lgn.lgn-walk'), function (o) { if (o !== el) o.classList.remove('lgn-walk'); });
    if (!el) { walker.hidden = true; herePlace(); return; }
    el.classList.add('lgn-walk');
    var r = el.getBoundingClientRect();
    walker.hidden = false;
    walker.style.left = Math.round(r.left + r.width / 2) + 'px';
    walker.style.top = Math.round(r.top) + 'px';
    walker.setAttribute('data-at', at);
    herePlace();
  }
  // the camera on the box you stand on; again a little later after the map moves by itself (a box opened, Back: the
  // map recenters and fits itself once its connections load)
  function camera() { if (dropping && window.LG_MAP && box(at)) LG_MAP.focus(at, ZOOM); }
  function cameraSoon() {
    later.forEach(clearTimeout);
    later = [350, 900, 1700].map(function (ms) { return setTimeout(camera, ms); });
  }
  // spatial steps 6-7: the entry path. A box is entered only through its valve (data-valve, -owner, -invite on the box):
  //   open: anyone; closed: no one; private: only its owner; invite: only people on its list;
  //   request: turned away at the door, and a request is recorded for the owner (data-requested-by, and an
  //   'lg-valve-request' event); conduit: passed through to the next box in the same direction, never stood on.
  // Returns 'in', 'through' or 'no'.
  function entry(id, record) {
    var el = box(id);
    if (!el) return 'no';
    var type = el.getAttribute('data-valve') || 'open';
    if (type === 'open') return 'in';
    if (type === 'conduit') return 'through';
    if (type === 'private') return me && el.getAttribute('data-valve-owner') === me ? 'in' : 'no';
    if (type === 'invite') return me && (el.getAttribute('data-valve-invite') || '').split(',').indexOf(me) >= 0 ? 'in' : 'no';
    if (type === 'request' && record) {
      el.setAttribute('data-requested-by', me);
      document.dispatchEvent(new CustomEvent('lg-valve-request', { detail: { box: id, by: me } }));
    }
    return 'no';   // closed, request, and any type this file does not know
  }
  // where a push in direction w ends: the box it picks if that is enterable; through a conduit on to the next box
  function target(w) {
    var from = at, next = pick(w), guard = 0;
    while (next && entry(next, false) === 'through' && guard++ < 8) {
      at = next;
      next = pick(w);
      if (next === from) next = null;
    }
    at = from;
    return next && entry(next, true) === 'in' ? next : null;
  }
  function step(w, speed) {
    if (!box(at)) at = startBox();
    var next = target(w);
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
  walker.setAttribute('data-instance', 'mainstream');
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
    // the switch sits in the map's own row, after the trail, so the map keeps its place on the page
    dropIn: document.querySelector('.lgm-navrow'),
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
    // B / Esc: close the open page, else one step back (LG_MAP.back, as the phone's back gesture)
    back: function () {
      var panel = document.getElementById('lgm-panel'), close = document.getElementById('lgm-close');
      if (panel && !panel.hidden && close) close.click();
      else if (window.LG_MAP && LG_MAP.back) LG_MAP.back();
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
        presenceOn();
        show();
        camera();
        follow = setInterval(function () { if (!box(at)) at = startBox(); show(); }, 120);   // the map glides and recenters
      } else {
        walker.hidden = true;
        if (window.LG_MAP) LG_MAP.fit();
        Array.prototype.forEach.call(nodeLayer.querySelectorAll('.lgn.lgn-walk'), function (o) { o.classList.remove('lgn-walk'); });
        instance();
        proximity();
        presenceOff();
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
