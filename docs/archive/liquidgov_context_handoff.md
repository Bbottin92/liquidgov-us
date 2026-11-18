# LiquidGov.US / liquidgov-core — full conversation handoff

**Purpose:** Drop this file into a new conversation so the next agent does not re-derive decisions, re-encode settled types, or treat AI-drafted AVND text as Brandon’s decisions.

**Date of this snapshot:** 2026-09-08 (reconciled: kernel facts from this repo + platform/AVND gaps from the parallel Claude thread)  
**Owner:** Brandon Bottin  
**Repo on disk:** `/home/workdir/artifacts/liquidgov-core`  
**Smoke tip (14-event fixture, default RecordParcel encoding):** `6c1e5962ea0f7cfc`  
**AVND source text:** not in this folder. Re-upload the white paper into any new conversation that will cite sections. Do not reconstruct the document from summaries.

Do **not** encode from “continue.” Encode only on an explicit write, or when order-of-operations makes a type cheaper *now* than after more acts attach to a fused shape.

---

## 1. What this project is

Rebuild **LiquidGov.US** from scratch in **C++**, governance kernel first, no UI in the core.

Source framework: **Voluntary Natural Democracy (AVND)**. Three laws: do not steal / damage / deprive of liberty.

**Build order (settled from the first session, still in force):**

1. STL-only domain logic  
2. Hashable event-sourced ledger from day one  
3. Local persistence  
4. Networking last  

Kernel stays portable. Crypto and real-time media are **not** “just add a library to liquidgov-core.”

**LiquidGov-as-real-government does not exist yet.** Phase 2 requires a landslide vote (“>50% of the eligible citizen population”). However long that takes.

A separate **platform** (social / virtual world) is in scope as product, **not** inside `liquidgov-core`.

---

## 2. What is actually built (code)

Path: `artifacts/liquidgov-core/`

| Piece | Status |
|---|---|
| Domain `Polity` (no I/O) | Built |
| SHA-256 event chain, canonical encode/decode | Built |
| `LedgerFile` + `GovernanceStore` + `replay_into` | Built |
| `Replica` (tail flushed file, fail-closed) | Built |
| Identity / citizenship split | Built |
| Land + consign/dedicate/claim | Built |
| Business | Built |
| Genesis authority + Judgment + outcome + renounce | Built |
| `Locality` type only | Built (no events, no Office field) |

**Tests:** `smoke`, `persist`, `delegation`, `replica`, `identity`, `land`, `judgment`.

**CivicAct variant order (indices are load-bearing — append only):**

```
0  EnrollCitizen          // live shorthand: writes identity THEN citizenship
1  ResignCitizen
2  SetDelegation          // binary; capability exists; NOT the default posture
3  FileMotion
4  CastBallot
5  OpenSeat
6  ClaimSeat              // interim seating — retire when standing tally exists
7  VacateSeat
8  RecordParcel           // extra kind+bond only if not (CitizenAllocable, bond 0)
9  TransferParcel
10 OpenOffice
11 AppointOffice          // interim — retire with ClaimSeat
12 RemoveOffice
13 BindContract           // WRONG shape vs §5; do not migrate; do not extend
14 NoteAlignment          // WRONG shape vs §5
15 EnrollIdentity
16 GrantCitizenship
17 ConsignParcel          // exactly one of to | to_business
18 ReleaseConsignment     // holder, citizen consignee, or business controller
19 DedicateParcel
20 ClaimVacantParcel      // cap number unset → unbounded on purpose
21 OpenBusiness
22 JoinBusiness
23 AssertGenesisAuthority
24 RecordJudgment
25 NoteJudgmentOutcome    // field `by` must equal judgment.author_holder
26 RenounceGenesisAuthority
```

### Persistence rules (do not reopen)

- Polity has **no I/O**.
- `submit` = **apply then append + fsync**. Never write a rejected act.
- Fail-closed on content_hash / prev_hash / apply mismatch.
- Framing-incomplete tail: drop, boot prefix (empty if first write died).
- Truncate-on-load only after successful replay, `good_bytes` from **framing only**.
- Tamper must not truncate the file.
- `append` = one assembled record, `open(O_APPEND)` + `write(2)` + `fsync`.
- `EINTR` retries same offset; short `write` advances pointer; `n==0` errors.
- Named remainder: POSIX files are not PIPE_BUF-atomic; short-write loop can tear under single-writer EINTR/full-disk.
- Replica uses the **same** `read_all`. Never truncates. Never submits. On pull failure keeps last good Polity. Decode errors are caught and returned, not thrown through pull.
- Replicas must consume **flushed** records only — never in-memory Polity.

### Identity / citizenship

Document: identity first, citizenship opt-in on top. Kernel had collapsed them into `EnrollCitizen`.

- `EnrollIdentity` — standing, court party, Track 2 eligibility. No points.
- `GrantCitizenship` — accrual clock starts here (`vote_points` = complete years since **citizenship** start).
- `EnrollCitizen` **stays live**: apply writes **both** layers so old logs replay with `has_standing`. Same numeric id (`as_citizen` / `as_identity`). Not decode-only (apply is history and live). Two doors into the **same** state (identity+citizen), unlike AppointOffice vs election.
- IDs are caller-chosen, one map; collision → `person exists` / `identity exists`.

### Delegation (§6 in code vs posture)

- Weight live at tally: `floor((now − citizen_since) / year)`.
- `SetDelegation` is **binary** (full weight to self or one proxy per topic). Partial `points` on SetDelegation was added then **reverted** — split is for **office candidate allocation**, which does not exist.
- Reject-at-set if `to` already reaches `from`. `resolve_delegate` cycle-dump is backstop only.
- Ceiling is a Polity ctor param, `0` = unset. **Delegate ceiling is off §13** because delegation is no longer assumed default (AI-inserted). Direct voting is the posture; SetDelegation stays as dormant capability.
- Opt-in allocation in races (no proxy spends your candidate split) is now just “no default delegation” everywhere.

### Land

- `PoolKind`: CitizenAllocable, ReservedNature, Infrastructure, **Commons**.
- Bond = assessed-value **number on the parcel**, not a financial instrument. Stays with **title**. Title is always a citizen (or `None`).
- Consignment ≠ dedication. Consign = lease (one mechanism). Nature: never owned, never consigned, never transferred.
- Dedicate: title-holder **alone** (anti-seizure — a motion taking bonded land is the forbidden act). Nature/infra: title ends, bond 0. **Commons: title and bond stay** (still in the citizen allotment).
- Zero-bid / vacant: first active citizen `ClaimVacantParcel`. Same per-capita cap as purchased land — **number unset, claim is live and unbounded** until §13. Deliberate interim.
- Geometry/subdivision (access + min shape) is **§13**, no coordinate type. Data model did not wait on a map.
- RecordParcel compact encode: omit kind+bond iff `(CitizenAllocable && bond==0)` so pre-change hashes verify. Joint extra fields, not two independent optionals. Bond 0 means “no assessed value” in both eras.

### Business (closed)

- `OpenBusiness` + `JoinBusiness` only. Founder = first controller.
- No owner field, no RemoveController, no resign. Kick without a prior deal is theft. Liability is a §3 Judgment, not a field.
- Future `TransferBusiness` (not built) needs incoming consent **and** everyone currently liable.
- No points/ballots: `CastBallot.voter` / `SetDelegation.from` are `CitizenId` — type-level, not convention.
- Consign exclusivity enforced in apply: exactly one of citizen or business.

### §3 + genesis authority (wired)

- `JudgmentAuthor { Judge, Polity, Panel, Genesis }`.
- Genesis is a **stand-in for the whole office system**, not a fake Judge seat.
- `AssertGenesisAuthority{holder: IdentityId}` — standing required; second assert **hard reject**.
- `RecordJudgment` — subject required standing; harmed if not None; **only Genesis is live**; `author_holder` must be current genesis holder. Judge/Polity/Panel hard-reject even from a valid genesis caller.
- `NoteJudgmentOutcome{judgment, by, outcome}` — `by` must be that judgment’s **author_holder** (survives renounce).
- `RenounceGenesisAuthority` — only current holder. Afterward Genesis judgments dark until re-assert or seating. **Deliberate** that renounce does not require a successor (gating on a Judge would make seating a precondition of leaving genesis).
- Does **not** unblock §5 (SignGuarantee needs a real seat) or Case (seam 1).  
- Why this is not the §12 coup-provision problem (load-bearing; do not re-derive from “genesis exists” alone): §12 bars self-declared emergency powers because an official **already ruling over an existing citizenry** could extend their own power with no one inside the system able to consent or object. Genesis is different in **kind**, not degree — there is no citizenry yet whose consent could be bypassed; anyone joining during genesis already knows the arrangement going in. Founder/pre-launch, not incumbent-over-constituents. This distinction **expires** the moment real citizens exist without a real say in it — re-check once seating or real Phase 1 participation lands; do not assume the exemption holds forever. Same argument covers kernel genesis and platform sole control; do not copy this paragraph into both places.

### Locality (type only)

- `LocalityId::National = 0` seeded in `Polity` ctor. **Not an event.** Same class as ThreeLaws.
- No `OpenLocality`, no `Office.locality`, no membership field/event. Idle optional membership was refused (DeclareTicket / CaseId class).
- Formation + membership (land-tied vs independent) + immigration-vote scoping = **one §14 citizen vote**. National-only offices are enough to boot.

---

## 3. Discipline / anti-patterns (load-bearing)

- **Wrong event shape on a hash chain is a history rewrite**, not a migration. Append variant indices. Do not insert in the middle.
- **Two doors into different rooms** (AppointOffice vs elected seat; SignGuarantee-on-appoint; DeclareTicket without allocation; Office.locality on appoint) — refuse.
- **Two doors into the same room** (EnrollCitizen vs Identity+Grant) — allowed if apply always converges.
- **Do not invent §13 numbers** to make tests look alive.
- **Do not implement crypto in this kernel.** Silent-valid leak ≠ fail-closed hash bug.
- Sortition was considered then **abandoned for the kernel**. AVND scopes it to judicial review of cases (decline right), not lawmaking. No `DrawPanel` in Polity.
- Many AVND sentences were **AI-drafted, not Brandon decisions**. Treat like sortition: reopen, do not assume settled. Already reopened: sortition-as-default, executive pairing as pinned, delegation-as-default.
- Audit is **paused, not closed**. Unexamined candidates named and never checked: identity-verification mechanics beyond the §13 accreditation-standard vote itself; vote-point accrual/split as concepts (distinct from the ceiling, which resolved when delegation was demoted). Three-reopened ≠ audit-complete.

---

## 4. Parked / blocked (do not encode)

### Spine — office / seating (blocked on seam 1)

Pinned reasoning:

- Standing allocation poll, **no Open/Close race**.
- Strict majority of **allocated** points (silence ≠ no). Last holder until someone else clears 50% (transfer trigger, not vacant-on-drop). New office vacant until first 50%.
- Denominator (allocated-only vs all national points) is a **§13 mechanism vote**, not a locked interpretation. Landslide contrast is evidence for a ballot option.
- Opt-in allocation; proxy does not spend candidate splits.
- `AppointOffice` / `ClaimSeat` retire when this lands.
- Judge = ordinary elected office, locality-scoped when localities exist.
- `SignGuarantee` gates the **seating moment**, not appoint.

**Executive pairing reopened** (AI insert). §13, ranked with platform-ops:

- (a) joint ticket, pre-declared mates (purpose: no ideological split at the top)  
- (b) two independent offices, unprivileged succession  
- (c) separate election + VP auto-succeeds — **dominated, off the ballot**

Ticket-consent edges (two tickets, half-resign, withdraw-before-50%) wait on (a) vs (b).

### Seam 1 — two primitives, not one

| | Case petition | Election / ticket allocation |
|---|---|---|
| Prove | Count ≥ N distinct eligible | Sum of weights per candidate within budget |
| Class | Threshold / linkable ring | Homomorphic tally + range proofs (or other) |
| Deps | No trusted setup (rings) | Heavy (GMP/Paillier etc.) — deferred past networking |

**Case spec (settled, not implemented):**

- Eligible base = **§4 legal standing** (identity), not the vote roll. Citizen-only would exclude the paradigm outnumbered group.
- Exclusion thin and inverted from panels: accused cannot count toward N; affected group stays in.
- Disclosure: petition existence public; **count batched at N** — one proof event, not incremental 1-of-N (timing leak).
- Interim transparent quorum named as real, **not default** (prints a retaliation list).

**Election spec:**

- Individual allocations hidden; **aggregates public and live** (“immediately” has no close).
- Homomorphic tally-at-close **may not fit** live aggregates (decrypt-every-change).
- Token+plaintext-amount is a **candidate**, not a pin.
- Case-style batching **cannot apply**. Live per-allocation events reopen timing correlation. Named gap, no primitive swap.

### §5 Officials’ Contract

Real Brandon decision: honesty guarantee before office; breach → citizen vote case by case, using §3 graduated range (not bare pass/fail).

`BindContract` / `NoteAlignment` measure the **wrong concept** (topic-majority pledge + alignment score). Do not migrate.

`SignGuarantee` waits on real seating. Breach-as-ordinary-motion **inherits majority capture** — named property, not a bug. Liberty-clause Case is the insulated track.

### Case / Track 2

Nothing in kernel until invocation + petition-secrecy have a payload. No `CaseId` on CivicAct yet. Cooldown can be a numeric param later.

Two-track working assumption: ordinary official misconduct → liquid motion; liberty-clause / systemic majority-against-minority → future Case (random invite + decline, not compel). Track-2 invocation is the capture problem one level up — no clerk classifier. Options: objective triggers at adoption or standing petition with no gate. Hybrid fast-path must not let a predicate bypass N.

### Officials’ Mirror

Brandon’s actual decision. No field on Judgment yet.

---

## 5. §13 vs §14

**§13 — genesis-blocking (must have a value/mechanism before the legislature exists):**

- Executive structure (a) vs (b); (c) excluded  
- **Platform-maintenance branch structure and threshold** (same rank — includes “is admin the President?”)  
- Case N + cooldown  
- Office majority denominator (mechanism, not a scalar)  
- Land pool sizes, reassessment interval, auction duration, transition window  
- Land geometry / subdivision (access + optional min-shape number)  
- Identity-verifier accreditation standard(s)  
- Donation-trust formula (breadth-weighting concept kept; curve unset; proxy-donor gaming named unsolved)

**Working fit for platform ops, not encoded:** Exec does technical work, Judicial reviews after the fact (emergency-alone-then-prove maps onto existing Judgment), Civic publishes. 2-of-3 vs unanimous vs emergency vs removal-valve = the ballot. Unanimous defends collusion but deadlocks; emergency fires more often under a stricter default.

**§14 — invented / working defaults, re-votable:**

- Business controller-consent (closed in reasoning; code matches)  
- Ticket-consent edges (wait on executive a/b)  
- **One bundled vote:** locality formation + membership + immigration-vote scoping  

**Not votes:**

- §5 consequence = case-by-case vote (the mechanism)  
- §3 range = judicial discretion, not a knob  
- Business/entity was a missing type (now built)  
- Replica/consensus = engineering  
- Locality formation is **not** genesis-blocking (national-only offices boot Phase 1)

**Delegation ceiling dropped off §13** when delegation stopped being the default posture.

---

## 6. Document corrections that matter

- **Sortition** abandoned in kernel; if a future Case module uses randomness it is invite+decline, not draft.  
- **Split** on SetDelegation was wrong; split is candidates in one office race.  
- **Bond** is an attribute of title, not a third relation.  
- **Community vs locality** — never merge without text or a vote. §§8–11 “community” reads geographic/land-tied; membership-independent-of-title is invented and costly.  
- **§11 immigration** is a **national** standing proposal. “Citizens whose land and vote weight are affected” names **stakes**, not a local bypass of the cap. Apparent cap “crack” dissolved.  
- **Dedication vs consignment** share “title-holder initiates” but dedication ends title (except commons).  
- §13 pool sizes are **targets**; dedication is voluntary; targets may go unmet.

---

## 7. Platform (not kernel)

### Ordinary product features (confirmed wanted — not only the virtual world)

These were explicitly on the list **before** the 2D-world scope landed. Do not infer the platform is “just a world”:

- Social **feed / posts**
- **Communities** (app feature — still not AVND `Locality`)
- **Direct messaging**
- A page to **read the governance / AVND document**
- Identity / verification (one combined step for voting on the platform)
- Plus the world systems below
- Brandon also said **“and more”** — website scope conversation is **not finished**

### Landing / disclosure pages

Built earlier as **plain HTML/CSS**, no C++ backend:

- `index.html` — landing
- `who-holds-control.html` — genesis-authority disclosure
- shared `styles.css`

Those files are **not** in `artifacts/` in this project snapshot. They live in the website workstream. Placeholders still on the disclosure page as last described: `[Founder Name]`, `[date]`, placeholder contact email.

**Two corrections, both still true:**

1. Brandon wants **C++** in the stack (backend), not a static-only site as the end state.  
2. The landing page is **not final regardless of language.** It was drafted **before** full platform scope was known. Brandon has not finished saying what the public site should include. The C++ decision does **not** supersede or close the page.

Architecture if/when there is a live server: **C++ backend + ordinary HTML/CSS/JS**. Not WASM-as-the-UI for the first cut. A C++ static-site-generator is dead because feed/accounts/messaging/world need server state.

### Governance-adjacent platform facts

- Existing AVND social/community concept. Censorship-resistant, voluntarism.  
- Genesis-style sole control extends to the **website**, pre-Phase-2, not legally binding; treat-as-real intent. Disclose who holds it and that it is meant to end.  
- **Two clocks:** site control ends when a **working election system** exists; governmental authority stays Phase 2 landslide. Election *internal structure* is itself a citizen vote.  
- Access: anyone unverified; **one combined verify** unlocks voting (not the kernel’s two enroll steps — explicit product shortcut).  
- Platform “community” **≠** AVND locality. Unverified community ≠ standing.

**World slice (designed, not built, not in liquidgov-core):**

- One shared room, two people see each other move. Hand-rolled WebSocket framing (browsers have no raw TCP). Compact binary msgs like events. Canvas client.  
- Build order: 2D world → jukebox → podcast studio → canvas → marketplace.  
- Deps **per-system**: avatar sync hand-rollable; YouTube must use their API; canvas CRDT-ish; podcast = WebRTC-class (do not hand-roll soon); marketplace payments = processor.  
- **Policy:** ephemeral/observe anonymous (move, watch, listen, visit); durable/contested verified (own space, claim scarce slot, permanent canvas mark). Per-**action** check; `Session.identity` optional. Marketplace likely browse-open / list-or-trade-gated.  
- Waiting on Brandon to say **build the one-room slice**.

### Auth / sessions (platform only; design closed 2026-09-09)

Every social slice assumes `AccountId`. Not in `liquidgov-core`. Platform account ≠ `CitizenId`.

Settled:

- If passwords exist: argon2 or bcrypt, never plaintext, never a fast hash alone.
- Session in `HttpOnly` / `Secure` / `SameSite` cookie, never JS-readable storage.
- Rate-limit login. SameSite implies CSRF must be designed, not optional.
- Multi-device sessions: presence is OR across `active_sessions` (any connected socket → Online for friends; non-friends always Offline). One-connection-per-account was a bad implicit assumption.

**Credential (closed):** daily login is **password + breached-password check** as the required baseline; passkeys layered later. Phone is signup **binding** and step-up/recovery, not the daily authenticator. SMS OTP is not primary.

Review note on settled timing sentence: “fail before password compare so timing cannot leak existence” is **backwards**. Early-out on unknown account *is* the existence leak. Dummy-hash the miss path so compare cost is uniform; existence protection is uniform response + rate limit, not a shorter path.

### Profiles + friends + theme gallery (platform only; design closed 2026-09-09)

Not in `liquidgov-core`. `PlatformAccount` ≠ kernel `CitizenId` / `IdentityId`; `verified_link` optional. Civic badges / verified marks render in **platform chrome**, not inside author CSS.

**Accounts / graph**

- Profile create requires phone verification. Age is **not** established by phone.
- Friends: mutual request-accept (`FriendRequest` / `FriendEdge`). Not follow.
- Inner circle capped at 12; outer circle = friends minus inner, **derived**, not stored. Cache the derived set on FriendEdge / inner_circle writes.
- Visibility per-user. Under-13: default friends/family-only; public is a later-granted privilege. No age mechanism pinned — do not encode an under-13 lock you cannot evaluate.
- Friend states include reject / ignore / **block** / unfriend. Block drops the edge, clears inner-circle membership, hides the profile from the blocker’s search.

**Capability boundary (load-bearing)**

- Full visual restyling of a **fixed template** via CSS. No raw HTML, no script, no new structural sections.
- Named failure mode this makes structurally impossible: 2005 Samy worm (MySpace freeform About-Me HTML). Filter-for-HTML is not the plan; omit the field.

**Author CSS isolation**

- Sandboxed **iframe** for profile CSS (separate origin / cookie jar). Shadow DOM is reserved for chrome/badges only.
- In-profile interactivity (jukebox, etc.) is **postMessage** across that iframe. Do not put session-cookie forms or vote widgets on the same route as author CSS.
- Parser: constructed `CSSStyleSheet` → walk `cssRules` (browser CSSOM). No hand-rolled CSS parser.
- Deny (best-effort strip per violation, reasons back to editor): `@import`; any `url()` / `filter:url` / `clip-path:url` / `cursor:url` / `mask-image` / `border-image-source` / `list-style-image` / `offset-path` not already in the platform upload pipeline; `@font-face` / `@namespace` / `@layer` / `@scope`; `expression()` / `-moz-binding` / `behavior`; `:has()` and sibling combinators aimed at platform-injected nodes inside the profile tree.
- Author CSS is attached **only** on the standalone profile route. Lists / search / nameplates / messenger headers use a **platform theme** and a **server-rendered preview artifact** (no author stylesheet download, no embeds). Upgrade to full CSS only on navigation to the profile route — never on scroll/hover/`content-visibility` reveal.
- Previews pair `content-visibility: auto` with `contain-intrinsic-size`. Visibility rules are server-enforced; CSS-hidden is not access control.

**Compute budget**

- Static admission: `property-class × area × (infinite multiplier only if non-composite)`. Cap stylesheet size and rule count. Weights and threshold are **placeholders** until three fixtures run on a low-end machine.
- Runtime rAF-jitter circuit breaker (pause animations, fall back to platform theme) is **post-v1**, not shipped with the static budget.

**Community Assets Gallery (closed)**

- `publish_asset` uses the **same** sanitizer + budget as direct profile CSS. No weaker path for shared themes.
- Install is a **snapshot**: CSS + referenced media recorded into the installer namespace. Not a live link to the author’s files.
- Storage: content-addressed blobs (`blob_id = sha256(bytes)`), **refcount** on install/uninstall. Snapshot semantics without per-installer byte duplication. Author “replacing” a file is a new blob id.
- **Serve all Blobs (gallery + messenger) from a cookieless origin** — amendment to the 2026-09-09 close; attachment + nosniff on the app origin is not enough. Same host-split principle as the profile-CSS iframe.
- Gallery listing visibility mirrors profile visibility (under-13 authors may publish; listing is friends/family-only at the same enforcement point).
- Account deletion: installed copies stay (orphaned, harmless). Theme id / slug goes to a **security-holding stub** so the name cannot be reoccupied.

**Moderation (four triggers, not two)**

| Trigger | Effect |
|---|---|
| `object_takedown(blob_id)` | Walk every manifest/install that lists the blob; break the reference; decrement; delete bytes at refcount 0 unless `legal_hold`. Theme keeps running with a broken ref. Notify alongside. |
| `theme_delist(theme_id)` | `listed = false` only. Does **not** walk installs. New installs stop; frozen copies stay. |
| `exploit_or_legal_reach_in(origin, css_hash)` | Walk **both** `origin_theme_id` (honest install) **and** `css_hash` of sanitized CSS (copy-paste republish). **Disable CSS on those installs immediately**, fall back to platform theme, notify after. AssetLevel may notify-alongside; this path cannot wait. |
| `account_suspend(author)` | Delist everything they published. Installs elsewhere untouched unless a legal/exploit flag is also set. |

Severity tiers: AssetLevel (remove + notify) / AccountLevel (suspend + delist published) / LegalEscalation (compliance process, not moderation code) / ExploitTheme (fourth path above).

Do **not** auto-escalate “N asset flags ⇒ delist theme.” Flags are reports. Escalation is moderator-typed.

Recall keys: `origin_theme_id` is the honest-path index and may be absent. `css_hash` (sha256 of **sanitized** CSS) plus blob hashes are the adversarial-path keys. Exact-hash does not catch a one-property edit; that limit is **named**, not solved. Perceptual / near-duplicate matching is a future escalation if abuse appears — do not build it speculatively.

Installer notice on cascade: generic “an item was removed for a policy violation” (category only). LegalEscalation uses a compliance template, not the AssetLevel string.

**Still open on this layer (do not pretend closed):** age-determination mechanism; measured budget weights; rAF breaker; site-wide jukebox; self-portrait avatar. Messenger is a separate in-progress slice — do not treat it as closed.

### Messenger (platform only; design closed 2026-09-09)

Not in `liquidgov-core`. Reuses the world slice’s single per-session WebSocket + compact binary framing; multiplex by channel tag (`MESSENGER`, `WORLD_SYNC`). No second wire format.

Settled (do not reopen without a new fact):

- `Conversation{id, members[], is_group}`, `Message{id, conversation_id, sender, ts, text, state}`, `PresenceState`
- Groups from the start. Open messaging is the adult default (anyone can *attempt* to message anyone).
- `ConversationMember.state`: Active (friends) or Pending (non-friends). Declined and never-opened are **indistinguishable to the sender**. Same Pending gate on group invites from non-friends.
- **Minor-safety invariant:** a conversation may never contain a minor and a non-friend of that minor at the same time.
- Failed add: **hard refuse**, no roster mutation. Generic copy only (“conflicts with another member’s contact settings”). Same bytes as an adult “don’t add me to groups without asking” preference — ship that adult preference in the same release or the generic error is an age oracle.
- Inviter agency: **fork a new conversation** with the intended add; existing room untouched. Do not label the action as “chat without the people who can’t be here.” No kick-the-minor override. No group toggle that weakens the invariant (toggle may only disclose that contact limits exist).
- Anti-enumeration: do not implement any flow whose observable effect differs only when a minor is involved. Causal sequence (prompt → confirm → roster diff, or unfriend → bounce) is the leak, not the warning text.
- **Unfriend/block co-membership (corrected; previous minor-only removal was a pin error):** breaking a friend/block edge **always** removes the **initiator** from every shared conversation with the other party, for adults and minors alike. Uniform rule — not an opt-in adult preference sitting next to a minor-only bounce. Optional prefs do not close the oracle if defaults still differ. Minor is not evicted as collateral of someone else’s unfriend (initiator leaves).
- No override of the invariant. Age *engineering* closed; counsel still owns numbers.

**1:1 (closed — option B):** minors are friends-only for DMs too; no stranger request queue. Open messaging is adults. Declined ≡ never-opened unchanged.

File-transfer (upload validation only; transport/minor-invariant untouched): **design closed 2026-09-09** except exact per-type MB figures (product call).

- Default-deny allow-list. Content + structural sniff, never extension. SVG off v1 (rasterize-to-PNG would be a later feature). Archives off v1.
- Types: png/jpeg/gif/webp, mp3/wav, mp4/webm, pdf, txt/csv, macro-free OOXML. No HTML/XML/SVG/RTF/legacy OLE `.doc`.
- Pipeline: length-check → prefix **and** structural sniff → allow-list + container inspect → normalize (decode/re-encode raster media; PDF/OOXML stored as-is, never inline) → malware scan **stored** bytes → shared `Blob` store → cookieless origin, `Content-Disposition: attachment`, `X-Content-Type-Options: nosniff`, `charset=utf-8` on text.
- OOXML: do **not** key off the filename `vbaProject.bin` alone (the spec allows any part name; relationships declare it). Reject if Content_Types or `*.rels` include VBA / `oleObject` / `package` / ActiveX, or if parts exist under `embeddings/`, or if any zip member is itself an OLE compound file. Filename list is a hint, not the check.
- Protocol hard cap **100 MB** sizes resumable upload. Per-type caps are a product pin; proposed working set (not competitor-sourced as fact): image 10 / gif 8 / audio 15 / video 50 / documents 25. Competitor “WhatsApp 100MB / Messenger 100MB” figures were **not** treated as verified.
- Messenger blobs are not profile-pipeline `url()` targets unless re-uploaded through profile ingest.
- Gallery media uses the same cookieless serve path (closed-layer amendment).

### Jukebox (platform only; design closed 2026-09-09)

Not in `liquidgov-core`. YouTube (or equivalent) playback; embeds talk to a player via postMessage allow-list, not an open RPC. Unmuted autoplay requires a user gesture (browser-enforced).

**Navigation fork (blocks “usable on any page”):** a full document load kills the JS context that owns audio. Service workers do not preserve playback. World canvas is already a long-lived session — in-world jukebox does not force this fork.

- A — MPA. Each page widget is its own instance. Navigate away → stop. No new site architecture.
- B — client-routed shell while playing (or always). Site-wide seamless playback. This is a **site navigation** change, not a jukebox detail.
- **Navigation (closed — option A):** website is MPA; each page widget is its own instance; navigate away → stop. Persist only inside the world session. No site-wide SPA for the jukebox. If site-wide persistence is later required: dedicated `/listen` or PiP, not a silent SPA conversion.

**Queue authorship (open product call):** owner-curated vs visitors can enqueue. Review lean: profile queues are **owner-only**; visitors listen. Visitor enqueue is a content-gating problem (NSFW / political bait on someone else’s page, worse on a minor profile). `REQUEST_QUEUE_UPDATE` accepted only from the owner’s embed (authenticated `source_id` + session), not from any iframe that can `postMessage`.

**Contract draft (not closed):** `REQUEST_PLAY` / `REQUEST_QUEUE_UPDATE` / `SOURCE_UNMOUNTING` → player; `PLAYBACK_STATE` → embed. Silent drop on bad `origin` or unknown type. Must also pin: `targetOrigin`; sandbox-iframe origin (opaque `null` if `sandbox` without `allow-same-origin` — then `event.origin != kExpectedOrigin` drops every real message); `SOURCE_UNMOUNTING` stops playback in A, does not in B; allow-list pause/seek only if the player is owner-controlled.

### Settings page (platform only; design closed 2026-09-09)

Aggregator of prefs that already live on the right objects, plus a small account-level bag. Five sections — do not grow a junk drawer:

| Section | Here | Not here |
|---|---|---|
| Security | credentials, change phone (old factor + delay), sessions, revoke-one, revoke-all-except-current | argon2 params, WebSocket guts |
| Privacy | group-invite confirm, friends-only inbound DMs, search/findability, presence hide if any, block list (one-way) | inner circle, profile CSS, per-thread mute |
| Identity | `verified_link` pointer: view / unlink-with-confirm | kernel writes, points, seats, parcels, `GrantCitizenship` |
| Data | platform export, delete account | kernel ledger, `ResignCitizen` |
| Notifications | prefs UI (always-on types not togglable) | APNs/FCM vendor guts |

- `blocks_add` and `blocks_dm` are **separate** functions. Same generic external copy. Minor friends-only is policy, not a user toggle. No self-reported `is_minor` control.
- Unlink confirm copy must state the product cost (combined-verify voting access drops until re-verified). Unlink ≠ resign citizenship.
- Delete account: phone quarantine **90 days** (see Account lifecycle). Gallery orphans + slug holding stub already pinned.
- Search/findability is its **own axis**, not implied by `Profile.visibility`.

### People search (platform only; design closed 2026-09-09)

How you find someone to friend. Gallery browse is a later reuse, not this slice.

- Logged-in `requester` only. Results are `SearchResult{account, username, avatar_blob_id}` under **list preview** rules (static image, no author CSS, no embeds).
- `is_findable`: friends always; minors never (except that friend path); else discoverability setting. No minor override.
- Exact-match miss is **identical** empty whether missing / opted out / protected minor.
- Typeahead is a different surface (prefix harvest). Same minor hard-exclude. Do not run typeahead under a short prefix (pin a minimum length).
- Rank: exactness + mutual-friend count. **Not** presence (friends-only signal; using it here leaks it).
- Rate-limit with the **auth** limiter infrastructure, not a second invention. Typeahead needs a tighter budget than login (per-account + per-IP + min prefix).
- Discoverability is **one** setting with three states, not two bools: `FriendsOnly` / `ExactMatch` / `Typeahead` (each level includes the previous). Two checkboxes is a junk-drawer split of one axis.

### Age assurance (platform only; engineering closed 2026-09-09 — counsel still owns threshold, method, jurisdiction)

Engineering shape pinned so theater cannot unlock adult/civic surfaces. Platform `is_minor` ≠ kernel standing. Self-declared DOB alone is not assurance.

```
enum class AgeBand { Unknown, Under13, RestrictedTeen, Adult };
struct AgeAssurance {
    AccountId account;
    AgeAssuranceMethod method;
    AgeBand band;
    optional<AgeRange> estimated_range;
    string policy_version;
    uint16_t threshold_years;
    optional<float> confidence;
    Timestamp determined_at;
    Timestamp expires_at;
    bool inconclusive;
};
```

- No row → fail-safe restricted (`band != Adult`). Product `is_minor` = not Adult. Under-13 is a separate band from RestrictedTeen.
- Self-declare may assert protected for free. Requesting Adult queues a real method; stay restricted until Adult **with margin**. Self-declare must not unlock public search, open DMs, public profile, or `verified_link`.
- Facial estimate: inconclusive or straddling range ≠ Adult. Clear threshold + margin. Error worst at 13–18.
- `expires_at`: UX re-prompt + defense-in-depth, not a safety patch. Staying restricted forever is not harm.
- On-device only with liveness, replay resistance, vendor-signed session attestation. Bare POST of the bit is a client-supplied `is_minor`.
- Discard raw selfie/ID; no leftovers in S3/CDN/vendor debug. Server/vendor processing is still collection. No templates.
- Store `policy_version`, `threshold_years`, method provenance (vendor, model, on-device vs server).
- Escalation: signup, first adult-gated act, jurisdiction change, report, range expiry.
- `verified_link` and combined-verify voting: never on SelfDeclared or bare facial estimate — DocumentVerified or counsel-named equivalent. Genesis does not license a sloppy bit.
- Do not ship lean-and-tune. Not legal advice.

### Notifications (platform only; design closed 2026-09-09)

Reuses multiplexed WebSocket (`NOTIFICATIONS`). APNs/FCM named if there is a mobile client.

```
enum class NotificationType {
    FriendRequestReceived, FriendRequestAccepted,  // never Declined
    GroupInvitePending, GroupMembershipChanged,
    ModerationNotice, NewDeviceLogin, AgeReverificationNeeded,
    NewMessage,
};
```

Togglable allow-list only: FriendRequestReceived/Accepted, GroupInvitePending, NewMessage. Anything not on the list is always-on. New types default always-on.

- No `FriendRequestDeclined` — would break declined ≡ never-opened.
- Membership copy: “You’re no longer in [Group].” Same type as voluntary leave (type field itself must not be a minor-only enum).
- Push preview: adult default generic; adult opt-in body; Under13/RestrictedTeen generic, no toggle. In-app toast may be full detail.
- Rate-limit at request/invite **create**, not at notify fanout.

### Account lifecycle — export + deletion (platform only; design closed 2026-09-09)

One graph walk, two outputs.

**Export includes:** username, phone, authored profile source (CSS/blurb as written, not sanitizer output), this-side friend list, owned blobs, published themes, settings, messages this account **sent**.  
**Export excludes:** `password_hash`, internal moderation files.

Messages default: **sender-only bodies** + envelope (conversation id, timestamps, participant names then visible). Not other people’s text. Full-thread export is a later option, not v1 — joint personal data.

**Deletion:**
- Blobs: existing refcount. Exclusive freed; shared theme/in-chat file decrements; orphan stays if refcount > 0.
- Messages: tombstone sender → `[deleted account]` for remaining members. Do not purge the thread. `under_review` / legal_hold survives (delete is not evidence shred).
- Sessions: revoke immediately.
- Phone quarantine **90 days** before re-bind. Stops delete-to-shed-suspension. FCC 47 CFR §52.15(f)(1)(ii): residential NANP aging **45–90 days** (min/max). 90 covers that whole window; a new assignee can exist from day 45 if the carrier uses the floor. VoIP may be faster; platform hold is still 90.

```
struct Blob { string blob_id; int refcount; bool legal_hold; };

struct ThemeManifest {
    ThemeId theme_id; AccountId author;
    string css_hash;        // sha256 of SANITIZED css
    string css_source;
    vector<string> blob_ids;
    bool listed;
};

struct Install {
    InstallId id; AccountId owner;
    ThemeId origin_theme_id;   // honest-path pointer, may be absent
    string css_hash;           // adversarial-path recall key
    vector<string> blob_ids;
};

void object_takedown(string blob_id);
void theme_delist(ThemeId id);
void exploit_or_legal_reach_in(ThemeId origin, string css_hash);
void account_suspend(AccountId author);
```

---

## 8. How to start the next conversation

Suggested first line:

> Read `artifacts/liquidgov_context_handoff.md` and `artifacts/liquidgov-core`. Do not re-pin settled items. Do not encode CivicActs without an explicit write. Variant indices append-only.

Safe next threads (pick one):

1. **Finish the website-scope conversation** — what is in “and more,” what the landing page must include. Unblocked now; independent of seam 1 and of the world-slice go/no-go.  
2. Seam 1 live-aggregates vs timing (reasoning; may not close)  
3. World slice implementation **outside** the kernel, if Brandon said yes  
4. Platform feature set design-closed except counsel-owned age numbers and product file MB caps. Self-portrait engineering closed (normalize+scan+blob; caps 2MiB / 1024px). Patched sources in artifacts/liquidgov-platform/ — not in liquidgov-core. Kernel encode rules unchanged.  
5. Sit — nothing else is an honest kernel encode without seating or a genesis vote  

Unsafe / already refused:

- SignGuarantee or locality on AppointOffice  
- DrawPanel / commit-reveal in Polity  
- Crypto in liquidgov-core  
- Inventing §13 numbers  
- Scaffolding CaseId or DeclareTicket  
- Fourth branch for “sysadmin”  
- Merging community and locality  

---

## 9. File map

```
artifacts/liquidgov-core/
  CMakeLists.txt
  include/liquidgov/{ids,events,hash,law,state,store}.hpp
  src/{events,hash,law,state,store}.cpp
  tests/{smoke,persist,delegation,replica,identity,land,judgment}.cpp
```

`BindContract` / `NoteAlignment` remain in the variant **only** so later indices do not shift. They are dead weight: wrong mechanic vs §5, **not** “being replaced in place,” never extend, never migrate, **do not delete**. `ClaimSeat` / `AppointOffice` are interim seating; retire when standing tally exists — same index rule, do not delete until a protocol epoch exists.

Website HTML/CSS listed in §7 is a separate workstream; it is not under `liquidgov-core/`.

---

## 10. Operator notes (cold-start)

**Build / test** (from `liquidgov-core/`):

```
cmake -S . -B /tmp/lg-build && cmake --build /tmp/lg-build
/tmp/lg-build/liquidgov_smoke
/tmp/lg-build/liquidgov_persist
/tmp/lg-build/liquidgov_delegation
/tmp/lg-build/liquidgov_replica
/tmp/lg-build/liquidgov_identity
/tmp/lg-build/liquidgov_land
/tmp/lg-build/liquidgov_judgment
```

Smoke still prints `ledger events=14` — that fixture never added land/business/judgment acts. A changed smoke tip on **default** RecordParcel means encode broke compact layout.

**Timestamps:** accrual uses unix seconds. Tests that need non-zero points use `n * kSecondsPerYear`. Do not use wall-clock `time()`.

**ThreeLaws::examine** on FileMotion text is the **§6 pre-enactment compliance gate** (crude keywords). It is not §3 sentencing and not Officials’ Mirror.

**Officials’ Mirror** is Brandon’s real decision; Judgment has no field for it yet.

**Donation-trust:** “many small donors outweigh few large” is kept as a goal. No formula. Proxy-account gaming is named unsolved — not a vote item until there is a mechanism to vote on.

**This repo is not git.** No history besides the event log format.

**Two parallel threads** produced this state (this kernel chat + a Claude thread on AVND text, landing pages, and platform). If they disagree on a pin, prefer: (1) code + tests, (2) this file’s “already reopened / paused audit” list, (3) Brandon in the current chat. Do not treat a prior Grok or Claude prose pin as stronger than the code.

**What this file is not:** a substitute for the AVND white paper; a complete dump of every exchange; a license to encode.
