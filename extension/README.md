# LiquidGov Drops (browser extension, prototype)

Hang out as drops with LiquidGov people on other websites, and share any page to your LiquidGov feed.
Chrome, Edge, Brave and other Chromium browsers (version 116 or newer). Not in any store yet.

## Try it

1. Sign in at https://liquidgov.us and turn on **Show me where I am** in Settings.
2. Open `chrome://extensions`, turn on **Developer mode**, click **Load unpacked**, and choose this `extension` folder.
3. On a website, click the extension's button, tick **Drops on this site**, and allow it when the browser asks.
   Your drop appears at the bottom right; drag it (or focus it and use the arrow keys). Anyone else with drops on
   the same page appears as a drop with their initial; hover for their name.
4. **Share this page to LiquidGov** opens your feed's post box with the page's title and link filled in.

## What it does and does not do

- Off on every site until you turn it on for that site; the browser then asks your permission for that one site.
  Turning it off removes the permission.
- Never reads the page. It draws its own layer on top and nothing else.
- The room for a page is a SHA-256 of its address without the query or `#` part (except a YouTube video's `v`), so
  search terms and sign-in tokens never go into it. Only that hash goes to LiquidGov, through the same presence socket
  profile pages use (`/here/ws?page=/x/<hash>`).
- Never on banking, payments, mail, health, sign-in, checkout or account pages, government sites, local addresses,
  or liquidgov.us itself (`common.js`, `LG_SENSITIVE`). Those addresses are not hashed or sent.
- Blocks are the site's: people you block, or who block you, never see each other here.
- Browsers send no cookie on an extension's WebSocket, so the extension first fetches a one-time ticket from
  `/here/ticket` (that request carries your sign-in; other websites cannot read the answer) and opens the socket with
  it. A ticket works once, within 30 seconds, and only for these web-page rooms.

## Checking it

`bash tests/run_check.sh <liquidgov_http> <liquidgov_e2e_seed>` loads a test copy into headless Chromium against a
throwaway local server: two signed-in people on the same page see each other, a drag moves your drop for the other,
the query and `#` do not change the room, another page is another room, and signed out shows how to sign in.

## Not yet

Microphone and camera, ripples, the drop menu, Firefox, a store listing, and "forward this post" buttons on
Facebook, TikTok or X posts (for now, use Share or the share sheet on a phone).
