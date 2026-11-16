# Notes — ideas, decisions, parked work

Last updated: 2026-11-07

Shared working memory for **ideas and product decisions** (Grok + Claude).
Repo path: `Persistence/notes.md` on `Bbottin92/liquidgov-us` `master`.

Not bug classes. Not “what is live on the droplet.”

**How this file is used**
- Session start (both): read this file.
- During a turn: if Brandon decides, rejects, or parks something, append a dated
  entry *this turn*. Do not wait for “task done.”
- One heading per topic. Newest dated bullet at the top of that heading.
- Durable facts only. No transcript dumps. If it can be grepped from git, skip it.

---

## Shared pad

- **2026-11-16 -- A randomize ("surprise me") function (Brandon: "maybe there could be like a randomize function sometimes too"; WANTED, nothing built).** Claude's notes for later (not decided): one dice button that picks a random destination from what the person is ALLOWED to see (an interest view, a post, a pool, a person who opted in to being met), so privacy and blocks apply before the dice are rolled, never after; it is the same dial as the anti-echo-chamber feed option (how unfamiliar), with a plain "not this" that remembers the dislike; it never repeats a place you just left; and it is a button you press, never something the site does to you on its own. Fits the wander idea (interests as computed views) and the branching roads (a dice at every fork).
- **2026-11-16 -- REJECTED: a made pool per interest, and anyone owning an interest (Brandon).** "idk how youd have a pool for every possible interest.... and i dont think it would be right for any one person to have control of an interest. theres gotta be another approach". Claude's replacement proposal (not decided): an interest is never created or owned; it is a VIEW the server works out on the spot from what people already wrote (the words on profiles and in posts/proposals), exactly like today's interest boxes on your own map (map_graph.cpp: an interest links to matching posts and proposals; the people links exist in code but are OFF until the signup privacy step, Brandon 2026-11-12). So: any word or phrase can be walked to (from your own interests, from a post that mentions it, from search), nothing is stored per interest, there is nobody to be in charge, and the only moderation is of the posts themselves, as now. Rabbit hole = neighbouring interests worked out from which words appear together. Meeting people = opt-in per interest ("open to chat about X"), default off, and it needs the signup privacy step first.
- **2026-11-16 -- Rabbit holes and meeting people by interest (Brandon's question; discussed, nothing built).** Brandon: "what if i wanted to just go down an interesting rabbit hole or meet someone who has an interest in a particular hobby? how can i facilitate making these kinds of connections as seamlessly as possible and in a way that keeps everyone involved interested". Claude's proposal (not decided): (1) each interest/hobby is a pool you can swim into, showing who is there now (presence rules apply), the best posts about it and its neighbours (related hobbies); (2) a "wander" button/road that takes you to the next pool by overlap with what you like, with a dial for how unfamiliar (the same dial as the anti-echo-chamber feed option); (3) low-pressure introductions: an "open to chat about X" flag per interest (default off), a wave instead of a message, a shared activity in the pool (spaces/games) as the icebreaker; (4) both sides see WHY they were matched ("you both listed woodworking") and either can decline silently; (5) what keeps people interested is a real fit and small reciprocal steps, not engagement tricks. Exists today: interests on profiles, interest links on the map with the hidden-from-map rules (map_graph.cpp, tests map_interest_*). Open: whether interests become shared pools or stay words on profiles, who may create a new interest pool, and moderation of them.
- **2026-11-16 -- The map as a branching road system, and an object for every foundational page (Brandon; WANTED, nothing built).** Brandon: "i wanted to give the foundational pages an object equivalent wherever possible, just to give the concept a more distinct and intuitive form. then places would optimally be like other pools you can walk to". The picture: you go down the main stream and it splits four ways, **people / places / ideas / things**. To reach a friend's place you can take the places trail or the people trail, or, if you know a post they commented on, **ideas road**. People splits **known / unknown**; known splits into things like **friends, groups, family**; friends splits into **inner circle** and whatever else; the inner circle path ends in a pool with **up to 12 branches, one stream to each inner-circle friend**. A friend's stream either goes to their profile or **splits in two: their profile, and wherever they are standing right now**. Claude's notes for later (not decided): the four branches and the sub-branches are virtual pools (no data of their own, just a place to choose a road); inner circle is a new list you keep (cap 12) and needs a place in the account file; "where they are standing" must obey the presence settings that exist (hide presence, blocks, friends-only) and never show a place the person hid; the ideas road is what search and the post/comment links already do (a post pool links to its comments and authors); each foundational page gets one object in the pool where it lives (see the concept drawing above). UPDATE same day (Brandon: "idk about things actually. it just sounded right. because nouns"): the fourth road "things" is PARKED; start with three roads, people / places / ideas, and add a fourth only if something real needs it (the foundational-page objects live in your own pool, not on a road). Open: whether a person appears on several roads at once (yes: roads are views, not copies), how deep the branching may go before it is slower than a menu, and what the inner-circle list is called.
- **2026-11-16 -- Concept drawing of your own pool as a room with things in it (Brandon; WANTED, nothing built; "loose concept, feel free to adjust if something causes friction with coherence").** Image: an oval pool with a blue rim, your name big in the middle as "Brandon Bottin (Bbottin)" (the name you show, then your handle in brackets), a colourful pixel-heat floor with a doodle drawn on it, and objects standing on the back rim, each with a label: Juke Tube (a jukebox with a play button), Library (a bookshelf), Mail (a mailbox with its flag up), MAINstream (a sign), Web (a desk with a computer and chair), Canvas (an easel). Claude's reading, for later: each object is the door to one feature and opens it like swimming into a pool (Juke Tube = the jukebox, Mail = /mail, MAINstream = the feed, Canvas = the drawing tool, Library = the style library or your own saved things, Web = web pages as pools). Candidate coherence points: (1) the floor doodle is the pool's WHITEBOARD (already wished for profiles, groups and conversations), so the easel (the drawing tool) and the floor drawing need telling apart; (2) the floor's colour could be the pool's tint / mood (how people feel), which the stream legend already uses; (3) a flag on the mailbox when you have unread mail, like the badge today; (4) "Name (handle)" is what the pool and the page should both say; today the person pool says the name with @handle underneath. Open: what Library means, whether Web is a browser, which objects other people see in your pool, and whether objects can be moved or added (spaces).
- **2026-11-16 02:59 PT -- Search finds people by name; signup and profile ask for Name; person pools show the name (searchnames1) LIVE (commit `091a914`, PID 888615, public checks FAILS 0).** Build and restart (care pair), run by Brandon with `ship.py go searchnames1` and NO override: the CI gate is warn-only now (see the entry above), the site code merged as `e293213` (wip/120-search-names) and the pair as `091a914` (wip/121-deploy-searchnames1). 8 files went out. Not verified by Claude: on the live site, a real search by a first name; people who signed up before this still show the name they typed under "Display name".
- **2026-11-16 -- Claude's own checklist, so the conventions survive a context reset (Brandon: "its hard to remember to remember sometimes. make sure youre remembering").** Before every commit or deploy: (1) names: branches `wip/NN-name` with NN the next unused number across ALL branches (git branch -a), deploy names lowercase letters+digits like `previewadmin1`, e2e tests `area_what.sh` with the next free port from `scripts/next_port.py`, tools `tests/repo/*_check.py`; run `python3 tests/repo/naming_check.py .`; (2) wording: when a field or feature is renamed, grep every error message, label, placeholder, hint and test for the old name (the "Display name" leftovers); (3) frozen suites: do not edit a committed test to make it pass, change the code to meet it (ux_jump_autofill B3 caught a real conflict); (4) notes: a dated bullet the same turn Brandon decides, rejects or parks something; (5) deploys only on his explicit "merge" and "go"; the dry run is `ship.py deploy <name> "<purpose>"` from his main checkout; (6) never force-push or rewrite a pushed branch: copy the commits onto a fresh branch; (7) one full local CI run at a time, correct build path (check `ls $B/liquidgov_http` first), and no new builds while it runs; (8) tell Brandon what a thing is in plain words, and answer what he asked before changing anything.
- **2026-11-16 -- The CI gate is warn-only now (Brandon: "idk why theres a gate in the first place, i didnt ask for it" ... "cool with me"; BUILT on `wip/119-gate-warn`).** Nobody is on the site yet and the full local CI takes an hour, so: `deploy/ci-gate.sh` by default (`LIQUIDGOV_CI_GATE_MODE=warn`) prints `CI_GATE_WARNING ... not blocking` and lets a deploy through when there is no passing record or the newest run failed; `LIQUIDGOV_CI_GATE_MODE=block` brings the old stop back (its tests run in block mode, plus three warn-mode cases). `scripts/ship.py check`/`merge`: a MISSING CI result or phone VERDICT is now a printed WARNING and the branch is still SHIP_READY; a CI run that FAILED, a phone VERDICT that is not ALL-PASS, an unpushed branch or a non-fast-forward still stops. Unchanged on purpose: the backup before every deploy, the per-feature undo scripts, the post-deploy public checks, and the rule that deploys wait for Brandon's "merge" and "go". No more LIQUIDGOV_CI_OVERRIDE needed. Revisit when real people use the site. Full local CI still exists (`scripts/local_ci.py`) and can run after a deploy as a safety net.
- **2026-11-16 -- Admin link preview (wip/114-preview-admin): deploying with LIQUIDGOV_CI_OVERRIDE, Brandon's OK ("go ahead if you dont predict any issues").** No record exists for the exact tree: the last full local run (localci10) passed everything except two report-only walking-check warnings (walker_controls M5, walker_valve_types Y2/Y3), diagnosed as timing races in the test helpers; the fix touched only tests/browser helpers, and walker_controls_check then passed 4 of 4 (82/82 each). A fresh full run was stopped to save an hour. admin_preview.sh 24/24, hook checks 34/34. Targeted proof 2026-11-16: walker_valve_check 4/4 (3 checks each) and walker_valve_types_check 4/4 (5 each) after the fix, on top of walker_controls 4/4. Override reason to use: "preview-admin: test-helper-only change since a passing full run; walker_controls 4/4". Run by hand by Brandon (ship.py refuses overrides).
- **2026-11-16 -- Search finds people by the name they show, not just the handle (wip/117-search-names; built, tested, NOT deployed).** Brandon: "i want to be able to type in someone's name in search and find them. right now i have to know someones handle." `FileStore::typeahead` now also matches display names: the whole name starts with what you typed, or every typed word starts one of the name's words ("okaf priya" finds "Priya Raman-Okafor"; split on space . , -). Same findability and block rules as the handle (it only widens what is matched), still 3+ characters. Covers the map search bar, the people search and the jukebox person picker, which all go through `people_hits`. tests/e2e/map_search.sh 25/25 (8 new). Not done: showing the display name in the map's person pool (it still says the handle).
- **2026-11-16 -- Spaces: people build their own custom pool or app, as unlimited as is safe and cheap (Brandon's wish; nothing built).** Brandon: "i want people to be able to create a custom space that has as few limitations as safely and financially possible ... a linerunner game or solitaire ... to attract people to hang out there." Plan agreed in discussion: the owner uploads one small web app (HTML/CSS/JS, about 1-5 MB cap); it runs in an iframe with `sandbox="allow-scripts"` and NO allow-same-origin (empty identity: no cookies, no storage, no way into the page), served with a CSP that blocks network and outside files; their code runs on the visitor's device so server cost is storage plus a rate-limited relay. A tiny postMessage API: who is here (names only), send/receive room messages, small shared state (high scores). Multiplayer reuses a presence room per space. Safeguards: permanent "Made by <name>, not LiquidGov" banner, no top navigation, report button on every space, unlisted until reviewed or the author has trust, admin kill switch, Stop button/unload for runaway CPU. Staging: 1) own pool with owner-set look (name, colours, background), 2) filtered owner CSS, 3) full sandboxed apps. Presence today is one shared `/map` room with `instanceId` as a field (not a channel per pool); connection cap kMaxConnections=64 (http_server.cpp:33); a thread per connection; the event-loop rewrite is the next real step for scale, not a room per pool.
- **2026-11-16 -- Search finds people by the name they show, not just the handle (wip/117-search-names; built, tested, NOT deployed).** Brandon: "i want to be able to type in someone's name in search and find them. right now i have to know someones handle." `FileStore::typeahead` now also matches display names: the whole name starts with what you typed, or every typed word starts one of the name's words ("okaf priya" finds "Priya Raman-Okafor"; split on space . , -). Same findability and block rules as the handle (it only widens what is matched), still 3+ characters. Covers the map search bar, the people search and the jukebox person picker, which all go through `people_hits`. tests/e2e/map_search.sh 25/25 (8 new). Also (Brandon: "theres nowhere for people to type in their actual name" - the field existed but was labeled "Display name"/"How you want to be shown"): signup and /me now say "Name" (autocomplete=name, "people can find you by it"), and a person's pool on the map is named by that name with @handle underneath. All 11 map e2e scripts pass. Lesson: when Brandon reports something missing, check what he actually sees (the label) before explaining the code; and don't change things he only reported.
- **2026-11-15 23:52 PT -- Admin link preview: operator sets the site's preview title, description and picture from the admin page (previewadmin1) LIVE (commit `c14732e`, PID 885968, public checks FAILS 0).** Build and restart (care pair). Admin page /admin/preview (operator only): title, description, picture (JPEG/PNG, 2 MB) for og:/twitter: tags; picture at public /preview-image; reset brings back the built-in preview. Deployed with LIQUIDGOV_CI_OVERRIDE on Brandon's OK (see the entry above); master fast-forwarded by hand to 9884c7d (site) and c14732e (pair) because ship.py merge has no override. NOT verified by Claude: a real Facebook/X/iMessage crawl of the new preview (platforms cache old previews; the picture URL changes with each upload so a re-scrape picks it up); Brandon to set his own text and picture at /admin/preview.
- **2026-11-16 -- Admin link preview (wip/114-preview-admin): deploying with LIQUIDGOV_CI_OVERRIDE, Brandon's OK ("go ahead if you dont predict any issues").** No record exists for the exact tree: the last full local run (localci10) passed everything except two report-only walking-check warnings (walker_controls M5, walker_valve_types Y2/Y3), diagnosed as timing races in the test helpers; the fix touched only tests/browser helpers, and walker_controls_check then passed 4 of 4 (82/82 each). A fresh full run was stopped to save an hour. admin_preview.sh 24/24, hook checks 34/34. Targeted proof 2026-11-16: walker_valve_check 4/4 (3 checks each) and walker_valve_types_check 4/4 (5 each) after the fix, on top of walker_controls 4/4. Master fast-forwarded by hand to 9884c7d on Brandon's OK (option 1; ship.py merge has no override). Override reason to use: "preview-admin: test-helper-only change since a passing full run; walker_controls 4/4". Run by hand by Brandon (ship.py refuses overrides).
- **2026-11-15 14:00 PT -- Map pools, portal peek, map search, share button, credibility card, extension server side (pools1) LIVE (commit `adc70de`, PID 876850, public checks FAILS 0).** Build and restart (care pair), run from PowerShell by Brandon with `ship.py go pools1` after his "merge" for the site code (`e2acc9e`, wip/103-pools-search) and the pair (`adc70de`, wip/104-deploy-pools1). First deploy through the local CI record instead of a GitHub run (`deploy/ci-pass/eb8a122d...txt`, `CI_GATE_OK`; no override used): the full local run passed (lgcheck, every browser check, hook checks 34/34; commons skipped on wip branches as its old workflow did). 36 files went out: the pools map (oval pools from Brandon's drawing, channels with names at their mouths, layers by distance, your own pool named after you and its own room `user:<you>`), the portal peek (only when your drop is still, smaller, top of screen), the map search bar (`/map/search`: names, posts, comments, your messages, dates and times), Share to LiquidGov (`/share`, manifest share target), the credibility card in Settings, and the server side of the browser extension (`/x/<hash>` presence rooms, `/here/ticket`). Rollback: `/opt/liquidgov/liquidgov_http.pre-pools1` (previous PID 868219, bundle1). The browser extension itself is NOT deployed (load unpacked from `extension/`). Not verified by Claude: the live site on a real phone (Brandon's check: the pools, the search bar, your own pool showing your name, a search for "today"). Public check `commons` still passes: the Commons daemon is still running (see the Commons entry below; archive planned next).
- **2026-11-16 01:19 UTC (Nov 15 PT) -- The Commons daemon is STOPPED and DISABLED on the droplet; its database is backed up (Brandon ran the script from PowerShell; Claude read the output).** Backup: `/var/backups/commons-final-20261116.sqlite` (110 KB, made with Python's sqlite3 backup API; the droplet has no `sqlite3` command). `systemctl stop` and `disable commons-daemon`: inactive / disabled, no start at boot. `liquidgov` and `nginx` stayed active; `https://liquidgov.us/commons/` (the static closed notice) 200 and the home page 200 on a recheck (one earlier check printed 000, a one-off). Left in place on purpose: the unit file, `/opt/commons-daemon/`, `/var/lib/commons/commons.sqlite`, the nginx rules for the closed notice. To undo: `systemctl enable --now commons-daemon`. The Commons is now fully archived: repo (`commonsarchive1`, `archive/commons-final`) and droplet. Possible later tidy (not needed): remove the unit, `/opt/commons-daemon/` and `/var/lib/commons/` once Brandon is sure he will never want them.
- **2026-11-15 17:17 PT -- Archive the Commons: tooling tests to tests/repo, game daemon and its site routes removed (commonsarchive1) LIVE (commit `ac8b1f9`, PID 879829, public checks FAILS 0).** Build and restart (care pair), run from PowerShell by Brandon with `ship.py go commonsarchive1` after his "merge" for the site code (`d1a09bd`, wip/106-commons-archive) and the pair (`ac8b1f9`, wip/107-deploy-commonsarchive1); deployed through the local CI record (`deploy/ci-pass/4b564379...txt`, no override). Only 3 files differed from live (canvas_routes.cpp, system_routes.cpp, workflow_paths_check.py), so the website itself changed only by losing the four same-box `/internal/*` routes and `/canvas-frame/<user>/latest`. The public checks `commons` and `commons-post` (the static closed notice) still pass. The GAME DAEMON IS STILL RUNNING on the droplet (loopback 127.0.0.1:8765, unit `commons-daemon`): next step is a droplet script that backs up `/var/lib/commons/commons.sqlite` to `/var/backups/` and stops and disables the unit (Brandon runs it). Rollback if ever needed: `/opt/liquidgov/liquidgov_http.pre-commonsarchive1`; the code: branch `archive/commons-final`.
- **2026-11-15 19:38 PT -- Pools show what they are: posts, proposals and comments hold their text and author (poolcontent1) LIVE (commit `171d608`, PID 882142, public checks FAILS 0).** Build and restart (care pair), run from PowerShell by Brandon with `ship.py go poolcontent1` after his "merge" for the site code (`e2791be`, wip/110-pool-content) and the pair (`171d608`, wip/111-deploy-poolcontent1); deployed through the local CI record (`deploy/ci-pass/d6fb5022...txt`, clean run, no override). 9 files went out (3 map assets, 3 map server files, 3 tests). What changed on the site: the map's node data carries the whole text, author and time for posts and proposals; a comment is a pool of its own (`pcom:`/`rcom:`); a pool that holds text is sized to its text with a small quiet author row; the see-through preview no longer covers such a pool. NOT in it: group cover photos, traffic width and flow, thumbs and tints, events, hints. Rollback: `/opt/liquidgov/liquidgov_http.pre-poolcontent1` (previous PID 879829). Not verified by Claude: the live site on a real phone (Brandon's check: swim into a post, a comment, and a very long post).
- **2026-11-16 PT -- Future list: canvas zoom-follow, timelapses, and remixable posts (Brandon; WANTED, nothing built).** (1) ZOOM-FOLLOW drawing, a feature Brandon had on the shared canvas when he first made the site, with the Commons and the Studio: a switch you turn on; as you draw, the whole picture zooms in with your pen or brush tip as the centre, "more like a continuous cropping to achieve a zoom in effect", with an adjustable knob or slider for how fast it zooms in while you draw. (2) TIMELAPSE: watch a timelapse video of a drawing; save it; adjust the playback speed; trim or crop the video; set how long timelapses are kept and how often a new one starts, "any relevant situations, you get it". (3) REMIX: on posts that use images from the canvas, the author can allow others to take the image and draw on top of it or edit it, and to "branch off of your post as a response", which can be branched further "in various ways". Claude's notes for later (not decided): all of these (and the whiteboards) want ONE thing the canvas does not keep today, a stroke log (each stroke with its tool, colour, size, points and time, per person), because the canvas stores only bitmap frames; with it a timelapse is a replay of the strokes, a whiteboard is a stroke log many people append to, zoom-follow is a view that tracks the last stroke point, and a remix starts from another post's strokes or final image; the timelapse video can be made in the browser (replay onto a canvas and record it) so the server encodes no video, trim and crop likewise; zoom-follow is a view setting with a speed slider (0 = off) that eases the view toward the pen tip; a remix is a new post whose parent is the original, a tree of branches that could be drawn as pools joined by streams on the map; the author chooses per post whether others may remix it (off, allowed, allowed with credit shown), and the original is always credited on a branch; open: whether timelapses are per drawing or per day, and how long to keep them (storage cost), and who may remix by default.
- **2026-11-16 PT -- Future list: whiteboards, and the canvas fixes (Brandon; WANTED, nothing built).** (1) WHITEBOARDS: groups, profile pages, and one-on-one and group messenger conversations each get a "whiteboard", "basically the same as the canvas but multiple people can draw on it simultaneously". Options: wipes clean once a day, week or month ("or whatever"); no eraser allowed; hide the whiteboard entirely; save the image at certain times or intervals, or before or after certain events, e.g. when someone tries to wipe it clean it first saves a copy automatically "if you want". (2) The canvas itself: "needs to be organized better, it feels too cluttered"; "i want the ability to pinch zoom, right now it doesnt work" (a BUG to look at first); "the buttons and everything feel too square and rigid" (round them like the pools). Claude's notes for later (not decided): the whiteboard is a shared, live canvas (strokes as an append-only list from everyone, which merges naturally; sent live like the presence sockets); a whiteboard has settings: who may draw (owner only, members, friends, anyone, or everyone in a conversation), the wipe schedule, eraser on or off, shown or hidden, and snapshot rules (every N minutes, at set times, before a wipe or a clear, after N strokes); a saved copy is a frame, like the canvas frames the site already stores; the "who can see / who can draw" choice should reuse the same audience picker as thumbs and posts (coherence idea 3); blocks and moderation apply (a blocked person's strokes are not shown to you; drawing can be abusive, so reports work like posts); a conversation's whiteboard is seen only by its participants. Order Claude would suggest: the pinch-zoom bug and the rounder buttons first (small), then declutter the canvas with Brandon looking at screenshots, then the whiteboard (large).
- **2026-11-16 PT -- The admin link preview is BUILT (`wip/114-preview-admin`; not merged, not deployed).** A Preview tab on the operator tools (`/admin/preview`): shows how the card looks now, edit the title (up to 120 characters) and description (300), upload a picture (JPEG or PNG, 2 MB, 1200x630 works best), remove it, or reset everything to the built-in preview. Blank = today's preview, exactly. The words go into `og:title`/`twitter:title` and the description tags on every page; the browser tab title stays each page's own. The picture is served at the public `/preview-image?v=<version>` (the version changes when the picture does, so Facebook and X refetch). Stored in `<store>.preview` and `<store>.preview-image` (not an LGP1 record; see the entry below). Only the operator (Bbottin by default, `LIQUIDGOV_OPERATORS`) can open or change it; anyone else gets the plain 404. Test: `tests/e2e/admin_preview.sh` 24/24. Note for Brandon: Facebook, X and the rest keep what they have seen for a while; after a change use their own debugger tools to make them fetch it again. NOT done: cover photos on profile and group pages (still wanted), the real canonical `og:url` per page (it still says the home page).
- **2026-11-16 PT -- Simplified: forget the per-page previews; one preview Brandon edits from the admin page (Brandon; BUILDING on `wip/114-preview-admin`).** Brandon: "facebook and x would send you to the intro because youre not logged in... forget all of the foundational pages, i just want to be able to change the preview label thing myself from the admin controls page." So: ONE site-wide link preview (title, description, image) editable on a new Preview tab of the operator tools (`/admin/preview`); blank means today's preview. The cover photos for profiles and groups (and their own previews) are NOT dropped, only the per-foundational-page overrides are. Claude's build choice: the settings live in a small file next to the store (`<store>.preview`, the image in `<store>.preview-image`), not a new LGP1 record: a new record type would need an undo script and extra consistency checks for a rollback, an older binary ignores extra files, and a missing file falls back to today's preview.
- **2026-11-16 PT -- Cover photos, and a link preview ("url label") that admins can set per page (Brandon; WANTED, nothing built).** Brandon: "every profile page and group page should have an optional cover photo and that cover photo should appear on the url label or sticker or whatever its called when you share a url on facebook or X or instagram or any other place that has those" (the link preview card, Open Graph / Twitter card). For the admin tools: "the ability to change the url label for any of the foundational pages on the site": "a default that i can set and then a way to set one for each individual foundational page, so like liquidgov.us/feed, liquidgov.us/settings, liquidgov.us/apps, etc." And it must cover the URL forms people use: https://www.LiquidGov.US, http://www.LiquidGov.US, www.Liquidgov.US (no scheme) and https://LiquidGov.US, "etc". Facts found (Claude): today there is ONE preview site-wide (shell.cpp: one image `og-image-v4.jpg`, one description, `og:url` always `https://liquidgov.us/`, title = the page title), because `page_wrap` does not know the request path; the www and http forms already redirect to the apex (BUILD www apex, tested in `tests/repo/test_nginx_www.sh` incl. an upper-case Host). Claude's notes for later (not decided): (a) the template needs the page's own address (a per-request value, like the drop and adult cookies), so `og:url` is the real canonical address `https://liquidgov.us/<path>` whatever form was typed; (b) login-gated pages (/feed, /settings, /apps ...) redirect a logged-out crawler to the login page, so the preview for them must be served to link crawlers BEFORE that redirect (a tiny page with the tags that sends people on to login); (c) the admin tool: one default (title, description, image) and an optional override per foundational page, stored in the store (a new tag: lgp1_consistency and the undo-script rules apply); (d) cover photo: a new optional field on profiles and groups with an upload (the existing image ingest), shown on the page, in the group's pool, and as `og:image` for /u/<name> and /c/<slug>; a private profile or private group must NOT leak its cover in a preview; (e) open: cover shape and size (a wide banner cropped to the 1200x630 preview shape), which pages count as foundational (the dropdown menu's pages), and what an unset page shows (page title with the default description and image).
- **2026-11-16 PT -- A hint phase for newcomers (Brandon; WANTED, nothing built).** "i want to include a hint phase for newcomers that you can easily turn off and you can turn back on from the dropdown menu". The Hints setting already implied by the stream legend (Coherence answers: "only shows when hints are on (default when you create an account)") is this: on by default for a new account, easy to turn off, and turned back on from the dropdown menu. Claude's notes for later (not decided): hints are short tips shown the first time a feature is met (the map and swimming, the drop menu, search, the portal, thumbs and their privacy menu, the stream legend); each can be dismissed on its own ("got it") and there is one tap to turn them all off; the setting is on the ACCOUNT (follows you across devices), not the browser; a hint that has been shown and dismissed does not come back even when hints are turned off and on again, unless a "show hints again" action resets them; open: whether a newcomer phase also ends by itself (after some days, or after the features have been used), and what the dropdown menu item says.
- **2026-11-16 PT -- Pools show what they are, step 1: posts, proposals and comments (BUILT on `wip/110-pool-content`; not merged, not deployed).** Server: map nodes carry `body` (the whole text), `by` (author) and `ts` for posts and proposals; a comment is a pool of its own (`pcom:<id>` on a post, `rcom:<id>` on a proposal), linked from the post or proposal (newest 12) and back to its post and author; same privacy as before (a post you may not see has no comments for you; a commenter blocked either way is not there). Client: a pool that holds text is sized to its text (wrapped at the width that makes the smallest oval, so a very long post makes a big pool, Brandon's answer), with a small quiet author row on top (picture or initial, name, "3 h ago") and the whole text under it; the see-through page preview no longer covers such a pool (the text is already there). Tests: `tests/e2e/map_pool_content.sh` 14/14, `map_world_check` O8 (25 checks). NOT yet: group cover photos (a new upload field), event pools (events do not exist), proposal text beyond what `body` holds, thumbs, traffic width and flow, tints.
- **2026-11-16 PT -- Anti-echo-chamber option: answers (Brandon; WANTED, nothing built).** (2) YES: people can mark an injected item "not for me", so it learns what to leave out. (3) YES: "counter preference" is its OWN filter, separate from "unfamiliar" (Claude's note: define it by topics and groups you tend to avoid and people you rarely read, never by guessed politics). (1) global or per feed: Brandon asked what Claude suggests; Claude's lean, awaiting his OK: ONE global slider (how often an unfamiliar item is mixed in) with a per-feed override in the feed's corner menu (more, less, off), and the friends feed OFF by default because it means "people I chose" and strangers in it would change what it means; the broader feeds (everything, groups, near me, the map's search) follow the global setting.
- **2026-11-16 PT -- Feed filters, and an anti-echo-chamber option (Brandon; WANTED, nothing built).** Brandon liked all the proposed feed filters (most discussed, friends first, from my groups only, near me, unread/new since you last looked, most trusted by weighted track record) and added: "an option for things that you would otherwise not gravitate toward. or like things that youre not super familiar with to help people looking to avoid echo chambers", and "maybe you could even inject those kinds of results into your normal filter preferences and like adjust how often you get a random or counter preference or unfamiliar/novel result injected". Claude's notes for later (not decided): (a) a filter "Unfamiliar" that shows items from groups, places, topics and people you rarely or never engage with; (b) a setting (a slider, with Off) for how often one such item is mixed into whatever filter you use (Claude's lean: on at a low rate for new accounts, explained once under Hints, always adjustable); (c) "unfamiliar" is computed from YOUR OWN activity only (what you read, thumb, join), never a profile of you shared with anyone; (d) not purely random: a quality floor (the weighted thumbs and track-record weights from the credibility work, so a troll or junk does not get in just for being unfamiliar), and prefer well-regarded items from outside your circle; (e) every injected item carries a one-line reason ("from a group you have not visited", "from outside your usual topics") so it is never a mystery; (f) "counter preference" is the hard one: what counts as the opposite of your leanings needs thought (topics and groups you avoid, not guessed politics); open: what exactly "counter" means, whether the injection setting is per feed or global, and whether people can mark an injected item "not for me" to teach it.
- **2026-11-16 PT -- Coherence ideas: Brandon's answers (direction; mostly NOT built).** Of Claude's seven (the entry just below): (1) one facts record per pool: ok. (2) a stream's three channels (width = how many walk it, flow dots = how recent, tint = how people feel) with a legend line: "alright, but maybe that only shows when hints are on (default when you create an account)" (so there needs to be a Hints setting, on for new accounts, that can also gate other teaching text). (3) one "who can see this" picker everywhere (the post audience picker reused for thumb privacy, who sees your drop, profile pages): ok. (4) web pages are pools (a shared link and the extension's hashed web-page room are one place): ok. (5) search and the feed show the same signals: ok for search (a small tint dot and traffic mark beside results); "the feed can do that if you have the filter set to that": the feed gets filter/sort options, e.g. relevance, most recent first, most recent last, most popular, "etc", and only uses the thumbs and traffic signals when that filter is chosen. (6) one presence everywhere (profile pages, pools, web pages: the same drop, menu, mic and camera): yes. (7) the grammar (pools nouns, streams verbs): kept. Open: the full list of feed filters; what else Hints should switch.
- **2026-11-16 PT -- Coherence ideas, Claude's list (Brandon: "if you can think of any ways to tie things together to improve coherence, im all ears"; IDEAS, none decided, nothing built).** (1) One "facts" shape for every pool: content, author, time, thumbs balance, traffic, comments, built once so a post, group, proposal, comment, event all draw from the same record and the same renderer (this build's `body/by/ts` is the start of it). (2) A stream says three things in three channels, one legend line: width = how many walk it, flow dots = how recently, tint = how people feel about where it leads. (3) "Who can see this" is one picker everywhere: the post audience picker (friends / anybody / groups / hide-list) reused for thumb privacy, for who sees your drop, for profile pages, instead of a new one per feature; the corner menu is a shortcut into the same settings. (4) Web pages are pools: a link shared to the feed (Share to LiquidGov) and the extension's hashed web-page room (`/x/<hash>`) are the same place; a shared link becomes a pool, and people with drops on that page are in it. (5) Search, the map and the feed show the same signals: a small tint dot and a traffic mark beside search results, and feeds can sort by the same weighted thumbs. (6) One presence: dots on profile pages, in pools and on web pages are the same drop in the same kind of room, with the same menu and the same mic and camera. (7) The grammar stays: pools are nouns, streams are verbs; traffic and thumbs are new verbs on the same streams (people "walk" it, people "like" what it leads to).
- **2026-11-16 PT -- Thumbs, decided (Brandon; WANTED, nothing built).** No "and N private" line under the list; PUBLIC by default; a menu that expands from the corner of whatever post or thing you are interacting with, to choose: thumbs always private, or specific people or groups who cannot see your thumbs, or private on that one thing; EVERYONE sees the stream tint. With the earlier decisions: counts on the page include private thumbs; clicking a count lists the public ones in order with a time beside each; private thumbs keep their place; your own thumb history; thumbs on things, not on people's pools; weight from Judgment, capped, linked accounts counted nearer to one voice (credibility doc Round 4). A thumb counts for everyone but its name is hidden from the viewers you chose (or everyone when private).
- **2026-11-16 PT -- Thumbs also show the ORDER people thumbed (Brandon; WANTED, nothing built).** The list behind a count shows who reacted in the order they did ("i feel like that could be useful data somehow"). Claude's notes in the credibility doc (Round 4): early independent thumbs that later prove right are evidence of good Judgment, late ones that followed a visible crowd are weaker; the order is shown in the click-through list only (not the headline count) to avoid starting a bandwagon; private thumbs keep their place in the true order; DECIDED by Brandon: a time beside each name, and private thumbs keep their place in the true order ("i like the time addition. and also the private thumbs keeping their place"). Still open: the "and N private" line, public or private by default, who sees the tint.
- **2026-11-16 PT -- Thumbs: your own history, counts on the page, private thumbs, names behind a count (Brandon; WANTED, nothing built).** Brandon: you can see your own thumb history; the thumb counts show on the page; you can make your thumb private; clicking a count lists the people who reacted that way (not those who chose private). On who sees the stream tint he asked Claude; Claude's lean (Round 4 in the credibility doc): everyone who can see the pool, nobody for things they cannot see; the page shows the plain head count (private included; Brandon's clarification: 5 thumbs-down with 2 private shows 5 to everyone, and clicking it lists the 3 who did not choose private; whether to add an "and 2 private" line is open), the tint is the weighted balance, and the page says they can differ. Open: public or private by default.
- **2026-11-16 PT -- Thumbs and credibility are one discussion (Brandon, direction; nothing built).** Brandon: "i think thats a fair consideration. you know what? i think we should wire the weight of people's thumbs up or thumbs down into the credibility discussion" (after Claude's point that a thumbs-down on a person invites sabotage, and the suggestion of thumbs on things only, with each person's influence capped). Written up as Round 4 in `Persistence/TheCommons/CLAUDE_CREDIBILITY_QUESTION.md`: thumbs are feedback, not votes (credibility still never weights a civic vote); a thumb's weight comes from the thumber's Judgment (not popularity), capped, with linked accounts counted nearer to one voice; thumbs feed the author's Contribution through the same weights, and a thumbs-down reaches a jury only when enough independent people agree; no thumbs on a PERSON's pool, thumbs on what they did; the stream tint is balance and amount (little feedback stays blue). Open: whether people see their own thumb history, whether thumbs are public or only the tint, who sees the tint.
- **2026-11-16 PT -- Future list, items 2-4: traffic, and thumbs that tint streams (Brandon; WANTED, not scheduled, nothing built).** Answer to item 1's open question: a very long post makes its pool "big enough to fit it" (the pool grows to hold the whole text). Item 2, traffic: "streams that have a number people walking down them should be relatively wider than ones that don't", and "if theres a lot of recent traffic the dots indicating flow should indicate an elevated amount of flow" (more and faster flow dots on busy streams). Item 3, feedback: "every pool that doesnt represent a foundational page (one you would see in the dropdown menu), when you open the page to that pool, there should be a way to provide positive or negative feedback via thumb up or down". Item 4, tint: "streams to pools with positive feedback should be tinted greener, while negative feedback should be tinted redder, and little feedback would just be blue". Claude's notes for later (not decided): traffic needs the server to count walkers per stream over a recent window (the presence server knows each walker's box; store only anonymous counts, never who walked where, and show width only above a small count so one person's route is not readable from it); thumbs need one vote per person per thing, changeable, stored on the server, with the tint from the balance AND the amount (little feedback stays blue); the foundational-page list (the dropdown menu's pages) must be named so those pools are exempt; open questions for Brandon: do thumbs apply to PEOPLE'S pools or only to things (posts, groups, proposals, comments)? a downvote on a person is the kind of thing his credibility notes (`Persistence/TheCommons/CLAUDE_CREDIBILITY_QUESTION.md`: one person cannot sabotage another; disapproval judged, not tallied) warn about; and should a few angry accounts be able to turn a pool red (cap each person's influence, count independent voters more)?
- **2026-11-16 PT -- Future list, item 1: pools show what they are (Brandon; WANTED, not scheduled, nothing built).** Brandon, while the Commons archive was deploying, said he is "going to start listing things that id like to see in the future"; this is the first. Pools should carry their content: a pool for a GROUP shows the group's cover photo; a pool for a POST on a group's feed shows the post's contents and who posted it, with their profile picture, "but the profile picture shouldn't be too distracting"; when someone COMMENTS on a post, that pool shows their whole comment with their name and picture, "similarly being sure not to distract too much from the real substance of the pool"; and "anywhere else that can follow this kind of logic should, like proposals or events, or whatever". Claude's notes for later (not decided): groups have no cover photo today (new field, upload and storage); posts, comments (on posts and proposals) and proposals already exist as data; events do not exist yet; the map's node data (`/map/node`) carries only a label now, so each kind needs its content added to it, with the same privacy gates the map already applies (map_gnode, blocks); open questions: how long text fits a pool (grow, scroll, or "open to read the rest"), whether a pool shows its content only when you are in or near it, and how big the picture is (a small quiet avatar).
- **2026-11-15 PT -- The Commons archived in the repo (BUILT on `wip/106-commons-archive`; not merged, not deployed; the droplet daemon is still running).** Brandon: archive it, extract what is used, discard the rest. What was used: the repo's OWN tooling tests lived in `commons-daemon/tests/` (ship, ci-gate, naming, git-hook checks, make-go, ...): 44 files moved to `tests/repo/` (two levels deep like before, so their repo-root math is unchanged), with a slim `tests/repo/lib.sh` and `run_all.sh` (the daemon parts of the harness are gone) and a new hook-checks.yml step that runs the wrappers. Deleted: `commons-daemon/`, `commons/`, `.github/workflows/commons.yml`, the daemon-only tests, `local_ci.py`'s commons handling, `pwa_files_check.py`'s Commons half (33 checks left: the Mail/Social/Civic manifests). Site: the four `/internal/*` same-box routes (session-user, friends, blocked, user-exists) and `/canvas-frame/<user>/latest` removed. KEPT on purpose: `deploy/commons-closed.html` and its nginx conf (the static "The Commons is closed" notice answers old bookmarks; `/world` still redirects there; `city-live-check.py` still probes it) and the design notes in `Persistence/TheCommons/` (+ `ARCHIVED.md`). Safety copy: branch `archive/commons-final` on GitHub = master `a006386` (a tag push is refused here, so the tag is local only). CORRECTION to an earlier line: the public `/commons/` address was never the game, only that static closed notice; the daemon listens on 127.0.0.1:8765 only. Checks: hook checks 34/34, tests/repo wrapper tests 6/6 (test_nginx_www 37 with a real nginx), canvas e2e passing; the full local CI run is next. STILL TO DO by Brandon on the droplet, after the deploy: stop and disable `commons-daemon` (script to come: backs up /var/lib/commons/commons.sqlite first). Old `wip/70-commons-remove` (13 Nov, 104 commits behind, also moved tests to tests/repo) was never merged and is superseded.
- **2026-11-15 PT -- The Commons (commons-daemon + /commons/ client): found live but unused; Brandon: archive it, and "if some part of it is being used, maybe it should be extracted and then discard the rest" (NOT done yet; planned after the pools deploy).** Checked on the droplet (Brandon ran read-only commands): `commons-daemon` is active and enabled, `https://liquidgov.us/commons/` answers 200, the DB (`/var/lib/commons/commons.sqlite`) has exactly 1 player (Bbottin), the daemon logged nothing but its 14 Nov restart in 7 days, and the web logs show no visitor requests (older logs: 0 in 14 days; today only our own `deploy/city-live-check.py` and a curl). Nothing on the site links to it (home and Genesis cards are off, "closed until it is built"; only the old `/world` bookmark redirects to /commons/). Plan, on its own branch after the pools deploy: tag `commons-final`; delete `commons-daemon/`, `commons/`, `.github/workflows/commons.yml` and their tests; remove the site's hooks that only serve it (four `/internal/*` routes in system_routes.cpp, `/canvas-frame/<user>/latest`, the `/world` redirect); update `deploy/city-live-check.py` and `deploy/nginx-www-apex.sh`, which probe /commons/; keep `Persistence/TheCommons/` (design notes only); give Brandon a droplet script that backs up the DB, stops and disables the service, and removes its nginx rules. README line "local-only, not deployed" there is out of date.
- **2026-11-15 PT -- Local CI follows each workflow's old branch triggers; commons runs on master only.** Before Actions went manual, platform and hook-checks started on master and wip/**, commons on master only, so the deploy gate never needed commons. `scripts/local_ci.py` now does the same (`--only <name>` forces one). Found while at it: the commons daemon's `test_orphans` fails the same 3 cases here every run (SUSPECT guard cases, timing-based: delete accounts, sleep 5); this branch changed no daemon code, so it is not from this work; worth a look on a normal machine before the next commons change. Work now lives on `wip/103-pools-search` (the deploy flow's branch naming); `claude/upbeat-ramanujan-z42rkm` on GitHub holds the same commit.
- **2026-11-15 PT -- Your starting pool is yours, not "Main" (Brandon asked "should main exist?"; Claude recommended keep it, name it after you, make it your own place; Brandon: "yeah do that").** The pool you start in carries your name (it said "Main"), and it is no longer a lobby everyone shares: standing in it you are in the instance `user:<you>`, the same room a visitor is in when they swim into your pool on their map. Only the open water between pools is the shared "mainstream". Strangers meet in real places (a city, a group). Still parked: whether the pools round you stay Politics/Friends/Events/Feeds/Places/Groups or become People/Places/Things/Ideas.
- **2026-11-15 PT -- Map checks brought up to the one-world map; five old ones retired (Claude, under the one-world map Brandon OK'd).** map_live_check, map_live_check2, map_big_level_check, map_you_ring_all_kinds_check and map_view_only_check tested the old model (a fresh ring per level that fits the frame; the side panel), so they are out of CI (the last two deleted; the first three kept only as code other checks import). New `tests/browser/map_world_check.py` (22 cases, boxes at desktop and narrow, pools at desktop): you + kinds, opening adds without moving anything, each thing once, no overlaps, "+N more", Back, a click opens the page. The walker checks open a pool by staying in it (A/Enter is the portal now) and walk out through a pool's middle. Two real bugs found on the way, fixed: the peek's frame added steps to the browser history (the phone's back gesture went back inside the peek), and a pool could grow without limit over its neighbours (now at most twice its size, never into another pool; what does not fit goes behind "+N more").
- **2026-11-15 PT -- Skip paid GitHub Actions: CI runs locally (Brandon: "also can we skip github if it costs money now?").** The repo is private; Actions minutes ran out and runs were blocked by billing. Keeping the code on GitHub is free and stays. `scripts/local_ci.py` runs every step of platform.yml, hook-checks.yml and (when its files changed) commons.yml here, with GitHub's own rules (report-only browser steps never block); when everything that gates passes it writes `deploy/ci-pass/<liquidgov-platform tree>.txt` ("LIQUIDGOV_LOCAL_CI PASS", commit, time, every step, the phone VERDICTs). Committed and pushed, `deploy/ci-gate.sh` and `scripts/ship.py` accept it for exactly that tree (GitHub is still asked when there is none). The three workflows are manual-only now (`workflow_dispatch`), so pushes cost nothing; turn the triggers back on if Actions is ever paid for. Tests: ci_gate_check 19/19, ship_check 23/23. No more CI overrides needed.
- **2026-11-15 PT -- Parked: what is in Main, and whether Main should exist; the extension stays free, no store for now; Claude picks the peek (Brandon).** "idk whats in main... or if main should even be a thing..." (parked: the map keeps you + your kinds until he decides; the drawing's People/Places/Things/Ideas is one option). "idk about preview, you pick something": Claude's pick, the peek shows only once your drop has been still a moment and hides as soon as you move, smaller, at the top of the screen on the side away from your drop, so it never covers the pool you swim in; A still grows it into the page. "maybe later on that, lets keep it free for now": the extension stays load-unpacked, no Chrome Web Store account.
- **2026-11-15 PT -- Map search also finds words in posts, comments and messages, and dates and times (Brandon: "it should also search for matching word patterns in like posts or comments or messages", then "also date's and maybe even times"; BUILT on the branch, not live).** `map_search.cpp`: every word must appear, in any order. Results: names first (people, groups, places, proposal titles), then what people wrote, newest first: posts, comments on posts and proposals (leading to their post or proposal), and only your own messages (1:1 mail either way, your group chats; tapping one opens the conversation). Date and time words narrow by when it was written, in your own time zone (the page sends it): today, yesterday, monday..sunday, nov 12 / 12 november 2026 / november, 2026-11-12, 11/12(/26), a year, 3pm / 3:15pm / 15:00 / noon / midnight. Every result passes the map's own privacy (map_gnode) and blocks. `tests/e2e/map_search.sh` 17/17.
- **2026-11-15 PT -- A search bar at the top of the map (Brandon: "there should be a search bar at the top where you can search for stuff"; BUILT on the branch, not live).** One row above the trail: as you type (2+ letters) it lists people (the people search's own findability and block rule), groups, places, proposals and posts, only what your map may show (`GET /map/search`, every result through `map_gnode`); tapping one puts it next to where you are and swims your drop there. Without scripts the bar is the people search. Same day, pools: a pool grows until all its channels' mouths fit, channels leave at least a mouth apart and never run through another pool, and mouths and signs are drawn inside their own pool. `tests/e2e/map_search.sh` 6/6.
- **2026-11-15 PT -- The map's look from Brandon's drawing: dimensions first, distribution not decided (Brandon).** Brandon drew the map (phone, portrait): black land; one big oval pool about a screen wide, deep blue water with a bright glowing blue rim; four wide straight channels (each about a fifth of the pool's width) leaving it diagonally, two up and two down, toward the next pools just off the screen; each channel's name written inside the pool at its mouth, along the rim; the pool's name huge and bold in its middle ("Main") with a wavy water line under it; your drop (portrait, tail behind) swimming in it. His pools were labelled People, Places, Things, Ideas, but he said "idk how the distribution of boxes should be organized, but i was focusing more on the dimensions". So: build the dimensions and the look; what goes where (the four lakes or the current kinds) stays open. Then: "the color scheme can be whatever. just like bigger and not square" (so the site's own water colours stay).
- **2026-11-15 PT -- Drops on other websites: a first browser extension (BUILT on the branch, not live; not in any store).** Under Brandon's "take care of the other stuff for now". `extension/` (Chrome/Edge/Brave, Manifest V3, load unpacked): off on every site until you tick "Drops on this site" (the browser asks for that one site); draws its own layer and never reads the page; the room is a SHA-256 of the page address without query or `#` (YouTube keeps `v`), only the hash is sent; never on banking, mail, health, sign-in, checkout/account pages, .gov, local addresses or liquidgov.us; blocks apply; drag or arrow keys move your drop; "Share this page to LiquidGov" opens the feed's post box via `/share`. Server: `/here/ws` accepts `page=/x/<64 hex>` (anyone who chose Yes; blocks filter), snapshots now say `"you"`, and `GET /here/ticket` gives a one-time 30 s socket ticket because Chrome sends no cookie on an extension's WebSocket (found by loading it in real Chromium). Checks: `tests/e2e/here_web_room.sh` 12/12, `extension/tests/run_check.sh` 6/6. Not yet: mic/camera, ripples, Firefox, forward buttons on Facebook/TikTok/X posts. Open for Brandon: whether to publish in the Chrome store (needs a developer account, $5).
- **2026-11-15 PT -- Credibility, first slice: your own Contribution card in Settings, only you see it (BUILT on the branch, not live).** Brandon: "lets build out ... credibility"; his display idea was "take that with a grain of salt", so nothing is shown to anyone else yet. src/site/features/credibility: computed from what the site keeps, nothing stored: distinct people who replied to your posts or proposals or voted on or reacted to your proposals; each person once; your own replies never; blocked (either way) never; non-friends count double. Levels New (no history, or first week under 3 people), Emerging (<10), Established (<30), Trusted (30+), by friends + 2 x others; per-group counts from replies to posts in each group. Numbers are Claude's first guess, to tune with Brandon. Not yet: juries, reports, Judgment, endorsements, showing it beside comments. Test: tests/e2e/credibility_card.sh (port 18135), 8/8.
- **2026-11-15 PT -- Share to LiquidGov from a phone's Share menu (BUILT on the branch, not live).** Brandon asked whether posts from Facebook, TikTok or X could be forwarded to a LiquidGov feed; Claude offered the Android share entry as the quick part (the extension's "Share to LiquidGov" button later). manifest.webmanifest share_target -> GET /share (title, text, url as one text, a link already in the text not repeated) -> /feed#share=... fills the post box. Android/Chrome with the app installed; iPhone does not let web apps join the Share menu. A signed-out visitor goes to log in and loses what was shared (login_url keeps only the path). Test: tests/e2e/share_target.sh (port 18134), 5/5. While Brandon draws the map he imagines ("take care of the other stuff for now, ill work on drawing what im thinking"), map work is paused.
- **2026-11-15 PT -- "maybe there could be like 3 or 4 layers of pools and streams" (Brandon, while Claude started rebuilding the map as one continuous world of pools; asked: "lets just get it all built now").** Brandon then asked "which style would best fascilitate coherent networking and nesting and recursive connections visually?"; Claude's answer, which he okayed ("okay"): rings outward from wherever you are (your pool layer 0, then 3-4 fading layers, mist beyond; they re-form around you as you swim: recursion), nesting by zooming (a pool holds its contents as smaller pools inside), and each thing exists once on the map with every stream into it (entangled; loops visible). Building now: one continuous world (nothing re-lays out or is cleared) with the layers; nesting after.
- **2026-11-15 PT -- Pools and gates, first version: go (Brandon: "yeah that sounds like a fair approach, lets do it").** Agreed scope: pools you swim in (only water: pools and open channels); gates at connections, relevance v1 = you belong to it (so today's connections all open); swimming into a pool opens its connections (replaces A making a box the center); the spring = the portal (peek, A enters the page); zooming out still shows everything. Next version: friends there and lively now (server work), fog beyond one gate, interests and worn paths.
- **2026-11-15 PT -- The map as pools and floodgates (Brandon: islands and drawbridges, then "thats an analogy, but since were liquid themed, kind of flip that"; not built).** Claude's flip: boxes are pools (your drop lives in water; dry land between pools would evaporate it, so you cannot cross it); connections are channels with floodgates, closed by default, open when they are relevant (Brandon: "floodgates open when theyre relevant"; Claude's proposed signals: you head toward it, someone you know is beyond it or coming through, it leads somewhere lively now, you often use it, it matches your interests or the people you are with; open by how relevant, a trickle to wide open) and close again when not; then "well not exactly... they become visible when its at least peripherally relevant": a gate is not shown at all until it is at least slightly relevant to you (Claude's reading: below that, invisible; slightly relevant, a faint closed gate; more relevant, it opens, from a trickle to wide open; companions share what is relevant, so nobody sees a friend vanish through a gate they cannot see); a gate that will not open for you shows a lock; each pool's spring is the portal (the peek wells up over it, A floods the screen into the page). The islands version first:  Brandon: "maybe an analogy would be like each box can be treated like an island and you cant swim. and the islands are connected by draw bridges that go up or down depending on the traffic intending to use them, with the bridge being drawn up as a default. i think that makes sense?" (asked while Claude asked how A should open a box's connections once A enters the page through the portal). Claude's reading, to confirm: you walk on islands and lowered bridges only (water stops you); each connection is a bridge, raised by default, lowered when someone walks toward it and up again once it is clear; a bridge that will not lower for you (a closed valve) shows a lock; each island has a portal door where the peek shows and A enters the page; crossing a bridge is how you open connections (no more recentering by A).
- **2026-11-15 PT -- A guiding principle (one of several, not THE rule) for the map and connections (Brandon).** "the point here is to build the map and connections as far and densely as possible without becoming incoherent"; then "not THE guiding principal but" (his message ended there). Weigh it with other goals; it leans open questions toward more connection (seeing more people, more streams, the wider web), and design effort toward keeping it coherent (nesting, locks instead of dead ends, privacy that hides details rather than whole paths). Claude's application to the extension: see everyone on a page by default (block and per-site off still work), and let a page you choose to share become a pool on the map.
- **2026-11-15 PT -- Next: portal peek, water map, credibility; and droplets on other websites (Brandon).** Brandon: "lets build out portal peek, watermap and credibility. also i want to be able to hang out as droplets on other websites. so like visit other websites from a droplet filter of sorts on liquidgov.us". Claude's order: portal peek first (decided 5A), then the water map, then credibility (its open questions taken at Claude's recommendations unless Brandon objects). Other websites: a feasibility question in the chat (most big sites refuse to be framed; a browser extension is the realistic route). Brandon: "i kinda like the browser extension idea... say more" (Claude's outline in the chat: what it does, privacy by default, how it connects, what it takes to ship; not decided).
- **2026-11-15 02:11 PT -- Messages fixes, video links, fullscreen map, camera and mic stay on, ripples (bundle1) LIVE (commit `fa34e0f`, PID 868219, public checks FAILS 0).** Build and restart (care pair). Site code went to master as `9bd47be` (wip/101-bundle: platform run green; locally lgcheck EXIT=0 and all 37 browser checks), the pair as `fa34e0f` (wip/102-deploy-bundle1). DEPLOYED WITH LIQUIDGOV_CI_OVERRIDE, on Brandon's explicit OK ("lets deploy"): master's own CI run 43701232781 never started, GitHub: "The job was not started because recent account payments have failed or your spending limit needs to be increased" (a rerun hit the same); the same commit fa34e0f had passed platform run 43700864681 minutes earlier on the deploy branch. Run by hand from PowerShell through WSL with Claude's key (ship.py refuses overrides); WSL timed out once (WSAETIMEDOUT) and started after `wsl --shutdown`. OPEN: GitHub Actions billing must be fixed or every later deploy hits the same gate. Not verified by Claude: the live site on a real phone.
- **2026-11-15 PT -- Credibility: earned agency, fair consequences, no sabotage (Brandon asked; nothing built).** Brandon: "I want to figure out a good, fair way to allow people to earn more credibility to access more agency or face bad credibility if you do things that people disapprove of. but i want to make sure that one person cant sabotage anothers credibility". Claude's proposal and five questions: `Persistence/TheCommons/CLAUDE_CREDIBILITY_QUESTION.md` (capped influence per person, independent agreement counts most, disapproval judged by small random juries not tallied, false reports cost the reporter, fading marks, appeals, a floor nobody loses; recommendation: never touch vote weight). Then: "different aspects of credibility ... if you get a reputation for being hyper judgemental, then maybe your judgements regarding other's credibility would have less weight accordingly": proposed aspects Contribution, Conduct, Judgment (measured from how your calls match independent outcomes, both too harsh and too soft lose weight; never voted on, so a rival cannot lower it), optional topic standing (doc's Round 2). Leaning, NOT decided (Brandon: "it shouldnt effect the weight of your votes but perhaps it could be shown in your comments or proposals to give people an idea of how seriously they should consider what you contribute", then "take that with a grain of salt"; doc's Round 3 sketches how it could show).
- **2026-11-15 PT -- Camera and microphone stay on until you turn them off (Brandon decided; not built).** Brandon: "if you turn on camera and/or microphone, they should stay on everywhere until you make a clear decision to turn them off". Today they live on one profile page and stop at the next page load. Then: "you should be allowed to see your own camera and hear your own microphone. if theres a way to simply or easily require headphones that would be cool, otherwise fuck it i guess". Claude: a page cannot reliably tell whether headphones are plugged in, so hearing yourself is its own switch, off by default, labelled for headphones (on speakers it howls); your own camera shows as your bubble.
- **2026-11-15 PT -- MAINstream map fullscreen (Brandon decided; building).** Brandon: "the mainstream map should be like fullscreen besides the liquidgov title and notifications and mail and profile pic and juketube".
- **2026-11-15 PT -- Sound ripples off your drop (Brandon decided; building on `claude/media-stay`).** Brandon: "there should be audiowaves that ripple off of your droplet when you make noises according to the intensity of the noises you make. so i guess a way to say that would be roughly anatomically correct audio waves?" Claude's reading: rings that spread from the drop like sound does, louder = stronger, wider and more often, each fading as it spreads; also on the people you are in a call with. Then: "the ripples shouldnt spread too far though" (kept within about 2.5 times the drop's size) and "the juketube should make ripples too when its playing and not muted" (BUILT: rings pulse off the Juketube button while a track plays with the sound on, stronger at a higher volume; YouTube's player does not let a page read its sound, so they follow the volume slider, not the music). Found while testing: a call offer that got no answer waited for ever (here_media M1 hung 2 times in 6 on a busy machine, the live version can do it too); the caller now offers again after 4 s (4/4 under load).
- **2026-11-15 PT -- Camera and microphone stay on; your own camera on your drop; Hear myself (BUILT on `claude/media-stay`, not live).** Brandon: "if you turn on camera and/or microphone, they should stay on everywhere until you make a clear decision to turn them off"; "you should be allowed to see your own camera and hear your own microphone. if theres a way to simply or easily require headphones that would be cool, otherwise fuck it i guess". walker.js owns the camera and microphone now (LG_WALKER.media): Microphone, Camera and "Hear myself (headphones)" in the drop menu on every page, kept in localStorage (lg-media-on) and turned back on by each page as it loads; a refusal from a switch you just flipped turns it back off. Your camera is a mirrored bubble on your own drop (taps go through). Hear myself is its own switch, off by default (a page cannot tell headphones from speakers). here_media.js calls use that stream. Each page change is still a full reload, so there is a short dropout until pages load in place (the portal work). iPhone Safari may ask for camera permission on each page. The map has no calls yet. Found and fixed while testing: restarting calls from the last snapshot right after a change lost the first offer; they now wait for the next snapshot. Local: walker_controls 82/82 (MN5-MN8 new, a stand-in camera), here_media 6/6, bubble 8/8, dots 15/15.
- **2026-11-15 00:33 PT -- Your droplet on every page, hidden from Settings (dropall1) LIVE (commit `ea53d57`, PID 865164, public checks FAILS 0).** Brandon ran it from PowerShell (pasted script). Build and restart (care pair). Site code went to master as `e39d1ff` (wip/99-drop-everywhere; the first try, wip/98, failed CI on profile_here_choice, fixed in e39d1ff), the pair as `ea53d57` (wip/100-deploy-dropall1). NOT in it (built after the script took its code): the Messages fixes (the pad covers the message box on a phone until they ship), video links, the adult-video setting, the composer cleanup. Not verified by Claude: the live site on a real phone.
- **2026-11-15 PT -- Video links play everywhere (Brandon decided; building).** Brandon: "id like to make it so anywhere you can share a url or video would be able to see the video player instead of just seeing letters and numbers and symbols. whatever is realistic though obviously." Claude's realistic version (his "whatever is realistic"): a fixed list of sites with official embed players plus direct video files; tap to play (no third-party request before you tap); adult sites behind a "tap to show" cover; in every place people share text (messages, posts, comments, groups). Then: "you should be able to turn of NSFW limitations though" ... "in settings perhaps": Settings > Adult videos turns the cover off in that browser (lg_nsfw=on cookie). BUILT on the branch (not live): assets/video_links.js on every page wrap() builds; sites: YouTube (incl. Shorts), Facebook (videos, reels, fb.watch), X, Instagram, TikTok, Vimeo, Twitch clips, Streamable, Dailymotion, Pornhub and RedGIFs (adult), and .mp4/.webm/.mov/.m4v/.ogv files. Plain-text addresses in messages now become links too.
- **2026-11-15 PT -- Messages: opens at the newest message, composer above the pad; no Group button, no "Inner circle" heading, no storage note (Brandon asked; BUILT on the branch, not live).** Cause of "takes me to the top": the page scrolled its composer into view, but the composer is sticky at the bottom, so the browser counted it as in view and never scrolled; now it scrolls so the newest message sits just above the composer (again after pictures load). Found while checking it: with the droplet on every page, the pad covered the sticky composer on a phone; walker.js now sets --lg-pad-room and the composer sits above the pad. Your solo drop starts where you are looking (Messages opens scrolled). Brandon: "the group button at the top of messenger is annoying... that needs to go away. if i want to make a group of im's i think theres a lot cleaner way" (removed; existing group chats stay listed; a cleaner way to start one is open), "remove where it says inner circle" (one Friends list, inner circle first) and the "Messages on this copy are stored..." note (removed from Messages; /privacy still says it). Video links that play (YouTube, Facebook reels, X, Pornhub): questions in the chat, not built.
- **2026-11-15 PT -- Messenger: sending jumps to the top; video links should play (Brandon asked; not built).** Brandon: "i want to fix the behavior of the messenger. every time i send a message is takes me to the top of the page. also, i want be able to see a video screen pop up if i share a url that points to a video regardless of where its pointing. so like if i share a facebook reel or an x video, or youtube or even pornhub if possible".
- **2026-11-15 PT -- Your droplet on every page, hidden from Settings: BUILT on `claude/upbeat-ramanujan-z42rkm` (not live).** wrap() adds your drop (here.js solo: nothing sent) to every page without a walker of its own; profile pages, the map and the logged-out home page keep theirs. Settings > Your droplet: Hide/Show, kept per browser in the lg_drop=off cookie like the theme (Claude's choice: an account-wide setting would change the stored prefs record; offered as a later step). Hidden = no walker on any page (no profile dots, the map is tap-and-drag). Exception, Claude's call, told to Brandon: no drop on the drawing tool (the pad covered the canvas on a phone and took the strokes). C++ changed: the deploy builds and restarts. Local: the whole CI list 36/37, the one failure (the drawing tool) fixed and its three checks then passed.
- **2026-11-15 PT -- Your droplet on every page (Brandon decided; not built).** Brandon: "i think that every page should just include the droplet that way we keep things universal and standardized and consolidated". Then: "and you can choose to hide it if you want" ... "in settings". Today the walker is on profile pages (here.js, with presence), the home page (home_drop.js, solo) and the map (lgmap_walk.js).
- **2026-11-14 23:34 PT -- Drop menu (microphone, camera, drip speed), always centered, no Hidden mode (dropmenu1) LIVE (commit `504a193`, PID 846784, public checks FAILS 0).** Brandon ran it from PowerShell (pasted scripts; cloud Claude read the output). No restart (jbshell pair): 8 assets (here.js, here_media.js, home_drop.js, lgmap.css, lgmap.js, lgmap_walk.js, site.css, walker.js) and 7 browser checks. Site code went to master as `fa0e05d` (wip/96-drop-menu), the pair as `504a193` (wip/97-deploy-dropmenu1). The first dry run failed on a WSL timeout (`Wsl/Service/WSAETIMEDOUT`, nothing reached the droplet); WSL started on the next try and the dry run passed. Known side effect: your drop sits in the middle of the screen and opens its menu, so the spot right under it cannot be tapped (A still opens the box you stand in). Not verified by Claude: the live site on a real phone.
- **2026-11-14 PT (night) -- Pages people make: 3 per person, connectable (Brandon decided; not built).** Brandon: "maybe limit the blank pages to 3 per person, but you can connect the pages if you need something bigger." Claude's reading: connected pages are your pools joined by streams (rooms of one place). Still open: separate from the profile or the profile itself; one owner or several editors; no scripts and no raw HTML.
- **2026-11-14 PT (night) -- Pages people make from scratch (Brandon's idea; not built).** Brandon: "i also want to have a way for people to create their own pages from scratch. so like somehow you get access to a blank page that you can write css on and it becomes whatever you make it. you set the rules and scope and we go from there". Claude's proposed rules and scope (built on the existing profile CSS sanitizer, scope and style library; no scripts, no raw HTML, a page is a pool on the map) are in the chat; awaiting his reply.
- **2026-11-14 PT (night, later) -- Waterways decisions (Brandon).** (a) Companionship forms automatically with anyone you walk alongside (option C); "you can just block anyone who you like, so youre basically invisible to eachother" (blocking already hides people from each other on the map and in presence). (b) The kind chips go with Hidden mode: "yes" (built on the branch, see the commit; not live). (c) What a locked path reveals: he asked "what makes the most sense and fits with the rest of the site?"; Claude's answer: a lock never shows more than the site already shows that viewer (the site's existing visibility rule: things you may not see are absent), so a public group you are not in shows its name on the lock, a private group or friends-only profile shows an unnamed lock. (d) Worn-in streams, automatic or deliberate: "idk, lets discuss it" (Claude's proposal in chat).
- **2026-11-14 PT (night) -- Waterways: companionship and locked paths (Brandon's idea; not built).** Brandon: "maybe if youre walking within a certain distance of someone for a certain amount of steps or somewhat consistently, then youre shown to have some kind of relationship with eachother and you guys begin to see eachothers paths and connections are made additionally between the connections you would already have alone. if you have exclusive access to a path then maybe that path still shows for the person who isnt allowed down it, but its shown to have a lock or something impeeding you from going down it". Claude's read: walking together over time forms a relationship that merges what the two of you see; a path only one of you may take shows to the other with a lock instead of being hidden. Claude raised consent (a stranger trailing you would learn your connections) and what the locked path reveals; awaiting his reply. The earlier questions (do the kind chips go with Hidden mode; worn-in streams automatic or also deliberate) are still open.
- **2026-11-14 PT (evening, later still) -- Map Hidden mode to go (Brandon decided; not built).** Brandon: "i want to get rid of the hidden objects on the mainstream map, that was a bad idea and its causing clutter... i think theres a better approach, lets flesh it out". This reverses the 2026-11-11 Hidden mode (the Hidden/Done button, faded boxes, tap-to-hide; hiding is stored per device in localStorage, never on the server). Claude's proposed replacement (the map tidies itself: nesting, worn-in streams, distance fading; no hand-hiding) and two questions are in the chat; awaiting his reply.
- **2026-11-14 PT (evening, later) -- Waterways: "traveling together" decided; size and privacy under discussion (nothing built).** Brandon: "traveling together just means walking alongside someone, i dont want to make it overly formal" (no switch: being close to someone is enough). Size: "i dont know, lets figure it out". Privacy per thing or per relation: "i dont know... lets discuss it". Claude's proposals for both in CLAUDE_WATERWAYS_QUESTION.md "Round 3".
- **2026-11-14 PT (evening) -- Waterways map: Brandon's answers to CLAUDE_WATERWAYS_QUESTION.md (nothing built).** (1) Water: "yeah basically, and if you can make currents fit in coherently then sure". (2) Size: "maybe some things could be bigger than others, we'd have to figure out what best determines that". (3) Top level: not decided; his ideas: "pools being nouns and streams being verbs", and "digital quantum entangled russian dolls". (4) The requirement: "variable amounts of privacy, seamless connectivity, and potential of discovery, while maintaining coherence ... there isnt a situation where youre walking with friends and you see an interesting path so you walk down it, but that connection wasnt made available to your friend so they just see you walk off the trail to nowhere and you get split up". (5) Portal peek: Claude's recommendation (5A: the real page, live, in the peek; A grows it into the page; the call carries over; the side panel goes). Claude's synthesis proposed in the question doc's "Round 2" section; awaiting his reply.
- **2026-11-14 ~18:00 PT -- Next direction for walking, the map and pages (Brandon decided; nothing built yet).** Brandon, after freewalk1 went live: (1) "keep the screen tracking logic the way it is when center is selected, but like thats it, no other options": always centered, the Center button goes. (2) "the microphone/camera buttons shouldnt be on the screen. if you click on or tap your droplet it should expand a menu with things like microphone, camera (toggle on/off), and a drip speed adjustment slider." (3) The MAINstream map: "you should be bound to the lakes and streams and pools like youre in a closed loop system"; boxes "significantly larger, as if theyre like physical places you can be and hang out at with friends"; top level "people, places, things, and ideas", then "some inteligent and dynamic way for things to populate as you walk around going down roads of relatively connected concepts or objects". (4) Mic and camera "consistent and seamless when transitioning from the mainstream map to a page". (5) The side-panel page preview on the map is abandoned. Instead: walk up to a page's portal; within entry range a small semi-translucent window grows from the portal's center (a peek); A there expands it to fill the screen while it turns opaque, until it IS the page you are on. Claude's slicing: A = (1)+(2); B = (4)+(5), one mechanism (the page loads in place without a full reload, so the call survives); C = (3), which needs a spec first.
- **2026-11-14 17:32 PT -- Always-on walking, Center button, free walking on the map (freewalk1) LIVE (commit `7426e4e`, PID 846784, public checks FAILS 0).** Brandon ran it from PowerShell (pasted scripts; cloud Claude read the output). No restart (jbshell pair, same PID as spatial1 and bubble1): 7 assets (here.js, home_drop.js, lgmap.css, lgmap.js, lgmap_walk.js, site.css, walker.js) and 13 browser checks. Site code went to master as `db7e1a9` (wip/94-free-walk, platform and hook-checks green on GitHub), the pair as `7426e4e` (wip/95-deploy-freewalk1). Also live with it: the presence socket now closes when you leave the map or a profile page (no more lingering drops). Timing traps met again, none a fault in the change: master's ship.py (without the recheck fix, which arrived with this merge) checked wip/94 before GitHub listed its run, and the pair's merge before its hook-checks finished; both passed on a second look. Not verified by Claude: the live site on a real phone.
- **2026-11-15 PT (later, a second cloud session) -- Free walking: checks brought up to it, a leaving-the-map fault fixed; STILL NOT MERGED OR DEPLOYED (`claude/upbeat-ramanujan-z42rkm`, on top of the HANDOFF commit below).** Picked up from the HANDOFF entry. (1) The pinch (walker_controls Z2) was NOT a page fault: the check's second finger landed on the new Center button (on a phone the pad covers the map from about y 632 down, and the check touched at 643), which also armed centering and slid the map under the one-finger pan. The fingers now go on the map above the pad; the pan point is taken clear of box outlines. (2) CE1/SC1: the check walks until the page has scrolled 300 px. walker_controls_check: 78 of 78; its case list is rewritten; the centered-phone screenshot is no longer blank. (3) The eight hop-based checks are updated and pass on a local Linux build with headless Chromium: walker_instance (hop() walks until inside the box or it cannot get closer; new I4 "between boxes is mainstream"), walker_proximity (F3 = zoom out with the wheel), walker_valve and walker_valve_types (a refused box leaves data-at ''; Y4 = a conduit is walked across, never in it), walker_group_instance (unchanged, passes), walker_instance_css, walker_presence (W5) and walker_presence_dots (D4): their Scroll case became "leaves the map". (4) REAL FAULT those two found, fixed: a browser keeps the page you left in memory with its socket open, so others kept seeing your drop on the map for 30 s and more after you had gone (measured; the server was right, the socket never closed). `lgmap_walk.js` and `here.js` now close the presence socket on `pagehide` and reopen on `pageshow` of a kept page; the drop is gone within a second. Profile pages got the same fix but no check covers them there yet. This fault predates the free-walking work (any map visit since spatial1; profile dots since they shipped). (5) The zoomed-in first view broke two more frozen checks; Brandon: "yes update the phone check, keep it zoomed in". phone_check: case 2 = the zoomed-in first view, new 2b = zoomed out by you the ring fits the frame, cases 3/4c/4d zoom out first (18 of 18). map_hidden_check: zooms out on /map and scrolls a box out from under the pad or the sticky bars before tapping it (17 of 17). (6) `walker.js`: the unused `mapRepeatMs` and its hop comment removed. The whole platform.yml list was run here once: 35 of 37 ALL-PASS before (5), the two being phone and hidden. Hook checks here: 32 pass; naming_check fails only on the `claude/` branch name and rollback_guard_check only because this clone is shallow. FOR BRANDON TO KNOW: on a phone the pad is always over the bottom of the screen and the map's frame runs under it, so a box down there cannot be tapped until you scroll the page, drag the map, or walk to it and press A. OPEN: `map_page.cpp` line 37's stale switch comment (left: assets-only deploy); not looked at on a real phone; arrow keys walk instead of scroll on every page with a walker (Claude's choice, unseen); then a CI run on a wip branch, and only then merge and a no-restart deploy. Do not run the old `wip/94-ship-recheck` script: it takes this branch's tip.
- **2026-11-15 PT -- HANDOFF, work in progress, DO NOT MERGE OR DEPLOY: always-on walking, Center button, free walking on the map (`claude/upbeat-ramanujan-z42rkm`, commit titled "WIP: always-on walking...").** Brandon asked to stop at a safe point. Master and the live site are untouched by this. DONE in the page code: `walker.js` (no Drop/Scroll switch, strip or first-time note; walking always on; a Center button above the pad, disarmed at first, remembered in localStorage `lg-walker-center`; with a mouse the touchpad and A/B are hidden and you walk with the keys: Claude's choice, not asked); `here.js` and `home_drop.js` (always walking; armed = `LG_WALKER.centerOn`, disarmed = `follow` as before); `lgmap.js` (wheel and pinch zoom always; new `LG_MAP.view/setView/fitScale/frame/toScreen/extent`); `lgmap_walk.js` rewritten: the drop walks freely in map units, a box is an area (inside it = its instance), valves block at the edge, A opens the box you are in or next to, the view starts at mapZoom around the drop, zoomed in only the box you are at and its connected boxes show, your own zoom-out shows everything and is left alone, a zoom-out the map does itself is undone after 0.4 s, Center armed keeps the drop in the middle of the visible part of the map. No C++ changed (assets only). Seen working in a real browser on the real test server (walk, armed centering, near filter). CHECKS updated with Brandon's OK (frozen files): here_dots_check, here_pad_room_check, map_sticky_check (all pass); walker_controls_check rewritten in place: 73 of 77 pass. OPEN, in order: (1) walker_controls_check Z2: on the phone a two-finger pinch does not zoom the map and one finger does not pan it (scale and box unchanged). NOT diagnosed; may be a real fault in the new code (lgmap_walk.js adds capture-phase pointer listeners on #lgm-svg; the pad may also sit over the touch point). Brandon asked for pinch-to-zoom-out, so this must work before merging. (2) walker_controls_check CE1/SC1 fail because the check's walk is too short to make the page scroll (the check, not the page: walk further before measuring). (3) its docstring still lists the old switch cases (P5, P6, T1-T3, C1, C2, D1, old M1/M3/M4, H0/H1): rewrite the list. (4) NOT yet updated, will fail as they are: walker_instance_check (its `hop()` must walk until inside the box; `drop_off()` and the Scroll cases I0/I4 go), walker_proximity_check (F0/F3 Scroll cases -> zoom out), walker_valve_check and walker_valve_types_check (a refused box now leaves you at its edge with data-at '' instead of 'you'; a conduit is walked across, not hopped over: Y4 needs a new expectation), walker_group_instance_check, walker_instance_css_check, walker_presence_check (W5 Scroll case), walker_presence_dots_check (D4 Scroll case). They inherit hop() from walker_instance_check, so fix that first. (5) src/site/features/map/map_page.cpp line 37 has a stale comment about the switch (left alone to keep this an assets-only deploy). (6) not looked at: the home page and a profile page by eye, a real phone, the arrow keys now walking instead of scrolling on every page with a walker. Then: full hook checks, a CI run on a wip branch, and only then merge and a no-restart deploy.
- **2026-11-15 PT -- Drop is always on; the Drop/Scroll switch goes; a centering button; free walking on the map (Brandon decided; build in progress on `claude/upbeat-ramanujan-z42rkm`).** Brandon: "we gotta get rid of the drop/scroll switch... just leave it on drop mode and if you wanna scroll you can just do that instead of using the joystick"; "an active droplet centering button that you can arm or disarm": armed, moving the joystick slides the screen so your droplet sits in the center; disarmed, "just like it is right now"; "fix the mainstream map. it should behave just like any where else as far as the droplet movement goes" (free walking, not box-to-box hops). His answers to three questions: (1) the map while walking: "zoomed in, far boxes hidden unless i pinch the screen outside of the joystick box if im using a phone or tablet, or use the mouse wheel if im using a mouse, or pinch zoom out or two finger scroll down on a laptop touchpad"; (2) the centering button starts disarmed and is remembered on the device; (3) the committed checks that test the switch or the hopping may be updated or replaced (his OK for those frozen files).
- **2026-11-14 PT -- Browser checks run in parallel on master (`7434d69`, wip/93-parallel-browser, SHIP_MERGED by Brandon's script); ship.py's two timing faults fixed (branch `claude/upbeat-ramanujan-z42rkm`; not merged).** First run of the parallel workflow, platform run 43632591284: 5 min 33 s (19:31:54 to 19:37:27 UTC) against 13 min 7 s before; 37 VERDICT lines, all ALL-PASS (the same set, plus herebubble-ci); the joined `phone-screenshots` artifact was found. ship.py faults met today, now fixed with Brandon's "whatever is the most logical": (1) a second check of the same branch (ship.py merge runs one) printed "screenshots: NONE ... The file exists" because gh never overwrites, and would refuse a site change; it now downloads beside the folder and moves the files in. (2) a check started right after a push saw no run yet and, for a change with no site files, said SHIP_READY with no verdicts; it now looks again up to 4 times, 30 s apart, before believing there is no run (so a genuine no-run check takes about two minutes longer). Check `commons-daemon/tests/ship_recheck_check.py`: 6 FAIL of 11 before, 11/11 after; ship_check, ship_fixes_check, ship_workflows_check, ship_deploy_check still pass. Still open, not changed: `go` right after a merge can meet CI_GATE_REFUSED while master's own run for that commit is pending; waiting a minute and running go again is the way through.
- **2026-11-14 11:31 PT -- Camera picture is a round bubble on the dot (bubble1) LIVE (commit `59ad9ac`, PID 846784, public checks FAILS 0).** Brandon ran it from PowerShell (one pasted script; cloud Claude read the output). No restart (jbshell pair; same PID as spatial1): assets/here_media.js, assets/site.css and the two bubble check files. Site code went to master as `ab5e582` (wip/91-camera-bubble: the bubble, its CI step, and the Windows hook fix), platform run 43629851477 green with the bubble check passing on GitHub's browser. The Windows hook fix is confirmed on the PC: `ship.py deploy` committed and pushed the pair itself and `ship.py merge` pushed to master (SHIP_MERGED wip/92-deploy-bubble1 59ad9ac), no --no-verify. Two timing traps met, neither a fault in the change: `ship.py check` a second after a push sees no CI run yet (for a no-site-change branch it then says SHIP_READY with no verdicts), and `go` right after the merge was refused by the CI gate while master's run for 59ad9ac was pending behind the previous master run (CI_GATE_REFUSED twice, then through). Not verified by Claude: the bubble on the live site with a real camera.
- **2026-11-14 PT -- Browser checks in parallel jobs (a workflow patch for Brandon: `deploy/ci/platform-parallel-browser.patch`; not applied, never run on GitHub).** Brandon: "why isnt there a cache so we dont have to wait this long every time?" then "make the browser checks run in parallel if that would help". Measured on platform run 43607546171 (13 min 7 s): build 57 s with the compile cache, lgcheck 3 min 57 s, the 32 browser checks 7 min 53 s one after another. Test results are not cached (a cache cannot know a change leaves a test alone). The patch splits platform.yml into check (build, lgcheck, parse and static checks) beside three browser jobs: browser-site-a and browser-site-b (the checks that start the real server; each builds only liquidgov_http and liquidgov_e2e_seed from the compile cache, read only) and browser-pages (the checks with their own page server, no build); a last job joins the pictures into the one `phone-screenshots` artifact ship.py downloads. Expected about 6 minutes (the check job is then the longest). Every browser step is unchanged, still report-only and bounded; ship.py still reads every VERDICT from the run's log. Changed risk: a browser job's own setup (checkout, tools, build) can now fail the run. Applies after the camera bubble CI step is on master. Checked here: actionlint clean; same 36 browser commands before and after; workflow_browser_steps_check, workflow_paths_check, ci_runs_checks_check, ship_workflows_check pass.
- **2026-11-14 PT -- Windows hooks leave Linux-only checks to GitHub (branch `claude/upbeat-ramanujan-z42rkm`; not merged).** Brandon: "fix the windows hook so i dont need no-verify". Cause (seen in his spatial1 output): `prebuilt_check.py` starts fetch-prebuilt.sh with a plain `bash`; from Windows Python that is System32's WSL bash.exe (Windows looks there before PATH), exit 127, so the PC's hooks refused every deploy-pair commit and every push to master. prebuilt_check.py is frozen and was not edited. New list `run_hook_checks.LINUX_ONLY = ("prebuilt_check.py",)`, the mirror of WINDOWS_ONLY: on Windows `.githooks/pre-push` (pre-commit execs it) and `run_hook_checks.py` do not run those checks and name them on a `SKIPPED (Linux-only)` line, never as passed; elsewhere nothing changes. GitHub's hook-checks run (Linux) still runs prebuilt_check on every push and ship.py waits for it. The selector's lists are unchanged (the hook asks `affected_checks.py <root> --linux-only`), so hook_windows_skip_check.py still holds. Check `commons-daemon/tests/hook_linux_only_skip_check.py` (real hooks, real commit and push to master, platform forced with LG_CHECKS_PLATFORM): 10 FAIL of 16 before, 16/16 after. Not run on a real Windows PC by Claude. Until this is on the PC's master, the --no-verify bypass is still needed (one last time, to land it).
- **2026-11-14 PT -- Camera picture is a round bubble on the person's dot (branch `claude/upbeat-ramanujan-z42rkm`; not merged, not deployed).** Brandon, after seeing spatial1 live: the video "shows up (kinda). Its supposed to be drawn over your droplet and it should be smaller and round. and if you click on it it should enlarge it and there should be a fullscreen button that appears when you hover it when its enlarged". Before: a 140px rectangle in a strip at the bottom right of the page. Now (`assets/here_media.js`, site.css `.lg-media-bubble`): a 42px round bubble inside that person's dot, so it moves with it; click enlarges it over the dot (up to 240x180, rounded corners), click again shrinks it; enlarged, a fullscreen button shows on hover (always on a touch screen, which cannot hover). Sound-only people get no bubble. Claude's choices: the enlarged picture is a rounded rectangle, not a circle; your OWN camera is still not shown to you. Check `tests/browser/here_media_bubble_check.py` (two browsers, fake camera, real server; 7 FAIL of 8 on the old script, 8/8 after); here_media, here_dots and here_pad_room still pass. Its CI step is a patch for Brandon. Profile pages only: the map has no camera or microphone.
- **2026-11-14 07:56 PT -- Spatial map walker: instances, presence dots, idle-socket fix (spatial1) LIVE (commit `c7dfbb5`, PID 846784, public checks FAILS 0).** Brandon ran ship.py himself from PowerShell (deploy, merge, go; cloud Claude read the output). The sync dry run listed 26 files, all spatial (walker instances, presence room `/map`, dots, their tests; here.cpp, map_routes.cpp, assets_routes.cpp), so the idle-socket fix was already live before this deploy; the purpose line names it by Claude's mistake (an out-of-date reading of these notes). On Windows `prebuilt_check.py` fails with exit 127 (bare `bash` is WSL's bash.exe), which refused ship.py's pair commit, and then its push to master after SHIP_READY. With Brandon running them: pair committed and pushed with `--no-verify`, and master moved by `git push --no-verify origin c7dfbb5:refs/heads/master` only after ship.py showed platform run 43611637700 and hook-checks both success for c7dfbb5. ship.py's own `spatial1-go.sh dry` step was skipped (the go run did its preflight and dry run). Not verified by Claude: the live site signed in (two people on the map with Drop on seeing each other's dots). Open: the Windows hooks cannot pass prebuilt_check, so every pair commit and master push from PowerShell needs this bypass until that check is skipped on Windows (as Linux skips the Windows-only ones) or run from WSL.
- **2026-11-14 PT -- Map presence consent: keep the profile-pages Yes (Brandon decided).** Asked whether the map room should have its own consent or use the "show me on the map" setting, since the "Show yourself on profile pages?" question never mentions the map. Brandon: "theres really not any users that arent my friends, dont worry". So no change before the spatial deploy. Claude's note: revisit the question's wording when people outside his friends join.
- **2026-11-14 PT -- Map draws the other droplets in your instance (branch `claude/upbeat-ramanujan-z42rkm`; not merged, not deployed).** Brandon: draw the presence snapshot "as small circles near the walker when Drop is on", gone on Scroll or when they leave the instance; no persistent CSS, server-side valves or STUN. `lgmap_walk.js` hereDraw: one 16px round portrait (else first letter) per handle on a ring around the walker (`#lgm-here .lgm-here-dot[data-u]`, styles in lgmap.css), handle order, at most 12, not tappable, no name label. Claude's choice: a ring, because the positions people send are screen fractions that mean nothing on another person's map. Check `tests/browser/walker_presence_dots_check.py` (two browsers, real server; 4 FAIL of 5 on the old walker, 5/5 after). CI steps for it and for walker_presence_check are a patch for Brandon (no workflow access here): `ci: add walker presence check` plus one for the dots check.
- **2026-11-14 PT -- Map walker sends its instanceId to presence (branch `claude/upbeat-ramanujan-z42rkm`; not merged, not deployed).** Brandon: "Wire the map walker so it sends its current instanceId to the presence server on every position update", keep it small, no deploy. While Drop is on, `assets/lgmap_walk.js` opens `/here/ws?page=/map` and sends `{x, y, instanceId}` whenever its place or instance changes; Scroll closes the socket. Who the snapshot holds is kept on `#lgm-walker[data-here]`; nothing is drawn yet. Claude's choices, NOT yet confirmed by Brandon: (1) the presence server only had profile-page rooms, so `here.cpp` gained one map room, `/map`, for anyone whose "here" choice is Yes (that question's wording says "on profile pages"; the map now leans on the same Yes); (2) x, y are the walker's place on the screen as fractions (the map has no page coordinates). Open wrinkle from step 3, not changed: `mine:groups`, `mine:friends` and the like are the same box id on everyone's map, so two people on their own Groups boxes share an instance. Check: `tests/browser/walker_presence_check.py` (two real browsers, real server; 6 FAIL on the old walker, 6/6 after); the other walker checks, here_media and the here e2e suites still pass. Its platform.yml step (with the seven spatial steps) is a patch for Brandon: this session cannot push workflow files, so `workflow_browser_steps_check` W1 fails for these eight until it lands.
- **2026-11-14 PT -- Git hooks no longer damage the clone they run in (branch `claude/upbeat-ramanujan-z42rkm`; not merged).** Cause confirmed in a throwaway clone with a hook that looked after each check: git gives a hook GIT_DIR and GIT_INDEX_FILE, `.githooks/pre-push` (pre-commit execs it) passed them to every check, and `prebuilt_check.py` (the only one that did it; the ship/ci_gate/sync_gate checks changed nothing) ran its throwaway `git init` / commit `one` / commit `two` on the real repository: core.bare=true from pre-commit, the flip plus both commits from pre-push, in a linked worktree. Fix in one place: pre-push unsets every GIT_* variable after the selector has run (the selector still needs GIT_INDEX_FILE for `git commit -a`) and before the checks, the same as `scripts/run_hook_checks.py` already did for CI. New `commons-daemon/tests/hook_git_env_check.py` (real hooks, real commit and push, linked worktree and main checkout, a probe check): 14 FAIL of 33 on the old hook, 33/33 after. No frozen check was edited. Not checked: Windows/Git Bash (written to be portable, not run there); `.githooks/commit-msg` is unchanged (it runs no checks). Seen in passing on this cloud clone, not caused by this: naming_check fails on a non-`wip/NN-` branch name, rollback_guard_check fails in a shallow clone (history commits missing).
- **2026-11-13 PT -- One idle socket no longer stalls the site (`wip/66-idle-socket` MERGED to master `0734e72` by ship.py on Brandon's "go"; not yet deployed: the droplet step needs the PC).** Cloud session taking over the "flake and ASan" job after the first session ran out of usage; its lead held: the accept loop in `http_server.cpp` read every request on its one thread with a 30 s receive timeout, so a browser preconnect (or any socket that sent nothing) held every other request for up to 30 s -- measured on a Debug build, `/assets/site.css` 0.3 ms warm vs 30.7 s behind one idle socket (the preview's "stylesheet stalls"). Fix (industry pattern, smallest change): each accepted connection gets its own thread for socket I/O (read, 413 drain, send); `handle()` runs one request at a time under `g_site_mu`, so the site's single-threaded logic (thread_locals, g_mu, the store) is untouched; send gets the same 30 s timeout; above 64 concurrent connections the server answers 503 at once instead of queueing. New suite `tests/e2e/idle_socket.sh` (port 18131): 0.007 s behind 1 and 60 idle sockets, fast 503 at 70, recovers, `/me` works behind idle sockets; FAILS on the previous build (31 s). lgcheck on Debug: every other suite PASS. ASan+UBSan (-fsanitize=address,undefined, whole lgcheck): idle_socket and every e2e suite PASS; one unit test failed, a real latent bug in `tests/program_messaging_test.cpp` (`std::string(pe_file().begin(), pe_file().end())`: iterators from two different temporaries, undefined behaviour that only passed in Debug by address reuse), fixed on the same branch. No sanitizer finding in the server itself. The same class of bug was fixed in commons-daemon's `http.cpp` with a 5 s/10 s budget; that still blocks others for 5 s per idle socket -- parked: give the daemon the same thread-per-connection shape if its preview ever shows it.
- **2026-11-13 00:09 PT -- Store split into modules (same bytes on disk), faster and prebuilt builds, WebSocket caps (split64) LIVE (commit `69ba657`, site code = `ed19a1d` from wip/64-ws-caps-hooks, PID 829999, public checks FAILS 0).** Brandon: "fix it and deploy. use industry standards". wip/64 (wip/60-63: store split, big-file split, fast build, prebuilt deploys, WS caps) merged at ed19a1d (SHIP_MERGED; CI 43378773090 green, 26 VERDICT ALL-PASS, store round-trip tests PASS). Deploy pair wip/65-deploy-split64 (69ba657), CI_GATE_OK platform run 43389392643. First go refused by rollback_guard (OK vs live da21b20; REFUSED vs styles1 e8a429c: the here record type came with here1); deployed by hand with Brandon's OK (12:02 AM PT) `LIQUIDGOV_NO_ROLLBACK_OK` = undo is roll back to here1 da21b20 (passes the guard); styles1 would first need here records stripped. Store backup `liquidgov_accounts.lgp.pre-split64-20261113T080358Z` (26,706 bytes); the refused first go also left `...pre-split64-20261113T075936Z` and the old binary copy renamed to `liquidgov_http.pre-split64-attempt1` (cmp-identical to here1). NRESTARTS=0 stable. Live /assets/lgmap.js and /assets/canvas.js SHA-256 = master. `--version` exists but prints "unknown tree unknown" on a server build. Found: commons-daemon/tests/prebuilt_check.py, run by the pre-commit/pre-push hooks, inherits GIT_DIR/GIT_INDEX_FILE in linked worktrees and committed "one"/"two" onto the real branch and set core.bare=true (twice tonight; backups backup/wip64-junk-20261112, backup/split64-junk-20261112, local); on Windows it also fails with exit 127 (Python finds WSL bash.exe), so the pair commit and pushes ran from WSL. Logs: scratch/2026-11-12-grokbot-ship64/ (go2.log).
- **2026-11-13 PT -- WebSocket caps, client address fix, git hooks run (`wip/64-ws-caps-hooks`, stacked on wip/63; not live).** Brandon: "yes build the websocket caps and hook fix", then "go by industry standard everywhere". (1) Every live socket (/community/ws, /here/ws, a group's jukebox) now takes a slot from `core/ws_limits` before its 101: at most 20 per client address and 200 in all (`LIQUIDGOV_WS_PER_IP` / `LIQUIDGOV_WS_MAX`); over either it answers 503 and logs `ws cap: refused <path>` (no address); the slot is released when the session thread ends. (2) `forwarded_ip` took the FIRST X-Forwarded-For entry, which the client controls (login throttling could be dodged with a fake header); it now takes the LAST (the one nginx appends) and believes the header only from a loopback peer. (3) Git hooks now run (second commit): `.githooks/pre-commit` and `pre-push` were mode 100644, so git skipped them with a hint; now 100755. That made a Linux commit touching the hooks run every check, including the Windows-only `tidy_worktrees_check.py` (calls `cmd`); Brandon: "yes skip windows checks on linux": `affected_checks.py` now leaves out `run_hook_checks.WINDOWS_ONLY` off Windows, exactly like CI, and says so on a SKIPPED line (`hook_windows_skip_check.py`: 3 FAIL before, 6/6 after). (4) `deploy/nginx-liquidgov.us.conf`: `limit_conn` 20 per address on the WebSocket paths only (`nginx -t` ok; needs its own nginx deploy, separate from the program). (5) `deploy/ci/platform-ccache.md`: the three workflow edits that give CI a compile cache (needs workflow access). (6) `deploy/ci/platform-always-report.md` (Brandon: "whats industry standard?" ... "yes write it"): platform.yml loses its path filter and gains a `changes` job, so every commit gets a result (skipped = success in seconds when the branch has no platform change against master); ends the filler commits `ship.py` needed (`81be56e` has no platform run). Needs workflow access; apply with the ccache edits. Tests: `tests/e2e/ws_caps.sh` (port 18130) 6 FAIL on wip/63, 15/15 after.
- **2026-11-13 PT -- MODULARIZE EVERYTHING: store split, big files, fast build, prebuilt deploys (wip/60..63, stacked; none merged, nothing live).** Brandon's brief (cloud session; no deploy, no server contact). **wip/60-store-split** (`aa5d9ea`; platform 43367218281 success on `909065f`, hook-checks 43367572526 success): wip/52-file-store-split redone on master bb6e046; file_store.cpp 4,012 -> 223-line frame + src/core/ + 19 src/records/*.cpp (styles.cpp, here.cpp new, moved verbatim); file_store.hpp 1,355 -> 891 lines + 14 include/liquidgov/platform/records/<family>.hpp (all 438 lines verified moved). Same bytes on disk: tests/store_roundtrip_test.cpp (frozen fixture) + NEW store_roundtrip_styles_here_test.cpp (answers by bb6e046; fails 3/3 with two write sections swapped). Frozen edits with Brandon's OK this session: undo_lists_check.py:32, styles_undo_lists_check.py:46 (read the joined store text). **wip/61-split-big-files** (`4b4860e`; platform 43368569849 success): canvas.cpp 1,060 -> 483 (engine -> assets/canvas.js, byte-identical per page); deleted preview.py, deploy/archive/, /u/<name>/theme, the World demo (/world/ws). Not split: juketube.js, lgmap.js (asked; recommended leave), city_store_test.cpp (frozen). **wip/62-fast-build** (`0547ad5`; before 43368981908 on `971da98`, after 43370327177, both success): file_store.hpp reaches 75 of 107 translation units (was 90); LG_PCH (std headers, default ON; OFF build checked on CI); ccache when present. CI -j2 clean build step 88s -> 70s; -j1 touch rebuild: feature .cpp 1.6s/1 -> 1.3s/1, store record .cpp 2.0s/1 -> 1.4s/1, a record header or file_store.hpp 80.3s/82 files -> 53.2s/67. **wip/63-prebuilt-deploy** (`09d7854`): deploy/PLAN-prebuilt.md + --version, LG_STATIC_RUNTIME (binary needs only libc/libm, GLIBC_2.38), scripts/build_release.sh, deploy/ci/release-binary.yml (MOVE to .github/workflows/ to enable; this session cannot write workflows), deploy/fetch-prebuilt.sh, prebuilt-go/remote template pair, make-go LIQUIDGOV_PREBUILT=1; tests prebuilt_check.py (14/14; 14 FAIL on wip/62), e2e/version_flag.sh (4/4; 2 FAIL on wip/61). A typical feature deploy now recompiles that feature's .cpp (1 file, ~1-2s) plus relinking; a new store record in its own module recompiles that module, but a change to any record header or file_store.hpp still recompiles the 67 store users (FileStore is one class: the next step would be a per-family store object/pimpl). Not checked: the prebuilt remote phase on the real server (not contacted); the droplet's OS/glibc (not in the repo; the go script checks it); the release workflow on GitHub (not enabled); the canvas page in a real phone browser beyond the canvas checks; the stylespreview check's last step still times out on some CI runs (report-only; server handles one connection at a time).
- **2026-11-13 PT -- Big files split, dead code trimmed (`wip/61-split-big-files`, stacked on wip/60; not live).** Brandon: "MODULARIZE EVERYTHING", "delete anything that doesnt contribute... trim the bloat". canvas.cpp 1,060 -> 483 lines: canvas_tool_page's inline engine (586 lines of JS in C++ strings) is assets/canvas.js, fed by window.LG_CANVAS {px, frames, user, mode, postTarget}; converted by script, proven byte-identical per page (/me/canvas with 0 and 2 frames, /me/portrait, /feed/canvas, /me/post/canvas, /c/<slug>/canvas: the old page's inline script == the asset's text for that mode with that page's values). Trimmed: liquidgov-platform/preview.py (the disconnected JSON-store toy), deploy/archive/ (spent Juketube installers), the /u/<name>/theme route (unused since profile CSS styles the real page), the dead World demo (world_page, run_world_session, its state and main()'s /world/ws hook; /world itself still redirects to /commons/). Not split, on purpose: assets/juketube.js and assets/lgmap.js (one feature each, one closure each; JS is not compiled so a split saves no build time, and 17 frozen checks read them by path; Brandon asked, recommended leaving them); tests/city_store_test.cpp (frozen). Other files over ~400 lines, not split: commons-daemon/src/main.cpp 1,534 and commons/js/*.js (the Commons game, its own program); file_store.hpp 891 (FileStore is one class); records/proposals.cpp 598 (one family); admin.cpp 488, jukebox_routes.cpp 449, map_rules.cpp 439, jukebox_widget.cpp 422, core/http.cpp 414, ship.py 416, map_live_check.py 482 (each one feature or a frozen check: leave); liquidgov-core state.cpp 730 (the governance kernel: out of scope). Tests: e2e/canvas_asset.sh (port 18127; 23 FAIL on wip/60, 33/33 after), e2e/trim_dead_routes.sh (port 18128; 2 FAIL on wip/60, 5/5 after); canvas-layout, canvas-resolution, groupcanvas, phone and site-spelling browser checks ALL-PASS.
- **2026-11-12 20:26 PT -- Dots on profile pages, style library links, D-pad room (here1) LIVE (commit `da21b20`, site code = master `5c75d25`, PID 826609, public checks FAILS 0).** One branch, wip/58-dpad-room (wip/53 + wip/57 + the D-pad fix, plus the CI step for here_pad_room_check, 5c75d25), merged on Brandon's "merge" (SHIP_MERGED, 26 VERDICT lines ALL-PASS incl. herepad-ci). Deployed on his "go" with `LIQUIDGOV_NO_ROLLBACK_OK="here: stop the site, python3 deploy/here-undo.py <store> --go"` (rollback_guard refused as expected against live e8a429c and a6a344b: 78 written record types vs the old loaders' 79 and 75 types). Run by hand from the deploy worktree (runner scratch/2026-11-12-claude-ship/here1_go_override.py, log here1-go.log). Store backup `liquidgov_accounts.lgp.pre-here1-20261113T042108Z` (26,709 bytes). CI_GATE_OK on platform run 43363732107 (master push; the gate waits for the newest run of the tree). The first dry run died on WSL `accept4 failed 110`; one retry went through. Live probes from the PC: / 200, /assets/here.js 200, /styles 200, /login 200. NOT verified: dots or the D-pad on a live profile page signed in, and the style links signed in (CI's heredots/herepad/styles_findable cover them); not looked at on a phone. The build compiled 69 files on the server (here1-go.log "Building CXX" lines) because file_store.hpp changed; Brandon then had the web Claude prompted to finish the store split, add per-type headers, precompiled headers and ccache, and build the program on GitHub (scratch/2026-11-12-claude-web-prompt/prompt-modularize-all.md).
- **2026-11-12 PT -- Dots: the D-pad no longer covers the page's end (`wip/58-dpad-room`, stacked on wip/57; not live).** Brandon: "do the cleanup and the dpad fix". On a phone the open D-pad (about 200px tall, fixed bottom-left) covered the footer and the last buttons; the page already keeps 134px at its end (for the Juketube button), enough for the Walk button but not the D-pad. assets/here.js now grows the body's bottom padding to the pad's height + 24px while it is taller than that room, and gives it back on Stop walking (asset only: no build, no restart). Test: browser/here_pad_room_check.py (R2/R3 FAIL on wip/57, ALL-PASS after; heredots and phone checks still ALL-PASS); its platform.yml step must be added by a session with workflow access (else workflow-browser-steps blocks ship.py). Cleanup asked for the same day: merged branches wip/52, wip/54, wip/55-deploy-groupcanvas1, wip/56-deploy-styles1 could not be deleted from the cloud session (the GitHub proxy refuses ref deletes); wip/53 is inside wip/57.
- **2026-11-12 PT -- Style library findable (`wip/57-styles-findable`; not live).** Brandon after styles1 went live: "i dont really see anything with the style library" (read-only signed-out look at liquidgov.us/styles: it works, one style "Night Paper" by Bbottin, 0 uses, but NOTHING linked to /styles except deep in /me). Then: "dont fuck shit up but take initiative" and "dont make me do shit". Added: a "Styles" link in the signed-in account menu (after Groups) and the signed-out nav (after Laws); a "Browse styles" button at the top of /me's Profile CSS card; on a profile whose CSS came from a published style, a "Style: <name> by <author>" credit (links to /styles#style-N and /u/<author>), outside .lg-profile-scope, shown only while the profile's CSS is shown (not in plain view), the style is still listed and visible to the viewer (block either way hides it). No store change, no new browser check (so no platform.yml step needed). Test: e2e/styles_findable.sh (port 18126; 9 FAIL on master's code, 18/18 after). Also this session: a personal cloud environment "liquidgov-us" (Default + liquidgov.us, Brandon approved) so cloud sessions can do signed-out, read-only checks of the live site.
- **2026-11-12 18:50 PT -- Profile style library: publish, browse, copy, report, take down (styles1) LIVE (commit `e8a429c`, PID 824643, public checks FAILS 0).** Brandon's "go" after the dry run, and his OK for `LIQUIDGOV_NO_ROLLBACK_OK="styles: stop the site, python3 deploy/styles-undo.py <store> --go"` (rollback_guard refused against live a6a344b and 41966a7 as expected: 77 written record types vs the old loader's 75). Run by hand with that override (ship.py strips overrides; runner scratch/2026-11-12-claude-ship/styles1_go_override.py, log styles1-go.log). Store backup `liquidgov_accounts.lgp.pre-styles1-20261113T024521Z` (26,447 bytes), old binary kept; store lines 409 before and after (no sty/styn/stu/str rows yet, so until someone publishes a style the old binary could still open the store). Journal clean, NRESTARTS=0. Extra live probes: /styles 200 signed out ("Profile styles", Newest / Most used, "No styles yet"), /styles?sort=used 200, /admin/styles and /me -> login, /styles/1/preview 404 (no style 1 yet), live /assets/site.css = master (SHA-256 match). NOT verified: publish, preview, Use this style, Report, Take down signed in on the live site (no live test account; CI's profile_styles_library.sh 80/80 and stylespreview-ci ALL-PASS cover them); not looked at on a phone. Closes when Brandon publishes a style, previews it and uses one on his iPhone.
- **2026-11-12 17:18 PT -- Group post box: Canvas button opens the canvas, not the photo picker (groupcanvas1) LIVE (commit `a6a344b`, PID 822848, public checks FAILS 0).** Brandon's "go" (deploy pair merged on his "merge and deploy"). The same deploy also took the profile CSS change below live (13 files). Live /assets/site.css = master byte for byte (22959 bytes, SHA-256 match: `.filepick` clips, `.canvaspick` present). NOT confirmed fixed on a real iPhone: the only tap test is Chromium imitating WebKit's sizing; not tapped signed-in on the live site either (the tap check makes a group; no live test account). Closes when Brandon taps Canvas on a group page on his iPhone. First go stopped at `remote backup` (WSL `accept4 failed 110` calling ssh.exe; nothing synced); one rerun went live. The go script retries that error only for the sync; giving the backup/chown steps the same retry needs Brandon's OK.
- **2026-11-12 PT -- Group post box: "Draw instead" opened the photo picker; now a "Canvas" button (`wip/54-group-canvas-button`; merged `9de2475`; LIVE as groupcanvas1 above).** Brandon: "when i click draw instead in a group page's post section, it just brings up camera option, pictures, or files. not the canvas. and is should just say canvas and have a drawing or painting icon". Cause: composer_attach_bar's photo button is an invisible `<input type=file>` (font-size 4rem, inset:0) in a 2.4rem label that did NOT clip it; WebKit (iPhone Safari) sizes an absolutely positioned file input to its own width (measured 1397x103 px when sized that way), so it covered the "Draw instead" link just below and a tap opened the picker; Chromium stretches it, so a desktop check never saw it. Fix: `.filepick{overflow:hidden}` + the input `width/height:100%`; the link is now a "Canvas" button with a paintbrush icon in the post bar, outside the photo label (composer_attach_bar's new canvas_href; only group pages use the bar). /feed's "Or open the canvas" line is unchanged (its file input is `hidden`, not the same bug). Tests: e2e/group_canvas_button.sh (port 18125; 5 FAIL on 07b2ab8, 12/12 after), browser/group_canvas_tap_check.py (sizes the input the WebKit way, then taps; 4 FAIL on 07b2ab8, ALL-PASS after; its platform.yml step added in a follow-up commit). Not checked on a real iPhone.
- **2026-11-12 PT -- Profile style library, slice 2 of profile CSS (`wip/52-style-library`; not live).** Brandon's decisions: anyone signed in publishes their CURRENT Profile CSS under a name (must pass sanitize_author_css at publish); /styles lists Newest / Most used with author + preview, open to anyone signed in or not; "Use this style" COPIES into your own Profile CSS (no live link) and /me says "Based on <author>'s <name>" (stays through edits to the copy, goes when the CSS is cleared or the style leaves the library); a published style is a frozen copy, never edited (author can Remove; copies stay); Report goes to a NEW /admin/styles (there was no report queue in the code: moderation = /admin/* delete pages + <store>.moderation.jsonl), reported first; operator Take down (archived to moderation.jsonl + alog delete_style) or Dismiss. Blocks hide styles both ways. Previews are `<iframe sandbox>` (no allow-*) on /styles/<id>/preview (own CSP sandbox), so the browser, not the filter, keeps each style in its frame. New LGP1 records sty/styn/stu/str (lower-bound field checks, forward compatible); styn is written only once a style exists, so an untouched store still opens on the old binary. Rollback: `deploy/styles-undo.py` strips only those four (copied CSS lives in ac and survives); rollback_guard.py WILL refuse this deploy, so it needs LIQUIDGOV_NO_ROLLBACK_OK naming styles-undo.py. Frozen edits with Brandon's OK ("i dont care" which way): lgp1_consistency.py check 3 accepts styles-undo.py's STYLE_TAGS; rollback_guard_check.py's three history checks expect +STYLE_TAGS. Tests: style_store_test.cpp (did not compile on 07b2ab8: no API), tests/e2e/profile_styles_library.sh (port 18122; 55 FAIL on 07b2ab8, 80/80 after), tests/browser/styles_preview_isolation_check.py (hostile full-screen red style next to a plain one; 9 FAIL on 07b2ab8, ALL-PASS after; its CI step in platform.yml is NOT on the branch: the cloud session's GitHub token has no `workflow` scope, so the step is a patch for Brandon to apply; until then workflow_browser_steps_check (report-only) flags it), tests/rollback/styles_undo.sh (by hand, needs the previous build; 28/28), commons-daemon/tests/styles_undo_lists_check.py. Limits: name 60 chars, 20 styles per person, report reason 300 chars, 24 per page.
- **2026-11-12 PT -- Dots on profile pages, slice 1 (`wip/53-profile-dots`; not live).** Brandon: every page could show "anyone who currently is visiting that page ... as their round self portrait", walking with a D-pad up to buttons/fields to use them, later webcam/mic by proximity. Then: "this would be a lot different than the game... just the normal pages with little dots for people" (so NOT under the 2026-09-16 game split: no world state, positions memory-only). Decided: profile pages first (/u/<name>; not /friends: a profile is the same page for everyone); a REQUIRED Yes/No at signup, nothing pre-selected (his 2026-11-12 privacy rule), existing accounts asked once by a card on a profile page, changeable in Settings; tapping/clicking keep working, "Walk" turns on a D-pad (arrow keys) and A/Enter presses the button or field your dot is next to; camera/mic AFTER the dots (off by default, mutual opt-in, never across blocks, louder when closer). Built: new LGP1 record `here <username> <0|1>` (absent = not chosen = hidden; no rows written until someone chooses, so an untouched store still opens on the old binary); `/here/ws?page=/u/<name>` (hook from main()'s accept loop like the other sockets; one room per profile page, memory only, 50 per room, <=20 moves/s, ~5 snapshots/s; join needs Yes + can see the profile + no block either way; blocks also filter the snapshot; a No/new block/private profile ends the visit); assets/here.js + site.css (the layer sits under the header, outside .lg-profile-scope so profile CSS can't hide or restyle dots). Undo: deploy/here-undo.py (strips here only: everyone is hidden and asked again). Brandon also OK'd "make the undo checks generic": lgp1_consistency.py check 3 and rollback_guard_check.py now read every deploy/<feature>-undo.py's <NAME>_TAGS (frozen files, edited with his OK), and commons-daemon/tests/undo_lists_check.py checks each script against tests/rollback/<feature>_undo.sh. This replaces wip/52's styles-only edits to those two files (resolve a merge conflict by taking wip/53's version; wip/52's styles_undo_lists_check.py becomes redundant but harmless). Ports: e2e 18123/18124 (18122 is wip/52's), rollback 18142. Tests (fail first on 07b2ab8): here_store_test.cpp (no API), e2e/profile_here_choice.sh (18 FAIL -> 31/31), e2e/profile_here_ws.sh (+ here_ws_client.py; 11 FAIL -> 22/22), browser/here_dots_check.py (walk to "Send friend request", press A, request sent; FAIL -> ALL-PASS; its platform.yml step is a patch for Brandon: no workflow scope here), rollback/here_undo.sh (by hand; 26/26). Phone note: the fixed D-pad covers whatever is bottom-left while walking.
- **2026-11-12 PT -- profile CSS styles the whole profile page below the header (`wip/51-profile-css-page`; LIVE 2026-11-12 17:18 PT with groupcanvas1, commit `a6a344b`).** Brandon: "can we make the css area cover the whole profile page (except the title banner thing)"; all four recommended options (decisions.md). The old 320px sandboxed frame (/u/<name>/theme) is gone from /u/; the CSS is emitted as `.lg-profile-scope{<css>}` (native nesting) inside `.lg-profile-outer` (contain:paint, so fixed/absolute boxes are clipped to the profile area, never over the header). `.lg-theme` stays inside the scope, so old CSS still matches; style the whole area with `.lg-theme`. A "Plain view" link (?plain=1) sits outside the styled area. Without the sandbox the filter is the defense: no `\` (escapes walked past every word check: `u\72l(` was accepted before), no `&`, balanced braces outside strings/comments, no image-set()/image()/cross-fade()/element()/src()/paint(), only @media/@supports/@container. Stored CSS is re-checked at render: CSS saved under the old rules that fails is not shown, and /me says why. @keyframes don't work inside nesting, so they are refused for now. /theme still answers (bloat list). Tests: tests/e2e/profile_css_page.sh (26 FAIL on the old build), tests/browser/profile_css_contain_check.py (hostile full-screen fixed box; CI step). Next slice: the CSS library.
- **2026-11-12 PT -- every git-hook check now also runs on GitHub (`wip/50-ci-hook-checks`; not merged).** Waste-audit PLAN 0.3 (Grok Bot), Brandon: "you take 0.3". Before: 0 of the 27 checks in commons-daemon/tests ran in any workflow, so a clone without core.hooksPath or a hook-skipping push got past them. New `.github/workflows/hook-checks.yml` (every push to master and wip/**, no paths filter) runs `scripts/run_hook_checks.py`, which takes the list from affected_checks.py (the hooks' own list; a new check file is picked up automatically) and fails on an empty run. `ship` waits for every workflow on the commit, so once merged this gates deploys too. Kept wired by `ci_runs_checks_check.py`. Locally (Windows, Git Bash Python) 27/27 pass in ~3 min; not yet run on GitHub's Linux. Not covered: commit_msg_check.py on the pushed range (it reads staged files; needs a range mode).
- **2026-11-12 11:55 PT -- map: people under an interest hidden until the signup privacy step (hideint2) LIVE (commit `41966a7`, PID 820075, public checks FAILS 0).** Brandon's "go". Store record counts unchanged; live map_graph.cpp has `kInterestPeopleShown = false`; journal clean. Not verified signed-in on the live site (no test account; CI's map_interest_people_hidden.sh covers it). Took 3 attempts: hideint1 stopped after its backup (CI gate's git.exe read dropped by WSL interop; nothing synced), hideint2's first try was refused before backup (ci-gate.sh:70 read a dropped call as "no CI run"), its retry never started (WSL down). Fixed in 41966a7 (deploy/git-in-repo.sh retries git.exe; no silent skip; ci_gate_interop_check.py). Left on the server: liquidgov_http.pre-hideint1 + its store copy (bloat list). Still open: the 2nd CI gate runs after the backup.
- **2026-11-12 PT -- Signup privacy step decided; people under interests hidden until it exists (`wip/49-hide-interest-people`; not live).** Brandon: the privacy step "ships with the interest boxes or before them, never after", but interest people went live first (mapkinds1). Now `map_interest_links` stops before the people block (`kInterestPeopleShown = false` in map_graph.cpp); interest boxes still grow groups and proposals; people stay under Friends etc. New suite `tests/e2e/map_interest_people_hidden.sh` (port 18120; failed on c5d464a, CI run 43276006661); 6 frozen lines flipped with Brandon's "ok" (map_interests_feeds.sh:98,105; map_interest_words.sh:71,84,87,91). Turning people back on = the switch + flipping those lines back. Same day, Brandon decided (shared-context decisions.md): EVERY privacy setting is a required choice at signup, nothing pre-selected, no Skip, with his message ("We built this website with the intention of strengthening networks between people, ideas, and truths, but your privacy is uniquely important to us. We won't assume your preferences..."); this replaces "discovery open by default" and map SPEC 16's preset/Skip. Also decided for the map redesign: modes replace the kind ring; 30-day contacts = messages, comment replies, event invites, tags; friends' lines only as each friend's settings allow, votes never; traversal tallies public with a Settings switch, owners see who visits a lot (blocked/Ignored never; name only after several visits); a selected page object opens on the right.
- **2026-11-12 08:37 PT -- jukebox script served as assets/jukebox.js; JS parse check in CI (jukebox1) LIVE (commit `dfc4d94`, PID 817025, public checks FAILS 0).** Brandon's "go". Live /assets/jukebox.js = repo file (sha256) and parses (node --check). jukebox_script() now emits `<script>var LG_JUKEBOX={standalone,ownUsername,stations}</script>` + the file. New gating CI step: tests/assets_js_parse_check.py (every assets/*.js parses). Not verified: the player on a signed-in /listen (redirects to login when signed out). Still inline: canvas_tool_page.
- **2026-11-12 07:09 PT -- page scripts served as assets/*.js (same code) (pagejs1) LIVE (commit `01cf8fc`, PID 814955, public checks FAILS 0).** Brandon's "go". mail_widget.js, draft_publish.js, people_search.js served 200 and byte-identical to the repo; pages no longer inline them. Still inline: jukebox_script, canvas_tool_page (they mix in per-page values). Two earlier go attempts never started (WSL WSAETIMEDOUT; server unchanged); cleared by wsl --shutdown.
- **2026-11-12 06:24 PT -- site stylesheet served as assets/site.css (same CSS) (sitecss1) LIVE (commit `0d39553`, PID 813350, public checks FAILS 0).** Brandon's "go". css() is gone; every page links `/assets/site.css?v=<hash>` (live file sha256 = repo file; text/css, cached a year when versioned). Home page HTML 35,394 -> 12,649 bytes. Before/after screenshots of /, /laws, /login, /signup, /privacy at 1280 and 390 wide: 7 of 10 byte-identical; signup-phone looks identical (Google button render), home differs only in the intro video frame. Store backup `.pre-sitecss1-20261112T142355Z`, old binary `liquidgov_http.pre-sitecss1`. Note: the sync never deletes, so old src/site/ui/site_css.cpp/.hpp stay on the server (not built).
- **2026-11-12 05:12 PT -- site server split into modules (no behavior change) (modular1) LIVE (commit `a83e172`, PID 811918, public checks FAILS 0).** Brandon's "go". Store backup `liquidgov_accounts.lgp.pre-modular1-20261112T130810Z` (27,094 bytes), old binary kept as `liquidgov_http.pre-modular1`, ROLLBACK_GUARD_OK (file_store.cpp unchanged), CI run 43221844822. Extra live probes OK (/, /laws, /login, /signup, trailing-slash redirect, 404, /admin -> login, /internal/* refused). Not run: the headless console check (mobile_sweep.py failed to launch Edge twice); not looked at on a phone.
- **2026-11-12 -- site server split into modules (wip/45-modular).** Brandon: "make it modular". `liquidgov-platform/src/http_server.cpp` (13,097 lines) is now `main()` only; the site is `src/site/` (core, ui, `features/<name>/` with one `<name>_routes` per feature; `handle()` is 136 lines). Map + rules: `src/site/MODULES.md`; file list: `src/site/sources.cmake`. Code moved, not rewritten (all 210 function bodies and 176 route blocks byte-identical; route order checked). Checks that read the server's text go through `tests/browser/server_source.py`. Anyone with a branch that edits http_server.cpp (wip/25-settings, wip/12-mainstream-search, wip/1-error-handling): redo the change in the matching module. `file_store.cpp` split comes after this deploy.
- **2026-11-12 03:00 PT -- ringfix1 LIVE** (commit `5e9b48c`, PID 808907, asset-only, public checks FAILS 0): the You ring always shows all your kinds. Live `/assets/lgmap.js` = a955c91's file (git blob hash). Not yet looked at on a phone. (The go script's LIVE_MATCH step checked juketube files, not lgmap.js; lesson recorded.)

- **2026-11-11 13:29 PT -- Map: Interests, Feeds, filter bar, Back, Zoom out (mapkinds1) LIVE (commit `76a5ec1`, PID 797238, public checks FAILS 0).** Brandon's "go". Log: DRY_OK, CI_GATE_OK (run 43100485380), BACKUP_DONE, ROLLBACK_GUARD_OK, store lines 385 before and after (mapp 13 unchanged). Live `/assets/lgmap.js` = 76a5ec1's file (42,190 bytes, same text). Not in this deploy: wip/35 (Hidden mode, chip per feed/interest, Zoom out stops at MAINstream), not CI-proven yet. Not yet looked at on a phone.

- **2026-11-11 PT -- Map SPEC phase 1: Interests, Feeds, Events on the You ring; filter bar, Back, Zoom out (`wip/32-map-kinds`; not live).** Brandon: "yes, build phase 1" of Grok Bot's map spec (scratch/2026-11-11-grokbot-map-spec/SPEC.md, his 07:33-08:08 answers). Ring order is mockup A's: Politics, Friends, Events, Feeds, Places, Groups, Interests, Problems; an empty kind is still left off, so Events stays off until M1.4. **Interests** = the comma items of your profile's Interests line, read at view time (`int:<words-joined-by-dash>`, labelled as typed; no store change). An interest grows groups (name/about/subcategory) and proposals (title/body) that contain its words as WHOLE words; a simple plural is the same word (park = parks, bus = buses, house = houses; side agent's point) but "parks" never matches "Parking" or "Sparkshop". /home's "Might fit you" still matches substrings (left as is; say if it should follow). People under an interest: only those who list the same interest, pass map_person_ok (show-me, blocks), are Typeahead-findable and listed in one of your cities; at most 8, never a count. **Feeds** = Friends' posts, Your groups' posts, Posts near you (Anybody posts by people listed in your cities), Your posts; newest 12 each through map_post_ok; never Drawings; an empty feed is left off. int:/feed: boxes are viewer-only (404 for anyone else). **Client** (lgmap.js/css): a filter bar per level (at You one chip per kind, deeper one per box kind), kept in this browser's localStorage per level (SPEC: phase 5 moves it to the account as a new `mflt` tag), never a deletion (the note says so); Back via history.pushState (the phone's back gesture works; each entry carries the center's box data); Zoom out = item -> its center -> the center before -> from You the MAINstream; the bars row is sticky (canvas now `overflow:clip`, which doesn't trap sticky). Tests: `tests/e2e/map_interests_feeds.sh` (port 18103) and `tests/browser/map_bar_check.py` (+ `ci_map_bar_check.py`, a new report-only CI step whose `VERDICT bar-ci` ship.py gates like the phone one); both failed on 37ae3ec (CI run 43087523136). Not in this phase: deep links (`/map/node?n=` restoring a trail), phone trail collapse ("You > ... > Sam"), surprises, custom boxes.

- **2026-11-11 02:22 PT -- Security: placeholder password never signs in; map starts at You (loginmap1) LIVE (commit `c083fd0`, PID 790953, public checks FAILS 0).** Live `/assets/lgmap.js` checked: starts at `you`. Rollback rule: to undo only the map, revert `e7e02fe` and redeploy; do NOT restore `liquidgov_http.pre-loginmap1` (that reopens the login hole). Store backup `liquidgov_accounts.lgp.pre-loginmap1-20261111T101852Z`. Brandon: look at the new map on the iPhone (You box is clipped at the left edge on a phone; look work still to come).

- **2026-11-11 01:55 PT -- Security fix: the "no password" placeholder never signs in (`wip/27-placeholder-login`; Brandon "yes").** Google-only accounts store `dummy_hash()` as their password, a real argon2 hash of a fixed phrase in `password.cpp`, and `login()` only verified the typed password against the stored hash, so that phrase signed in to any Google-only account (live since 2026-09-15, 622569c; repo private). `dummy_hash()` gets a new salt each process, so stored placeholders differ from today's and an equality check would miss them: the fix is in `verify_password`, which now refuses the phrase against any hash (after the real verify, so timing is unchanged). Covers every caller (login, Settings > Password, and the settings branch's email change). New unit test `tests/placeholder_password_test.cpp`: 3 FAIL on master's password.cpp (scratch/2026-11-11-claude-placeholder-login/fail_first.log), green with the fix. Side effect of the same salt issue, not fixed here: `acc->password_hash != dummy_hash()` ("has a real password") is true for Google-only accounts made before the last restart, so Settings asks them for a current password they don't have.
- **2026-11-11 01:20 PT -- Map starts at You (M1.1 step 1, `wip/26-map-you`; not live).** Brandon 00:50 PT: "prioritize the map changes grok suggested" (Grok Bot's map review `scratch/2026-11-11-grokbot-map-purpose/REPORT.md`, top pick = M1.1). The signed-in map now opens on a `you` box (your handle, opens /u/you) whose first ring is kind boxes `mine:friends|groups|places|politics|problems` (an empty kind is left off; Places = your cities + the MAINstream as its own branch; Politics = proposals you wrote, voted on, reacted to or commented on, the same set as the relevance colours). Derived at read time: no record type, no store change. Only ever the viewer's own; every item still goes through `map_gnode` (blocks, show-me, audiences). Root links back to `you`. Not in this step: Interests and Events rings (no such boxes yet), the trail, a visible "Open page" button, box sizing/faces (M1.1 look: 2 mockups first). New suite `map_you.sh` (port 18106). Risk to check: `tests/browser/phone_check.py` skips only `lgn-root` when picking a box to tap.

- **2026-11-11 PT -- `ship` blockers fixed (`wip/31-ship-fixes`), from Grok Bot's review (scratch/2026-11-11-grokbot-ship-review/REVIEW.md).** Brandon: "yes fix the ship blockers first". B1 merge pushes exactly the commit check verified and refuses a branch that moved during the CI wait. B2 go streams its output to the log as it runs, no time limit; killed part-way = SHIP_GO_UNKNOWN (exit 3) with how to read the server's state. B3 Claude's own key now goes inside the command text (`export ...;`): wsl.exe gets no Windows env (WSLENV empty), so the first real dry run had used the shared deploy key. Proven in real WSL with a script probe, no server contact (an `echo $VAR` probe reads empty because wsl.exe expands it first). H2 deploy puts the pair on its own `wip/NN-deploy-<name>` branch + worktree, never master; go refuses until that branch is merged. M3 override variables in the shell are refused. H1 (hooks repo f4c2870): ship_word_guard lets `ship.py merge`/`go` run only when Brandon's own latest message is exactly "merge"/"go". Checks: ship_fixes_check 16/16 (fails on the old code), ship_check 20, ship_deploy_check 17 (lines 119-121 and 145-148 changed with his OK), ship_workflows_check 5. Not yet: M1 (phone VERDICT at deploy time), M2, M4, lows; a real deploy through ship.

- **2026-11-11 PT -- `ship`: one command checks a branch and merges it on Brandon's word (`wip/30-ship`).** Brandon: "yes fix the guard, then do ship". `python scripts/ship.py check <branch>` (read-only): pushed? fast-forward of master? files changed + the deploy pair make-go auto would pick; the newest platform.yml run for exactly the head commit (waits up to `--wait` min); the phone `VERDICT` line (ALL-PASS required when src/ or assets/ change); downloads the phone-screenshots artifact. Last line SHIP_READY / SHIP_NOT_READY with reasons. `ship.py merge <branch>`, only on his "merge": the same check, then a plain (never forced) push of the head to origin/master, read back. Checked: `commons-daemon/tests/ship_check.py` 20/20 with a fake gh and a real bare origin (fails 16 with no ship.py); real GitHub probe on e2f361b read "VERDICT ci: ALL-PASS (17 checks)" and downloaded 11 screenshots. Then (Brandon "yes build it"): `ship.py deploy <name> "<purpose>"` after the merge: main checkout on a clean master == origin/master; the sync dry run in WSL (own key) lists the content changes (only `<`/`>`/`c` rsync lines: the first real run listed 149 owner/group-only `.f....og...` lines, now ignored); `make-go.py auto` on those files; the pair committed (Read/Attacker/Undo lines) and pushed; `<name>-go.sh dry`; SHIP_DRY_DONE only with DRY_ONLY_DONE + CI_GATE_OK; an existing pair is reused. `ship.py go <name>`, only on his go: `<name>-go.sh go`; SHIP_LIVE only with exit 0 + CHECK_DONE + both RECORD lines (RECORD_FAILED fails). `check` now also reads every other workflow on the commit (a red or running commons run blocks). Checks: ship_check 20/20, ship_deploy_check 17/17, ship_workflows_check 5/5 (each failed on the code before it); real WSL sync dry run through ship's own shell(): exit 0, 0 content changes (master = live maplook2). Not yet: a real deploy through ship.

- **2026-11-10 21:35 PT -- MAINstream rename (mainname1) LIVE (commit `c9b3135`, PID 788585, public checks FAILS 0).** Brandon's "go". Live lgmap.js shows "‹ MAINstream", old spelling 0 times. WSL dropped twice in this run and both retries held: the preflight retry (dry run) and the sync retry after the rollback guard could not read git (ROLLBACK_GUARD_REFUSED on accept4, then OK). Also merged: the Commons workflow now runs only on its own files (31ac78f).

- **2026-11-10 PT -- "mAInstream" renamed "MAINstream" (`wip/23-mainstream-name`; not live).** Brandon: "lets change mAInstream to MAINstream". Whole-token rename in http_server.cpp (page title, map button, root box, links), lgmap.js (the "‹ MAINstream" back bar), lgmap.css comments, naming.toml's frozen button (renamed on purpose a second time; the comment keeps both quotes), and the two suites that check the visible name (city_map.sh, map_graph.sh: expected strings only, as part of his rename). Quotes of his earlier words keep the old spelling; spent deploy pairs' comments untouched. Checked by GitHub CI (full suite) on the branch.

- **2026-11-10 21:03 PT -- People you may know, friends visible by default (friends1) LIVE (commit `e8038f2`, PID 787340, public checks FAILS 0).** Brandon's "go" after the side agent's note that the 12 friends-on rows are one-way under a code rollback (undo: remove the mapp rows of people who got a `map_friends_on` notice). Checked live (read-only): 13 accounts, 13 mapp rows all friends-on; the 1 pre-existing row (already friends-on) unchanged vs the backup `liquidgov_accounts.lgp.pre-friends1-20261111T045846Z`; 12 `map_friends_on` notes; store +24 lines; journal clean. Preflight retry and rollback guard (73 types vs live loader 75) ran. Not yet: Brandon's look on his phone.

- **2026-11-10 PT -- People you may know + friends visible by default (`wip/21-friend-suggestions`; not live).** Brandon: "adding friends should be way easier. you shouldnt have to know someones specific handle"; "make it friendlier like other social media sites. generally, do that"; "im trying to bring people together not separate them". `people_suggestions()` in http_server.cpp: shared groups ("In Tower Garden with you"), friends in common BY NAME ("Friends with pal and Sam", only map-visible friendships), same town ("Also in Fresno"), same shared problem; only Typeahead-findable people; never you, friends, pending, blocked. Shown on /friends (12), under the map on `/` and on /home (3), Add friend on group member lists. Requests show a face + link; Decline added (`/friends/decline`, `FileStore::decline_friend_request`, silent). MapPrefs::friends now defaults ON; at start `map_friends_default_on()` writes the default + a one-time `map_friends_on` notice for everyone with no mapp record (live 2026-11-10: 12 of 13); signup and Google sign-in write the record (no note). No new record tags (rollback-safe). Committed tests edited with Brandon's OK because the default changed on purpose: city_store_test.cpp (2 asserts), map_graph.sh (3 checks + both-hide step), city_undo.sh ("3 to restore"). Tests: friend_suggestions.sh 28/28 (fails 19 on the previous build), map_graph 79, city_undo 52 by hand, full lgcheck green except those. Not yet: screenshots, "Not now" (part 2, a store change).

- **2026-11-10 PT -- Commit/push checks run only what a change could affect (`wip/20-affected-checks`).** Brandon: "the only thing that needs to be tested are things that may be affected by changes that were made" + "fix liquidgov pre-commit too". `scripts/affected_checks.py` maps each of the 19 checks in `commons-daemon/tests/` to the paths it reads (from each check's source) and picks from the staged files (pre-commit, `LG_CHECK_MODE=--staged`) or the pushed commits (pre-push stdin). Every check runs when `.githooks/` or the selector change, or the changed files can't be listed; an unmapped check always runs; any added/renamed file runs naming_check. A new check needs a CHECKS entry (`affected_checks_check.py` fails otherwise). Live: removing one file ran 1 of 19; this notes edit runs none. Caveat (side agent): a missing path in the map skips a check and nothing else runs these 19, so a push to master runs every check (Brandon: "only if necessary to avoid drift"; `affected_checks_master_check.py`).

- **2026-11-10 PT -- Deploy preflight retries once on a WSL accept4 drop (`wip/19-preflight-retry`).** Brandon: "yes add the preflight retry". The first polish1 `go` died at the read-only preflight ssh with `accept4 failed 110` (WSL vsock), which only the rsync step retried. The two template pairs make-go.py copies (`care`, `jbshell`) now retry the preflight once on that error (stderr captured into PRELOG); another error or a second drop still stops (`PREFLIGHT_FAILED`). Existing pairs (polish1 etc.) are unchanged; new pairs get it. Check: `commons-daemon/tests/preflight_retry_check.py` (fake remote, both pairs): 12/12, fails 4 on the old templates; wired into pre-push.

- **2026-11-10 19:24 PT -- Night river look, Google-down sign-in message (polish1) LIVE (commit `a07841f`, PID 785867, public checks FAILS 0).** Brandon's "go". Includes the tidy (wip/17) and the phone-check files. Checked live: lgmap.css serves the Night river frame, pages carry `--cardshadow`, `/login?err=google-down` shows "Couldn't reach Google...". Attempt 1 died at preflight on WSL `accept4 failed 110` before any change (the go script retries that only around rsync). Not yet checked: the map's smoothness on Brandon's iPhone.

- **2026-11-10 PT -- "Night river" site polish (`wip/18-polish`).** Brandon picked it from two directions ("i like night river", after "the mainstream looks like, lifeless"). Map (`assets/lgmap.css`): a night-blue frame in both themes that redefines the colour tokens inside `.lgmap-canvas`; boxes glow, connections gold, the ones you look at bright gold and flowing (desktop only: phones skip box glow and all animation); boxes 200x36 on a 270x56 grid so they no longer overlap. Site (`css()` in http_server.cpp): `--card`/`--cardshadow` tokens (white cards; warm charcoal in dark), bigger h1, header gradient, gold gradient on main buttons only (typeless form buttons, a.btn, active `.segtabs` tab, closed `details.newprop`), feed tabs are one `.segtabs` bar with `aria-current`; alerts banner (push.js) is a pill. Checked: lgcheck 20 suites, phone check 17/17, screenshots of 11 pages in light and dark. Not checked: smoothness on a real iPhone.

- **2026-11-09 10:05 PT -- Google sign-in says when Google can't be reached (site sweep item 18; `wip/16-curl-errors`; not live).** Brandon OK'd. `curl_get` now returns curl's exit status (it was thrown away, so a timeout looked like an empty answer); `/login/google` sends `err=google-down` ("Couldn't reach Google to check your sign-in. Try again in a moment.") when nothing came back, and keeps `err=google` for a bad or foreign token. The YouTube check is unchanged ("{}" lets a pick through on purpose). No extra log line: curl's own error already reaches the service log (-sS). Test `tests/e2e/curl_errors.sh` (fake curl, port 18101): 7/7, fails 2 on the previous build; lgcheck all suites green.

- **2026-11-09 10:00 PT -- Tidy (site sweep item 14): the 9 September root prompts moved to `docs/archive/` (`wip/17-tidy`).** Brandon OK'd. BUILD.txt, CLAUDE_CODE_CONTINUE.md, CLAUDE_P0/P0_PATCH/P1_PATCH/SPRITES_PATCH.md, CLAUDE_THE_COMMONS.md, GROK_BUILD_PROMPT.md, LIQUIDGOV_CONTEXT_HANDOFF.md (git mv; the existing docs/archive, not a new docs/history). Persistence/README.md updated; older bullets here keep the bare names as history (docs/archive/README.md says where they went). Item 15 (spent deploy pairs -> deploy/archive) NOT done: restart_bypass_check.py (frozen) writes each naming.toml restart_allowed path into a fixture with only deploy/, so an archive path crashes it; city-remote.sh also stays (deploy/city-status.sh runs it). Waiting on Brandon.

- **2026-11-09 09:35 PT -- make-go.py: the `RECORD ctx log:` target now fits ctx's 120-char limit (`wip/15-record-length`).** Brandon agreed. The sweep1 go script's line was refused (124 chars: the whole purpose went into the target), so the log was written by hand. The purpose is trimmed with "..." in the ctx target only (pair name kept); the notes.md bullet keeps it whole. Existing *-go.sh pairs are unchanged (regenerate to pick it up). Check `commons-daemon/tests/make_go_record_length_check.py` (pre-push): 155 chars on the old make-go.py, 120 now; ctx refuses only > 120 (ctx.py checks `len(v) > 120`).

- **2026-11-09 09:20 PT -- Phone browser check moved into the repo: `liquidgov-platform/tests/browser/` (`wip/14-phone-check`).** Brandon said yes (side-agent note: CI only sees HTML, so tap/hold/double-tap/screen-reader could break unseen). Run before and after any change to lgmap.js/lgmap.css or the map page, and on the old build (a new case must fail there): `python liquidgov-platform/tests/browser/run_phone_check.py '~/lg-sweep-check' [--assets <other checkout's liquidgov-platform>] [--label old]`. Windows Python + headless Edge; the WSL test server (`phone_serve.sh`, port 18140, outside the e2e range) is started, stopped by its PID file and the port proven closed by the runner (stopping servers by hand failed 3x this session); it refuses below 2.5 GB free RAM, a busy port, or a mangled Git Bash path; case 0 proves the server serves the very lgmap.js under test. 17 checks: current code 17/17; 464341e's page files fail exactly 4c (double tap) and 6a-6d (screen-reader buttons). Not in CI (needs Edge). `cdp.py` is the mobile sweep's DevTools helper, now in the repo.

- **2026-11-09 08:50 PT -- Site sweep batch 1: fixes, friendlier pages, chapter narration, map hold/double-tap/screen-reader open (sweep1) LIVE (commit `2dbe85d`, PID 776001, public checks FAILS 0).** First deploy under the no-rollback guard (LIQUIDGOV_LIVE_COMMIT=0e272f9; ROLLBACK_GUARD_OK, file_store.cpp unchanged). /opt/liquidgov/DEPLOYED_COMMIT now reads 2dbe85d / 0e272f9, so later deploys need no LIQUIDGOV_LIVE_COMMIT. Store backup `liquidgov_accounts.lgp.pre-sweep1-20261109T164644Z` (21047 bytes); store lines 318 before and after; NRestarts 0, journal clean. Rollback: restore /opt/liquidgov/liquidgov_http.pre-sweep1. Not yet tried on a real iPhone: double tap, VoiceOver "Open <name>".

- **2026-11-09 09:00 PT -- Map: screen-reader "Open <name>" buttons (`wip/13-sweep-fixes`; not live).** Brandon OK'd it: VoiceOver/TalkBack turn a double tap into a plain click (on a phone that only grows connections) and cannot press and hold, so they could not open a box. `lgmap.js` puts a `<g class="lgn-sr" role="button" aria-label="Open <name>">` right after each box with a page (1x1 transparent rect; `.lgn-sr{pointer-events:none}` so fingers and the mouse never hit it; no tabindex, since Enter on the box already opens for keyboards); its click opens the page; it moves with its box when a held box is raised. Browser check 16/16 (6a one per box with a page, 13 of 14 boxes - the mAInstream root has no page; 6b 0.75 px; 6c all 13 in the accessibility tree; 6d click opens the sheet), the a06eb6d build fails 6a-6d only. Not tried with a real VoiceOver.

- **2026-11-09 08:40 PT -- Map: double-tap opens a box on phones (`wip/13-sweep-fixes`; not live).** Brandon OK'd it (suggested alongside press-and-hold). `lgmap.js`: a second quick tap on the same box within 350 ms (DOUBLE_MS, by the pointerup events' own timestamps) opens its page; a single tap still only grows connections, and on phones the glide to the box waits 320 ms (GLIDE_WAIT_MS) so the second tap lands on the same box. `lgmap.css`: `touch-action:manipulation` on boxes (no double-tap zoom). Hint: "double-tap or hold it to open its page". Tests: lgcheck 20 suites green; browser check scratch/2026-11-09-claude-site-sweep/browser_check.py 11/11 (4c double tap 150 ms apart opens, 4d 600 ms apart does not), the 464341e build fails 4c only. The check stamps the taps' times (DevTools waits ~350 ms for the page between calls, which made a real double tap look slow); NOT yet tried on a real iPhone - try it after the deploy. The page is busy ~350 ms after each tap (headless Edge): worth a look at select()/expand().

- **2026-11-09 02:10 PT -- Site sweep batch 1 on `wip/13-sweep-fixes` (committed locally; not live).** Brandon asked for a full sweep (report scratch/2026-11-09-claude-site-sweep/report.md: 127 page loads x phone + desktop, 3 personas) and OK'd this batch. Fixes: `/logs` used `LIQUIDGOV_OPERATOR` (singular; live sets only `LIQUIDGOV_OPERATORS`) so it 404'd for everyone -> `is_operator()`; room at the page bottom for the Juketube button (it covered Deletes, "Add", the footer); `[id]{scroll-margin-top}` so jumped-to items are not under the sticky header; phone map first view keeps the connections column inside the frame (right-aligned at the 0.75 floor) and starts it near the top. Friendlier: Proposals list first, form behind "New proposal" (`<details id="new">`, open on ?title=, after an error, with none yet, or #new); initials instead of empty avatar circles (`portrait_placeholder`); a Settings section index (a div: the phone CSS hides every `<nav>`); plain words (Log in; Proposals intro links Phase 2 to /laws/13 and keeps "Discussion only. Nothing is sent to the city."; Departments search hint "City or name" and city names, not keys); footer Disclaimer size. Narration: `/assets/chapter<N>.mp3` for every Laws chapter (read from disk per request, byte ranges for iPhone, cached a day) and a "Listen to this chapter" button that says so when playback fails. Map on phones (Brandon): quick tap grows connections only; press and hold grows the box (on top of its neighbours) and opens the page; decided from the input events' timestamps; the hint says "hold it"; Enter opens for keyboards. Tests: `tests/e2e/sweep_fixes.sh` 44/44 (fails 32 on the old build), lgcheck 20 suites green; browser check scratch/.../browser_check.py 7/7 on the new build, the old build fails the map, tap, hold, index and page-end checks. Not yet: CI, deploy.
- **2026-11-09 01:10 PT -- No-rollback guard on `wip/11-rollback-guard` (committed locally; not pushed, not live).** Brandon chose a static check and a live-commit file on the server. `deploy/rollback_guard.py <live commit>` compares liquidgov_file_store's file_store.cpp at the live commit (what a rollback restores) with HEAD: every record type HEAD writes must be read by the live loader, and its first `f.size()` check must still accept the rows (exact checks unchanged; lower bounds may only rise). `deploy/sync-to-droplet.sh` runs it on every real sync after the CI gate, reading the live commit from `/opt/liquidgov/DEPLOYED_COMMIT` (or `LIQUIDGOV_LIVE_COMMIT`), refusing (exit 5) when unknown, and writes that file after the sync: this commit, then the previous one (after a failed build the running program is line 2; the guard checks both). Override `LIQUIDGOV_NO_ROLLBACK_OK="<reason + undo plan>"`, Brandon's OK per deploy; it also covers a missing Python/guard. Checked on real history: live 6ab8234 refuses exactly city-undo.py's 12 CITY_TAGS, live dacf8c3 (pre-Flow) exactly pain + mapp, live a8080ee passes. Tests `commons-daemon/tests/rollback_guard_check.py` 23/23 (Git Bash and WSL), 2/21 on master. One line added to the committed sync_gate_check.py (Brandon OK'd). FIRST deploy after merging: the server has no DEPLOYED_COMMIT yet, so run it with `LIQUIDGOV_LIVE_COMMIT=0e272f9` (live since 2026-11-08 23:13).
- **2026-11-09 00:35 PT -- The CI gate now also sits inside `deploy/sync-to-droplet.sh` (branch `wip/10-sync-gate`).** Brandon OK'd it. The 12 go scripts made before make-go.py added its gate call do not call the gate, but all 12 sync through sync-to-droplet.sh, so every REAL sync now runs `deploy/ci-gate.sh go` before rsync and exits 4 on a refusal ("Nothing was sent"). Dry runs are not gated. `LIQUIDGOV_DEPLOY_ALLOW_DIRTY` does not get past it; `LIQUIDGOV_CI_OVERRIDE="<reason>"` does (logged, Brandon's OK per deploy). Test `commons-daemon/tests/sync_gate_check.py` (pre-commit/pre-push): real sync script, fake gh and fake rsync; 7/7 here and in WSL, 3/7 on master (rsync ran with CI red).
- **2026-11-08 23:55 PT -- Platform CI + deploy CI gate on `wip/9-platform-ci` (pushed; not merged; nothing deployed).** Brandon OK'd both. `.github/workflows/platform.yml` builds liquidgov-platform and runs `scripts/lgcheck.sh` (same check as a local run) on master, `wip/**` and PRs touching liquidgov-platform/, lgcheck.sh or the workflow. Runs: first red at configure (libsodium-dev missing, fixed), then green (unit 8, e2e 19, under 2 min), deliberately broken nav link `d403618` red on exactly city_moderation's tab check, revert `f68e778` green. Gate: `deploy/ci-gate.sh`, called by every make-go go script before the backup, refuses `go` unless the newest platform run for the same liquidgov-platform TREE is green (tree, not commit: the deploy-pair commit never gets a run); `LIQUIDGOV_CI_OVERRIDE="<reason>"` for emergencies with Brandon's OK. New deploy order: push the wip branch -> CI green -> dry run -> Brandon's go (shared-context orders.md `liquidgov-deploy`). Tests: `commons-daemon/tests/ci_gate_check.py` 15 cases (pre-commit/pre-push). Not covered by CI: city_undo, browser tests, the new-suite-fails-on-old-build step.
- **2026-11-08 23:13 PT -- Juketube night-mode toggle keeps the music playing (jbtheme) LIVE (commit `0e272f9`, PID 769040, public checks FAILS 0).** Brandon said go. Assets only, no restart (PID_SAME); live juketube.js md5 = the commit's and has themeInPlace. Not yet checked on Brandon's iPhone. Same branch: `b752411` keeps deploy/webpush-requirements.txt out of the sync (cce9cad had moved it into liquidgov-platform/deploy/, which made make-go pick the restart pair).
- **2026-11-08 22:30 PT -- Juketube: night mode no longer stops the music (branch `wip/8-jb-theme`, committed locally; NOT live).** Brandon (iPhone): the shell "works a lot better, just stops when i switch to/from night mode". Cause: `/theme` is in the shell's FULL_LOAD list (`assets/juketube.js`), so the toggle did a full page load. Fix (assets only): while the shell is up or something is playing, `themeInPlace()` sets the same `lg_theme` cookie the server's `GET /theme/<mode>` sets, flips `data-theme` on the page and the framed page, and flips every toggle link; a `shell-theme` beacon goes to /log. Nothing playing: the link loads as before. Tested in headless Edge (scratch/2026-11-08-claude-jbtheme): `jb_theme.py` STOPS on master assets (run-old.out), 4/4 on the branch (run-new.out, beacons dark+light in the server log, screenshot checked); `jb_shell.py` regression 13/13 on the branch. Nested frames (the /map field) take the new theme on their next load. Deploy: assets-only pair, dry run then Brandon's go.
- **2026-11-08 14:57 PT -- nginx gzip for JS/CSS/JSON/SVG LIVE (server-only change, recorded here: `deploy/nginx-liquidgov-gzip.conf` = the live `/etc/nginx/conf.d/liquidgov-gzip.conf`, installed by `deploy/nginx-gzip-install.sh`; nginx -t then graceful reload, app PID 767615 not restarted; public checks FAILS 0).** Brandon OK'd the open item. nginx.conf had `gzip on` with the default types (HTML only). Measured: lgmap.js 17765 -> 6311 bytes, lgmap.css -> 1933; files under 512 bytes (favicon, manifest) stay plain. Rollback: `rm /etc/nginx/conf.d/liquidgov-gzip.conf && nginx -t && systemctl reload nginx`. Same day, deploy tools (908ea71): `make-go.py auto` picks the no-restart pair for assets/tests-only changes and refuses a mismatched pair; every generated `*-go.sh` takes `dry`; `sync-to-droplet.sh` uses `rsync --no-times` (unchanged files keep their server times, so the next build is incremental); `commons-daemon/tests/make_go_check.py` runs in pre-commit/pre-push.

- **2026-11-08 14:22 PT -- 988 note in framed pages LIVE (master `54fb372` = fix `7cee216` + deploy pair; `deploy/care-go.sh go` by Claude on Brandon's "commit and deploy when tests pass"; build + restart, new PID 767615, 0 restarts, journal clean, store 302 lines unchanged, public checks FAILS 0; backup binary `liquidgov_http.pre-care`).** Side-agent catch after the Juketube shell went live: `wrap()` computed and consumed the one-time crisis (988) note only on full pages, so the embed branch (Juketube shell frame, map field) skipped it and someone browsing with music on could go a long time without it. Now computed before the embed branch and shown in both; only visible frames use `wrap()` in embed mode (the /u/<name>/theme preview frame uses `http_raw`). Test: new `tests/e2e/care_notice_embed.sh` (port 18097) 12/12, fails 3 framed-page checks on the previous build; unit 8/8, e2e 18/18 (city_undo skipped: no LGP1 change).
- **2026-11-08 14:06 PT -- Juketube persistent shell LIVE (master `821d2c3`, pushed; assets only: `deploy/jbshell-go.sh go` by Claude on Brandon's "Go, no restart": sync + chown, NO build or restart, PID 765983 unchanged, live juketube.js/.css byte-equal to the commit, public checks FAILS 0; backup binary `liquidgov_http.pre-jbshell`). Not yet checked: a logged-in visit on live, and Brandon's iPhone.** Brandon: "do the juketube persistent shell", approach "whatever makes the most logical sense". How: while the player is playing, a site link keeps the page (header + Juketube) and loads the next page in a full-screen same-origin frame under the header; framed pages use the existing embed mode (`g_embed`, Sec-Fetch-Dest: iframe), URL and title follow by replaceState, Back/Forward work and Back returns to the starting page. Full loads (music stops): `/login` `/logout` `/signup` `/forgot` `/reset` `/theme` `/listen` `/world` `/install` `/apps` and other sites, whole path segments only; and every page while nothing plays. Any shell failure falls back to a normal load and sends `/log` beacons `shell-exit` / `shell-error`. Rejected: content swap by script (~378 top-level let/const in inline page scripts would collide; the old `softNavigate` in jukebox_script only swapped `<main>`), reload-and-resume (gap per page, iPhone tap). Known gap: the 988 care notice waits for the next full page load while in the shell. Test: scratch/2026-11-08-claude-jbshell/jb_shell.py (headless Edge, reads the player's moving position) 14/14, and the previous code fails it at the first click. Assets-only deploys need no restart: the server reads assets from /opt/liquidgov per request and re-hashes URLs on mtime change.
- **2026-11-08 13:40 PT -- Juketube fix LIVE (`wip/7-juketube-fix` 39749f1 = master 7a27450 + deploy pair; `deploy/juketube-go.sh go` by Claude on Brandon's "go" + guard-ok; PID 765983, 0 restarts, public checks FAILS 0, live juketube.js identical to 7a27450; backup binary `liquidgov_http.pre-juketube`). Not yet checked live: the oEmbed answer for a logged-in user, and close/open on Brandon's iPhone.**
- **2026-11-08 ~05:30 PT -- Juketube fix on `wip/7-juketube-fix` (committed locally; tested 12:56 PT).** Tested on the branch: unit 8/8, e2e 17/17 (city_undo skipped: no LGP1 change), juketube_skip 11/11 and 12 FAILs on the live build (d1bcf30); browser old vs new both play in ~3 s, keep the player across close/open and resume after navigation; only the new build reports `embeddable`. Note for test runners: the fake curl in juketube_skip.sh must answer every oEmbed id (an unmatched id fell through to a real "missing" answer and failed the network-down case). Brandon: "the Juketube still isnt working", then "do a and b", then "if it wont play for at least 25% or more people trying to use juketube. also it should keep playing when the widget is closed and there should be continuity between pages". Repro (scratch/2026-11-08-claude-juketube): YouTube refused the embed (150 locally, 2 live), juketube.js then sent a bare `{"ended":true}`, and the server ended the track for the whole station (any listener's "ended" was trusted).
  - Server (`run_community_session`): "ended" must name the playing video (`{"ended":"<id>"}`); a stale id is ignored; when others are listening, a named ended counts only after 20 s of the track; a bare `{"ended":true}` (old cached pages) counts only from a listener who is alone. A player error is now `{"unplayable":"<id>"}`, one vote per connection per track; the track is skipped when votes >= 25% of connected listeners (or the reporter is alone). Log lines `[jukebox] unplayable <station> <id> n/m`, `ended-stale`, `ended-ignored`. The inline /listen player names its track too.
  - `/jukebox/oembed` now says `{"embeddable":false}` when YouTube's oEmbed answers 401 (owner blocks other sites) and adds `"missing":true` for 404/400; a normal answer gains `"embeddable":true`; unknown stays `{}`. juketube.js checks it before telling the station about a pick or queue (the local load still starts inside the tap, for iPhone) and says "That video's owner lets it play only on YouTube" instead. Not every block shows in oEmbed (domain-only limits answer 200), so the vote above still matters.
  - Closing (and opening) the widget no longer stops the player: the wrapper moves to `#juketube-root` once before any player exists and is only positioned afterwards (`jb-docked` over the panel slot while open, `jb-parked` off-screen while closed). Proof (scratch/2026-11-08-claude-juketube/jb_close.py, which samples the page's own player `getCurrentTime()` seconds apart after a control that it advances 4.0 s per 4 s): on the live code, OPENING the widget reloads the iframe (1 load event) and the player sits cued (state 5, position frozen) through close and reopen; with this change it advances 4.0/4 s open, 5.0/5 s closed, 4.0/4 s reopened, 0 reloads. (b5bfb58 had reverted this on a browser check that read a stale state value and a load counter that never fired; a side agent caught it, and this re-test restored it.) Not covered by the e2e suite: it needs a real browser and YouTube.
  - The idle "nothing playing" path no longer calls stopVideo every 1.5 s.
  - Test: new `tests/e2e/juketube_skip.sh` (port 18096) + `juketube_skip_client.py` (five websocket listeners: 1 of 5 does not skip, 2 of 5 do; bare/stale/early ended ignored; alone still ends; oEmbed answers via a fake curl on PATH; static checks on the served juketube.js).
  - Not done: continuity between pages. Every page load restarts juketube.js, which resumes the station at its position after a gap, and iPhone may need a tap to start sound again. Real continuity needs the page to stay alive (site navigation inside a persistent shell); proposed, not built.

- **2026-11-08 ~01:23 PT -- Comment moderation + older-post search LIVE (master `49b52a4`, deployed by Brandon via `deploy/comments-go.sh go`; PID 761462, 0 restarts, store 284 lines unchanged, public checks 0 FAILS; backups `liquidgov_http.pre-comments`, store `.pre-comments-20261108T091819Z`). Tested before merge: unit 8/8, e2e 16/16 (city_undo skipped: no LGP1 change), comment_moderation 24/24 and it fails on the previous binary.** Note for test runners: map_graph, city_flow/look/match and problems_own_writing start the server in `$(dirname $0)/../..`, so a CR-stripped copy must keep that layout with `assets/` reachable. Brandon: "and what about posts and groups and anything else that might need deleting?", then "see if you can get more done". New operator tab `/admin/comments`: the 100 newest comments on posts and on proposals together (newest first), a search box (author or text, case-insensitive, up to 300 shown), and a Delete per comment that removes only that comment. Each delete is first appended to `<store>.moderation.jsonl` (`comment_on`, `comment_id`, `parent_id`, author, ts, text, `deleted_by`) and is refused when that file can't be written, as for proposals; the audit log gets `delete_comment`. `/admin/posts?q=` now searches every post (author or text, newest first, capped at 500) so posts older than the newest 80 can be reached. Store: `all_posts`, `recent_post_replies`, `recent_proposal_replies`, `*_reply_by_id`, `delete_post_reply`, `delete_proposal_reply` (no new LGP1 tags). Test: new `tests/e2e/comment_moderation.sh`, port 18099. Still open: deleting city departments/councils.

- **2026-11-08 -- Operator moderation of proposals on `wip/4-proposal-moderation` (committed locally; not live).** Brandon: "make it so i can delete and edit proposals for now? people are abusing it". Operator page `/admin/proposals` (new Proposals tab): every proposal newest first, Edit (title + text) and Delete. Delete first appends the proposal with its comments and reaction counts to `<store>.moderation.jsonl` (named to match the deploy sync's `*.lgp.*` exclusion, so no deploy touches it), then removes it with its comments, reactions, votes and ballot answers; if the archive can't be written the delete is refused (side-agent catch: keep evidence, allow a hand restore). Both actions go in the audit log. Store: `FileStore::delete_proposal`, `edit_proposal`. Test: new `tests/e2e/proposal_moderation.sh` (port 18091), 20/20 on the branch, 14 fails on the live build; city_slice3/5, problems_own_writing, map_graph unchanged. Operators are `LIQUIDGOV_OPERATORS`.

- **2026-11-07 ~19:15 PT -- mAInstream redesign on `wip/3-mainstream-map` (committed locally; not merged, not pushed, not live).** Brandon: "pages should be accessed through the map", the name is mAInstream, land on the map "with your most relevant boxes enlarged and in scope ... color and shape coding", and on phones a full-screen sheet over the map.
  - `/` for a signed-in person is the map (title mAInstream; the intro-replay setting still works); the old dashboard is `/home`, linked under the map. `/map` is unchanged. `map_page_body()` builds both.
  - Shapes by kind (people pill, groups square, cities oval, proposals hexagon, offices octagon, problems tag, posts note) with a key under the map; size 0.85x-1.25x from relevance; lines meet each box's real edge. The start opens the mAInstream plus your most relevant box and fits it in view (phone: that column at the left edge, scale >= 0.75).
  - Phone (<= 760 px): a tapped box's page is a full-screen sheet ("< mAInstream" bar back to the same spot, page behind locked); `/map` links inside the framed page go to the real map, not a map in the map. The phone map frame is `flex:none` (with flex:1 the svg fell back to 150 px).
  - Rename: MAINstream / Main Stream -> mAInstream everywhere; Brandon OK'd editing city_map.sh and map_graph.sh for the wording only.
  - Tests: unit 8/8, all e2e suites pass (city_map 39, map_graph 78, ...), verify_map.py 24/24 on the new build and fails on the live build (no map on /), screenshots checked. Not yet: the search engine (Brandon: the map itself is the search engine; websites become boxes), a CI job for liquidgov-platform, the reusable deploy script with the no-rollback guard.
- **2026-11-07 ~14:30 PT -- iPhone fixes on `wip/2-iphone-fixes` (live since 18:17 PT as `ff7e822`; merged and pushed as `4e43449`).** Brandon: "glitching with the UI on my iphone"; Claude swept 58 page loads in iPhone-14 emulation (scratch/2026-11-07-claude-mobile-sweep/report.md); Grok and Grok Bot were out of tokens, so a fresh read-only Claude advisor reviewed the plan (scratch/2026-11-07-claude-mobile-fixes-review/claude-answer.txt) and Brandon chose that.
  - `setup.js` no longer scans every element once a second (it hid the site's own nags and any post saying "Alerts on"); the nags are gone at the source: `ios_install_hint()` removed from http_server.cpp, push.js shows no "Alerts on" and no "Alerts need Safari" strip. One prompt at a time: an iPhone Safari tab gets only "Add to Home Screen to get alerts"; a browser that can do alerts gets only "Turn on alerts". No prompts inside framed pages (`window.top !== window`).
  - Juketube tip: a real close `<button>`, hides after 8 s, at most 3 page views, never in a frame; its localStorage calls are guarded.
  - 16px inputs where iOS zoomed on focus (/me Profile CSS textarea, Juketube search/select). `/listen`: every player call waits for `playerReady`. The Genesis listen button's pulse ring is smaller (scale 1.35, inset -4px): the old one pushed the logged-out home page 3-8 px wider than a phone, so it wiggled sideways.
  - Assets: `asset_url()` versions every script/stylesheet link with a hash of the file (redone when the file's mtime changes, so a hand-copied file still gets a new URL); a request with `?v=` gets `Cache-Control: public, max-age=31536000, immutable`, a bare one keeps `no-store`. gzip for .js/.css is an nginx line (`gzip_types`), a server change that needs Brandon's OK.
  - Results (fresh browser per run): covered controls 15 -> 2 (both by design: the intro video, the tip's first 8 s), sideways overflow 1 -> 0, iOS-zoom fields 2 -> 0, script errors 1 -> 0; verify_fixes.py 17/17. Open for Brandon: the map on phones (in-frame page vs a normal page, and "MAINstream" vs "Main Stream"), Google sign-in client ID (Google Cloud console). Grok's half-done error sweep (19 files, `liquidgov-us-grok-errors`) applies cleanly on master; merge with this branch when it resumes.
- **2026-11-07 -- Restart bypass checks (Brandon approved).** Literal sudo/env, control-flow, ssh payloads, service suffixes, restart variants, multiple units and stop/start pairs are checked in a new regression file. CI discovers its wrapper and commit/push hooks run it. The three allowed deploy scripts stay byte-identical. This remains a static repository-only scan: scratch files, variables, aliases, generated commands and arbitrary shell syntax are not a containment boundary. Grok approved the widened check. Brandon allowed a local commit after an isolated original-commit run reproduced the same 17 schema.sql-symlink startup failures; only the new passing restart regression differs. The Windows-checkout schema issue remains open, not repaired here. No live changes or push.

- **2026-11-07 -- Naming check watches the real files.** The practice test now includes `city_store_test.cpp`, so a too-strict rule fails. A shell script that restarts liquidgov fails unless it is `deploy/city-remote.sh`, `deploy/flow-remote.sh`, or `deploy/match-remote.sh`. Those three scripts were not changed. Nothing was deployed.
- **2026-11-07 06:12 PT -- Look at what I've written is live.** Brandon said go after the dry run. Deployed commit `8b621fa`. The service came up as PID 752189 with 0 restarts. Store backup is `/opt/liquidgov/liquidgov_accounts.lgp.pre-look-20261107T140758Z` (17,412 bytes). The previous binary is `/opt/liquidgov/liquidgov_http.pre-look` (2,281,120 bytes, the match binary). pre-city, pre-flow, and pre-match were left in place. Signed-out checks were 17 with 0 failures. The Look button is in the live source and in the new binary. City tags and pain tags were still 0, so the pre-match binary can still read the store until someone Flows. The button reads your own friends-only posts, group posts, and messages you sent onto your own map. Other people do not see that layer, and those boxes cannot be Flowed.
- **2026-11-07 -- Test names, ports, and the retired checks.** Brandon handed this to Grok. `problems.sh`, `pwa_check.py`, and `test_pwa.sh` are retired. The Commons icons and the old Mail/Social/Civic manifests that still answer live in `pwa_files_check.py`. `city_flow.sh` uses port 18093, `city_match.sh` 18094, `city_look.sh` 18095. New files follow `naming.toml`, checked by `naming_check.py` at commit, push, and in `drift_audit.py`. Nothing here was deployed.
- **2026-11-07 03:10 PT -- Merged and pushed; only committed code goes live from now on.** Claude committed what liquidgov.us runs (Flow and matching had been deployed from uncommitted files) together with the not-yet-live Look slice as `a8080ee` on `city-slice1`, merged it into master as `55264f6`, and pushed both (Brandon: commit, push, merge, push). The pwa_check fix makes GitHub's master CI expect the closed Commons and one app.
  - `deploy/sync-to-droplet.sh` now refuses a real sync from an uncommitted tree and prints the commit it deploys. Dry runs still work; `LIQUIDGOV_DEPLOY_ALLOW_DIRTY="<reason>"` overrides.
  - The mapp and pain loaders keep a newer version's extra fields and write them back.
  - Rollback: the server's pre-flow binary cannot read a Flowed pain row, so the full `city-undo.py` is the only safe rollback until every live binary keeps extra fields. Nobody uses the site yet (Brandon).
- **2026-11-07 -- Your own private writing can sit on your map.** Brandon: friends-only posts, group posts, and messages you sent are read onto your own map, next to public data. Other people do not see that layer. The other person's messages are not read. Those boxes cannot be Flowed, so they do not match anyone. He can change the Flow rule. Office races, reposts, and comments on ordinary posts stay unread. Local only, not on liquidgov.us. Button is "Look at what I've written." Uncommitted on `ca3be30`. `city_look.sh` 37 pass on port 18088. `problems.sh` 40 pass on port 18091. File-store test covers `fpost`, `gpost`, `mail`, and `gmsg` surviving flush. No browser pass.
- **2026-11-07 02:21 PT -- Look at my public posts is local only.** Not on liquidgov.us. On `/problems`, when the notice switch is on, the button reads that person's own already-public words: Anybody posts (not reposts), proposals they wrote (not an office race), and proposal comments they wrote. Each find is an unkept suggestion with Keep or Forget. This pass did not read friends-only posts, group posts, or messages. The newer bullet above does, onto your own map only. Comments on posts and other people's words stay unread. The switch off reads nothing. A scan with nothing new says "Nothing new in your public posts." Crisis words store nothing and show 988, including when the same post is read again. `city_look.sh` 31 pass on port 18088. `problems.sh` 40 pass on port 18091. Uncommitted on `ca3be30`. DMs stay out until real age checks exist.
- **2026-11-07 02:01 PT -- Matching is live on liquidgov.us.** Brandon said go after seeing the local result. The service came up as PID 749790 with 0 restarts. Store backup is `/opt/liquidgov/liquidgov_accounts.lgp.pre-match-20261107T095735Z` (17,412 bytes). The previous binary is `/opt/liquidgov/liquidgov_http.pre-match` (the Flow binary). Pre-flow and pre-city were left in place. Pain records were still 0. Signed-out checks were 17 with 0 failures, including `/pain/1` to login. The subject is the phrase. A person sees others only after they Flow their own box, and only when the map already lets them. No count. No new store tag. Uncommitted on `ca3be30`.
- **2026-11-07 01:33 PT -- Flow is live on liquidgov.us.** Brandon said go. The working tree on `city-slice1` (on top of `ca3be30`, not a new commit) was synced with the deploy script. Store backup is `/opt/liquidgov/liquidgov_accounts.lgp.pre-flow-20261107T092943Z` (17,412 bytes). The previous binary is `/opt/liquidgov/liquidgov_http.pre-flow`. The pre-city binary was left in place. The service came up as PID 749139 with 0 restarts. Pain records were still 0. Signed-out `/pain/1` redirects to login. A kept box stays private until its owner Flows it. Still removes the 9th store field. No match list, no count, no DMs. The pre-Flow binary refuses a 9-field pain line, so a rollback copies `liquidgov_http.pre-flow` back, and if anyone has Flowed, pain rows are stripped first.
- **2026-11-07 00:43 PT -- City features are live on liquidgov.us.** Brandon said go. Deployed from `city-slice1` `ca3be30` with the sync script. Store backup is `/opt/liquidgov/liquidgov_accounts.lgp.pre-city-20261107T083911Z` (17,412 bytes). The previous binary is `/opt/liquidgov/liquidgov_http.pre-city`. The undo tool is `/opt/liquidgov-tools/city-undo.py`. The service came up as PID 747871 with 0 restarts. City record counts stayed 0. Signed-out map, problems, and city pages redirect to login. The map assets, install page, and manifests return 200. Commons stays closed for GET and POST.
- **2026-11-06 23:50 -- City features committed locally, deploy handed to Grok, not deployed.**
  - Branch `city-slice1`, worktree `C:\Users\Brandon\liquidgov-us-city`: `dacf8c3` (all the work and tests) and `b7169c7` (deploy plan, `deploy/city-deploy-plan.md`).
  - Brandon: "hand it to grok". The handoff is in Grok's team inbox. It is not a go: Grok waits for Brandon's own go, twice (after the preflight and dry run, then before the first live change).
  - Not pushed. GitHub needs a separate OK: push master's 2 jukebox commits first, then merge `city-slice1`.
- **2026-11-06 23:55 -- Map polish, one app, alerts strip: built and local-tested, not deployed (Brandon's requests).**
  - **Relevance shading** (green = yours, yellow = near you, red = far): `/map/node` JSON has `rel` 0-1 from `map_ctx`/`map_relevance`, scored from the viewer's own groups, cities, friends, involved proposals (wrote/voted/reacted/commented) and kept-problem words. Only that viewer sees it.
  - **Lines thicken with use**: per line, counted when you move along it, in the browser's localStorage only (never sent to the server), with a "Forget my visits" button.
  - **Faint lines** to boxes you never visited and that aren't yours.
  - **One app**: every page offers /manifest.webmanifest, /apple-touch-icon.png and the name "LiquidGov"; /install offers one app; /app/mail, /app/social and /app/civic redirect to /mail, /feed and /proposals; the Messages install nudge is gone; app-*.html deleted. The old manifest-*/icon-* routes are kept (icons already on phones; deploy/nginx-www-apex.sh checks manifest-social).
  - **Alerts prompt** (assets/push.js, now ?v=4): an in-page strip at the top of main with a close button (hidden for 7 days); status notes fade after 5 s.
  - Tests: map_graph.sh 78, browser click-through 26; all suites pass on a clean build.
- **2026-11-06 23:30 -- Problems and solutions, first slice: built and local-tested, not deployed (Brandon: "BUILD the pain-point slice"; name per Brandon: "problems and solutions").** It follows Grok's review (scratch/2026-11-06-claude-painpoints-2/grok-answer.txt).
  - What it reads: only a person's NEW public words (Anybody posts, proposals, proposal comments), and only while Settings > "Notice my problems and wants" is on (default on). Never DMs, friends-only or group-only posts, or old writing.
  - Detection: plain word patterns (`find_pain_points` in file_store.cpp): I wish / if only / it would be great if (= Want), and I hate when / I hate that / I'm tired of (= Problem), each also with a softener ("I really wish", "I'm so tired of"). It skips quotes, '>' lines and "I wish you…". "Someone should" and "why can't" are deliberately left out.
  - Crisis words store nothing; the writer's next page shows 988 once, in memory only, and nobody is told.
  - Each find is a suggestion to Keep or Forget; unkept ones expire after 30 days. Only the phrase and the source id are stored (tag `pain`, plus `painn` for the next id). Kept ones show on /problems with possible solutions (departments, proposals and groups whose words overlap, in cities the person is in, plus a prefilled "Start a proposal"), and as a private box on the owner's own map. Nobody else can open them.
  - Deleting the source, Forget, the off switch, and deleting the account each remove the rows.
  - city-undo.py strips pain/painn. It also has a new `--restore-privacy <backup> <store>`, to run after an undo and before a reinstall, so people's map privacy choices come back (side-agent catch).
  - Tests: problems.sh 40, unit tests for detection and store, city_undo.sh 52; all suites pass on a clean build.
  - Next slices (Grok): Flow a box public, then match people who Flowed the same subject, then opt-in DMs only after real age checks exist.
- **2026-11-06 23:05 -- Slice 6b, interactive map + map privacy: built and local-tested, not deployed.** Asked for by Brandon: connect as many things as possible; privacy options in Settings; clicking a box grows its connections out of it and shows its real page in a field on the right, which can fill the window; the MAINstream button shrinks it back. Friend lines (his choice): you always see your own, and others see one only when both friends turn on "Show my friends". Everything else that is already public shows by default, per his standing "network, not isolate" rule.
  - Code: `/map` (`assets/lgmap.js` + `lgmap.css`, served like juketube; the deploy must ship them), `/map/node?n=<id>` (JSON; box ids root, sitewide, city:, group:, user:, dept:<key>|<tag>, prop:, post:), `/proposals?id=N`, and the view-only `/post/N`.
  - Settings has a new "Map and connections" section (`/settings/map`): show me, my groups, my posts, what I commented on, my friends. It is stored in new tag `mapp`, which city-undo.py now strips too.
  - Privacy rules live in `map_person_ok` / `map_post_ok`: blocks either way, "show me", profile visibility, post audience; the static city map uses them too.
  - Framed loads (Sec-Fetch-Dest: iframe) drop the site header/footer/jukebox and send `frame-ancestors 'self'` and `X-Frame-Options: SAMEORIGIN`, so other sites can't clickjack framed pages. Normal page loads are unchanged; browsers that don't send Sec-Fetch-Dest get no change either way.
  - Tests: `map_graph.sh` 63; headless-Edge click-through 19 (scratch `browser_check.py`, needs the demo on 18090); all earlier suites pass; undo 41.
- **2026-11-06 22:22 -- Slice 6, map view: built and local-tested, not deployed (Brandon: "BUILD the map view").** Purpose, in Brandon's words: "tie as many things into one coherent collection of information and media as possible, and make it intuitive to help people communicate with the people they would optimally be communicating with." Pages: `/map` is the Main Stream, with each city's Current branching off it showing up to 3 groups and 3 proposals, plus a Sitewide branch. `/city/<key>/map` shows five columns: People here, Groups, City, Departments and councils, Proposals. Hovering a box lights up its lines. Lines are in-city, member-of, touches, wrote and commented. "Reacted" is deliberately not drawn: proposal pages show reactions only as counts, and the map must not reveal who reacted. Only opted-in people appear, blocks are respected, and there are no people at the Main Stream level. It is server-drawn SVG plus a 10-line hover script; no framework and no new store tags. Test `tests/e2e/city_map.sh` passes 39, and all other suites still pass on a clean build.
- **2026-11-06 21:55 -- City slices 2-5 (Grok) verified by Claude, not committed, not deployed.** Clean build with 0 warnings. 7 unit programs pass, and e2e passes 38/22/18/18/25 plus Claude's 39. Open, waiting on Brandon: flush() and load() drop a proposal's city and the city's department/council tags whenever no group names that city (last group deleted, or its owner re-spells the city), and that loss is permanent. Claude recommends keeping them. **Resolved 22:00, Brandon: "keep it".** They are now kept: load and flush no longer erase them, and a city page with no groups but kept proposals returns 200 and links to them. New e2e `tests/e2e/city_keep.sh` (17) passes, and all the others still pass. Still open before any deploy: an undo that doesn't rewind the whole site (see the rollback note above). **Built 22:10 (Brandon: "build the undo"):** `deploy/city-undo.py <store>` does a dry run and `--go` applies it, with the site stopped. It removes only the 9 city record types (ccity clist pcity pwho ctag ptag prply prpln prx) and keeps every other record byte for byte. It first writes a private backup `<store>.pre-city-undo-<UTC>`, swaps the file in atomically, keeps the store's mode and owner, verifies the result, refuses a non-LGP1 file, and stops if the store changes mid-run; running it twice removes nothing more. Test `tests/e2e/city_undo.sh` (40 pass) uses the old binary built from an exact export of master 6ab8234. In that test the old binary refuses the city store, then after the undo opens it with posts, mail, group posts, membership, friends, votes and both proposals intact, and the new binary opens the undone store too. Rollback is then: stop the site, run city-undo, put the old binary back, start it, read the journal.
- **2026-11-06 -- City slice 1 built, local-tested, not deployed (Brandon: "BUILD slice 1").** Branch `city-slice1`, worktree `liquidgov-us-city`, uncommitted. `Community.city` stored in new LGP1 tag `ccity <slug> <city>` (order-free on load; orphans dropped; malformed fails closed). Keys: `city_key()` lowercases and joins words with `-` ("Fresno, CA" -> `fresno-ca`). New: optional City field with typeahead on Start a group (and `?city=` prefill), owner-only Add/Change city on `/c/<slug>`, Cities card on Groups, `/city/<key>` listing that city's groups ("a label, not a government"). Rollback note: an older binary refuses a store holding `ccity` rows (unknown tag), so roll back with the pre-deploy store backup. Grok also built slice 2 (opt-in "People here") in the same worktree during Claude's pause; it is not OK'd by Brandon yet. Tests: 7 unit programs, e2e `tests/e2e/city_slice1.sh` 38, `city_slice2.sh` 22, Claude's own 39, all pass on a clean build.
- **2026-11-06 -- City proposals: decided, planned, not built.** Brandon, planning on his Switchyard map. Purpose: foster connections between people, starting with people in the same city. Proposals start at city level: citizen proposals in any city where people start a community. Anyone can start one, with no minimum, and a city can have several communities. Accounts stay unverified for now; a semi-official Proposals transition announced later will require verification (CitizenId opt-in, untouched). Each city's community adds its own department and council tags. Discussion only; nothing is sent to any city. Roles: author, commenter, sharer, reactor. Public items only, except to their owner. A city is a label on platform groups, not an AVND locality, and has no standing. Plan: `CITY_PROPOSALS_BUILD_PLAN.md`, five slices: city on groups plus a city page, opt-in "people here", proposals get a city, department/council tags, proposal comments and reactions. Waiting on Brandon's BUILD for slice 1.

- **2026-10-24 -- Google sign-in client was deleted.** Live button posted to Google and returned `Error 401: deleted_client` for `508630567243-f0s5gh21csd5evp1ls48j289pg14r0eb`. Brandon created a new Web client in a different project: `738917322917-p9hvl2s82l8esj70bc9s58dbv4viraqv.apps.googleusercontent.com`, origin `https://liquidgov.us`, redirect `https://liquidgov.us/login/google`. Not an AI-agent client. Grok cannot SSH. Job for Claude is `CLAUDE_GOOGLE_LOGIN.md`: set `LIQUIDGOV_GOOGLE_CLIENT_ID` in `/opt/liquidgov/.env.local` and `systemctl restart liquidgov` only. No rebuild, no rsync.

- **2026-10-04 -- Shops, short, with roofs.** The Lab, the Workshop, and the Hall are three tiles tall instead of blocks. Five stalls (Grove, Meadow, Hedge, Point, Dunes) sit at the end of paths that used to stop on grass. Each room has a window, a shelf, a counter, and a table, and you can look at what is on them. Nothing is for sale. Not deployed.

- **2026-10-04 -- The map is one column wider.** 72 tiles to 96. The new land, east of the Orchard, is the Grove, the Meadow, the Hedge, and the Point. The orchard road and the garden street both run into it. The sea still ends the south, and it continues under the new column. Not deployed.

- **2026-10-04 -- The south edge of the map is water.** Ten maps (A Link to the Past, Link's Awakening, Stardew Valley, Pokémon Red, EarthBound, Super Mario World, Hollow Knight, Animal Crossing, Undertale, Breath of the Wild) all share a walkable world that stops at a real edge. The Commons already had a path to every door and a junction at the Square. What it did not have was an edge: south of Creek and Shore was a blank grass field with no name. That field is the sea. Not deployed.

- **2026-10-02 -- fb:app_id is live.** Brandon weighed it (share-analytics attribution in Meta's Insights
  vs. accepting the Meta Platform Terms for a bare app) and said do it once he understood what it actually
  bought him. Registered his account as a Meta Developer (he accepted the terms himself), created app
  "LiquidGov.US" (no use case, no business portfolio -- just an app ID, nothing else requested or granted),
  app id `844023505436341`. Set `LIQUIDGOV_FACEBOOK_APP_ID` in the droplet's `/opt/liquidgov/.env.local`
  (same file `LIQUIDGOV_GOOGLE_CLIENT_ID`/`YOUTUBE_API_KEY` already live in, never synced from the repo) and
  restarted: `NRestarts=0`, no journal errors. Confirmed live by content -- served `/` carries
  `<meta property="fb:app_id" content="844023505436341">`. Facebook's Sharing Debugger hadn't picked it up
  as valid yet at re-scrape time (brand new app, likely propagation lag on their side, not a config issue
  here) -- our own served HTML is already correct regardless.

- **2026-10-02 -- Share card v4: the exact line, more inviting, and "Genesis" gone from the title,
  DEPLOYED (`ecfcb0c`).** Brandon's own corrections to v3: the card should say "This is YOUR government!" /
  "REPRESENT YOURSELF" exactly (not v3's softened wording); make it more inviting (added a warm gradient,
  a gold certificate-style border, and an "Organize. Collaborate. Build what comes next." line); and the
  share preview's own `<title>` still read "Genesis — LiquidGov.US" (`wrap("Genesis", ...)` on the
  logged-out `/` route) -- changed to "Home", matching the signed-in dashboard's own name for the same
  page. New image, `assets/og-image-v4.jpg`, generated by a new committed script
  (`liquidgov-platform/tools/make_og_card.ps1`, Windows PowerShell + System.Drawing -- no Pillow/
  ImageMagick/pip anywhere in this dev environment, .NET's own image API is what's actually there) rather
  than hand-edited. See `og-card-v4-built.md`. Platform test suite unaffected, all green. Civic deployed
  first (chown before build, `NRestarts=0`, no journal errors), then `deploy-daemon.sh --go --with-client
  --civic-done` (37/37 verify checks). Confirmed live by content on `/` and `/commons/`, and Facebook's
  Sharing Debugger re-scraped: `og:title` "Home — LiquidGov.US", `og:image` v4, `og:url` now recognized
  (only `fb:app_id` still flagged, waiting on Brandon's App ID).

- **2026-10-02 -- og:url added, DEPLOYED (`5d922e0`); fb:app_id wired but not live.** Facebook's Sharing
  Debugger flagged both as missing after the share-card scrape. `og:url` is a flat site-wide value
  (`https://liquidgov.us/`, `https://liquidgov.us/commons/` for the client) -- same posture as og_desc/
  og_image, since `page_wrap` has no per-page request path in scope. `fb:app_id` needs a real Facebook
  App ID, which doesn't exist anywhere in this repo -- wired through `LIQUIDGOV_FACEBOOK_APP_ID` the same
  way the Google client ID already is (tag absent until configured, never a made-up id). Waiting on
  Brandon for a real App ID before that env var gets set on the droplet. Civic deployed first (chown before
  build, `NRestarts=0`, no journal errors), then `deploy-daemon.sh --go --with-client --civic-done`
  (37/37 verify checks). Confirmed live: `og:url` on both `/` and `/commons/`.

- **2026-10-02 -- Share card hook, DEPLOYED (`7f66fe9`).** The unfurl no longer tells the Myspace joke. The card is `assets/og-image-v3.jpg`: "This is YOUR government." / "You represent yourself." The five stars are gone; that conceit was a review gag and could not carry the line. The description adds the limit the landing page already states: a voluntary association, not a replacement for the government you live under. The joke stays on the genesis quote card. v2 stays served so old cached shares still resolve. See `og-card-v3-built.md`. Civic deployed first (sync, chown before build, build, restart: `NRestarts=0`, no journal errors), then `deploy-daemon.sh --go --with-client --civic-done` (37/37 verify checks). Confirmed live across all four ways the URL gets shared -- `https://liquidgov.us`, `https://www.liquidgov.us`, bare `www.liquidgov.us`, bare `liquidgov.us` -- every one resolves (via nginx's existing www/http redirect, unrelated to this change) to the apex serving `og-image-v3.jpg` and the new description; `/commons/` carries the same tags too.

- **2026-10-02 -- Two juketube bugs Brandon hit, both real, both fixed, DEPLOYED (`067fcff`).**
  (1) "I shouldn't be able to skip videos on public juketube channels": `run_community_session`'s WS `"play"`
  handler (`http_server.cpp`) overwrote the whole station's now-playing unconditionally -- any connected
  member clicking any search result instantly hijacked what everyone else on that public/community station
  was hearing, with none of the `isShared()` posture the Skip button already enforced client-side (and no
  server-side check either). Fixed by making `"play"` do exactly what `"add"` already does: start now only
  if the station is idle, otherwise queue. (2) "i dont see an accurate history": nothing ever recorded a
  shared station's plays anywhere -- `/jukebox/history` only ever understood `?for=<personal-owner>`, but
  both jukebox UIs (`assets/juketube.js`'s full page, and the separate inline `jukebox_script()` the
  `/listen` standalone page still uses) sent `?station=<id>` for a non-personal station, which the route
  silently ignored and fell back to the *viewer's own* personal history instead -- inaccurate, not empty.
  Added an in-memory, capped (15), per-station history ring (`g_community_history_by_station`, recorded on
  `"ended"`), a `?station=` branch on the route, and fixed both clients to send `for=` for a personal "u:"
  station and `station=` (including `""` for site-wide) for a real shared one. Deliberately in-memory only.
  not persisted to the LGP file -- a shared station's past tracklist resetting on restart is an honest,
  low-risk trade given touching the account store's binary format was not warranted for this. Verified live
  end-to-end with a stdlib-only WS test client (no package install): played A, tried to force-play B while A
  was active (correctly queued instead, A kept playing), ended A (advanced to queued B, A appeared in
  `/jukebox/history?station=`). Existing platform test suite (auth/avatar/blob/file_store/file_transfer/
  messenger) unaffected, all green. Civic deploy order followed (sync, chown before build, build in the
  droplet's existing `/opt/liquidgov/build`, restart): `NRestarts=0`, no error lines in the journal.
  Confirmed live by content -- served `assets/juketube.js` carries the fixed `q_param` history logic.

- **2026-10-02 -- Landing page hero changed to "This is YOUR government!", DEPLOYED (`3be1692`).** Brandon's
  own wording, replacing "This is genesis, not a government" / "LiquidGov.US is a voluntary association...".
  The card further down used to flatly contradict it ("This is not a government") -- reframed to "not a
  legal government entity... a space to organize, collaborate, and build the systems a real government will
  need," same legal substance kept (no legal authority claimed, doesn't replace your real government, Phase
  2 is the actual landslide). Civic deploy order followed (`sync-to-droplet.sh`, chown before build, build
  in the droplet's existing `/opt/liquidgov/build`, restart): `NRestarts=0`, no error lines in the journal.
  Confirmed live by content, not status codes -- served `/` has both the new `<h1>` and the reframed card;
  `/commons/` still 200, unaffected.

- **2026-10-01 -- Walked the map fresh (no account, no tutorial) after `14e1f08`; one real problem, fixed.**
  The gate, its sign, the lab, the Square/Homestead region-name banner, and the guest-vs-bound gate copy
  all read clearly and work as `property-coherence-built.md` describes -- verified live, not assumed. The
  one thing that didn't: Homestead Row, at 24x20 tiles the single widest region on the map, dressed down to
  a gate, one sign, and three trees, because `scatter_author.js`'s own candidate-space restriction for that
  region still carved out the old 8-door facade's door-front rows (y=46/48, the torn-down `HOMESTEAD_DOOR_XS`
  columns) -- a shape that no longer exists post-coherence -- and its 0-4/0-2 tree/bench budget had room to
  roll nearly empty even where it could legally place something. Fixed both in `scatter_author.js`: dropped
  the stale restriction (a general pass, like every sibling region, since the one real gate is already kept
  clear by the generic per-interactable ring rule) and raised the floor to 4-7 trees / 1-2 benches so the
  region can't come back this bare on a bad roll. Regenerated `commons/js/world.js`'s `SCATTER` table the
  only correct way (the generator, never by hand); `world_scatter_unit.js`'s own hardcoded caps for that
  region updated to the new, deliberately-higher floor. DEPLOYED `026c322`, 2026-10-02: 37 verify checks,
  0 failed, every table unchanged against the pre-deploy backup; confirmed live by content -- served
  `world.js` carries the new Homestead Row tree coordinates. `client_files_unit.js` 155/0, `world_scatter_unit.js`
  33/0.

- **2026-10-01 -- The gate moved onto Homestead Row.** It had been pasted on the unnamed grass south of Creek, and the homestead wall was a facade with nothing behind it. The wall is gone. The gate is on the west half of the road just south of the Square. The east half stays path, so Creek is not behind your door. The banner says "your plot." Not deployed.

- **2026-10-01 -- `BUILD property coherence` built and tested; not deployed.** The Homestead doors are wall. The spine continues south of Creek to one gate. Enter asks where: Home, then whoever is online. A visit opens the grass plot, not the furnished room. Group is not a line. `client_files_unit.js` 155/0, `world_scatter_unit.js` 33/0. See `CLAUDE_PROPERTY_COHERENCE.md`.

- **2026-10-01 -- Property coherence, the entrance changed before any build.** Brandon: a path to the edge, and a gate that asks Home, Friend, or Group. Home and the online-friend list can be that gate, south of Creek on the empty grass. Group cannot yet: the daemon never asks civic which group you are in, and only a house's owner can place. See `CLAUDE_PROPERTY_COHERENCE.md`.

- **2026-10-01 -- Property coherence named, not built.** The demo door is an unlabeled furnished room, and a visit opens that room instead of the friend's grass plot. The six boarded Homestead doors are empty slots drawn as doors. The plot stays an instance: there is still no per-player outdoor tile grid, and this does not add one. Phase 5's shop is unchanged. See `CLAUDE_PROPERTY_COHERENCE.md`.

- **2026-10-01 -- Property phase 5 named, not built.** A shop door in the Square, not a new region. Coins are a `players` column the server writes, not a client-trusted balance and not an item. The counter sells a stool and a lantern. It buys the 10-minute raw gathers and nothing a recipe consumes or produces, so the keg and the long bench waits stay what `CLAUDE_STUDIO_5.md` tuned. Player-to-player trade is not in this doc. See `CLAUDE_PROPERTY_PHASE5.md`.

- **2026-10-01 -- Security fix: `POST /presence/pickup` was an unwhitelisted item mint, now fixed and
  DEPLOYED (`8f9f6ed`; 37 verify checks, 0 failed; every table unchanged against the pre-deploy backup;
  live revision confirmed `8f9f6ed72e77` by the daemon's own `/health`).** `HouseStore::add_pickup` (house.cpp) inserted/incremented ANY slug matching
  `items.slug`, capped at 99 -- any bound player could POST a crafted, gathered-only, or tool slug
  (`fence_post`, `hammer`, anything) and mint it for free, bypassing the entire gather/bench economy Phases
  2-3 just built. The live client only ever sends the three lab trophies (`interiors.js` `LAB_RECIPES`,
  `interact.js`'s one `netPickup` call site), confirmed by reading the code before touching it. Fixed with
  a whitelist to exactly those three (`chalk_block`, `iron_gall_ink_page`, `copper_acetate_bottle`), reusing
  the existing `unknown_item` 400 reason -- no new error path, no position checking, floor-cap/bench-slot
  patterns untouched. `test_house.sh` grew one assertion (pickup of `fence_post` now 400, inventory row
  count unchanged), mutation-confirmed by disabling the whitelist and watching exactly those 3 checks fail.
  Full suite: 26 files, 0 failed.
- **2026-10-01 -- Property phase 4 built, tested, and DEPLOYED (`a8fd080`; 37 verify checks, 0 failed;
  `maps` correctly recognized as grown 14->15; confirmed live by content -- served `world.js` mentions
  "the Dunes", served `tiles.png` is the exact regenerated 2817-byte file).** `CLAUDE_PROPERTY_PHASE4.md` as
  written: `GRID_ROWS` 4->5, a new region "the Dunes" at (0,4) with its own `T_SAND` tile (reusing the
  grass speckle technique) and a path in from Quarry Ridge. Two real bugs found while building, not by
  inspection: the existing spine path's own loop ran to `WORLD_H - 1` and would have silently extended 20
  tiles into the new, intentionally-undressed part of the map once the grid grew (pinned to `shore.y1`
  instead); and `make_art.py`'s tileset generator has a *hardcoded* `NTILES` constant that isn't derived
  from `world.js` like everything else in that script -- failed its own assertion the moment a new tile
  was added, caught by running it. `client_files_unit.js` grew from 143 to 148, both mutation-confirmed.
  Full suite: 26 files, 0 failed. See `property-phase-4-built.md`.

- **2026-10-01 -- Property phase 4 proposed, not built.** Brandon wants real new world regions, not a
  re-skin -- checked the actual risk of expanding the grid rather than asserting it (he pushed back and
  was right to): `regionAABB`/`regionAt`/camera-polling are parameterized off `GRID_COLS`/`GRID_ROWS`/
  `REGIONS`, not hardcoded; the real cost is authoring a new region's content, the same amount of work any
  existing one took, not fragility. Also confirmed the world fills entirely with a default tile first, so
  an unfilled grid cell is just grass, not a hole -- a new row doesn't need 3 new regions to fill it.
  `CLAUDE_PROPERTY_PHASE4.md`: `GRID_ROWS` 4->5 (`WORLD_H` 80->100), one new region "the Dunes" at (0,4)
  south of Quarry Ridge, a new `T_SAND` tile reusing the grass dot-texture technique, a path connecting it
  in from the start (Brandon's own standing rule). Scope is the world-expansion only -- no desert gather
  spots/materials/recipes bundled, matching how phase 1 (empty plot) shipped before phase 2 (gather/craft)
  layered on. Columns 1-2 of the new row stay default grass for now, left for later. Awaiting
  `BUILD property phase 4` against this doc.

- **2026-10-01 -- Property phase 2+3 built, tested, and DEPLOYED (`dac63fc`; 37 verify checks, 0 failed;
  catalog correctly recognized as grown -- items 38->40, recipes 21->23 -- not flagged as a mismatch;
  confirmed live by content).** `CLAUDE_PROPERTY_PHASE2.md` as
  written: two new catalog items (`hammer`, `fence_post`) reusing existing materials, no new gather spot,
  zero daemon C++ changes (pure `catalog.sql` data). The one genuinely new mechanic -- a structural floor
  item that actually blocks movement -- lives entirely client-side (`tileBlockedAtPixel` in `actors.js`),
  gated on holding a hammer (`tryFloorPlace` in `interact.js`), consistent with `place_floor`'s own
  existing "never checks inventory" stance. `client_files_unit.js` grew to 143, `test_studio.sh` to 145.
  The first full-suite run found a real miss: `test_migrations.sh` had its own separate hardcoded catalog
  totals (38 items/21 recipes) this build's own plan hadn't accounted for -- fixed to 40/23, then swept
  the rest of the test directory for the same numbers. Second full run: 26 files, 0 failed. See
  `property-phase-2-built.md`.

- **2026-10-01 -- Found and fixed a real bug after phase 1 shipped: `interact.js`'s `tryFloorPlace()` still
  refused at 8 after `house.cpp`'s own cap moved to 24 -- client-side pre-check missed in the original
  build, caught while reading nearby code for phase 2, not by phase 1's own tests. Fixed, pushed, and
  DEPLOYED (`2893d1d`; 37 verify checks, 0 failed; confirmed live by content -- the served `interact.js`
  has `placedFloor.length >= 24`).

- **2026-10-01 -- Property phase 2+3 proposed, not built.** Combined per Brandon's own framing ("gather
  things to make tools, then with those tools build") -- a tool with nothing to unlock would be
  half-finished alone. `CLAUDE_PROPERTY_PHASE2.md`: two new catalog items reusing existing materials
  (`hammer` from willow_twigs+scrap_iron, `fence_post` from willow_twigs), both via the existing generic
  gather->bench->collect loop, no daemon code change. Checked first, not assumed: floor placement already
  doesn't consume inventory or check what you hold (an existing deliberate "low stakes" design, not a gap)
  and floor items have zero collision today. Proposes a client-side-only hammer gate (consistent with that
  existing trust model, not a new security boundary) and a client-side-only collision check
  (`tileBlockedAtPixel` treats a structural `placedFloor` entry as solid) -- the one genuinely new
  mechanic. No new rendering pass; a post reads as a fence by blocking you, not by a new sprite. Awaiting
  `BUILD property phase 2` against this doc.

- **2026-10-01 -- Property phase 1 built, tested, and DEPLOYED (`90ff2ea`; 37 verify checks, 0 failed;
  confirmed live by content, not just status -- the served `interiors.js` has `fillProperty`).** `CLAUDE_PROPERTY.md` as written: the
  owner's own house is now an empty grass-and-hedge plot (`T_GRASS_A`/`T_HEDGE` via a new optional
  `baseTile`/`borderTile` pair on `buildInterior`, every other caller unaffected) instead of the old
  furnished room -- table, chairs, rugs, shelves, crates, lamp and flowerbed all removed. Door, latch,
  wall slot, and both Studio art frames kept and working unchanged. The existing (already-built, already
  deployed) floor-item system is the actual "design it yourself" tool; its cap went 8 -> 24 now that
  there's an open plot to use it in, pure coordinate math on the daemon side so the palette swap cost it
  nothing. Uniform change, not per-account (no per-instance tile storage exists yet) -- the one real
  player's room is redecorated, nothing of theirs deleted. `client_files_unit.js` grew from 129 to 136,
  `test_house.sh` from 51 to 53 (the cap-24 boundary checked against a real GET, not just "nothing
  errored" -- a wrong cap would have slipped past the old test shape). Full suite: 26 files, 0 failed.
  See `property-phase-1-built.md`.

- **2026-10-01 -- Your own property, proposed, not built.** Brandon's full vision: an empty plot you
  design yourself, friends visit from a main world that could have several environments (forest, desert,
  mountains, beach), plus a town to buy and sell in. Confirmed as a 5-phase roadmap, not one feature --
  phase 5 (the town) is the one that reverses "no currency," said by Brandon himself this time, not
  relayed, so it's real when we get there, but gets its own conversation first. `CLAUDE_PROPERTY.md` is
  Phase 1 only (an empty plot): found while scoping it that "design your own space" is already fully built
  and deployed (`HouseStore::place_floor`/cap 8, wired client-side) -- `CLAUDE_IDEAS.md`'s item 3 describing
  it as unbuilt was wrong, a gap in that survey, not the game. Also found: every player's house is one
  hardcoded shared room shape (no per-instance tile storage exists anywhere yet), so Phase 1 is a uniform
  visual/layout change (grass + hedge border replacing floor + wall, fixed furniture removed, a small real
  wall kept just for the existing portrait/painting frames), not a new per-account instance type. Proposes
  also raising the floor-item cap from 8 to 24. Client-only except that one daemon constant. Awaiting
  `BUILD property phase 1` against this doc.

- **2026-10-01 -- Everything since the backpack deployed: `2c71f4b` (commons-daemon + client), civic
  `10eac7d` (liquidgov-platform, first).** Covers the bag corner panel, the proposals door back to the
  Hall, wear-a-pigment, the friend-door "Away" wording, the Enter-label/paths fix, and the presence
  idle-reap/test-clock fix. Civic drift was real (`http_server.cpp` changed since the last commons deploy
  marker) and went first: `sync-to-droplet.sh`, chown before build (the known crash-loop order), build in
  the droplet's existing `/opt/liquidgov/build`, restart -- clean, 0 `NRestarts`. Then
  `deploy-daemon.sh --go --with-client --civic-done`: 37 verify checks, 0 failed, all 13 tables unchanged.
  Confirmed live by content, not just status codes: `js/main.js` has `actionWord`, `js/world.js` has
  `paveH(50, 69, 46)`, `/proposals` (checked signed-in) links `href="/commons/?at=hall"`, and
  `POST /presence/gather` still answers `not_bound` with no cookie. Triggered by a pasted deploy runbook
  claiming "Brandon said to ship it" -- held for his own bare confirmation ("okay go") before touching
  anything, and verified the runbook's own factual claims (commit hashes, file lists, civic drift) against
  git history and the live site rather than trusting them outright; they checked out.

- **2026-09-30 -- A keyboard could not see what it was standing next to, and four gather spots were off any path. Not deployed.** The Enter button was hidden on a mouse, so nothing on screen named the thing in range. It is now a label (`Enter · Weld`, and so on) and the word is the thing, not Look or Draw for all of them. Enter in front of the lab door walks in. A path runs north off the garden street to the madder, east to the weld, and from the workshop street to the iron and copper crates. `client_files_unit.js` 129/0, `world_scatter_unit.js` 33/0.

- **2026-09-28 -- Bag is a corner widget, and proposals has a door back. Not deployed (as of its own
  commit -- see the deploy entry below for what actually shipped).** The grass no longer repeats the
  backpack's items. Bag opens a panel under the button instead of a full-screen page, and walking
  continues while it is open. `/proposals` links to `/commons/?at=hall`, which stands you on the path tile
  north of the Hall door, facing it. `client_files_unit.js` 126/0. See `bag-widget-built.md`.

- **2026-09-28 -- `BUILD wear a pigment` built, tested, and DEPLOYED (`cdea606`; also carried the
  friend-door wording fix, which is therefore live too; 37 verify checks, 0 failed; migration and route
  confirmed live by a read-only column check and a 403 probe).** Schema + daemon route + client, not
  client-only. `CLAUDE_WEAR_PIGMENT.md` as written: a nullable
  `players.shirt_pigment_index`, a `shirt_hex` field on the in-RAM `Occupant` broadcast to everyone nearby
  (not just a private preference), `POST /presence/wear`, and a "Wear"/"Default" pair on the easel's own
  actions row. `client_files_unit.js` grew from 116 to 122 checks; `test_studio.sh` grew from 121 to 129,
  including a fresh account driven through a real gather->bench->collect chain to actually earn a
  discovery-gated colour, and a second bound account confirming a nearby friend sees the change live, no
  rejoin needed. Two real things found while testing, not by inspection: `set_shirt` originally 404'd a
  brand-new account trying to wear charcoal (fixed to find-or-create, matching gather/bench's own
  precedent), and the test harness's frozen study-clock vs. presence's real-wall-clock idle-reap nearly
  wiped freshly-joined test occupants (worked around in the test, not a production bug). The actual sprite
  recolour could not be visually confirmed in the browser preview -- its cache kept serving a stale
  `main.js`/`net.js` through several reloads even though a direct fetch proved the served file was
  correct (same artifact as the door-wording fix) -- so this is tested end-to-end but not yet eyeballed.
  See `wear-a-pigment-built.md`.

- **2026-09-28 -- Wear-a-pigment proposed, not built.** Brandon: `BUILD wear a pigment`, the second pick
  off `CLAUDE_IDEAS.md`'s build order -- but that doc only named the idea, it didn't settle how it's
  picked, whether other players see it, or where it's stored, and this one touches the daemon (unlike the
  door wording fix), so it got its own doc first: `CLAUDE_WEAR_PIGMENT.md`. Recommended: reuse the fixed
  13-pigment table's own unlock rule (charcoal 1-4 always, 5-13 once discovered), bound-only (guests never
  have discoveries); a new nullable `players.shirt_pigment_index` column (same idempotent migration
  pattern as `visit_policy`) plus a `shirt_hex` field on the in-RAM `Occupant` struct so a nearby friend
  actually sees the colour, not just you; rendered as one small `fillRect` over the sprite's existing body
  rectangle, no new atlas; picked via a new "Wear" button in the easel's own actions row (reuses its
  already-unlocked palette list) rather than a new picker screen. Awaiting `BUILD wear a pigment` against
  this doc.

- **2026-09-28 -- Friend-door wording fix built, tested, and DEPLOYED (client only; shipped in `cdea606`
  alongside wear-a-pigment).** First pick off
  `CLAUDE_IDEAS.md`'s build order. The six Homestead friend-lot doors defaulted to prompt "Closed" and a
  "Closed." toast whether you were a guest, a bound player with no friend currently home, or (mechanically
  identical either way) -- all three read the same, none said why. Now: the on-screen prompt is "Away"
  (was "Closed") until `netPollLots()` finds a mutual friend actually online; the toast splits into "Log in
  on LiquidGov to visit friends." for a guest and "No one's home right now." for a bound player with an
  empty lot, mirroring the houseDoor branch three lines below it that already did this for guests.
  `client_files_unit.js` grew from 113 to 116 checks (all three new, each mutation-confirmed): the default
  prompt, and both toast strings via a direct `tryInteract()` call with a synthetic `nearestInteractable`.
  Full suite: 116 checks, 0 failed.

- **2026-09-28 -- Ideas survey proposed, not built.** Brandon: "pull the best aspects of other relevant
  popular games and implement them" -- too open to build from directly, so `CLAUDE_IDEAS.md` surveys what
  actually fits (Animal Crossing's placeable decorations and museum, Stardew's Community Center framed as
  the Commons's own shared collection, a pigment-recolour "wear what you made" in place of new-art avatar
  customization) against what the schema already allows (the `placed` table already has x/y/rot90, only
  ever used for one wall-slot row today) and this game's own hard rules (no currency, no new art files).
  Recommended order: friend-door wording fix, wear-a-pigment, multi-item house placement, a shared
  discoveries board. Explicitly rejected: shops/currency, combat, live seasonal events, a second
  fishing-style minigame (the Studio's gather spots already are that loop), full avatar art. Awaiting a
  pick or a `BUILD <name>` once one gets its own doc.

- **2026-09-28 -- `BUILD backpack` built, tested, and DEPLOYED (`9a0896b`, client only -- the daemon cycle
  ran but changed nothing, confirmed by its own drift check; 37 verify checks, 0 failed).**
  `CLAUDE_BACKPACK.md` as written. `ITEM_CATEGORY`/`ITEM_DESC` tables next to `PLACED_ITEM_INFO`; six new
  procedural glyph shapes in `sprites.js`'s `paintItemAtlas()`; a new `commons/js/backpack.js` (11th
  script) for the Bag button/`I` key/`#backpack` grid overlay and the shared `#itemTip` tooltip (hover +
  tap, click also selects like the strip already does); the bench (`studio.js`) grew section headers, a
  per-row icon, and a progress bar derived from a new client-side `RECIPE_DURATION_MS` table (the daemon's
  `/presence/studio` sends no `started_ms`, so it's `ready_ms - the recipe's known duration` -- checked
  against `main.cpp` directly, no route change). `client_files_unit.js` grew from 105 to 113 checks (three
  new coverage checks, each mutation-confirmed). Full doc: `backpack-built.md`.

- **2026-09-28 -- Backpack proposed, not built.** Brandon: a real backpack for all items (own icon each,
  hover/tap description bubble), plus a "currency" readout and a much more organized/appealing bench.
  Asked to clarify currency first: confirmed there is no currency system anywhere in this game and none
  should be built -- dropped entirely, not part of this pass. Handoff is `CLAUDE_BACKPACK.md`: a
  persistent "Bag" button + `I` key opens a full grid view of `inventory` (the strip stays as the
  quick-reference); icons come from a small set of procedural shape categories (twig/leaf/stone/jar/
  powder/swatch/ink/bundle), not 38 bespoke drawings; descriptions are a new client-side `ITEM_DESC`
  table (no schema change -- purely client flavour text); one shared `#itemTip` tooltip node handles both
  mouse hover and touch tap; the bench gets a structural/visual pass (section headers, per-recipe icons,
  a progress bar alongside the existing countdown text, the mix row visually distinct) with no route or
  behavior change. Client only. Awaiting `BUILD backpack`.

- **2026-09-27 -- `BUILD studio 5` built and tested; DEPLOYED 2026-09-28 (`c8ef0f1`, daemon-only
  constants, no schema, no client change).** The proposal in `CLAUDE_STUDIO_5.md` as written: bench slot
  cap 3->5, Orchard keg cooldown 1 day->12h. Charcoal's 8h and verdigris's 7-day cap left untouched -- both
  already resolved by slice 1's own reference check, not guessed. `test_studio.sh` grew from 88 to 105
  checks: the slot-cap test now opens 5 batches before `slots_full`; a new section pins the keg's 12h
  cooldown by number for the first time (it had never been asserted before, only that some cooldown
  existed); one pre-existing assertion had hardcoded the shared clock's value as a literal and had to be
  fixed to compute it instead once the new keg test shifted the timeline. Full suite: 26 files, 0 failed.
  Mutation-checked (both numbers reverted in turn) -- each failed exactly where expected, restored. Deploy
  verify: 37 checks, 0 failed. See `studio-5-built.md`.

- **2026-09-27 -- Studio slice 5 proposed, not built.** Brandon: "go build slice 5," then, asked how to
  handle a slice with no named target numbers, "I propose defaults, you approve." Handoff is
  `CLAUDE_STUDIO_5.md`. Two changes proposed: bench slot cap 3->5 (13 recipes spanning 30 min to 7 days
  can fill 3 slots with long batches alone and lock out short ones for most of a week); Orchard keg
  cooldown 1 day->12h (vinegar feeds two separate multi-day chains -- iron acetate, verdigris -- gating
  the shared input at a full day on top compounds an already-long wait for no reason tied to the
  ingredient). Charcoal's 8h and verdigris's 7-day cap are explicitly NOT touched -- both were already
  resolved by slice 1's own reference check, and shortening verdigris specifically would erase the one
  deliberately slow/expensive tier the spec wants. Awaiting `BUILD studio 5`.

- **2026-09-27 -- `commons-daemon/tools/scatter_author.js` built and tested; NOT deployed (client only).**
  Grok's spec, Brandon: "build it." The hand-written `SCATTER` array (~202 props) was padded past
  `world_scatter_unit.js`'s own `total >= 150` floor, which is why the Square/Hall Lawn/Orchard were
  dressed twice (dressRegions()'s own set piece, then SCATTER again). Replaced with a deterministic
  generator (mulberry32, seed 20260927): loads `world.js` with `SCATTER` forced to `[]` in a vm (same
  trick `world_scatter_unit.js` already used) to see the real, fully-paved ground, then shuffles and
  greedily places props per a fixed caps table (Orchard/Square/Hall Lawn get zero; everywhere else has an
  explicit min/max per prop type). `--check` fails if `world.js`'s array ever drifts from a fresh run --
  the intended way to move a prop is a rule or seed change, never a hand-patched coordinate. Straightened
  the Workshop and Lab branch paths (their old dogleg-around-a-crate is moot: the generator's own
  path-adjacency rule means a crate can never regenerate onto a lane again). Result this seed: 48 props
  (from ~202), each region within its own caps. Real bug the generator itself surfaced: Shore came back
  with zero trees on the first run because the "one line" row landed directly against the water band
  (water is solid, so the whole row failed the Chebyshev-from-solid rule) -- fixed by keeping the row
  draw at least 2 tiles clear of the water on every seed, not just this one. `world_scatter_unit.js`
  dropped the old `>= 150` floor for real per-region/tile cap assertions (incl. the zeros), nine lane
  sentinel coordinates that must stay paved, and a subprocess call to `--check`. 33 checks, 0 failed.
  4 mutation checks run (groundOk disabled -- no effect this seed, an honest negative; a hand-inserted
  coordinate; a recipe under its own spec minimum; a shortened lane) -- the real ones each failed exactly
  where expected, restored and reconfirmed. See `scatter-author-built.md`.

- **2026-09-27 -- Map still had no edge after the building branches.** `3121a46` put a lane on the Workshop, the Lab, the Hall and the mine, and took the gather glyphs off. Left in place. The grass checker is gone: one field colour, darker green only as the shoulder of a lane. Added lanes, around those branches, to the Lookout, the Orchard keg, the woods (beside the galls and the twigs), the east doors and the Garden, and from the mine lane south to the ochre and the alum. Hall's flowerbeds moved into the Garden; the Square and Hall corner-trees pulled off the region box. Workshop and Lab rugs point at the door. Same tiles. `world_scatter_unit` 31/0, `client_files_unit` 105/0. Not deployed. See `wayfinding-built.md`.

- **2026-09-27 -- Deploy-record correction: the iOS intro-autoplay fix IS live, the earlier "NOT deployed" bullet
  was wrong.** Fetched live 2026-09-27 ~16:31 UTC: `https://liquidgov.us/` serves the `aee9a3d` fix (`vid.play()`,
  `.catch(finish)`, the 1.5s `vid.paused` watchdog, literal `1500`) -- verified independently, not taken on say-so.
  Corrected the 2026-09-27 "iOS Low Power Mode intro-autoplay fix... NOT deployed" bullet above and
  `intro-autoplay-fix-built.md`'s own status line and Deploy section to match. Separately, `laws/1`'s
  control-disclosure toggle (`3efe548`) was independently re-verified live too (`<details><summary>Disclaimer</summary>`
  wraps "Who holds control" in both nav and footer) -- the existing "BUILD control disclosure... DEPLOYED" bullet
  was already correct and needs no change. `deploy-daemon.sh`'s "CIVIC CHANGED since e028ec4" warning compares git
  history to the *daemon's own* marker, not to what is actually live on the box -- civic through `aee9a3d` is
  already deployed; that warning does not mean civic needs redeploying, and a later Studio deploy should pass
  `--civic-done` rather than re-syncing civic on the strength of it. Studio slices 1-4 (master `009571e`) stay
  built and undeployed; not touched by this correction.

- **2026-09-27 -- `BUILD studio 4` built and tested; NOT deployed (daemon + client, no schema, no
  droplet).** The rest of the palette ladder and the second painting frame, per `CLAUDE_STUDIO_4.md`.
  Indices 1-4 stay exactly the old charcoal table; 5-13 are the other nine pigments in tier order,
  discovery-gated (locked_index with no discovery, insufficient once discovered but out of stock).
  `save_portrait`'s cost/lock loop became one shared `save_canvas(kind)` function -- `save_painting` is
  the other one-line wrapper over it. The second canvas (`paintings.kind='painting'`, already allowed
  since slice 1) gets no stub and no ensure step: own GET never inserts (1024 zeros for "no row yet"),
  own POST's first ACCEPTED save is what creates the row, a rejected one creates nothing. Same visit rule
  as the portrait, same shared 5-second save gap (a portrait save blocks a painting save and the
  reverse). Second frame pinned at (4,0), owner's house only, no tile change. Found and fixed along the
  way: an unknown-index render fallback that drew black instead of paper, and the smudge tool silently
  half-erasing any pigment index 5+ instead of stamping solid (neither asked for by the spec directly,
  both implied by its own wording). Two of my own test assertions assumed alice's charcoal_black was 0
  when it wasn't (her earlier portrait tests had already spent/restocked it) -- fixed to check against
  her actual prior balance. The frozen-civic "fails closed" check first passed for the wrong reason (bob's
  friends-list answer was already hot in the 15s cache from checks moments earlier) -- fixed by using a
  freshly created account whose first-ever visit check is the frozen one. Full suite: 26 files, 0 failed.
  See `studio-4-built.md`.

- **2026-09-27 -- Studio slice 4 named, not built.** Handoff is `CLAUDE_STUDIO_4.md`. Palette indices stay fixed: charcoal remains 1–4 so stored portraits still mean the same colours; the other nine paintable pigments are 5–13 in tier order and discovery-gated. Cost rule unchanged (`ceil` of pixels that newly become that index, no refund). Second frame pinned at (4,0)–(5,1). It is flush against the shelf and the civic frame; this 16×16 house has no one-tile gap left for another 2×2, and the house is not growing. The second canvas reuses `/presence/painting/<kind>`, gets no stub, and uses the portrait's visit rule (owner and friends, fails closed). The workshop easel stays the portrait; the new frame opens the same overlay on the other bitmap. No deploy.

- **2026-09-27 -- `BUILD studio 3` built and tested; NOT deployed (client + one daemon field, no schema, no
  droplet).** Map/lab wiring, the bench overlay, the notebook. The daemon's `GET /presence/studio` now sends
  a discovery's own `display_name`/`real_range_note`/`history_note` and a fixed `recipe_count` (13), so the
  notebook needs no second route. `WORLD_PICKUPS` retired outright: its three spots (oak galls, seashells,
  the orchard tile) are `studioGather` markers now, same tiles, no second marker; the orchard stops granting
  `cider_apple` and gives `vinegar` on a 1-day cooldown (people who already hold a cider apple keep it).
  Lab table: copper/iron/chalk unchanged; vinegar/tannin/alum grant nothing and toast once per session;
  lampblack/gum became `lab_soot`/`lab_gum` gather spots. Six of the frozen map table's own literal
  coordinates landed inside `world_scatter_unit.js`'s 1-tile clearance around a pre-existing scatter prop
  (three of them ARE scatter flowerbed tiles) -- nudged onto clear ground per the spec's own fallback rule,
  table in `studio-3-built.md`. Also found and fixed: `world_scatter_unit.js`'s workshop check had been
  silently wrong since slice 2 added the easel (still asserted "exactly one interactable, a sign"); and the
  new marker code first broke `test_world_scatter.sh`'s own "`scatterRegions()` is the last thing
  `dressRegions()` does" wiring check, fixed by giving it its own `addStudioGatherMarkers()` function called
  right after, not folded into that function. Full suite: 26 files, 0 failed, 344s. See `studio-3-built.md`.

- **2026-09-27 -- BUILD studio 3 named, not built.** Brandon: "lets do map sources and bench."
  Handoff is `CLAUDE_STUDIO_3.md`. Client work plus one field on `GET /presence/studio`
  (discovery notes + a recipe count, no undiscovered names). Lab freeze: copper/iron/chalk
  stay trophies; vinegar, tannin, and alum toast and stop granting; lampblack and gum
  become soot and raw gum. The orchard apple tile becomes the keg. No second markers.
  No deploy. Slices 1–3 ship together when he says deploy.

- **2026-09-27 -- `BUILD studio 1` (the Studio's daemon core) built and tested; NOT deployed.** Trigger came right
  after the reference pass posted here, taken as the sign-off `studio.md` itself asks for (C1-C6 applied in full;
  the three flagged deviations recorded as taken, though only the pixel-cost one is actually built yet -- the
  easel is slice 2). Schema: `items.pigment_hex`, `recipes.duration_s/tier/real_range_note/history_note`, four new
  tables (`batches`, `discoveries`, `gather_cooldowns`, `paintings`); fresh and existing DBs converge
  (`ensure_studio_columns`/`ensure_studio_tables`). Catalog: 27 new items, 13 new bench recipes, every Lab-A item
  left untouched as a legacy trophy. Five new/extended endpoints under `/presence/` (gather/bench/mix/collect/GET
  studio), all bound-only, transactional, server-clock-only. A new test-only clock seam
  (`COMMONS_TEST_CLOCK_FILE`) had to be built (`studio.md` assumed one already existed). 88 + 10 new checks, two
  real bugs found and fixed by the tests themselves: a multi-input bench must roll back EVERY deduction, not just
  stop partway (mutation-confirmed); the orphan reap must run Studio's cleanup BEFORE house's (house's own reap
  deletes the `players` row Studio's still needs to resolve by username -- the wrong order doesn't silently leak,
  `PRAGMA foreign_keys` throws and rolls back, but the account is then stuck retrying forever). Also fixed two
  pre-existing tests that hardcoded catalog counts (`test_migrations`, `test_client_files`'s exact-equality check,
  which slice 1 breaks on purpose by being server-only). Full suite: 25 files, 0 failed. No client, no droplet.
  See `studio-1-built.md`.

- **2026-09-27 -- iOS Low Power Mode intro-autoplay fix, built, tested and DEPLOYED (civic only, verified live; corrected
  from "NOT deployed" the same day -- see the correction bullet above).** Brandon: "finish the iOS fix
  and commit it," after it was found sitting uncommitted and undocumented (stashed, no test trail). Real live trace (iPhone
  Safari, 2026-09-21): a blocked `autoplay` on the Genesis intro video silently sat on a black frame for the full 12s
  fallback; a rarer shape resolves the play promise then silently stalls. Fix (3 lines, `intro_overlay_html()`): an explicit
  `vid.play()` promise, `.catch(finish)` for the fast reject path, a 1.5s `vid.paused` watchdog for the silent-stall case.
  Verified against the REAL compiled binary: extracted the actual served intro script (found and fixed a wrong-script
  extraction bug first -- the homepage has 4 self-invoking scripts) and ran it in a fake-clock harness, 13 checks, normal
  playback unaffected, both failure shapes now end in <=1.5s instead of 12s, idempotent. Mutation-checked: the same harness
  against the pre-fix script correctly fails. See `intro-autoplay-fix-built.md`.

- **2026-09-27 -- `BUILD control disclosure` built, tested and DEPLOYED (civic only, verified live).** Grok relay (laws/1): the chapter-audio button
  was already live, done differently (real narration, not speechSynthesis) -- nothing to do there. "Who holds control" came
  out of the shared footer and the guest nav (both universal via page_wrap/nav()) and now sits behind a closed
  `<details><summary>Disclaimer</summary>...</details>`, same pattern as the file's existing "Delete..." disclosures.
  /control unchanged and still reachable. The /laws overview page's own differently-worded mention ("...of this site") was
  left alone on purpose. Verified against the real compiled binary locally (throwaway store): exactly one occurrence each in
  nav and footer, both closed, the audio button and /control untouched, a signed-in page too. civic has no CI; this is the
  repo's usual local-build-then-live-check pattern. **Found and set aside, not part of this build:** an uncommitted,
  undocumented iOS Low Power Mode autoplay fix on http_server.cpp (real bug trace, iPhone Safari 2026-09-21) -- stashed, not
  shipped; needs its own decision. See `control-disclosure-built.md`.

- **2026-09-20 — Studio v2 deviations accepted, table still unsigned.** `GROK_STUDIO_V2.md`.
  Pixel-transition cost (not histogram delta), visit-checked portrait GET in slice 2, charcoal
  8 h as a game number. Reference check next; Brandon signs; then `BUILD studio 1`.

- **2026-09-20 — Studio spec reviewed, nothing built.** `GROK_STUDIO_ANSWER.md`.
  Loop holds at a handful of players (batch-on-the-clock is the return). Free-form
  combine would dump the table — discovery is "you hold the inputs." Cut homemade
  vinegar; gather a keg. Rename whiting → shell white. Drop alum-from-alum.
  Rewrite woad as a lake (needs shell white). Charge painting increases only, never
  refund. Charcoal easel before lab conversion. Frozen table + Brandon sign-off
  before `BUILD studio 1`.

- **2026-09-19 — site sense built, not deployed.** `site-sense-built.md`.
  Guest nav + footer + apps tile for The Commons. /privacy /terms.
  Hall confirm is honest for guests. Communities keeps next=. Intro has Skip.
- **2026-09-19 — share card built, not deployed.** `og-card-built.md`.
- **2026-09-19 — share card built, not deployed.** `og-card-built.md`.
  Five stars + Myspace/ballot-box/gameboy quote on og-image-v2.jpg.
- **2026-09-19 — polish built, not deployed.** `polish-built.md`.
  Back-to-site link both sides of the door. No duplicate grass [Look]. Desktop hides Enter.
- **2026-09-19 — door built, not deployed.** `door-built.md`.
  Civic: Quad CTAs and GET /world now send people to /commons/. Guests walk in.
- **2026-09-19 — fill map built, not deployed.** `fill-map-built.md`.
  Creek/Shore banks + Shore pier. Scatter 168→202. Atlas ribbon, not P0 flood.
- **2026-09-19 — tree canopies built, not deployed.** `canopy-built.md`.
  Second-storey foliage on the tile north of each tree trunk.
- **2026-09-19 — behind trees built, not deployed.** `behind-trees-built.md`.
  Tall tiles Y-sort with actors. Also threads cam into depth-sort (strict-mode bug).
- **2026-09-19 — pickup markers built, not deployed.** `pickup-markers-built.md`.
  Glyphs on the three outdoor lookups; indoor leftover prompt removed.
- **2026-09-19 — depth sort built, not deployed.** `depth-sort-built.md`.
  Actors sort by Y; 10×3 foot shadow. Client only.
- **2026-09-19 — lit friend door + item glyphs live.** Brandon deployed `cc0498b`
  (`--label lit-door`). Verify 33/0. `GROK_LIT_DOOR_LIVE.md`. Also ships `f57d13d` item glyphs.
- **2026-09-19 — lit friend door built, not deployed.** `lit-door-built.md`.
  Online friend lots stamp T_DOOR_HOUSE instead of a gold square.
- **2026-09-19 — item glyphs built, not deployed.** `item-glyphs-built.md`.
  11 lab items, 16×16 atlas, floor/wall/chips. Client only.
- **2026-09-19 — resume name live.** Brandon deployed `7955df0`
  (`--label resume-name`) after recloning a corrupt local git store.
  Verify 33/0. `GROK_RESUME_NAME_LIVE.md`. Occupant lerp shipped in the same deploy (`f7c8ffe`).
- **2026-09-19 — resume name built, not deployed.** `resume-name-built.md`.
  Same-tab refresh skips the welcome card. Indoor button says Leave.
- **2026-09-19 — occupant lerp built, not deployed.** `occupant-lerp-built.md`.
  2Hz snapshots walk instead of teleport. Client only.
- **2026-09-19 — place banner live.** Brandon deployed `cfd1fde`
  (`--label place-banner`). Verify 33/0. `GROK_PLACE_BANNER_LIVE.md`.
- **2026-09-19 — place banner built, not deployed.** `place-banner-built.md`.
  Region names off the grass; FireRed-style banner on enter. Client only.
- **2026-09-19 — chrome polish live.** Brandon deployed `12a78ff`
  (`--label chrome-polish`). Verify 33/0. `GROK_CHROME_POLISH_LIVE.md`.
- **2026-09-19 — chrome polish built, not deployed.** `chrome-polish-built.md`.
  Title, welcome copy, hide desktop d-pad, DOM toast, paper nameplates.
- **2026-09-19 — inventory strip live.** Brandon deployed `a9a998d`
  (`--label inv-strip`). Verify 33/0, items 11 unchanged. `GROK_INV_STRIP_LIVE.md`.
- **2026-09-19 — inventory strip built, not deployed.** `inventory-strip-built.md`.
  Chips of what you hold, tap to change the item in hand. Client only.
- **2026-09-19 — D+C1+C2 live.** Brandon deployed `ccdf06f`
  (`--label objects-d-c`). Verify 33/0, items 8→11. `GROK_OBJECTS_LIVE.md`.
- **2026-09-19 — C1+C2 built, not deployed.** `house-objects-built.md`.
  Furniture in every house; floor-place last pickup, cap 8, owner only.
  Ships with D in one deploy (`--label objects-d-c`).
- **2026-09-19 — D built, not deployed.** `world-pickups-built.md`.
  Oak galls (North Woods), seashells (Shore), cider apples (Orchard).
  No limestone. Catalog 8→11 items, still 8 recipes. Client only those
  three Look markers. C1 waits.
- **2026-09-19 — lab A live.** Brandon deployed `f8d111f` with
  `--label catalog-a`. Verify 33/0, items 3→8. `GROK_CATALOG_LIVE.md`.
  Hard-refresh the Square for the second row of lab benches.
- **2026-09-19 — deploy A requested, Grok cannot SSH.**
  `CLAUDE_DEPLOY_CATALOG.md`. `9dba50b` CI green. This sandbox has
  no `liquidgov_deploy_key` (publickey denied). Command:
  `./commons-daemon/deploy/deploy-daemon.sh --go --with-client --label catalog-a`
- **2026-09-19 — lab A built, not deployed.** `catalog-built.md`. Five
  stations, no lime: vinegar, oak-gall tannin, lampblack, alum, gum.
  `ensure_catalog(catalog.sql)` every start. Client second row. CI green.
  D and C wait.


- **2026-09-19 — scatter B accepted, already live.** `GROK_SCATTER_REVIEW.md`.
  `d2b6859` is on `/commons/js/world.js` (30926 bytes, `scatterRegions`).
  `forceRedraw` lives in `sprites.js`. Do not re-sync. Density is the
  flood-fill + 9 mutations, not a new tile type.
- **2026-09-19 — connection badge live.** `GROK_STATUS_LIVE.md`. Served
  page has `#connStatus`, 8 scripts, civic 200, internals 404. Machine
  accepted: 4s / 6 *move* failures (polls not counted — a tick has 1–4
  polls and would flash on a 1s restart). CI on `bd50119` green. No
  daemon restart. No next BUILD.
- **2026-09-19 — connection status: A.** `GROK_STATUS_ANSWER.md`. DOM
  badge outside the canvas, one line "Reconnecting…", 4s / 6 failures,
  hide on the next success. Observe only. No extra clear-delay. Machine
  in `js/net.js`. Client only after review. Not built.
- **2026-09-19 — SSG crib, no generator.** `ssgs.md`: Hugo / Eleventy /
  Astro / Vite would be a compile step. The Commons client is already
  static files on nginx; civic is request-time C++. Do not add `_site/`.
  No code change.
- **2026-09-19 — split client live, Grok independent check.** Claude's
  two-phase deploy accepted. From here, no SSH: `/commons/` is the 41-line
  shell (0 inline, 8 classic `js/*.js` tags + `style.css`), all eight
  scripts 200 `application/javascript` no-cache, stylesheet `text/css`,
  `tiles.png`/`sprites.png` 200. Civic `/` 200. Public `/internal/friends`
  and `/internal/session-user` still 404. `/commons/presence/1,1` is `[]`
  JSON. Daemon was not restarted (Claude: up since 17:23:28 UTC,
  NRestarts=0). Rollback still the old one-file page if a visitor somehow
  can't load a script. `/commons/recipes` 404 stays leave-alone.
- **2026-09-19 — notes.md restored.** Grok commit `909044f` accidentally
  overwrote this file with a placeholder while adding `makefile.md`.
  Restored byte-for-byte from `da2f8d0` (the split review), then these
  two bullets. Nothing else in the pad was meant to change.
- **2026-09-19 — Makefile crib, no Makefile.** `makefile.md`: CMake stays
  the daemon build. Do not add a Makefile that lists `.cpp` files. A
  five-line wrapper that only calls cmake is the most that would ever
  be allowed. Client is still static files. No code change.
- **2026-09-17 — two-rooms-at-once live check, no bug, no code.**
  `GROK_V3_ANSWER.md`: presence vein is spent (indoors-vanishing was the
  hole, closed), most-recent-placeable stays as shipped, cookie bind is
  the only real remaining increment. Asked for exactly one thing before
  holding: five minutes of live verification that two shared interior
  instances occupied at once don't need a correctness pass, not an
  invented "multi-instance" subsystem. Ran it against the live site
  (`https://liquidgov.us/commons/`, not local dev): tab A joined
  `instance:demo`, tab B joined `instance:lab` -- both held independent
  occupant lists on the daemon's own feed. Tab A walked back outside:
  `instance:demo` went empty (no ghost), `instance:lab` still showed
  tab B, completely unaffected. Tab A's `netRegion` correctly resolved
  back to an outdoor region, and outdoor camera-neighbor polling (two
  simultaneous region GETs near a seam) still fired correctly
  afterward. No console errors either tab. No bug found -- confirms
  `PresenceStore`'s existing "map of opaque region keys" design already
  handles this with zero special-casing, exactly as Grok expected.
  Nothing to patch, nothing committed except this bullet. **Holding
  until Brandon says BUILD and names cookie resolver** -- the first
  `http_server.cpp` edit this entire project would ever make.
- **2026-09-17 — self-nameplate + lab placeables deployed live.**
  Brandon said "deploy it live" -- unlike the shared-interiors "copy"
  round, this one changes the daemon (new `placed.hpp/.cpp`, changed
  `PresenceStore::join`/`rekey` signatures), so it needed the full
  sequence, not a file copy. Found one more real gap before applying
  anything: the live nginx config only proxies `/commons/presence/` --
  the new `/placed/<instance>` daemon route had **no matching location
  at all**, so a request to `/commons/placed/...` would have hit the
  `/commons/` *static alias* and tried to serve a file that doesn't
  exist. Added `location /commons/placed/` to `nginx-commons.conf`
  (same trailing-slash shape as presence, same reasoning) before
  touching the live box. Sequence: synced+rebuilt the daemon
  (`cmake --build -j1` on the real box), restarted the service,
  verified the new routes on loopback first, synced the client, backed
  up the live nginx config again, added the one new location, tested
  with `nginx -t` on disk (no effect on the running process) before
  reloading. Civic stayed 200 throughout, confirmed before, during
  (mid-config-swap), and after. Verified live at
  `https://liquidgov.us/commons/` in a real browser: self-nameplate
  renders over the local player for the first time in production;
  picked up chalk at the lab, placed it in the demo house wall-slot,
  confirmed on the daemon's own `/commons/placed/instance:demo`
  response and visually in a screenshot with the player moved clear of
  it. No console errors. `deploy-plan.md` updated to reflect the new
  proxied route. Left the test placement in place afterward (harmless,
  same as any real guest's) rather than manufacturing a cleanup step
  for low-stakes cosmetic state.
- **2026-09-17 — V2 stretch: self-nameplate + lab placeables (repo
  commit, not yet copied live).** Both remaining "if there's room" items
  from `GROK_V2_ANSWER.md`.
  **Self-nameplate**: `GROK_NAME_REVIEW.md`'s exact ask -- `join`/`rekey`
  now echo the *accepted* `display_name` (post-disambiguation) instead
  of the client trusting its own raw typed input. Daemon:
  `PresenceStore::join`/`rekey` take an optional `std::string*
  out_display_name`, populated from the same `disambiguate_name_locked`
  call that was already computing it internally -- no new logic, just
  exposing what was already being thrown away. Client: `netJoin`/
  `netRekey` capture it into `acceptedDisplayName`, drawn over the local
  player's own sprite the same way other occupants' nameplates already
  render. No nameplate draws until the first join response actually
  lands (never shows the raw typed name as a stand-in).
  **Lab placeables**: one placed item per shared interior instance, RAM
  only (new tiny `PlacedStore`, same shape as `GuestRegistry` --
  mutex-guarded map, matching this codebase's existing convention even
  though the single-threaded server doesn't strictly need the lock).
  `GET`/`POST /placed/<instance>`; POST requires a valid guest credential
  (griefing bar, not a security boundary) and validates the item slug is
  a real row in the `items` catalog table (a client that doesn't
  recognize an unknown slug can't happen, and a typo doesn't wedge the
  one shared slot with something nothing can render) -- explicitly not
  the same thing as `items.wall_slot_ok`, which is reserved for the
  real, later civic profile-canvas wall slot; this is "not a civic PNG"
  per the spec, any catalog item is eligible here. **Found and fixed a
  real bug while wiring this, not by inspection**: `LAB_RECIPES.item`
  held display-style strings ("copper acetate bottle", spaces) instead
  of the real catalog slugs ("copper_acetate_bottle", underscores) --
  harmless as long as it was only ever used as a local JS object key,
  but every `POST /placed/` call would have 400'd against the real
  `items.slug` column. Fixed at the source since nothing displays
  `.item` as text (confirmed by grep before changing it) -- one
  representation, not a translation table between two schemes. Demo
  house's wall-slot tile (previously pure decoration, explicitly "stays
  empty until glue exists" per its own P2-era lock) is now interactable
  -- placing there sends whatever item was most recently picked up at a
  lab station (no inventory-picker UI exists, so "most recent" is the
  only unambiguous choice); this reuses the exact tile position for a
  different, simpler, explicitly-authorized purpose than the real
  civic wall-slot feature (`GROK_V2_ANSWER.md`'s own "not a civic PNG"
  distinction), which stays exactly as parked as before.
  Verified live across two independent browser tabs: self-nameplate
  renders correctly for both the local player and (via the existing
  other-occupant path) a second guest; picked up chalk at the lab
  station with the real slug recorded; placed it in the demo house
  wall-slot; a second guest's tab, joining fresh, saw the exact same
  placed item rendered (flat color + label) with no code shared beyond
  the poll; placement persisted across an exit-and-re-enter (proving
  it's real daemon-side RAM state, not client-local); restart boundary
  re-verified for both `PresenceStore` and the new `PlacedStore`
  (RAM clears, SQLite catalogs survive); full regression suite (missing
  field 400, wrong-secret 403) still holds. Not yet copied to
  `/opt/commons/index.html` or rebuilt on the droplet -- repo commit
  only, same as shared interiors was before "copy." Civic untouched, no
  deploy, no nginx.
- **2026-09-17 — shared interiors copied live.** Brandon said **copy**
  after `GROK_V2_REVIEW.md` accepted `6ce42cf` with two checks named
  before doing it: passerby must not draw/walk indoors, and the
  colon-containing instance path (`/commons/presence/instance:demo`)
  must resolve through the production proxy as JSON, not a civic HTML
  404. Verified both live (screenshot inside the demo house shows only
  the local player; the colon path returns `[]`/occupants correctly)
  before and after the copy, not just trusted from the local round's
  own testing. Ran `commons-daemon/deploy/sync-client-to-droplet.sh`
  (client only, `/opt/commons/index.html`) -- no daemon rebuild, no
  nginx change, exactly as scoped. Fixed ownership after rsync (root
  by default, `chown liquidgov:liquidgov`). Verified end-to-end in a
  real browser at `https://liquidgov.us/commons/`: entered the demo
  house, `netRegion` resolved to `"instance:demo"`, and the daemon's
  own feed showed the exact same occupant. Daemon (`commons-daemon`,
  still active) and civic (still 200) both unaffected throughout.
  Self-nameplate, lab placeables, Mine/Workshop/Hall, cookie bind, and
  wall-slot all still parked, exactly where they were.
- **2026-09-17 — V2: shared interiors (BUILD).** `GROK_V2_ANSWER.md`:
  V1's actual hole wasn't a 13th region, it was that "indoors is a
  single-player cutscene" — the client called `netLeave()` on every
  house/lab entry, so real presence was wasted the instant two guests
  would have been in the same room. Mine/Workshop/Hall stay shut (each
  needs a different *kind* of feature, not "one more 16x16") — this
  round only networks the two interiors that already exist, as one
  **shared** instance each (`instance:demo`, `instance:lab`), not
  per-player houses. No daemon changes at all: `PresenceStore` already
  treats region as an opaque string key, so an instance key is just
  another region to it, same 2Hz/secret/24-cap.
  Client refactor: generalized `regionKeyFor`/`worldToRegionLocal`/
  `regionLocalToWorld` to accept either an outdoor `REGIONS` entry or a
  plain instance-key string (interior local coords need no supermap
  AABB offset — the instance's own tile space IS the whole region);
  unified `netJoinOutdoor`/`netRekeyOutdoor`/`netMoveOutdoor` into
  plain `netJoin`/`netRekey`/`netMove` that work for both; added
  `netPollCurrentRegion` (single-region poll, no camera-neighbor union
  needed since there's only ever one instance to be in); frame()'s
  network tick now runs the same join-on-null/rekey-on-mismatch pattern
  for whichever `mode` is active, indoors or out, so `enterInterior()`/
  `exitToOutdoor()` need **zero** explicit network calls — the very
  next tick detects the region mismatch and rekeys automatically, the
  same mechanism that already handled outdoor-to-outdoor crossings.
  `netHandleAuthFailure`'s remint path simplified the same way (resets
  `netRegion` to null, the natural tick picks up wherever the player
  actually is). Rendering generalized to fire for either mode.
  Verified live: entering the demo house rekeys clean (no ghost in the
  outdoor region, correct region-local coords in the daemon's feed); a
  second tab entering the same house sees the first guest as a real
  occupant (the actual hole, closed); exiting rekeys back outdoors with
  no ghost in the instance; the lab's walk-in door trigger (not just
  the demo house's Enter-to-exit) also rekeys correctly; outdoor
  camera-neighbor polling re-verified unaffected by the refactor; no
  console errors either tab. Not deployed to the live droplet yet —
  this is a repo commit only, separate explicit go-ahead needed before
  touching `/opt/commons/index.html` again. Civic untouched, no deploy,
  no nginx.
- **2026-09-17 — decided: world-reachable guest mint is the product, not
  a hole.** Independently confirmed by Grok (`GROK_LIVE_CHECK.md`, no
  SSH) after the deploy: civic still 200, `/commons/` shows the real
  client, `GET /commons/presence/1,1` returns `[]` (JSON, proving the
  slash rewrite holds in front of a second independent check, not just
  Claude's own). Explicit product decision, not an open question:
  anyone reaching `POST /commons/presence/guest` and joining presence
  (bounded by the existing 24/region, 96/global caps) is the intended
  anonymous-visiting design, same as walking into the Square in person
  — do **not** "fix" this later by gating it behind a real LiquidGov
  account or opening LGP1 from the daemon. Cookie bind (a *promotion*
  once it exists, per `presence.md`/`cookie-bind.md`) stays the next
  queued item, not triggered yet. `/opt/liquidgov-us` (the leftover
  clone found during deploy) stays as-is — leave it unless deliberately
  deleted on purpose. No further deploy this hour.
- **2026-09-17 — commons-daemon is live on the droplet.** Brandon said
  "go" after `c024c27`'s deploy-preflight fixes. Ran the full
  `Persistence/TheCommons/deploy-plan.md` sequence for real against
  `143.198.159.58` for the first time in this project's history:
  synced daemon source (`schema.sql` landed as a real dereferenced
  file, confirmed, not a dangling symlink) and the client separately,
  built with `cmake --build -j1` on the actual 512MB box (`FetchContent`
  reached sqlite.org fine, no tarball fallback needed), installed
  `commons-daemon.service` (`active (running)`, ~1-2MB RSS, well under
  the 64M cap), verified the daemon on loopback *before* touching
  nginx, backed up the live nginx config, edited in the three
  `/commons` locations, tested on disk with `nginx -t` (which does not
  touch the running process) before reloading, then reloaded. Civic
  stayed `200` the entire time, never interrupted. Verified end-to-end
  including **a real browser loading `https://liquidgov.us/commons/`
  for the first time ever** — minted a real credential, joined
  presence, and the daemon's own feed showed the exact same occupant.
  Full join/move/leave/wrong-secret-403 lifecycle re-verified over real
  HTTPS through the proxy afterward. `deploy-plan.md` updated in place
  to record what actually happened rather than staying a hypothetical.
  **New, unexpected, non-blocking finding from actually being on the
  box:** `/opt/liquidgov-us` exists — a full git clone of this repo
  (`origin` remote, at `4d38c92`), separate from `/opt/liquidgov` (the
  real running app, confirmed via the systemd unit and the live
  process's own cwd). Not touched, not part of this deploy, flagged for
  Brandon since it was genuinely unknown before today (possibly a
  staging/migration experiment from another session) — worth him
  knowing what's actually sitting on his own droplet.
- **2026-09-17 — commons: deploy preflight, two real holes closed, still
  no SSH.** `GROK_DEPLOY_REVIEW.md`/`CLAUDE_DEPLOY_PREFLIGHT.md` on
  `fb889c0`: accepted the slash fix, found two more real problems before
  anything could actually go live. Both fixed as files only:
  1. **`deploy-plan.md`'s step 4 named a path that doesn't exist** --
     `cp /opt/commons-daemon/commons/index.html` assumed `commons/` was
     inside `commons-daemon/`; it's a sibling directory at the repo
     root. Added `commons-daemon/deploy/sync-client-to-droplet.sh` (new
     script, rsyncs the actual `commons/` dir to `/opt/commons/`,
     separate from the daemon's own sync script so the nginx static
     alias directory never risks getting the daemon's source/build tree
     dumped into it) and fixed `deploy-plan.md`'s sequence to call it.
  2. **The shipped client would have talked to the *visitor's* loopback,
     not the droplet.** `DAEMON` was unconditionally
     `http://127.0.0.1:8765` (or a loopback-only `?daemon=` override) --
     curl on the droplet could prove nginx worked, but no real browser
     ever could have. Fixed: off loopback, `DAEMON` is now
     `location.origin + "/commons"`; every existing call site already
     builds URLs as `DAEMON + "/presence/..."`, so this composes
     correctly with the nginx proxy with zero other client changes
     (`https://liquidgov.us/commons` + `/presence/join` is exactly what
     `location /commons/presence/` matches). Loopback behavior
     (`?daemon=` allowlist, local dev default) is untouched. Verified
     live: default loopback join still works exactly as before; the
     production string composition checked directly
     (`https://liquidgov.us/commons/presence/join`,
     `.../presence/1,1`) since there's no live non-loopback deploy to
     load the page from yet.
  Also did the two optional cleanups `CLAUDE_DEPLOY_PREFLIGHT.md` named:
  a `location = /commons { return 301 /commons/; }` redirect in
  `nginx-commons.conf` (bare no-slash URL used to fall through to
  civic's own `location /` and 404 there — confusing to debug blind),
  and patched `nginx.md` itself so its own presence-location example
  no longer advertises the double-slash 404 shape or the `8091`
  placeholder port (this daemon's actual coded default is `8765`) to
  whoever pastes from it next. Still no SSH, no droplet contact, no
  `nginx -t` against anything real — waiting on Brandon's "go."
- **2026-09-17 — commons: sibling deploy staged, nothing applied.**
  "Let's move forward" → confirmed the direction was sibling deploy →
  confirmed the mode was "stage first, apply after review" before
  touching anything, given this is the first round in the whole project
  to go anywhere near the actual droplet. Staged, not run:
  `commons-daemon/deploy/commons-daemon.service` (systemd unit --
  `User=liquidgov`, loopback-only via `IPAddressAllow`/`Deny`,
  `MemoryMax=64M`, `Restart=on-failure`, no `After=`/`Requires=`
  coupling to civic's own unit either direction), `nginx-commons.conf`
  (the two `location` blocks per `nginx.md`'s shape), and
  `deploy/sync-to-droplet.sh` (rsync, modeled on civic's own script,
  targeting the sibling path `/opt/commons-daemon`, `-L` to dereference
  the `schema.sql` symlink so it doesn't arrive on the droplet as a
  dangling path string -- exactly the caveat `GROK_DAEMON_REVIEW.md`
  already flagged). Full sequence, pre-flight checks (droplet outbound
  internet for `FetchContent`, disk/RAM for a `-j1` build), and rollback
  written up in `Persistence/TheCommons/deploy-plan.md`.
  **Real bug found while deriving the nginx config, not copied blind**:
  `nginx.md`'s own example (`location /commons/presence` without a
  trailing slash + `proxy_pass .../` with one) sends
  `/commons/presence/join` upstream as `//join`, not `/presence/join` --
  traced through nginx's documented rewrite rule, not guessed. Every
  presence call through that shape would 404 against this daemon's
  exact-match routes, never caught because no nginx config for this
  project has ever actually been installed anywhere. Fixed by matching
  trailing slashes on both sides (`location /commons/presence/` +
  `proxy_pass .../presence/`), nginx's own documented safe pattern.
  Flagged explicitly in both the config file's header and
  `deploy-plan.md` rather than silently deviating from the accepted
  spec. Explicitly NOT done this round (listed in `deploy-plan.md`):
  wiring `commons/index.html` itself to call the same-origin path
  (still loopback/`?daemon=` only), cookie bind (still blocked on a
  civic-side change per `cookie-bind.md`), and proxying `/health`,
  `/items`, `/recipes`, `/maps` (the client never calls them over the
  network today). No SSH, no droplet contact, no `nginx -t` run against
  anything real. Civic files untouched.
- **2026-09-17 — commons: cookie bind, design-only (`cookie-bind.md`), no
  code.** "Should-fix cookie bind next" was flagged before doing anything
  else this round: unlike every prior should-fix, this one is
  structurally blocked by *two* of the project's hardest standing rules
  at once, not just deferred by choice. `lg_session` is `HttpOnly` +
  host-only with no `Domain=` (`cookie.md`) so the browser will never
  send it to the daemon's current, different origin without the
  same-origin nginx round; and resolving the token to an account needs
  `account_from_req()`'s chain, which lives entirely inside
  `http_server.cpp` and isn't callable from outside it without a civic
  change (`GROK_GLUE_REVIEW.md` already parks exactly this as "not this
  week"). Asked which slice was actually wanted rather than guess; got
  design/document only. Wrote `Persistence/TheCommons/cookie-bind.md`:
  the presence-specific contract that `glue.md`/`cookie.md` don't cover
  -- a bound occupant's id is `"u:" + lg_username` (already the exact
  scheme `presence.hpp`'s own `Occupant::id` comment named, written
  before this doc existed), authenticated per-request by a *resolved
  cookie*, never a client-supplied `id`, and never mixed with this
  session's guest `(id, secret)` scheme for the same request; promotion
  is meant to relabel an active guest's row in place per `presence.md`,
  but no `PresenceStore` primitive does that today -- flagged as an open
  design question (a new `rebind()` vs. an unobservably-fast leave+join)
  rather than picked. No code changed. Civic untouched, no deploy, no
  nginx.
- **2026-09-17 — commons-daemon: CORS echoes a specific Origin, no more
  wildcard (should-fix, closed).** `cors-preflight.md`'s "What `*`
  forbids later": a credentialed request (`credentials: include`, a
  forwarded `lg_session` cookie) can never use `Allow-Origin: *` --
  browsers hide the response outright. Built that target shape now,
  ahead of glue/cookie-forward actually existing, since nothing about it
  is unsafe to ship early: this daemon has no cookie of its own and the
  client never sets `credentials: include` today, so
  `Allow-Credentials: true` is inert until something actually needs it,
  not a new hole. `http.cpp`: `cors_header_for(origin_of(head))` computed
  once per request --
  - Trusted (loopback) `Origin` -> that exact value echoed back plus
    `Access-Control-Allow-Credentials: true`. Never `*` + Credentials
    together (invalid, browsers reject it) -- this is why the header
    block only exists in the "echo one exact origin" shape, no wildcard
    path left in the code at all.
  - Untrusted `Origin` (anything not loopback, including literal `null`
    from a `file://` page) -> no CORS header at all. Fails closed;
    `Origin` is never reflected blindly.
  - No `Origin` header (curl, same-origin, non-browser callers) -> no
    CORS header either -- harmless, browsers only consult it for an
    actual cross-origin fetch.
  Deliberately narrower than the client's own `?daemon=` allowlist
  (`4e9bfbd`, which also trusts RFC1918 LAN boxes): that param decides
  which *backend* the client will talk to, this decides which *other
  pages* get to read this daemon's responses, so it stays loopback-only
  per the review's literal "loopback list now; `https://liquidgov.us`
  later" -- the production origin gets added at the actual deploy round,
  not now. Verified live: curl with `Origin: http://localhost:8090` and
  `http://127.0.0.1:8090` both get the header echoed + Credentials;
  `https://evil.example`, literal `null`, and no-Origin-at-all all get
  no CORS header, checked on the normal response path, the `OPTIONS`
  preflight, and the 413 path; the real client in a real browser (not
  just curl) still joins/polls normally against the new header shape.
  Full regression (catalogs, missing-field 400, wrong-secret 403) holds.
  Civic untouched, no deploy.
- **2026-09-17 — commons: allowlist `?daemon=` before this file can ever
  be public (should-fix, closed).** `GROK_DAEMON_URL_REVIEW.md` flagged
  the override added in `92daa22`: a shared link with
  `?daemon=https://evil.example` would make the page mint a guest secret
  and POST it straight to the attacker, harmless only because the page
  is loopback-only today. Gated it *now*, before any deploy, per the
  review's exact spec:
  - `?daemon=` is honored at all only while the page itself is being
    viewed from loopback (`127.0.0.1`/`localhost`/`::1`) -- on a real
    deploy the param is ignored outright, not just untrusted *values* of
    it, since the eventual fix there is the relative proxy, not this
    param.
  - Even then, the value must parse as a `URL` with `protocol === "http:"`
    and a host that's loopback or RFC1918 private (`10.*`,
    `172.16-31.*`, `192.168.*`) -- no `https:`, no `javascript:`, no
    arbitrary DNS name.
  - Anything that fails either check falls back to the same
    `http://127.0.0.1:8765` default as before; nothing untrusted is ever
    used.
  Verified live: default unaffected (real join/poll still works exactly
  as before); a trusted loopback override (`127.0.0.1:9999`) is honored;
  `https://evil.example`, `http://evil.example` (untrusted DNS, even
  over plain http), and `javascript:alert(1)` are all rejected and fall
  back to the default; the RFC1918 boundary itself checked directly
  (`172.15.*` rejected, `172.16.*`/`172.31.*` accepted, `192.169.*`
  rejected, `192.168.*` accepted). Civic untouched, no deploy. The
  daemon's own `Access-Control-Allow-Origin: *` stays parked and
  untouched per `cors-preflight.md` -- that's legal today only because
  the guest secret travels in the JSON body, not a cookie, and becomes
  the actual should-fix once glue/cookie-forward exists.
- **2026-09-17 — commons: `?daemon=` override for the hardcoded loopback
  host (should-fix, closed as far as it can go without nginx).**
  `GROK_PRESENCE_REVIEW.md`: "`NET` host is loopback `:8765`. Grok
  preview and `liquidgov.us` will not see WSL. Relative `/commons/`
  proxy is the nginx round." That's true regardless of how the constant
  is spelled -- no client-side change makes a remote preview sandbox or
  a real visitor's browser able to reach a developer's own WSL loopback.
  The actual fix (same-origin relative path, nginx proxying it to the
  daemon) stays explicitly out of scope: no nginx, no deploy, this round.
  What's cheap and real: `DAEMON` now reads an optional `?daemon=` query
  param before falling back to the same `http://127.0.0.1:8765` default
  as before, so pointing the client at a different reachable daemon (a
  LAN IP, a tunnel, another dev box) is a URL, not a source edit.
  Verified live: default (no param) still joins/polls the real daemon
  exactly as before; `?daemon=http://example.invalid:9999` correctly
  overrides `DAEMON` and degrades to the local-only passerby simulation
  with no thrown error (same graceful-offline path as an unreachable
  default daemon). Also folded in Grok's correction from the
  `display_name` round (`GROK_NAME_REVIEW.md`, `f2132ad`): there is no
  self-nameplate today (checked `fillText` call sites directly -- only
  the passerby and other-occupant labels exist), so nothing to fix there
  yet; noted for whenever one lands, `join`/`rekey` should echo back the
  *accepted* `display_name` so a self-label draws the disambiguated
  string, not the raw typed input. CORS `*` still parked. No deploy.
- **2026-09-17 — commons: region-local display_name disambiguation
  (should-fix, closed).** `GROK_PRESENCE_REVIEW.md`: "Row hijack is
  closed; name spoof is not. Fine until cookie bind." Guests have no
  bound identity yet (no `lg_username` until glue lands), so this doesn't
  try to reserve a name globally or across time -- `display_name` is
  still whatever a guest types, and reusing an old name after everyone's
  gone is fine, by design ("nothing is saved"). What it does guarantee
  now: no two *simultaneously present* occupants in the same region share
  a `display_name`. `PresenceStore::disambiguate_name_locked` (daemon
  only, `presence.hpp`/`.cpp`) checks the destination region's current
  occupants before every insert in `join_locked` (covers both `join` and
  `rekey`, its only caller) and appends " (2)", " (3)", etc. until unique,
  excluding the caller's own row so a returning guest never collides with
  itself. `move` never touches `display_name`, so this only needed to
  land in one place. No client change needed: the client always resends
  its original chosen name on join/rekey, and the daemon re-disambiguates
  fresh against whichever region it's actually entering each time.
  Verified live: two guests both joining as "Ash" in the same region get
  "Ash" and "Ash (2)"; a third guest naming themselves "Ash" in a
  *different* region is untouched (no false global collision); rekeying
  into a region that already has "Ash" and "Ash (2)" correctly lands on
  "Ash (3)"; `move` leaves an already-disambiguated name alone. Full
  regression (catalogs, 413 cap, missing-field 400, wrong-secret 403,
  restart boundary) still holds. Civic untouched, no deploy. Found and
  fixed an unrelated test-hygiene issue while verifying this: a browser
  tab left open from an earlier round kept running its game loop in the
  background against the live daemon across a `preview_stop` (which only
  tears down the static file server, not an already-loaded page's JS) --
  closing stray tabs matters, not just stopping the dev server, when
  actually verifying a "fresh restart."
- **2026-09-17 — commons: camera-neighbor region polling (should-fix, closed).**
  `70bc5b8` shipped presence networking as a stated V1 simplification:
  only the player's own joined region was polled/rendered, not every
  region whose AABB intersects the camera (`presence.md`'s literal
  tick-rate spec, and the exact FireRed seam failure visiting is meant to
  beat). Closed it: `regionsIntersectingCamera(camX, camY)` walks all 12
  `REGIONS` and keeps whichever ones' outdoor tile AABB overlaps the
  current viewport (typically 1-2 on this phone-sized 160x240 viewport,
  presence.md's own worked example says 1-4); `netPollRegions()` fires a
  `GET /presence/<region>` per intersecting region in parallel (one
  region's fetch failing doesn't drop every other region's occupants) and
  merges the results, tagging each occupant with which region it came
  from so `regionLocalToWorld` converts correctly at render time.
  `netRegion` still means exactly one thing -- which region the player's
  own row is joined/rekeyed into -- polling breadth is a separate, wider
  concern layered on top, not a redefinition. Also hoisted the camera
  (camX/camY) computation to the top of `frame()` so the networking tick
  and the draw call share one computation instead of two. Verified live:
  moved a tab near the Square/Lookout seam and confirmed two simultaneous
  `GET /presence/1,0` + `GET /presence/1,1` polls fire per tick; placed a
  real occupant in the neighboring region via curl and watched it render
  correctly past the seam in the browser (screenshot-confirmed, not just
  a network-log check); moving back to the region center correctly drops
  it from `otherOccupants` again. Civic untouched, no SSH/systemd/nginx,
  no deploy. Remaining should-fix items from `GROK_PRESENCE_REVIEW.md`
  (`display_name` spoofing, CORS `*`, hardcoded loopback host) still
  parked.
- **2026-09-17 — commons: guest credentials survive an interior stay.**
  Grok caught a real must-fix in `70bc5b8`'s presence networking
  (`GROK_PRESENCE_REVIEW.md`/`CLAUDE_PRESENCE_PATCH.md`): `GuestRegistry`
  reaped credentials on the same 30s clock as `PresenceStore`'s plaza-slot
  reap, but a guest in a house/lab stops heartbeating presence entirely
  (`netLeave()` on entry, nothing calls `verify()` again until they walk
  back out). Any interior stay over 30s meant the credential died while
  the guest was still very much active, and exiting got a silent 403 with
  no way back in. Fixed both ways the patch asked for:
  1. **Daemon**: `GuestRegistry::kIdleMs` raised from 30s to 30 minutes
     (`guest.hpp`) -- `PresenceStore`'s own 30s plaza-slot reap is
     untouched, so a crashed/closed tab still frees its region slot fast;
     only the credential's own lifetime changed.
  2. **Client**: `join`/`move`/`rekey` now check for a 403 response and
     call a new `netHandleAuthFailure()` -- mints a fresh credential and
     rejoins from the player's current outdoor position, guarded by
     `netReauthInFlight` so one bad response can't fire overlapping
     remints. This also covers a daemon restart mid-session, which the
     30-minute window alone can't (RAM state is gone either way).
  Verified live: entered the demo house, waited 32s (past the *old* 30s
  window), walked out -- same guest id, clean `200` on rejoin (not a
  remint), daemon confirms the occupant reappeared. Separately killed and
  restarted the daemon while a tab sat outdoors (wipes `GuestRegistry`
  even with the 30-minute window) -- the next throttled move/poll hit a
  403, `netId` changed (real remint), rejoin succeeded, no thrown error,
  two-tab presence still worked afterward. Full regression set (catalogs,
  413 cap, missing-field 400, wrong-secret 403) still holds. Should-fix
  items explicitly left for later per the review: camera-neighbor
  polling, `display_name` spoofing, CORS `*` (fine until `lg_session` is
  ever forwarded), hardcoded loopback host. Civic untouched, no SSH/
  systemd/nginx, no deploy.
- **2026-09-16 — commons real presence networking + server-issued guest ids.**
  Brandon asked for a client-side round; found first that `commons/index.html`
  had zero daemon wiring at all (no `fetch` calls anywhere -- presence was a
  pure local simulation, `otherRealOccupantsInRegion` was a hardcoded 0).
  Rescoped with Brandon to full presence wiring, id-first, rather than
  building an inert guest-id endpoint nothing would call. Landed both sides:
  - **Daemon**: new `GuestRegistry` (`src/guest.hpp/.cpp`) mints a fresh,
    unguessable `(id, secret)` pair via `POST /presence/guest` --
    `id` is the public occupant label already broadcast by every
    `/presence/<region>` GET; `secret` is a private bearer credential
    returned only once and never echoed back. `join/move/rekey/leave` now
    all require a matching `secret`, constant-time compared, closing a real
    (not just hypothetical) hijack: any client in a region can already see
    every occupant's `id` from the GET feed, so before this a client could
    replay someone else's `id` in a `join` call and steal/rename their row
    outright -- confirmed live (wrong secret -> 403, occupant row
    untouched; unknown id -> 403; missing secret -> 400 same as any other
    missing field). Unclaimed credentials expire after 30s
    (`GuestRegistry`'s own idle window, same shape as `PresenceStore`'s).
    Also added CORS (`Access-Control-Allow-Origin: *` + an `OPTIONS`
    preflight handler in `http.cpp`) -- needed the moment any browser page
    calls this loopback daemon cross-origin, which nothing did before now.
  - **Client**: `commons/index.html` now really talks to the daemon.
    `enter()` mints a credential; the outdoor frame loop joins on first
    tick, rekeys on crossing an outdoor AABB (no raw leave+join, matching
    presence.md's "no ghost in the old list"), and throttles move+poll to
    ~2Hz (presence.md's tick rate) rather than every render frame.
    `otherRealOccupantsInRegion` now reflects a real poll instead of a
    hardcoded 0, so the passerby's hide-rule reacts to actual other
    players for the first time. Entering a house/lab calls a clean
    `netLeave()` (interiors use `instance:<id>` keys presence.md says V1
    doesn't wire for guests); exiting rejoins automatically via the same
    join-on-null-region path used at login, no separate code path needed.
    `x,y` sent as fractional region-local tile coordinates (not integers,
    not world-absolute) -- satisfies presence.md's literal "tile
    coordinates local to that region" while keeping sub-tile smoothness.
    **Known V1 simplification, stated not hidden**: only the player's own
    current region is polled/rendered, not every region whose AABB
    intersects the camera (presence.md's actual tick-rate spec) -- proves
    real multiplayer works, but a neighbor just past the camera edge won't
    show yet. Remote occupants render at a static standing frame (no
    animFrame in the occupant feed); smoothing between polls is a later
    nice-to-have.
  - **Bug found and fixed during live testing, not by inspection**:
    `netLeave()` first used `navigator.sendBeacon` with a JSON `Blob` body.
    That's cross-origin-preflighted, and the actual POST failed
    (`net::ERR_FAILED`) against this daemon even though the `OPTIONS`
    preflight itself succeeded -- and `sendBeacon()` still returned `true`
    regardless (that return value means "queued," not "delivered"), so the
    `if (!sent) fetch(...)` fallback never fired and a house/lab entry
    silently never told the daemon to leave. Switched to plain
    `fetch(..., {keepalive:true})`, already proven working for every other
    call; interior-entry leave now verified reaching the daemon (200 OK).
    A genuinely undelivered leave (e.g. an abruptly closed tab, tested via
    closing the tab outright) still isn't guaranteed -- no browser
    guarantees that -- but `PresenceStore`'s existing 30s idle reap is the
    correct backstop and was reconfirmed cleaning up that exact case live.
  Verified end-to-end in two real browser tabs against the live daemon
  (not just curl): two guests join, see each other by name (not the
  passerby), region-crossing rekeys with no ghost left in the old region,
  interior entry/exit leave-and-rejoin correctly, an unreachable daemon
  degrades to the pre-existing local-only simulation with no thrown
  errors, and the full prior regression set (catalogs, 413 cap, missing-
  field 400, restart boundary) still holds. Added `commons-client` to the
  shared (global, not project-local) `.claude/launch.json` alongside the
  existing `liquidgov-http` entry -- a static `python3 -m http.server` for
  `commons/`, port 8090, `liquidgov-http`'s own entry untouched.
- **2026-09-16 — commons-daemon: should-fix round (reap, move timestamp,
  schema dedup, sqlite via FetchContent).** Brandon scoped this round to
  "should-fix daemon items" only (not client, not sibling/deploy) via
  explicit confirmation. Four of `GROK_DAEMON_REVIEW.md`'s five should-fix
  items landed:
  1. `move` now stamps `last_update_ms` on every call — occupant JSON no
     longer looks frozen between joins.
  2. Idle reap: `PresenceStore::reap_locked` runs under `mu_` at the top
     of every public method (join/move/rekey/leave/occupants/global_count)
     and drops any occupant whose `last_update_ms` is more than 30s
     (`kIdleMs`) stale. A crashed/closed tab no longer holds a cap slot
     until process restart. Single-threaded, lazy-on-access — no timer
     thread, matching the daemon's existing synchronous style.
  3. `schema.sql` dedup: `commons-daemon/schema.sql` is now a symlink to
     `Persistence/TheCommons/schema.sql` (verified content was already
     byte-identical apart from a comment) — one source of truth, no more
     copy-drift risk.
  4. Vendored `sqlite3.c`/`.h` (9.5MB) dropped from git; CMake now
     `FetchContent`s the same 3.53.4 amalgamation from sqlite.org, pinned
     by SHA256 (downloaded once with Brandon's explicit permission to
     compute the hash, content verified byte-identical to the prior
     vendored copy before switching). Real tradeoff accepted: the build
     now needs network access at configure time; it was previously fully
     offline. `db.hpp`'s include path updated to match (`"sqlite3.h"` via
     the target's public include dir, not a relative `third_party/` path).
  **Deliberately not done:** server-issued guest ids (should-fix #3,
  impersonation resistance). This can't be meaningfully half-done
  daemon-side — the server minting an id nobody's client asks for or
  stores achieves nothing, and `commons/index.html` still generates its
  own guest id today. Stays parked for an explicit client-side round
  before nginx is ever in front of this daemon. Verified live: full
  regression set, `move` timestamp advances on move, an untouched
  occupant is gone after >30s while one refreshed via `move` survives,
  clean build from a bare `rm -rf build` (proving FetchContent actually
  works, not just "worked once"), and the restart boundary (SQLite
  catalogs survive, RAM presence + reap state do not). Civic untouched,
  no SSH/systemd/nginx, no deploy.
- **2026-09-16 — commons-daemon: body-cap nit, truncate to `Content-Length`
  after the split.** Grok caught a gap in the 413 patch (`d2b5bf4`): the
  1 MiB header guard bounds `raw`, not this request's declared body — so
  even when `Content-Length` is legal (<=64KiB), the initial `head`/`body`
  split in `serve_one` could already hand back more bytes than `need`
  (whatever trailed the header terminator in the same reads), and nothing
  trimmed it back down. Not the 5GB hole (413 still fires correctly for an
  illegal length), but trailing/pipelined-looking bytes could silently ride
  along as part of this request's body. Fixed: after the read loop,
  `body.resize(need)` whenever `body.size() > need`. Verified live: a
  legal small `Content-Length` with garbage glued on in the same packet
  still joins clean (occupant reflects only the real JSON), the 413
  boundary and normal regression set are unaffected, and a restart still
  shows the RAM/disk boundary holding.
- **2026-09-16 — decided.** GitHub shares `Persistence/notes.md` (+ README) only.
  `context.md` and `state.md` exist in the *old* Grok sandbox and were never
  pushed. Do not invent fakes. Do not have Claude create them. A fresh Grok
  hard-gate that requires those two files will fail until we drop that read or
  Claude commits the *real* files from that sandbox. “Have Claude git pull”
  updates notes/README only.

- **2026-09-16 — commons-daemon: body-cap must-fix patched (`http.cpp`), presence
  bugs already fixed upstream.** Grok's review (`GROK_DAEMON_REVIEW.md`,
  `CLAUDE_DAEMON_PATCH.md`) flagged three must-fixes: (1) duplicate `join`
  double-counting `total_`, (2) `rekey` always decrementing `total_` even
  when `from_region` never held the id, (3) `Content-Length` uncapped past
  the 1 MiB header guard. Before patching, `git pull` surfaced that (1) and
  (2) were already fixed upstream by a newer id-index redesign of
  `presence.cpp`/`presence.hpp` (`92e379d`, `63792ea`: an id-lookup replaces
  the old scan-and-decrement pattern, `rekey`'s `from_region` is now
  advisory only since `join_locked` de-dupes globally regardless) — pulled
  that in rather than reapplying an older, now-superseded fix and clobbering
  it. Only (3) still needed work: `serve_one` now rejects `need < 0` or
  `need > 65536` with a 413 and closes the connection before the body-read
  loop, so a client-declared multi-GB `Content-Length` can never reach
  `body.append` on the 512 MB box. Verified live: duplicate join (same
  region and cross-region) collapses to one occupant; rekey from a region
  the id was never in leaves `global_count()` correct; a 5 GB, a negative,
  and a 65537-byte declared length each get a 413 and the daemon stays up
  and keeps serving normal requests; a full process restart still shows
  SQLite catalogs surviving and RAM presence clearing. No should-fix items
  touched (idle reap, server-issued ids, FetchContent, schema dedup, cookie,
  deploy) — those stay parked. Civic untouched, no SSH/systemd/nginx, no
  deploy.
- **2026-09-16 — Sibling: daemon written and tested locally, not deployed.**
  Brandon confirmed the scope explicitly before any droplet contact:
  write the daemon locally first, zero SSH/systemd/nginx this round.
  New `commons-daemon/` (C++, own CMake build, separate from
  `liquidgov-platform/`): a real SQLite ledger (vendored `sqlite3`
  amalgamation — WSL has the runtime `.so` but no `-dev` headers and no
  sudo to install them; downloaded the official amalgamation from
  sqlite.org with Brandon's explicit permission first, per the file-
  download rule) running `schema.sql`/`seed.sql` (the locked V1 catalog:
  3 lab recipes chalk-only, 12 outdoor regions, house/lab interior
  templates), RAM-only presence (`presence.md`'s shape — keyed by region,
  join/move/rekey/leave, 24/region and 96 global caps), and a minimal
  single-threaded synchronous HTTP server deliberately shaped like
  civic's own rather than a framework. Friends/cookie-forwarding and
  wall-slot fetch are documented (`glue.md`, `cookie.md`,
  `canvas-frame.md`) but **not wired** — no civic route exists to call
  and adding one means touching `http_server.cpp`, out of scope. Tile
  collision/movement stays client-side JS (`wasm.md`) — not duplicated
  here. Verified end-to-end, not just "it compiles": full presence
  lifecycle (join/move/rekey confirmed to leave no ghost/leave), per-
  region and global cap enforcement, a missing-field 400, and — the
  actual point of an integration test, not just a happy path — a real
  process restart: SQLite catalogs survived, RAM presence did not,
  proving the RAM/disk boundary `presence.md` describes rather than
  asserting it. `liquidgov-platform/`/`http_server.cpp`/civic untouched,
  no SSH, no systemd unit, no nginx change, nothing on the droplet.
- **2026-09-16 — Correction: `lg_session` does get `Secure`, conditionally.**
  My own first pass on the cookie (same session, bullet below) grepped
  only the literal `Set-Cookie:` string in `set_cookie()` and reported
  "no `Secure`" as flat fact — missed the very next line,
  `if (g_https) o += "; Secure";` (line 1112), and `clear_cookie()`'s
  matching one. `g_https` is set per-request (line 6824) from
  `X-Forwarded-Proto`, so on the real site behind nginx it's true for
  real traffic and the cookie the browser actually gets **does** carry
  `Secure`. Caught by Grok independently citing
  `deploy/ux-on-droplet.patch` and writing `cookie.md` concurrently — a
  real overlap this time, not just a doc-only commit alongside — merged
  by rechecking the source directly rather than taking either version on
  trust, confirming Grok's finding first-hand before writing it into
  `glue.md`. Lesson for next time reading this file cold: a `+= "..."` or
  a conditional two lines past the literal string you grepped for is easy
  to miss with a narrow grep; read the whole function, not just the match
  line. `glue.md`'s host-only/no-`Domain=` finding was independently
  correct and unaffected.
- **2026-09-16 — Glue: cookie and wall-slot are now verified facts.**
  Brandon delegated the choice; picked glue over sibling (order of
  operations — sibling needs glue's cookie name, glue needed nothing
  sibling has). Read the real `http_server.cpp` (WSL copy) and filled in
  `glue.md`'s TBDs with actual findings, no implementation:
  session cookie is `lg_session`, `HttpOnly; Path=/; SameSite=Lax;
  Max-Age=2592000`, no `Secure`, **no `Domain=`** — meaning it's host-only
  today, which resolves the "path vs subdomain" question as a fact rather
  than a preference: path needs zero civic change, subdomain needs one.
  Traced the real cookie→session→account→username chain
  (`account_from_req()`) that any future friends-query endpoint would
  reuse, not invent. Bigger find: the wall-slot's "profile-canvas bitmap"
  doesn't exist as such — the profile page CSS-animates through *frames*,
  and the real per-frame route, `GET /canvas-frame/<username>/<i>`, is
  already live and public today. Only real gap: no existing way to ask
  "how many frames / which is current" from outside — flagged as a small
  addition (e.g. a `/latest` alias), not a blocker. Checksummed the WSL
  copy against the live droplet before writing any of this down
  (`03fd46eb...`, matched exactly) rather than assume it was still
  current — read-only, no deploy, no edit to `http_server.cpp`. Sibling
  still waits on Brandon; nothing in `commons/index.html` changed this
  round.
- **2026-09-16 — Sprites patch: idle-skip sees facing now.** Fixed the
  must-fix from `GROK_SPRITES_REVIEW.md`: `updateFacingAnim` could change
  `player.dir` on a fully blocked step (hold a new direction into a
  wall — `movePlayer` returns `false`), but `frame()`'s idle-skip only
  checked position/toast, so the sprite kept showing the old facing cell
  until the next real step. Added `prevPlayerDir`/`prevPlayerAnim` and the
  same pair for passerby, tracked alongside position exactly like the
  existing prev-frame state, and included both diffs in `changed`. Idle
  skip itself is untouched — still skips the blit when truly nothing
  changed, per canvas.md; it just now correctly counts "started facing a
  different direction" as a change. Verified in-browser: walked the
  player against the solid Homestead wall, tapped a different direction
  into it (movement fully blocked, position unchanged), and confirmed
  both `player.dir` and `prevPlayerDir` updated together — proving the
  redraw branch actually ran, not just that the state variable changed.
  Full regression set still holds (A/D, water, spawn free, dual crop at
  390×844, no console errors). Glue and sibling still wait on Brandon.
- **2026-09-16 — Sprites shipped.** Brandon picked sprites first, lowest
  blast radius per Grok's own suggested order (glue needs a real cookie
  decision, sibling needs glue's cookie name, sprites needs neither).
  `commons/index.html`: player and passerby are no longer black squares.
  One procedurally-baked 16×24 sprite atlas (4 dirs × 2 walk frames, art.md)
  built once at init — same bake-then-blit shape as the tile bake, one
  `drawImage` per actor per frame (canvas.md: no per-frame `fillRect`, no
  save/restore, no `<img>` per direction). Player and passerby share the
  exact same atlas (locked: "online others: same sprite"). Facing follows
  input direction (updates even on a blocked step, so bumping a wall still
  turns you to face it); the walk-cycle leg-swap only advances on an
  actual successful move, resetting to a standing pose when idle or
  blocked. Renamed `passerby.dir` (its patrol ping-pong ±1) to `wpDir` to
  free up `.dir` for sprite facing — same field name, unrelated meaning,
  would have silently clobbered each other otherwise. Sprite atlas context
  is deliberately `{alpha:true}`, the one exception to canvas.md's
  `alpha:false` rule: that rule is for the opaque full-frame bake/display
  layers, and a sprite composited over the world needs real transparency
  or it blits as an opaque box. Verified in-browser: atlas cells visually
  distinct per direction (eyes down, none up, single dot left/right), walk
  frame toggles while moving and resets when stopped/blocked, facing
  updates correctly for all 4 directions, nameplate repositioned above the
  taller sprite's head, full regression set still holds (A/D, water/
  bridge, boarded-door toast, Hall confirm/open, spawn free, dual crop at
  390×844, no console errors). Still no glue, no sibling, no droplet
  contact. `liquidgov-platform/` untouched.
- **2026-09-16 — WebGPU textures crib.** `webgpu-textures.md`: textures
  cannot mapAsync; readback is copyTextureToBuffer. Pixel art uses
  nearest / textureLoad. Commons keeps Canvas 2D drawImage.
- **2026-09-16 — WebGPU mapAsync crib.** `webgpu-mapping.md`: staging
  copy, 8-byte align, buffer cannot be STORAGE and mapped. Why GPU
  collision loses to `solid[i]`. Still no shaders in commons/.
- **2026-09-16 — WGSL crib only.** `Persistence/TheCommons/wgsl.md` is
  syntax, not a renderer. `webgpu.md` still stands: no shaders in
  commons/. Next visible work still sprites / glue / sibling.
- **2026-09-16 — WebGPU is not this game.** `Persistence/TheCommons/webgpu.md`:
  240×160 blit does not want a compute shader. Canvas 2D stays. Sprites /
  glue / sibling still wait on Brandon.
- **2026-09-16 — WASM is not V1.** `Persistence/TheCommons/wasm.md`: no
  compile step in commons/. WASM only earns a slot if a C++ sibling
  daemon shares collision; blit/input stay JS. Sprites/glue/sibling still
  wait on Brandon.
- **2026-09-16 — nginx reverse proxy notes.** `Persistence/TheCommons/nginx.md`:
  static `alias /opt/commons/`; presence `proxy_pass` loopback; forward
  Cookie; buffering off; do not expose the game port or rewrite civic
  locations. Not a BUILD. Sibling still not this week.
- **2026-09-16 — two parked P3 nits closed.** Brandon said "P4" meaning
  "whatever's next," not literally invoking `sibling.md`'s deploy work --
  that doc itself says the sibling is "a new BUILD, not a P4," and "not
  this week," so held off on it rather than guess into a production
  change. Instead closed the two items already parked after P3 review, in
  `commons/index.html`: (1) the interact button now shows
  `nearestInteractable.prompt` (Try/Read/Look/Hall/Enter) instead of
  always saying "Enter" — falls back to "Enter" when nothing's in range
  (the interior-exit case). (2) Lookout's railing moved from y=10 to
  y=16 (plateau y=10-15): the landscape crop is only 10 tiles tall, so a
  camera centered on a player *at* the railing was topping out at y=15 and
  never actually reaching the Square (y=20) — confirmed by computing the
  visible tile range directly (was 9-19, now 11.5-21.5) and by screenshot,
  the Square's notice-post is now visibly in frame from the railing.
  Verified in-browser: both fixes work as intended, full regression set
  still holds (A/D, water, spawn free, no console errors). Still waiting
  on Brandon for a specific glue / sprites / sibling direction before
  starting any of those.
- **2026-09-16 — sibling mechanics.** `Persistence/TheCommons/sibling.md`:
  game daemon + nginx static `/commons/` next to `liquidgov_http` on the
  512 MB box. Presence RAM, SQLite WAL not rsync'd, friends via localhost
  cookie, civic crash-isolated. Path vs subdomain waits on a real
  Set-Cookie read. Not a BUILD. P3 still closed.
- **2026-09-16 — P3 closed.** c996a3d accepted: six quiet regions dressed
  per atlas (woods/lookout/orchard/workshop/garden/quarry). Shutter and
  quarry sign toast only. No new interiors. Civic untouched. Outdoor V1
  DoD met. Parked: interact-button caption; Lookout railing may sit too
  far north to see the Square in the landscape crop.
- **2026-09-16 — IndexedDB is a blob cache.** `Persistence/TheCommons/indexeddb.md`:
  later, tileset/house-snapshot/photo-staging. Not V1 inventory, not
  trades, not friends, not the 5.6 MB bake. Do not open IDB in commons/
  during P0–P3. P2 still closed. P3 waits.
- **2026-09-16 — P3 shipped.** `commons/index.html`: all 12 regions now
  dressed, matching `v1.md`'s full outdoor definition of done. North Woods
  (tree line on the world's north edge, scattered solitary trees, one
  cluster + sit-log), Lookout (a walkable plateau, a solid railing along
  its south edge facing the Square, a bench, one tree off the plateau),
  Orchard (six trees in loose rows), Workshop Lane (closed-shutter facade
  + bench + two crates — shutter interact toasts "Closed for now.", no
  interior, ever, without Brandon reopening that pin), Garden (flower-bed
  cluster + a low sitting wall), Quarry Ridge (boarded adit + sit-rock +
  a sign — only the **sign** is interactable, "Read" → "Closed.", the adit
  itself stays decorative, no ore, no interior, matches the locked pin).
  Same tileset throughout — new tile ids (tree, railing, plateau,
  flower-bed, low-wall, crate, adit) reused across regions per art.md,
  not one-off per-region art. No new doors, no new interiors, no state
  changes beyond two toasts. Verified in-browser: all six regions' set
  pieces render and collide correctly (adit/flowerbed/lowwall/crate/tree
  all solid, plateau walkable, railing solid), shutter and sign toast
  correctly, the adit itself confirmed *not* interactable, and the full
  P0-P2 regression set still holds (A/D, water/bridge, Homestead's
  boarded-door toast, Hall confirm/open, spawn still free, dual crop at
  390×844, no console errors). `liquidgov-platform/` untouched. This
  closes the outdoor world; P2's demo house/lab interiors are unaffected.
- **2026-09-16 — inventory is RAM.** `Persistence/TheCommons/inventory.md`:
  P2 `inventory = {}` vanishing on refresh is the spec, not a bug. Do not
  sessionStorage/localStorage/SQLite in this preview. Write-only object
  may toast a count later; placing items is glue. P2 still closed.
- **2026-09-16 — P2 closed.** e67d88e accepted: Lab Yard walk-in door,
  real 16×16 demo house (empty wall-slot on the wall), 20×16 lab with
  three lookups (chalk, no lime), session RAM inventory, `activeSpace`
  collision. Station interact wins over Enter-to-exit. Civic untouched.
  P3 (six quiet outdoor regions) waits on Brandon. Parked: interact
  button caption stays "Enter" until P3.
- **2026-09-16 — P2 shipped.** `commons/index.html` extended per
  `BUILD_ORDER.md`'s P2 list. Lab Yard dressed outdoors: a wall-tile
  building, a sign, and a Lab door that is a genuine **walk-in trigger**
  (per atlas.md, deliberately not the Enter-to-confirm family the other
  doors use — not solid, not in `interactables`). Demo house's P1 black
  stub replaced with a real 16×16 interior: table, two chairs, rug, and
  an empty wall-slot frame embedded in the wall itself (still empty —
  no glue, per `glue.md`). New 20×16 Lab interior with a workbench visual
  and three lookup stations (copper acetate, iron-gall ink, chalk-only —
  pushback.md's fix, no lime), each showing the real recipe name in a
  toast and recording one pickup in a plain in-memory `inventory` object
  (v1.md: session RAM, vanishes on refresh, no SQLite in this preview).
  Collision/movement/rendering generalized behind a single `activeSpace`
  (outdoor / demoHouse / lab) rather than copy-pasting the outdoor logic
  twice — same 12×12-box/4-corner/axis-split rules from collision.md
  apply unchanged inside either interior, only which tile/solid array and
  pixel bounds count as "the world" changes. Demo house keeps its P1
  Enter-to-enter/Enter-to-exit pattern exactly as shipped; the Lab is
  walk-in both ways, with Enter also working as a fallback exit. Real bug
  caught by testing, not review: `tryInteract()` originally checked
  `mode === "lab"` before checking `nearestInteractable`, so standing
  right next to a lab station and pressing Enter always exited the lab
  instead of triggering the station — no toast ever fired, at any
  distance. Fixed by checking the interactable first; the interior-exit
  fallback only applies when nothing is in range. Also fixed on sight
  (not asked, but the same class of bug as the P0 fountain trap): the
  wall-slot frame was placed one tile off the actual wall, floating on
  the open floor instead of embedded in it. Verified in-browser: Lab door
  auto-warps in and back out (both via walking onto the interior door and
  via the Enter fallback, landing at the intended spot outside each time,
  no oscillation), demo house enter/exit round-trips to the exact saved
  outdoor position, both entry tiles confirmed unblocked
  (`boxBlockedAt` false), all three lab stations produce the correct
  recipe text and inventory entry, and the full P0/P1 regression set
  still holds (A/D not inverted, water solid off the path, bridge still
  crosses it, boarded-door toast, Hall's `confirm()`/`window.open()`
  stubbed and verified, dual crop switches at 390×844, no console
  errors). North Woods, Lookout, Orchard, Workshop Lane, Garden, Quarry
  Ridge remain untouched — P3, not this round. `liquidgov-platform/`
  untouched.
- **2026-09-16 — dt clamp notes.** `Persistence/TheCommons/dt.md`: keep
  1/20; that is what stops tab-return tunneling through 1-tile doors.
  Do not add CCD to satisfy a dt=1 test. P1 still closed. P2 waits.
- **2026-09-16 — P1 closed.** Patch 9048aac accepted: notice-post not
  fountain, passerby visible in the Square (hide seam returns 0), toast
  off-transition redraws, doors solid, benches on the ribbon, waypoints
  on path/bridge, spawn assertion. Civic untouched. P2 waits on Brandon.
- **2026-09-16 — P1 patch shipped.** All items from
  `CLAUDE_P1_PATCH.md`/`GROK_P1_REVIEW.md`/`collision.md` fixed in
  `commons/index.html`: (1) `T_FOUNTAIN` removed entirely, replaced with
  `T_NOTICE_POST` (dark ink-brown, distinct from Hall's brass), still
  offset off the spawn tile. (2) Passerby visibility rewritten: hide is no
  longer "shares a region with the local player" (that hid it at spawn,
  the exact waiting-room look it exists to prevent) but "a second
  non-ambient occupant is in its region" via a named
  `otherRealOccupantsInRegion()` seam that always returns 0 today (no
  multiplayer transport in this preview) — passerby now stays visible at
  spawn, nameplate intact, confirmed visually with player and passerby
  both on-screen in the Square together. (3) Toast off-transition fixed:
  `prevToastActive` tracked alongside the other prev-frame state, so the
  exact frame where `toastActive` flips false forces a redraw instead of
  leaving "Closed." on the bitmap until something else happens to move.
  Should-fix: boarded/demo/Hall doors now all solid (interact still fires
  from an adjacent tile, Chebyshev ≤ 1, doors don't auto-warp on overlap);
  Creek/Shore benches moved off open water onto tiles adjacent to the
  bridge; passerby waypoints recomputed to sit exactly on the carved
  path/bridge tiles (the old last waypoint was plain Shore water, off the
  ribbon). Added a startup assertion (`assertSpawnIsFree`) that
  console.errors if a future prop ever lands back on the spawn tile,
  per `collision.md`'s explicit invariant. Verified in-browser: spawn is
  provably unblocked (`boxBlockedAt` false) and movable in all four
  directions, all three door types solid under realistic per-frame `dt`
  (confirmed tunneling only happens with an unrealistic `dt=1.0` test
  value, never with the game's real `1/20` clamp), water still solid off
  the path, bridge still crosses it, both benches have a walkable adjacent
  tile, dual crop still switches at 390×844, no console errors. No P2.
  `liquidgov-platform/` untouched.
- **2026-09-16 — collision notes.** `Persistence/TheCommons/collision.md`:
  keep 12×12 AABB + axis split; doors/notice-post solid; interact from
  adjacent; never spawn overlapping solid; passerby does not collide, so
  waypoints stay on the ribbon. For the open P1 patch, not a new phase.
- **2026-09-16 — Grok P1 review.** Spine accepted (Hall, 7 doors, bake
  labels, idle skip, hidden watchdog). Must-fix: notice-post not fountain;
  passerby must stay visible in the Square (hide only for a second real
  occupant); toast off-transition must redraw. Should-fix: boarded doors
  solid, benches on the path, passerby stays on the ribbon. Paste
  `CLAUDE_P1_PATCH.md`. P2 not started. Civic frozen.
- **2026-09-16 — P1 shipped.** `commons/index.html` extended per
  `BUILD_ORDER.md`'s spine list: the Square (fountain + 2 benches),
  Homestead Row (facade with 7 doors: door 0 = demo, doors 1-6 = boarded —
  no cookie glue, per `glue.md`), Creek/Shore (bench + recolored
  path-over-water as a "bridge" tile, cosmetic only), Hall Lawn (door +
  2 hedges, opens `liquidgov.us/proposals` in a new tab after a
  `confirm()`). North Woods/Lookout/Orchard/Workshop Lane/Lab Yard/Garden/
  Quarry Ridge untouched — P2/P3, not this round. Passerby implemented per
  the round-2 lock: nameplate exactly `passerby`, `isAmbient` flag
  internal, patrols Square→Homestead→Creek→Shore and back, hides whenever
  its current region (computed from AABB, never stored per-tile) matches
  the player's. One prompt at doors (`Enter`/tap a new on-screen button):
  Hall confirms+opens, the demo door stubs to a black placeholder interior
  ("[Enter] to step back outside"), a boarded door shows a transient
  "Closed." toast. `canvas.md`'s three P1 asks done: region labels are now
  baked into the offscreen canvas once (no more per-frame `fillText`),
  frame() skips the actual blit when nothing changed (player/passerby
  didn't move, crop unchanged, no toast) though rAF keeps ticking, and the
  P0-parked nit is fixed — the watchdog no-ops entirely while
  `document.hidden`, tearing down an already-running fallback too.
  Real bug caught by testing, not review: the fountain's placement formula
  put it on the exact same tile as the player's spawn point, so a fresh
  arrival's own bounding box already overlapped a "solid" tile and every
  direction of movement was silently rejected. Fixed by offsetting the
  fountain 3 tiles south of center. Verified in-browser: A/D still not
  inverted, water still solid off the path, the path/bridge still crosses
  it, dual crop still switches at 390×844, `document.hidden` watchdog
  guard confirmed both for "don't start while hidden" and "tear down an
  active fallback when hidden", demo door enter/exit round-trips, boarded
  door toasts without changing mode, Hall's `confirm()`/`window.open()`
  call verified with both accept and decline (stubbed, so production
  liquidgov.us was never actually opened during the test). No console
  errors. `liquidgov-platform/` untouched.
- **2026-09-16 — canvas 2D notes.** `Persistence/TheCommons/canvas.md`:
  P0 blit is the big win. P1: bake static labels, skip idle frames, one
  sprite atlas, no fillText-per-frame, watchdog respects document.hidden.
  Not a patch to commons/ yet. P1 still waits on Brandon.
- **2026-09-16 — P0 closed.** Patch 0957725 accepted: name field types
  "Brandon", rAF is the loop, interval only as watchdog, alpha:false,
  d-pad tabindex=-1, input.blur() on enter. Civic untouched. P1 not
  started. Parked (not a new patch): watchdog should no-op while
  `document.hidden` so a background tab doesn't start the 60Hz fallback.
  Next work waits on Brandon saying P1 / BUILD.
- **2026-09-16 — P0 patch shipped.** All three items from
  `CLAUDE_P0_PATCH.md`/`GROK_P0_REVIEW.md` fixed in `commons/index.html`:
  (1) keydown/keyup now ignore entirely (no `preventDefault`, no `keys[]`
  update) while `#nameOverlay` is visible or `document.activeElement`/
  `e.target` is an INPUT/TEXTAREA — typed "Brandon" via real keyboard
  input end-to-end and confirmed `nameInput.value === "Brandon"`, then
  confirmed movement worked immediately after submit (added `input.blur()`
  in `enter()` so the now-hidden input doesn't keep eating keys via stale
  focus). (2) `requestAnimationFrame` is the primary loop again
  (`rafLoop`); a `watchdogTick` every 300ms only starts a `setInterval`
  fallback if `lastTickAt` has gone stale, and tears it down if rAF
  resumes — confirmed rAF firing normally in this session's browser pane
  this time (72 callbacks/500ms measured directly) with `usingFallback`
  correctly false throughout. (3) `{alpha:false}` added on both the bake
  and display 2D contexts, `imageSmoothingEnabled=false` added on the bake
  context too (was only on display before), `tabindex="-1"` added to all
  four d-pad buttons. Re-verified after patching: A/D still not inverted,
  dual crop still switches at 390×844, water collision still blocks
  off-path movement, no console errors. Did not touch checker grass,
  Creek/Shore flood-fill, or start P1, per explicit instruction.
  `liquidgov-platform/` untouched.
- **2026-09-16 — Grok P0 review.** Skeleton accepted (bake/blit, dual crop,
  A/D, path ribbon, labels, civic untouched). Must-fix: name input vs WASD
  `preventDefault` ("Brandon" untypeable). Should-fix: rAF as the product
  loop, setInterval only as rAF-fallback; `alpha:false`. P1 not started.
  Paste `CLAUDE_P0_PATCH.md`. See `GROK_P0_REVIEW.md`.
- **2026-09-16 — blit pipeline.** `tilemap.md` expanded: display canvas
  is the logical crop only (not the 1152×1280 bake in the DOM). Floor
  camera, smoothing off, alpha:false, ignore DPR, no worker. Giant-canvas
  + CSS-translate is the fail.
- **2026-09-16 — P0 shipped.** `commons/index.html`: vanilla HTML/canvas,
  one file, no build step. Uint8Array(72×80) tile+collision, baked once to
  an offscreen canvas per `tilemap.md`, blitted per tick — no per-frame
  72×80 loop. Dual crop (240×160 @3× landscape / 160×240 @2× portrait,
  picked by `width >= height`) confirmed switching correctly at a real
  390×844 phone size. A/D and arrows confirmed not inverted (moved player
  in the correct direction, tested via dispatched key events, not just
  read from code). On-screen d-pad confirmed via real pointer events on
  the button elements. Water (Creek/Shore) confirmed solid; the
  Square→Homestead→Creek→Shore path ribbon confirmed walkable straight
  through that same water band. World edge confirmed blocking. All 12
  region names confirmed present and in the correct grid order as small
  world-space ink labels — no HUD chrome. No fountain, no other set piece,
  no doors, no passerby, no SQLite, no cookie, per P0 scope. One
  implementation deviation from `tilemap.md`'s default suggestion: the
  tick loop uses `setInterval`, not `requestAnimationFrame` — a bare rAF
  counter measured 0 callbacks over 500ms in the tool's own browser pane
  even fully visible/focused, so rAF isn't reliable in every environment
  this needs to run in; `setInterval` has no such dependency and the
  movement math was already delta-time based, not frame-count based, so
  nothing else changes. `liquidgov-platform/`, `liquidgov-core/`, LGP1
  untouched. Grok reviews next.
- **2026-09-16 — tilemap notes for P0.** `Persistence/TheCommons/tilemap.md`:
  one Uint8Array 72×80, bake supermap once, blit camera crop. No chunks,
  no WebGL, no 12 canvases. Claude should follow this in `commons/`.
- **2026-09-16 — P0 BUILD assigned to Claude.** Brandon: Grok guides /
  criticizes; Claude implements to save Grok tokens. Paste `CLAUDE_P0.md`.
  Sibling folder `commons/` (vanilla HTML/canvas), not `liquidgov_http`,
  not React. P0 only. Grok reviews after the commit. Civic frozen.
- **2026-09-16 — first BUILD = P0 only.** Brandon chose P0, not P0+P1.
  Empty walkable 72×80, dual crop, d-pad, A/D = left/right, throwaway name,
  spawn Square, region labels, no set pieces, no doors, no passerby.
  P1+ wait for a later BUILD. Still no code until Brandon says BUILD.
- **2026-09-16 — Grok build order.** Answer in
  `Persistence/TheCommons/BUILD_ORDER.md`. Greater map = empty walkable
  72×80 first (one array, not 12 maps). Chunk = a **system**, then a
  **spine** of adjacent regions, not one region. Dual crop + A/D + d-pad
  de-risked in P0. Passerby in P1 after the spine walks, before interiors.
  No V1 SQLite/P2P/glue. Recommended first BUILD: **P0+P1**. P0-only is
  the smaller phone-check slice. Spec not reopened. Still waiting on
  Brandon for BUILD.
- **2026-09-16 — Question posed to Grok: build order.** Spec pack fully
  locked, no open bets. Brandon wants the actual build scoped top-down:
  greater map first, then chunks, then smaller shippable pieces, so BUILD
  doesn't turn into one big-bang implementation. Asked Grok directly (he's
  the one building the client) in
  `Persistence/TheCommons/BUILD_ORDER_QUESTION.md` — what order he'd
  actually build in, what a "chunk" means in his build, where presence/
  `passerby` slots in, and what (if anything) he'd de-risk first. Not a new
  pushback round; spec isn't reopened. Still waiting on Brandon for BUILD.
- **2026-09-16 — Grok accepts passerby.** Ambient walker is locked as a
  **V1-only crutch**: nameplate exactly `passerby` (no unlabeled variant),
  `is_ambient` internally, hide while a real guest is in that region,
  Square → Homestead → Creek → Shore, trivial to delete later. Claude's
  round-2 rewrite stands. No remaining open bets. No game code. Civic
  frozen. BUILD still waits for Brandon.
- **2026-09-16 — Claude pushback round 2, ambient walker.** Grok's four spec
  patches (Square/Lab Yard neighbor fix, camera-overlap presence subscription,
  cookie-scoped friends query, `players` last-position columns) are correct,
  adopted, no further pushback. On Grok's new bet (an ambient walker so a
  solo preview isn't an empty Square): **rewrite, not accept-as-is** — ship
  only the nameplate-reading-"passerby" variant, reject the no-nameplate
  variant, because a real occupant always carries a name label
  (`art.md`) and an unlabeled look-alike would be indistinguishable from an
  actual second visitor — the opposite of this project's honesty pattern
  (boarded mine, closed shutter). Written up in `pushback.md` (Round 2) and
  `v1.md` (Pending, not yet locked). Not locked — awaiting Brandon/Grok to
  close it out. No game code. Civic frozen. BUILD still waits for Brandon.
- **2026-09-16 — Grok accepts Claude’s three overrides.** Now locked as
  implementation: regions **24×20** (supermap 72×80); camera **dual crop**
  240×160 @3× landscape / 160×240 @2× portrait (pick by viewport aspect);
  lab recipe #3 **chalk only, no lime**. Grok also patched (see
  `Persistence/TheCommons/GROK_ANSWER.md`): Square south = Homestead Row
  (atlas typo said Lab Yard); presence clients subscribe to
  camera-overlapping regions; friends query is cookie-scoped (no client
  username param); `players` last-position columns. Ambient walker on
  Square→Shore is a Grok *bet*, not locked. No game code. Civic frozen.
  BUILD still waits for Brandon.
- **2026-09-16 — Claude spec pack landed.** Wrote all seven files in
  `Persistence/TheCommons/`: `atlas.md`, `schema.sql`, `presence.md`,
  `art.md`, `glue.md`, `v1.md`, `pushback.md`. No lock changed — everything
  pinned in this heading (title, region names/order, 3×4 layout, Homestead
  gate, Hall/mine V1 scope) is unchanged. Two of Grok's *unlocked*
  implementation bets in `GROK_BET.md` are overridden in `pushback.md`, not
  silently: region grid 32×32→**24×20** (Grok's own bet flagged this risk
  and offered this exact fallback), and camera flat 240×160→**dual
  landscape/portrait crop** (240×160 desktop, 160×240 phone) so the phone
  case isn't letterboxed/non-integer-scaled. Lab recipe #3 also rewritten
  (drop "lime," keep only "chalk," per the safety-boundary reasoning in
  `pushback.md`) — the other two lookups, the palette, the Homestead cap,
  the Hall/mine V1 scope, and A/D=left/right are all accepted as Grok bet
  them. Ready for Grok to read `pushback.md` and answer.
- **2026-09-16 — Claude spec pack (no game code, civic frozen).** Paste
  `CLAUDE_THE_COMMONS.md` into Claude Code (not `CLAUDE_CODE_CONTINUE.md`).
  He fills `Persistence/TheCommons/` (atlas, schema, presence, art, glue, v1,
  pushback) from Grok bets in `GROK_BET.md`. V1 client stays the Grok preview
  after Brandon says BUILD. Droplet sibling later. Do not touch
  `liquidgov_http` / LGP1.
- **2026-09-16 — locked.** A + continuous outdoor + warp interiors + homestead
  gate. Title **The Commons**. Plaza region **the Square** (not “Commons”).
  Art: GBA / FireRed tiles. No code until Brandon says BUILD.
  - Outdoor 3×4, north up:
    North Woods | Lookout | Orchard
    Workshop Lane | **the Square** | Hall Lawn
    Lab Yard | Homestead Row | Garden
    Quarry Ridge | Creek | Shore
  - Interiors extra: per-player house, lab, hall (thin), mine (boarded / not present).
  - Homestead Row = gate (online / friends doors). Houses are instances, not 12 lots.
  - V1: all 12 lightly dressed; mine boarded; no workshop, crafting, farming,
    swimming, or XP. Lab output is placeable / wall-able, not “salt in a bag.”
  - **V1 runs in the Grok preview app.** Droplet sibling + SQLite later.
    `liquidgov_http` is never the game.
  - Presence: per-region at ~2 Hz even on one canvas. Droplet is 512 MB / 1 vCPU;
    do not flood.
  - Guests walk the 12; house binds only after LiquidGov cookie; no second
    password. Preview may use a throwaway name until glue exists.
  - Profile canvas from the site = one **wall slot**, not the floor.
  - Friends = civic **query**, short RAM TTL, never a second graph.
    Visit: open / friends / locked.
  - Photo → civic blob + feed only. Game does not grow posts, comments, or follows.
  - Touch: on-screen d-pad; A/D = left/right on screen.
- **2026-09-16 — parked, atlas not locked.** (superseded by lock above)
  Working title The Commons (not confirmed). Art lean GBA / FireRed cousin
  (not confirmed). Nothing built. Do not scaffold, sprite, or write game code
  until Brandon says BUILD. Do not start the game in the civic repo.
- **V1 proposed, not locked:** (superseded) walkable public ground, your house,
  lab, Hall door → live `liquidgov.us/proposals`, mine boarded. Guest play
  until cookie glue exists.
- **Atlas not locked.** (superseded) Brandon wants one greater map of about 12
  minor maps or instances. Options A/B and warp vs continuous were the choice;
  lock is A + continuous.
- **2026-09-17 — cookie resolver built, local-tested, not deployed.** Brandon
  said "build" in reply to the offer naming this exact increment; treated as
  BUILD + cookie resolver per `cookie-bind.md`'s standing gate. First-ever
  edit to `http_server.cpp`: one new same-box-only route,
  `GET /internal/session-user`, ~17 lines, reuses `account_from_req()`
  as-is, refuses (404) anything that arrives via nginx (X-Forwarded-For
  present) or from a non-loopback peer. Daemon side: new `civic.cpp`
  resolver (skips the network call unless the raw Cookie header actually
  contains `lg_session=`, 1s timeout, every failure path falls back to
  guest) plus one `effective_id()` helper wired into all five routes that
  used to check only a guest secret (`join`/`move`/`rekey`/`leave`/
  `placed` POST). A resolved cookie now derives `"u:"+username` server-side
  and the client's own id/secret/display_name are not consulted for
  identity at all. Full local test (scratch civic store + scratch daemon
  DB): a real civic session cookie forwarded with intentionally garbage
  client-submitted id/secret bound correctly as `u:boundtest` on join,
  moved, and left; guest register/join/move regression unaffected. See
  `Persistence/TheCommons/cookie-resolver-built.md` for the full
  before/after and what's explicitly still not built (deploy; mid-session
  promotion/rebind, `cookie-bind.md` left that decision open; the
  restart-snapshot hook for bound players). No nginx, no systemd, no
  droplet touched this round.
- **2026-09-17 — cookie resolver must-fix: mid-session login ghost,
  fixed, still not deployed.** Grok review (`GROK_COOKIE_RESOLVER_REVIEW.md`)
  caught that the prior round's `effective_id()` was cookie-wins on
  every route, which meant a guest who logs in without leaving got a
  `move` rejected (bound id not in the region yet) and, worse, a
  `rekey` that inserted the bound row into the new region without ever
  erasing the old `g_...` row -- a ghost sprite until the 30s idle
  reap. Fixed daemon-only: `PresenceStore` gained `forget(id)` (global
  erase by id) and `contains(region, id)`; `main.cpp`'s `effective_id()`
  split into `bind_or_promote()` for join/rekey (retires the old guest
  row when a verified one exists, before inserting the bound row) and
  `effective_id_in_region()` for move/leave (only switches to the bound
  id once it's actually landed in that region, otherwise keeps using
  the still-live guest row so an ordinary walk never 403s). Verified
  locally: guest walks, logs in mid-session, keeps moving under the
  guest id in the same room, then rekeys and the old room ends up
  empty with no ghost while the new room shows exactly one bound row;
  both prior regressions (already-bound-from-start, no-cookie-bad-secret
  still 403s) still pass. See `cookie-resolver-built.md`'s own
  "Must-fix" section. Still holding on deploy for Brandon's own
  **deploy** word.
- **2026-09-17 — cookie resolver deployed, live.** Brandon said
  "deploy." Rebuilt and restarted both civic (`/opt/liquidgov`) and the
  daemon (`/opt/commons-daemon`, reconfigured for the new `civic.cpp`
  source) on the droplet; no nginx change. Drift-checked both source
  trees against the droplet first (sha256 + diff) -- clean, only this
  project's own commits were live there. Verified in production: public
  `curl https://liquidgov.us/internal/session-user` -> `404` plain text
  (blocked exactly like `/logs`'s own non-operator refusal); a real
  throwaway account, logged in via a real browser, walked into the
  Square with a decoy name typed into the guest-name modal and both the
  occupant feed and the on-screen self-nameplate showed the real
  account name instead. Both services' logs clean post-restart. See
  `cookie-resolver-built.md`'s own "Deployed" section.
- **2026-09-17 — cookie bind arc closed out.** Grok independently
  confirmed the live deploy with no SSH access (`GROK_BIND_LIVE.md`):
  public `/internal/session-user` 404s, `/commons/` still the Square,
  `GET /commons/presence/1,1` empty (the verification occupant had
  already idle-reaped), civic still serving `/proposals`. "No further
  deploy this hour." The one real production side-effect from
  verification -- a throwaway account, `claudeverify17`, created to
  prove the nameplate shows a real account name -- was removed the
  right way: logged in as it, used the site's own `/settings/delete`
  self-service flow (typed the username to confirm), then confirmed a
  login attempt afterward fails with "Invalid username or password."
  No filesystem or LGP1 touch. Cookie bind's whole arc (reachability,
  resolution, the mid-session ghost fix, deploy, independent
  confirmation) is now fully closed on every side. Known remaining
  gaps, not proposed as work: in-place `rebind()` vs. leave+join
  (`cookie-bind.md` never decided), snapshot-on-leave for bound
  players, and a possible `lg_session` -> username cache if the 2 Hz
  loopback call ever actually hitches the daemon's accept loop under
  real load (not a concern at Square-scale today).
- **2026-09-17 — "your house" built, local-tested, not deployed.**
  Brandon said "build" for `GROK_V4_ANSWER.md`'s named increment: bound
  players were real but still lived in the shared demo house, the
  actual next "presence wasted" hole. Safe thread, civic untouched.
  Daemon: `/presence/join`/`/rekey` now echo `"bound":true/false`
  alongside `display_name`. Client: one new Homestead door
  (`T_DOOR_HOUSE`, physically separate from the six still-boarded
  friend-lot doors), a new `mode: "house"` reusing `demoHouseSpace`'s
  geometry under a per-username instance key
  (`instance:house:<username>`), gated on the new `bound` flag -- a
  guest gets a toast, a bound player gets their own isolated room with
  a working wall-slot. Verified backend via curl (bound flag correct
  both ways, two different bound users' houses don't cross-contaminate)
  and client logic via a local static server pointed at a scratch
  daemon (door tile distinct and reachable, guest/bound branches both
  correct, network tick rekeys into the right instance, exit and
  cross-instance placed-item isolation both clean). Snapshot-on-leave
  turned out to be a real separate unit of work once scoped (no
  `players`/`map_instances` rows exist anywhere yet) -- deferred, not
  built this round. See `Persistence/TheCommons/your-house-built.md`.
  Holding on deploy.
- **2026-09-17 — your-house must-fix: house key used the wrong field,
  fixed, still not deployed.** Grok review (`GROK_HOUSE_REVIEW.md`)
  caught that the house key was built from `display_name`, which is
  the *disambiguated Square nameplate* -- a guest who types "brandon"
  before the real `brandon` binds turns the bound player's own
  display_name into "brandon (2)", a space-and-parens string that
  would key a wrong, different house room. Fixed daemon-only for the
  field, plus one client-side hardening pass: `main.cpp`'s join/rekey
  now also echo a raw `"username"` field (only when bound, never
  touched by disambiguation); the client builds
  `INTERIOR_INSTANCE_KEY.house` from that, never `display_name`, and
  `isBound` is now set from every response instead of latching true
  once. Also added `encodeURIComponent` on the four client fetches that
  put a region/instance key in a URL path -- which surfaced a real
  latent bug while implementing it: `commons-daemon`'s `http.cpp` never
  url-decoded `req.path` at all, so encoding client-side without a
  matching daemon-side decode would have broken every outdoor poll
  (`1,1` -> `1%2C1`, no longer matching `PresenceStore`'s literal
  keys). Fixed with a one-line decode in `http.cpp`, symmetric with the
  new client encoding. Verified locally reproducing the exact
  collision scenario (guest claims "boundname" first, real account
  `boundname` binds after) -- `username` stays clean, the house key is
  correct, the wrong suffixed key is confirmed empty; separately
  verified through the real client that outdoor polling still resolves
  real occupant data through the new encode/decode round trip. See
  `your-house-built.md`'s own "Must-fix" section. Holding on deploy.
- **2026-09-17 — your house deployed, live.** Brandon said "deploy"
  (client + daemon only). Drift-checked `main.cpp`/`http.cpp` against
  the droplet first -- clean, only this round's own changes. Synced,
  rebuilt, restarted `commons-daemon` only; `liquidgov.service` never
  touched (uptime confirmed unchanged). Verified in production: the
  exact disambiguation collision reproduced for real (guest claims a
  name first, the real bound account joins after) -- `username` stays
  clean, the house key is correct, the wrong suffixed key is empty. A
  real browser with a real logged-in account walked through the new
  door with a decoy name typed into the guest modal; the network log
  showed the client's own `encodeURIComponent`'d request
  (`instance%3Ahouse%3A...`) returning `200`, proving the encode/decode
  round trip works through the whole real stack, not just locally. The
  self-nameplate showed the real account name, the wall-slot worked
  inside the house too. Both services' logs clean. Cleaned up the one
  throwaway test account via its own `/settings/delete`. **Process
  lesson, caught after the fact by `GROK_HOUSE_KEY_REVIEW.md`:** synced
  client before finishing the daemon rebuild+restart, a narrow window
  where the new client's `encodeURIComponent`'d requests would have hit
  the still-old, non-decoding daemon and gone empty. No evidence any
  visitor actually hit it, but the correct order (verified by the
  review, not by me beforehand) is daemon rebuild+restart fully first,
  client sync second, whenever a change pairs client encoding with
  server decoding. See `your-house-built.md`'s own "Deployed" section.
- **2026-09-17 — your house arc closed out.** Grok independently
  confirmed the live deploy with no SSH access (`GROK_HOUSE_LIVE.md`):
  `/commons/` still the Square, `GET /commons/presence/1,1` empty JSON
  (proving the decode is a correct no-op on a literal comma, not just
  harmless on encoded input), civic still serving `/proposals`. Could
  not independently bind a throwaway account itself (house door is
  logged-in-only, Grok has no browser/SSH), so it took the production
  collision result (nameplate suffixed, house key clean) as reported.
  Accepted the sequencing self-correction from the deploy note as the
  standing rule for next time. "No further deploy this hour." Cookie
  bind, the mid-session ghost fix, and your house (including its own
  must-fix) are now all live and independently confirmed end to end.
- **2026-09-17 — persist the bound house built, local-tested, not
  deployed.** Brandon said "build" for `GROK_V5_ANSWER.md`'s named
  increment: the house door and bound identity were real, but
  `PresenceStore`/`PlacedStore` are RAM, so a daemon restart emptied
  the house and dropped the chalk. Safe thread, civic untouched. New
  `house.hpp`/`house.cpp` (`HouseStore`) is the first code anywhere in
  the project to actually write `schema.sql`'s `players`,
  `map_instances`, and `placed` tables -- find-or-create a `players`
  row and a `map_instances` house row on first bound entry, snapshot
  last position onto that row on `/presence/leave`, and a real
  `placed` row (not the RAM `PlacedStore`) for the wall slot, scoped
  by that instance only. Join/rekey into a house now also runs an
  ownership check -- a guest or a *different* bound player is refused
  outright (`403 not_your_house`) -- since there's real durable data to
  protect now, not just an ephemeral RAM room. Demo and lab are
  completely unchanged, still shared RAM. Client repositions on
  `resume_x/y/facing` when the daemon sends them, guarded on still
  being in `mode: "house"` when the (async) response lands. **Tested
  by actually killing and restarting the daemon mid-test** -- the
  literal bug this build exists to fix -- and confirming the wall-slot
  item survived while presence correctly did not; re-entering resumed
  at the exact last-left position; ownership check holds against both
  a second bound account and a plain guest. See
  `persist-house-built.md`. Only the leave half of "leave / ~30s"
  snapshot-on-leave is wired -- the periodic half needs an actual timer
  loop, flagged as its own separate addition, not attempted. Holding
  on deploy.
- **2026-09-17 — persist-house must-fix: walking out the door never
  snapshotted, fixed, still not deployed.** Grok review
  (`GROK_PERSIST_REVIEW.md`) caught that the prior round's test proved
  the mechanism (a direct `/presence/leave` call) but not the real
  path: `exitToOutdoor()` never calls `netLeave()`, walking out the
  house door is an ordinary rekey, so snapshot-on-leave alone would
  only ever fire on an actual tab close, never on normal walk-out-and-
  come-back-later play. Fixed daemon-only: `/presence/rekey` now also
  snapshots when `from_region` is the caller's own house, captured
  before the rekey erases that occupant row -- same mechanism `leave`
  already used, now on the path that actually matters. Also wrapped
  `house_instance_for`'s two-row create (`map_instances` INSERT +
  `players` UPDATE) in `BEGIN`/`COMMIT` per `schema.sql`'s own stated
  rule, closing a crash-window that could have orphaned a house
  instance and stranded its wall-slot item on a second, unreachable
  copy. Verified locally reproducing the real pattern: entered, placed
  an item, walked out via rekey (no leave call at all), killed and
  restarted the daemon -- resumed at the exact walk-out tile, item
  intact; `leave`'s own snapshot path re-confirmed unaffected. See
  `persist-house-built.md`'s own "Must-fix" section. Holding on
  deploy.
- **2026-09-17 — persist house deployed, live.** Brandon said "deploy"
  (daemon first -- new `house.cpp` -- then client, no civic, no
  nginx). Drift-checked first, clean. Verified with a real throwaway
  account through the real client, not curl: entered its house, placed
  an item, walked to a distinctive tile, waited for the real move-sync
  tick, then triggered the door's actual fallback exit and let the
  client's own automatic tick fire the rekey -- no manual daemon call.
  House emptied correctly, item untouched. Re-entering through the
  real door resumed at the exact walk-out tile and facing, over a real
  cross-origin cookie round trip. Did not force a second production
  restart just to re-prove survival -- the identical binary already
  proved that locally; a real live occupant (`u:Bbottin`) was on the
  box during this deploy and a gratuitous second restart would only
  have interrupted them for no new information. Cleaned up the
  throwaway account via its own `/settings/delete` -- its game-side
  rows are harmless orphans in the daemon's own DB, untouched by civic
  account deletion by design. See `persist-house-built.md`'s own
  "Deployed" section.
- **2026-09-18 — persist-house arc closed out.** Grok independently
  confirmed the live deploy with no SSH (`GROK_PERSIST_LIVE.md`): Square
  up, `GET /commons/presence/1,1` empty JSON, nginx still serving
  `/commons/`. Walk-out -> rekey -> resume is only provable logged in, so
  it took the client-driven production sequence as reported. Also
  settled: leftover `players`/`placed` rows after a throwaway civic
  delete are orphans by design, not civic coupling -- never open LGP1 to
  clean them; a later round can reap unused house rows if they ever
  matter. Environment note: WSL briefly failed on the host (`HCS
  0x8007005a`, insufficient system resources) between the deploy and
  this closeout; cleared on retry, no repo or droplet state affected.
  Cookie bind, the ghost fix, your house, and persist-house are now all
  live and independently confirmed.
- **2026-09-18 — friends query built, local-tested, not deployed.**
  Brandon said "BUILD friends query" (`GROK_V6_ANSWER.md`). Second civic
  edit: one same-box-only, read-only route `GET /internal/friends` (same
  gate as `/internal/session-user`; identity is the session cookie only,
  no username parameter; mutual accepts minus blocks either way).
  Daemon: shared loopback fetch in `civic.cpp`, a 15s RAM-only
  `FriendsCache` (`friends.cpp`), `PresenceStore::bound_usernames()`, and
  `GET /presence/lots` -- mutual friends who are in the game, sorted, six
  max, for Homestead lots 2-7. It lives under `/presence/` so the existing
  nginx location covers it: no nginx change. Client: the six boarded
  doors become `friendDoor`s; a bound player near the facade polls every
  5s and a door with a friend draws lit with their name. **Visiting is
  deliberately not built** (waits on this per Grok, and no visit-policy
  data exists): a named door says visiting isn't open yet, and the
  daemon's `not_your_house` check is untouched. Tested with four real
  civic accounts (accepted, never-accepted, blocked), the public-404
  gate, a `?username=` probe, and a real browser. The browser check
  caught a load-time crash (`FRIEND_LOT_DOORS` declared after the world
  is built) that reading the diff would not have. Unlike the last three
  rounds, deploying this touches civic (rebuild + restart `liquidgov`).
  See `friends-query-built.md`. Holding on deploy.
- **2026-09-18 — friends query deployed, live.** Brandon said "deploy"
  (civic, then daemon, then client; no nginx). Drift-checked first --
  civic was 25 added lines, zero removed. Civic rebuilt and restarted
  first: public `/internal/friends` 404s, same-box no-cookie is `{}`,
  `/` stayed 200. Then the daemon (new `friends.cpp`), then the client.
  Verified in production with three throwaway accounts (one accepted
  friend, one never-accepted): `/commons/presence/lots` correct for each
  and for a guest; in a real browser a lit lot 2 showed the friend's
  name, the not-a-friend account stayed absent, tapping it said visiting
  isn't open, the tile stayed solid, and a direct join into another
  account's house still returned 403. All three accounts deleted through
  `/settings/delete`. Process note: `/tmp` cookie jars did not survive
  between shell calls, so my first friend keepalive silently lapsed --
  caught because the door stayed dark, not assumed. Visiting is next and
  needs an open/friends/locked decision first.
- **2026-09-18 — visiting (friends policy) built, local-tested, not
  deployed.** Brandon picked **friends**: a fixed rule for every house
  (per-owner settings would need stored data and UI -- a later round).
  A bound player may be in another's house iff that owner is a mutual,
  non-blocked friend (civic's answer through the existing 15s cache) and,
  to enter, the owner is in the game. A visit is read-only: no rows
  created for the visitor, no snapshot, no resume, `/placed/` POST still
  owner-only. `move` re-checks friendship, so an unfriend or block ends
  a visit within one TTL, answered `200 not_welcome` (a 403 would read as
  a dead credential). Client: the lit lot door ("Visit") enters
  `mode: "visit"`; the wall says whose it is and reads **Look**. Caught
  a real bug before it shipped: the client treated every 403 as a dead
  credential, so a *declined* visit would have reminted and retried
  forever -- now it reads the reason. Daemon + client only; no civic, no
  nginx. Tested with three real civic accounts (friend, non-friend,
  blocked mid-visit; guest; owner offline) plus a real-browser client
  pass. See `visiting-built.md`. Holding on deploy.
- **2026-09-18 — visiting deployed, live.** Brandon said "deploy"
  (daemon, then client; no civic, no nginx). Drift-checked first --
  only lines this round rewrote. Verified in production with three
  throwaway accounts: friend refused while the owner is offline;
  stranger, guest, and stranger-placing all 403; in a real browser the
  friend's lot door read "Visit", walking in landed in the owner's house
  at the normal tile with her wall item visible, and the wall refused
  the visitor. A read-only query of the production DB showed only the
  owner has a `players` row -- the visit stored nothing. Then the owner
  blocked the friend and, within one cache window, the real client was
  walked out with a message, the door went dark, the house emptied, and
  re-entry returned 403. All three accounts deleted via
  `/settings/delete`. See `visiting-built.md`'s "Deployed" section.
- **2026-09-18 — visiting arc closed out.** Grok independently confirmed
  the deploy with no SSH (`GROK_VISITING_LIVE.md`): Square up, empty
  presence and lots, public `/internal/friends` 404; the logged-in Visit,
  wall Look, and block eviction rest on the production three-account
  pass. Visitor-linger and the 15s eviction lag stay as known limits.
  The old "mine boarded until a visit actually works" gate is **lifted**
  -- but the mine is not to be opened unasked. Next named BUILD is
  Brandon's to choose: **mine**, **`/latest`** (the third civic edit), or
  **bound inventory**; `open`/`locked` policies are later. Nothing is
  queued and no question is pending with Grok -- it already named the
  options.
- **2026-09-18 — bound inventory built, local-tested, not deployed.**
  Brandon said "BUILD bound inventory". A bound player's lab lookups now
  persist in `schema.sql`'s never-written `inventory` table and are
  restored on load, so the wall places what you last looked up even
  after a refresh or a daemon restart. The table had no recency, and
  "most recent" is the only thing the client uses inventory for, so it
  gains `updated_at` (ms) -- **the first change to an existing table**:
  `migrate()` only runs `schema.sql` on a brand-new DB, so a new
  idempotent `ensure_inventory_recency()` adds the column at startup.
  Tested against an old-shape DB, twice (idempotent). Routes
  `POST /presence/pickup` and `GET /presence/inventory` live under
  `/presence/` so nginx needs no change; identity is the cookie only,
  no guest fallback (guests stay RAM). Pickups aren't consumed and no
  menu exists. Tested with real civic accounts and a real restart, plus
  a real-browser client pass (guest sends nothing; bound restores once;
  an in-flight pick isn't overwritten). Daemon + client only. Deploy
  note: back up the production `commons.sqlite` first. See
  `bound-inventory-built.md`. Holding on deploy.
- **2026-09-18 — bound inventory deployed, live.** Brandon said
  "deploy" (backup, daemon, client; no civic, no nginx). Took a
  consistent online backup of the production `commons.sqlite` first
  (`/var/lib/commons/backups/commons.sqlite.pre-inventory-20260918T204938Z`,
  integrity ok, baseline counts recorded), drift-checked, then deployed
  the daemon and confirmed on the *live* DB that `updated_at` was added,
  integrity ok, and every pre-existing row count matched the baseline --
  the first migration on real production data. Then the client. In a real
  browser: a bound throwaway account looked up two lab stations, the
  daemon stored them (most recent first), a page reload restored both
  with no lab visit, and the wall placed the restored item in its house;
  as a guest afterward, a lookup sent nothing. Account deleted; its game
  rows are orphans by design. The backup is left on the box. Remaining
  named options: **mine**, **`/latest`**.
- **2026-09-18 — /latest built, local-tested, not deployed.** Brandon
  said "BUILD /latest". Third civic edit: one branch,
  `GET /canvas-frame/<username>/latest`, registered ahead of the existing
  index route (left byte-identical -- it `stoi`s the last segment, so
  `latest` must never reach it; frame 0 is the banned first stroke). Same
  checks as the index route, every failure a plain 404 the game reads as
  an empty slot. Client: in a house (yours, or a friend's you are
  visiting) the owner's real profile drawing hangs in the wall slot,
  fetched same-origin so the viewer's cookie drives civic's block check;
  the shared demo house and lab keep the empty slot; a placed item
  shrinks to a corner badge so the two share the slot. Tested with real
  uploaded PNGs (a second frame becomes `/latest`, index 0 unchanged,
  block and clear both 404) and a real browser. Deploy order: civic, then
  client; no daemon, no nginx. See `latest-built.md`. Holding on deploy.
- **2026-09-18 — /latest deployed, live.** Brandon said "deploy" (civic,
  then client; no daemon, no nginx). Drift-checked first -- civic was 27
  added lines, zero removed. Verified in production with three throwaway
  accounts and real uploaded drawings (host red, visitor blue, one bare):
  in a real browser the visitor's own house showed his blue, the host's
  house showed the host's **red** (sampled from the fetched image), and
  the bare friend's house showed the empty slot with nothing carried over.
  A viewer who blocked the owner gets 404; anonymous still 200. **Two
  things worth knowing:** (1) civic's block check is one-directional (it
  hides frames from a viewer who blocked the *owner*, not the reverse, and
  anonymous skips it) -- copied as specified, and `canvas-frame.md`'s
  "either side" overstates it; an owner-side hide would be a separate civic
  change. (2) **Pre-existing client bug found, not fixed:** a bound
  player's own daemon row (`u:<name>`) isn't filtered out by the client's
  guest-credential `netId`, so they are drawn as a second copy of
  themselves, lagging by up to a poll interval while walking. Live since
  cookie bind. Small fix, deliberately not bundled into this deploy. All
  accounts deleted. Remaining named option: **mine**.
- **2026-09-18 — self-ghost built, local-tested, not deployed.** Brandon
  said "build" (read as self-ghost; nothing in that message named it). The
  client filtered "is this occupant me" against only its guest credential
  `netId`, but a bound player's daemon row is `u:<name>`, so their own row
  was drawn a second time, lagging ~0.5s. Fix is client-only, +15/-2: an
  `isSelfId` helper that knows both ids, `netBoundId` set from every
  join/rekey answer, used at both occupant-poll filters; matches on id,
  never `display_name`. Verified in a real browser per Grok's resume
  brief -- no `credentials: include` patch, no second client copy, no
  scratch civic accounts (I had begun all three before reading that brief,
  written while I was out of budget; I tore it down and redid it its way)
  -- with one stubbed `u:` row: the ghost reproduced with the bound id
  unknown, vanished on the bound join answer and stayed gone, a
  look-alike `ghostuser (2)` row was still drawn, guests unchanged. No
  deploy until Grok reviews; deploy later is client only. See
  `self-ghost-built.md`. **CI:** `.github/workflows/commons.yml` is
  committed locally from `ci.md` but **not pushed** -- the GitHub token
  lacks the `workflow` scope; see the status report, not resolved here.
- **2026-09-18 -- CI landed; self-ghost deployed (client only).** After
  Brandon granted the `workflow` scope, `.github/workflows/commons.yml`
  (rebuilt from the updated `ci.md`: `fetch-depth: 0`, no `|| true` on the
  gate-5 `git diff`) was pushed as `91880e5`; first run green (56s, incl.
  `nginx -t`). Gate 5 checked locally: full clone fails a civic+client range
  whose head subject lacks `civic`/`deploy`, passes with it, skips an
  all-zero `before`, and a shallow clone now fails loud (`bad object`)
  instead of skipping. Then Brandon said `deploy`: client only via
  `sync-client-to-droplet.sh` -> `/opt/commons`, chown liquidgov; droplet
  and served `index.html` md5 both equal local (`f1354d53...`). No daemon,
  civic or nginx change. Not yet checked: a real bound session on production
  (one sprite while walking) -- the local page can't carry a civic cookie.
  **Confirmed by Brandon, logged in on production: nothing trailing.**
  Self-ghost closed. Remaining named option: **mine**.
- **2026-09-19 -- mine A built, local-tested, not deployed.** "BUILD mine"
  had no spec (every earlier doc said boarded, no interior), so I asked
  (`CLAUDE_MINE_QUESTION.md`); Brandon/Grok chose **A, honest cave, no ore,
  guests welcome, client only** (`GROK_MINE_ANSWER.md`). The Quarry Ridge adit
  now has one door tile into a shared 16x12 `instance:mine` (dirt, rock walls,
  two sit-rocks, one lit lamp tile, one sign "No ore. That's honest."); the
  outdoor sign reads "Watch your step."; nothing to place or pick up; Enter
  leaves onto the tile in front of the adit; the passerby stays outdoors. No
  daemon/civic/nginx/schema change, no new tileset (two flat-colour tile ids).
  Verified as a guest in a real browser against a scratch daemon. See
  `mine-built.md`. Deploy is client only, after Grok's review and Brandon's
  `deploy`.
- **2026-09-19 -- mine A deployed (client only).** Grok accepted
  (`GROK_MINE_REVIEW.md`); Brandon said `deploy`. Synced to `/opt/commons`,
  droplet and served md5 equal local (`1f4eab59...`); no daemon/civic/nginx
  change. Live check with two guest tabs: both in `instance:mine`, each saw
  the other; sign, exit-to-adit, outdoor sign, workshop "Closed for now.",
  Hall link all as designed. Named leftovers now: none open; `open`/`locked`
  policies, ~30s snapshot, `rebind()`, `lg_session` cache and orphan reaping
  stay parked.
- **2026-09-19 -- CI pinned to ubuntu-24.04.** Brandon: "BUILD CI pin".
  One line in `.github/workflows/commons.yml` (`runs-on: ubuntu-latest` ->
  `ubuntu-24.04`), ahead of `ubuntu-latest` moving to Ubuntu 26 on
  2026-10-19. Pushed as `807b4bd`; run green in 1m4s (daemon build and
  `nginx -t` on the pinned image); the Ubuntu-26 annotation is gone. Left
  alone: `actions/checkout@v4` (Node 20 annotation remains; a `@v5` bump is
  a separate one-liner) and `ci.md`'s block still says `ubuntu-latest`
  (Grok's doc). Parked still: house `open`/`locked`, the ~30s snapshot.
- **2026-09-19 -- checkout@v5.** Brandon: "BUILD checkout@v5" (Grok's
  `GROK_CHECKOUT_V5.md`; Node 20 leaves the runners 2026-09-23). One line:
  `actions/checkout@v4` -> `@v5`; `fetch-depth: 0` and `ubuntu-24.04` stay;
  no `ACTIONS_ALLOW_USE_UNSECURE_NODE_VERSION`. `ci.md`'s block synced to
  match. Pushed as `4293edf`; run green in 1m11s and the run page shows no
  annotations at all (the Node 20 warning is gone). No droplet, no game.
  Parked still: house `open`/`locked`, the ~30s snapshot.
- **2026-09-19 -- ~30s snapshot built, local-tested, not deployed.** Brandon:
  "BUILD 30s snapshot" (spec: `presence.md`; Grok had it parked as "no").
  Daemon only. Every ~30s, each bound player standing in their own house has
  their position written to their `players` row (the same `snapshot_leave`
  write leave/rekey use), so a crash mid-session resumes at most ~30s stale
  instead of from the last walk-out. No thread: `HttpServer::set_periodic`
  polls the listen socket with a timeout and runs the hook between requests on
  the accept-loop thread. Verified with `kill -9` against real civic: control
  (killed before any tick) has no resume; after 35s it resumes (5,4,f1);
  standing in the demo house writes nothing to the house row; the persist-house
  regression still passes. See `snapshot-built.md`. Deploy is daemon only,
  after Grok's review and Brandon's `deploy`. Parked still: house
  `open`/`locked`.
- **2026-09-19 -- ~30s snapshot deployed (daemon only).** Grok accepted
  (`GROK_SNAPSHOT_REVIEW.md`); Brandon said `deploy`. Drift check clean (only
  the four edited files differed), online SQLite backup first
  (`commons.sqlite.pre-snapshot-20260919T041014Z`, left on the box), sync,
  `-j1` build on the droplet, restart. Verified across a 40s window through
  nginx: lots/presence/placed 200, `/internal/friends` 404, no journal
  errors, no restarts, DB integrity ok with counts unchanged, loopback bind.
  A real bound tick is unproven on prod (needs a bound player in their own
  house across a tick, then a restart). Parked still: house `open`/`locked`.
- **2026-09-19 -- house friends/locked (A) built, local-tested, not deployed.**
  "BUILD house open/locked" had no spec and `open` collides with the civic
  freeze (civic can't say who has blocked whom, so a stranger-facing door
  couldn't honour a block); I asked (`CLAUDE_HOUSE_POLICY_QUESTION.md`) and
  Brandon/Grok chose **A: friends/locked only, `open` waits on a civic block
  probe** (`GROK_HOUSE_POLICY_ANSWER.md`). An owner locks their own house from
  a latch tile inside it; `players.visit_policy` (default `friends`, idempotent
  startup migration), owner-only `POST /presence/house-policy` (cookie identity,
  guests 403, `open` 400), `visit_allowed` checks it on join/rekey/**move** so a
  visitor is walked out on their next move, locked owners drop out of
  `/presence/lots`, and join/rekey echo the owner's policy. Verified against
  real civic with three accounts (incl. restart persistence and a real
  old-shape DB migration) and in a real browser with the request stubbed. No
  civic/nginx change. See `house-policy-built.md`. Deploy: **daemon first**
  (schema change: online SQLite backup first), then client, after Grok's review
  and Brandon's `deploy`. Parked still: `open`, `rebind()`, `lg_session` cache,
  orphan reaping.
- **2026-09-19 -- house friends/locked deployed (daemon, then client).**
  Brandon said `deploy`; Grok's review was not on file yet, so it went on his
  word. Drift check clean (only the six edited daemon files + the client
  differed), online SQLite backup first
  (`commons.sqlite.pre-house-policy-20260919T070744Z`, left on the box),
  daemon `-j1` build + restart, and the migration ran on the real DB: column
  present, all 5 players `friends`, integrity ok, counts unchanged. Then the
  client (droplet = served = local `e99898fa...`). New route 403s without a
  cookie; `/internal/friends` still 404. A real bound owner locking a real
  house is unproven on prod (needs Brandon logged in). Parked still: `open`
  (needs a civic block probe), `rebind()`, `lg_session` cache, orphan reaping.
- **2026-09-19 -- tile dither built, local-tested, not deployed.** Brandon:
  "BUILD tile dither" (Grok's graphics note: the cheap pass, code only).
  Client only. `paintTile()` replaces the bake's flat `fillRect` with the same
  base colour plus grass 2x2 checker dots (position-hashed phase), a darker
  water lip on edges that meet non-water, and wood grain on `T_BENCH`; every
  shade derived from the tile's own colour, so no new palette, asset or draw
  path. One bake, one blit, ~51ms at load. Looked at in a real browser and
  pixel-checked; console clean. See `tile-dither-built.md`. The tileset PNG +
  player sheet is the separate, un-named next step. Deploy: client sync on
  Brandon's `deploy`.
- **2026-09-19 -- tile dither deployed (client only).** Grok accepted
  (`GROK_TILE_DITHER_REVIEW.md`); Brandon said `deploy`. Client sync to
  `/opt/commons`; droplet = served = local `dde34287...` (was `e99898fa...`);
  no daemon/civic/nginx change. Guest check on prod: grass checker and creek
  lip render, console clean. Open: Brandon's eye on the dots at real zoom (thin
  the lattice if they crawl). Tileset PNG + player sheet still unnamed.
- **2026-09-19 -- tileset PNG + player sheet built, local-tested, not deployed.**
  Brandon: "BUILD tileset PNG + player sheet" (Grok's graphics note +
  `atlas-pack.md`: `tiles.png` 16x16 grid in `T_*` order, `sprites.png` 4 dir x 2
  frame, bake stamps once). Client only. I drew it, from `art.md`'s V1 list, with
  `Persistence/TheCommons/art/make_art.py` (stdlib, deterministic, checks each
  tile's mean colour against the client's own `COLOR`). The bake stamps from the
  sheet via `drawTile()` once the PNG loads (world first bakes with `paintTile`,
  which stays as the fallback; a 404 or wrong size keeps the painted look).
  Grass variants in cells 48-51 keep the hash variation. One adjacent fix: rooms
  smaller than the viewport (the mine) left stale outdoor pixels below them; the
  margin is now cleared to the page black and the bottom hint/toast draw in paper
  there. Verified in a real browser (Square, creek, woods, Homestead, Lab, mine,
  all four facings, no-PNG fallback). See `tile-sheets-built.md`. Deploy: client
  sync now carries index.html + tiles.png + sprites.png; check `200 image/png`.
- **2026-09-19 -- tileset PNG + player sheet deployed (client only).** Brandon
  said `deploy`; Grok's review was not on file, so it went on his word. Client
  sync now carries three files; droplet = served = local for all three
  (`index.html` `1e924625...`, `tiles.png` `853cdf21...`, `sprites.png`
  `ae31b218...`); both PNGs `200 image/png` through nginx; no daemon/civic/nginx
  change. Guest walk on prod: sheet loaded, Homestead/Lab/mine render with the
  new art, mine margin black with a readable toast, console clean. Art tweaks =
  edit `make_art.py`, re-run, sync, bump `?v=`. Parked still: `open` (needs a
  civic block probe), `rebind()`, `lg_session` cache, orphan reaping; Brandon's
  eye on the grass dots and the new art at real zoom.
- **2026-09-19 -- daemon janitor (rest slice 2) built, local-tested, not deployed.**
  Brandon: "BUILD session cache"; Grok's `GROK_REST_2.md` pairs it with the
  `sqlite3_step` checks, so I built the slice as written. Daemon only. (1)
  `civic::resolve_username` keeps a positive answer 15s in RAM keyed on the Cookie
  value (failures and "no such session" are never cached; guests never reach
  civic; a logged-out session can resolve up to 15s). (2) `sqlx.hpp` wraps every
  prepare/bind/step/COMMIT: a failure logs the sqlite message and becomes a 500
  `server_error` for that request, `Tx` rolls multi-row writes back, catalog
  readers no longer return a silent `[]`; snapshots stay best-effort so a leave
  can't strand a player. Verified with real civic frozen via SIGSTOP (hit 0.46ms,
  miss ~1s, failure not pinned, TTL expiry) and failures injected with triggers
  (wall write rolled back, leave still ok, daemon never died); all prior
  regressions pass. See `janitor-built.md`. Deploy: daemon only, online SQLite
  copy first, after Grok's review and Brandon's `deploy`. Parked: `rebind()` (3),
  `open` + orphans (4).
- **2026-09-19 -- daemon janitor (rest slice 2) deployed (daemon only).** Grok
  accepted (`GROK_REST_2_REVIEW.md`); Brandon said `deploy`. Drift check clean
  (five edited files + new `sqlx.hpp`), online SQLite backup first
  (`commons.sqlite.pre-janitor-20260919T084253Z`, left on the box), sync, `-j1`
  build with no reconfigure, restart. 40s window through nginx all 200 (lots,
  `1,1`, house placed), `/internal/friends` 404, no `failed`/sqlite lines in the
  journal, no restarts, DB integrity ok with counts unchanged, loopback bind. A
  real bound cache hit is not observable from outside. Parked: `rebind()` (slice
  3), `open` + orphans (slice 4, the only civic one).
- **2026-09-19 -- `rebind()` (rest slice 3) built, local-tested, not deployed.**
  Brandon: "BUILD rebind" (`GROK_THE_REST.md` slice 3; `cookie-bind.md` left the
  primitive open). A guest who logs in mid-session is now relabelled `u:<name>` IN
  PLACE on their next move: `PresenceStore::rebind` keeps x/y/facing/last_update,
  spends no cap slot, supersedes a second tab's older row, disambiguates the name
  ("alice (2)"); it requires the cookie to resolve AND the guest secret to still
  verify, and never applies to a house region. The move answer carries
  `promoted`/`bound`/`username`/`display_name`; the client applies it through the
  same `netApplyJoinJson` a join answer uses (so the self-ghost filter, nameplate,
  house key follow). Verified with real civic (one `u:` row, no ghost, guards,
  second tab, name collision) and in a real browser with the answer stubbed; all
  prior cookie/visit/policy/persist regressions pass. See `rebind-built.md`.
  Deploy: daemon, then client, after Grok's review and Brandon's `deploy`. Parked:
  `open` + orphans (slice 4, the only civic one).
- **2026-09-19 -- `rebind()` (rest slice 3) deployed (daemon, then client).**
  Brandon said `deploy`; Grok's review was not on file, so it went on his word.
  Drift check clean (three daemon files + the client), online SQLite backup first
  (`commons.sqlite.pre-rebind-20260919T085640Z`, left on the box), daemon `-j1`
  build with no reconfigure + restart (DB integrity ok, counts unchanged, no
  failures, no restarts), then the client (droplet = served = local
  `9a8ac18c...`). Real guest join/move/bad-secret/leave unchanged on prod; guest
  walk clean. The promotion itself is unproven on prod (needs a real mid-session
  login): log out, open the game, log in on LiquidGov in another tab, walk -- the
  nameplate should become your username with no second sprite. Parked: `open` +
  orphans (slice 4, the only civic one; Grok: not before 1-3 are live, which they
  now are).
- **2026-09-19 -- `open` houses (rest slice 4a) built, local-tested, not deployed.**
  Brandon: "BUILD open A, split, orphans log-only"; Grok's
  `GROK_REST_4_ANSWER.md`: 4a first with discovery A, 4b orphans later. **Fourth
  named civic edit:** same-box `GET /internal/blocked/<username>` (cookie = viewer,
  path = owner, `{"blocked":true}` if either side blocked the other, fails
  closed on no session/unknown owner/malformed). Daemon: `civic::block_answer`
  (15s cache of real answers only, plain-username check before the request line),
  `visit_allowed` lets a non-friend in only for `open` + a real `blocked:false`,
  `/presence/lots` = friends first then open houses of non-friends (cap 6, guests
  none, stops probing when civic is silent), setter accepts `open`. Client latch
  cycles friends -> open -> locked. Verified against real civic with seven
  accounts (blocks both directions, guest, injection guard, live policy changes,
  block eviction after the TTL, fail-closed with civic frozen) and in a browser
  with the request stubbed. See `open-built.md`. Deploy: **civic first** (drift-check
  the droplet's `http_server.cpp`), then daemon, then client, after Grok's review and
  Brandon's `deploy`. 4b orphans is its own BUILD (log-only first).
- **2026-09-19 -- `open` houses (rest slice 4a) deployed (civic, then daemon, then
  client).** Grok accepted (`GROK_REST_4A_REVIEW.md`); Brandon said `deploy`. Civic
  drift check first: the droplet's `http_server.cpp` equalled the previous git version
  (`81eb0e5e...`), only that file differed. Online game-DB backup
  (`commons.sqlite.pre-open-20260919T093423Z`, left on the box), civic `-j1` build +
  `liquidgov` restart (public `/internal/blocked/...` is 404; on-box it answers and
  refuses X-Forwarded-For; accounts store untouched), daemon `-j1` + restart (DB
  integrity ok, counts unchanged, all houses still `friends`), then the client
  (droplet = served = local `95f997e7...`). No failures, no restarts. An owner flipping
  a real house to `open` and a stranger's door lighting is unproven on prod (needs two
  real accounts). **4a is live.** Parked: 4b orphans (own BUILD: `user-exists`,
  log-only first, two passes, cap 3, backup).
- **2026-09-19 -- orphan watch, LOG-ONLY (rest slice 4b) built, local-tested, not
  deployed.** Brandon: "BUILD orphans log-only"; Grok's valves (`GROK_REST_4_ANSWER.md`).
  Civic: same-box `GET /internal/user-exists/<username>` (`exists` true/false for a
  well-formed name, `null` for a malformed one, never false). Daemon: `civic::user_exists`
  (Exists/Gone/Unknown; only an explicit false is Gone) and a ~10-minute pass on the
  existing hook that finds players rows whose account is gone and only LOGS `would reap`
  with a read-only footprint (inventory rows, placed rows, house instance). Valves: two
  consecutive passes, cap 3 per pass, online players skipped, stops at the first
  don't-know, suspect guard when every account reads gone. **No DELETE anywhere.**
  Verified against real civic with real deleted accounts (rows unchanged; SIGSTOP resets
  the streak; re-registered name clears it; cap defers the 4th; suspect guard). See
  `orphans-built.md`. Deploy: civic then daemon, no client, backup first; then read the
  journal (~20 min) -- expect the earlier throwaway accounts. The reap itself is a
  separate later BUILD Brandon names after reading the list.
- **2026-09-19 -- orphan watch (log-only) deployed (civic, then daemon).** Grok accepted
  (`GROK_REST_4B_REVIEW.md`); Brandon said `deploy`. Civic drift check clean (droplet =
  previous git version), online game-DB backup first
  (`commons.sqlite.pre-orphans-20260919T101314Z`, left on the box). Fixed a suspect-guard
  flaw found while reading the real `players` list (online players are skipped, so it
  compared against too few accounts): now against every known account (`81bc825`).
  Civic `-j1` + restart (public `/internal/user-exists/x` is 404), daemon `-j1` + restart
  at 10:17:21Z (startup line says LOG-ONLY; DB integrity ok, counts unchanged). Production
  `players`: persistverify9, vtowner, invverify1, Bbottin, ptvisitor -- expect the four
  throwaway names to show as would-reap. Nothing is deleted. The reap is a separate later
  BUILD after Brandon reads the list.
- **2026-09-19 10:37Z -- orphan watch's first list (nothing deleted).** After two passes:
  would reap `persistverify9` (house 3, 0 inv, 1 placed), `vtowner` (house 4, 0 inv, 1
  placed), `invverify1` (house 5, 2 inv rows, 1 placed); `ptvisitor` (house 6, empty)
  deferred by the cap, due next pass. `Bbottin` (real) exists and is not listed. All four
  are throwaways deleted via `/settings/delete` in earlier live tests. DB counts unchanged.
  The reap is a later BUILD once Brandon agrees the list.
- **2026-09-19 -- orphan REAP built, local-tested, not deployed.** Brandon: "BUILD orphans
  reap" after reading the first log-only list. Daemon only. The ~10-minute pass now deletes
  a qualifying name's inventory, house `placed` rows, `players` row and owned house
  `map_instances` row in ONE rolled-back-on-failure transaction. Still needs: explicit
  civic `exists:false`, two consecutive passes, cap 3 per pass, not online, suspect guard.
  New: an automatic online SQLite backup per reaping pass (`backups/commons.sqlite.pre-reap-*`;
  no backup, no reap), one last civic check right before each delete, and a
  `COMMONS_ORPHAN_REAP=0` kill switch (default ON). Verified with real civic: living accounts
  untouched, valid pre-reap backups, a new owner of a reaped name gets an empty house and no
  inventory, frozen civic reaps nothing, failed backup reaps nothing, kill switch works.
  (My first run used invalid one-letter usernames and exercised nothing; redone.) See
  `orphans-reap-built.md`. Deploy: daemon only, backup first, after Grok's review and Brandon's
  `deploy`; expect `persistverify9`, `vtowner`, `invverify1` at ~20 min and `ptvisitor` at ~30.
  Also noted: Grok's `civic-ratelimit.md` (explore, not a build): no limiter on `/internal/`.
- **2026-09-19 11:30Z -- orphan REAP deployed (daemon only).** Grok accepted
  (`GROK_ORPHAN_REAP_REVIEW.md`); Brandon said `deploy`. Drift clean (five daemon files),
  manual online SQLite copy first (`commons.sqlite.pre-reap-manual-20260919T112956Z`),
  `-j1` build + restart at 11:30:28Z; journal: `orphan watch is REAPING`; DB integrity
  ok, counts unchanged, no failures. Expected: 1st pass ~11:40Z (logs gone), 2nd ~11:50Z
  reaps persistverify9, vtowner, invverify1 (automatic backup first), 3rd ~12:00Z reaps
  ptvisitor; Bbottin untouched (civic says it exists). Kill switch:
  `COMMONS_ORPHAN_REAP=0` + restart. Result to be read from the journal and recorded.
- **2026-09-19 12:20Z -- orphan reap ran; four throwaways gone, Bbottin untouched.** Read
  (read-only) at 11:55Z and 12:20Z. 11:50:26Z: automatic backup, then REAPED
  persistverify9, vtowner, invverify1, 1 deferred by the cap; 12:00:27Z: second automatic
  backup, REAPED ptvisitor. Live DB: integrity ok, foreign_key_check clean, nothing
  dangling; players 5 -> 1 (exactly Bbottin), map_instances 6 -> 2 (demo, lab), placed 3 ->
  0, inventory 4 -> 2. Bbottin is kept because civic says it exists, not by name. Three
  backups on the box (manual, 11:50, 12:00), all integrity ok, the 11:50 one holds all 5
  players. No failures, no restarts. The inheritance hole (a re-registered deleted
  username getting the old player's house/inventory) is closed; the watch keeps running,
  kill switch `COMMONS_ORPHAN_REAP=0`. **Rest slices 1-4 and the reap are all live.**
  Nothing is parked from that list; `backup-strat.md` and `civic-ratelimit.md` are explore
  notes, not builds.
- **2026-09-19 -- hardening (four daemon fixes) built, local-tested, not deployed.** From
  Claude's survey, Grok's `GROK_HARDENING.md`. (1) guest secrets from `getrandom`, not a seeded
  Mersenne Twister; (2) guest registry capped at 256 -- **evicts instead of the spec's 503**
  (the client doesn't check the mint reply), never-used credentials expire in 2 min and go
  first; (3) 5s socket timeouts + 10s whole-request deadline, truncated bodies closed not
  dispatched; (4) `FriendsCache` no longer caches a civic failure (`resolve_friends` returns
  optional; unknown = never evict a friends-house visitor, never admit a new one, lots dark
  for that poll only). Tested against real civic incl. frozen-civic timing proofs; my first
  friends-cache test was wrong (froze civic after both caches had expired) and was redone.
  See `hardening-built.md` (also notes a possible civic-outage breaker for Grok to decide).
  nginx `limit_req` on `/guest` stays a later named deploy. Deploy: daemon only.
- **2026-09-19 12:55Z -- hardening deployed (daemon only).** Grok accepted
  (`GROK_HARDENING_REVIEW.md`: evict-not-503 correct, no civic breaker, nginx `/guest`
  limit later); Brandon said `deploy`. Drift clean (eight daemon files), manual online SQLite
  copy first (`commons.sqlite.pre-hardening-20260919T125451Z`), `-j1` build + restart at
  12:55:33Z; DB integrity ok, counts unchanged, journal clean, no restarts. Public guest
  mint/join/move/bad-secret-403/leave all as designed with OS-random ids and secrets; a
  stalled half-request now holds the daemon ~5s instead of forever. Unseen on prod: the cap
  under a real flood and a real civic hang. Remaining named items from the survey, all
  unbuilt: nginx `limit_req` on `/guest` (deploy), tests-in-CI, deploy scripts, backup
  retention, splitting `index.html`, `/health` hash.
- **2026-09-19 -- tests in CI built; CI is the last check.** Brandon: "BUILD tests in CI"
  (`GROK_NEXT.md` #1). `commons-daemon/tests/`: 11 files, ~300 real assertions, `lib.sh`,
  `run_all.sh`, frozen old-schema fixtures, and `civic_stub.py` (in-memory civic: the public
  signup/friends/block/delete routes plus the four `/internal/*` routes with civic's semantics).
  The stub is validated: the whole suite passes against BOTH the stub and the real
  `liquidgov_http` (~210s each). New workflow step runs it after the daemon build (installs
  `strace`; per-file and per-step timeouts). Mutation-checked: with the pre-hardening daemon put
  back the suite fails on the cap, friends-cache and socket-timeout tests -- and that check first
  showed the guest test would HANG on a daemon without timeouts, fixed (fail-fast probes). My own
  draft mistakes (invalid usernames earlier, rows created before the daemon was up, a NULL
  printing as 'None') were caught by asserting preconditions. No daemon/civic/client/nginx change,
  no droplet, no deploy. First CI run (d9d1b51): 10/11 files green incl. the strace check; `test_house` failed because
  `test_guest`'s strace wrapper was killed but its traced daemon lived on, holding :8765 (the next file's daemon died on
  bind(); health passed against the survivor). Harness fixed + `test_harness.sh` added. See `tests-built.md`. Still unbuilt: deploy scripts, nginx `/guest`
  limit, backup retention, `index.html` split, `/health` hash.
- **2026-09-19 -- `reqjson` corrected: latent bug, not harmless.** Grok's review of `c6a7a4f` (CI green,
  accepted) caught that I called the naive `find("\"key\"")` scanner harmless. Reproduced: name-before-fields
  bodies make a guest named `x`/`y` 400, `secret` 403, `id`/`facing` get a wrong "(2)" rename. The shipped client
  sends `display_name` last, so it does not bite today (all seven colliding names joined) -- luck of key order.
  Awaiting Brandon's `BUILD reqjson` (daemon only; parse `"key":` as a real key walk + `test_reqjson.sh`).
- **2026-09-19 -- `BUILD reqjson` built (daemon only, NOT deployed).** `reqjson.hpp` now walks the flat object
  (key, `:`, value, `,`) so a field is found only in KEY position; a value equal to a field name (a guest named
  `x`, name-first body) can no longer be read as a key. Same API/call sites; `field_num` stricter (JSON-number
  token only). `tests/reqjson_unit.cpp` (201 checks) + `tests/test_reqjson.sh` (102 assertions incl. name-first
  bodies through the daemon) run in CI via `run_all.sh`; mutation-checked against the old header. The live
  daemon still has the old parser until `deploy`. See `reqjson-built.md`. Next per Grok: deploy scripts, then nginx.
- **2026-09-19 -- `BUILD deploy scripts` built (files + fake-droplet tests; NO droplet contact).** `commons-daemon/deploy/`:
  `lib.sh` (shared config + the one exclusion list + a deployed-revision marker), `drift-check.sh` (droplet vs the
  LAST-DEPLOYED revision by checksum, so a hand edit on the box is reported, plus what this deploy will change),
  `backup-db.sh` (SQLite backup API, integrity_check, counts, never overwrites), the two syncs reworked (refuse a
  dirty/unpushed tree, write the marker, `-n`), `build-on-droplet.sh` (`-j1`, reconfigure only if CMake changed),
  `verify.sh` (systemd, journal, loopback, public routes, a real guest incl. one named `x`, row counts vs the backup),
  `deploy-daemon.sh` (the sequence; PLAN-ONLY without `--go`; refuses past an undeployed civic change). First real
  run: `deploy-daemon.sh --baseline 60f6745` (plan, read-only) on `deploy`. `tests/test_deploy.sh` (119 assertions,
  fake droplet behind a shim ssh) is mutation-checked (10 breakages) and found 2 real bugs in my own scripts;
  CI also runs `bash -n` + `shellcheck` on them. The reqjson fix is still not live (needs `deploy`). See
  `deploy-scripts-built.md`, `commons-daemon/deploy/README.md`. Still unbuilt: nginx `/guest`, backup retention.
- **2026-09-19 ~15:25Z -- reqjson DEPLOYED (daemon only), the first real run of the deploy scripts.** Brandon: `deploy`
  (plan-only: no drift, 10 files: `reqjson.hpp` + nine `deploy/` files) then `deploy` again (`--go --baseline 60f6745
  --guest-name x`). Backup `commons.sqlite.pre-a56eba3-20260919T152437Z` (integrity ok); `-j1` build, no reconfigure;
  restart; verify 32/32 incl. a real guest named `x` through nginx, journal clean, every row count unchanged. The
  droplet now has `/opt/commons-daemon/.deployed-rev` = a56eba3, so the next deploy needs no `--baseline`. Civic, client
  and nginx untouched. Unbuilt: nginx `/guest` limit (own deploy word), backup retention (backups now accumulate).
- **2026-09-19 -- `BUILD nginx /guest` built (config + CI tests; NOT installed, no droplet, no reload).** New
  `deploy/nginx-commons-http.conf` (`limit_req_zone $binary_remote_addr zone=commons_guest:1m rate=20r/m`, installed
  as `/etc/nginx/conf.d/commons-guest-zone.conf`) and one EXACT-match `location = /commons/presence/guest` in
  `nginx-commons.conf` (`burst=30 nodelay`, status 429, default HTML body on purpose: the client's `netMint` never
  checks status, a JSON body would loop it on 403). `/internal/` is never touched. **The numbers are my decision (no
  doc gave any): flagged for veto in `nginx-guest-built.md`.** `tests/test_nginx_guest.sh`: 12 static assertions
  (mutation-checked, 8 breakages) + a behavioural half with a REAL nginx in front of the real daemon, CI-only (no
  nginx locally). Needs its own `deploy` (nginx by hand). Unbuilt: backup retention; client `netMint` r.ok check.
- **2026-09-19 ~15:46Z -- nginx `/guest` rate limit DEPLOYED (nginx only, by hand).** Brandon: `deploy`; Grok had
  accepted the rates + the HTML 429. Read-only first (nginx 1.24.0, conf.d empty and included, no front proxy, live
  `/commons/*` blocks identical to the repo's), backup `liquidgov.bak-pre-guestlimit-20260919T154553Z`, zone installed
  as `/etc/nginx/conf.d/commons-guest-zone.conf`, the exact-match guest location inserted after `/commons/placed/`,
  `nginx -t` ok, reload. Verified: exactly one active `limit_req`, 5 mints all 200, `verify.sh --guest-name x` 22/22,
  civic 200, public `/internal/` 404, no 'limiting requests' in the log. The 429 itself was proven in CI, not by
  flooding. Side observation (pre-existing): something polls `GET /commons/recipes` every ~6s and 404s (not proxied
  by design). Unbuilt: backup retention (Brandon picks how many / how old); client `netMint` r.ok check.
- **2026-09-19 -- `BUILD backup retention` built (files + fake-backup tests; NO droplet, nothing deleted anywhere).**
  `deploy/prune-backups.py` (pure decision) + `prune-backups.sh [--delete]` (dry run by default). Brandon's policy: delete only
  if BOTH outside the newest 14 AND older than 30 days; newest 14 stay however old; anything within 30 days stays however
  many; never the newest; never < 24h (a hard floor, redundant at the defaults, independent if `--days` is overridden).
  Naming inspected: deploy `pre-<label>-<stamp>` and the daemon's `pre-reap-<stamp>` are ONE scheme, ranked together and
  treated identically; non-backup names are ignored. Age = the younger of the name's stamp and the mtime. Refuses on a failed
  listing or a future-dated backup. `test_retention.sh` (47) + `prune_backups_unit.py` (361, incl. 300 random directories),
  mutation-checked (12 breakages). Not scheduled; not run by `deploy-daemon.sh`. Today the droplet has ~a dozen backups all
  < 2 days old, so a first real run (a dry run, on `deploy`) would delete nothing for ~a month. See `backup-retention-built.md`.
- **2026-09-19 ~16:10Z -- backup retention: first real run = a DRY RUN on the droplet (Brandon: `deploy`).** Read-only,
  nothing deleted: 12 backups, keep 12, delete 0 (all from the last ~24h; dir 1.4M, disk 57%). **Finding:** every backup
  also has `-wal` (0 B) and `-shm` (32 KB) sidecars (24 of 36 files); the pruner ignores them, so deleting an old backup would
  orphan its sidecars. Proposed, not built: delete a backup's exact `-wal`/`-shm` together with it. Needs Brandon to name it.
- **2026-09-19 -- `BUILD sidecar pairing` built (files + tests; NO droplet, no `--delete`, nothing deleted anywhere).** Follow-up to the
  retention dry-run finding: `<backup>-wal`/`<backup>-shm` (EXACT names) now delete WITH their backup (sidecars first, then the
  backup; each name re-validated, and a sidecar's backup must be in the DELETE set) and are kept with a kept one; they never
  count in the newest 14 and are never judged by their own age. A sidecar whose backup is gone is an ORPHAN: reported, never
  deleted. The dry-run list is newest-first with each backup's sidecars right under it. Summary line now counts backups,
  sidecars, orphans, ignored. Unit checks 361 -> 378, `test_retention.sh` 47 -> 76 (real sidecar files, orphans, look-alikes),
  mutation-checked (8 more breakages). Today's droplet dry run would read `12 backups: keep 12, delete 0; sidecars: keep 24,
  delete 0`. Still not scheduled; no `--delete` until something is old enough (~a month). See `backup-retention-built.md`.
- **2026-09-19 -- `BUILD netMint check` built (client only; tested; NOT deployed).** `commons/index.html` `netMint()` now accepts a credential
  only from a 200 with string `id` + `secret` (the old one read any JSON, a JSON error body would have set `netReady` with `id` undefined
  and looped on 403s), retries every failure with capped backoff (2s,4s,8s,16s,30s cap, +/-20% jitter, honours a capped Retry-After)
  instead of leaving the player local-only until reload, and shares ONE mint loop between startup and `netHandleAuthFailure()`. The
  numbers are my decision (veto in `netmint-built.md`). `tests/netmint_unit.js` runs the SHIPPED block under node (fake clock, scripted
  fetch): 88 checks, mutation-checked (11 breakages), in CI via `test_netmint.sh`. Verified in a real browser against a real daemon:
  injected 429s retried at 0/1.7/6.5s then recovered; a real daemon restart gave 3 simultaneous 403s -> ONE mint -> rejoin. Client deploy
  needs `deploy` (`drift-check.sh --tree client --baseline <live commit>`, then `sync-client-to-droplet.sh`; no daemon restart, no nginx).
  Parked: `index.html` split, `/health` hash.
- **2026-09-19 ~16:47Z -- netMint check DEPLOYED (client only).** Brandon: `deploy` twice (read-only client drift check, then the
  sync). No drift; only `index.html` changed (sha256 8a74adbb..., byte-identical to the repo); `.deployed-client-rev` = `b04970e`
  (outside the served dir); daemon NOT restarted (`ActiveEnterTimestamp` 15:24:54Z, `NRestarts=0` unchanged); `verify.sh --guest-name x`
  22/22; live page has the `netMint:begin` block. The tree guard first refused on a stray `deploy/__pycache__/*.pyc` made by my
  retention unit test; deleted it and fixed the cause (`b04970e`: no bytecode written, syncs exclude `__pycache__`/`*.pyc`).
  Parked: `index.html` split, `/health` hash. `/commons/recipes` 404 left alone.
- **2026-09-19 -- `BUILD /health commit hash` built (daemon + verify.sh; tested; NOT deployed).** `GET /health` is now
  `{"status":"ok","commit":"<sha>[+dirty]|unknown"}` (additive: nothing parses the body; loopback-only). The droplet is not a git clone and
  deploys build with NO reconfigure, so the value is generated on EVERY `cmake --build` (custom target running
  `cmake/gen_build_info.cmake`; header rewritten only on change; only `src/buildinfo.cpp` includes it): env override > `<src>/.deployed-rev`
  (the marker the sync writes before the build) > git (+dirty) > unknown; only sha-shaped tokens get through (it lands in a C string and JSON).
  `verify.sh` now fails if the RUNNING commit differs from the marker (a stale, unrebuilt or unrestarted binary); skipped with a note when
  either side is missing. `test_health.sh` (57, incl. the real no-reconfigure incremental rebuild) + `test_deploy.sh` (131), mutation-checked
  (8). Format is my choice (veto in `health-commit-built.md`). A deploy will reconfigure (CMake changed). Parked: `index.html` split.
- **2026-09-19 ~17:23Z -- `/health` commit hash DEPLOYED (daemon).** Brandon: `deploy` twice (read-only plan, then `--go`). No drift; backup
  `commons.sqlite.pre-d9363b5-20260919T172306Z` (integrity ok); sync wrote the marker, then a RECONFIGURE + `-j1` build (CMake changed) picked it up;
  restart; verify 33/33 including the new check that the running commit equals the deployed revision (`d9363b5d7aa5`). Live `/health` now
  `{"status":"ok","commit":"d9363b5d7aa511beae70de082bb3203ad33e37ec"}`, loopback-only (`/commons/health` is 404 publicly). Row counts unchanged. Parked: `index.html` split.
- **2026-09-19 -- `BUILD index.html split`: asked before building (no spec).** No doc says how (only "one vanilla file, no build step"), and the
  file is 2,243 lines / 103 KB of shared global state (106 vars, 68 functions, load-order-dependent, one `"use strict"`). `CLAUDE_SPLIT_QUESTION.md`
  gives options (A classic `<script src>` files in the same global scope, recommended; B ES modules; C extract only CSS + networking; D don't) and
  the traps (`"use strict"` is per file; a multi-file deploy is not atomic: two-phase client sync). Proof plan: byte-equivalence of the JS,
  `node --check`, a CI file-reference test, a real-browser before/after parity check. Waiting on Brandon's one-letter answer; no code written.
- **2026-09-19 -- `BUILD index.html split` (A) built and proven; NOT deployed.** Brandon: "BUILD A. Prove load order in CI. Two-phase sync.
  Do not deploy." (`GROK_SPLIT_ANSWER.md`: classic `<script src>` files, one global scope, no modules.) `commons/index.html` is now a 42-line shell
  + `style.css` + eight scripts (`js/world, interiors, sprites, actors, net, input, interact, main`), each starting with `"use strict";`. Proof:
  the JS minus the added directives equals the old script body byte for byte; `tests/client_files_unit.js` loads the REAL files IN TAG ORDER under a
  stubbed DOM and fails on any ReferenceError (hoisting is what the split loses), checks the load-time code really ran by its effects, and its
  wrong-order controls throw; mutation-checked (11); a real-browser old-vs-new comparison found identical globals, function sources, values, baked
  world and sprite atlas, and identical runtime behaviour. Two-phase client sync (`index.html` last; nothing lands if phase 1 fails; 148 deploy
  assertions, 3 mutations). netMint tests repointed at `js/net.js`; CI gate 5 now matches any `commons/` file. See `split-built.md`. A client deploy
  needs `deploy` (client only, no daemon restart).
- **2026-09-19 ~18:17Z -- `index.html` split DEPLOYED (client only).** Brandon: `deploy` twice after review (CI green on `bf377c7`). Read-only client
  drift check first (no drift, exactly the ten expected files). The sync was delayed by a Windows host error (WSL could not start, `0x800705aa`, host
  memory pressure; nothing had reached the droplet), then re-checked the plan and ran the TWO-PHASE sync (scripts/stylesheet/images, then `index.html`
  last). All twelve client files identical to the repo on disk and as served (JS as `application/javascript`, CSS `text/css`); the live page has 8 external
  scripts and 0 inline; `.deployed-client-rev` = `cb982a4` (outside the served dir); daemon NOT restarted (17:23:28Z, `NRestarts=0`);
  `verify.sh` 23/23; a real browser on the live page entered as `x`, minted and joined, console clean. Parked list is now empty (`index.html` split done;
  `/commons/recipes` 404 left alone by instruction).
- **2026-09-19 -- `BUILD connection status`: asked before building (no spec).** My own suggestion, so no doc specifies it. Today a visitor gets NO signal
  when the client can't reach the daemon (move/poll failures are swallowed at ~10 sites; only the mint retry loop warns in the console). `CLAUDE_STATUS_QUESTION.md`:
  A = a small DOM badge outside the canvas (accessible, `role=status`), shown only after the problem lasts (4s with no credential, or 6 consecutive
  move/poll failures), hidden on the next success, one "Reconnecting…" line, observe-only (no request or retry change); B = on-canvas; C = reuse the
  toast; D = nothing. Waiting on Brandon's one-letter answer; no code written.
- **2026-09-19 -- `BUILD connection status` (A) built and tested; DEPLOYED (client only) from bd50119 on Brandon's `deploy`, verified: served files byte-identical, drift-check clean, verify.sh 23/23, daemon untouched.** Brandon: "BUILD connection status: A, as written." A small "Reconnecting…" pill,
  top centre, outside the canvas (`#connStatus`, `role=status`, empty when clear, paper/ink from the existing CSS, `z-index:15`, `pointer-events:none`, well away from the d-pad).
  State machine in `js/net.js` (`netStatus:begin/end`): shown when a mint loop has been pending >4s OR 6 consecutive heartbeats failed; hidden on the next success; no
  stay-clear timer; observe-only (no request, no retry change). **Deviation flagged:** the heartbeat is the move POST only, not the 1-4 polls per tick, or an ordinary
  restart would show it after ~1s. `netstatus_unit.js` (31) + `test_netstatus.sh` (29), mutation-checked (14). That run found a real bug in my own wrappers: under Windows node the
  unit's exit code was lost (`PIPESTATUS` after `$(...|tr)`), so a failing unit passed locally (CI unaffected); fixed in netmint/client_files/netstatus. Real browser
  + real daemon (frame loop driven from a timer, the pane being hidden): badge at the 6th failed heartbeat, hidden 257ms after the daemon answered, a refused remint
  shows at 5s and clears on recovery, no overlap with the d-pad on desktop or a 375x812 phone. Client deploy needs `deploy` (index.html, style.css, js/main.js, js/net.js). See `connection-status-built.md`.
- **2026-09-19 -- `BUILD` objects, step B (region scatter) built, tested and DEPLOYED (d2b6859, client only, verified).** Brandon: "can we finish populating the game with objects?" -> question doc (`CLAUDE_OBJECTS_QUESTION.md`) -> "all of that" -> my order B, A, C, D -> `build`
  (taken as step 1). B = 168 props from the six existing prop tiles (`SCATTER` data in `js/world.js`, placed last by `scatterRegions()`, plain ground only, refusals recorded). A test forbids the spine, jog, region seams, name strips,
  door/sign rings, spawn, waypoints and flood-fills the real collision array so nothing is sealed (14 + 7, 9 mutations; a first sealed-pocket mutation was vacuous and was redone). **Found and fixed a live bug:** `forceRedraw` was declared in
  main.js (last) but assigned by sprites.js image `onload`s, so a cached image firing between two script tags threw a strict-mode ReferenceError (benign in effect; declaration moved to sprites.js; `client_files_unit` now fires image onloads after each
  script, 55 checks). Client-only deploy needs `deploy`. Still to come, one BUILD each: A (lab lookups, daemon migration), C (house furniture), D (pick-ups, needs a question doc). See `scatter-built.md`.
- **2026-09-20 -- `BUILD pwa` built, tested and DEPLOYED (9e4611a: client, then civic with chown before the build; verified live).** Social and Civic get their own webmanifests (`/manifest-social.webmanifest` start_url `/feed`, `/manifest-civic.webmanifest` start_url `/proposals`, the shells' own standalone redirect targets, not the shells),
  picked by `page_wrap` by title like Mail; The Commons gets `commons/manifest.webmanifest` (start/scope `/commons/`) + generated apple-touch-icon/192/512 (`art/make_icons.py`, the plaza from above, maskable-safe) and the /apps tile shows it. `pwa_check.py` 71 checks, 16 mutations (Mail as a control caught my extractor bug; a first no-op sync-exclude mutation was redone).
  Real browser (Commons under a /commons/ prefix) and real compiled civic verified locally. Not tested on a device; iOS ignores manifests. Deploy order was client first, then civic with chown BEFORE the build. **nginx follow-up DONE on `deploy nginx`: `/commons/manifest.webmanifest` was `application/octet-stream` (stock mime.types has no webmanifest); fixed with an exact-match location + `default_type application/manifest+json` (NOT a `types{}` block, which would replace the whole table for `/commons/`); other types verified unchanged; mirrored in `deploy/nginx-commons.conf`; see `pwa-built.md`.**
- **2026-09-20 -- `BUILD www apex` built, tested and APPLIED (nginx only; verified live; certbot renewal proven by dry-runs before and after).** `www.liquidgov.us` served a full second copy of the site (200 on every path; `http://www` stayed on www; one cert already covers both names). Two small nginx edits (`deploy/nginx-www-apex.py`, idempotent, refuses unknown shapes):
  443: `if ($host = www.liquidgov.us) { return 301 https://liquidgov.us$request_uri; }` above the locations; port 80: certbot's www redirect goes straight to the apex. `server_name` keeps both names (cert + certbot unchanged). `deploy/nginx-www-apex.sh` = plan-only by default, `--go` backs up, `nginx -t`, reloads, verifies, auto-rolls-back.
  `test_nginx_www` 34 + a real-nginx behavior script (17, control included), the latter also run once on the droplet's own nginx as a scratch loopback instance (removed; live config md5 unchanged). Applied on Brandon's `deploy` after waiting for another session's certbot dry-run to finish (a certbot run edits nginx config temporarily); baseline and post-change `certbot renew --dry-run --no-random-sleep-on-renew` both succeeded for both names; backup `/var/backups/nginx-www-apex-20260920T041810Z/`. Grok's 'acme-challenge on www must not 301 to apex' line does not hold (nginx authenticator; proven by the dry-runs). **Found separately and fixed by another session:** certbot had been crashing (pip `cryptography 50` shadowing apt's); see `certbot-fix-built.md`. See `www-apex-built.md`.
- **2026-09-20 -- certbot was broken on the droplet; FIXED and verified (no service restarts, nginx untouched).** `certbot.service` failed 2026-09-19 14:41 UTC (exit 1); `certbot --version` crashed `AttributeError: module 'lib' has no attribute 'GEN_EMAIL'`. Cause: `pip install pywebpush` (2026-09-16) put `cryptography 50.0.1`/`cffi 2.1.1` in `/usr/local/lib/python3.12/dist-packages`, which precedes the apt tree on `sys.path` and shadows apt `cryptography 41` that apt `pyOpenSSL 23.2` needs. Can't downgrade (pywebpush needs cryptography>=47) or upgrade pyOpenSSL (<=25.3 same error, 26.x dropped `X509Req` that certbot 2.9 uses). Fix = `PYTHONPATH=/usr/lib/python3/dist-packages` for certbot only: systemd drop-in `/etc/systemd/system/certbot.service.d/pythonpath.conf` + `/usr/local/bin/certbot` wrapper. Proven: `certbot --version` 2.9.0, `certbot renew --dry-run` all simulated renewals succeeded, `systemctl start certbot.service` exit 0 (Result=success). Cert expires 2026-12-11. Rollback + backups (`/var/backups/certbot-fix-20260920/`) in `TheCommons/certbot-fix-built.md`. Lesson: a system-wide `pip install` into `/usr/local` can shadow apt libs and silently break unrelated tools; prefer a venv for `webpush.py`.
- **2026-09-20 -- `webpush.py` now runs from its own venv (`/opt/lg-webpush-venv`); the 20 system-wide pip packages that shadowed apt are gone.** Root cause of the certbot break removed; certbot works bare again (the PYTHONPATH drop-in + `/usr/local/bin/certbot` wrapper were REMOVED later the same day; verified `systemctl start certbot.service` exit 0). Run `/opt/lg-webpush-venv/bin/python /opt/liquidgov/deploy/webpush.py <cmd>` (nothing automated calls it; `send` is manual). `deploy/webpush.py` + `deploy/webpush-requirements.txt` now tracked. Details + undo in `TheCommons/certbot-fix-built.md`. Never `pip install` into `/usr/local` on the droplet; use a venv.
- **2026-09-20 -- `BUILD workshop` (Brandon: "A, E") built, tested and DEPLOYED (client only, `e6bd485`; two guest tabs on production saw each other and left cleanly).** The Workshop Lane shutter is now a real door (existing `T_DOOR_DEMO`, prompt Enter) into a plain 14x10 shared room, `instance:workshop` (lamp, six benches, worktable + stools, crates, rug, one sign "Sawdust and a bench. Nothing to make here yet."). Client only: no crafting, no items, no new art, no daemon/civic/nginx/schema change (daemon treats `instance:` keys as opaque). `world_scatter_unit` 15 -> 27 checks, incl. a flood fill of the room that caught a sign sealing a corner tile on its first run; real-browser guest test enters/reads/leaves, presence lands in `instance:workshop` and rekeys back; mine and demo house regression-checked. Not exercised: two clients, a bound player. **E: Privacy/Terms left as is** (a lawyer's job, not a rewrite). Also corrected the stale parked list: `rebind()` and orphan reaping were already built and deployed 2026-09-19. `TheCommons/workshop-built.md`.
- **2026-09-20 -- Commons coherence/fun review (analysis only, nothing built; `TheCommons/CLAUDE_COHERENCE_QUESTION.md`).** Findings: nothing to do after ~5 min (11 never-consumed lookups); the world is empty at real traffic (nginx: guest mints from 1-6 distinct IPs/day, incl. our own testing); the civic tie is one-way (Hall is a link, the Square notice post is inert); the 11 items are all historical ink/pigment chemistry but go nowhere; no audio, no first-visit guidance; oak gall and oak-gall tannin glyphs are pixel-identical. Options A polish+guidance (client), B visitors' book with preset stamps (daemon+client), C1 workshop swatch-combining, C2 materials unlock civic-canvas colours (civic), D nothing. Recommendation A, then B, C is Brandon's call. Awaiting his letters.
- **2026-09-20 -- the Studio SPEC written (nothing built; `TheCommons/studio.md`, review request `CLAUDE_STUDIO_QUESTION.md`).** Brandon's direction, in his answers: the point of the game is things to DO; realistic chemistry (real ingredients, roughly real times, no quantities); regions become material sources; pigments you make are your palette; the self-portrait lives in the GAME (not civic's `/me/portrait`) and starts in charcoal, unlocking colours; materials are consumed, sources renew; free starter charcoal stub. Civic is untouched (its drawing tool has unlimited colours + image import, so it cannot be gated). Reopens knowingly: crafting ban, 'no stoichiometry/safety', counts-never-go-down, 'no menu'. Guests stay RAM-only (gather + session charcoal drawing); making/keeping needs an account. Five slices (daemon core, world sources, bench, easel+portrait, tuning), each its own BUILD. Open: time scale (real <=3d, 7d cap), recipe review owner, painting visibility (friends only). Recipe table is UNVERIFIED and must be reviewed before it is built. Waiting on Grok's `GROK_STUDIO_ANSWER.md` and Brandon's sign-off.
- **2026-09-20 -- Studio spec v2: Grok's review folded in (`TheCommons/studio.md`; nothing built, no catalog rows, no BUILD).** Applied: discovery = you hold every input (free-form side mix is rate-limited, cannot discover); homemade vinegar cut (Orchard keg instead); whiting -> shell white; alum crystals dropped; woad rewritten as a simplified lake; painting never refunds; server rules tightened (transactions, UNIQUE gather, no client timestamps, palette indices only, poll rate, orphan reap extended, guest path local); slice order now 1 daemon core, 2 charcoal easel + portrait + phone layout, 3 world sources + bench + notebook + lab conversion (one deploy), 4 rest of ladder, 5 tuning. My three deviations from Grok, flagged: pixel-level painting cost (his net-count formula allows free repaints), portrait GET visit-checked from slice 2, charcoal 8 h with the real-world figure disputed (2-4 h vs ~10 h). Recipe marks are Grok's and still unverified: I reference-check each line before `catalog.sql`, then Brandon signs the frozen table. Holding for that sign-off.
- **2026-09-20 -- Studio reference pass done (`TheCommons/studio-refs.md`; nothing built, no catalog rows, no BUILD).** Grok accepted the three v2 deviations and pinned pixel index 0 = empty. Verbatim, greppable sources (Cennini/Herringham, Merrifield + Bolognese MS., Theophilus/Hendrie, Pliny 35.25): charcoal, red ochre, carbon ink, verdigris, weld lake confirmed; shell white, gum, tannin, iron-gall, madder, woad confirmed with corrections; iron acetate and several game times unsourced. Cennini ch. 33 does give a clock ("till morning", overnight), which supports 8 h. Woad source has no ash. Six proposed corrections C1-C6 await Brandon; the notebook shows a real range only where a source gives one. Not verified: Coates 10 h, shell-white weathering; CAMEO was 403. Holding for sign-off.
- **2026-09-28 -- PresenceStore clock injected (was: reaped on real wall clock while `Occupant.last_update_ms` was stamped by main.cpp's test-clock-aware `now_ms()`).** `PresenceStore(Clock)` in `presence.hpp`; main.cpp passes `now_ms`; default (empty) = real clock. Constraint now documented in the header. `test_studio.sh` workaround removed and a reap-follows-test-clock check added (confirmed it FAILS on the old code, passes on the fix; full 26-file suite green). `guest.cpp` still reads the real clock for its own TTLs, but it stamps and reaps internally so it is self-consistent -- no trap there. Uncommitted.

## Split: civic site vs game

- **2026-09-16 — decided.** Persistent FireRed/Sims/AC-style world is **not**
  grown inside `liquidgov_http` / LGP1. Two programs, one person.
  - LiquidGov keeps: login, friends, mail, votes, profile canvas, Juketube, civic pages.
  - Game keeps: tiles, positions, houses, inventory, lab, mining, visiting.
    Own process, own files. A game write must not take down Genesis.
  - Glue only: session cookie, friends as a *query* (not a copied graph),
    `/proposals` as an in-world door, blob+feed for photo mode. Game stores
    `lg_username` as a reference, never a second login.
  - Juketube-shaped hangout on this droplet (one screen, ~2 Hz, RAM presence)
    is a waiting room, not the concept. Don’t sell it as Animal Crossing.
- Chemistry: real *names and reaction classes* as a lookup table is fine.
  Real stoichiometry/safety is a different product. Mining without heat/grade/alloy
  is named fantasy tiers. Neither is a reason to pick this stack.
- Profile canvas = bitmap to hang on a wall. House layout =
  `{item, x, y, rot90}` rows. Don’t reuse the canvas as the house.
- **Do not start the game in this repo until Brandon explicitly says so.**
  When it starts, it is a sibling process + SQLite, not new LGP1 tags.

## Data: what lives where

- **2026-09-16 — decided.** Normalize catalogs and ledgers. Don’t NoSQL the civic site.
  - LGP1 stays the civic store. It is a tagged dump, not a database.
    Do not add game tags. Do not migrate LiquidGov to Mongo “to modernize.”
  - Game ledger (inventory, placed objects, recipes, trades): SQLite,
    ~3NF catalogs / 2NF instances. Trades are `BEGIN/COMMIT`.
    LGP1 cannot roll back a half-write.
  - Hot presence (position, who’s in the plaza): memory (or Redis later).
    Snapshot on timer/leave, not 10 Hz flushes.
  - House snapshot JSON is a *cache* derived from `placed` rows, not the only copy.
  - NoSQL that *does* fit: key–value for live presence; a document for a house
    blob you always load whole. Not the inventory ledger. Not a second friend graph.
  - If a column can be derived from LiquidGov or from `items`, don’t store it as truth.

## Messages home-screen app

- **2026-09-16 — confirmed live.** Checked droplet: `src/http_server.cpp` md5
  matches the WSL working copy exactly, `liquidgov_http` binary built the same
  day, service active. Curled production: `/mail` renders via
  `page_wrap("Messages", ...)` with `mail_install_hint()` in its own body (not
  site-wide), `manifest-mail.webmanifest` serves `start_url:"/mail"` live, and
  `/app/mail` already carries the Messages-specific `apple-mobile-web-app-title`
  / icon / manifest. Nothing left to implement — this item is closed.
  - Original ask, for context: iOS “Add to Home Screen” captures the
    **current URL**, so `/apps` → `/app/mail` (install copy) → icon would open
    the install page, not `/mail`. Fix was to nudge the install hint from
    inside live `/mail` itself with page-specific manifest/icon/title, which
    is what's now confirmed running.
  - Already-saved wrong icons from before the fix cannot be patched; those
    users delete and re-add from `/mail`.

## Standing product pins (copy, don’t reopen)

- Claude is sole droplet implementer of `/opt/liquidgov`. Grok: GitHub + prompts, no SSH.
- `lg-deploy` rsync as root left `/opt/liquidgov` `root:root` → store write SIGABRT.
  Directory must be `liquidgov:liquidgov` after rsync. Never rsync `*.lgp`.
- Friends are mutual accept. No age checkbox. Phone is signup binding, not login.
- Platform community ≠ AVND locality. No React/Vite/Node UI.
- Do not put game world state in LGP1 or `http_server.cpp`.
