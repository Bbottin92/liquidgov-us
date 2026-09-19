# Resume — Claude hit the monthly limit mid self-ghost

Brandon screenshot 2026-09-18 ~17:16 PDT. Same desktop session, spend
limit, auto-resume ~5:31 PM. Origin is still `/latest` live
(`ef41c03` / Grok `9afd331`). **Self-ghost is not on GitHub.**

## What was in flight (local only)

- `commons/index.html` **+15 / −2** — helper that knows both ids
  (`g_…` guest credential **and** `u:<username>`), set from every
  join/rekey response, used at the two occupant-draw filters.
- `start_ghost_env.sh` **+16** — local civic+cookie harness. **Do not
  commit this.** Test scaffolding, not the game.
- Plan to patch `fetch` with `credentials: include` so a page on
  `:8891` can carry a civic cookie to `:8765`. **Stop. Do not ship that
  in this round.**

## The whole fix

Client-only. Civic frozen. Daemon frozen. Nginx frozen.

A bound row is `u:<name>`. The client still skips only `o.id === netId`
(`g_…`), so it draws itself a second time, ~0.5s lag. Skip if the
occupant id is **either** `netId` **or** the bound id echoed on
join/rekey (`"u:" + username` when `bound`).

That is the round. Two call sites. One helper. Join/rekey already echo
`bound` + `username` (`1dfe2ec`). Do not invent a third id. Do not
filter on `display_name` (disambiguation suffixes).

## Do not do these to "test it"

- Global `credentials: include` on every `fetch`. Production `/commons/`
  is same-origin; cookies already ride. A local `:8891` → `:8765`
  credentialed CORS change is a different review (`cors-preflight.md`).
- A second copy of the client "for comparison."
- Signing up throwaway civic accounts on a scratch civic just to see a
  `u:` row. Stub one occupant `{id: "u:"+username}` in the draw path,
  or wait and verify on production after Grok accepts.
- `/mail` PWA. Same session title. Leave it.

## After the helper is in

1. Guest: still one sprite (`g_` skip unchanged).
2. Bound (or a stubbed `u:` row matching the join username): still one
   sprite, no lag copy.
3. A second real occupant still draws.
4. `self-ghost-built.md` + notes.md. **No deploy** until Grok reviews.
5. Deploy later is **client only**.
