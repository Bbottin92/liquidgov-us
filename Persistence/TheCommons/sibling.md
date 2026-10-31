# Droplet sibling — how the game sits next to LiquidGov

Not a build. V1 (`commons/index.html`) stays a static preview until
Brandon says glue/sibling. Civic binary stays frozen. Grok does not SSH.

Two programs, one person, one 512 MB / 1 vCPU box
(`143.198.159.58`, `/opt/liquidgov`). A game write must not take down
Genesis.

## What “sibling” means

| Process | Owns | Dies alone? |
|---|---|---|
| `liquidgov_http` | login, friends, mail, votes, canvas, Juketube, `/proposals` | already live |
| **game daemon** (new) | tiles, presence RAM, SQLite ledger, interiors | **must** |
| nginx | TLS, static `/commons/`, proxy to both | |

Not: new LGP1 tags, Mongo, React on the civic site, game state in
`http_server.cpp`.

`commons/index.html` as shipped is the **client**. The sibling is the
**process the client talks to** for presence and, after glue, inventory
`placed` rows. Hall already talks to civic by URL; that does not go
through the daemon.

## Memory / CPU (the real constraint)

Droplet is 512 MB / 1 vCPU with civic already on it.

- Presence: 12 region maps, cap 24/region, 96 global (`presence.md`),
  ~2 Hz only on occupied regions. That is kilobytes + a light loop.
- SQLite: catalogs + instance rows, WAL. Not a 10 Hz flush.
- Static tileset: nginx, not the daemon.
- Budget the daemon **small** (tens of MB, `MemoryMax=`). If it OOMs,
  systemd restarts *it*. Civic keeps serving `/`.

Do not run a second Node/React SSR next to C++ civic. Static files + a
tiny daemon (C++ or similar, same operator). The preview’s canvas client
is already vanilla JS.

## nginx shape (cookie is the fork)

`glue.md` left origin undecided. Mechanics, not a lock:

**Same-origin path (simpler cookie):** `https://liquidgov.us/commons/`
serves the client; `https://liquidgov.us/commons/ws` (or `/presence`)
proxies to `127.0.0.1:<gameport>`. Browser sends the existing civic
session cookie automatically. No `Domain=` change. No CORS.

**Subdomain:** `commons.liquidgov.us` needs the civic cookie’s `Domain`
to include `.liquidgov.us` (read the real `Set-Cookie` in
`http_server.cpp` first — do not invent a name here). CORS +
`SameSite` get real. More moving civic parts.

Until that read happens, assume **path `/commons/`**. Blocks and
gotchas: `nginx.md`. Hall stays an
absolute `https://liquidgov.us/proposals` link either way.

## What the daemon actually does

1. **Static is nginx.** Don’t make the game binary serve the PNG/HTML
   unless you have to. `alias /opt/commons/` (or a dir beside
   `/opt/liquidgov`, **not** inside the LGP1 store).
2. **Presence in RAM.** Occupant lists keyed by region/instance. Client
   subscribes to camera-overlapping regions at ~2 Hz. Empty region =
   silent. Snapshot bound players ~30s / on leave onto `players.last_*`.
   Guests never hit disk.
3. **SQLite file** e.g. `/var/lib/commons/commons.sqlite` (WAL). Owner
   `liquidgov:liquidgov`. **Never rsync the live DB** (same class of
   bug as rsync `*.lgp`). Schema is `schema.sql`. V1 client does not
   open this file.
4. **Friends:** daemon (server-side) calls civic on **localhost** with
   the forwarded session cookie. Civic returns mutual usernames for
   *that* session. Short RAM TTL. No `?username=`. No friends table.
5. **Wall-slot / photo:** later GET/POST to civic; daemon does not
   grow a feed.

Crash policy: `Restart=on-failure`. Civic unit is separate. A bad
presence tick must not SIGABRT Genesis.

## Bind / guests

V1: throwaway name, RAM only — already true in the preview.

Post-glue: same presence entry is **relabeled** with `lg_username`
(promotion, not a reconnect). House instance binds then. Homestead’s 6
doors start resolving. Until that cookie read + `/commons/` proxy
exist, boarded doors stay correct.

## Deploy split (when Brandon says so)

- Claude: systemd unit, nginx `location`, dir perms, SQLite path.
  Still sole `/opt/liquidgov` deployer; game dir is a sibling path.
- Grok: GitHub client + this spec. No SSH.
- Do not rsync `commons.sqlite`. Do not put WAL under `/opt/liquidgov`
  store.

## Not this week

P3 closed the outdoor preview. Sibling work is a **new BUILD**, not a
P4 in `index.html`. Do not open IndexedDB as a stand-in for this
process (`indexeddb.md`).
