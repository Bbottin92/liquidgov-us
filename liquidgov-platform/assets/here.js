/* Dots on profile pages (Brandon 2026-11-12): everyone on this profile page who chose Yes shows as a small round
   portrait with their handle, moving around the page. Settings come from window.LG_HERE = {me, page}, set just before
   this file loads (only for someone who chose Yes). Positions are fractions of the page (0..1) so a phone and a
   desktop see roughly the same place. Tapping and clicking work as always; "Walk" turns on a D-pad (arrow keys on a
   keyboard): walk your dot up to a button or field and press A (or Enter) to use it. */
(function () {
  var cfg = window.LG_HERE;
  if (!cfg || !cfg.me || !cfg.page || !window.WebSocket) return;
  var STEP = 7, NEAR = 30, SEND_MS = 110;

  var layer = document.createElement('div');
  layer.id = 'lg-here';
  layer.setAttribute('aria-hidden', 'true');
  document.body.appendChild(layer);

  var dots = {}, me = { x: 0.5, y: 0.08, known: false }, walking = false, held = {}, lastSent = 0, dirty = false;
  var near = null, ws = null, tries = 0;

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
    d.appendChild(img);
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
      try { render(JSON.parse(ev.data).people || []); } catch (e) { /* a bad frame is skipped */ }
    };
    ws.onclose = function () {
      Object.keys(dots).forEach(function (u) { if (u !== cfg.me) { dots[u].remove(); delete dots[u]; } });
      if (++tries <= 5) setTimeout(connect, 1000 * tries);
    };
  }

  // ---- walking
  function editing() {
    var a = document.activeElement;
    return a && (a.tagName === 'INPUT' || a.tagName === 'TEXTAREA' || a.tagName === 'SELECT' || a.isContentEditable);
  }
  function usable() {
    var all = document.querySelectorAll('a[href],button,input:not([type=hidden]),select,textarea,summary,[role=button]');
    var out = [];
    for (var i = 0; i < all.length; i++) {
      var e = all[i];
      if (e.closest('#lg-here,#lg-here-pad')) continue;
      var r = e.getBoundingClientRect();
      if (r.width && r.height) out.push(e);
    }
    return out;
  }
  function findNear() {
    var cx = me.x * pageW() - window.scrollX, cy = me.y * pageH() - window.scrollY, best = null, bestD = NEAR;
    usable().forEach(function (e) {
      var r = e.getBoundingClientRect();
      var dx = Math.max(r.left - cx, 0, cx - r.right), dy = Math.max(r.top - cy, 0, cy - r.bottom);
      var d = Math.sqrt(dx * dx + dy * dy);
      if (d <= bestD) { bestD = d; best = e; }
    });
    if (near !== best) {
      if (near) near.classList.remove('lg-here-near');
      near = best;
      if (near) near.classList.add('lg-here-near');
    }
  }
  function press() {
    if (!near) return;
    var t = near.tagName;
    if (t === 'INPUT' || t === 'TEXTAREA' || t === 'SELECT') near.focus();
    else near.click();
  }
  function step() {
    var dx = (held.right ? 1 : 0) - (held.left ? 1 : 0), dy = (held.down ? 1 : 0) - (held.up ? 1 : 0);
    if (dx || dy) {
      me.x = Math.max(0, Math.min(1, me.x + dx * STEP / pageW()));
      me.y = Math.max(0, Math.min(1, me.y + dy * STEP / pageH()));
      me.known = true;
      dirty = true;
      if (dots[cfg.me]) place(dots[cfg.me], me.x, me.y);
      var top = me.y * pageH(), margin = 90;
      if (top < window.scrollY + margin) window.scrollTo(window.scrollX, top - margin);
      else if (top > window.scrollY + window.innerHeight - margin * 2) window.scrollTo(window.scrollX, top - window.innerHeight + margin * 2);
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

  // ---- the D-pad
  var pad = document.createElement('div');
  pad.id = 'lg-here-pad';
  pad.innerHTML =
    '<button type="button" class="lg-here-walk" aria-pressed="false">Walk</button>' +
    '<div class="lg-here-keys" hidden>' +
    '<button type="button" data-dir="up" aria-label="Walk up">&#9650;</button>' +
    '<button type="button" data-dir="left" aria-label="Walk left">&#9664;</button>' +
    '<button type="button" data-dir="right" aria-label="Walk right">&#9654;</button>' +
    '<button type="button" data-dir="down" aria-label="Walk down">&#9660;</button>' +
    '<button type="button" class="lg-here-a" aria-label="Use what you are next to">A</button></div>';
  document.body.appendChild(pad);
  var walkBtn = pad.querySelector('.lg-here-walk'), keys = pad.querySelector('.lg-here-keys');
  // Room at the page's end for the pad (Brandon 2026-11-12 "do the dpad fix"): the page already keeps some room at the
  // bottom (for the Juketube button); while the pad is taller than that, the page gets the pad's height plus a margin,
  // so scrolled to the end nothing stays under it. Given back when walking stops.
  var baseRoom = parseFloat(getComputedStyle(document.body).paddingBottom) || 0;
  function fitRoom() {
    var need = pad.getBoundingClientRect().height + 24;
    document.body.style.paddingBottom = need > baseRoom ? need + 'px' : '';
    sizeLayer();
  }
  function setWalking(on) {
    walking = on;
    walkBtn.setAttribute('aria-pressed', on ? 'true' : 'false');
    walkBtn.textContent = on ? 'Stop walking' : 'Walk';
    keys.hidden = !on;
    fitRoom();
    held = {};
    if (on) findNear();
    else if (near) { near.classList.remove('lg-here-near'); near = null; }
  }
  walkBtn.addEventListener('click', function () { setWalking(!walking); });
  Array.prototype.forEach.call(pad.querySelectorAll('[data-dir]'), function (b) {
    var dir = b.getAttribute('data-dir');
    var on = function (e) { e.preventDefault(); held[dir] = true; };
    var off = function () { held[dir] = false; };
    b.addEventListener('pointerdown', on);
    b.addEventListener('pointerup', off);
    b.addEventListener('pointerleave', off);
    b.addEventListener('pointercancel', off);
  });
  pad.querySelector('.lg-here-a').addEventListener('click', press);

  var KEYS = { ArrowUp: 'up', ArrowDown: 'down', ArrowLeft: 'left', ArrowRight: 'right' };
  document.addEventListener('keydown', function (e) {
    if (!walking) return;
    if (editing()) {
      if (e.key === 'Escape') document.activeElement.blur();   // back to walking
      return;
    }
    if (KEYS[e.key]) { held[KEYS[e.key]] = true; e.preventDefault(); }
    else if ((e.key === 'Enter' || e.key === 'a' || e.key === 'A') && near) { e.preventDefault(); press(); }
  });
  document.addEventListener('keyup', function (e) { if (KEYS[e.key]) held[KEYS[e.key]] = false; });

  window.addEventListener('resize', function () {
    fitRoom();
    Object.keys(dots).forEach(function (u) { if (u === cfg.me) place(dots[u], me.x, me.y); });
  });
  sizeLayer();
  setTimeout(sizeLayer, 1500);
  connect();
})();
