# Paste this into Claude Code. Same product. Do not start over.

You are continuing **LiquidGov.US**. Do not invent a new product. Do not merge the governance kernel into the website process. Read the trees before you edit. Do not treat `preview.py` as source of truth. Do not rewrite the site in Node / React / WASM-as-UI. Ordinary HTML from the C++ process.

If the trees are at `/workspace/artifacts/`, that is the Grok Build mount of the same repo. On WSL they live under `~/lg/` (or wherever Brandon opened them). Same layout either way: `liquidgov-core/`, `liquidgov-platform/`.

Also read, in this order, and do not re-pin what they already closed:

1. `LIQUIDGOV_CONTEXT_HANDOFF.md` (kernel + platform design; variant indices append-only)
2. `GROK_BUILD_PROMPT.md` then **this file** (this file is current; GROK_BUILD_PROMPT still describes an older slice)
3. `BUILD.txt`
4. `liquidgov-platform/src/http_server.cpp`
5. `liquidgov-platform/include/liquidgov/platform/{auth,account,file_store,messenger,avatar,file_transfer,social_graph}.hpp`

Start by listing the files you opened and the **one** slice you will implement. Then implement that slice. Stop and report. Do not dump the Python preview junk drawer.

---

## What this is

Voluntary natural-law democracy software. Three laws: do not steal, do not damage, do not deprive of liberty.

- Civic acts, liquid delegation, land, judgment live in C++ kernel `liquidgov-core` (STL-only, event-sourced, no UI, no I/O in Polity).
- The website is `liquidgov-platform`. **PlatformAccount is not CitizenId.** Do not fake a verified_link or combined-verify voting.
- `/` is genesis disclosure, not sovereignty. Website control is temporary. Real governmental authority is a later landslide after a working election.
- Do not claim liquidgov.us is live. Binary + port: `liquidgov_http` on 8080, bind `0.0.0.0`. Reverse proxy: none in this tree.

---

## What is already built (do not rebuild, do not regress)

**Kernel** `liquidgov-core/` — Polity, SHA-256 event chain, LedgerFile, Replica, identity/citizenship, land, business, genesis judgment. Tests: smoke, persist, delegation, replica, identity, land, judgment. Sortition is **not** in the kernel. Do not encode CivicActs unless Brandon explicitly says write. Variant indices append-only. `BindContract` / `NoteAlignment` are dead weight — do not migrate, do not delete.

**Site process** (this is the website):

```
cd liquidgov-platform
g++ -O2 -std=c++17 -I include \
  src/http_server.cpp src/auth.cpp src/password.cpp src/session.cpp src/file_store.cpp \
  -lsodium -pthread -o $HOME/liquidgov_http
LIQUIDGOV_STORE=./liquidgov_accounts.lgp $HOME/liquidgov_http
```

WSL helper: `bash wsl-setup.sh`. Auth is argon2id via libsodium, `create_account` / `login` in `auth.cpp`. Session cookie `lg_session` HttpOnly SameSite=Lax. **Do not invent a second auth system.**

**Persistence** is `FileStore` (LGP1). Not the kernel ledger. Atomic tmp+fsync+rename, mode 0600. Fail-closed on bad magic. Restart keeps users. Phone number is not stored; `ph` is a blake2b fingerprint for 90-day quarantine after delete.

**Live routes** (cookie-scoped, ordinary HTML):

| Route | What |
|---|---|
| `/` | Genesis disclosure |
| `/laws` | Three laws + kernel boundary |
| `/control` | Who holds control (disclosure) |
| `/signup` `/login` `/logout` | Username+password; phone is binding only |
| `/feed` | Friends-only text posts |
| `/me` `/u/<username>` | Blurb profile; public page |
| `/friends` `/search` `/mail` | Mutual friends, exact+typeahead, friends-only 1:1 |
| `/communities` `/c/<slug>` | Platform groups — **not** AVND localities |
| `/mail/group/new` `/mail/g/<id>` | Messenger groups (Slice C). Friend invitees land Active, non-friends Pending until they accept. Minor invariant is a hard refuse, generic copy. Edit/delete/leave/accept are per-conversation. |
| `/notices` | friend_request, friend_accept, message, group_invite. **No FriendRequestDeclined** |
| `/settings` | Five sections: security, privacy, identity, data, notifications |
| `/export` | Sender-only platform export |
| `/favicon.svg` | Three-laws mark |

Settings already has: change password, sessions, revoke, logout-other-devices, findability (FriendsOnly / ExactMatch / Typeahead), group-invite confirm, block list, identity stub (no civic link), export, delete+90-day phone quarantine, notification toggles. Inner circle cap 12 is in the C++ site. Unfriend and block exist.

**Modules on disk:** all of them are linked into `liquidgov_http` now (`avatar.cpp`, `blob.cpp`, `file_transfer.cpp`, `file_type.cpp`, `image_normalize.cpp`, `messenger.cpp` — Slices A, B, C are all live). `preview.py` is a Windows toy.

`FileStore` now also implements `ConversationStore` (Slice C) — new LGP1 tags `gcn`, `gmsn`, `gc`, `gcm`, `gmsg`, `gmh`, documented at the top of `file_store.hpp`. `SiteSocialGraph` (in `http_server.cpp`) adapts `FileStore`'s username friend graph to `messenger.hpp`'s `AccountId`-keyed `SocialGraph`; its `is_minor` returns `false` unconditionally (AgeAssurance still doesn't exist — same unpaid-liability reasoning as before, just now load-bearing for the group minor invariant too, not only 1:1 mail).

`messenger.hpp`/`messenger.cpp` gained two functions the original spec didn't have but the site needs to make `Pending` usable: `accept_invite` (the only Pending→Active path) and `leave_conversation` (covers both "decline a Pending invite" and "leave as Active" — same function, never notifies on decline, per the closed pin below). Both are unit-tested in `tests/messenger_test.cpp`.

Brand: parchment `#efe8d8`, paper `#f7f1e4`, ink `#1a1814`, navy `#1e2d42`, stone `#6b6458`. Keep the editorial paper look. No emoji chrome. No purple/gold.

---

## Closed pins (do not reopen)

- Friends are mutual accept, not follows. Inner circle cap 12; outer is derived.
- Minors: friends-only everywhere including 1:1 DMs (Option B). **No age checkbox. No `is_minor()` settings toggle.** `SocialGraph::is_minor` is unpaid until AgeAssurance exists — do not encode an under-13 lock you cannot evaluate, and do not treat everyone as minor (that would brick the site). Until AgeAssurance, keep 1:1 mail friends-only for everyone and say so.
- Group add: hard refuse, generic copy only (“conflicts with another member’s contact settings”). Fork a new conversation; do not kick the minor. Unfriend/block: initiator leaves shared conversations, age-blind. **Resolved in Slice C:** `group_invite_confirm` (the adult “don’t add me without asking” pref) is deliberately **not** wired into `create_conversation`/`add_member` — it defaults `true` for every account, so gating on it there silently excluded everyone, including friends, which broke the friends-Active default entirely (caught by the smoke test, not a design choice). That pref stays a communities-only mechanism (`invite_community`). Messenger's own Active/Pending + minor-invariant hard-refuse already is the “ask before adding” control for groups — do not re-add the `group_invite_confirm` gate here without fixing this default-excludes-everyone problem first.
- Auth: password + breach-list; passkeys later. Phone is not the login. Dummy-hash on unknown username.
- File transfer (when you wire it): default-deny, magic-byte sniff, **no SVG, no archives v1**, cookieless origin, attachment + nosniff. Shared Blob (sha256, refcount, legal_hold).
- Profile CSS: sandboxed iframe + CSSOM walk + deny-list + compute budget. **Not raw user CSS.**
- Jukebox: page-local (option A). No silent SPA conversion.
- Export: sender-only messages v1. Delete: tombstone `[deleted account]`; phone quarantine 90 days.
- Notifications: never `FriendRequestDeclined`.
- Kernel: no sortition, no DrawPanel, no invented §13 numbers, no live seating, no CitizenId linkage as if elections exist.
- Platform community ≠ AVND locality. Do not merge them.
- `/` stays a disclosure.

---

## Your job this session (one slice)

Slices A, B, and C are all landed now (see below). Pick the next thing from "Do not do this session" only if Brandon explicitly names it in the same message — otherwise stop and ask what's next.

### Slice A — DONE (self-portrait on the C++ site)

Landed: `/me/avatar` multipart upload, `/portrait/<username>` PNG, LGP1 `av` tag,
`FilesystemBlobStore` + `adopt()` after restart, 2 MiB / 1024px, stb decode/re-encode,
no SVG, nosniff, no author CSS. Binary now links blob.cpp avatar.cpp image_normalize.cpp.

### Slice B — DONE (mail attachments)

Wired `file_transfer.cpp` default-deny onto friend-only `/mail`. No SVG, no archives. Same blob store. Stranger DMs still refused.

### Slice C — DONE (messenger groups)

`FileStore` implements `ConversationStore` directly (new LGP1 tags `gc`/`gcm`/`gmsg`/`gmh`/`gcn`/`gmsn`). `SiteSocialGraph` in `http_server.cpp` adapts it to `messenger.hpp`'s `SocialGraph`, `is_minor` always `false` (unpaid until AgeAssurance — see closed pins). Routes: `/mail` lists groups + a "start a group" form; `/mail/group/new` creates one (friends land Active, strangers Pending, unknown/blocked usernames silently dropped); `/mail/g/<id>` is the thread (roster, compose, invite, leave/accept/decline, per-message edit/delete, all gated on being a member — non-members get a plain 404, no existence leak); `/mail/g/<id>/message/<msg_id>/{edit,delete}` enforce sender-only via `messenger.cpp` itself. `unfriend()` in `file_store.cpp` now calls `on_relationship_ended` so the initiator leaves shared groups (block() calls unfriend, so it gets the same behavior for free). `messenger.hpp` gained `accept_invite`/`leave_conversation` — see the "Modules on disk" note above. Group `group_invite_confirm` gating was tried and reverted — see the closed-pins note on that.

### Do not do this session

- 2D world / WebSocket room (waiting on an explicit “build the one-room slice”).
- Profile CSS iframe (plan exists; easy to ship wrong; not this slice unless Brandon names it).
- Jukebox, passkeys, phone-change-with-delay, presence, combined-verify voting.
- Any CivicAct, seating, CaseId, SignGuarantee, locality-on-office, sortition.
- React/Vite/Node UI. The App Builder React scaffold in a Grok sandbox is **not** the product.
- A second account store.

---

## How to work

1. List files opened + the slice letter.
2. Extend LGP1 only with **new tags**. Do not change the 7-field `a` record. Old files must still load.
3. Keep genesis copy honest. Keep PlatformAccount ≠ CitizenId in any new chrome.
4. Compile the same sources `wsl-setup.sh` lists, plus any new `.cpp` you actually link.
5. Run `tests/file_store_test.cpp` (and avatar/file_transfer tests if you touch those). Do not break persist or fail-closed magic.
6. If you add a host path: name binary, port, bind, proxy. Do not claim liquidgov.us is live.
7. Stop and report: what landed, what you refused, what file format changed.

If a pin and a convenience conflict, keep the pin and say so.
