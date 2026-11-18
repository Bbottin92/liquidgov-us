# `try_files` + `alias` — why we banned it

Not a BUILD. Complements `nginx.md`. One `index.html`; no SPA fallback.

## What `try_files` actually does

```
try_files $uri $uri/ /commons/index.html;
```

1. For each **file** argument (`$uri`, `$uri/`): look on disk.
2. Last argument is an **URI** — internal redirect, new location search.
   Not a file path.

`root /opt;` + `location /commons/` maps `/commons/x` → `/opt/commons/x`.
`try_files` prepends `root`. That works.

`alias /opt/commons/;` **replaces** the location prefix. `/commons/x` →
`/opt/commons/x`. `try_files` often **does not use that replacement**
when it stats (nginx ticket 97, still the practical rule): it stats
`root` + `$uri` instead, or double-prefixes (`/opt/commons/commons/x`).
Either a false 404 or a file you did not mean.

The last argument `/commons/index.html` then re-enters `location
/commons/` → same `try_files` → **redirect loop** (`rewrite or internal
redirection cycle`).

## For this repo

URL prefix **is** the directory name: `/commons/` → `/opt/commons/`.
That is `root /opt;`, not a reason to `alias` + `try_files`.

`nginx.md` still prefers `alias /opt/commons/;` **without** `try_files`,
plus `index index.html`, so a move to `/var/lib/commons/static/` later
does not require the URL to change. Do not add `try_files` “to be safe.”

```
# ok — no try_files
location /commons/ {
    alias /opt/commons/;   # trailing slashes match
    index index.html;
}

# also ok — if the dir stays named commons
location /commons/ {
    root /opt;
    index index.html;
}

# fail
location /commons/ {
    alias /opt/commons/;
    try_files $uri $uri/ /commons/index.html;
}
```

Named-location workarounds exist. We do not need them: there is no
client-side router.
