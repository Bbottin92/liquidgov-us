# GitHub Actions secrets — this CI needs none

Explore, not a BUILD. Do not add repository secrets. Do not put a
droplet SSH key, `lg_session`, civic store path, or sqlite password in
Actions. Deploy stays a spoken word on the box.

## What `ci.md` actually uses

`permissions: contents: read` and the automatic `GITHUB_TOKEN`.
Checkout, cmake, apt nginx, grep. FetchContent hits sqlite.org over
plain HTTPS; the pin is `URL_HASH`, not a token.

A public repo workflow with no `secrets.` references cannot leak what
isn't there. **Keep it that way.**

## What GitHub secrets *are*

Encrypted values, set in the repo (or org / environment), injected only
at run time as `secrets.NAME`. Masked in logs if GitHub recognizes the
exact string. Not a vault: anyone with admin on the repo can read them
out by echoing in a workflow they control. Fork **pull_request** runs
do **not** get them (except `GITHUB_TOKEN`, read-only). That is the
feature that stops a drive-by PR from exfiltrating a deploy key.

`pull_request_target` **does** get secrets and runs the workflow from
the default branch. Do not add it. Do not check out
`github.event.pull_request.head.sha` under it. GitHub is tightening
this through 2026; we don't need the event at all.

OIDC (`id-token: write` → cloud role) is how you avoid stored cloud
keys. Irrelevant here: there is no AWS/Vercel for the civic site, and
the game daemon is a systemd unit on one droplet.

## Fail if "explored" by

| Temptation | Why not |
|---|---|
| `DROPLET_SSH_KEY` / `secrets.SSH_KEY` | Auto-deploy from Actions *is* SSH from a third machine. Grok doesn't SSH; CI shouldn't either. A leaked Actions secret is a leaked root-equivalent on 143.198.159.58. |
| `secrets.LG_SESSION` for a live curl | A session cookie in CI is a real account. Public 404s (`/internal/friends`, `/latest` for nobody) need no cookie. |
| `pull_request_target` so forks get a deploy key | Classic exfil. `ci.md` uses `pull_request`. |
| Org-wide secret "just in case" | This repo is the only consumer. Don't share with Studio / other bots. |
| Printing `secrets.GITHUB_TOKEN` to debug | Don't. `contents: read` is enough and already in the yaml. |

## If a later round *must* hit the droplet

Don't. If Brandon still wants it: a GitHub **environment** named
`droplet` with required reviewers, OIDC or a deploy key that can only
`rsync` `/opt/commons` (not `/opt/liquidgov`), and a `workflow_dispatch`
not `push`. That is a new BUILD, a new review, and it is not this file.

`ci.md`'s yaml stays secret-free.
