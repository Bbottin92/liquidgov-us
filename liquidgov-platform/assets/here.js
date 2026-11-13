/* Dots on profile pages (Brandon 2026-11-12): everyone on this profile page who chose Yes shows as a small round
   portrait with their handle, moving around the page. Settings come from window.LG_HERE = {me, page}, set just before
   this file loads (only for someone who chose Yes). Positions are fractions of the page (0..1) so a phone and a
   desktop see roughly the same place. Tapping and clicking work as always; "Walk" turns on a D-pad (WASD or the arrow
   keys on a keyboard): walk your dot up to a button or field and press A (or Enter) to use it, B (or Esc) to go back.
   Walker controls (Brandon 2026-11-13): on a phone the D-pad sits lower left and A/B lower right; the Walk toggle
   ("dripping") sits at the top of the screen, just under the site header. */
(function () {
  // ---- the controls, ONE map: every key binding and on-screen button is named here and only here. The code below reads
  // it, and so does tests/browser/walker_controls_check.py (through window.LG_HERE_CONTROLS), so a test never keeps its
  // own copy. keys: KeyboardEvent.key, lowercased -> action. buttons: action -> its class, accessible name and face.
  var CONTROLS = {
    dirs: ['up', 'left', 'right', 'down'],
    keys: {
      w: 'up', arrowup: 'up', a: 'left', arrowleft: 'left', s: 'down', arrowdown: 'down', d: 'right', arrowright: 'right',
      enter: 'select', escape: 'back'
    },
    buttons: {
      toggle: { cls: 'lg-here-walk', label: 'Walk', on: 'Stop walking' },
      up: { cls: 'lg-here-up', label: 'Walk up', text: '&#9650;' },
      left: { cls: 'lg-here-left', label: 'Walk left', text: '&#9664;' },
      right: { cls: 'lg-here-right', label: 'Walk right', text: '&#9654;' },
      down: { cls: 'lg-here-down', label: 'Walk down', text: '&#9660;' },
      select: { cls: 'lg-here-a', label: 'Use what you are next to', text: 'A' },
      back: { cls: 'lg-here-b', label: 'Go back', text: 'B' }
    }
  };
  window.LG_HERE_CONTROLS = CONTROLS;
  function keyAction(e) {
    if (e.ctrlKey || e.metaKey || e.altKey) return null;   // Ctrl+W, Cmd+S, ... stay the browser's
    return CONTROLS.keys[String(e.key || '').toLowerCase()] || null;
  }
  function isDir(act) { return CONTROLS.dirs.indexOf(act) >= 0; }

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

  function back() {
    if (editing()) document.activeElement.blur();   // B in a field: back to walking
    else if (history.length > 1) history.back();
  }

  // ---- the D-pad (lower left), A and B (lower right), and the Walk toggle (top)
  var B = CONTROLS.buttons;
  function keyButton(act, attrs) {
    return '<button type="button" class="' + B[act].cls + '"' + (attrs || '') + ' aria-label="' + B[act].label + '">' +
      B[act].text + '</button>';
  }
  var pad = document.createElement('div');
  pad.id = 'lg-here-pad';
  pad.innerHTML =
    '<button type="button" class="' + B.toggle.cls + '" aria-pressed="false">' + B.toggle.label + '</button>' +
    '<div class="lg-here-keys" hidden><div class="lg-here-dpad">' +
    CONTROLS.dirs.map(function (d) { return keyButton(d, ' data-dir="' + d + '"'); }).join('') +
    '</div><div class="lg-here-ab">' + keyButton('back') + keyButton('select') + '</div></div>';
  document.body.appendChild(pad);
  var walkBtn = pad.querySelector('.' + B.toggle.cls), keys = pad.querySelector('.lg-here-keys');
  var abKeys = pad.querySelector('.lg-here-ab');
  // The toggle sits at the top of the screen, on the lower edge of the (sticky) site header: it overlaps only the
  // header's bottom padding, so the header's own links stay uncovered, and it stays in the top of a phone's screen.
  function placeToggle() {
    var h = document.querySelector('header.top'), below = h ? h.getBoundingClientRect().bottom : 0;
    var pad = h ? parseFloat(getComputedStyle(h.querySelector('.topinner') || h).paddingBottom) || 0 : 0;
    walkBtn.style.top = below > 0 ? Math.round(below - pad) + 'px' : '';
  }
  // A and B stay clear of the Juketube button, which is fixed at the right edge just above the bottom.
  function clearJuketube() {
    abKeys.style.marginRight = '';
    var jb = document.getElementById('jbtoggle');
    if (!jb || keys.hidden) return;
    var j = jb.getBoundingClientRect(), a = abKeys.getBoundingClientRect();
    if (!j.width || !j.height) return;
    var overlap = a.right - j.left + 8;
    if (overlap > 0 && a.top < j.bottom && a.bottom > j.top) abKeys.style.marginRight = Math.round(overlap) + 'px';
  }
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
    walkBtn.textContent = on ? B.toggle.on : B.toggle.label;
    keys.hidden = !on;
    clearJuketube();
    fitRoom();
    held = {};
    if (on) findNear();
    else if (near) { near.classList.remove('lg-here-near'); near = null; }
  }
  // blur: Enter (select) must never land on the focused toggle and switch walking off
  walkBtn.addEventListener('click', function () { setWalking(!walking); walkBtn.blur(); });
  Array.prototype.forEach.call(pad.querySelectorAll('[data-dir]'), function (b) {
    var dir = b.getAttribute('data-dir');
    var on = function (e) { e.preventDefault(); held[dir] = true; };
    var off = function () { held[dir] = false; };
    b.addEventListener('pointerdown', on);
    b.addEventListener('pointerup', off);
    b.addEventListener('pointerleave', off);
    b.addEventListener('pointercancel', off);
  });
  pad.querySelector('.' + B.select.cls).addEventListener('click', press);
  pad.querySelector('.' + B.back.cls).addEventListener('click', back);

  // Keys work only while walking, and never while you type in a field (there only B/Esc counts: it leaves the field).
  document.addEventListener('keydown', function (e) {
    if (!walking) return;
    var act = keyAction(e);
    if (editing()) {
      if (act === 'back') back();
      return;
    }
    if (!act) return;
    if (isDir(act)) { held[act] = true; e.preventDefault(); }
    else if (act === 'select' && near) { e.preventDefault(); press(); }
    else if (act === 'back') { e.preventDefault(); back(); }
  });
  document.addEventListener('keyup', function (e) {
    var act = CONTROLS.keys[String(e.key || '').toLowerCase()];
    if (isDir(act)) held[act] = false;
  });
  window.addEventListener('blur', function () { held = {}; });   // a key let go in another window never sticks

  window.addEventListener('resize', function () {
    placeToggle();
    clearJuketube();
    fitRoom();
    Object.keys(dots).forEach(function (u) { if (u === cfg.me) place(dots[u], me.x, me.y); });
  });
  placeToggle();
  sizeLayer();
  setTimeout(sizeLayer, 1500);
  connect();
})();
