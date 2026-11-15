(async function () {
  const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
  const on = document.getElementById('on'), site = document.getElementById('site');
  const url = tab && tab.url || '';
  let pattern = '';
  try { pattern = lgOriginPattern(url); site.textContent = new URL(url).hostname; } catch (e) { site.textContent = 'This page'; }

  if (!lgAllowedUrl(url)) {
    on.disabled = true;
    document.getElementById('onlabel').textContent = 'Drops are never shown here (a private or sensitive page)';
  } else {
    const st = await chrome.runtime.sendMessage({ lg: 'status', pattern });
    on.checked = !!(st && st.on);
  }
  on.addEventListener('change', async () => {
    if (on.checked) {
      // the browser asks you to allow this one site; nothing runs on it otherwise
      const granted = await chrome.permissions.request({ origins: [pattern] });
      if (!granted) { on.checked = false; return; }
      const r = await chrome.runtime.sendMessage({ lg: 'enable', pattern, tabId: tab.id });
      if (r && r.error) { on.checked = false; site.textContent = r.error; }
    } else {
      await chrome.runtime.sendMessage({ lg: 'disable', pattern, tabId: tab.id });
    }
  });

  const share = document.getElementById('share');
  if (!/^https?:/.test(url)) share.disabled = true;
  share.addEventListener('click', () => {
    const q = new URLSearchParams({ url, title: tab.title || '' });
    chrome.tabs.create({ url: LG_SITE + '/share?' + q.toString() });
    window.close();
  });
})();
