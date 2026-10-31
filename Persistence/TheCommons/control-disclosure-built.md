# "Who holds control" tucked behind a disclosure — DEPLOYED 2026-09-27 (civic only, verified live)

Brandon: `BUILD control disclosure`, after a relayed Grok request (`/laws/1`: the chapter-audio button was already live and done differently
— real recorded narration, `speechSynthesis` was superseded — nothing to do there; the "Who holds control" link was still a plain nav/footer item).
**civic only** (`liquidgov-platform`); the commons client, the daemon and nginx are untouched. Deployed on Brandon's `deploy` the same day.

## What changed

Two edits to `liquidgov-platform/src/http_server.cpp`, both string literals:

- **The shared footer** (`page_wrap`, used by every rendered page, signed in or out): `<a href="/control">Who holds control</a>` came out of the
  inline `Genesis · Laws · The Commons · ... · Privacy` row and now sits behind a closed `<details><summary>Disclaimer</summary>...</details>`
  at the end of that row, same visual family as the "Delete…" disclosure this file already uses elsewhere.
- **The guest nav** (`nav()`, logged-out only — the logged-in account menu never had this link): same change, same `<details>` pattern, in
  the same position between "The Commons" and "App".
- `/control` itself is unchanged (still `pg("Who holds control", control_body())`, still reachable, still 200).
- **Deliberately left alone:** the `/laws` overview page's own editorial line, `"Who holds control of this site" · "Genesis disclosure"`. Its
  link text differs from the exact chrome link ("of this site" is not in the nav/footer wording) and it's body content, not navigation chrome —
  the ask named the nav and the footer specifically.

## Verified (real compiled binary, local, throwaway store)

- Guest nav on `/laws/1`: exactly one "Who holds control", inside a closed `<details>`, not a bare nav item (`<a href="/control">...</a><a` no
  longer appears — a bare item would be immediately followed by the next `<a>` with nothing between).
- Footer on `/laws/1`: same, exactly one occurrence, inside a closed disclosure with a "Disclaimer" summary.
- The chapter audio button (`.listen-pulse`) is untouched.
- `/laws` overview: its own differently-worded mention is unchanged, as intended.
- `/control`: still 200, still renders its own `<h1>Who holds control</h1>`.
- A signed-in page (`/laws` after a real signup): the footer disclosure is there too, confirming it's universal via the one shared `page_wrap`.
- Exactly one `<footer` and one guest `"<nav>` template exist in the whole file, so there is nowhere else this link could be hiding.
- Civic's own log: no errors during any of the above.

civic has no CI and no automated test suite (only `commons.yml`, which is the game daemon's); this repo's convention for civic changes is a
local build against a throwaway store plus a live check after deploy, which is what was done here and will be repeated live.

## Found on the way, set aside, not part of this build

An **uncommitted, undocumented** 15-line fix was sitting in the working tree on `http_server.cpp` when this build started: an iOS Low Power Mode
fix for the intro video's autoplay (a `play()` promise rejection handler plus a 1.5s "still paused" watchdog, on top of the existing 12s
fallback), with a real bug trace cited from iPhone Safari, 2026-09-21. It predates this build, was never committed by whoever wrote it, and has
no test or doc trail. **Not touched, not shipped here** — stashed (`git stash`, message "orphaned uncommitted iOS intro-autoplay fix
(http_server.cpp), found 2026-09-27") so it isn't lost. Brandon has been told; it needs its own decision (finish + verify + commit, or drop).

## Deployed (2026-09-27, from 3efe548)

Standard civic recipe: drift-check (droplet matched the last deployed commit, 9e4611a), binary backed up, sync (content
change was exactly `src/http_server.cpp`), chown immediately after the sync, build, restart only after confirming the
binary rebuilt. 0 crash lines during the build and 0 since the restart; accounts store checksum unchanged;
`commons-daemon` and nginx untouched.

Confirmed live on `/laws/1`: exactly one "Who holds control" in the guest nav and one in the footer, both inside a
closed disclosure, neither a bare nav item; `/control` still 200; the `/laws` overview's differently-worded mention
untouched; the chapter audio button unaffected.
