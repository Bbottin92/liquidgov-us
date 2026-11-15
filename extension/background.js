/* The extension's background: turns drops on or off per site, and holds one presence socket per tab that has them.
   The page itself never talks to LiquidGov: the content script sends positions here, and this worker connects to
   wss://liquidgov.us/here/ws?page=/x/<hash>&ticket=<one-time ticket fetched with your LiquidGov sign-in>. */
importScripts('common.js');

function scriptId(pattern) { return 'lg-drops ' + pattern; }

async function enableSite(pattern, tabId) {
  const id = scriptId(pattern);
  const have = await chrome.scripting.getRegisteredContentScripts({ ids: [id] });
  if (!have.length) {
    await chrome.scripting.registerContentScripts([{ id, matches: [pattern], js: ['content.js'], runAt: 'document_idle', persistAcrossSessions: true }]);
  }
  if (tabId != null) await chrome.scripting.executeScript({ target: { tabId }, files: ['content.js'] }).catch(() => {});
}
async function disableSite(pattern, tabId) {
  await chrome.scripting.unregisterContentScripts({ ids: [scriptId(pattern)] }).catch(() => {});
  if (tabId != null) chrome.tabs.sendMessage(tabId, { lg: 'off' }).catch(() => {});
  await chrome.permissions.remove({ origins: [pattern] }).catch(() => {});
}
async function siteOn(pattern) {
  const have = await chrome.scripting.getRegisteredContentScripts({ ids: [scriptId(pattern)] });
  return have.length > 0;
}

chrome.runtime.onMessage.addListener((msg, sender, reply) => {
  if (!msg || sender.id !== chrome.runtime.id || sender.tab) return;   // only the popup asks these
  if (msg.lg === 'status') { siteOn(msg.pattern).then(on => reply({ on })); return true; }
  if (msg.lg === 'enable') { enableSite(msg.pattern, msg.tabId).then(() => reply({ on: true }), e => reply({ error: String(e) })); return true; }
  if (msg.lg === 'disable') { disableSite(msg.pattern, msg.tabId).then(() => reply({ on: false })); return true; }
});

// One socket per content script port. The page's address is read from the port's sender (the tab), never from the
// page, hashed here, and only the hash is sent.
chrome.runtime.onConnect.addListener(port => {
  if (port.name !== 'lg-drops' || !port.sender || !port.sender.tab) return;
  let ws = null, opened = false, closedByUs = false, room = '';
  function connect(href) {
    if (ws) { closedByUs = true; ws.close(); ws = null; }
    if (!lgAllowedUrl(href)) { port.postMessage({ state: 'blocked' }); return; }
    // browsers send no cookie on an extension's socket, so first a one-time ticket (this fetch does carry the cookie)
    Promise.all([lgRoomFor(href), fetch(LG_SITE + '/here/ticket', { credentials: 'include', cache: 'no-store' })
      .then(r => r.ok ? r.json() : {}).catch(() => ({}))]).then(([r, t]) => {
      room = r; opened = false; closedByUs = false;
      if (!t || !t.ticket) { port.postMessage({ state: 'refused' }); return; }
      const sock = new WebSocket(LG_SITE.replace(/^http/, 'ws') + '/here/ws?page=' + encodeURIComponent(r).replace(/%2F/g, '/') +
        '&ticket=' + encodeURIComponent(t.ticket));
      ws = sock;
      sock.onopen = () => { opened = true; port.postMessage({ state: 'open' }); };
      sock.onmessage = e => { if (typeof e.data === 'string' && e.data.length < 65536) { try { port.postMessage({ snap: JSON.parse(e.data) }); } catch (x) {} } };
      sock.onclose = () => {
        if (ws !== sock || closedByUs) return;
        ws = null;
        // never opened: signed out, or "Show me where I am" is off on LiquidGov
        try { port.postMessage({ state: opened ? 'closed' : 'refused' }); } catch (x) {}
      };
    });
  }
  port.onMessage.addListener(m => {
    if (m.hello || m.moved) connect(port.sender.tab.url || port.sender.url);   // first contact, or the page's address changed
    else if (m.x != null && ws && ws.readyState === 1) {
      const x = Math.min(1, Math.max(0, +m.x)), y = Math.min(1, Math.max(0, +m.y));
      ws.send(JSON.stringify({ x, y }));
    }
  });
  port.onDisconnect.addListener(() => { closedByUs = true; if (ws) ws.close(); ws = null; });
});
