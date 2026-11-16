/* Hints for newcomers (Brandon 2026-11-16: "a hint phase for newcomers that you can easily turn off and you can turn
   back on from the dropdown menu"). The server loads this only for a signed-in person whose Hints setting is on,
   with LG_HINTS = {done: [ids already dismissed]}. At most ONE hint shows per page, and one per visit (sessionStorage) (the first whose spot is on the
   page and that is not dismissed). "Got it" saves that hint as dismissed on the account (POST /settings/hints), so it
   never returns even if hints are turned off and on again, until "Show hints again" in Settings; "Turn hints off"
   turns the whole setting off. Not shown inside a frame (the map's boxes, previews). Everything fails quiet. */
(function () {
  if (window.top !== window || !window.LG_HINTS) return;
  var done = {};
  (window.LG_HINTS.done || []).forEach(function (d) { done[d] = 1; });
  // id, where it applies (path regex), what to point at, the words. Order matters: the first due one shows.
  var HINTS = [
    { id: 'menu', path: /^\/(feed|map|home|)$/, at: '.navavatar',
      text: 'Your picture opens your menu: Settings, People, Groups, and where you turn these hints off or back on.' },
    { id: 'map-swim', path: /^\/map/, at: '#lgmap',
      text: 'Swim into a pool to open it. Drag, or use the arrow keys or WASD, and press Enter on a pool to go in.' },
    { id: 'map-search', path: /^\/map/, at: '#lgm-q',
      text: 'Search the map by a person\u2019s name, a word, or a date like \u201cnov 12\u201d.' },
    { id: 'thumbs', path: /^\/(feed|u\/)/, at: '.reactions',
      text: 'Thumbs are how people say what they think. You choose who sees yours.' }
  ];
  function post(body) {
    try { fetch('/settings/hints', { method: 'POST', credentials: 'same-origin',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: body + '&ajax=1' }); } catch (e) {}
  }
  function show(h, el) {
    var box = document.createElement('div');
    box.className = 'lg-hint'; box.setAttribute('role', 'note');
    var p = document.createElement('p'); p.textContent = h.text; box.appendChild(p);
    var row = document.createElement('div'); row.className = 'lg-hint-row';
    var ok = document.createElement('button'); ok.type = 'button'; ok.textContent = 'Got it';
    ok.addEventListener('click', function () { post('dismiss=' + encodeURIComponent(h.id)); box.remove(); });
    var off = document.createElement('button'); off.type = 'button'; off.className = 'ghost'; off.textContent = 'Turn hints off';
    off.addEventListener('click', function () { post('on=0'); box.remove(); });
    row.appendChild(ok); row.appendChild(off); box.appendChild(row);
    var st = document.createElement('style');
    st.textContent = '.lg-hint{position:fixed;left:50%;bottom:1rem;transform:translateX(-50%);z-index:60;max-width:min(28rem,92vw);' +
      'background:var(--card,#fffdf8);color:var(--ink,#1a1814);border:1px solid var(--accent,#8a6620);border-radius:12px;' +
      'padding:.8rem 1rem;box-shadow:0 6px 24px rgba(0,0,0,.25);font:.92rem/1.4 var(--body,system-ui)}' +
      '.lg-hint p{margin:0 0 .6rem}.lg-hint-row{display:flex;gap:.5rem;flex-wrap:wrap}' +
      '.lg-hint button{min-height:36px;padding:.3rem .8rem;font-size:.85rem}';
    document.head.appendChild(st);
    document.body.appendChild(box);
  }
  // Not overwhelming (Brandon 2026-11-16: "whatever gets the most without being overwhelming"): one new tip per visit
  // (browser session), however many pages are opened. Storage that throws just means no limit this time.
  function shownThisVisit() { try { return sessionStorage.getItem('lg-hint-shown') === '1'; } catch (e) { return false; } }
  function markShown() { try { sessionStorage.setItem('lg-hint-shown', '1'); } catch (e) {} }
  function go() {
    if (shownThisVisit()) return;
    for (var i = 0; i < HINTS.length; i++) {
      var h = HINTS[i];
      if (done[h.id] || !h.path.test(location.pathname)) continue;
      var el = document.querySelector(h.at);
      if (!el) continue;
      show(h, el);
      markShown();
      return;
    }
  }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', function () { setTimeout(go, 800); });
  else setTimeout(go, 800);
})();
