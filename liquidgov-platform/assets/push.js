(function () {
  function banner(text, clickable) {
    var old = document.getElementById('lgpushask');
    if (old) old.remove();
    var b = document.createElement('button');
    b.id = 'lgpushask';
    b.type = 'button';
    b.textContent = text;
    b.style.cssText = 'position:fixed;left:50%;top:calc(env(safe-area-inset-top,0px) + 4.2rem);transform:translateX(-50%);z-index:90;padding:.7rem 1.1rem;border:0;border-radius:8px;background:#8a6620;color:#f7f1e4;font:600 .95rem sans-serif;max-width:92vw';
    if (clickable) {
      b.addEventListener('click', function (e) {
        e.preventDefault();
        e.stopPropagation();
        if (clickable()) b.remove();
      });
    }
    document.body.appendChild(b);
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
          banner(r.ok ? 'Alerts on' : 'Subscribe failed', null);
        });
      });
    }).catch(function () { banner('Subscribe failed', null); busy = false; }).then(function () { busy = false; });
  }
  if (!('serviceWorker' in navigator) || !('PushManager' in window) || !('Notification' in window)) {
    banner('Alerts need Safari → Add to Home Screen', null);
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
