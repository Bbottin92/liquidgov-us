/* The walker's controls, shared by every page you can walk on (Brandon 2026-11-13): the dots on profile pages (here.js)
   and the map (lgmap_walk.js). ONE map names every key binding and on-screen button; the pages and
   tests/browser/walker_controls_check.py read it from here (window.LG_WALKER.CONTROLS), so no one keeps a copy.
   Phone: the D-pad sits lower left, A and B lower right; the Walk toggle ("dripping") sits at the top of the screen.
   Desktop: WASD or the arrows move, Enter = A (select), Esc = B (back). Keys only count while walking, and never while
   you type in a field (there B/Esc just leaves the field). */
(function () {
  var CONTROLS = {
    dirs: ['up', 'left', 'right', 'down'],
    // KeyboardEvent.key, lowercased -> action
    keys: {
      w: 'up', arrowup: 'up', a: 'left', arrowleft: 'left', s: 'down', arrowdown: 'down', d: 'right', arrowright: 'right',
      enter: 'select', escape: 'back'
    },
    // action -> the button's class, accessible name and face
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

  // mount(page): puts the Walk toggle and the pad on the page (once) and wires keys and buttons to the page's walker.
  // page: {dir(dir, down), select() -> true when it used something, back(), walking(on)}. Returns {walking(), stop()}.
  function mount(page) {
    var pad = document.createElement('div');
    pad.id = 'lg-here-pad';
    pad.innerHTML =
      '<button type="button" class="' + B.toggle.cls + '" aria-pressed="false">' + B.toggle.label + '</button>' +
      '<div class="lg-here-keys" hidden><div class="lg-here-dpad">' +
      CONTROLS.dirs.map(function (d) { return keyButton(d, ' data-dir="' + d + '"'); }).join('') +
      '</div><div class="lg-here-ab">' + keyButton('back') + keyButton('select') + '</div></div>';
    document.body.appendChild(pad);
    var walkBtn = pad.querySelector('.' + B.toggle.cls), keys = pad.querySelector('.lg-here-keys');
    var abKeys = pad.querySelector('.lg-here-ab'), walking = false, held = {};

    // The toggle sits at the top of the screen, on the lower edge of the (sticky) site header: it overlaps only the
    // header's bottom padding, so the header's own links stay uncovered, and it stays in the top of a phone's screen.
    function placeToggle() {
      var h = document.querySelector('header.top'), below = h ? h.getBoundingClientRect().bottom : 0;
      var inner = h ? parseFloat(getComputedStyle(h.querySelector('.topinner') || h).paddingBottom) || 0 : 0;
      walkBtn.style.top = below > 0 ? Math.round(below - inner) + 'px' : '';
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
      if (page.resized) page.resized();
    }
    function hold(dir, down) {
      if (!!held[dir] === down) return;
      held[dir] = down;
      page.dir(dir, down);
    }
    function letGo() { Object.keys(held).forEach(function (d) { hold(d, false); }); }
    function setWalking(on) {
      letGo();
      walking = on;
      walkBtn.setAttribute('aria-pressed', on ? 'true' : 'false');
      walkBtn.textContent = on ? B.toggle.on : B.toggle.label;
      keys.hidden = !on;
      clearJuketube();
      fitRoom();
      page.walking(on);
    }
    function back() {
      if (editing()) document.activeElement.blur();   // B in a field: back to walking
      else page.back();
    }
    // blur: Enter (select) must never land on the focused toggle and switch walking off
    walkBtn.addEventListener('click', function () { setWalking(!walking); walkBtn.blur(); });
    Array.prototype.forEach.call(pad.querySelectorAll('[data-dir]'), function (b) {
      var dir = b.getAttribute('data-dir');
      b.addEventListener('pointerdown', function (e) { e.preventDefault(); hold(dir, true); });
      ['pointerup', 'pointerleave', 'pointercancel'].forEach(function (t) { b.addEventListener(t, function () { hold(dir, false); }); });
    });
    pad.querySelector('.' + B.select.cls).addEventListener('click', function () { page.select(); });
    pad.querySelector('.' + B.back.cls).addEventListener('click', back);

    document.addEventListener('keydown', function (e) {
      if (!walking || e.defaultPrevented) return;   // defaultPrevented: the focused element already took the key
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
    window.addEventListener('blur', letGo);   // a key let go in another window never sticks
    window.addEventListener('resize', function () { placeToggle(); clearJuketube(); fitRoom(); });
    placeToggle();
    return { walking: function () { return walking; }, stop: function () { if (walking) setWalking(false); } };
  }

  window.LG_WALKER = { CONTROLS: CONTROLS, keyAction: keyAction, isDir: isDir, editing: editing, mount: mount };
})();
