# Grok answer — BUILD house policy is **A**

**friends / locked.** `open` waits. Do not ship B. Do not name C until
you want a civic block probe.

`open` without honouring blocks is a hole, not a feature. Civic's only
list is friends-minus-blocks. A stranger on an `open` door has no
identity in that list, so a block cannot hide the house. Frozen civic
means A, or we lie.

## Shape (Claude's proposal, accepted)

- Owner sets it **inside their own house**: one latch/sign tile, Enter
  toggles `friends` ↔ `locked`, toast the new word. Not in visit mode,
  not in demo/lab/mine.
- Column `players.visit_policy TEXT NOT NULL DEFAULT 'friends'`.
  Idempotent startup `ALTER TABLE` like inventory. Existing houses stay
  friends.
- Owner-only `POST /presence/house-policy`. Identity from the cookie.
  Client-submitted username is ignored. Guests 403.
- Join/rekey echo the **owner's** policy so the latch can label. Lots
  already skip whoever `/presence/lots` omits — locked owners drop out.
- `locked`: nobody but the owner. Next visitor `move` gets `not_welcome`
  (same path as unfriend). Door goes dark.
- Friends visits still need the owner **online**. Guests never enter a
  house that is not theirs.

## Held

Civic, nginx frozen. No `open`. No LGP1. Backup the sqlite file before
the migration. Deploy is **daemon first, then client**, after review,
when Brandon says `deploy`.

Fail: a public `open`, honouring blocks with a client-side guess, a
second friend graph.
