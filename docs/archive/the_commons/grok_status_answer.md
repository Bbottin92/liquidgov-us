# Connection status — A

Letter: **A**, as written. Client only. Observe. Do not change any request.

## Lock

- DOM badge outside the canvas, top of `#stage`, not drawn on the canvas,
  not the toast. `role="status"` `aria-live="polite"`.
- Visible copy: **Reconnecting…**
- Longer text only on `title` / `aria-label`: *"Can't reach the Square:
  you can still walk, but others can't see you until it reconnects."*
- Show after the problem has lasted:
  - no credential for **4s** after enter
  - remint pending **>4s**
  - **6** consecutive move/poll failures (~3s at 2 Hz)
- Hide on the next successful request or a credential arriving.
- Anti-flap is the thresholds themselves. A success resets the failure
  counter. Do **not** add a third "must stay clear for a moment" timer;
  that would hide a second outage.
- Palette from existing `style.css` (ink / cream). No new colour. Must
  not cover the d-pad or the Enter button.
- Guests and bound, outdoors and interiors: same badge.
- No sound, no storage, no new requests, no retry/backoff change.

## Tests (required)

Same bar as `netMint` and the split:

1. Node unit test, fake clock, marked block: show only after threshold,
   never on a 1s blip, hide on recovery, no flap, success resets the
   counter, ordinary path shows nothing.
2. `test_client_files` still checks load order and the new element.
3. Real browser, real daemon: stop daemon → badge after threshold;
   start daemon → badge clears. Mutation-check the test.

## Files

`index.html` (one element), `style.css` (~10 lines), and the machine in
`js/net.js` (that is where `netReady` / mint already live). Do not add a
ninth script unless the unit test cannot see the machine otherwise.

## Deploy

Client only, two-phase. No daemon restart. Not until review.

Not A-with-two-wordings. Not B, C, or D.
