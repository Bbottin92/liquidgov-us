# QPACK dynamic table

Not a BUILD. Complements `http3.md`, `hpack.md`, `quic-streams.md`.
No table in `commons-daemon`.

## Shape

Two tables per HTTP/3 connection, encoder and decoder **mirrored**:

- **Static** — RFC 9204 appendix A, 99 fixed rows (`:method GET`,
  `content-type application/json`, …). Never evicted. Indexed as
  `0b01` + T=1.
- **Dynamic** — a **FIFO of name/value pairs** the encoder has *inserted*
  on the encoder uni-stream. Capacity is bytes (name + value + 32
  overhead each), not row count. Default 0 until
  `SETTINGS_QPACK_MAX_TABLE_CAPACITY`. Civic/game do not set this;
  nginx might on 443.

Inserts append. Evicts drop from the **front** until the new row fits.
An encoder must not evict a row a still-in-flight HEADERS block might
index — decoder acks (Insert Count Increment / Section Ack) tell it
what is safe. That is the decoder uni-stream.

## Indices

HPACK relative index is “distance from the newest row” on one ordered
pipe. QPACK cannot assume that pipe.

- **Absolute index** — 0-based from the first insert on this connection,
  grows forever (mod wrapping in the wire integers).
- **Relative index** on a request — relative to that block's **Base**
  (usually the Required Insert Count). Same row, different integers
  depending on which HEADERS block you are in.
- **Post-base** — a row inserted *after* Base; the request is then
  **blocked** until the encoder stream catches up (`quic-streams.md`).

`SETTINGS_QPACK_BLOCKED_STREAMS` caps how many requests may sit in that
wait. 0 means “never block; send literals.” Fine for 96 occupancy GETs.

## Never-indexed

`Cookie`, `Set-Cookie`, `Authorization` are sent as **literals with
never-indexed** attribute. They occupy **zero** dynamic-table bytes.
`lg_session` must stay that way (`cookie.md`, `hpack.md`). A table that
held the session secret would be shared across every h3 stream on that
connection — CRIME-class length leaks plus one decode bug exposing
every tab.

Huffman on the **literal** is allowed. That is not indexing.

## This repo

Loopback HTTP/1.1 has no table. nginx → daemon repeats `Cookie:` in
plaintext. If 443 ever speaks h3, **nginx's** QPACK table is the only
one, and Cookie stays never-indexed there too.

Do not size a dynamic table for `/commons/presence`. The static table
already knows `GET` and `content-type`. The body cap is still 64 KiB of
**1.1** bytes (`http.cpp`).

## Reviewer fail

- A `std::deque` of headers in the game process “to be ready for h3”
- Inserting `lg_session` with incremental indexing
- Treating QPACK capacity as “number of players”
- UDP 8765

No deploy.
