# Static site generators — not a BUILD

Hugo, Eleventy, Jekyll, Astro, 11ty, Vite, and friends turn markdown or
templates into HTML at *compile time*. This repo already ships the HTML.

## For this repo

Do **not** add an SSG.

The Commons client *is* a static site: `index.html` + `style.css` + eight
classic `js/*.js` files + two PNGs, rsynced to `/opt/commons`, served by
nginx `alias`. There is no template language, no markdown-to-HTML pass,
no content-hashed filenames. That was the split lock (A): same globals,
no modules, **no build step**.

An SSG would fight:

* Two-phase rsync (css/js/png first, `index.html` last). A generator
  rewrites the shell every build and usually fingerprints `world.a1b2.js`.
  The load-order test and the rollback (put the old `index.html` back)
  both assume filenames do not move.
* `file://` and a local static server. Hugo's `baseURL` and Eleventy's
  path prefixes break that.
* CI. The client tests load the files with a list of `<script src>` in
  a fixed order. A generated bundle is a different program.
* Civic. `liquidgov_http` renders pages from LGP1 at request time.
  That is not a static site. Putting Hugo in front of `/proposals` would
  be a civic rewrite, which stays closed.

nginx is the "generator": it copies bytes. `Cache-Control: no-cache` is
the cache. Do not add a `_site/` output directory.

## If you ever write a real SSG (not here)

1. Source in, artefact out. Never edit the output by hand.
2. Pin the generator version. "latest Hugo" is a different site next month.
3. Pretty URLs are a trailing-slash decision. Pick one and make the server
   match (this project's nginx already has `= /commons` → `/commons/`).
4. Don't hash filenames unless you also rewrite every referrer in the same
   atomic publish. That is why the Commons split kept `js/world.js`.
5. A static site still needs headers (MIME, cache, security). The SSG
   does not replace nginx.

None of that belongs next to `commons/` or `liquidgov-platform/`.
Client stays vanilla files. Civic stays C++. No code change.
