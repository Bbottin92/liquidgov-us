# HPACK — not the daemon

Not a BUILD. Complements `http2.md`. Do not link nghttp2 or write a
header table in `commons-daemon`.

## What it is

HTTP/2 header compression. Each side keeps a **static table** (common
names like `:method` / `content-type`) and a **dynamic table** (recent
headers, shared state per connection). A header can be:

- indexed (one integer into the table)
- literal with incremental indexing (send once, remember)
- literal never indexed (send every time, **do not remember**)

Strings may also be Huffman-coded. This is why h2 requests look tiny
after the first.

## Why it is dangerous for cookies

The dynamic table is **shared across streams on that connection**.
If you index `Cookie: lg_session=…`, a later stream can refer to it by
index. Compression-against-secrets is the CRIME/BREACH family: an
attacker who can influence other headers and watch ciphertext length
can recover table entries. HTTP/2's answer is **never index** on
`Cookie` / `Authorization` / `Set-Cookie` (RFC 7541 §7.1.3, and
browsers do this).

`lg_session` is HttpOnly, host-only, Secure when proto is https
(`cookie.md`). It must not sit in an HPACK dynamic table **in our
process**. We do not speak h2, so we do not have that table. nginx
might, on the public hop — that is nginx's problem, and it already
knows not to index Cookie.

## This repo

| Hop | Headers |
|---|---|
| Browser → nginx | h2 + HPACK, possibly; Cookie never-indexed |
| nginx → daemon | HTTP/1.1 **plaintext** on loopback, full `Cookie:` every request |

The daemon's `http.cpp` copies the raw header block and does not parse
`lg_session` (`grok_glue_review.md`: forward the header to civic, do
not open LGP1). No Huffman. No table. Body cap is `Content-Length`,
not a compressed frame size.

Do not “optimize” presence GETs by adding h2 so `Cookie` is one byte.
96 × a 200-byte Cookie on loopback is nothing. A bug in a dynamic
table next to the session secret is not nothing.

## Reviewer fail

- A HPACK encoder in `commons-daemon`
- Indexing `Cookie` “to save RAM”
- Confusing HPACK with gzip of the **body** (`Content-Encoding`)
- Enabling h2c upstream so nginx and the daemon share a table

No deploy. Idle reap / server-issued ids still the public-nginx gate.
