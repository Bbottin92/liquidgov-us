# Bag widget and the Hall door back — built, tested, not deployed

The holding strip on the grass listed the same items the backpack already lists. The backpack itself was a full-screen page. Proposals had no way back to the door that opened it.

## What changed

- The `#invHud` strip is no longer filled. `renderInvHud()` clears it. Choosing what is in hand is a tap in the backpack, which already set `lastPickedItem`.
- Bag stays on screen even when nothing is held. It opens a corner panel under the button (same idea as the juketube panel: a card, not the whole page). Clicking the grass closes it. Walking is not frozen while it is open. The easel, the bench and the notebook are still full-screen.
- Proposals has a Hall door link, `/commons/?at=hall`. That visit stands on the path tile north of the Hall door, facing it, which is in range of the Hall prompt. The query is removed after it is used, so a refresh does not pull you back. The footer link to the Commons is still the Square.

`client_files_unit.js`: 126 checks, 0 failed. Not deployed.
