/* Dots on profile pages (Brandon 2026-11-12): everyone on this profile page who chose Yes shows as a small round
   portrait with their handle, moving around the page. Settings come from window.LG_HERE = {me, page}, set just before
   this file loads (only for someone who chose Yes). Positions are fractions of the page (0..1) so a phone and a
   desktop see roughly the same place. Tapping, clicking and scrolling work as always; the touchpad works like a
   joystick (WASD or the arrow keys on a keyboard): walk your dot up to a button or field and press A (or Enter) to use it, B (or Esc) to go back.
   Walking is always on (no Drop/Scroll switch, Brandon 2026-11-14). Every move puts your dot in the middle of the screen
   (as far as the page's ends allow). Tap or click your own dot for its menu: microphone, camera, drip speed.
   The pad, the drop menu and the keys are walker.js's (window.LG_WALKER, loaded just before this file); this file
   walks the dot. */
(function () {
  var cfg = window.LG_HERE;
  if (!cfg || !cfg.me || !cfg.page || !window.WebSocket || !window.LG_WALKER) return;
  var W = LG_WALKER, MAX_SPEED = W.CONTROLS.maxSpeed, SEND_MS = 110;   // px per 40 ms tick at full speed

  var layer = document.createElement('div');
  layer.id = 'lg-here';
  layer.setAttribute('aria-hidden', 'true');
  document.body.appendChild(layer);

  var dots = {}, me = { x: 0.5, y: 0.08, known: false }, held = {}, pull = { x: 0, y: 0 }, lastSent = 0, dirty = false;
  var near = null, ws = null, tries = 0;
  var peopleFns = [], sigFns = [];
  // spatial step 11: what assets/here_media.js needs from this file (proximity voice and video): who is here, a way to
  // send, and the setup messages other people's browsers send through the server.
  window.LG_HERE_API = {
    send: function (o) { if (!ws || ws.readyState !== 1) return false; ws.send(JSON.stringify(o)); return true; },
    moveTo: function (x, y) {
      me.x = Math.max(0, Math.min(1, x)); me.y = Math.max(0, Math.min(1, y)); me.known = true; dirty = true;
      if (dots[cfg.me]) place(dots[cfg.me], me.x, me.y);
    },
    onPeople: function (fn) { peopleFns.push(fn); },
    onSignal: function (fn) { sigFns.push(fn); }
  };

  function pageW() { return Math.max(document.documentElement.scrollWidth, document.body.scrollWidth); }
  function pageH() { return Math.max(document.documentElement.scrollHeight, document.body.scrollHeight); }
  function sizeLayer() { layer.style.height = pageH() + 'px'; }

  function makeDot(u) {
    var d = document.createElement('div');
    d.className = 'lg-here-dot' + (u === cfg.me ? ' me' : '');
    var img = document.createElement('img');
    img.alt = '';
    img.src = '/portrait/' + encodeURIComponent(u);
    img.onerror = function () {
      var s = document.createElement('span');
      s.className = 'lg-here-ph';
      s.textContent = u.charAt(0).toUpperCase();
      if (img.parentNode) img.parentNode.replaceChild(s, img);
    };
    var label = document.createElement('span');
    label.className = 'lg-here-name';
    label.textContent = u;
    d.appendChild(u === cfg.me ? LG_WALKER.portrait(u) : img);
    if (u === cfg.me) LG_WALKER.menu(d.firstChild);   // tap or click your drop: microphone, camera, drip speed
    d.appendChild(label);
    d.setAttribute('data-u', u);
    layer.appendChild(d);
    return d;
  }

  function place(el, x, y) {
    el.style.left = (x * pageW()) + 'px';
    el.style.top = (y * pageH()) + 'px';
  }

  function render(people) {
    var seen = {};
    people.forEach(function (p) {
      seen[p.u] = true;
      var el = dots[p.u] || (dots[p.u] = makeDot(p.u));
      if (p.u === cfg.me) {
        if (!me.known) { me.x = p.x; me.y = p.y; me.known = true; }
        place(el, me.x, me.y);
      } else {
        place(el, p.x, p.y);
      }
    });
    Object.keys(dots).forEach(function (u) {
      if (!seen[u] && u !== cfg.me) { dots[u].remove(); delete dots[u]; }
    });
  }

  function connect() {
    var proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
    ws = new WebSocket(proto + '//' + location.host + '/here/ws?page=' + encodeURIComponent(cfg.page));
    ws.onopen = function () { tries = 0; dirty = true; };
    ws.onmessage = function (ev) {
      try {
        var d = JSON.parse(ev.data);
        if (d.sig) sigFns.forEach(function (fn) { fn(d.sig); });
        else { render(d.people || []); peopleFns.forEach(function (fn) { fn(d.people || []); }); }
      } catch (e) { /* a bad frame is skipped */ }
    };
    ws.onclose = function () {
      Object.keys(dots).forEach(function (u) { if (u !== cfg.me) { dots[u].remove(); delete dots[u]; } });
      if (!gone && ++tries <= 5) setTimeout(connect, 1000 * tries);
    };
  }
  // Leaving the page (2026-11-15): a browser can keep the page you left in memory (its back/forward cache) with this
  // socket still open, and then the others on the page kept seeing your dot after you had gone. So the socket is closed
  // on the way out (not retried), and opened again if that kept page is shown again (Back).
  var gone = false;
  window.addEventListener('pagehide', function () {
    gone = true;
    try { if (ws) ws.close(); } catch (e) { /* err-ok: already closing */ }
  });
  window.addEventListener('pageshow', function (e) { if (e.persisted && gone) { gone = false; tries = 0; connect(); } });

  // ---- walking
  // what you stand next to, and using it: walker.js's (the home page walks the same way)
  function findNear() { near = W.nearest(me.x * pageW() - window.scrollX, me.y * pageH() - window.scrollY, near); }
  function press() { if (near) W.use(near); }
  function step() {
    var v = W.velocity(held, pull), dx = v.x, dy = v.y;
    if (dots[cfg.me]) LG_WALKER.shape(dots[cfg.me].querySelector('.' + LG_WALKER.CONTROLS.shape.cls), dx, dy);
    if (dx || dy) {
      me.x = Math.max(0, Math.min(1, me.x + dx * MAX_SPEED / pageW()));
      me.y = Math.max(0, Math.min(1, me.y + dy * MAX_SPEED / pageH()));
      me.known = true;
      dirty = true;
      if (dots[cfg.me]) place(dots[cfg.me], me.x, me.y);
      W.centerOn(me.x * pageW(), me.y * pageH());   // the screen always keeps your drop in its middle
      findNear();
    }
    var now = Date.now();
    if (dirty && ws && ws.readyState === 1 && now - lastSent >= SEND_MS) {
      ws.send(JSON.stringify({ x: me.x, y: me.y }));
      lastSent = now;
      dirty = false;
    }
  }
  setInterval(step, 40);

  // ---- the touchpad, A and B (walker.js)
  LG_WALKER.mount({
    dir: function (dir, down) { held[dir] = down; },
    pull: function (x, y) { pull = { x: x, y: y }; },
    select: function () { if (!near) return false; press(); return true; },
    back: function () { if (history.length > 1) history.back(); },
    resized: sizeLayer,
    box: cfg.box || ''
  });

  window.addEventListener('resize', function () {
    Object.keys(dots).forEach(function (u) { if (u === cfg.me) place(dots[u], me.x, me.y); });
  });
  sizeLayer();
  setTimeout(sizeLayer, 1500);
  connect();
})();
