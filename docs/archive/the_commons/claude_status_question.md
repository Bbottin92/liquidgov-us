# Question — BUILD connection status: what exactly? (no code written)

Brandon said **BUILD connection status**. It was my own suggestion after the `netMint` retry work, so no doc specifies it, and it is
visitor-facing UI (where it shows, when, what it says). One answer unblocks the build. Client only; no daemon, civic or nginx change.

## Facts (measured on the live client, `commons/js/*.js` at `24a4c10`)

- **Today a visitor gets no signal at all.** The client talks to the daemon at 2 Hz (`NET_INTERVAL_MS = 500`): a move POST and a region
  poll GET. When those fail, the ~10 call sites swallow it (`.catch(function () {})`): a 502 while the daemon restarts, a network drop,
  an outage. The game keeps running as a local simulation, and to everyone else the player has vanished. The only trace is a console warning
  from the mint retry loop (`netMint`, first failure and every 10th).
- **What the client already knows:** `netReady` (a credential is in hand), `netReauthInFlight` (re-minting after a 403), `netMintAttempt`
  (consecutive mint failures), `netRegion` (joined). It does NOT track consecutive move/poll failures; that is the one thing to add.
- **Existing UI:** `showToast(msg, ms)`: a canvas-drawn line at the bottom of the viewport that disappears after 1.5s (used for "Couldn't
  change the door."); DOM layers with fixed positions and z-indexes (`#dpad` 10, `#interactBtn` 11, `#nameOverlay` 20); the 240x160 viewport is
  scaled 3x, with on-canvas text at 7-8px monospace. `index.html` has 5 `aria-*`/`role` attributes.
- **The cases worth telling apart:** (1) **never connected**: the first mint is failing or slow (e.g. the nginx `/guest` limit); (2)
  **lost**: it was connected and requests now fail (a daemon restart usually recovers in about a second through the existing remint; a
  longer outage does not); (3) **recovered**: it comes back on its own. A brief blip or the normal first-second connect must NOT flash a message.

## The decisions

### 1. Where

| | | Cost / risk |
|---|---|---|
| **A. A small DOM badge**, top centre, outside the canvas (recommended) | Crisp text at any scale, styled by `style.css`, can be `role="status"` / `aria-live="polite"` so screen readers hear it, can never be hidden by the game's own drawing. | One element in `index.html`, ~10 lines of CSS. |
| **B. On the canvas**, drawn like the toast | Same pixel font as everything else. | 7-8px text at a 3x scale is small; no accessibility; competes with the toast line and nameplates. |
| **C. Reuse the toast** | Zero new UI. | It disappears after 1.5s, so it can't show a *state*, and it would spam every 2 Hz failure. |
| **D. Nothing** | | Fine if the silent behaviour is acceptable. |

### 2. When it shows (proposal)

Show only after the problem has **lasted**, hide the moment it's over:

- **Never connected:** no credential for **4 seconds** after entering (a healthy mint takes about 50ms; the nginx limit case takes longer).
- **Lost:** a remint pending for more than **4 seconds**, or **6 consecutive** move/poll failures (about 3 seconds at 2 Hz). A daemon restart that
  recovers in a second never triggers it.
- **Hide:** immediately on the next successful request (or a credential arriving).
- One indicator, no flapping: it does not re-show for a new problem until it has been clear for a moment.

### 3. Wording (proposal)

One short line for all cases: **"Reconnecting…"**, with the longer text in the badge's `title`/`aria-label`: *"Can't reach the Square: you can still
walk, but others can't see you until it reconnects."* (Two different messages for "connecting" vs "lost" is more precise; one line is calmer. My pick: one.)

### 4. What it deliberately does NOT do

No change to the retry policy, backoff or any request; no new requests; no sound; nothing stored; guests and bound players, outdoors and
interiors, all behave the same. It only **observes** state the client already has, plus one counter of consecutive move/poll failures.

### 5. Tests (same standard as the split and netMint)

The state machine lives in one marked block (like `netMint`) and is unit-tested under node with a fake clock: it shows only after the
threshold, never on a blip, hides on recovery, does not flap, counts consecutive failures (a success resets), and the ordinary path shows
nothing. `test_client_files` keeps checking load order and the new element. Then a real-browser check against a real daemon: stop the daemon and
watch the badge appear after the threshold, restart it and watch it clear. Mutation-checked.

### 6. Deploy

Client only (`index.html`, `style.css`, one `js` file): a client-only `deploy` in the usual two steps. No daemon restart.

## Recommendation

**A, with 4s / 6 failures, the single "Reconnecting…" line, and everything in 4 unchanged.** It is the smallest change that makes the
silent failure visible, it is accessible, and it cannot alter any request.

**Reply with one letter: A (whole recommendation) / B / C / D, or A with named changes** (e.g. different thresholds, or two wordings).
