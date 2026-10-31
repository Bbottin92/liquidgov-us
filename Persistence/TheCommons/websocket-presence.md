# WebSocket presence — not this daemon

Not a BUILD. Complements `presence.md`, `http2.md`, `nginx.md`.
`commons-daemon` stays HTTP/1.1 `Connection: close` + ~2 Hz poll.
Do not add a WS frame parser to `http.cpp`.

## What we have

Each outdoor walker, ~twice a second:

1. `POST /presence/move` (id + secret + x,y,facing)
2. `GET /presence/<region>` (public occupant list)

Every call is a new TCP accept, one `serve_one`, close. Caps 96 / 24.
Secret in JSON. CORS `*` until glue. Interiors unnetworked; 30 min guest
idle vs 30 s plaza reap (`GROK_PRESENCE_PATCH_REVIEW.md`).

That is enough for V1 on loopback. Two tabs already see each other.

## What a socket would buy

| | HTTP poll (shipped) | WebSocket |
|---|---|---|
| Occupancy | Client pulls | Server **pushes** the region list when it changes |
| Move | POST body every 500 ms even if standing | Client send; skip identical x,y |
| TCP | 2 accepts / walker / second | 1 connection / walker |
| Auth | secret on every POST | secret (or cookie) on **handshake** only |
| nginx | `proxy_pass` 1.1 | `Upgrade` / `Connection` headers (`nginx.md`) |
| Dropped tab | 30 s idle reap | `close` → `leave` immediately + reap backstop |

96 walkers × 2 Hz × 2 req ≈ 400 short 1.1 calls/s. Fine on one box.
Sockets win **push** and **instant leave**, not FPS.

## Why not in `http.cpp`

The server is **one thread, blocking `accept` + `recv`**. A WS client
that never sends stalls everyone behind it. Real sockets need
non-blocking I/O (epoll/poll) or a second thread per connection — both
are a different program. Civic `http_server.cpp` is the same shape;
copy that, not Chrome.

SSE (`text/event-stream`, nginx `proxy_buffering off`) is a halfway
house: still HTTP/1.1, still one hanging GET per walker, still blocks
this accept loop. Same veto.

## Protocol sketch (later, explicit BUILD)

Only if Brandon asks, and only **after** an event-loop rewrite (or a
tiny dedicated presence process):

- Handshake: `GET /presence/ws` with guest secret as
  `Sec-WebSocket-Protocol` or first text frame `{id,secret}` — not
  query-string (logs).
- Client → server: `{op:move|rekey|leave, ...}` at ≤ 2 Hz.
- Server → client: `{op:region, region, occupants:[...]}` on change,
  not 10 Hz, not every move from every stranger.
- Reconnect: existing remint-on-403 path.
- Cookie bind: handshake forwards `Cookie:` to civic; never LGP1 here.
- nginx: `proxy_http_version 1.1; proxy_set_header Upgrade $http_upgrade;`
  Same-origin `/commons/` — then CORS `*` goes away.

Do not invent a second presence graph. Do not 10 Hz. Do not binary
frames this year.

## Reviewer fail

- `listen` 0.0.0.0 with WS “so the Grok preview can connect”
- Socket.io / npm
- Holding `serve_one` open as a fake WS
- Replacing the 30 s reap with “WS close is enough” (mobile browsers
  lie)

No deploy.
