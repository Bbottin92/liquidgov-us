# shellcheck shell=bash
# shellcheck disable=SC2034  # variables here are used by the scripts that source this file
# Shared config and helpers for the commons deploy scripts. Sourced, never run.
#
# Every knob is an env var, so tests/test_deploy.sh can drive the whole flow against a FAKE droplet
# (a temp dir behind a shim "ssh") and CI never touches the real box. Defaults are production.
#
#   DEPLOY_HOST          root@143.198.159.58
#   LIQUIDGOV_DEPLOY_KEY ~/.ssh/liquidgov_deploy_key      DEPLOY_SSH  (whole ssh command; test seam)
#   DEPLOY_DAEMON_DIR    /opt/commons-daemon      the daemon's source + build (NOT served by nginx)
#   DEPLOY_CLIENT_DIR    /opt/commons             the nginx static alias: client files only, ever
#   DEPLOY_DATA_DIR      /var/lib/commons         commons.sqlite + backups/
#   DEPLOY_SERVICE       commons-daemon           DEPLOY_CIVIC_SERVICE liquidgov   DEPLOY_SERVICE_USER liquidgov
#   DEPLOY_PUBLIC_BASE   https://liquidgov.us     DEPLOY_PREFIX /commons

DEPLOY_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$DEPLOY_DIR/../.." && pwd)"

HOST="${DEPLOY_HOST:-root@143.198.159.58}"
KEY="${LIQUIDGOV_DEPLOY_KEY:-$HOME/.ssh/liquidgov_deploy_key}"
read -r -a SSH_ARR <<< "${DEPLOY_SSH:-ssh -i $KEY -o BatchMode=yes -o ConnectTimeout=10}"
SSH_E="${SSH_ARR[*]}"                                   # the same command as one string, for rsync -e
DAEMON_DIR="${DEPLOY_DAEMON_DIR:-/opt/commons-daemon}"
CLIENT_DIR="${DEPLOY_CLIENT_DIR:-/opt/commons}"
DATA_DIR="${DEPLOY_DATA_DIR:-/var/lib/commons}"
SERVICE="${DEPLOY_SERVICE:-commons-daemon}"
CIVIC_SERVICE="${DEPLOY_CIVIC_SERVICE:-liquidgov}"
SERVICE_USER="${DEPLOY_SERVICE_USER:-liquidgov}"
PUBLIC_BASE="${DEPLOY_PUBLIC_BASE:-https://liquidgov.us}"
PREFIX="${DEPLOY_PREFIX-/commons}"                      # may be set empty (tests talk to the daemon directly)
DB_REMOTE="$DATA_DIR/commons.sqlite"
BACKUP_DIR_REMOTE="$DATA_DIR/backups"

# What is NEVER synced. One list, used by the sync AND the drift check, so they cannot disagree.
# Live data (sqlite anywhere in the tree, the backups dir) is the class of bug that once overwrote civic's
# accounts store: it is excluded twice over, by pattern and because nothing here lives in the synced tree.
DAEMON_EXCLUDES=(--exclude build --exclude .git --exclude tests
                 --exclude '*.sqlite' --exclude '*.sqlite-shm' --exclude '*.sqlite-wal' --exclude 'backups/'
                 --exclude '.deployed-*' --exclude '.deploy-log')
CLIENT_EXCLUDES=(--exclude .git --exclude '.deployed-*')

# The scripts set these from --allow-dirty / --allow-unpushed; need_clean_tree / need_pushed below read them.
# Reset to 0 on every load, so nothing in the operator's environment can loosen the guards.
export ALLOW_DIRTY=0
export ALLOW_UNPUSHED=0

say() { printf '%s\n' "$*"; }
die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
q() { printf '%q' "$1"; }                                # shell-quote for the remote side
remote() { "${SSH_ARR[@]}" "$HOST" "$@"; }

# The paths a sync ships. tests/ is excluded from the sync, so it is not part of "what is deployed".
SYNC_PATHS=(commons-daemon commons Persistence/TheCommons/schema.sql ':(exclude)commons-daemon/tests')

dirty_paths() { git -C "$REPO" status --porcelain --untracked-files=all -- "${SYNC_PATHS[@]}"; }

# A sync ships the WORKING TREE, not a commit. Refuse anything that is not exactly HEAD, or the marker
# ("what is on the droplet") would be a lie.
need_clean_tree() {
  local d; d="$(dirty_paths)"
  [ -z "$d" ] && return 0
  if [ "$ALLOW_DIRTY" = 1 ]; then say "WARNING: deploying a dirty tree (--allow-dirty):"; say "$d"; return 0; fi
  say "The synced paths differ from HEAD (a sync would ship uncommitted work):"; say "$d"
  die "commit (and push) first, or pass --allow-dirty if you really mean it"
}

# Deployed code must be code that git and CI have seen.
need_pushed() {
  git -C "$REPO" remote get-url origin >/dev/null 2>&1 || { say "note: no 'origin' remote; cannot check HEAD is pushed"; return 0; }
  git -C "$REPO" fetch -q origin 2>/dev/null || say "WARNING: could not fetch origin; pushed-check may be stale"
  local up ahead
  up="$(git -C "$REPO" rev-parse --abbrev-ref '@{upstream}' 2>/dev/null || echo origin/master)"
  ahead="$(git -C "$REPO" rev-list --count "$up..HEAD" 2>/dev/null || echo 0)"
  [ "$ahead" = 0 ] && return 0
  [ "$ALLOW_UNPUSHED" = 1 ] && { say "WARNING: HEAD is $ahead commit(s) ahead of $up (--allow-unpushed)"; return 0; }
  die "HEAD is $ahead commit(s) ahead of $up: push first (deployed code must be code git and CI have seen), or --allow-unpushed"
}

# The synced trees exactly as they were at revision $1, unpacked into directory $2.
# (schema.sql is a symlink inside commons-daemon; the archive carries its target so -L can follow it.)
export_rev() { git -C "$REPO" archive "$1" commons-daemon commons Persistence/TheCommons/schema.sql | tar -x -C "$2"; }

# tree_src TREE / tree_dest TREE / tree_marker TREE / tree_excludes TREE (names an array)
tree_src()    { case "$1" in daemon) echo "commons-daemon";; client) echo "commons";; *) die "tree must be daemon or client";; esac; }
tree_dest()   { case "$1" in daemon) echo "$DAEMON_DIR";; client) echo "$CLIENT_DIR";; esac; }
tree_marker() { case "$1" in daemon) echo ".deployed-rev";; client) echo ".deployed-client-rev";; esac; }

# rsync dry-run listings. $1 = local source dir (with trailing /), $2 = tree. Regular files only.
#   changed: files a sync from $1 would change or add on the droplet (checksums, not mtimes)
#   extra:   files that exist on the droplet and not in $1 (a sync never deletes them; they are worth knowing about)
_rsync_dry() { # tree src extra-args...
  local tree="$1" src="$2"; shift 2
  local -a ex
  if [ "$tree" = daemon ]; then ex=("${DAEMON_EXCLUDES[@]}"); else ex=("${CLIENT_EXCLUDES[@]}"); fi
  rsync -rLnci "$@" -e "$SSH_E" "${ex[@]}" "$src" "$HOST:$(tree_dest "$tree")/"
}
list_changed() { _rsync_dry "$2" "$1" | sed -n 's/^[<>]f[^ ]* //p' | sort; }                       # src tree
list_extra()   { _rsync_dry "$2" "$1" --delete | sed -n 's/^\*deleting   //p' | grep -v '/$' | sort || true; }

# The revision the droplet's tree was last synced from, from its marker; "" if there is none.
deployed_rev() { # tree
  # no marker is a normal answer ("" = unknown), not an error: cat failing must not kill a set -e caller
  { remote "cat $(q "$DAEMON_DIR/$(tree_marker "$1")") 2>/dev/null" || true; } | awk 'NR==1{print $1}'
}

mark_deployed() { # tree
  local rev now; rev="$(git -C "$REPO" rev-parse HEAD)"; now="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  [ -n "$(dirty_paths)" ] && rev="$rev+dirty"
  remote "printf '%s %s\n' $(q "$rev") $now > $(q "$DAEMON_DIR/$(tree_marker "$1")") && printf '%s %s %s\n' $now $1 $(q "$rev") >> $(q "$DAEMON_DIR/.deploy-log")"
}

# Actually sync one tree. -n = dry run (allowed on a dirty tree; it changes nothing).
sync_tree() { # tree [-n]
  local tree="$1" dry=()
  [ "${2:-}" = "-n" ] && dry=(-n)
  [ "${#dry[@]}" = 0 ] && need_clean_tree
  local -a ex
  if [ "$tree" = daemon ]; then ex=("${DAEMON_EXCLUDES[@]}"); else ex=("${CLIENT_EXCLUDES[@]}"); fi
  say "Syncing $REPO/$(tree_src "$tree")/ -> $HOST:$(tree_dest "$tree")/ ${dry[*]:+(DRY RUN)}"
  # -L: schema.sql is a symlink to ../Persistence/TheCommons/schema.sql; the droplet needs the real file.
  rsync -az -L "${dry[@]}" -e "$SSH_E" "${ex[@]}" "$REPO/$(tree_src "$tree")/" "$HOST:$(tree_dest "$tree")/"
  if [ "${#dry[@]}" = 0 ]; then mark_deployed "$tree"; say "marked $(tree_marker "$tree") on the droplet"; fi
}
