# Git rebase — this repo

Not a BUILD. Complements `git_conflicts.md`. Default on `master` after a
push is **merge**. Rebase is for a commit **only you have**.

## What rebase does

`git rebase origin/master` takes your unpushed commits, peels them off,
replays each on top of origin. History looks linear. The replayed
commits get **new SHAs**. Anyone who already pulled the old SHAs now has
a fork. That is why you do not rebase `master` after Grok or Claude has
pushed it.

Merge: one extra “Merge branch” commit, both SHAs stay. This pair has
been doing that all session (notes append-order, sprites, daemon). Keep it.

## When rebase is the right tool

| Situation | Do |
|---|---|
| Local commits, `git status` says ahead of origin, **not pushed** | `git fetch && git rebase origin/master` |
| You pushed, origin moved (Grok crib, Claude ship) | `git merge origin/master` |
| Fix a typo in the commit you **just** made, still local | `git commit --amend` (same rule: unpushed) |
| Split/squash a messy local stack before first push | `git rebase -i HEAD~N` then push once |

Unpushed + origin moved: rebase is cleaner than merge. Pushed + origin
moved: merge. The 413 body-cap commit is still unpushed — **that one**
may rebase onto current origin (presence 92e379d + cribs). After it hits
GitHub, stop rebasing it.

## How (local only)

```
git fetch origin
git log --oneline origin/master..HEAD    # your unpushed SHAs; these will change
git rebase origin/master
# if conflict: fix, git add, git rebase --continue
# abort: git rebase --abort
```

Same conflict rules as merge (`git_conflicts.md`): notes keep both;
`presence.cpp` keep origin's join/rekey; 413 lives in `http.cpp`.
`rebase --skip` drops a commit. Don't.

`git pull --rebase` = fetch + rebase. Fine **before** the first push of
a slice. After push, `git pull` (merge) or `git pull --no-rebase`.

## What not to do

- `git rebase -i` of commits already on `origin/master`
- `git push --force` / `--force-with-lease` to `master` so the rebase
  “takes” — Grok's cribs vanish from the tip others pulled
- Rebase the 9.5 MB `sqlite3.c` commit for fun (rewrites a blob both
  sides already have)
- `rebase --onto` across `liquidgov-platform/` — civic frozen, don't
  invent a game commit that touches it

## For this pair

Claude ships slices, Grok ships specs, both on `master`. Linear history
is not worth a force-push. Use rebase to **land a still-local patch on a
moving origin**. Use merge once the patch is public.
