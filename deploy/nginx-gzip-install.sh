#!/bin/bash
# Remote half: compress JS/CSS/JSON/SVG on liquidgov.us (open item "nginx gzip for .js/.css", Brandon OK 2026-11-08).
# Adds ONE new file; nginx.conf already has "gzip on" (HTML only by default) and includes conf.d/*.conf in http {}.
# Validates with nginx -t before a graceful reload (no dropped connections, the app is not restarted). If the test
# fails, the new file is removed and nothing is reloaded. Rollback later: rm the file, nginx -t, systemctl reload nginx.
set -euo pipefail
F=/etc/nginx/conf.d/liquidgov-gzip.conf
if [ -e "$F" ]; then echo "ALREADY_PRESENT $F"; exit 1; fi
cat > "$F" << 'CONF'
# liquidgov.us: compress text assets too (2026-11-08, Claude, Brandon's OK). nginx.conf turns gzip on for text/html.
gzip_types text/css application/javascript text/javascript application/json image/svg+xml application/manifest+json text/plain;
gzip_vary on;
gzip_comp_level 5;
gzip_min_length 512;
CONF
if ! nginx -t 2>&1; then
  rm -f "$F"
  echo "NGINX_TEST_FAILED: removed $F, nothing reloaded"
  exit 1
fi
PID_APP_BEFORE=$(systemctl show -p MainPID --value liquidgov)
systemctl reload nginx
sleep 1
echo "NGINX_ACTIVE=$(systemctl is-active nginx)"
echo "APP_PID_BEFORE=$PID_APP_BEFORE APP_PID_AFTER=$(systemctl show -p MainPID --value liquidgov)"
echo GZIP_DONE
