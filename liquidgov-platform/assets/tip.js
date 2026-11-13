/* First-time tips (Brandon 2026-11-13 2:05 AM): one mechanism for every "first time here" note, moved out of
   juketube.js (its #jbhint tip, 2026-11-07 iPhone sweep) so the walker's Drop/Scroll note reuses it. ONE place keeps a
   tip's seen-flag: localStorage 'lg-<name>-hint' = '1' (seen for good) and 'lg-<name>-hint-views' (pages it was shown
   on). A tip shows only outside a framed page, on at most maxViews page views, until its close button is pressed (that
   saves the flag); hideAfterMs > 0 hides it for this page after that long. The look is juketube.css's .lg-tip and
   .lg-tip-close. One tip at a time: while one shows, another waits for a later page view (its view isn't counted), so
   two first-time notes never stack up and push or cover the page together. localStorage is guarded: private mode
   throws, and then no tip shows. The landing intro (#lgintro, intro.js) counts as the one on screen: a tip due while
   it plays waits for its 'lgintro:done' and shows then (onShow tells the page), never under the video. */
(function () {
  var showing = 0;   // tips on the screen now
  function flag(name) { return 'lg-' + name + '-hint'; }
  // wire(el, closeBtn, name, {maxViews, hideAfterMs, onHide, onShow}) -> {shown, hide(forGood)}: shows el now if it
  // is due (after the intro, if one is playing), else hides it.
  function wire(el, closeBtn, name, opts) {
    opts = opts || {};
    var tip = { shown: false, hide: hide };
    function hide(forGood) {
      el.style.display = 'none';
      if (tip.shown) showing--;
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
    if (framed || seen === '1' || showing > 0) { hide(false); return tip; }
    if (document.getElementById('lgintro')) {   // the intro is playing: wait for it (its view isn't counted yet)
      var was = el.style.display;
      el.style.display = 'none';
      document.addEventListener('lgintro:done', function () {
        if (showing > 0) return;
        el.style.display = was;
        show();
        if (opts.onShow) opts.onShow();
      }, { once: true });
      return tip;
    }
    show();
    return tip;
    function show() {
      try { localStorage.setItem(flag(name) + '-views', String(views + 1)); } catch (e) {} // err-ok: private mode: the tip just shows for this page
      if (views + 1 >= (opts.maxViews || 1)) { try { localStorage.setItem(flag(name), '1'); } catch (e) {} } // err-ok: as above
      if (opts.hideAfterMs > 0) setTimeout(function () { hide(false); }, opts.hideAfterMs);
      tip.shown = true;
      showing++;
    }
  }
  window.LG_TIP = { wire: wire, flag: flag };
})();
