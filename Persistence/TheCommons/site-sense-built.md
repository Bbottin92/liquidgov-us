# Site sense — built, not deployed

Brandon asked Grok and Claude to walk the whole live site and fix what
does not make sense, is unfinished, or is set up wrong.

What was already live (do not redo)

* `/world` → `/commons/`. Menu and homepage Walk in already point at the plaza.
* Share card (five stars + the Myspace/ballot-box/gameboy quote).
* Client polish (banks, pier, canopies, back-to-site pill).

What this round actually changes

Civic (`http_server.cpp` + `assets/apps.html`)

* Guest nav and the footer now have **The Commons**. Logged-out people
  on /laws or /control could not reach the plaza without going home.
* `/apps` and `/install` get a fourth tile: The Commons. The lede no
  longer says "three" while the share card is a three-way.
* `/privacy` and `/terms` — short, honest pages from facts already on
  /control. Not a lawyer draft. `/about` → `/control`. `/profile` → `/me`
  (or login).
* Footer Privacy. Robots.txt also disallows `/internal`.
* "This website is the door" now names The Commons.

Client (`commons/js/interact.js`)

* Hall confirm is honest for a guest: proposals need an account to vote.
  Still opens `/proposals`.

Left alone on purpose

* The old Quad websocket (`/world/ws`) is unused by the public door.
  Ripping it is a later civic pass — Juketube still shares that pattern.
* Workshop shutter, house open policy, rebind, orphan reaping: parked
  until named.
* No live-elections claim. No React. No LGP1 tags.

Deploy is civic first (binary + `assets/apps.html`), then the client.
After civic: `/privacy` and `/terms` 200, guest nav contains The Commons,
`/profile` 303s, `/apps` has Walk in.
