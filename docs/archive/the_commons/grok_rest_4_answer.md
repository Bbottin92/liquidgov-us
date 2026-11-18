# Grok answer — split. BUILD **4a open** first. Discovery **A**.

Do **not** merge 4b. Do not ship C. Do not ship B.

## 4a — `open` (this BUILD)

**A.** `/presence/lots` = online mutual friends first, then online
`open` houses of non-friends, cap 6. That is the discovery path. An
`open` policy with no door is a no-op; you caught that.

Semantics, as you wrote:

- Bound only. Guests never.
- Owner **online** to enter. Staying re-checks policy + blocks, not
  "still online".
- Latch: `friends` → `open` → `locked` → `friends`. Cookie only.
- `locked` / `friends` unchanged.

### Civic: `GET /internal/blocked/<username>`

Fourth named civic edit. Same-box gate as session-user (XFF or
non-loopback → plain 404). Cookie is the **viewer**. Path is the **owner**.

`{"blocked":true}` if either side blocked the other.

**Fail closed:** no session, unknown owner, malformed path →
`{"blocked":true}` (200, so the daemon does not confuse it with the
public 404). Timeout / connection error: daemon treats as blocked and
**does not cache** (same as session-cache: never remember a failure).
A real `{"blocked":false}` is the only thing that can open a door.

Daemon cache: 15s RAM, key `(Cookie value, owner)`. Not disk. Not friends.

`POST /presence/house-policy` now accepts `open` as well as friends/locked.

## 4b — orphans (later, own BUILD)

Keep every valve: `user-exists` explicit `exists:false` only; two consecutive
passes; **log-only first deploy**; cap 3; online backup. Inheritance after
`/settings/delete` is the real reason. Not this slice.

## Held

No LGP1. No username prompt. No nginx. Deploy 4a: **civic first**, then
daemon, then client, after review, when Brandon says `deploy`.

Fail: enter-by-name, a public blocked route, deleting rows in 4a.
