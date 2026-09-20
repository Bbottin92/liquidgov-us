# certbot fix — built and verified live (2026-09-20)

**Status:** fixed on the droplet; no site services restarted, nginx config untouched, no real issuance.

## What was wrong

`certbot` crashed on import (`certbot --version`, `certbot renew`, and the timer-run `certbot.service`, which
FAILED 2026-09-19 14:41 UTC exit 1):

    AttributeError: module 'lib' has no attribute 'GEN_EMAIL'   (OpenSSL/crypto.py line 846, class X509Extension)

Cause: on 2026-09-16 08:56 UTC `pip install pywebpush` (for `/opt/liquidgov/deploy/webpush.py`) put
`cryptography 50.0.1` + `cffi 2.1.1` (+ pywebpush, py_vapid, http_ece, aiohttp, requests, ...) into
`/usr/local/lib/python3.12/dist-packages`. That directory comes BEFORE `/usr/lib/python3/dist-packages` on
`sys.path`, so apt's `pyOpenSSL 23.2.0` (used by certbot/josepy) imported the pip `cryptography 50`, which no longer
has the OpenSSL bindings it needs (`GEN_EMAIL`, removed after cryptography 41). apt `cryptography 41.0.7` was fine
but shadowed.

Why not the obvious fixes (all tried or ruled out, nothing left installed):
- Downgrade pip cryptography: `pywebpush 2.5.0` needs `cryptography>=47`, `py_vapid 1.9.4` needs `>=46`. Would break web push.
- Upgrade pyOpenSSL (tested in throwaway `--target` dirs, deleted): 24.2.1..25.3.0 still fail with `GEN_EMAIL`;
  26.4.0 removed `X509Req`, which certbot 2.9.0 / josepy 1.14 still use.

## What changed (2 small files; nothing else)

1. `/etc/systemd/system/certbot.service.d/pythonpath.conf` — `Environment=PYTHONPATH=/usr/lib/python3/dist-packages`
   (apt tree first, for certbot.service only).
2. `/usr/local/bin/certbot` — 3-line sh wrapper that sets the same PYTHONPATH and execs `/usr/bin/certbot`, so a
   hand-typed `certbot ...` works too (`/usr/local/bin` precedes `/usr/bin` in PATH; the systemd unit still calls
   `/usr/bin/certbot` and is covered by the drop-in).

No packages installed/removed/upgraded. pywebpush unaffected (still imports cryptography 50.0.1).
`liquidgov`, `commons-daemon`, nginx never restarted; `/etc/nginx/sites-available/liquidgov` not touched (mtime unchanged).

## Proof

- `certbot --version` -> `certbot 2.9.0`
- `certbot renew --dry-run` (LE staging) -> `Simulating renewal of an existing certificate for liquidgov.us and www.liquidgov.us` ... `Congratulations, all simulated renewals succeeded: /etc/letsencrypt/live/liquidgov.us/fullchain.pem (success)`, exit 0. `nginx -t` OK before/after.
- `systemctl reset-failed certbot.service; systemctl start certbot.service` -> exit 0, `Result=success`, `ExecMainStatus=0`.
  (The timer also fired 03:30 UTC after the drop-in and finished successfully.)
- liquidgov, commons-daemon, nginx all `active` throughout. Next timer run 2026-09-20 17:05 UTC.
- Cert expires 2026-12-11; renewal begins ~2026-11-11.

## Rollback

    rm /etc/systemd/system/certbot.service.d/pythonpath.conf && rmdir /etc/systemd/system/certbot.service.d
    rm /usr/local/bin/certbot
    systemctl daemon-reload

(Back to the broken state.) Records/copies: `/var/backups/certbot-fix-20260920/` (dpkg list, pip freeze/show before,
/usr/local dist-packages listing, renewal conf copy, sys.path, the two new files, dry-run output).

## Watch out / follow-up

- Any future `pip install` into `/usr/local` (`--break-system-packages`) can shadow other apt Python libs the same
  way. Cleaner long-term: give `webpush.py` its own venv (e.g. `/opt/liquidgov/venv`) and `pip uninstall` the
  system-wide copies — not done here (touches live push feature; not the minimum).
- If certbot is ever upgraded (apt) to a version that needs a newer pyOpenSSL/cryptography, the wrapper/drop-in
  can simply be removed.
