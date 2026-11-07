# City proposals: build plan

Written by Claude, 2026-11-06, from Brandon's Switchyard map (Projects > liquidgov.us > Switchyard Integration).
This is a plan only. Nothing here is built or deployed, and nothing goes live without Brandon's go for that slice.

## Purpose

Foster connections between people, starting with people in the same city (Brandon, 2026-11-06).

## Decisions this plan follows (Brandon, 2026-11-06)

- Proposals start at city level: citizen proposals, in any city where people start a community.
- Anyone can start a city's community. There is no minimum. A city can have several communities; all of them belong to that city, so its people still find each other.
- Accounts stay unverified (today's signup) for now. A semi-official Proposals transition, announced later, will require verification. That transition means opting into CitizenId, which is out of scope here.
- Each city's community adds its own departments and councils as tags. Each proposal is tagged with the ones it touches (the first "tentacles of government").
- Discussion only for now: nothing is submitted to any city.
- Roles that link a person to an item: author, commenter, sharer, reactor.
- Other people see only what was made public. Private items show only to their owner.

## Repo rules this plan keeps (CLAUDE_CODE_CONTINUE.md, notes.md pins)

- A platform account is not a CitizenId. No fake verification, no binding votes.
- A platform group is not an AVND locality. A city here is a label on platform groups. It confers no standing, and the copy says so.
- Ordinary HTML from `liquidgov_http`. No React, Vite or Node UI. Persistence stays LGP1 FileStore: new tags only, append-only, never rewrite old ones, never rsync `*.lgp`.
- Proposals keep their "unofficial straw poll, not binding" copy.

## What already exists (reuse, don't rebuild)

| Map idea | Already on the site |
|---|---|
| City community | Groups: `/communities`, `/c/<slug>`, category "Local & Civic" (`Community`, file_store.hpp:355) |
| Flowed / Still | Post audience Anybody / Friends (`PostAudience`) |
| Commenting | Post replies (`add_reply`, `replies_of`) |
| Sharing, credited to the author | Reposts (`repost_of_author`, `repost_of_id`) |
| Citizen proposals | `/proposals` straw polls (`Proposal`, file_store.hpp:393), five ballot kinds |
| Connections | Friends: mutual accept |
| Verified transition | CitizenId in `liquidgov-core` (separate; not touched here) |

Missing: a city on groups and proposals, a city page, per-city department/council tags, comments and reactions on proposals, and "people in my city".

## Slices (each one small, tested, and deployable on its own)

### Slice 1: City on groups, and a city page

- `Community` gets an optional `city` (free text, for example "Fresno, CA, US"). It's stored in a new LGP1 tag (`ccity <slug> <city>`); groups without it keep working.
- Starting a Local & Civic group asks for a city, with typeahead of cities already in use. This reuses the subcategory datalist pattern, so "Fresno, CA" isn't split three ways by spelling.
- City key: case-folded, whitespace-collapsed text. There is no geocoding and no official list.
- New page `/city/<key>`: the city's groups. Copy: "People here started these groups. A city on LiquidGov is a label, not a government."
- Tests: the tag round-trips through save/load, an old store loads unchanged, and the page lists only groups in that city.

### Slice 2: People in the same city (the purpose)

- On `/city/<key>`: "People here" lists members of that city's groups who chose to be listed. They opt in per city; nobody is listed by default. Each entry has the existing Add-friend action (mutual accept).
- This satisfies "people in the same city can find each other, whichever community they joined" without exposing anyone who didn't choose it.
- Tests: an unlisted member never appears, and listing is per city.

### Slice 3: Proposals belong to a city

- `Proposal` gets an optional `city` (new tag `pcity <id> <key>`). The existing `jurisdiction` field is left alone; it belongs to `/officials`.
- The proposal form gets an optional "City" picker (cities from Slice 1) and a "Who could act on it" free-text line.
- `/proposals?city=<key>` filters, and `/city/<key>` shows the city's proposals.
- Copy stays "unofficial straw poll". Add: "Discussion only. Nothing is sent to the city."

### Slice 4: Departments and councils (first tentacles)

- Per-city tag list, added by any member of that city's groups (new tag `ctag <key> <tag>`). Capped at 80 characters and de-duplicated case-folded.
- A proposal picks zero or more of its city's tags (`ptag <id> <tag>`). `/city/<key>` groups proposals by tag.
- Open (later): who can rename or merge duplicate tags.

### Slice 5: Comment and react on proposals

- Proposal comments reuse the post-reply shape (new tags `prply`/`prpln`). They're as public as the proposal: every signed-in member sees proposals today.
- Reactions: one reaction per person per proposal from a small fixed set (new tag `prx <id> <user> <kind>`), changeable and removable. Posts get the same later if wanted.
- Roles: the author, commenters, sharers (a repost of a proposal link) and reactors become the cross links on a person's page later. Not in this slice.

## Order and size

Recommended order: 1, 2, 3, 4, 5. Slices 1 and 2 alone deliver the purpose (people in a city finding each other). Each slice is one `http_server.cpp` change plus FileStore tags and tests, deployed with the existing runbook (backup, `--civic-done` checks, verify, journal).

## Not in this plan

- The CitizenId / verified transition, the Commons (closed), and renaming the UI to the Media map's words (Pool, Flow, Still, Current, Main Stream). Renaming is a copy decision for Brandon; this plan uses the site's current words.
