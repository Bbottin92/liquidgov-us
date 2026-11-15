/* The extension in a real Chromium: two signed-in browsers on the same page see each other's drops. Run by run_check.sh. */
const { chromium } = require('playwright');
const [ext, admin, pal] = process.argv.slice(2);
async function browser(name, sid) {
  const ctx = await chromium.launchPersistentContext('/tmp/lg-ext-check/prof-' + name, {
    headless: true, channel: 'chromium', args: ['--disable-extensions-except=' + ext, '--load-extension=' + ext] });
  await ctx.addCookies([{ name: 'lg_session', value: sid, domain: '127.0.0.1', path: '/', httpOnly: true, sameSite: 'Lax' }]);
  let sw = ctx.serviceWorkers()[0] || await ctx.waitForEvent('serviceworker');
  await sw.evaluate(() => chrome.scripting.registerContentScripts([{ id: 't', matches: ['http://localhost/*'], js: ['content.js'] }]));
  const page = await ctx.newPage();
  await page.goto('http://localhost:18138/a.html');
  return { ctx, page };
}
const names = p => p.evaluate(() => {
  const h = [...document.documentElement.children].find(e => e.shadowRoot);
  return h ? [...h.shadowRoot.querySelectorAll('.n')].map(n => n.textContent).sort().join(' ') + '|' + (h.shadowRoot.querySelector('.note') || {}).textContent : 'NO LAYER';
});
(async () => {
  let fails = 0; const check = (n, ok, got) => { console.log((ok ? 'PASS ' : 'FAIL ') + n + (ok ? '' : ' (' + got + ')')); if (!ok) fails++; };
  const a = await browser('a', admin), b = await browser('b', pal);
  await a.page.waitForTimeout(2500);
  const sa = await names(a.page), sb = await names(b.page);
  check('Bbottin sees pal on the page', sa.startsWith('pal|'), sa);
  check('pal sees Bbottin on the page', sb.startsWith('Bbottin|'), sb);
  // dragging moves the drop for the other person
  const before = await b.page.evaluate(() => { const h = [...document.documentElement.children].find(e => e.shadowRoot); return h.shadowRoot.querySelector('.d:not(.me)').style.left; });
  const box = await a.page.evaluate(() => { const h = [...document.documentElement.children].find(e => e.shadowRoot); const r = h.shadowRoot.querySelector('.me').getBoundingClientRect(); return [r.x + r.width / 2, r.y + r.height / 2]; });
  await a.page.mouse.move(box[0], box[1]); await a.page.mouse.down(); await a.page.mouse.move(200, 200, { steps: 8 }); await a.page.mouse.up();
  await b.page.waitForTimeout(1200);
  const after = await b.page.evaluate(() => { const h = [...document.documentElement.children].find(e => e.shadowRoot); return h.shadowRoot.querySelector('.d:not(.me)').style.left; });
  check('dragging your drop moves it for the other person', before !== after && Math.abs(parseFloat(after) - 200) < 3, before + ' -> ' + after);
  // a different page is a different room
  await b.page.goto('http://localhost:18138/a.html?x=1#y');
  await b.page.waitForTimeout(2000);
  check('the query and # do not change the room', (await names(b.page)).startsWith('Bbottin|'), await names(b.page));
  await b.page.goto('http://localhost:18138/other.html'); await b.page.waitForTimeout(2000);
  const so = await names(b.page);
  check('another page is another room', so.startsWith('|'), so);
  await a.ctx.close(); await b.ctx.close();
  // signed out: the note
  const c = await chromium.launchPersistentContext('/tmp/lg-ext-check/prof-c', { headless: true, channel: 'chromium', args: ['--disable-extensions-except=' + ext, '--load-extension=' + ext] });
  const sw = c.serviceWorkers()[0] || await c.waitForEvent('serviceworker');
  await sw.evaluate(() => chrome.scripting.registerContentScripts([{ id: 't', matches: ['http://localhost/*'], js: ['content.js'] }]));
  const pc = await c.newPage(); await pc.goto('http://localhost:18138/a.html'); await pc.waitForTimeout(2000);
  const sc = await names(pc);
  check('signed out: a note says how to sign in', /signed in/.test(sc), sc);
  await c.close();
  console.log('fails=' + fails); process.exit(fails ? 1 : 0);
})().catch(e => { console.log('ERROR ' + e.message); process.exit(2); });
