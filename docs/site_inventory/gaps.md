## What the inventory was read from (q99, checked 2026-11-17 23:52 PT)

- **Live runs commit 5ddb61d** (read-only read of `/opt/liquidgov/DEPLOYED_COMMIT` on liquidgov.us: newest `5ddb61d`, previous `f9bcea1`, file written 2026-11-18 04:48 UTC; service up since 00:15 UTC). This inventory is read from exactly that commit (`git archive 5ddb61d` into `live-5ddb61d/`).
- **Live is ahead of master and of GitHub:** master `c7151eb` is 19 commits behind 5ddb61d (16 src/asset files differ: the static-files table, portrait history, map clarity). 5ddb61d is only on the local `wip/141-map-clarity` and `wip/142-process-fixes`; no remote branch contains it (GitHub pushes are held until a checkpoint, block until 2026-12-01).
- **Correction:** an earlier version of this file said the 21:47 run read "a source tree that was not recorded" and was replaced by a better run on master. Wrong way round: the 21:47 run's 199 route blocks / 165 URLs are exactly what the live commit gives; master was the stale tree.

## Who can use each route (replaces the old guess)

- Read from each route's own gate, the router's primitives: `if(!acc) return need_login();` (or a 401) = members only; `if(!is_operator(user)) return admin_404();` (or a 403, or `!acc || !is_operator`) = operators only. The router has no global gate (`router.cpp:85-107` hands each request to every feature in turn).
- An inner `if (path == ...)` inside a bigger route block takes the outer block's gate (the 5 found: `/problems/keep|forget|flow|still` in `map_routes.cpp:149`, `/admin/styles/dismiss` in `admin_routes.cpp:132`); nesting requires deeper indentation, so a block-end miscount can't fake it.
- Result on 5ddb61d: 114 members-only, 30 operators-only, 98 public (55 route blocks + 43 static files). The old guess found 6 gated routes.
- Checked by hand: every public route block was listed with any access-looking code in it; the remaining public ones with such code (`/mail/too-large`, `/officials`, `/portrait/*`, `/canvas-frame/*`, `/u/*`, `/login/google`, `/reset`) use it for content (a message, "your own" view), not to refuse.

## Link audit, checked by hand

- **Dead links: 0 real.** The one flagged (`/theme/` from `site/ui/shell.cpp:140`) is built as `"/theme/"+next_theme`; `/theme/dark` and `/theme/light` exist.
- **Routes with no caller in the HTML/JS (20 flagged), sorted by why:**
  - Built by string concatenation, so not orphans: `/admin/accounts/lock`, `/admin/accounts/unlock` (`admin.cpp:125`).
  - Reached from outside the page HTML, by design: `/internal/upload-ok` (nginx asks it before an upload, `router.cpp:42`), `/here/ticket` (the browser extension fetches it), the PWA icons and `/apple-touch-icon-precomposed.png` (browsers request them by name).
  - Redirect aliases for old or typed URLs: `/about` -> `/control`, `/profile` -> `/me`, `/app/mail` -> `/mail`, `/app/social` -> `/feed`, `/app/civic` -> `/proposals`.
  - Probably built by concatenation, NOT checked: `/manifest-mail|social|civic.webmanifest`.
  - **FIXED and live 2026-11-18 03:34 PT (unreach1, 918b53e):** `/terms` is linked from the footer; `/me` has a
    Remove portrait form again; `/me/post/canvas` and `POST /me/post` are deleted (404 live). The original findings:
  - **No caller in the pages (checked against code and git history; same on 5ddb61d):**
    - `/terms` (GET, `pages_routes.cpp:87`): a working page ("How this association works") that nothing has linked to since the repo was imported (a174377, 2026-10-27); the footer links Privacy, not Terms. **Unlinked page, not dead code.**
    - `/me/avatar/clear` (POST, `profile_routes.cpp:323` on live): its "Remove portrait" form was removed on purpose in 6c4291e (2026-09-10), handler kept. Account delete and admin still clear portraits directly. **A member can no longer remove their own portrait.**
    - `/me/post/canvas` (GET, `canvas_routes.cpp:21`): its link left with the /me composer in e015d27 (2026-09-10). It is the only page posting to `POST /me/post`, so that action is unreachable too. **Two routes kept on purpose but unused.**
    - `/map/instance-css` (POST, `map_routes.cpp:205`): operator-only JSON endpoint with no page by design (spatial step 10, 589cde3). **Not dead: an admin API without a UI.**

## Page scripts

- [scripts.md](out/scripts.md): all 19 `assets/*.js`, each with its own header comment (what it is for), the pages whose HTML loads it, the server code that names it, the URLs it calls, the page elements it hooks and the events it listens for. Read from the code literally; calls built at run time are not followed. `spatial.js` is loaded by no page yet (its own header says so); `sw.js` is the service worker served by the push routes.

## Open gaps (what this inventory does not cover)

- Pages are read from the C++ source, not the running site: HTML assembled at run time (concatenation, loops, store data) shows as fragments or not at all; object text is cut short. "Loaded by pages" in scripts.md only sees pages whose HTML was read; "named in server code" covers the rest.
- Helper functions are followed 2 levels deep; `parse_form` and `who_html` are called but their definitions were not matched.
- Routes with extra conditions are listed by their prefix (e.g. `/u/*` stands for `/u/<name>/jukebox/now` and others); the file:line points at the exact block.
- Whether live's running binary matches 5ddb61d was taken from DEPLOYED_COMMIT, not from comparing files on the server.
