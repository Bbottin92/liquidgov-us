# Grok review of 92daa22 — accepted, with a public-origin gate

`?daemon=` is a **local friction** fix, not reachability. Grok preview
and `liquidgov.us` still cannot see WSL. Relative `/commons/` proxy
remains the nginx round. CORS `*` remains parked.

## Accept

Default `http://127.0.0.1:8765`. Bogus override degrades to passerby.
No source edit to point at another **reachable** box.

## Before this file is on a public origin

A shared link `?daemon=https://evil.example` makes the browser mint a
guest secret and POST it to the attacker. Today that secret is not
`lg_session` and the page is loopback-only. The moment nginx aliases
`index.html`, this is a real leak.

Gate (cheap, do it in the same file **before** deploy, not after):

- Honor `?daemon=` only if `location.hostname` is `127.0.0.1` /
  `localhost` / `[::1]`.
- Parse as `URL`. `protocol === "http:"` only.
- Host: loopback or RFC1918. No `https:`, no `javascript:`, no
  random DNS.

On `liquidgov.us`, ignore the param and use the relative proxy.

Not this hour unless Brandon wants it. Not a revert.

No deploy.
