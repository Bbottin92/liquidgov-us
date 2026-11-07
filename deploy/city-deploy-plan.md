# Deploy plan: city features to liquidgov.us

Written by Claude, 2026-11-06. **Plan only.** Nothing here runs until Brandon says **go** for this deploy.
The deploy itself is two steps: Claude first shows the dry run and preflight results, then waits for **go** again before the first live change.

## What goes live

Branch `city-slice1`, commit `dacf8c3`, on top of `6ab8234` (the jukebox fix live today):
city pages and People here, departments and councils, proposal cities, comments and reactions, the Map (`/map`),
Settings > Map and connections, Problems and solutions, framed pages for the map field, one installable app, and the in-page alerts strip.

New store records (each one makes the previous binary refuse to start, so the undo tool matters):
`ccity clist pcity pwho ctag ptag prply prpln prx mapp pain painn`.

## 0. Preflight (read-only; stop if anything is off)

Run from the PC with Claude's own key (`ssh -i C:\Users\Brandon\.ssh\id_ed25519_claude -o IdentitiesOnly=yes root@143.198.159.58`), and log each step as `ctx log claude server-read`.

1. **Live code = what we built on.** The checksum of `/opt/liquidgov/src/http_server.cpp` and `file_store.cpp` must equal `6ab8234`'s. If live differs (someone deployed since), stop and rebase first.
2. **Store path and size.** Read `LIQUIDGOV_STORE` and the `WorkingDirectory` from the systemd unit. Note the store file's size and the time it last changed.
3. **nginx headers.** Look for `X-Frame-Options` / `Content-Security-Policy` in the live nginx config. A site-wide `DENY` or `frame-ancestors 'none'` would blank the map's right-hand field: stop and decide.
4. **Disk space** for one store backup plus a build.
5. **Service healthy now:** `systemctl is-active` plus the last 50 journal lines, so we know what normal looks like.

## 1. Local, before touching the server

1. Clean build plus all suites on the exact commit: `scratchpad/verify_all.sh` (unit tests, slices 1-5, keep, map, map data, problems, undo).
2. Sync dry run: `LIQUIDGOV_DEPLOY_DRY_RUN=1 deploy/sync-to-droplet.sh`. Read the list. It must not contain any `.lgp`, blob or `.env` file.
   Expected: source, test and asset changes, plus new `assets/lgmap.js` and `lgmap.css`. The three deleted `app-*.html` files stay on the server; the script doesn't delete, and nothing serves them.
3. **Show Brandon steps 0-1 and wait for go.**

## 2. Live (after go; about 4 minutes, with a short outage only at the restart)

1. **Back up the store:** `cp -p <store> <store>.pre-city-<UTC>` and check that both sizes match.
2. **Keep the old binary:** `cp -p /opt/liquidgov/build/liquidgov_http /opt/liquidgov/liquidgov_http.pre-city`.
3. **Put the undo tool on the server:** copy `deploy/city-undo.py` to `/opt/liquidgov-tools/city-undo.py` (outside the synced tree).
4. **Sync:** `deploy/sync-to-droplet.sh` (no dry run).
5. **chown at once** (lesson: a sync resets ownership, and building before the chown crash-looped the site): `chown -R <service user> /opt/liquidgov`.
6. **Build:** `cmake --build /opt/liquidgov/build -j1`. If it fails, stop; the old binary is still running untouched.
7. **Restart:** `systemctl restart liquidgov`, then **read the journal** (not just is-active). Look for `unknown record type` or `store open failed`.

## 3. Check it's live (no test accounts; account creation needs Brandon's approval)

- Signed out: `/map`, `/problems` and `/city/<x>` redirect to login. `/assets/lgmap.js` and `/assets/lgmap.css` return 200. `/install` shows one app.
- `/app/social` redirects to `/feed`. `/manifest.webmanifest` returns 200, and so do the old `manifest-social` (the nginx check script uses it).
- A framed request (`Sec-Fetch-Dest: iframe`) carries `X-Frame-Options: SAMEORIGIN`; a normal page doesn't.
- Store: the new records only appear once people use the features; the first save keeps every old record. Compare the record counts per tag before and after.
- **Brandon clicks through on his own account:** Map, Settings > Map and connections, one Problems post.
- Journal again after 10 minutes.

## 4. If it has to come out (rollback, no full rewind)

1. `systemctl stop liquidgov`
2. `python3 /opt/liquidgov-tools/city-undo.py <store>` (dry run, read the counts), then the same with `--go`. It writes its own backup.
3. `cp -p /opt/liquidgov/liquidgov_http.pre-city /opt/liquidgov/build/liquidgov_http`
4. `systemctl start liquidgov` and read the journal.

What's lost: the city data, map settings and problems. Posts, mail, friends, groups and votes are all kept (tested: `tests/e2e/city_undo.sh`, 52 checks).
**Reinstalling later:** with the site stopped, run `city-undo.py --restore-privacy <the undo's backup> <store> --go` **before** the first start, so nobody who hid themselves reappears.

## 5. After a good deploy

- Log it: `ctx log claude deploy liquidgov.us --ref dacf8c3`.
- Add a dated note in `Persistence/notes.md`.
- **GitHub (separate OK):** master is 2 commits ahead of GitHub (Grok's jukebox fixes), and this branch adds one more. Proposed order: Grok pushes master, then `city-slice1` merges into master, then push. Not part of this deploy.

## Known gaps (said up front)

- **Older browsers** that don't send `Sec-Fetch-Dest` (Safari before 16.4) show the map's field pages with the full site header. That only looks worse; nothing breaks.
- **Relevance colours and thicker lines** are a first pass. Thickness is kept per device, by design.
- **Problems detection is English word patterns only.** Expect misses.
- `mail_install_hint()` is now unused code. Remove it in a later tidy.
