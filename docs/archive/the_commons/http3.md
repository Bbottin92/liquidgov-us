# HTTP/3 / QUIC headers — not the daemon

Not a BUILD. Complements `http2.md` and `hpack.md`. Do not add
quiche / msquic / ngtcp2 to `commons-daemon`.

## QUIC, not TCP

HTTP/3 runs on **QUIC** (UDP). Independent streams: a lost packet does
not stall every request the way TCP + HTTP/2 still does. TLS 1.3 is
built into the handshake. There is no `“Connection: close”` hop; a
QUIC connection is a set of streams plus control frames.

The daemon stays **TCP HTTP/1.1** on loopback. nginx (or nothing) would
be the only place h3 ever terminates, same split as h2.

## Headers are QPACK, not HPACK

HPACK's dynamic table is one ordered stream — fine on HTTP/2, **racy on
QUIC** where HEADERS frames arrive out of order. QPACK splits that:

| Stream | Role |
|---|---|
| Encoder stream | “Insert this name/value into the table” |
| Decoder stream | acks / “I can evict now” |
| Request/response | HEADERS that **refer** to the table, or literals |

A header can be **blocked** until the encoder-stream insert arrives.
`blocked_streams` is a setting. `Cookie` / `Authorization` /
`Set-Cookie` stay **never-indexed** (same CRIME reason as HPACK).
`lg_session` must not enter the QPACK table in any process we write.

Literal vs indexed vs Huffman: same idea as HPACK, different wire
integers, plus a **required insert count** on each HEADERS block so the
decoder knows how much table state that block assumes.

## Hop table

| Hop | Headers |
|---|---|
| Browser → nginx | maybe h3/QPACK someday; Cookie never-indexed |
| nginx → `127.0.0.1:8765` | HTTP/1.1, full `Cookie:` every time |

`proxy_http_version 1.1` in `nginx.md` does not change if the public
`listen` grows `http3`. Do not `listen quic` on the game port. Do not
open UDP 8765. Body cap is still a 1.1 `Content-Length`.

96 occupancy polls do not pay for a QUIC stack on a 512 MB box that
already runs civic. Idle reap is still the occupancy bug; UDP will not
fix a crashed tab holding a cap slot.

## Reviewer fail

- QUIC in `commons-daemon`
- Indexing `lg_session` in QPACK “to save a round trip”
- Confusing QPACK with gzip of the body
- `listen 443 quic reuseport` edits on civic's nginx as a game task
- Assuming HTTP/3 removes the need to forward `Cookie` to civic

No deploy.
