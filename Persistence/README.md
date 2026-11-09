# Persistence (shared, Grok + Claude)

`notes.md` is the idea/decision pad. Not bug classes, not deploy recipes, not “what is live.”

**GitHub has `notes.md` + this README only** as the pad. `context.md` and `state.md`
live in an old Grok sandbox and were never pushed. Do not invent them. Do not have
Claude create fakes. “Have Claude git pull” updates notes/README (and now
`Persistence/TheCommons/` spec files).

**The Commons spec** (sibling game, not the civic binary): `Persistence/TheCommons/`.
Paste `docs/archive/CLAUDE_THE_COMMONS.md` for that job. The old civic continue file is
`docs/archive/CLAUDE_CODE_CONTINUE.md` (both moved from the repo root 2026-11-09).

**Claude — standing rule (every session, non-negotiable):**
1. `git pull` if your WSL clone might be behind.
2. Read `Persistence/notes.md` before any product/architecture work, and before
   implementing anything Grok or Brandon just described.
3. If Brandon decides, rejects, or parks an idea, append a **dated** bullet to
   the matching heading *that turn*. Do not wait for “push it.” Commit notes
   with the work they belong to, or on their own if there is no code yet.
4. You may edit this file. Do not delete entries without Brandon saying so.
   Newest bullets at the top of a heading.
5. Closed pins in `notes.md` and `docs/archive/CLAUDE_CODE_CONTINUE.md` stay closed.

Grok writes here via GitHub. You write here via git in WSL. Same file.
