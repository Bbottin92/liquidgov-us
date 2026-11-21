/* The walker's controls, shared by every page you can walk on (Brandon 2026-11-13): the dots on profile pages (here.js),
   the home page (home_drop.js) and the map (lgmap_walk.js). ONE map names every key binding and on-screen button; the
   pages and the checks read it from here (window.LG_WALKER.CONTROLS), so no one keeps a copy.
   Walking is always on (Brandon 2026-11-14: "we gotta get rid of the drop/scroll switch... just leave it on drop mode
   and if you wanna scroll you can just do that instead of using the joystick"): there is no switch. You scroll the
   page, or pinch and wheel the map, the way you always do; the touchpad and the keys walk your drop.
   Phone: one small round see-through joystick at the bottom center, in thumb reach (Brandon 2026-11-17, q64; it
   replaced the touchpad with A and B beside it): drag it to walk; a quick tap on it is A (use what you are next to).
   There is no B button: the phone's own back button and gesture go back. With a mouse (a screen that can hover) the
   joystick is not shown.
   Desktop: WASD or the arrows move, Enter = A (select), Esc = B (back); never while you type in a field (there B/Esc
   just leaves the field).
   The screen keeps your drop in its middle as you walk, always (Brandon 2026-11-14: "keep the screen tracking logic the
   way it is when center is selected, but like thats it, no other options"; the Center button is gone).
   Your drop's menu (same day: "if you click on or tap your droplet it should expand a menu with things like microphone,
   camera (toggle on/off), and a drip speed adjustment slider"): menu(el) makes a drop open it. Microphone, Camera and
   Hear myself are on every page and stay as you set them (CONTROLS.media); the speed is remembered on this device. */
(function () {
  // A page shown inside a portal's peek window (lgmap_walk.js adds lg_peek=1 to its address) is only looked at: no
  // drop, no presence, no camera or microphone there (here.js and here_media.js need LG_WALKER, so they stop too).
  if (window.parent !== window && /[?&]lg_peek=1(&|$)/.test(location.search)) return;
  var CONTROLS = {
    dirs: ['up', 'left', 'right', 'down'],
    // The walker's top speed: a dot walks maxSpeed px per 40 ms tick with a key held or the touchpad pulled all the way.
    maxSpeed: 7,
    // The drop (Brandon 2:15 AM): your portrait, circular at rest, a tail behind you and a little stretch at speed.
    // speed is a fraction of maxSpeed; one shape function is used on profiles and the map. Reduced motion: circle.
    shape: { cls: 'lg-walker-drop', tailCls: 'lg-walker-tail', phCls: 'lg-here-ph', size: 38, tail: 0.9, stretch: 0.16,
      settleMs: 180, mapMoveMs: 350 },
    // The phone's joystick (Brandon 2026-11-17, q64: "a small round joystick ... drag walks, tap = A, B removed"; it
    // replaced the 2026-11-13 touchpad with A/B): touch it and drag; the farther your finger from where it touched (up to
    // reach px), the faster you walk. A pull under deadZone (a fraction of reach) does nothing; full reach is maxSpeed.
    // A touch that lets go within tapMs ms, never having moved tapSlop px, is a tap: A (use what you are next to).
    // Its look and place, the one source for site.css (as CSS variables on #lg-here-pad): a round size px across at
    // opacity alpha, at the bottom center, lift px above the bottom edge (above the safe area), margin px from the sides.
    // (The key stays 'touchpad' so every page and check reads the same entry.)
    touchpad: { cls: 'lg-here-touchpad', knob: 'lg-here-knob', label: 'Walk: drag. Tap: use what you are next to',
      reach: 34, deadZone: 0.2, size: 68, alpha: 0.62, lift: 28, margin: 16, tapMs: 300, tapSlop: 8 },
    // On the map the camera shows the boxes this many times bigger than the fitted view, around your drop (Brandon
    // 2026-11-13 12:55 AM: "it should look bigger"), until you zoom out yourself (pinch, wheel).
    mapZoom: 1.8,
    // The portal (Brandon 2026-11-13 1:02 AM): the site's MAINstream link (the brand in the header, to /) drawn as a
    // round ring. Dropped in, using it (A / Enter next to it) takes you to the map at the box of the page you were on:
    // the page's box id rides along in sessionStorage[handoff] for up to handoffMs.
    portal: { selector: 'header.top a.brand', cls: 'lg-portal', handoff: 'lg-walker-portal', handoffMs: 120000 },
    // How close (px from your drop's middle to a button's edge) you must stand to use a button with A / Enter.
    near: 30,
    // A with nothing next to you (Brandon 4:03 AM): a quick squish-and-bounce. The drop crouches (scale crouch, x/y),
    // splats out a little (splat; a mild slime mark spreads under it, splatCls), then grows briefly (grow: the jump)
    // and settles, all in ms. Individual CSS scale (not transform), so it adds to the teardrop's shape. Reduced motion:
    // none.
    hop: { ms: 560, crouch: [1.14, 0.8], splat: [1.32, 0.66], grow: 1.24, splatCls: 'lg-walker-splat' },
    // The home page's hook (Brandon 4:03 AM): your drop starts near the top of the page (startTop px under the header)
    // and a guide points you down the page (guide.text); past the scrolling-words section (belt.after) the floor is a
    // moving sidewalk flowing belt.dir: walking with it is (1 + boost) x as fast, against it (1 - boost) x; standing,
    // it leaves you be.
    home: { startTop: 56, guide: { cls: 'lg-home-guide', text: 'Walk down' },
      belt: { after: '.solution-row', cls: 'lg-home-belt', dir: { x: 0, y: 1 }, boost: 0.6 } },
    // KeyboardEvent.key, lowercased -> action
    keys: {
      w: 'up', arrowup: 'up', a: 'left', arrowleft: 'left', s: 'down', arrowdown: 'down', d: 'right', arrowright: 'right',
      enter: 'select', escape: 'back'
    },
    // On-screen buttons: none since the joystick (q64): select is a tap on it (or Enter), back is the phone's own back
    // (or Esc). Kept as an empty map so a page or check that lists the buttons finds none.
    buttons: {}
  };
  // Your drop's menu: id and look of the popup, the accessible name of your drop, the toggles' names, and the drip speed
  // slider (a multiple of maxSpeed, from min to max in steps; store: its localStorage key).
  CONTROLS.menu = { id: 'lg-drop-menu', label: 'Your drop: microphone, camera and speed',
    toggles: { mic: 'Microphone', cam: 'Camera', hear: 'Hear myself (headphones)' },
    speed: { label: 'Drip speed', min: 0.5, max: 2, step: 0.1, store: 'lg-walker-speed' } };
  // Your camera and microphone (Brandon 2026-11-15: "if you turn on camera and/or microphone, they should stay on
  // everywhere until you make a clear decision to turn them off"; "you should be allowed to see your own camera and hear
  // your own microphone"). The switches are in your drop's menu on every page; the choice is kept on this device
  // (store) and every page with your drop turns them back on as it loads. Your camera shows as a bubble on your own
  // drop; "Hear myself" plays your microphone back to you (off by default: a page cannot tell whether headphones are
  // plugged in, and on speakers it howls). Calls (here_media.js) use this stream through LG_WALKER.media.
  CONTROLS.media = { store: 'lg-media-on', selfCls: 'lg-media-self',
    // Sound ripples (Brandon 2026-11-15: "audiowaves that ripple off of your droplet when you make noises according to
    // the intensity of the noises you make ... roughly anatomically correct"): rings spread from a drop the way sound
    // does. The sound's level (RMS, 0..1) is read about 60 times a second; above floor a ring starts, at most one every
    // minMs (louder: down to fastMs apart), lives ms, and spreads to reach (1 + level * spread) times the drop's size;
    // its strength fades as it spreads (as sound does), starting from the level. Kept close (Brandon: "the ripples
    // shouldnt spread too far"): at most 2.5 times the drop's size. Not with reduced motion.
    ripple: { cls: 'lg-ripple', floor: 0.02, gain: 6, minMs: 260, fastMs: 110, ms: 900, reach: 1.3, spread: 1.2 } };
  var B = CONTROLS.buttons, M = CONTROLS.menu;
  var speedMul = 1;
  try { var sv = parseFloat(localStorage.getItem(M.speed.store)); if (sv >= M.speed.min && sv <= M.speed.max) speedMul = sv; } catch (e) {} // err-ok: no storage: 1x

  function keyAction(e) {
    if (e.ctrlKey || e.metaKey || e.altKey) return null;   // Ctrl+W, Cmd+S, ... stay the browser's
    return CONTROLS.keys[String(e.key || '').toLowerCase()] || null;
  }
  function isDir(act) { return CONTROLS.dirs.indexOf(act) >= 0; }
  function editing() {
    var a = document.activeElement;
    return !!a && (a.tagName === 'INPUT' || a.tagName === 'TEXTAREA' || a.tagName === 'SELECT' || a.isContentEditable);
  }
  // The touchpad's pull, from the finger's offset (px, from where it touched) to a walking vector: {x, y} with length 0
  // inside the dead zone, rising to 1 at full reach (so speed = length * maxSpeed).
  function pullVector(dx, dy) {
    var S = CONTROLS.touchpad, len = Math.sqrt(dx * dx + dy * dy), pull = Math.min(1, len / S.reach);
    if (!len || pull < S.deadZone) return { x: 0, y: 0 };
    var k = (pull - S.deadZone) / (1 - S.deadZone) / len;
    return { x: dx * k, y: dy * k };
  }

  // mount(page): puts the pad on the page (once) and wires keys, the touchpad and buttons to the page's walker.
  // page: {dir(dir, down) (keys), pull(x, y) (the touchpad's vector; 0, 0 = let go), select() -> true when it used
  // something, back(), resized() (optional), box: this page's map box id (optional; the portal takes you there)}.
  function mount(page) {
    var T = CONTROLS.touchpad;
    var pad = document.createElement('div');
    pad.id = 'lg-here-pad';
    pad.innerHTML =
      '<div class="lg-here-keys"><div class="' + T.cls + '" role="application" aria-label="' + T.label +
      '"><span class="' + T.knob + '"></span></div></div>';
    [['--lgw-size', T.size + 'px'], ['--lgw-alpha', String(T.alpha)], ['--lgw-lift', T.lift + 'px'],
      ['--lgw-margin', T.margin + 'px']].forEach(function (v) { pad.style.setProperty(v[0], v[1]); });
    document.body.appendChild(pad);
    var keys = pad.querySelector('.lg-here-keys');
    var held = {};
    var touchpad = pad.querySelector('.' + T.cls), knob = touchpad.firstChild, grip = null, from = null;
    var downAt = 0, moved = false;   // a tap = let go within T.tapMs, never past T.tapSlop px: A (select)

    // The unit stays clear of the Juketube button (fixed at the right edge just above the bottom): if they meet, the
    // unit rises above it.
    function clearJuketube() {
      keys.style.marginBottom = '';
      var jb = document.getElementById('jbtoggle');
      if (!jb) return;
      var j = jb.getBoundingClientRect(), a = keys.getBoundingClientRect();
      if (!j.width || !j.height || !a.width) return;
      if (a.left < j.right && a.right > j.left && a.top < j.bottom && a.bottom > j.top) keys.style.marginBottom = Math.round(a.bottom - j.top + 8) + 'px';
    }
    // Room at the page's end for the pad (Brandon 2026-11-12 "do the dpad fix"): the page already keeps some room at the
    // bottom (for the Juketube button); while the pad is taller than that, the page gets the pad's height plus a margin,
    // so scrolled to the end nothing stays under it.
    var baseRoom = parseFloat(getComputedStyle(document.body).paddingBottom) || 0;
    function fitRoom() {
      var need = pad.getBoundingClientRect().height + 24;
      document.body.style.paddingBottom = need > baseRoom ? need + 'px' : '';
      // A bar pinned to the bottom of a page (the Messages composer) sits above the pad, not under it (2026-11-15: on a
      // phone the pad covered the message box and Send): --lg-pad-room is how much of the screen's bottom the pad takes
      // (0 with a mouse, where the pad is not shown).
      var kb = keys.getBoundingClientRect();
      document.documentElement.style.setProperty('--lg-pad-room', kb.height ? Math.max(0, Math.round(window.innerHeight - kb.top + 8)) + 'px' : '0px');
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
      if (len >= T.tapSlop) moved = true;
      if (len > T.reach) { dx = dx * T.reach / len; dy = dy * T.reach / len; }
      knob.style.left = Math.round(from.x - r.left + dx) + 'px';
      knob.style.top = Math.round(from.y - r.top + dy) + 'px';
      var v = pullVector(dx, dy);
      page.pull(v.x, v.y);
    }
    function release(tapOk) {
      if (grip === null) return;
      grip = null;
      knob.style.left = knob.style.top = '';
      touchpad.classList.remove('lg-here-touching');
      page.pull(0, 0);
      if (tapOk && !moved && Date.now() - downAt <= T.tapMs) page.select();   // a tap on the joystick is A
    }
    function letGo() { Object.keys(held).forEach(function (d) { hold(d, false); }); release(); }
    function back() {
      if (menuOpenNow()) { menuClose(true); return; }   // B with your drop's menu open: it closes
      if (editing()) document.activeElement.blur();   // B in a field: back to walking
      else page.back();
    }
    touchpad.addEventListener('pointerdown', function (e) {
      e.preventDefault();
      grip = e.pointerId;
      from = { x: e.clientX, y: e.clientY };
      downAt = Date.now(); moved = false;
      touchpad.classList.add('lg-here-touching');
      try { touchpad.setPointerCapture(e.pointerId); } catch (x) {} // err-ok: without capture it still follows over itself
      pull(e);
    });
    touchpad.addEventListener('pointermove', function (e) { if (e.pointerId === grip) pull(e); });
    // only a real pointerup can be a tap; cancel or a lost capture just lets go
    ['pointerup', 'pointercancel', 'lostpointercapture'].forEach(function (t) {
      touchpad.addEventListener(t, function (e) { if (e.pointerId === grip) release(t === 'pointerup'); });
    });

    document.addEventListener('keydown', function (e) {
      if (e.defaultPrevented) return;   // the focused element already took the key
      var act = keyAction(e);
      if (editing() || (menuOpenNow() && menuEl.contains(document.activeElement))) {
        if (act === 'back') { e.preventDefault(); back(); }
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
        try { sessionStorage.setItem(CONTROLS.portal.handoff, JSON.stringify({ box: page.box || '', t: Date.now() })); } catch (e) {} // err-ok: private mode: the map just starts at you
      });
    }
    window.addEventListener('blur', letGo);   // a key let go in another window never sticks
    window.addEventListener('resize', function () { clearJuketube(); fitRoom(); });
    clearJuketube();
    fitRoom();
    return {};
  }

  // ---- your drop's menu (Brandon 2026-11-14). One popup for the page, next to the drop that opened it; it closes on a
  // tap or click anywhere else, on B / Esc, or on the drop again. The switches are this file's own (CONTROLS.media below);
  // 'lg-media-change' fires on document when they change.
  var menuEl = null, menuFor = null;
  function menuOpenNow() { return !!menuEl && !menuEl.hidden; }
  function menuClose(refocus) {
    if (!menuOpenNow()) return;
    menuEl.hidden = true;
    if (menuFor) { menuFor.setAttribute('aria-expanded', 'false'); if (refocus && menuFor.focus) menuFor.focus(); }
  }
  function menuPaint() {
    if (!menuEl) return;
    var st = mediaOk ? mwant : null;
    Array.prototype.forEach.call(menuEl.querySelectorAll('[data-kind]'), function (b) {
      var k = b.getAttribute('data-kind'), on = !!(st && st[k]);
      b.hidden = !st;
      b.setAttribute('aria-pressed', on ? 'true' : 'false');
      b.lastChild.textContent = on ? 'On' : 'Off';
    });
  }
  function menuBuild() {
    var S = M.speed;
    menuEl = document.createElement('div');
    menuEl.id = M.id;
    menuEl.setAttribute('role', 'group');
    menuEl.setAttribute('aria-label', M.label);
    menuEl.hidden = true;
    var h = '';
    Object.keys(M.toggles).forEach(function (k) {
      h += '<button type="button" data-kind="' + k + '" aria-pressed="false"><span>' + M.toggles[k] + '</span><span></span></button>';
    });
    h += '<label class="lg-drop-speed"><span>' + S.label + ' <output>' + speedMul.toFixed(1) + 'x</output></span>' +
      '<input type="range" min="' + S.min + '" max="' + S.max + '" step="' + S.step + '" value="' + speedMul + '"></label>';
    menuEl.innerHTML = h;
    document.body.appendChild(menuEl);
    Array.prototype.forEach.call(menuEl.querySelectorAll('[data-kind]'), function (b) {
      b.addEventListener('click', function () { mediaToggle(b.getAttribute('data-kind')); menuPaint(); });
    });
    var range = menuEl.querySelector('input'), out = menuEl.querySelector('output');
    range.addEventListener('input', function () {
      var v = Math.min(S.max, Math.max(S.min, parseFloat(range.value) || 1));
      speedMul = v;
      out.textContent = v.toFixed(1) + 'x';
      try { localStorage.setItem(S.store, String(v)); } catch (e) {} // err-ok: no storage: it lasts this page view
    });
    document.addEventListener('pointerdown', function (e) {
      if (menuOpenNow() && !menuEl.contains(e.target) && !(menuFor && menuFor.contains(e.target))) menuClose(false);
    }, true);
    document.addEventListener('lg-media-change', menuPaint);
    window.addEventListener('resize', function () { if (menuOpenNow()) menuPlace(); });
  }
  // next to the drop: above it when there is room, else below; kept on the screen
  function menuPlace() {
    var r = menuFor.getBoundingClientRect(), w = menuEl.offsetWidth, h = menuEl.offsetHeight, gap = 10;
    var top = r.top - h - gap >= 8 ? r.top - h - gap : Math.min(window.innerHeight - h - 8, r.bottom + gap);
    var left = Math.max(8, Math.min(window.innerWidth - w - 8, (r.left + r.right) / 2 - w / 2));
    menuEl.style.left = Math.round(left) + 'px';
    menuEl.style.top = Math.round(Math.max(8, top)) + 'px';
  }
  function menuOpen(el) {
    if (!menuEl) menuBuild();
    if (menuFor && menuFor !== el) menuFor.setAttribute('aria-expanded', 'false');
    menuFor = el;
    menuEl.querySelector('input').value = speedMul;
    menuEl.querySelector('output').textContent = speedMul.toFixed(1) + 'x';
    menuPaint();
    menuEl.hidden = false;
    el.setAttribute('aria-expanded', 'true');
    menuPlace();
  }
  // menu(el): your drop el opens and closes the menu (tap, click, or Enter / Space when it has the focus)
  function menu(el) {
    if (!el || el.getAttribute('data-menu')) return;
    el.setAttribute('data-menu', '1');
    myDrop = el;
    selfPaint();
    el.setAttribute('role', 'button');
    el.setAttribute('tabindex', '0');
    el.setAttribute('aria-label', M.label);
    el.setAttribute('aria-haspopup', 'true');
    el.setAttribute('aria-expanded', 'false');
    el.style.pointerEvents = 'auto';
    el.style.cursor = 'pointer';
    function flip() { if (menuOpenNow() && menuFor === el) menuClose(false); else menuOpen(el); }
    el.addEventListener('click', function (e) { e.stopPropagation(); flip(); });
    el.addEventListener('keydown', function (e) {
      if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); flip(); }
    });
  }

  // The portal's arrival: {box} once (it is removed as it is read), or null (none, or older than handoffMs).
  function takeHandoff() {
    var P = CONTROLS.portal, v = null;
    try { v = JSON.parse(sessionStorage.getItem(P.handoff) || 'null'); sessionStorage.removeItem(P.handoff); } catch (e) { v = null; } // err-ok: unreadable = no arrival
    return v && typeof v.t === 'number' && Date.now() - v.t <= P.handoffMs ? { box: String(v.box || '') } : null;
  }

  // ---- walking a page (profiles and the home page): what you can use, using it, your speed, keeping you in view
  var USABLE = 'a[href],button,input:not([type=hidden]),select,textarea,summary,[role=button]';
  // nearest(cx, cy, prev): the usable thing (link, button, field) within CONTROLS.near px of the viewport point (cx, cy),
  // or null; it is marked lg-here-near (prev, the one marked before, is unmarked when it changes).
  function nearest(cx, cy, prev) {
    var all = document.querySelectorAll(USABLE), best = null, bestD = CONTROLS.near;
    for (var i = 0; i < all.length; i++) {
      var e = all[i];
      if (e.closest('#lg-here,#lg-here-pad')) continue;
      var r = e.getBoundingClientRect();
      if (!r.width || !r.height) continue;
      var dx = Math.max(r.left - cx, 0, cx - r.right), dy = Math.max(r.top - cy, 0, cy - r.bottom);
      var d = Math.sqrt(dx * dx + dy * dy);
      if (d <= bestD) { bestD = d; best = e; }
    }
    if (prev !== best) {
      if (prev) prev.classList.remove('lg-here-near');
      if (best) best.classList.add('lg-here-near');
    }
    return best;
  }
  // use(el): A on a thing: a field gets the cursor, anything else is clicked
  function use(el) {
    var t = el.tagName;
    if (t === 'INPUT' || t === 'TEXTAREA' || t === 'SELECT') el.focus();
    else el.click();
  }
  // ---- your camera and microphone (CONTROLS.media)
  var mediaOk = !!(navigator.mediaDevices && navigator.mediaDevices.getUserMedia);
  var mwant = { mic: false, cam: false, hear: false }, mstream = null, mfns = [], mtoken = 0, myDrop = null, selfWrap = null, hearEl = null;
  try {
    var mv = JSON.parse(localStorage.getItem(CONTROLS.media.store) || '{}') || {};
    ['mic', 'cam', 'hear'].forEach(function (k) { mwant[k] = mv[k] === true; });
  } catch (e) {} // err-ok: no storage: everything starts off
  function mediaSave() { try { localStorage.setItem(CONTROLS.media.store, JSON.stringify(mwant)); } catch (e) {} } // err-ok: it lasts this page view
  function mediaStop() { if (mstream) mstream.getTracks().forEach(function (t) { t.stop(); }); mstream = null; }
  function mediaChanged() {
    selfPaint();
    menuPaint();
    mfns.forEach(function (fn) { try { fn(mstream); } catch (e) { /* a listener's own fault */ } });
    document.dispatchEvent(new CustomEvent('lg-media-change', { detail: { mic: mwant.mic, cam: mwant.cam, hear: mwant.hear } }));
  }
  // (re)open the camera and microphone for what is switched on. by: the switch you just flipped (a refusal turns it
  // back off: that is a decision too), or '' when a page turns them back on as it loads (a refusal then leaves your
  // choice as it was, so the next page tries again).
  function mediaApply(by) {
    var token = ++mtoken;
    if (!mediaOk || (!mwant.mic && !mwant.cam)) { mediaStop(); mediaChanged(); return; }
    navigator.mediaDevices.getUserMedia({ audio: mwant.mic ? { echoCancellation: true, noiseSuppression: true } : false,
      video: mwant.cam ? { facingMode: 'user' } : false }).then(function (s) {
      if (token !== mtoken) { s.getTracks().forEach(function (t) { t.stop(); }); return; }
      mediaStop(); mstream = s; mediaChanged();
    }, function () {
      if (token !== mtoken) return;
      if (by) { mwant[by] = false; mediaSave(); }
      mediaStop(); mediaChanged();
    });
  }
  function mediaToggle(kind) {
    if (!mediaOk || !(kind in mwant)) return;
    mwant[kind] = !mwant[kind];
    mediaSave();
    if (kind === 'hear') { mediaChanged(); return; }
    mediaApply(kind);
  }
  // ---- sound ripples (CONTROLS.media.ripple). ripples(el, stream): rings spread from el's drop while stream's sound
  // is loud enough; one watcher per element (a new stream replaces the old; no stream stops it). Your own drop gets yours
  // (selfPaint); here_media.js gives each person in a call theirs.
  var rctx = null, rwatch = [];
  function audioCtx() {
    if (!rctx) {
      var AC = window.AudioContext || window.webkitAudioContext;
      if (!AC) return null;
      rctx = new AC();
      // a browser starts sound only after a tap or a key: the first one wakes it
      var wake = function () { if (rctx.state === 'suspended') rctx.resume(); };
      ['pointerdown', 'keydown'].forEach(function (t) { document.addEventListener(t, wake, true); });
    }
    if (rctx.state === 'suspended') rctx.resume().catch(function () { /* waits for a tap */ });
    return rctx;
  }
  function ripples(el, stream) {
    for (var i = rwatch.length - 1; i >= 0; i--) if (rwatch[i].el === el) { rwatch[i].stop(); rwatch.splice(i, 1); }
    if (!el || !stream || !stream.getAudioTracks().length || reducedMotion()) return;
    var ctx = audioCtx();
    if (!ctx) return;
    var R = CONTROLS.media.ripple, src = ctx.createMediaStreamSource(stream), an = ctx.createAnalyser();
    an.fftSize = 512;
    src.connect(an);   // read only: never connected to the speakers
    var buf = new Float32Array(an.fftSize), last = 0, raf = 0, w = { el: el };
    w.level = 0;
    function frame(now) {
      an.getFloatTimeDomainData(buf);
      var sum = 0;
      for (var k = 0; k < buf.length; k++) sum += buf[k] * buf[k];
      var level = Math.min(1, Math.sqrt(sum / buf.length) * R.gain);
      w.level = level;
      el.setAttribute('data-level', level.toFixed(2));
      var gap = R.minMs - (R.minMs - R.fastMs) * level;
      if (level > R.floor && now - last >= gap && el.isConnected) { last = now; ring(el, level); }
      raf = requestAnimationFrame(frame);
    }
    raf = requestAnimationFrame(frame);
    w.stop = function () { cancelAnimationFrame(raf); try { src.disconnect(); } catch (e) { /* gone */ } el.removeAttribute('data-level'); };
    rwatch.push(w);
  }
  function ring(el, level) {
    var R = CONTROLS.media.ripple, host = el.parentNode;
    if (!host || !el.animate) return;
    var r = document.createElement('span'), size = el.offsetWidth || 38;
    r.className = R.cls;
    r.setAttribute('aria-hidden', 'true');
    r.style.width = r.style.height = size + 'px';
    r.style.left = (el.offsetLeft + el.offsetWidth / 2) + 'px';
    r.style.top = (el.offsetTop + el.offsetHeight / 2) + 'px';
    host.appendChild(r);
    var to = R.reach + level * R.spread, a = Math.min(0.9, 0.25 + level);
    var anim = r.animate([
      { transform: 'translate(-50%,-50%) scale(1)', opacity: a, borderWidth: (1 + level * 3).toFixed(1) + 'px' },
      { transform: 'translate(-50%,-50%) scale(' + to.toFixed(2) + ')', opacity: 0, borderWidth: '0.5px' }
    ], { duration: R.ms, easing: 'cubic-bezier(.2,.6,.35,1)' });
    anim.onfinish = function () { r.remove(); };
  }

  // your own camera as a mirrored bubble on your drop (taps go through to the drop: its menu), and your own sound when
  // "Hear myself" is on
  function selfPaint() {
    var vids = mstream ? mstream.getVideoTracks() : [];
    if (myDrop && vids.length && mwant.cam) {
      if (!selfWrap) {
        selfWrap = document.createElement('div');
        selfWrap.className = 'lg-media-bubble ' + CONTROLS.media.selfCls;
        selfWrap.setAttribute('aria-hidden', 'true');
        var v = document.createElement('video');
        v.muted = true; v.autoplay = true; v.playsInline = true;
        selfWrap.appendChild(v);
      }
      var vid = selfWrap.firstChild;
      if (vid.srcObject !== mstream) vid.srcObject = mstream;
      if (selfWrap.parentNode !== myDrop.parentNode && myDrop.parentNode) myDrop.parentNode.appendChild(selfWrap);
      var pl = vid.play(); if (pl && pl.catch) pl.catch(function () { /* the browser will not autoplay yet */ });
    } else if (selfWrap && selfWrap.parentNode) {
      selfWrap.firstChild.srcObject = null;
      selfWrap.parentNode.removeChild(selfWrap);
    }
    var auds = mstream ? mstream.getAudioTracks() : [];
    // your sound ripples off your drop while the microphone is on (only for you to see unless you are in a call)
    var rippling = rwatch.filter(function (x) { return x.el === myDrop; })[0];
    if (myDrop && mwant.mic && auds.length) { if (!rippling || rippling.stream !== mstream) { ripples(myDrop, mstream); var nw = rwatch[rwatch.length - 1]; if (nw && nw.el === myDrop) nw.stream = mstream; } }
    else if (rippling) ripples(myDrop, null);
    if (mwant.hear && mwant.mic && auds.length) {
      if (!hearEl) { hearEl = document.createElement('audio'); hearEl.autoplay = true; }
      if (!hearEl.srcObject || hearEl.srcObject.getAudioTracks()[0] !== auds[0]) hearEl.srcObject = new MediaStream(auds);
      var ph = hearEl.play(); if (ph && ph.catch) ph.catch(function () { /* the browser will not autoplay yet */ });
    } else if (hearEl) {
      hearEl.pause();
      hearEl.srcObject = null;
    }
  }
  if (mediaOk && (mwant.mic || mwant.cam)) setTimeout(function () { mediaApply(''); }, 0);   // on as you left them
  window.addEventListener('pagehide', mediaStop);   // the camera light goes out with the page; the next page turns it back on

  // velocity(held, pull): keys walk at full speed; else the touchpad at its pull (0..1 of full speed); both times the
  // drip speed from your drop's menu (1x unless you changed it)
  function velocity(held, pull) {
    var x = (held.right ? 1 : 0) - (held.left ? 1 : 0), y = (held.down ? 1 : 0) - (held.up ? 1 : 0);
    return x || y ? { x: x * speedMul, y: y * speedMul } : { x: pull.x * speedMul, y: pull.y * speedMul };
  }
  // centerOn(left, top): scrolls so the page point (px from the page's left and top) is in the middle of the screen,
  // as far as the page's ends allow (after every move: the screen always follows your drop)
  function centerOn(left, top) {
    window.scrollTo(Math.round(left - window.innerWidth / 2), Math.round(top - window.innerHeight / 2));
  }
  function reducedMotion() { return !!(window.matchMedia && matchMedia('(prefers-reduced-motion: reduce)').matches); }

  // hop(el): A with nothing next to you: the drop el squishes, splats and bounces (CONTROLS.hop). Counts in data-hops.
  function hop(el) {
    if (!el) return;
    el.setAttribute('data-hops', String(+(el.getAttribute('data-hops') || 0) + 1));
    if (reducedMotion() || !el.animate) return;
    var P = CONTROLS.hop, c = P.crouch, s = P.splat, g = P.grow;
    el.animate([{ scale: '1 1' }, { scale: c[0] + ' ' + c[1], offset: 0.22 }, { scale: s[0] + ' ' + s[1], offset: 0.4 },
      { scale: g + ' ' + g, offset: 0.7 }, { scale: '1 1' }], { duration: P.ms, easing: 'ease-out' });
    var mark = document.createElement('span');
    mark.className = P.splatCls;
    el.appendChild(mark);
    var a = mark.animate([{ opacity: 0, scale: '0.4' }, { opacity: 0.7, scale: '1', offset: 0.4 }, { opacity: 0, scale: '1.3' }],
      { duration: P.ms, easing: 'ease-out' });
    a.onfinish = function () { mark.remove(); };
  }

  // portrait(name): your drop: a tail (hidden at rest) behind your round portrait (its first letter if there is none)
  function portrait(name) {
    var S = CONTROLS.shape, el = document.createElement('span'), tail = document.createElement('span'), img = document.createElement('img');
    el.className = S.cls;
    tail.className = S.tailCls;
    el.style.width = el.style.height = S.size + 'px';
    el.style.setProperty('--lg-drop-settle', S.settleMs + 'ms');
    el.appendChild(tail);
    if (!name) {   // no account (the home page): a plain drop
      var plain = document.createElement('span');
      plain.className = S.phCls;
      el.appendChild(plain);
      shape(el, 0, 0);
      return el;
    }
    img.alt = '';
    img.src = '/portrait/' + encodeURIComponent(name || '');
    img.onerror = function () {
      var ph = document.createElement('span');
      ph.className = S.phCls;
      ph.textContent = String(name || '?').charAt(0).toUpperCase();
      if (img.parentNode) img.parentNode.replaceChild(ph, img);
    };
    el.appendChild(img);
    shape(el, 0, 0);
    return el;
  }
  // shape(el, x, y): the drop for a velocity (x, y) as a fraction of top speed (keys: 1; the touchpad: its pull).
  // Transforms only (no layout reads): the shell stretches along the motion by up to stretch, the tail grows on the
  // side opposite the motion up to tail x the drop's size; at rest (and with reduced motion) a plain circle. CSS
  // eases each change over settleMs, so stopping melts back to round.
  function shape(el, x, y) {
    if (!el) return;
    var S = CONTROLS.shape, speed = Math.min(1, Math.sqrt(x * x + y * y));
    if (reducedMotion()) speed = 0;
    var angle = speed ? Math.round(Math.atan2(y, x) * 180 / Math.PI) : +(el.getAttribute('data-angle') || 0);
    var key = angle + ':' + speed.toFixed(2);
    if (el.getAttribute('data-shape') === key) return;   // unchanged: no style writes
    el.setAttribute('data-shape', key);
    el.setAttribute('data-angle', angle);
    el.setAttribute('data-speed', speed.toFixed(2));
    el.style.transform = 'rotate(' + angle + 'deg) scaleX(' + (1 + S.stretch * speed).toFixed(3) + ') rotate(' + (-angle) + 'deg)';
    el.firstChild.style.transform = 'rotate(' + (angle + 180) + 'deg) scaleX(' + (S.tail * speed).toFixed(3) + ')';
  }

  window.LG_WALKER = { CONTROLS: CONTROLS, portrait: portrait, shape: shape, hop: hop, keyAction: keyAction, isDir: isDir, editing: editing,
    pullVector: pullVector, mount: mount, takeHandoff: takeHandoff, nearest: nearest, use: use, velocity: velocity, centerOn: centerOn,
    menu: menu, menuClose: menuClose,
    media: { state: function () { return { mic: mwant.mic, cam: mwant.cam, hear: mwant.hear }; },
      stream: function () { return mstream; }, onChange: function (fn) { mfns.push(fn); }, toggle: mediaToggle,
      hearing: function () { return !!(hearEl && hearEl.srcObject && !hearEl.paused); } },
    ripples: ripples,
    reducedMotion: reducedMotion };
})();
