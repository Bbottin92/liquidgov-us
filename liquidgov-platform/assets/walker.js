/* The walker's controls, shared by every page you can walk on (Brandon 2026-11-13): the dots on profile pages (here.js)
   and the map (lgmap_walk.js). ONE map names every key binding and on-screen button; the pages and
   tests/browser/walker_controls_check.py read it from here (window.LG_WALKER.CONTROLS), so no one keeps a copy.
   Phone: the D-pad sits lower left, A and B lower right; the Drop button (Brandon's name; it turns walking on) sits at the top of the screen.
   Desktop: WASD or the arrows move, Enter = A (select), Esc = B (back). Keys only count while Drop is on, and never while
   you type in a field (there B/Esc just leaves the field). */
(function () {
  var CONTROLS = {
    dirs: ['up', 'left', 'right', 'down'],
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
      drop: { cls: 'lg-here-drop', legacy: 'lg-here-walk', label: 'Drop', on: 'Stop dropping' },
      up: { cls: 'lg-here-up', label: 'Walk up', text: '&#9650;' },
      left: { cls: 'lg-here-left', label: 'Walk left', text: '&#9664;' },
      right: { cls: 'lg-here-right', label: 'Walk right', text: '&#9654;' },
      down: { cls: 'lg-here-down', label: 'Walk down', text: '&#9660;' },
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

  // mount(page): puts the Drop button and the pad on the page (once) and wires keys and buttons to the page's walker.
  // page: {dir(dir, down), select() -> true when it used something, back(), dropping(on), box: this page's map box id
  // (optional; the portal takes you there)}. Returns {dropping(), start(), stop()}.
  function mount(page) {
    var pad = document.createElement('div');
    pad.id = 'lg-here-pad';
    pad.innerHTML =
      '<button type="button" class="' + B.drop.cls + ' ' + B.drop.legacy + '" aria-pressed="false">' + B.drop.label + '</button>' +
      '<div class="lg-here-keys" hidden><div class="lg-here-dpad">' +
      CONTROLS.dirs.map(function (d) { return keyButton(d, ' data-dir="' + d + '"'); }).join('') +
      '</div><div class="lg-here-ab">' + keyButton('back') + keyButton('select') + '</div></div>';
    document.body.appendChild(pad);
    var dropBtn = pad.querySelector('.' + B.drop.cls), keys = pad.querySelector('.lg-here-keys');
    var abKeys = pad.querySelector('.lg-here-ab'), dropping = false, held = {};

    // The Drop button sits at the top of the screen, on the lower edge of the (sticky) site header: it overlaps only the
    // header's bottom padding, so the header's own links stay uncovered, and it stays in the top of a phone's screen.
    function placeDrop() {
      var h = document.querySelector('header.top'), below = h ? h.getBoundingClientRect().bottom : 0;
      var inner = h ? parseFloat(getComputedStyle(h.querySelector('.topinner') || h).paddingBottom) || 0 : 0;
      dropBtn.style.top = below > 0 ? Math.round(below - inner) + 'px' : '';
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
    function letGo() { Object.keys(held).forEach(function (d) { hold(d, false); }); }
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
    Array.prototype.forEach.call(pad.querySelectorAll('[data-dir]'), function (b) {
      var dir = b.getAttribute('data-dir');
      b.addEventListener('pointerdown', function (e) { e.preventDefault(); hold(dir, true); });
      ['pointerup', 'pointerleave', 'pointercancel'].forEach(function (t) { b.addEventListener(t, function () { hold(dir, false); }); });
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

  window.LG_WALKER = { CONTROLS: CONTROLS, keyAction: keyAction, isDir: isDir, editing: editing, mount: mount,
    takeHandoff: takeHandoff };
})();
