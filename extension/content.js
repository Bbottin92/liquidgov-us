/* Drops on this page. Draws only its own layer (a closed shadow root over the page); it never reads, changes or sends
   anything from the page. Your drop can be dragged; others' drops move as they move theirs. Positions are fractions of
   the window, so people with different window sizes see roughly the same arrangement. */
(function () {
  if (window.__lgDrops) return;
  window.__lgDrops = true;

  const host = document.createElement('div');
  host.style.cssText = 'all:initial;position:fixed;inset:0;pointer-events:none;z-index:2147483646';
  const root = host.attachShadow({ mode: 'closed' });
  root.innerHTML = `<style>
    .d{position:fixed;width:30px;height:36px;margin:-18px 0 0 -15px;pointer-events:auto;transition:left .2s linear,top .2s linear;
       font:600 12px/1 system-ui,sans-serif;color:#fff;display:flex;align-items:center;justify-content:center;padding-top:6px;box-sizing:border-box;
       border-radius:50% 50% 50% 50%/60% 60% 40% 40%;clip-path:path('M15 0 C15 0 30 18 30 24 A15 12 0 0 1 0 24 C0 18 15 0 15 0Z');
       background:radial-gradient(circle at 40% 55%,rgba(255,255,255,.55),transparent 35%),var(--c);opacity:.9}
    .me{cursor:grab;transition:none;outline:none;z-index:2}
    .me.drag{cursor:grabbing}
    .n{position:fixed;transform:translate(-50%,14px);font:12px system-ui,sans-serif;background:rgba(15,23,42,.8);color:#fff;
       padding:2px 6px;border-radius:6px;white-space:nowrap;pointer-events:none}
    .note{position:fixed;right:12px;bottom:12px;font:13px system-ui,sans-serif;background:rgba(15,23,42,.88);color:#fff;
       padding:8px 10px;border-radius:8px;max-width:260px;pointer-events:auto}
    .note a{color:#7dd3fc}
  </style><div id="w"></div>`;
  const wrap = root.getElementById('w');
  document.documentElement.appendChild(host);

  function colour(name) { let h = 0; for (const c of name) h = (h * 31 + c.charCodeAt(0)) >>> 0; return `hsl(${h % 360} 65% 45%)`; }
  const me = { x: 0.7 + Math.random() * 0.25, y: 0.75 + Math.random() * 0.15 };   // spread out, so drops do not start stacked
  let you = '', dots = {};
  const mine = document.createElement('div');
  mine.className = 'd me';
  mine.tabIndex = 0;
  mine.title = 'You (drag to move)';
  mine.style.setProperty('--c', '#2563eb');
  wrap.appendChild(mine);
  function place(el, x, y) { el.style.left = (x * innerWidth) + 'px'; el.style.top = (y * innerHeight) + 'px'; }
  place(mine, me.x, me.y);

  let port = null, note = null;
  function say(html) {
    if (note) note.remove();
    note = null;
    if (!html) return;
    note = document.createElement('div');
    note.className = 'note';
    note.innerHTML = html;   // fixed text from this file only
    wrap.appendChild(note);
  }
  function send() { if (port) port.postMessage({ x: me.x, y: me.y }); }
  function onSnap(s) {
    you = s.you || you;
    const seen = {};
    (s.people || []).forEach(p => {
      if (p.u === you || typeof p.u !== 'string') return;
      seen[p.u] = 1;
      let d = dots[p.u];
      if (!d) {
        d = dots[p.u] = { el: document.createElement('div'), n: document.createElement('div') };
        d.el.className = 'd';
        d.el.style.setProperty('--c', colour(p.u));
        d.el.textContent = p.u.slice(0, 1).toUpperCase();
        d.n.className = 'n';
        d.n.textContent = p.u;
        wrap.appendChild(d.el); wrap.appendChild(d.n);
      }
      place(d.el, +p.x || 0, +p.y || 0); place(d.n, +p.x || 0, +p.y || 0);
    });
    Object.keys(dots).forEach(u => { if (!seen[u]) { dots[u].el.remove(); dots[u].n.remove(); delete dots[u]; } });
  }
  function clearDots() { Object.keys(dots).forEach(u => { dots[u].el.remove(); dots[u].n.remove(); }); dots = {}; }
  function connect() {
    port = chrome.runtime.connect({ name: 'lg-drops' });
    port.onMessage.addListener(m => {
      if (m.snap) onSnap(m.snap);
      else if (m.state === 'open') { say(''); send(); }
      else if (m.state === 'refused') { clearDots(); say('Drops need you signed in at <a href="https://liquidgov.us/login" target="_blank" rel="noopener">liquidgov.us</a> with "Show me where I am" turned on in Settings.'); }
      else if (m.state === 'blocked') { clearDots(); host.style.display = 'none'; }
      else if (m.state === 'closed') { clearDots(); setTimeout(() => port && port.postMessage({ moved: true }), 3000); }
    });
    port.onDisconnect.addListener(() => { port = null; clearDots(); });
    port.postMessage({ hello: true });
  }
  connect();

  // single-page sites change the address without reloading: a new address is a new room
  let lastHref = location.href;
  const watch = setInterval(() => {
    if (location.href === lastHref) return;
    lastHref = location.href;
    clearDots(); host.style.display = '';
    if (port) port.postMessage({ moved: true });
  }, 1000);

  // drag your drop
  let drag = null;
  mine.addEventListener('pointerdown', e => { drag = e.pointerId; mine.setPointerCapture(e.pointerId); mine.classList.add('drag'); e.preventDefault(); });
  mine.addEventListener('pointermove', e => {
    if (drag !== e.pointerId) return;
    me.x = Math.min(1, Math.max(0, e.clientX / innerWidth)); me.y = Math.min(1, Math.max(0, e.clientY / innerHeight));
    place(mine, me.x, me.y); send();
  });
  mine.addEventListener('pointerup', () => { drag = null; mine.classList.remove('drag'); });
  mine.addEventListener('keydown', e => {
    const k = { ArrowLeft: [-1, 0], ArrowRight: [1, 0], ArrowUp: [0, -1], ArrowDown: [0, 1] }[e.key];
    if (!k) return;
    e.preventDefault(); e.stopPropagation();
    me.x = Math.min(1, Math.max(0, me.x + k[0] * 0.02)); me.y = Math.min(1, Math.max(0, me.y + k[1] * 0.02));
    place(mine, me.x, me.y); send();
  });
  addEventListener('resize', () => { place(mine, me.x, me.y); });
  const keepAlive = setInterval(send, 20000);   // an idle socket stays open

  chrome.runtime.onMessage.addListener(m => {
    if (m && m.lg === 'off') {
      clearInterval(watch); clearInterval(keepAlive);
      if (port) port.disconnect();
      port = null; host.remove(); window.__lgDrops = false;
    }
  });
})();
