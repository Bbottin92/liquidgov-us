#!/usr/bin/env bash
# Syncs commons-daemon/ source to the production droplet's /opt/commons-daemon (a sibling path -- NOT inside
# /opt/liquidgov, never the LGP1 store; see sibling.md). Does not build, chown or restart: build-on-droplet.sh
# and deploy-daemon.sh do that. Run drift-check.sh first (deploy-daemon.sh does it for you).
#
# Why a script and never a hand-typed rsync: on 2026-09-12 a hand-typed civic rsync overwrote live account data
# with a stale local copy that happened to sit in the source tree. The exclusions live in lib.sh
# (DAEMON_EXCLUDES: build, .git, tests, every sqlite file, backups/) and are shared with drift-check.sh so the
# two can never disagree.
#
# commons-daemon/schema.sql is a git symlink to ../Persistence/TheCommons/schema.sql. A plain rsync would copy
# the link, which dangles on the droplet; -L sends the real file (GROK_DAEMON_REVIEW.md's deploy gate).
#
# Refuses a working tree that differs from HEAD (a sync ships the tree, not a commit) unless --allow-dirty.
# After a real sync it writes /opt/commons-daemon/.deployed-rev (the revision now on the droplet), which is what
# drift-check.sh compares against next time.
#
# Usage: sync-to-droplet.sh [-n|--dry-run] [--allow-dirty]
#   Host/paths come from lib.sh (DEPLOY_HOST etc.); the old positional [user@]host[:path] argument is gone.
set -euo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

dry=""
for a in "$@"; do
  case "$a" in
    -n|--dry-run) dry="-n";;
    --allow-dirty) ALLOW_DIRTY=1;;
    -h|--help) sed -n '2,20p' "$0"; exit 0;;
    *) die "unknown argument: $a";;
  esac
done

sync_tree daemon ${dry:+"$dry"}
[ -n "$dry" ] || say "Synced. Still to do on the droplet: build-on-droplet.sh, then restart (deploy-daemon.sh does all of it)."
