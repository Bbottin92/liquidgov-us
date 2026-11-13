# Cookie parse — `lg_session`

Not a BUILD. Do not add cookie code to `commons/index.html`. Do not
invent a second cookie. Confirm `http_server.cpp` on WSL still matches
this before sibling glue.

## What civic actually sets

From this repo, not guessed:

| Source | Header |
|---|---|
| `deploy/ux-on-droplet.patch` | same + `Max-Age=2592000` + `; Secure` when `g_https` |
| `CLAUDE_CODE_CONTINUE.md` | Session cookie `lg_session` HttpOnly SameSite=Lax |

No `Domain=` attribute. That is a **host-only** cookie for
`liquidgov.us`. `commons.liquidgov.us` will **not** receive it unless
civic login starts setting `Domain=.liquidgov.us`. Path `/commons/`
does, because `Path=/`.

`HttpOnly`: the canvas **cannot** `document.cookie` this. Parsing
belongs on the **daemon** (or civic), from the `Cookie` request header
nginx already forwards (`nginx.md`).

The value is a **session token**, not `lg_username`. Civic maps token →
user. The game must never treat the cookie bytes as a display name.

## Parse (daemon)

```
Cookie: a=1; lg_session=TOKEN; other=2
```

1. Split on `';'`.
2. Trim each part.
3. Exact name: prefix `lg_session=` on that part — not `strstr` on the
   whole header (`not_lg_session=` is a real footgun).
4. Value = after the first `=` of that part. Do not URL-decode unless
   civic encoded it (it doesn’t today).
5. Empty value (`lg_session=; Max-Age=0`) = logged out.
6. Unknown token = guest. Do not create a `players` row.

Preview’s Python (same shape):

```python
def cookie(headers):
    raw = headers.get("Cookie") or ""
    for part in raw.split(";"):
        part = part.strip()
        if part.startswith("lg_session="):
            return part.split("=", 1)[1]
    return ""
```

Compare the token to the session table in **constant time**. Do not log
it. Do not put it in a query string.

## Game client (JS)

Does **nothing** with cookies in V1. After glue:

- Browser on `https://liquidgov.us/commons/` **sends** `lg_session`
  automatically (`SameSite=Lax` + `Path=/`).
- `fetch("/commons/presence", { credentials: "same-origin" })`.
- Daemon looks up token → `lg_username`, relabels the RAM presence row
  (`presence.md`). Throwaway name stays until that succeeds.

## Fail

- `document.cookie` scrape in `index.html`
- A second cookie named `commons_session`
- `?username=` instead of the token
- Assuming `Domain=.liquidgov.us` (it is not set)
- Parsing cookies in nginx `map` and putting the token in a URL
