## Anti-drift & anti-waste rules (apply to every step)

### Scope lock
- Only one step is in progress. Name it at the start of every session.
- Work is allowed only if it directly produces one of the step’s done criteria.
- If a change does not move a done criterion from unmet to met, do not make it.

### Forbidden without explicit new instruction
- Exploratory refactors or “cleanup while we’re here.”
- New tests that do not prove a stated done criterion.
- Extra logging, debug endpoints, metrics, or comments.
- UI polish, animation, styling, or renaming unrelated to the criterion.
- Reading or modifying files outside the minimal set required for the current criterion.
- Speculating about later steps or alternative designs.

### Required discipline
- Before any edit, state: “This change satisfies done criterion X because Y.”
- Prefer the smallest diff that meets the criterion. One file is better than three.
- Commit only when all done criteria for the step are met, with message `spatial: step N complete`.
- After the commit, stop. Do not begin the next step.
- If uncertain whether something is required, leave it out.

### Measurable-benefit test
Every action must answer yes to all three:
1. Does this make a done criterion observably true?
2. Is there a smaller change that would also make it true?
3. Will a later step be harder if I skip this?

If any answer is no, do not do it.

### Assistant behavior
- Refuse requests that jump ahead, add scope, or “improve” beyond the current step.
- When blocked on missing information, ask once for the single fact needed, then wait.
- Never generate code, tests, or docs for steps that are not current.

# Spatial Evolution — Strict Sequential Program
Follow these steps in order. Do not start step N+1 until step N’s done criteria are met and committed.  
Only perform work that has a direct, measurable benefit to the current step’s done criteria.  
No exploratory refactors, no “while we’re here” cleanups, no tests that do not prove a done criterion, no token-spending speculation.

## Step 0 — Workspace distillation & organization
**Goal:** The repository is simplified, consistently structured, and aligned with common industry practice so later steps have a clean base.  
**Done when:**
- Clear top-level layout exists (src/, assets/, tests/, docs/, deploy/ or equivalent) with a short README or CLAUDE.md that maps it.
- Dead code, duplicate configs, and unused assets identified in the areas we will touch are removed or archived.
- Naming and file placement follow one consistent convention (documented in one place).
- Build, test, and deploy entry points are obvious and minimal.
- A single “current state” note lists what is intentionally left unchanged.
**Measurable benefit:** Later steps can locate files and add fields without hunting or conflicting patterns.  
**Stop rule:** Do not add spatial features, new records, or behavior changes. Organization only.  
**Anti-waste:** If a file or pattern is not in the path of the upcoming spatial work, leave it alone.

## Step 1 — Droplet position + instanceId (no behavior change)
**Goal:** Every presence/droplet record can store `x`, `y`, and `instanceId`.  
**Done when:**
- Record format accepts the three fields (defaults: x=0, y=0, instanceId="mainstream" or current room).
- Existing walker and map still work exactly as before.
- One minimal check confirms a droplet can be saved and reloaded with the new fields.
**Stop rule:** Do not add regions, filtering, or valves.  
**Anti-waste:** No extra tests beyond the one reload check.  
**Status:** Complete on branch spatial-step-1 (HereDot carries instanceId; JSON includes it; old clients ignore the field).

## Step 2 — Regions on existing boxes
**Goal:** Current map nodes have a simple region (center + radius or bounds).  
**Done when:**
- Box data includes region info.
- A pure function `isInside(droplet, region)` exists and passes one focused test.
- No visual or presence changes yet.
**Stop rule:** Do not wire it to the walker.  
**Anti-waste:** One test only; no property-based or visual tests.

## Step 3 — Walker updates instanceId
**Goal:** Joystick/touchpad movement tests regions and sets `instanceId`.  
**Done when:**
- Moving the drop onto a box region changes `instanceId`.
- Moving out restores the parent (mainstream).
- Drop/Scroll toggle and all existing controls still work.
- One manual or automated check: walk onto a profile box → instanceId updates; walk off → it reverts.
**Stop rule:** Do not filter rendering or change who is visible.  
**Anti-waste:** No additional logging, no UI polish.

## Step 4 — Droplet-mode proximity filter
**Goal:** In droplet mode only, render droplets/nodes that are Inside/Near/Aware.  
**Done when:**
- Far items are hidden or minimized while Drop is on.
- Scroll mode shows the previous full view.
- One check confirms the filter toggles correctly with the Drop/Scroll switch.
**Stop rule:** Do not add conduits or valves.  
**Anti-waste:** No new animation or styling.

## Step 5 — Instance-scoped presence
**Goal:** Presence and realtime updates only include droplets in the same `instanceId`.  
**Done when:**
- Presence payload is filtered by instance.
- One check: two droplets in different instances do not see each other; two in the same instance do.
**Stop rule:** Do not add entry restrictions.  
**Anti-waste:** No extra metrics or debug endpoints.

## Step 6 — Valve record + Open default
**Goal:** Instances have a valve; default is Open (everything still enterable).  
**Done when:**
- Valve object exists on instance records.
- Entry path checks valve and allows Open.
- All current boxes remain enterable (one regression check).
**Stop rule:** Do not implement other valve types yet.

## Step 7 — Additional valve types
**Goal:** Support Conduit, Invite, Private, Request, Closed.  
**Done when:**
- Each type has a working check.
- Inheritance copies parent valve at creation.
- Three focused checks: Open, Private, and Conduit.
**Stop rule:** Do not build eject rules or admin CSS.

## Step 8 — Dynamic instance creation
**Goal:** Creating a group/proposal/marketplace writes a new instance with region + valve.  
**Done when:**
- New instance appears as a walkable region.
- Creator can enter it.
- Valve inheritance works (one check).
**Stop rule:** Do not add eject or custom CSS.

## Step 9 — Eject / hazard rules
**Goal:** Instance can force a droplet out using the same style of check as valves.  
**Done when:**
- Admin-defined rule can change `instanceId` to parent.
- One check confirms eject occurs and presence updates.
- Visual feedback is optional and cosmetic only.
**Stop rule:** Do not add free-form CSS yet.

## Step 10 — Scoped admin CSS
**Goal:** Instance can supply CSS that applies only while Inside.  
**Done when:**
- CSS is sanitized and scoped.
- It cannot move droplets or bypass valves (one negative check).
- Edit path exists for admins.
**Stop rule:** Evolution complete for core spatial model.

## Step 11 — Proximity voice & video
**Goal:** Droplets can optionally share microphone and/or webcam; volume (and video visibility) scales with distance inside the same instance.  
**Done when:**
- A droplet can opt in to mic, camera, or both (explicit permission; off by default).
- When two droplets are in the same instance and within Aware/Near range, audio is audible and video is visible.
- Volume increases as distance decreases (simple inverse or linear falloff from Aware edge to Near center).
- Outside the range, or in a different instance, no audio/video is transmitted or received.
- Valve and eject rules still apply; media does not bypass them.
- One focused check: two droplets approach → volume rises; they separate or change instance → media stops.
**Measurable benefit:** Realtime presence gains sight and sound without global streaming.  
**Stop rule:** Do not add recording, effects, or media outside the current instance.  
**Anti-waste:** No UI polish beyond a clear mute/camera toggle; no tests that do not prove distance-based volume or instance scoping.

## Anti-drift & anti-waste rules (apply to every step)
- Only one step is in progress.
- Every session begins by stating the current step number and its done criteria.
- If a change does not directly satisfy a done criterion, do not make it.
- No “nice to have” tests, logs, refactors, or comments.
- Commit only when the done criteria are met: `spatial: step N complete`.
- Assistants must refuse work on later steps or unrelated cleanup until the current step is finished.
- Measurable benefit is required: each step must leave the system observably closer to the spatial model or cleaner for the next step. If it does not, skip or shrink it.
