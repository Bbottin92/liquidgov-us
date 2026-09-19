# open houses (rest slice 4a) — built, local-tested, not deployed

Brandon: "BUILD open A, split, orphans log-only". Grok's `GROK_REST_4_ANSWER.md`:
split; **4a `open` first, discovery A**; 4b orphans is its own later BUILD
(log-only first). This is 4a only. **Civic (a fourth named edit), daemon and
client.** nginx, schema columns and LGP1 untouched. **No orphan reaping, no
DELETE anywhere.** No deploy.

## What ships

An owner can set their house to a third policy, **open**: any bound player who
has not blocked, or been blocked by, the owner may walk in while the owner is in
the game.

- **Civic: `GET /internal/blocked/<username>`** (`http_server.cpp`, one branch
  next to `/internal/friends`, everything else byte-identical). Same-box gate as
  `/internal/session-user` (plain 404 if `X-Forwarded-For` is present or the peer
  isn't loopback). The **viewer is the session cookie** (never a parameter), the
  path is the **house owner**. `{"blocked":true}` if either side blocked the
  other. **Fails closed:** no session, unknown owner and a malformed name all
  answer `{"blocked":true}` (still a 200, so the daemon can tell it from the
  public 404); only a real `{"blocked":false}` can open a door.
  `Cache-Control: no-store`. Read-only.
- **Daemon `civic::block_answer(headers, owner)`:** asks that route; caches a real
  answer (true or false) 15s in RAM keyed on `(Cookie value, owner)`; **never
  caches a failure**. No answer (no session, civic down/slow, unreadable reply) is
  treated as blocked. The owner name comes from a client-supplied region string, so
  it is checked to be a plain username (`[A-Za-z0-9._-]`, <=64) **before** it goes
  into a request line: a space or CR/LF is refused, never forwarded.
- **`visit_allowed`:** `locked` refuses everyone; a mutual friend passes as before;
  a **non-friend passes only if the policy is `open` and civic says not blocked**;
  entering still needs the owner online, staying only re-checks policy + blocks
  (the same call runs on every move, so a block ends a visit on the next move after
  the 15s cache, via the existing `not_welcome` path).
- **`/presence/lots` = discovery (option A):** your online mutual friends first
  (a locked house drops out), then the online `open` houses of non-friends, six in
  all, each door needing a real `blocked:false`. Guests get none; you never see
  your own house as an open door. If civic gives no answer while probing
  candidates, the loop **stops** (fail closed, one bounded wait per request, not
  one per candidate).
- **`POST /presence/house-policy`** accepts `open` as well as `friends`/`locked`
  (still owner-only, cookie identity only). `players.visit_policy` already held any
  string, so **no schema change** (only the comment in `schema.sql` was updated).
- **Client:** the latch cycles **friends -> open -> locked -> friends**. The prompt
  names the current state (`[Door: friends|open|locked]`, also the button); Enter
  moves to the next; the toast follows the daemon's answer. Two cosmetic fixes made
  along the way: the toasts are short enough for the 160px viewport ("Door open:
  anyone can visit."), and in-world prompts are clamped inside the viewport (the
  latch sits on the left wall, where a centred label was cut off).

## Tested (local: real civic, real daemon, scratch stores; alice owner, bob friend,
carol stranger, dave blocked by alice, eve who blocked alice, zed a second open
house, fay a fresh stranger, plus a guest)

Civic route, direct: carol -> alice `false`; dave -> alice `true`; eve -> alice
`true` (the **other direction**); no session `true`; unknown owner `true`;
malformed `true`; with `X-Forwarded-For` a plain **404**; `Cache-Control: no-store`.

Open house: `open` accepted, `public` `400`. carol, bob enter; **dave, eve, and a
guest are refused**; a name with a space and a CR/LF in the owner name are refused.
**Lots:** carol `[alice, zed]`, bob `[alice(friend), zed]`, alice `[bob, zed]`
(never herself), dave and eve `[zed]` (blocked either way), guest `[]`.
**Live policy:** `friends` -> carol's next move `not_welcome` at once and alice
leaves her lots; back to `open` -> carol re-enters; `locked` -> bob evicted, both
lots `[zed]`. **A block ends a visit:** alice blocks carol; still cached inside 15s;
after 16s `not_welcome`, no re-entry, alice hidden from carol's lots.
**Fail closed:** civic frozen (SIGSTOP): a never-probed stranger is refused after
one ~1.0s wait; civic back, they **walk in at once (the failure was not cached)**;
lots with civic frozen is one ~1.0s wait, not one per candidate.
Regressions passing: `policy_test` (friends/locked, restart; its old "open is
refused" line is the one intended change), `visit_test`, `rebind_test`,
`session_cache_test`, `persist_house_test`.

Client (real browser, policy request stubbed): the latch steps
friends -> open -> locked -> friends -> open with the right prompt, body sent and
toast at each step; a join echo `house_policy:"open"` labels it; an unknown echo
falls back to `friends`. Toast and prompt fit the viewport. Console clean.

Not exercised: a real cookie-carrying browser flipping a real house to open and a
stranger's door lighting (the deploy check), and the civic route through the real
nginx (it must stay unreachable there; nginx never routes `/internal/`).

## Deploy (when Brandon says `deploy`, after Grok's review)

**Civic first**, then daemon, then client. Civic drift-check first (`md5sum` the
droplet's `http_server.cpp` against the last known copy: Grok/Brandon have
hand-edited it live before). Civic rebuild `-j1` + restart `liquidgov`; then the
daemon (`-j1`, no reconfigure); then the client. No schema change, but an online
SQLite copy first is cheap. Live check: public `/internal/blocked/x` is **404**;
`/presence/lots` still 200; and, with a real logged-in player, set the door to
open and see a stranger's lots light it (and stay dark for someone you blocked).

## Not here

**4b orphans** (its own BUILD: `user-exists`, log-only first, two passes, cap 3,
backup). Enter-by-name and a public blocked route are explicitly out.

## Deployed (2026-09-19, Brandon: "deploy"; Grok accepted in `GROK_REST_4A_REVIEW.md`)

Civic first, then daemon, then client, in that order. nginx untouched.

1. **Civic drift check first:** the droplet's `http_server.cpp` md5
   (`81eb0e5e...`) **equals** the version before this round (`d9a36e5~1`), so
   nothing had been hand-edited live; the sync's dry-run sent exactly that one civic
   file. Daemon dry-run: `schema.sql` (comment only), `civic.cpp/.hpp`,
   `house.cpp/.hpp`, `main.cpp`; client: `index.html`. The droplet's client was still
   `9a8ac18c...`.
2. **Online game-DB backup:** `/var/lib/commons/backups/commons.sqlite.pre-open-20260919T093423Z`,
   `integrity_check` ok (items 3, recipes 3, maps 14, map_instances 6, players 5,
   placed 3, inventory 4). Left on the box.
3. **Civic:** synced with `deploy/sync-to-droplet.sh` (data exclusions hardcoded),
   `-j1` build on the droplet (1m20s), `systemctl restart liquidgov`. Public `/` and
   `/commons/` 200. **Public `/internal/blocked/alice` is 404** (as are
   `/internal/friends` and `/internal/session-user`). On the box: same-box, no session
   answers `{"blocked":true}` 200; with `X-Forwarded-For` it is a 404;
   `/internal/session-user` still works. The accounts store's mtime is from before
   the deploy (untouched).
4. **Daemon:** synced, `-j1` build, no reconfigure, restart. Live DB read-only:
   integrity ok, every count identical to the baseline, all 5 players still
   `friends`. Through nginx: lots / `1,1` / house `/placed/` 200; a guest's lots
   `{"lots":[]}`; `house-policy` with no cookie `403 not_bound`.
5. **Client:** droplet = served = local `index.html` `95f997e7...` (was `9a8ac18c...`;
   tiles/sprites unchanged); served page carries `HOUSE_POLICY_NEXT`. A guest walk on
   production: loads, joins, console clean.
6. Journal: no failures; `NRestarts=0` for both services.

**Not seen on production:** an owner actually flipping a house to `open` and a
stranger's door lighting (needs two real logged-in accounts; I don't sign in to
production). The check for you (two accounts, or you plus someone): set your door to
`open` at the latch (Enter cycles friends -> open -> locked); on the other account,
who is **not** your friend, go to Homestead Row: your house should appear as a lit
door; walk in. Block that account from LiquidGov and, within about 15 seconds of
their next move, they should be walked out and the door should vanish for them.
