/* Edit mode (q294, Brandon 2026-11-21: "draw an x in an area or over an object or in a certain shape to indicate to
   you thats the spot im referring to ... a number assigned to it followed by a comment line in an area on the right
   side of the screen ... so i can make more specific edits in real time").

   A DEVELOPER TOOL, not part of the site: it lives in tools/, is never served or deployed, and changes nothing on the
   server. Claude injects it into the page open in the Claude app's browser pane (live liquidgov.us or a local
   server); it works on any page. Everything it makes lives in the page until the tab navigates away.

   Using it: the "Draw" pill (bottom left; or Alt+Shift+E) turns drawing on. Draw an X, circle or any shape over a
   spot, or tap for a dot. Each mark gets a number and a comment line in the panel on the right (a bottom sheet on a
   phone). Two strokes drawn quickly over the same spot (an X) are one mark. Escape or the pill turns drawing off so
   the page can be used again; the marks stay. Numbers never shift: deleting mark 2 leaves 1 and 3.

   For Claude: window.LG_EDIT.report() is a JSON string: the page url and size, and per mark its number, comment,
   shape (strokes in page px), box (page px and as seen now), the element under its middle (a CSS selector, tag and
   text) and the elements inside it. Marks are also kept in this browser's localStorage per page path, so injecting
   again after a reload brings them back.

   CSP-safe on purpose: no <style> element, no inline handlers, no network; every style is set through element.style
   (a strict style-src cannot block that). */
(function () {
  var VERSION = 1;
  if (window.LG_EDIT && window.LG_EDIT.version === VERSION) { window.LG_EDIT.show(); return; }
  if (window.LG_EDIT && window.LG_EDIT.destroy) window.LG_EDIT.destroy();   // an older copy: replace it

  var Z = 2147483000, RED = '#ff3b30', MERGE_MS = 1200, DOT_PX = 6, PANEL_W = 320, NARROW = 700;
  var STORE = 'lg-edit-marks:' + location.pathname;
  var marks = [], drawing = false, stroke = null, lastEnd = 0, hot = 0, updated = Date.now();

  function css(el, s) { for (var k in s) el.style[k] = s[k]; return el; }
  function make(tag, s, text) { var e = document.createElement(tag); if (s) css(e, s); if (text != null) e.textContent = text; return e; }
  var FONT = '13px/1.35 system-ui, -apple-system, "Segoe UI", Roboto, sans-serif';

  // ---- the layers: a canvas over the page for the marks, the pill, the panel
  var root = make('div', { position: 'fixed', inset: '0', zIndex: String(Z), pointerEvents: 'none', font: FONT });
  root.id = 'lg-edit-root';
  root.setAttribute('data-lg-edit', '1');
  var cv = make('canvas', { position: 'absolute', left: '0', top: '0', width: '100%', height: '100%', pointerEvents: 'none',
    touchAction: 'none', cursor: 'crosshair' });
  var pill = make('button', { position: 'absolute', left: '12px', bottom: '12px', pointerEvents: 'auto', font: '600 13px system-ui, sans-serif',
    padding: '8px 14px', borderRadius: '999px', border: '1px solid rgba(0,0,0,.25)', background: '#fff', color: '#111',
    boxShadow: '0 2px 10px rgba(0,0,0,.3)', cursor: 'pointer' });
  pill.type = 'button';
  var panel = make('div', { position: 'absolute', pointerEvents: 'auto', background: '#fff', color: '#111', display: 'flex',
    flexDirection: 'column', boxShadow: '0 0 18px rgba(0,0,0,.35)', boxSizing: 'border-box' });
  var head = make('div', { display: 'flex', alignItems: 'center', gap: '8px', padding: '10px 12px', borderBottom: '1px solid #ddd' });
  var title = make('div', { fontWeight: '700', flex: '1' }, 'Edit mode');
  var clearBtn = make('button', { font: FONT, padding: '4px 10px', border: '1px solid #ccc', borderRadius: '6px', background: '#f6f6f6', cursor: 'pointer' }, 'Clear all');
  var hideBtn = make('button', { font: FONT, padding: '4px 10px', border: '1px solid #ccc', borderRadius: '6px', background: '#f6f6f6', cursor: 'pointer' }, 'Hide');
  clearBtn.type = hideBtn.type = 'button';
  var hint = make('div', { padding: '8px 12px', color: '#555', borderBottom: '1px solid #eee' });
  var list = make('div', { overflowY: 'auto', flex: '1', padding: '6px 0' });
  head.appendChild(title); head.appendChild(clearBtn); head.appendChild(hideBtn);
  panel.appendChild(head); panel.appendChild(hint); panel.appendChild(list);
  root.appendChild(cv); root.appendChild(panel); root.appendChild(pill);
  document.documentElement.appendChild(root);
  var ctx = cv.getContext('2d');

  function narrow() { return innerWidth < NARROW; }
  function layout() {
    if (narrow()) css(panel, { left: '0', right: '0', bottom: '0', top: 'auto', width: 'auto', height: '38vh', borderRadius: '12px 12px 0 0' });
    else css(panel, { right: '0', top: '0', bottom: '0', left: 'auto', width: PANEL_W + 'px', height: 'auto', borderRadius: '0' });
    css(pill, { bottom: narrow() && panel.style.display !== 'none' ? 'calc(38vh + 12px)' : '12px' });
    var dpr = window.devicePixelRatio || 1;
    cv.width = Math.round(innerWidth * dpr); cv.height = Math.round(innerHeight * dpr);
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    draw();
  }

  // ---- marks: points are page px (scroll-independent); the canvas shows them at the current scroll
  function boxOf(strokes) {
    var b = { l: Infinity, t: Infinity, r: -Infinity, b: -Infinity };
    strokes.forEach(function (s) { s.forEach(function (p) { b.l = Math.min(b.l, p[0]); b.t = Math.min(b.t, p[1]); b.r = Math.max(b.r, p[0]); b.b = Math.max(b.b, p[1]); }); });
    return b;
  }
  function isDot(m) { var b = boxOf(m.strokes); return b.r - b.l < DOT_PX && b.b - b.t < DOT_PX; }
  function near(a, b, pad) { return a.l - pad <= b.r && b.l - pad <= a.r && a.t - pad <= b.b && b.t - pad <= a.b; }

  function draw() {
    ctx.clearRect(0, 0, innerWidth, innerHeight);
    var sx = scrollX, sy = scrollY;
    marks.concat(stroke ? [{ n: 0, strokes: [stroke] }] : []).forEach(function (m) {
      var w = m.n === hot ? 5 : 3.5;
      if (m.n && isDot(m)) {
        var p = m.strokes[0][0];
        ring(p[0] - sx, p[1] - sy, 11, w);
      } else {
        m.strokes.forEach(function (s) {
          [['#fff', w + 3], [RED, w]].forEach(function (pass) {
            ctx.strokeStyle = pass[0]; ctx.lineWidth = pass[1]; ctx.lineCap = ctx.lineJoin = 'round';
            ctx.beginPath();
            s.forEach(function (p, i) { if (i) ctx.lineTo(p[0] - sx, p[1] - sy); else ctx.moveTo(p[0] - sx, p[1] - sy); });
            if (s.length === 1) ctx.lineTo(s[0][0] - sx + 0.1, s[0][1] - sy);
            ctx.stroke();
          });
        });
      }
      if (m.n) badge(m);
    });
  }
  function ring(x, y, r, w) {
    [['#fff', w + 3], [RED, w]].forEach(function (pass) { ctx.strokeStyle = pass[0]; ctx.lineWidth = pass[1]; ctx.beginPath(); ctx.arc(x, y, r, 0, 7); ctx.stroke(); });
  }
  function badge(m) {
    var b = boxOf(m.strokes), x = b.l - scrollX - 4, y = b.t - scrollY - 4, r = 11;
    x = Math.max(r + 2, x); y = Math.max(r + 2, y);
    ctx.fillStyle = RED; ctx.strokeStyle = '#fff'; ctx.lineWidth = 2;
    ctx.beginPath(); ctx.arc(x, y, r, 0, 7); ctx.fill(); ctx.stroke();
    ctx.fillStyle = '#fff'; ctx.font = '700 12px system-ui, sans-serif'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
    ctx.fillText(String(m.n), x, y + 0.5);
  }

  // ---- what is under a mark: the element at its middle, and the elements whose middle lies inside it
  function selector(el) {
    if (!el || el.nodeType !== 1) return '';
    var parts = [];
    for (var e = el, depth = 0; e && e.nodeType === 1 && depth < 5; e = e.parentElement, depth++) {
      if (e.id) { parts.unshift('#' + (window.CSS && CSS.escape ? CSS.escape(e.id) : e.id)); break; }
      var s = e.tagName.toLowerCase();
      var cls = [].filter.call(e.classList || [], function (c) { return c.indexOf('lg-edit') !== 0; }).slice(0, 2);
      if (cls.length) s += '.' + cls.map(function (c) { return window.CSS && CSS.escape ? CSS.escape(c) : c; }).join('.');
      var p = e.parentElement;
      if (p) {
        var same = [].filter.call(p.children, function (c) { return c.tagName === e.tagName; });
        if (same.length > 1) s += ':nth-of-type(' + (same.indexOf(e) + 1) + ')';
      }
      parts.unshift(s);
      if (e === document.body) break;
    }
    return parts.join(' > ');
  }
  function textOf(el) {
    var t = (el.getAttribute && (el.getAttribute('aria-label') || el.getAttribute('alt') || el.getAttribute('title'))) ||
      (el.value && typeof el.value === 'string' ? el.value : '') || (el.innerText || el.textContent || '');
    t = String(t).replace(/\s+/g, ' ').trim();
    return t.length > 80 ? t.slice(0, 77) + '...' : t;
  }
  function describe(el) { return el ? { selector: selector(el), tag: el.tagName.toLowerCase(), text: textOf(el) } : null; }
  function mine(el) { return !!el && (el === root || root.contains(el)); }
  function at(x, y) {   // viewport px
    var els = document.elementsFromPoint ? document.elementsFromPoint(x, y) : [];
    var hit = els.filter(function (e) { return !mine(e); });
    var good = hit.filter(function (e) { return e !== document.documentElement && e !== document.body; });
    return good[0] || hit[0] || null;
  }
  // Where an element shows: its text's extent when it holds text (a full-width <li> or <p> reads as where its words
  // are, so a loop drawn around a list's words counts its rows), else its box.
  function shownRect(e) {
    var r = e.getBoundingClientRect();
    if (!r.width || !r.height || !(e.textContent || '').trim() || /^(INPUT|SELECT|TEXTAREA|IMG|SVG|VIDEO)$/i.test(e.tagName)) return r;
    try {
      var rg = document.createRange();
      rg.selectNodeContents(e);
      var t = rg.getBoundingClientRect();
      return t.width && t.height ? t : r;
    } catch (x) { return r; }   // err-ok: no Range support: the element's own box
  }
  function inside(box) {   // page px box; the visible elements whose shown middle is in it (at most 6)
    var out = [], sel = 'a,button,input,select,textarea,img,svg,video,label,h1,h2,h3,h4,p,li,[role],[data-id]';
    [].forEach.call(document.querySelectorAll(sel), function (e) {
      if (out.length >= 6 || mine(e)) return;
      var r = shownRect(e);
      if (!r.width || !r.height) return;
      var cx = r.left + r.width / 2 + scrollX, cy = r.top + r.height / 2 + scrollY;
      if (cx >= box.l && cx <= box.r && cy >= box.t && cy <= box.b) out.push(describe(e));
    });
    return out;
  }
  function locate(m) {
    var b = boxOf(m.strokes), cx = (b.l + b.r) / 2 - scrollX, cy = (b.t + b.b) / 2 - scrollY;
    m.target = describe(at(cx, cy));
    m.inside = isDot(m) ? [] : inside(b);
  }

  // ---- drawing
  function pt(e) { return [Math.round(e.clientX + scrollX), Math.round(e.clientY + scrollY)]; }
  cv.addEventListener('pointerdown', function (e) {
    if (!drawing || e.button > 0) return;
    e.preventDefault();
    try { cv.setPointerCapture(e.pointerId); } catch (x) { /* err-ok: capture is a nicety; the stroke still works */ }
    stroke = [pt(e)];
    draw();
  });
  cv.addEventListener('pointermove', function (e) {
    if (!stroke) return;
    var p = pt(e), q = stroke[stroke.length - 1];
    if (Math.abs(p[0] - q[0]) + Math.abs(p[1] - q[1]) >= 2) { stroke.push(p); draw(); }
  });
  function endStroke() {
    if (!stroke) return;
    var s = stroke, sb = boxOf([s]), last = marks[marks.length - 1];
    stroke = null;
    if (last && Date.now() - lastEnd < MERGE_MS && near(sb, boxOf(last.strokes), 12)) {
      last.strokes.push(s);   // the second line of an X: same mark
      locate(last);
      changed(last.n);
    } else {
      var m = { n: marks.reduce(function (a, x) { return Math.max(a, x.n); }, 0) + 1, strokes: [s], comment: '', made: Date.now() };
      locate(m);
      marks.push(m);
      changed(m.n, true);
    }
    lastEnd = Date.now();
  }
  cv.addEventListener('pointerup', endStroke);
  cv.addEventListener('pointercancel', endStroke);

  // ---- the panel
  function row(m) {
    var r = make('div', { display: 'flex', gap: '8px', padding: '8px 12px', alignItems: 'flex-start', borderBottom: '1px solid #f0f0f0' });
    r.setAttribute('data-n', String(m.n));
    var n = make('div', { flex: '0 0 auto', width: '22px', height: '22px', borderRadius: '50%', background: RED, color: '#fff',
      font: '700 12px/22px system-ui, sans-serif', textAlign: 'center' }, String(m.n));
    var mid = make('div', { flex: '1', minWidth: '0' });
    var ta = make('textarea', { width: '100%', boxSizing: 'border-box', minHeight: '38px', font: FONT, padding: '5px 7px',
      border: '1px solid #ccc', borderRadius: '6px', resize: 'vertical' });
    ta.rows = 2;
    ta.placeholder = 'What should change here?';
    ta.value = m.comment || '';
    ta.addEventListener('input', function () { m.comment = ta.value; save(); });
    var t = m.target, what = t ? (t.text ? '"' + t.text + '" ' : '') + '(' + (t.selector || t.tag) + ')' : 'nothing under it';
    var info = make('div', { color: '#777', fontSize: '11px', marginTop: '3px', overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }, what);
    info.title = what;
    var del = make('button', { flex: '0 0 auto', border: '0', background: 'transparent', color: '#999', font: '700 16px system-ui', cursor: 'pointer', padding: '0 2px' }, '×');
    del.type = 'button';
    del.title = 'Delete mark ' + m.n;
    del.addEventListener('click', function () { remove(m.n); });
    r.addEventListener('mouseenter', function () { hot = m.n; draw(); });
    r.addEventListener('mouseleave', function () { hot = 0; draw(); });
    mid.appendChild(ta); mid.appendChild(info);
    r.appendChild(n); r.appendChild(mid); r.appendChild(del);
    return r;
  }
  function render(focusN) {
    list.textContent = '';
    marks.forEach(function (m) { list.appendChild(row(m)); });
    title.textContent = 'Edit mode · ' + marks.length + ' mark' + (marks.length === 1 ? '' : 's');
    hint.textContent = drawing ? 'Draw an X or a shape over a spot, or tap for a dot. Escape stops drawing.'
      : marks.length ? 'Drawing is off: the page works as usual. Press Draw to add marks.' : 'Press Draw, then mark a spot on the page.';
    pill.textContent = drawing ? '✏️ Drawing (Esc)' : '✏️ Draw';
    css(pill, { background: drawing ? RED : '#fff', color: drawing ? '#fff' : '#111' });
    if (focusN) {
      var ta = list.querySelector('[data-n="' + focusN + '"] textarea');
      if (ta) { ta.focus({ preventScroll: true }); ta.parentNode.parentNode.scrollIntoView({ block: 'nearest' }); }
    }
  }
  function changed(n, focus) { updated = Date.now(); save(); render(focus ? n : 0); draw(); }
  function remove(n) { marks = marks.filter(function (m) { return m.n !== n; }); if (hot === n) hot = 0; changed(0); }
  function setDrawing(on) {
    drawing = !!on;
    css(cv, { pointerEvents: drawing ? 'auto' : 'none' });
    if (drawing) show();
    render();
  }
  function show() { css(panel, { display: 'flex' }); hideBtn.textContent = 'Hide'; layout(); }
  function hide() { css(panel, { display: 'none' }); setDrawing(false); layout(); }

  pill.addEventListener('click', function () { setDrawing(!drawing); });
  clearBtn.addEventListener('click', function () { if (!marks.length || confirm('Delete all ' + marks.length + ' marks?')) { marks = []; changed(0); } });
  hideBtn.addEventListener('click', hide);
  function onKey(e) {
    if (e.key === 'Escape' && drawing) { setDrawing(false); e.stopPropagation(); }
    else if (e.altKey && e.shiftKey && (e.key === 'E' || e.key === 'e')) { e.preventDefault(); setDrawing(!drawing); }
  }
  document.addEventListener('keydown', onKey, true);
  var raf = 0;
  function onView() { if (!raf) raf = requestAnimationFrame(function () { raf = 0; draw(); }); }
  addEventListener('scroll', onView, true);
  addEventListener('resize', layout);

  // ---- kept per page path, so injecting again after a reload brings the marks back
  function save() {
    try { localStorage.setItem(STORE, JSON.stringify(marks)); } catch (e) { /* err-ok: private mode: the marks just last this page view */ }
  }
  try { marks = JSON.parse(localStorage.getItem(STORE) || '[]') || []; } catch (e) { marks = []; } // err-ok: unreadable store = no old marks

  // ---- for Claude
  function simplify(s) { if (s.length <= 40) return s; var k = (s.length - 1) / 39, out = []; for (var i = 0; i < 40; i++) out.push(s[Math.round(i * k)]); return out; }
  function report() {
    return JSON.stringify({
      url: location.href, title: document.title, updated: new Date(updated).toISOString(),
      viewport: { w: innerWidth, h: innerHeight, dpr: window.devicePixelRatio || 1 }, scroll: { x: scrollX, y: scrollY },
      marks: marks.map(function (m) {
        var b = boxOf(m.strokes);
        return { n: m.n, comment: m.comment || '', kind: isDot(m) ? 'dot' : (m.strokes.length === 2 ? 'x' : 'shape'),
          box: { l: b.l, t: b.t, r: b.r, b: b.b }, seen: { l: b.l - scrollX, t: b.t - scrollY, r: b.r - scrollX, b: b.b - scrollY },
          target: m.target, inside: m.inside || [], strokes: m.strokes.map(simplify) };
      })
    });
  }
  function destroy() {
    document.removeEventListener('keydown', onKey, true);
    removeEventListener('scroll', onView, true);
    removeEventListener('resize', layout);
    if (root.parentNode) root.parentNode.removeChild(root);
    if (window.LG_EDIT === api) delete window.LG_EDIT;
  }
  var api = window.LG_EDIT = {
    version: VERSION, report: report, destroy: destroy, show: show, hide: hide, remove: remove,
    on: function () { setDrawing(true); }, off: function () { setDrawing(false); },
    marks: function () { return JSON.parse(report()).marks; },
    clear: function () { marks = []; changed(0); }
  };
  layout();
  render();
})();
