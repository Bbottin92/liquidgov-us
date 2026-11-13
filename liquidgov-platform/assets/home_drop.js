/* Drop on the home page (Brandon 2026-11-13 4:03 AM): the hook. The page starts as usual (Scroll); the Drop/Scroll
   switch pulses and the first-time note ("Stop, Drop & Scroll", tip.js) points at it. Dropped in, your drop (a plain
   one: no account here) starts just under the switch and a guide points you down the page; past the scrolling-words
   section the floor is a moving sidewalk (with it you go faster, against it slower). A next to a link or button uses
   it; A with nothing next to you is a squish-and-bounce. Every number and name is walker.js's CONTROLS (home, hop,
   near, maxSpeed, shape); the switch, touchpad, keys, note, shape and hop are walker.js's too: this file only walks
   the drop on this page. Positions are page px (one visitor, nothing is sent anywhere). */
(function () {
  var W = window.LG_WALKER;
  if (!W) return;
  var C = W.CONTROLS, H = C.home, MAX_SPEED = C.maxSpeed;

  var layer = document.createElement('div');
  layer.id = 'lg-here';
  layer.setAttribute('aria-hidden', 'true');
  layer.hidden = true;
  var dot = document.createElement('div');
  dot.className = 'lg-here-dot me';
  var drop = W.portrait('');
  dot.appendChild(drop);
  var guide = document.createElement('div');
  guide.className = H.guide.cls;
  guide.textContent = H.guide.text;
  var belt = document.createElement('div');
  belt.className = H.belt.cls;
  // the stripes flow the sidewalk's way (one stripe, 32 px, a loop)
  belt.style.setProperty('--lg-belt-x', H.belt.dir.x * 32 + 'px');
  belt.style.setProperty('--lg-belt-y', H.belt.dir.y * 32 + 'px');
  layer.appendChild(guide);
  layer.appendChild(dot);
  // the sidewalk is the floor: behind the page's own content (its cards cover it), not in the drops' layer above it
  belt.hidden = true;
  document.body.appendChild(belt);
  document.body.appendChild(layer);

  var me = { x: 0, y: 0 }, held = {}, pull = { x: 0, y: 0 }, near = null, dropping = false, beltTop = Infinity;

  function pageW() { return Math.max(document.documentElement.scrollWidth, document.body.scrollWidth); }
  function pageH() { return Math.max(document.documentElement.scrollHeight, document.body.scrollHeight); }
  // the sidewalk: from the bottom of the scrolling-words section to the page's end
  function measure() {
    layer.style.height = pageH() + 'px';
    var s = document.querySelector(H.belt.after);
    beltTop = s ? Math.round(s.getBoundingClientRect().bottom + window.scrollY) : Infinity;
    belt.style.top = (isFinite(beltTop) ? beltTop : pageH()) + 'px';
    belt.style.height = Math.max(0, pageH() - beltTop) + 'px';
  }
  function place() {
    dot.style.left = me.x + 'px';
    dot.style.top = me.y + 'px';
    guide.style.left = me.x + 'px';
    guide.style.top = me.y + 'px';
    guide.hidden = me.y >= beltTop;   // past the words, the sidewalk takes over the guiding
  }
  function findNear() { near = W.nearest(me.x - window.scrollX, me.y - window.scrollY, near); }
  // onBelt(v): the sidewalk adds boost x your own speed along its direction (so with it 1 + boost, against 1 - boost)
  function onBelt(v) {
    if (me.y < beltTop || (!v.x && !v.y)) return v;
    var d = H.belt.dir, len = Math.sqrt(v.x * v.x + v.y * v.y);
    return { x: v.x + d.x * H.belt.boost * len, y: v.y + d.y * H.belt.boost * len };
  }
  function step() {
    if (!dropping) return;
    var v = onBelt(W.velocity(held, pull));
    W.shape(drop, v.x, v.y);
    if (!v.x && !v.y) return;
    me.x = Math.max(0, Math.min(pageW(), me.x + v.x * MAX_SPEED));
    me.y = Math.max(0, Math.min(pageH(), me.y + v.y * MAX_SPEED));
    place();
    W.follow(me.y);
    findNear();
  }
  setInterval(step, 40);

  W.mount({
    invite: true,
    dir: function (dir, down) { held[dir] = down; },
    pull: function (x, y) { pull = { x: x, y: y }; },
    // A: use what you stand next to; nothing there: squish and bounce (A was still yours, so the key is taken)
    select: function () { if (near) W.use(near); else W.hop(drop); return true; },
    back: function () { if (history.length > 1) history.back(); },
    dropping: function (on) {
      dropping = on;
      held = {};
      pull = { x: 0, y: 0 };
      layer.hidden = belt.hidden = !on;
      if (on) {
        measure();
        // just under the switch and the sticky header (the drop walks under the header, so it starts below it)
        var sw = document.querySelector('.' + C.buttons.drop.cls).getBoundingClientRect(), hd = document.querySelector('header.top');
        var below = Math.max(sw.bottom, hd ? hd.getBoundingClientRect().bottom : 0);
        me = { x: Math.round(sw.left + sw.width / 2 + window.scrollX), y: Math.round(below + H.startGap + window.scrollY) };
        place();
        findNear();
      } else if (near) { near.classList.remove('lg-here-near'); near = null; }
    },
    resized: measure,
    box: ''
  });
  window.addEventListener('resize', measure);
})();
