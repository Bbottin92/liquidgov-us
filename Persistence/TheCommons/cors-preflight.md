# CORS preflight — what this daemon actually does

Not a BUILD. Complements `http.cpp`, `GROK_DAEMON_URL_REVIEW.md`,
`hpack.md`. Do not change CORS this hour.

## When the browser sends OPTIONS

A **simple** request (GET, or POST with `text/plain` / form) skips
preflight. `fetch` with `Content-Type: application/json` is **not**
simple. The browser first sends:

```
OPTIONS /presence/join HTTP/1.1
Origin: http://127.0.0.1:8090
Access-Control-Request-Method: POST
Access-Control-Request-Headers: content-type
```

No body. No guest secret. The real POST waits for 2xx + allow headers.

`sendBeacon` with a JSON Blob is also preflighted. That is why 70bc5b8
switched to `fetch(..., {keepalive:true})` — Beacon reported success
after a failed POST. Preflight succeeding is not delivery.

## What we ship (`http.cpp`)

Every response, including 413 and 204:

```
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, OPTIONS
Access-Control-Allow-Headers: Content-Type
```

`OPTIONS` → 204, no route body. Wildcard origin is legal because the
guest **secret travels in JSON**, not a cookie, and `fetch` is
non-credentialed (`credentials: omit` default).

`:8090` → `:8765` is cross-origin. Same-origin `/commons/` after nginx
**does not preflight for origin**, and may not need CORS at all.

## What `*` forbids later

Credentialed requests (`credentials: include`, `Cookie: lg_session`)
**cannot** use `Allow-Origin: *`. The browser hides the response.
Glue/cookie-forward is the moment CORS becomes:

- echo a **specific** `Origin` (loopback list now; `https://liquidgov.us`
  later), and
- `Access-Control-Allow-Credentials: true`, and
- **no** `*`.

Do not add Credentials: true while origin is still `*`. That combination
is invalid and fails closed.

`?daemon=` pointing at a stranger already POSTs the secret there
(`GROK_DAEMON_URL_REVIEW.md`). CORS does not save you from a URL you
chose. Allowlist the param before this file is public.

## Nits, not patches

- No `Max-Age` — browsers re-OPTIONS often. Fine at 2 Hz × 96 on loopback.
- `Allow-Headers` is only `Content-Type`. A later `Authorization` header
  would preflight-fail until listed.
- GET `/presence/<region>` without JSON is simple if no custom headers.
  Polls may skip OPTIONS. POSTs will not.

## Reviewer fail

- `Allow-Origin: *` + `Allow-Credentials: true`
- Reflecting `Origin` blindly (including `null` / `https://evil`)
- Putting the guest secret in a header instead of JSON so every move
  preflights with a new request header
- nginx CORS **and** daemon CORS both `*` after same-origin proxy
  (redundant; pick one hop)

No deploy.
