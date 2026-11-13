#!/bin/bash
# TEMPLATE for prebuilt deploys (deploy/PLAN-prebuilt.md): care-remote.sh with the build phase installing the
# program GitHub built (checksum, server glibc, --version tree) instead of compiling it here.
# Remote half of the mAInstream deploy (2026-11-07; made from iphone-remote.sh). The local runner passes one phase:
# preflight, backup, chown, build, restart. preflight is read-only.
# This does not replace liquidgov_http.pre-city, .pre-flow, .pre-match or .pre-iphone.
set -euo pipefail
phase="${1:-}"
STORE=/opt/liquidgov/liquidgov_accounts.lgp
BIN=/opt/liquidgov/build/liquidgov_http
KEEP=/opt/liquidgov/liquidgov_http.pre-prebuilt
PRE_FLOW=/opt/liquidgov/liquidgov_http.pre-flow
PRE_CITY=/opt/liquidgov/liquidgov_http.pre-city

count_tags() {
  python3 - << 'PY'
CITY = ("ccity","clist","pcity","pwho","ctag","ptag","prply","prpln","prx","mapp","pain","painn")
counts = {t: 0 for t in CITY}
other = 0
lines = 0
with open("/opt/liquidgov/liquidgov_accounts.lgp", "rb") as f:
    for i, line in enumerate(f):
        lines += 1
        if i == 0:
            continue
        tag = line.split(b"\t", 1)[0].rstrip(b"\r\n").decode("ascii", "replace")
        if tag in counts:
            counts[tag] += 1
        else:
            other += 1
print("LINES", lines)
print("OTHER", other)
for t in CITY:
    print("TAG", t, counts[t])
PY
}

case "$phase" in
  preflight)
    echo "ACTIVE=$(systemctl is-active liquidgov)"
    echo "PID=$(systemctl show -p MainPID --value liquidgov)"
    echo "NRESTARTS=$(systemctl show -p NRestarts --value liquidgov)"
    if [ -x "$BIN" ]; then echo BIN_EXEC; else echo BIN_MISSING; exit 1; fi
    if [ -e "$KEEP" ]; then echo PRE_PREBUILT_PRESENT; else echo PRE_PREBUILT_ABSENT; fi
    if [ -e "$PRE_FLOW" ]; then echo PRE_FLOW_PRESENT; else echo PRE_FLOW_ABSENT; fi
    if [ -e "$PRE_CITY" ]; then echo PRE_CITY_PRESENT; else echo PRE_CITY_ABSENT; fi
    stat -c 'STORE size=%s mtime=%y' "$STORE"
    if [ -e "${STORE}.tmp" ]; then echo TMP_PRESENT; else echo TMP_ABSENT; fi
    df -h / | awk 'NR==2 {print "DISK", $4, "free"}'
    echo TAGS
    count_tags
    echo PREFLIGHT_DONE
    ;;
  backup)
    if [ -e "$KEEP" ]; then echo PRE_PREBUILT_ALREADY; exit 1; fi
    if [ ! -e "$PRE_FLOW" ] || [ ! -e "$PRE_CITY" ]; then echo ROLLBACK_CHAIN_MISSING; exit 1; fi
    if [ ! -x "$BIN" ]; then echo BIN_MISSING; exit 1; fi
    stamp=$(date -u +%Y%m%dT%H%M%SZ)
    dest="${STORE}.pre-prebuilt-${stamp}"
    cp -p "$STORE" "$dest"
    s1=$(stat -c %s "$STORE")
    s2=$(stat -c %s "$dest")
    if [ "$s1" != "$s2" ]; then
      cp -p "$STORE" "$dest"
      s1=$(stat -c %s "$STORE")
      s2=$(stat -c %s "$dest")
    fi
    if [ "$s1" != "$s2" ]; then echo STORE_SIZE_MISMATCH; exit 1; fi
    echo "STORE_BACKUP=$dest SIZE=$s2"
    cp -p "$BIN" "$KEEP"
    cmp -s "$BIN" "$KEEP"
    echo BINARY_COPY_OK
    echo PRE_FLOW_LEFT
    echo PRE_CITY_LEFT
    echo BACKUP_DONE
    ;;
  chown)
    chown -R liquidgov:liquidgov /opt/liquidgov
    owner=$(stat -c '%U:%G' /opt/liquidgov/src/http_server.cpp)
    echo "OWNER=$owner"
    [ "$owner" = "liquidgov:liquidgov" ]
    echo CHOWN_DONE
    ;;
  build)
    # prebuilt (deploy/PLAN-prebuilt.md): no compiling here. Check the program the sync just copied, then install it.
    # Any failed check stops before $BIN changes (set -e); the backup above already holds the running program.
    P=/opt/liquidgov/.prebuilt
    ( cd "$P" && sha256sum -c --quiet liquidgov_http.sha256 )
    tree=$(sed -n 's/^tree=//p' "$P/BUILD-INFO")
    need=$(sed -n 's/^glibc_needed=//p' "$P/BUILD-INFO")
    have=$(ldd --version | head -1 | grep -oE '[0-9]+\.[0-9]+$')
    if [ "$(printf '%s\n%s\n' "$need" "$have" | sort -V | head -1)" != "$need" ]; then
      echo "PREBUILT_GLIBC_TOO_OLD have=$have need=$need (use the compile path)"; exit 1
    fi
    chmod 755 "$P/liquidgov_http"
    v=$("$P/liquidgov_http" --version)
    if [ -z "$tree" ] || [ "$v" = "${v% tree $tree}" ]; then echo "PREBUILT_VERSION_MISMATCH '$v' (want tree $tree)"; exit 1; fi
    install -m 0755 "$P/liquidgov_http" "$BIN"
    echo "PREBUILT_INSTALLED $v"
    chown -R liquidgov:liquidgov /opt/liquidgov/build "$KEEP" "$PRE_FLOW" "$PRE_CITY"
    stat -c 'NEWBIN %U:%G %a %s' "$BIN"
    echo BUILD_DONE
    ;;
  restart)
    systemctl restart liquidgov
    sleep 2
    echo "ACTIVE=$(systemctl is-active liquidgov)"
    echo "PID=$(systemctl show -p MainPID --value liquidgov)"
    echo "NRESTARTS=$(systemctl show -p NRestarts --value liquidgov)"
    journalctl -u liquidgov --since "2 minutes ago" --no-pager -o short-iso
    echo TAGS_AFTER
    count_tags
    echo RESTART_DONE
    ;;
  *)
    echo "refuse phase"
    exit 2
    ;;
esac
