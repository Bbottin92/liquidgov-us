/* The landing intro (#lgintro, shell.cpp intro_overlay_html) and its Settings toggle (Brandon 2026-11-13 5:04 AM: "play
   every time you visit", default on, the first visit says it can be turned off in Settings).
   ONE pref name: the overlay's and the toggle's data-pref (shell.hpp kIntroPref, also the account's AccountPrefs field
   and the Settings form field). A logged-out browser keeps it in localStorage 'liquidgov.<pref>' ('0' = off, anything
   else = on, the default); a signed-in account keeps it on the server, which leaves the overlay out when it is off
   (data-mode="account": no local gate). The first-visit line shows once per browser ('liquidgov.intro.noted').
   When the overlay is gone it fires 'lgintro:done' on document: tip.js waits for that, so a first-time tip (the home
   page's Drop note) never shows under the video (one tip at a time). Storage blocked (private mode): plays (the
   default) and shows the line. */
(function () {
  var NOTED = 'liquidgov.intro.noted';
  function key(pref) { return 'liquidgov.' + pref; }
  function on(pref) { try { return localStorage.getItem(key(pref)) !== '0'; } catch (e) { return true; } } // err-ok: private mode: the default (on)
  function set(pref, yes) { try { localStorage.setItem(key(pref), yes ? '1' : '0'); return true; } catch (e) { return false; } } // err-ok: private mode: not kept

  // the logged-out Settings toggle (/settings/intro): a checkbox bound to this browser's pref
  document.querySelectorAll('input[data-lg-intro-toggle]').forEach(function (box) {
    var pref = box.getAttribute('data-pref');
    box.checked = on(pref);
    box.addEventListener('change', function () {
      var kept = set(pref, box.checked), msg = document.getElementById('lgintrosaved');
      if (msg) msg.textContent = kept ? 'Saved on this browser.' : 'This browser is not keeping settings (private mode?).';
    });
  });

  window.LG_INTRO = { on: on, set: set, key: key };
  var el = document.getElementById('lgintro');
  if (!el) return;
  var pref = el.getAttribute('data-pref');
  function gone() { if (el.parentNode) el.remove(); document.dispatchEvent(new Event('lgintro:done')); }
  if (el.getAttribute('data-mode') !== 'account' && !on(pref)) { gone(); return; }   // this browser turned it off
  var note = document.getElementById('lgintronote'), first = true;
  try { first = !localStorage.getItem(NOTED); localStorage.setItem(NOTED, '1'); } catch (e) {} // err-ok: private mode: show the line
  if (note) {
    note.hidden = !first;
    note.addEventListener('click', function (e) { e.stopPropagation(); }); // its link goes to Settings; the tap must not just close the intro
  }
  var vid = document.getElementById('lgintrovid'), done = false;
  function finish() {
    if (done) return;
    done = true;
    el.style.transition = 'opacity .7s ease';
    el.style.opacity = '0';
    setTimeout(gone, 720);
  }
  vid.addEventListener('ended', finish);
  vid.addEventListener('error', finish);
  el.addEventListener('click', finish);
  var skip = document.getElementById('lgintroskip');
  if (skip) skip.addEventListener('click', function (e) { e.stopPropagation(); finish(); });
  // iOS Low Power Mode and other autoplay-declining browsers never start an attribute-only autoplay: the play promise
  // is the fast signal, and the 1.5 s watchdog catches a play that silently freezes (both seen live on iPhone Safari
  // 2026-09-21; moved here unchanged from shell.cpp's inline script)
  var p = vid.play();
  if (p && p.catch) p.catch(finish);
  setTimeout(function () { if (vid.paused) finish(); }, 1500);
  setTimeout(finish, 12000);
})();
