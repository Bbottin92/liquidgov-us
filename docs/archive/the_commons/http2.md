# HTTP/2 multiplexing — not the daemon

Not a BUILD. Complements `nginx.md` and `http.cpp`. Do not add h2 or
nghttp2 to `commons-daemon`.

## What multiplexing is

HTTP/1.1: one request at a time per connection (pipelining exists, is
broken in practice, and this daemon already `Connection: close`).
HTTP/2: one TLS TCP connection, many **streams**, interleaved frames.
A slow `/presence/1,1` GET does not hold a second stream's response.

HTTP/2 still HOL-blocks on **TCP** (a lost packet stalls every stream).
HTTP/3/QUIC fixes that. Neither belongs in a 512 MB sibling this week.

## Split that already matches the spec

| Hop | Protocol |
|---|---|
| Browser → nginx (`liquidgov.us`) | TLS, likely already `http2` on 443 |
| nginx → `127.0.0.1:8765` daemon | HTTP/1.1, `Connection: close`, one request per accept |

`nginx.md`'s presence `location` is `proxy_http_version 1.1`. That is
correct. nginx **terminates** h2 (and h2c if anyone is foolish) and
speaks 1.1 to the game process. The daemon never sees a SETTINGS frame.

Do not `listen 0.0.0.0 http2` on the daemon. Do not ALPN on loopback.

## Why 2 Hz occupancy does not need streams in C++

- Caps 96. Single-threaded `accept` + `serve_one` is enough.
- Presence is either short GET polls or an SSE/WebSocket that nginx
  already set `proxy_buffering off` for (`nginx.md`). SSE is HTTP/1.1
  chunked; WebSocket is an Upgrade. Neither is HTTP/2 stream 7.
- Multiplexing 12 region GETs in **one** browser connection is nginx's
  job if the client uses h2 to the site. The daemon still sees 12
  sequential 1.1 requests. Fine.

## What would actually hurt

Putting h2 **inside** the daemon: HPACK, flow control, a frame parser,
stream state. Civic `http_server.cpp` is 1.1 by design; copy that, not
Chrome.

Enabling HTTP/2 **between** nginx and the daemon (`grpc_pass`, h2c
upstream) for 96 sprites: more moving parts, no extra occupancy.

## For this repo

Keep `HTTP/1.1` + close on loopback. If civic's nginx `listen 443 ssl
http2` already multiplexes the public site, `/commons/` inherits it for
**static** files. Presence still 1.1 upstream. Body cap 64 KiB stays a
1.1 `Content-Length` check.

No deploy. Idle reap / server-issued ids still the public-nginx gate.
