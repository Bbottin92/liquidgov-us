# QUIC stream ordering — why QPACK exists

Not a BUILD. Complements `http3.md`. Do not open UDP on the game port.

## Two orders

QUIC is many **streams** on one UDP connection (connection ID, not a
TCP 4-tuple).

| What | Ordered? |
|---|---|
| Bytes **inside** one stream | Yes. STREAM frames carry offsets; the receiver reassembles like TCP. |
| Streams **relative to each other** | **No.** Stream 7 can finish before stream 3's first byte. |
| Connection | A lost packet delays only streams that needed those frames. |

That is the whole HTTP/2-on-TCP complaint: one loss stalled every h2
stream. QUIC's answer is per-stream reliability, not a global byte
pipe.

Stream types: client-bidi (requests), server-bidi, uni (control,
QPACK encoder/decoder, HTTP/3 control). IDs: even/odd + two bits for
initiator and uni/bidi. Reset (STOP_SENDING / RESET_STREAM) abandons
*that* stream; the connection stays up.

## Why HPACK broke

HPACK assumes **one ordered header stream**. Insert #5 is visible to
every later HEADERS block because they arrive after it. On QUIC,
request stream 11 can arrive **before** the encoder-stream insert that
its indexed header needs. HPACK would decode the wrong row or stall
forever.

QPACK's encoder/decoder **uni** streams restore a single ordered log
of table edits. Request HEADERS name a **required insert count**.
If that count is in the future, the request is **blocked**, not
mis-decoded. `SETTINGS_QPACK_BLOCKED_STREAMS` caps how many may wait.
Never-indexed `Cookie` never waits on the table (`hpack.md`).

## This repo

The daemon has **one** TCP accept, one request, `Connection: close`.
There is no second stream to interleave with. Loopback HTTP/1.1 is
already totally ordered for that one body. Occupancy 2 Hz does not
need 4 QUIC streams so `/presence/1,1` and `/presence/1,2` can complete
out of order — nginx can already fan those out as two 1.1 upstreams.

A crashed tab is still an idle-reap problem (`presence_accounting.md`),
not a missing STREAM offset.

## Reviewer fail

- STREAM/RESET framing in `http.cpp`
- Assuming QUIC “reorders HTTP bodies” (it does not, per stream)
- Indexing `lg_session` because “QPACK blocked streams are free”
- UDP 8765

No deploy.
