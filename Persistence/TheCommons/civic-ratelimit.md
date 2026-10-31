# Civic API rate limits — public vs loopback

Explore, not a BUILD. Do not add a limiter to `http_server.cpp` or nginx
for `/internal/*`. 4a/4b stay as reviewed.

## Two doors

| Door | Who | Limit we actually have |
|---|---|---|
| **Public** (`443`, nginx → civic) | browsers | nginx + civic's own request size / session cookie. `/internal/*` is **plain 404** because nginx sets `X-Forwarded-For`. |
| **Loopback** (`127.0.0.1`, no XFF) | commons-daemon only | Same-box gate. Not on the internet. |

A public rate limit (conn, req/s, 429) is a civic/nginx product decision for
`/login`, `/proposals`, mail. It is **not** a game task. Putting `limit_req`
on a location that also matches `/internal/` would 429 the daemon and look
like "civic down": session cache miss → guest, lots empty, orphan watch
resets. Fail closed for *open* houses; fail noisy for everyone else.

## What already bounds the daemon

- **15s RAM caches** on session, friends, and blocked. A walker at ~2 Hz does
  **not** hit civic at 2 Hz.
- **Guests never call** session/friends/blocked.
- **Orphan pass:** ~10 min, stop at first `Unknown`, cap 50 names, cap 3
  would-reap. One frozen civic costs **one** 1s wait, not N.
- **1s timeout** on the loopback fetch. No retry storm.

That is the rate limit. A token bucket in C++ in front of four internal
routes is a second clock to desync from those caches.

## If civic ever grows a public JSON API

Then: nginx `limit_req` on **that** location, never on `/internal/`.
Identity stays the cookie. No client-supplied `?username=` (already a pin).

## Fail if "explored" by

- `limit_req` on `/internal/`
- 429 from civic to the daemon (the daemon would treat it as Unknown/blocked)
- A Redis rate limiter
- Changing 4a/4b to "batch civic" in a way that skips fail-closed

Leave the caches. Leave the same-box 404. No new limiter.
