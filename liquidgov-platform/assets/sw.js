self.addEventListener('install', function (e) { self.skipWaiting(); });
self.addEventListener('activate', function (e) { e.waitUntil(self.clients.claim()); });
self.addEventListener('push', function (e) {
  var data = { title: 'LiquidGov', body: 'Something new.', url: '/', tag: 'lg' };
  try { if (e.data) data = Object.assign(data, e.data.json()); }
  catch (err) { try { data.body = e.data.text(); } catch (e2) {} }
  e.waitUntil(self.registration.showNotification(data.title || 'LiquidGov', {
    body: data.body || '', icon: '/favicon.svg', badge: '/favicon.svg',
    data: { url: data.url || '/' }, tag: data.tag || 'lg'
  }));
});
self.addEventListener('notificationclick', function (e) {
  e.notification.close();
  var url = (e.notification.data && e.notification.data.url) || '/';
  e.waitUntil(clients.matchAll({ type: 'window', includeUncontrolled: true }).then(function (list) {
    for (var i = 0; i < list.length; i++) {
      if (list[i].url.indexOf(self.location.origin) === 0 && 'focus' in list[i]) {
        list[i].navigate && list[i].navigate(url);
        return list[i].focus();
      }
    }
    return clients.openWindow(url);
  }));
});
