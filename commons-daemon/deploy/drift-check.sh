#!/usr/bin/env bash
# Read-only. Is it safe to sync? Two questions, asked with checksum dry-runs (nothing is written):
#
#   1. DRIFT: does the droplet still hold what was last deployed? Compared against the revision named by the
#      droplet's own marker (.deployed-rev / .deployed-client-rev), exported from git. Anything that differs
#      was edited on the box or never deployed; a sync would silently overwrite it. The droplet is the runtime
#      truth (a hand-patch there once forked civic three ways), so this must be EMPTY before any sync.
#   2. PLAN: what would this deploy change? (working tree vs droplet.) You read this list and check it is the
#      change you meant to ship, the way the hardening deploy checked "exactly this round's eight daemon files".
#
# Also refuses a working tree that differs from HEAD, and HEAD not pushed (unless overridden).
#
# Usage: drift-check.sh [--tree daemon|client|both] [--baseline REV] [--allow-dirty] [--allow-unpushed]
#   --baseline REV   required when the droplet has no marker yet (first use). The revision that is live now.
# Exit: 0 no drift / 1 drift or a refused precondition / 2 cannot decide (no baseline)
# shellcheck disable=SC2034  # ALLOW_DIRTY / ALLOW_UNPUSHED are set here and read by need_clean_tree / need_pushed in lib.sh
set -euo pipefail
# shellcheck source=lib.sh
. "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

TREES=(daemon client); BASELINE=""
while [ $# -gt 0 ]; do
  case "$1" in
    --tree) case "${2:-}" in daemon) TREES=(daemon);; client) TREES=(client);; both) TREES=(daemon client);; *) die "--tree daemon|client|both";; esac; shift 2;;
    --baseline) BASELINE="${2:?--baseline needs a revision}"; shift 2;;
    --allow-dirty) ALLOW_DIRTY=1; shift;;
    --allow-unpushed) ALLOW_UNPUSHED=1; shift;;
    -h|--help) sed -n '2,20p' "$0"; exit 0;;
    *) die "unknown argument: $1";;
  esac
done

need_clean_tree
need_pushed

TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
drift=0; undecided=0
say "== drift check against $HOST (read-only)"

for tree in "${TREES[@]}"; do
  say ""; say "---- $tree ($(tree_src "$tree")/ -> $(tree_dest "$tree")/)"
  rev="$BASELINE"
  if [ -z "$rev" ]; then
    rev="$(deployed_rev "$tree")"
    case "$rev" in
      "") say "no marker on the droplet ($(tree_marker "$tree")): cannot tell what is live."
          say "Name the revision that is deployed right now: --baseline <rev>  (after that sync the marker exists)"
          undecided=1; continue;;
      *+dirty) say "the marker says the last deploy was from a dirty tree ($rev): no revision to compare against."
               say "Name one: --baseline <rev>"; undecided=1; continue;;
    esac
  fi
  git -C "$REPO" cat-file -e "$rev^{commit}" 2>/dev/null || die "revision '$rev' is not in this repository"
  say "last deployed revision: $(git -C "$REPO" log -1 --format='%h %s' "$rev")"

  mkdir -p "$TMP/base-$tree"; export_rev "$rev" "$TMP/base-$tree"
  drifted="$(list_changed "$TMP/base-$tree/$(tree_src "$tree")/" "$tree")"
  if [ -n "$drifted" ]; then
    say "DRIFT: the droplet differs from revision $(git -C "$REPO" rev-parse --short "$rev") in:"; sed 's/^/    /' <<<"$drifted"
    say "  (edited on the box, or a deploy that was never marked. A sync would overwrite these. Diff and reconcile first.)"
    drift=1
  else
    say "no drift: the droplet holds exactly what revision $(git -C "$REPO" rev-parse --short "$rev") shipped"
  fi

  plan="$(list_changed "$REPO/$(tree_src "$tree")/" "$tree")"
  if [ -n "$plan" ]; then say "this deploy WILL change:"; sed 's/^/    /' <<<"$plan"
  else say "this deploy changes nothing here (droplet already matches the working tree)"; fi

  extra="$(list_extra "$REPO/$(tree_src "$tree")/" "$tree")"
  [ -n "$extra" ] && { say "on the droplet but not in the repo (a sync leaves them alone):"; sed 's/^/    /' <<<"$extra"; }
done

say ""
if [ "$drift" = 1 ]; then say "== DRIFT FOUND: do not sync"; exit 1; fi
if [ "$undecided" = 1 ]; then say "== UNDECIDED: no baseline"; exit 2; fi
say "== no drift"
