/* First-time tips (Brandon 2026-11-13 2:05 AM): one mechanism for every "first time here" note, moved out of
   juketube.js (its #jbhint tip, 2026-11-07 iPhone sweep) so the walker's Drop/Scroll note reuses it. ONE place keeps a
   tip's seen-flag: localStorage 'lg-<name>-hint' = '1' (seen for good) and 'lg-<name>-hint-views' (pages it was shown
   on). A tip shows only outside a framed page, on at most maxViews page views, until its close button is pressed (that
   saves the flag); hideAfterMs > 0 hides it for this page after that long. The look is juketube.css's .lg-tip and
   .lg-tip-close. localStorage is guarded: private mode throws, and then no tip shows. */
(function () {
  function flag(name) { return 'lg-' + name + '-hint'; }
  // wire(el, closeBtn, name, {maxViews, hideAfterMs, onHide}) -> {shown, hide(forGood)}: shows el now if it is due,
  // else hides it.
  function wire(el, closeBtn, name, opts) {
    opts = opts || {};
    var tip = { shown: false, hide: hide };
    function hide(forGood) {
      el.style.display = 'none';
      tip.shown = false;
      if (forGood) { try { localStorage.setItem(flag(name), '1'); } catch (e) {} } // err-ok: private mode; hidden for this page anyway
      if (opts.onHide) opts.onHide();
    }
    var framed = true, seen = '1', views = 0;
    try { framed = window.top !== window; } catch (e) {} // err-ok: cross-origin parent: treat as framed (no tip)
    try { seen = localStorage.getItem(flag(name)); views = +(localStorage.getItem(flag(name) + '-views') || 0); } catch (e) {} // err-ok: private mode: seen stays "1", no tip
    closeBtn.addEventListener('click', function (e) {
      e.preventDefault();
      e.stopPropagation(); // a tip may open something itself (Juketube's does); the x must not
      hide(true);
    });
    if (framed || seen === '1') { hide(false); return tip; }
    try { localStorage.setItem(flag(name) + '-views', String(views + 1)); } catch (e) {} // err-ok: private mode: the tip just shows for this page
    if (views + 1 >= (opts.maxViews || 1)) { try { localStorage.setItem(flag(name), '1'); } catch (e) {} } // err-ok: as above
    if (opts.hideAfterMs > 0) setTimeout(function () { hide(false); }, opts.hideAfterMs);
    tip.shown = true;
    return tip;
  }
  window.LG_TIP = { wire: wire, flag: flag };
})();
