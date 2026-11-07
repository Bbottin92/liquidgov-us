#!/bin/bash
# Remote half of the Flow deploy. The local runner passes one phase word:
# preflight, backup, chown, build, restart. preflight is read-only.
# This does not touch liquidgov_http.pre-city. That copy is the rollback
# to the binary from before any city pages.
set -euo pipefail
phase="${1:-}"
STORE=/opt/liquidgov/liquidgov_accounts.lgp
BIN=/opt/liquidgov/build/liquidgov_http
KEEP=/opt/liquidgov/liquidgov_http.pre-flow

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
    if [ -e "$KEEP" ]; then echo PRE_FLOW_PRESENT; else echo PRE_FLOW_ABSENT; fi
    if [ -e /opt/liquidgov/liquidgov_http.pre-city ]; then echo PRE_CITY_PRESENT; else echo PRE_CITY_ABSENT; fi
    stat -c 'STORE size=%s mtime=%y' "$STORE"
    if [ -e "${STORE}.tmp" ]; then echo TMP_PRESENT; else echo TMP_ABSENT; fi
    df -h / | awk 'NR==2 {print "DISK", $4, "free"}'
    echo TAGS
    count_tags
    echo PREFLIGHT_DONE
    ;;
  backup)
    if [ -e "$KEEP" ]; then echo PRE_FLOW_ALREADY; exit 1; fi
    if [ ! -x "$BIN" ]; then echo BIN_MISSING; exit 1; fi
    stamp=$(date -u +%Y%m%dT%H%M%SZ)
    dest="${STORE}.pre-flow-${stamp}"
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
    cmake --build /opt/liquidgov/build -j1
    chown -R liquidgov:liquidgov /opt/liquidgov/build "$KEEP"
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
