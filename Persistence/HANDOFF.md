# Handoff for the next Claude (written 2026-11-16, end of a long session)

Read this, then `Persistence/README.md` (the standing rules) and the newest entries at the top of `Persistence/notes.md`
(newest first; it holds every decision, rejection and parked idea with the date). This file is the map; notes.md is the record.

## 1. Who you are working with, and how

- **Brandon** owns LiquidGov.US (repo `Bbottin92/liquidgov-us`). Builds it for real people, but almost nobody uses the site yet.
- **Speak plainly.** Answer the question he asked FIRST, in short words, before building anything. When he reports something
  ("theres nowhere to type a name") check what he actually SEES (a label), do not explain the code, and do NOT change things he
  only reported (he got angry at this once: "you're trying to sabotage me"). He is not a programmer; say what a thing is.
- **Words that mean something:** "merge" = merge that branch to master (you do it with `ship.py merge`). "go" = he runs the live
  deploy himself. "ok" is NOT "merge" (ask again). A deploy needs his explicit "merge" for the site branch, "merge" for the
  deploy pair, then he runs `go`.
- **Commands for him must be paste-ready**, no placeholders, one command per fenced `bash`/powershell block. He uses
  PowerShell on Windows; commands that reach the server go through WSL with a PowerShell here-string (see section 4).
  Never paste your own tool output at him as something to run (he pasted a python snippet into PowerShell once).
- **Record in `Persistence/notes.md` the SAME turn he decides, rejects or parks something**, a dated bullet at the top. Then
  commit and push (a stop hook also asks for this).
- **Prefers efficiency.** He asked for logical shortcuts (e.g. a targeted check instead of an hour of CI). Say honestly what you
  did not verify. He said take his shared-context folder (on his PC only, unreachable from here) "with a grain of salt".
- He delegated: "take what youre confident will bring substance and value." The chosen order is in notes.md (2026-11-16 entry).

## 2. The environment you run in

- A cloud container. **It cannot reach liquidgov.us or Wikipedia** (network policy), so the live site is checked only by what
  Brandon pastes back. All tests use local stand-ins.
- Repo clones (worktrees of one repo): `/home/user/liquidgov-us` (main; branch `wip/126-doorbell-notes`), `/home/user/lg-wiki`
  (branch `wip/128-sign-overlap`, the work in progress), `/home/user/lg-hints`, `/home/user/lg-gate` (old, done).
- **Build dirs (Debug)** in the scratchpad `/tmp/claude-0/-home-user/83993a40-57ca-5817-8b89-619133e9f552/scratchpad/`:
  `lg` (source = main checkout), `lgh` (source = lg-hints), `lgw` (source = lg-wiki). Note the path is `-home-user`, not
  `-home/user`; the wrong path once ruined a CI run. Check `ls $B/liquidgov_http` before using one. `cmake --build . -j8`
  there rebuilds incrementally. Binaries: `liquidgov_http` and `liquidgov_e2e_seed`.
- Browser checks: `LG_BROWSER=/opt/pw-browsers/chromium-1243/chrome-linux64/chrome python3 tests/browser/<check>.py . <outdir>`.
  Some need `phone_serve.sh` running (port 18140) with the work dir `/tmp/lg-phone-check`.
- e2e: `bash tests/e2e/<name>.sh $B/liquidgov_http $B/liquidgov_e2e_seed`. Test ports 18080-18159 (naming.toml); 18140-18149 are
  reserved for phone/rollback tools. Run ONE full local CI at a time (`scripts/local_ci.py --build $B`, about an hour); it is now
  optional (see 3).
- **Every new wip branch needs a fetch refspec**: `git config --add remote.origin.fetch '+refs/heads/<b>:refs/remotes/origin/<b>'`
  then `git fetch`. Tag pushes are refused: use branches. **Never force-push or rewrite a pushed branch** (the tool refuses and
  your own checklist forbids it): copy the commits onto a fresh branch from `origin/master` with `git cherry-pick`, resolving
  `Persistence/notes.md` conflicts by keeping both sides (a short python regex does it; do it in a loop).
- Branch names `wip/NN-name` (NN = next unused number across ALL branches: last used 128). Run
  `python3 tests/repo/naming_check.py .` before pushing.
- `Persistence/notes.md` has a checklist entry "Claude's own checklist" (2026-11-16). Follow it.

## 3. How code gets live

- Stack: C++ server `liquidgov_http` + LGP1 file store, vanilla JS/CSS assets in `liquidgov-platform/assets/`, pages via
  `wrap()`. A new store record type needs: header doc in `file_store.hpp`, read/write pairing, `deploy/<name>-undo.py` with a
  `<NAME>_TAGS` tuple, a rollback suite `tests/rollback/<name>_undo.sh`, and `tests/repo/lgp1_consistency.py` +
  `undo_lists_check.py` pass.
- **Committed test suites are frozen**: if a committed test fails because of your change, change the code (e.g.
  `ux_jump_autofill` B3 caught a real conflict), do not edit the test. New coverage goes in a new file.
- `scripts/ship.py check|merge <branch>` (you run, the branch must exist LOCALLY: `git branch -f <b> origin/<b>`) and
  `ship.py deploy <name> "<purpose>"` / `go <name>` (Brandon runs, in his main checkout `C:\Users\Brandon\liquidgov-us`).
- **The CI gate is warn-only** (Brandon 2026-11-16): a missing CI result prints a warning and does not block; a FAILED run,
  a non-ALL-PASS phone verdict, an unpushed branch or a non-fast-forward still stops `ship.py`. The dry-run line
  `CI_GATE_WOULD_REFUSE ... SHIP_NOT_READY` is a known leftover (fix is in `wip/122-live-searchnames1`, not merged); `go` still works.
- **Deploy sequence:** you finish tests -> push the wip branch -> `ship.py check` -> Brandon says "merge" -> you `ship.py merge`
  (run it in the background and watch the output file; it takes minutes) -> Brandon runs
  `git pull --ff-only; python scripts/ship.py deploy <name> "<purpose>"` (a few minutes at the pair push; he must NOT press Ctrl+C)
  -> he pastes it -> you merge the pair branch (`wip/NN-deploy-<name>`; create the local branch from origin first) on his "merge"
  -> he runs `git pull --ff-only; python scripts/ship.py go <name>` -> you record LIVE in notes.md from the RECORD lines.
- **Gotchas met today:** (a) WSL `WSAETIMEDOUT` before the script starts = nothing changed; `wsl --shutdown` then
  `wsl.exe bash -lc "echo WSL_OK"`, then rerun. (b) A new record type trips `rollback_guard.py` ("hints: new record type");
  needs Brandon's explicit OK, then he runs the go script by hand with `LIQUIDGOV_NO_ROLLBACK_OK='<reason + undo plan>'` in a
  here-string (see notes.md 2026-11-16 hintswiki1 for the exact block). `ship.py` refuses overrides. (c) The refused attempt
  already made `/opt/liquidgov/liquidgov_http.pre-<name>`, which makes the next preflight refuse (`PRE_..._PRESENT`); compare it
  byte for byte with `/opt/liquidgov/build/liquidgov_http` over ssh and remove it only if identical. (d) In a PowerShell
  here-string do not use `$VAR` inside the bash text; write paths out.

## 4. State right now (origin/master = `4eede33`)

**Live:** the admin link preview (`/admin/preview`), search by name ("Name" field, names on person pools), Hints for newcomers,
Wikipedia summaries on your interests and search for any topic (`topic:<Title>` pools). All deployed 2026-11-16.
**Not verified (cannot reach the site):** a real Wikipedia lookup from the droplet; the Hints line in the real menu. Ask Brandon.

**Branches waiting:**
- `wip/126-doorbell-notes` (main checkout, `d32b8c3`): NOTES ONLY (doorbell design and decisions, inner-circle name, live entries,
  this file). Based on an older master, so it is not a fast-forward: copy its commits onto a fresh branch from `origin/master`,
  then `ship.py check` and merge on his word. Cheap, no deploy needed.
- `wip/122-live-searchnames1`: the dry-run wording fix (`ci-gate.sh` dry prints `CI_GATE_WARNING` in warn mode, `ship.py deploy`
  accepts it; tests in `tests/repo/`) plus older notes. Also behind master: copy onto a fresh branch. Its notes overlap with 126.
- `wip/128-sign-overlap` (`/home/user/lg-wiki`, `0fe7545`): the bug in section 5. NOT ready.
- `wip/118-hints`, `wip/124-wikipedia` are superseded (live via 123/125/127); ignore them.

## 5. In progress: names at channel mouths jumble (Brandon's phone screenshot of the live Groups pool)

**Symptom:** on a pool with many channels (Groups: about 10 groups), the group names written inside the pool at each channel's
mouth pile on top of each other at the top of the pool ("The Great Recall / LiquidGov Think Tank / Tim... / Tin Foil Hat...").

**Cause found:** two things. (1) `assets/lgmap_walk.js`, `mouth()` (about line 460): each sign is up to 22 px bold and up to 11
characters (about 145 map units) but mouths can be as close as 23 degrees apart (MOUTH = 140 units in `lgmap.js`), and at a
pool's maximum size they sit closer still. (2) Worse, `poolPlace()` in `assets/lgmap.js` (about line 730): when the first ring
of children is full, the overflow children fall to `look(60, false)`, which ignores the mouth-room rule and puts a second child
at the SAME direction as a first (measured: `group:many3` and `group:many6` both at 165 degrees, distances 670 and 1095), so two
mouths land on the same spot and a channel also runs through another pool.

**Reproduction (committed, failing):** `tests/browser/walker_signs_check.py` (fake graph, 10 extra groups, Groups opened, signs
measured). Run: `cd /home/user/lg-wiki/liquidgov-platform && LG_BROWSER=... python3 tests/browser/walker_signs_check.py . /tmp/out`.
G0 and G2 pass; **G1 fails**: worst overlap is signs at the same shore point (centres about 22 units apart, text still too wide).
Before the fix it failed with overlap -135.

**What is already changed in the WIP commit:** `mouth()` now computes `room` = distance to the nearest other mouth on the same
pool (new `shorePoint()`, `mouths`, `roomTo()`, and a first pass in `water()`), and sizes the sign: font shrinks to 9 px, then
the name is cut (min 3 chars + ellipsis). This stops the jumble for spaced mouths but cannot help when two mouths coincide.

**What to do next (my recommendation):**
1. Fix the placement fallback in `poolPlace()` so an overflow child never takes an angle already used: replace the final
   `look(60,false)` by choosing, among 72 directions, the one with the largest angular clearance from `used`, at the nearest
   distance that does not touch another pool (keep `look(8,true)` first). Re-measure with `/tmp/dbg_signs.py` style prints
   (child angles about the Groups pool) so no two children share an angle within about 10 degrees.
2. Keep the sign sizing as the safety net, but when two mouths are closer than about 3 characters of room, show only the
   nearer sign's text (hide the other) rather than 3-letter fragments; make G1 also accept hidden signs.
3. Run `map_world_check.py` (25 checks), `walker_controls_check.py` (82), `walker_instance_check.py`,
   `walker_proximity_check.py`, `walker_valve_check.py`, `walker_valve_types_check.py` (all report-only flaky families: rerun a
   failing one 3 times before blaming your change). `walker_controls_check` M1/M5 flake about 1 in 10.
4. e2e not affected, but run `map_graph.sh` and `map_pool_content.sh`. Then `ship.py check`, ask Brandon for "merge".
5. A screenshot at 390x844 deviceScaleFactor 2 of the Groups pool (cdp.py launch, see `/tmp/lg-wikishot` style scripts) makes a
   good before/after for Brandon.
**Pitfall that cost an hour:** never write `open(p,'w').write(open(p).read())` (it truncates the file first). One of my edit
scripts emptied `lgmap_walk.js` and the only symptom was "no signs at all". After any scripted edit run `wc -c` on the file.

## 6. The build queue and what is parked (full reasoning in notes.md 2026-11-16)

Chosen order: (1) done: Hints, Wikipedia connector; (2) done: search finds any topic; (3) NEXT in the original order: **related
topics** (Wikipedia's own "more like this" so a topic pool leads to neighbours); (4) Watch (videos for a topic; the server
already searches YouTube for the jukebox); (5) trail memory and the dice (client only); (6) the older list: cover photos,
thumbs counts/privacy/history/credibility. **First, fix the sign bug (section 5), and the inner circle:**
- **Inner circle** (Brandon decided: the one list of up to 12, also the doorbell's "close friends"; called "inner circle"
  everywhere). Needs a new LGP1 record, undo script, UI on profile/friends, tests. Prerequisite of the doorbell and people road.
- **Doorbell** (decided details in notes.md): rings with per-person rules (friends N per window, inner circle unlimited up to a
  60/hour hard ceiling, per-friend override, a reserved ring, quiet hours), the person rung chooses what the ringer may see,
  plain mirror in v1 (REVISIT the "strictest limit over the last week or month" idea later: Brandon says there is a reason; ask),
  ring effects chosen by the recipient (banner with who, notification button pulse, a sound they pick or record, phone push when
  offline). Non-friends cannot ring by default. Ring log kept about 30 days.
**Do not build yet:** learning model (no users), made or owned interest pools (rejected), a fourth road "things" (parked),
sellers/shopping (needs his answers: individuals vs businesses, local vs shipping), sandboxed spaces (large; plan in notes),
roads (people/places/ideas) beyond a first people road, room-per-pool, seeking-vs-wandering inference beyond a one-tap cue.
**Rejected today:** one new tip per visit (Hints stay one per page), unrestricted unlimited rings (60/hour cap).

## 7. Open questions to ask Brandon

- Does a real Wikipedia lookup and the Hints menu line work on the live site? (He was asked; no answer yet.)
- Interests as shared pools: no (rejected); interests are computed views. Should outside content (Wikipedia) show to signed-out
  people? (Default today: members only, the map needs login.)
- Sellers: individuals or businesses only; local only or shipping.
- Which doors an interest pool shows first (Watch, Related, People, Places, Compete); People needs the signup privacy step.

## 8. Lessons, so you do not repeat them

- Check the build path and that no other test run is active before a full CI run; one run at a time.
- When a field is renamed, grep every error message, label and test for the old wording.
- Do not rewrite pushed history; do not edit a frozen test to make it pass.
- A scripted file edit that ends in a truncate is silent. Verify file size and run the relevant check right after.
- If a safety guard stops a deploy, explain what it protects, what is unchanged, and ask for his explicit OK; never use an
  override on your own initiative.
- He sometimes pastes text meant for you into PowerShell and vice versa: label clearly what is for him to run.
