/* The walker's controls, shared by every page you can walk on (Brandon 2026-11-13): the dots on profile pages (here.js)
   and the map (lgmap_walk.js). ONE map names every key binding and on-screen button; the pages and
   tests/browser/walker_controls_check.py read it from here (window.LG_WALKER.CONTROLS), so no one keeps a copy.
   Phone: a see-through touchpad with A and B stacked on its right side, one unit at the bottom center, in thumb reach; the Drop button (Brandon's name; it turns walking on) sits at the top of the screen.
   Desktop: WASD or the arrows move, Enter = A (select), Esc = B (back). Keys only count while Drop is on, and never while
   you type in a field (there B/Esc just leaves the field). */
(function () {
  var CONTROLS = {
    dirs: ['up', 'left', 'right', 'down'],
    // The walker's top speed: a dot walks maxSpeed px per 40 ms tick with a key held or the touchpad pulled all the way.
    maxSpeed: 7,
    // The phone's touchpad (Brandon 2026-11-13 1:08 AM, in place of the D-pad; it works like a joystick): touch anywhere
    // on it and drag; the farther your finger from where it touched (up to reach px), the faster you walk. A pull under
    // deadZone (a fraction of reach) does nothing; full reach is maxSpeed. On the map one push walks one connector, and
    // held it walks on, every mapRepeatMs at full pull.
    // Its look and place (1:08/1:09 AM), the one source for site.css (as CSS variables on #lg-here-pad): width x height
    // px, A and B are boxes height/2 tall stacked on its right side (together its height), all three at opacity alpha;
    // the unit sits at the bottom center, lift px above the bottom edge (above the safe area), at most the screen's width
    // less 2 x margin.
    touchpad: { cls: 'lg-here-touchpad', knob: 'lg-here-knob', label: 'Walk: touch and drag', reach: 48, deadZone: 0.2,
      mapRepeatMs: 600, width: 220, height: 132, alpha: 0.62, lift: 28, margin: 16 },
    // Dropped in on the map, the camera shows the boxes this many times bigger than the fitted view and follows the
    // walker box to box (Brandon 2026-11-13 12:55 AM: "it should look bigger"); off again, the normal view.
    mapZoom: 1.8,
    // The portal (Brandon 2026-11-13 1:02 AM): the site's MAINstream link (the brand in the header, to /) drawn as a
    // round ring. Dropped in, using it (A / Enter next to it) takes you to the map at the box of the page you were on:
    // the page's box id rides along in sessionStorage[handoff] for up to handoffMs.
    portal: { selector: 'header.top a.brand', cls: 'lg-portal', handoff: 'lg-walker-portal', handoffMs: 120000 },
    // KeyboardEvent.key, lowercased -> action
    keys: {
      w: 'up', arrowup: 'up', a: 'left', arrowleft: 'left', s: 'down', arrowdown: 'down', d: 'right', arrowright: 'right',
      enter: 'select', escape: 'back'
    },
    // action -> the button's class, accessible name and face
    buttons: {
      // legacy: the class it had before Brandon named it Drop; only the committed dots/herepad checks still look for it
      // bar: its own strip across the top of the screen (Brandon 1:59 AM: it covered the MAINstream button); the page and
      // its sticky header move down by the strip's height, so the button covers nothing; gap px above and below it
      drop: { cls: 'lg-here-drop', legacy: 'lg-here-walk', label: 'Drop', on: 'Stop dropping', bar: 'lg-here-dropbar', gap: 6 },
      select: { cls: 'lg-here-a', label: 'Use what you are next to', text: 'A' },
      back: { cls: 'lg-here-b', label: 'Go back', text: 'B' }
    }
  };
  var B = CONTROLS.buttons;

  function keyAction(e) {
    if (e.ctrlKey || e.metaKey || e.altKey) return null;   // Ctrl+W, Cmd+S, ... stay the browser's
    return CONTROLS.keys[String(e.key || '').toLowerCase()] || null;
  }
  function isDir(act) { return CONTROLS.dirs.indexOf(act) >= 0; }
  function editing() {
    var a = document.activeElement;
    return !!a && (a.tagName === 'INPUT' || a.tagName === 'TEXTAREA' || a.tagName === 'SELECT' || a.isContentEditable);
  }
  function keyButton(act, attrs) {
    return '<button type="button" class="' + B[act].cls + '"' + (attrs || '') + ' aria-label="' + B[act].label + '">' +
      B[act].text + '</button>';
  }

  // The touchpad's pull, from the finger's offset (px, from where it touched) to a walking vector: {x, y} with length 0
  // inside the dead zone, rising to 1 at full reach (so speed = length * maxSpeed).
  function pullVector(dx, dy) {
    var S = CONTROLS.touchpad, len = Math.sqrt(dx * dx + dy * dy), pull = Math.min(1, len / S.reach);
    if (!len || pull < S.deadZone) return { x: 0, y: 0 };
    var k = (pull - S.deadZone) / (1 - S.deadZone) / len;
    return { x: dx * k, y: dy * k };
  }

  // mount(page): puts the Drop button and the pad on the page (once) and wires keys, the touchpad and buttons to the
  // page's walker. page: {dir(dir, down) (keys), pull(x, y) (the touchpad's vector; 0, 0 = let go), select() -> true when it used something, back(), dropping(on), box: this page's map box id
  // (optional; the portal takes you there)}. Returns {dropping(), start(), stop()}.
  function mount(page) {
    var T = CONTROLS.touchpad;
    var pad = document.createElement('div');
    pad.id = 'lg-here-pad';
    pad.innerHTML =
      '<div class="' + B.drop.bar + '" aria-hidden="true"></div><button type="button" class="' + B.drop.cls + ' ' + B.drop.legacy + '" aria-pressed="false">' + B.drop.label + '</button>' +
      '<div class="lg-here-keys" hidden><div class="' + T.cls + '" role="application" aria-label="' + T.label +
      '"><span class="' + T.knob + '"></span></div><div class="lg-here-ab">' + keyButton('select') + keyButton('back') + '</div></div>';
    [['--lgw-w', T.width + 'px'], ['--lgw-h', T.height + 'px'], ['--lgw-alpha', String(T.alpha)], ['--lgw-lift', T.lift + 'px'],
      ['--lgw-margin', T.margin + 'px'], ['--lgw-dropgap', B.drop.gap + 'px']].forEach(function (v) { pad.style.setProperty(v[0], v[1]); });
    document.body.appendChild(pad);
    var dropBtn = pad.querySelector('.' + B.drop.cls), keys = pad.querySelector('.lg-here-keys');
    var dropping = false, held = {};
    var touchpad = pad.querySelector('.' + T.cls), knob = touchpad.firstChild, grip = null, from = null;

    // The Drop button sits at the top of the screen in its own strip (B.drop.bar, above the safe area's top): the page
    // gets the strip's height as room at its top and the sticky site header sticks just under it, so the button covers
    // nothing (not the header's links, not the map's MAINstream button).
    var bar = pad.querySelector('.' + B.drop.bar), header = document.querySelector('header.top');
    var baseTop = parseFloat(getComputedStyle(document.body).paddingTop) || 0;
    function placeDrop() {
      pad.style.setProperty('--lgw-droph', dropBtn.offsetHeight + 'px');
      var h = bar.getBoundingClientRect().height;
      document.body.style.paddingTop = (baseTop + h) + 'px';
      if (header && getComputedStyle(header).position === 'sticky') header.style.top = h + 'px';
    }
    // The unit stays clear of the Juketube button (fixed at the right edge just above the bottom): if they meet, the
    // unit rises above it.
    function clearJuketube() {
      keys.style.marginBottom = '';
      var jb = document.getElementById('jbtoggle');
      if (!jb || keys.hidden) return;
      var j = jb.getBoundingClientRect(), a = keys.getBoundingClientRect();
      if (!j.width || !j.height) return;
      if (a.left < j.right && a.right > j.left && a.top < j.bottom && a.bottom > j.top) keys.style.marginBottom = Math.round(a.bottom - j.top + 8) + 'px';
    }
    // Room at the page's end for the pad (Brandon 2026-11-12 "do the dpad fix"): the page already keeps some room at the
    // bottom (for the Juketube button); while the pad is taller than that, the page gets the pad's height plus a margin,
    // so scrolled to the end nothing stays under it. Given back when Drop goes off.
    var baseRoom = parseFloat(getComputedStyle(document.body).paddingBottom) || 0;
    function fitRoom() {
      var need = pad.getBoundingClientRect().height + 24;
      document.body.style.paddingBottom = need > baseRoom ? need + 'px' : '';
      if (page.resized) page.resized();
    }
    function hold(dir, down) {
      if (!!held[dir] === down) return;
      held[dir] = down;
      page.dir(dir, down);
    }
    // the touchpad: the pointer that touched it drives it until it lets go; the knob shows where it touched and the pull
    function pull(e) {
      var r = touchpad.getBoundingClientRect(), dx = e.clientX - from.x, dy = e.clientY - from.y;
      var len = Math.sqrt(dx * dx + dy * dy);
      if (len > T.reach) { dx = dx * T.reach / len; dy = dy * T.reach / len; }
      knob.style.left = Math.round(from.x - r.left + dx) + 'px';
      knob.style.top = Math.round(from.y - r.top + dy) + 'px';
      var v = pullVector(dx, dy);
      page.pull(v.x, v.y);
    }
    function release() {
      if (grip === null) return;
      grip = null;
      knob.style.left = knob.style.top = '';
      touchpad.classList.remove('lg-here-touching');
      page.pull(0, 0);
    }
    function letGo() { Object.keys(held).forEach(function (d) { hold(d, false); }); release(); }
    function setDropping(on) {
      letGo();
      dropping = on;
      dropBtn.setAttribute('aria-pressed', on ? 'true' : 'false');
      dropBtn.textContent = on ? B.drop.on : B.drop.label;
      keys.hidden = !on;
      clearJuketube();
      fitRoom();
      page.dropping(on);
    }
    function back() {
      if (editing()) document.activeElement.blur();   // B in a field: back to walking
      else page.back();
    }
    // blur: Enter (select) must never land on the focused Drop button and switch it off
    dropBtn.addEventListener('click', function () { setDropping(!dropping); dropBtn.blur(); });
    touchpad.addEventListener('pointerdown', function (e) {
      e.preventDefault();
      grip = e.pointerId;
      from = { x: e.clientX, y: e.clientY };
      touchpad.classList.add('lg-here-touching');
      try { touchpad.setPointerCapture(e.pointerId); } catch (x) {} // err-ok: without capture it still follows over itself
      pull(e);
    });
    touchpad.addEventListener('pointermove', function (e) { if (e.pointerId === grip) pull(e); });
    ['pointerup', 'pointercancel', 'lostpointercapture'].forEach(function (t) {
      touchpad.addEventListener(t, function (e) { if (e.pointerId === grip) release(); });
    });
    pad.querySelector('.' + B.select.cls).addEventListener('click', function () { page.select(); });
    pad.querySelector('.' + B.back.cls).addEventListener('click', back);

    document.addEventListener('keydown', function (e) {
      if (!dropping || e.defaultPrevented) return;   // defaultPrevented: the focused element already took the key
      var act = keyAction(e);
      if (editing()) {
        if (act === 'back') back();
        return;
      }
      if (!act) return;
      if (isDir(act)) { e.preventDefault(); hold(act, true); }
      else if (act === 'select') { if (page.select()) e.preventDefault(); }
      else if (act === 'back') { e.preventDefault(); back(); }
    });
    document.addEventListener('keyup', function (e) {
      var act = CONTROLS.keys[String(e.key || '').toLowerCase()];
      if (isDir(act)) hold(act, false);
    });
    var portal = document.querySelector(CONTROLS.portal.selector);
    if (portal) {
      portal.classList.add(CONTROLS.portal.cls);
      portal.addEventListener('click', function () {
        if (!dropping) return;
        try { sessionStorage.setItem(CONTROLS.portal.handoff, JSON.stringify({ box: page.box || '', t: Date.now() })); } catch (e) {} // err-ok: private mode: the map just starts at you
      });
    }
    window.addEventListener('blur', letGo);   // a key let go in another window never sticks
    window.addEventListener('resize', function () { placeDrop(); clearJuketube(); fitRoom(); });
    placeDrop();
    return { dropping: function () { return dropping; }, start: function () { if (!dropping) setDropping(true); },
      stop: function () { if (dropping) setDropping(false); } };
  }

  // The portal's arrival: {box} once (it is removed as it is read), or null (none, or older than handoffMs).
  function takeHandoff() {
    var P = CONTROLS.portal, v = null;
    try { v = JSON.parse(sessionStorage.getItem(P.handoff) || 'null'); sessionStorage.removeItem(P.handoff); } catch (e) { v = null; } // err-ok: unreadable = no arrival
    return v && typeof v.t === 'number' && Date.now() - v.t <= P.handoffMs ? { box: String(v.box || '') } : null;
  }

  window.LG_WALKER = { CONTROLS: CONTROLS, keyAction: keyAction, isDir: isDir, editing: editing, pullVector: pullVector, mount: mount,
    takeHandoff: takeHandoff };
})();
