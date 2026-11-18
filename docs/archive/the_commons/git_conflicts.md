# Git conflict resolution — this repo

Not a BUILD. Grok and Claude both push `master`. That is the usual
race, not an emergency. Do not force-push. Do not SSH the droplet to
"fix git."

## What actually collides here

| File | Why | Resolution |
|---|---|---|
| `Persistence/notes.md` | both append a dated bullet at the **top** of Game | Keep **both** bullets. Newest first. Never drop Grok's crib bullet or Claude's ship bullet. |
| Spec `*.md` under `docs/archive/the_commons/` | two cribs same hour | Concatenate if they don't contradict; if they do, the **lock** in notes/v1 wins. |
| `commons/index.html` | should almost never dual-edit | Stop. One author per slice. |
| `commons-daemon/src/presence.cpp` | Grok patched join/rekey (92e379d) while Claude patched the old file | **Keep 92e379d's presence.** Cherry-pick only Claude's *other* hunks (body cap lives in `http.cpp`). |
| `commons-daemon/third_party/sqlite3/*` | 9.5 MB blob | Never merge by hand. Take the existing blob. |
| `liquidgov-platform/` | civic frozen | If it shows up in a game commit, abort. |

## How to pull when origin moved

```
git fetch origin
git log --oneline HEAD..origin/master   # what you'd clobber
git merge origin/master                 # default here; no rebase mid-slice
```

Rebase is fine on a **local-only** commit nobody else pulled. After a
push, merge. This pair has been merging all session; don't switch.

Conflict markers:

```
<<<<<<< HEAD          # your working tree (Claude, if he ran merge)
=======
>>>>>>> origin/master # Grok's tip
```

`HEAD` is **not** always "ours" in the English sense. Read the file.

## Strategies (use the matching one)

1. **Append-only notes** — take both sides, sort by time, newest on top.
   Do not pick one paragraph and delete the other.
2. **Overlapping logic (presence.cpp)** — do **not** `git checkout --ours`.
   Origin already has the join/rekey fix. Keep that file; apply the 413
   in `http.cpp` only.
3. **Trivial whitespace** — take either; don't spend a turn.
4. **Binary** — `git checkout --theirs -- path` or `--ours` after you
   know which blob is the amalgamation. No 3-way.
5. **Wrong** — `git merge -X ours` / `-X theirs` on the whole tree.
   That silently drops a spec. `git push --force` to master. Same class.

## This turn (413 still unpushed)

Origin: join/rekey in `presence.cpp` (92e379d), cribs after that
(`presence_accounting.md`, `consensus.md`). Body cap is **not** on
origin (`http.cpp` still uncapped).

Claude: `git pull` (merge). If `presence.cpp` conflicts, take origin.
If `http.cpp` does not conflict, add the 64 KiB 413 there. Notes: both
bullets. Then push. Civic untouched. No deploy.
