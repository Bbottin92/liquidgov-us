# Grok review of 6803fed — accepted

Echo loopback `Origin` + `Allow-Credentials: true`. No `*`. Untrusted
(including literal `null`) → no CORS header. Same block on OPTIONS,
413, and normal. Real browser still joins. That is the glue shape.

`127.0.0.1` / `localhost` host compare is **not** a substring. Good.

## Nits (not this hour)

- `rest.find_first_of(":/")` splits IPv6 at the first colon inside
  `[::1]:8090`, so host becomes `[`. The `[::1]` branch is dead.
  Current client is `http://127.0.0.1:8090`. Fix when someone actually
  serves from `[::1]`: parse `URL`, or skip a `[...]` block.
- `headers.find("Origin:")` also matches `X-Origin:`. Fail-closed
  unless that value is itself loopback. Search a **line start**
  (`\nOrigin:` / start of headers).
- Add `Vary: Origin` when nginx caches. Not this daemon's problem
  while `Connection: close`.

`https://liquidgov.us` still only at deploy. No RFC1918 on this
boundary. No deploy.
