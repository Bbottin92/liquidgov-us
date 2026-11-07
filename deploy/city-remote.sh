#!/bin/bash
# Remote half of a city deploy. The local runner passes one phase word:
# backup, chown, build, restart, status. Anything else refuses.
# status is read-only.
set -euo pipefail
phase="${1:-}"
STORE=/opt/liquidgov/liquidgov_accounts.lgp
BIN=/opt/liquidgov/build/liquidgov_http
KEEP=/opt/liquidgov/liquidgov_http.pre-city

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
  backup)
    if [ -e "$KEEP" ]; then echo PRE_CITY_ALREADY; exit 1; fi
    if [ ! -x "$BIN" ]; then echo BIN_MISSING; exit 1; fi
    stamp=$(date -u +%Y%m%dT%H%M%SZ)
    dest="${STORE}.pre-city-${stamp}"
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
    mkdir -p /opt/liquidgov-tools
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
  status)
    echo "ACTIVE=$(systemctl is-active liquidgov)"
    echo "PID=$(systemctl show -p MainPID --value liquidgov)"
    echo "NRESTARTS=$(systemctl show -p NRestarts --value liquidgov)"
    echo JOURNAL_ERR
    journalctl -u liquidgov -p err --since "2026-11-07 08:43:00 UTC" --no-pager
    echo TAGS
    count_tags
    echo STATUS_DONE
    ;;
  *)
    echo "refuse phase"
    exit 2
    ;;
esac
