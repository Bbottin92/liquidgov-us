(function () {
  // The alerts prompt is an ordinary strip at the top of the page content, not
  // a floating button, so it can never cover anything. It has a close button
  // that hides it for a week, and failure notes fade after a moment.
  // Brandon 2026-11-06: "the turn on alerts thing is covering something".
  // A page shown inside another (the /map field) asks nothing: the outer page already did (2026-11-07 iPhone sweep).
  try { if (window.top !== window) return; } catch (e) { return; }
  var HIDE_KEY = 'lg.alerts.hiddenUntil';
  function hiddenByUser() {
    try { return Number(localStorage.getItem(HIDE_KEY) || 0) > Date.now(); } catch (e) { return false; }
  }
  function banner(text, clickable) {
    var old = document.getElementById('lgpushask');
    if (old) old.remove();
    if (clickable && hiddenByUser()) return;
    var wrap = document.createElement('div');
    wrap.id = 'lgpushask';
    wrap.style.cssText = 'display:flex;align-items:center;gap:.25rem;width:fit-content;max-width:100%;margin:0 0 .75rem;border-radius:8px;background:#8a6620;color:#f7f1e4;font:600 .9rem sans-serif';
    var b = document.createElement('button');
    b.type = 'button';
    b.textContent = text;
    b.style.cssText = 'flex:1;padding:.55rem .8rem;border:0;background:none;color:inherit;font:inherit;text-align:left;cursor:' + (clickable ? 'pointer' : 'default');
    wrap.appendChild(b);
    var x = document.createElement('button');
    x.type = 'button';
    x.textContent = '×';
    x.setAttribute('aria-label', 'Hide');
    x.style.cssText = 'padding:.4rem .65rem;border:0;background:none;color:inherit;font:700 1.1rem sans-serif;cursor:pointer;opacity:.85';
    x.addEventListener('click', function (e) {
      e.preventDefault();
      e.stopPropagation();
      if (clickable) { try { localStorage.setItem(HIDE_KEY, String(Date.now() + 7 * 864e5)); } catch (err) {} }
      wrap.remove();
    });
    wrap.appendChild(x);
    if (clickable) {
      b.addEventListener('click', function (e) {
        e.preventDefault();
        e.stopPropagation();
        if (clickable()) wrap.remove();
      });
    } else {
      setTimeout(function () { if (wrap.parentNode) wrap.remove(); }, 5000);
    }
    // One prompt at a time: when alerts can be turned on here, the "Get the app" bar (setup.js) steps aside.
    if (clickable) { var app = document.getElementById('lggetapp'); if (app) app.remove(); }
    var main = document.querySelector('main');
    if (main) main.insertBefore(wrap, main.firstChild); else document.body.insertBefore(wrap, document.body.firstChild);
  }
  function b64urlToUint8(b64) {
    b64 = (b64 || '').replace(/-/g, '+').replace(/_/g, '/');
    while (b64.length % 4) b64 += '=';
    var raw = atob(b64), out = new Uint8Array(raw.length);
    for (var i = 0; i < raw.length; i++) out[i] = raw.charCodeAt(i);
    return out;
  }
  var busy = false;
  function subscribe() {
    if (busy || Notification.permission !== 'granted') return;
    busy = true;
    navigator.serviceWorker.ready.then(function (reg) {
      return fetch('/push/vapid', { credentials: 'same-origin' }).then(function (r) {
        if (!r.ok) throw new Error('vapid');
        return r.text();
      }).then(function (vapid) {
        return reg.pushManager.getSubscription().then(function (ex) {
          if (ex) return ex;
          return reg.pushManager.subscribe({ userVisibleOnly: true, applicationServerKey: b64urlToUint8(vapid.trim()) });
        });
      }).then(function (sub) {
        var j = sub.toJSON();
        return fetch('/push/subscribe', {
          method: 'POST', credentials: 'same-origin',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ endpoint: j.endpoint, p256dh: j.keys && j.keys.p256dh, auth: j.keys && j.keys.auth })
        }).then(function (r) {
          // success needs no strip (it used to show "Alerts on", which setup.js then hid every second)
          if (!r.ok) banner('Subscribe failed', null);
        });
      });
    }).catch(function () { banner('Subscribe failed', null); busy = false; }).then(function () { busy = false; });
  }
  if (!('serviceWorker' in navigator) || !('PushManager' in window) || !('Notification' in window)) {
    // An iPhone Safari tab: alerts need the site on the Home Screen first. setup.js's get-app bar says exactly that,
    // so nothing here. (This banner's text used to be hidden every second by setup.js, leaving a strip with only "x".)
    return;
  }
  navigator.serviceWorker.register('/sw.js', { scope: '/' }).catch(function () {});
  if (Notification.permission === 'granted') { subscribe(); return; }
  if (Notification.permission === 'denied') { banner('Alerts blocked in iPhone Settings', null); return; }
  banner('Turn on alerts', function () {
    Notification.requestPermission().then(function (p) {
      if (p === 'granted') subscribe();
      else banner('Alerts blocked in iPhone Settings', null);
    });
    return true;
  });
})();
