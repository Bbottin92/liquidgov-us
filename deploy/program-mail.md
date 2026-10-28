# Programs in mail: deploy and roll back (2026-10-27, branch wip/program-messaging)

What it does: admins (LIQUIDGOV_OPERATORS) can send programs in mail (zip, 7z, rar, exe, msi, apk, Linux
programs) up to the 100 MB ceiling. On /admin/accounts an admin can allow anyone else, choosing their kinds and
their own size limit. Nobody else can send programs: a zip no longer passes as a Word file, and archives hidden
in PDFs, text or videos, or programs inside Office files, are refused. Mail attachments over 1 MB start working
at all (nginx had its 1 MB default); ordinary mail is capped at 26 MB, program uploads go through /mail/big.

**Programs are not virus-checked.** The site's "scanner" only re-checks the file type; ClamAV doesn't fit in
0.5 GB. The gate is who may send: grant rarely, only the kinds needed. Recipients see who sent it and a warning.

Nothing here is live until Brandon says go for this change (hard rule: the live server is read-only otherwise).

## Deploy (in this order; lessons: sync, chown, build, restart, read the journal)

1. Backup first: run the PC's `server-backups\backup.ps1` (pulls accounts .lgp, blobs, nginx, service files).
2. On the server: `md5sum /opt/liquidgov/src/http_server.cpp` must equal the md5 of this branch's base
   (d3e075d43951379dab8f38076f58bbf3, commit a174377). If it differs, the server changed: stop and pull it down.
3. Copy the .lgp aside: `cp -p /opt/liquidgov/liquidgov_accounts.lgp /opt/liquidgov/liquidgov_accounts.lgp.bak-pre-programs`.
4. Sync the code with `deploy/sync-to-droplet.sh` (never a hand-typed rsync), dry run first.
5. `chown -R liquidgov:liquidgov /opt/liquidgov`, then `cmake --build /opt/liquidgov/build -j1`.
6. nginx: `deploy/nginx-program-mail-zones.conf` to `/etc/nginx/conf.d/liquidgov-upload-zones.conf`; paste
   `deploy/nginx-program-mail.conf` into the 443 server block; `nginx -t`; `systemctl reload nginx`.
7. systemd: `deploy/liquidgov-program-mail.service.conf` to `/etc/systemd/system/liquidgov.service.d/program-mail.conf`,
   `systemctl daemon-reload`.
8. `systemctl restart liquidgov`, then `journalctl -u liquidgov -n 50`.
9. Check through nginx (this part could not be tested on the PC: building a local nginx was blocked):
   - signed out: `curl -s -o /dev/null -w '%{http_code}' -X POST https://liquidgov.us/mail/big -d x` -> 303 to /mail/too-large;
     `curl -s -o /dev/null -w '%{http_code}' https://liquidgov.us/internal/upload-ok` -> 404.
   - Brandon (admin) sends a small zip to a friend from /mail: the friend sees the card and downloads it with
     its name (the response comes from nginx: no `X-Accel-Redirect` header reaches the browser).
   - Brandon sends a ~30 MB file: it arrives; a member without a grant trying the same gets "Not sent".
   - a member sends a 2 MB picture: it arrives (was refused by nginx's 1 MB default before).
10. `ctx log claude deploy liquidgov.us --result ok|fail --ref <commit>`.

## Roll back

The store gains two record types (`prgr` grants, `afn` download names). The old binary refuses unknown record
types at load, so once an admin grants anyone or a program is sent, **rolling back the binary alone would stop
the site from starting.** Roll back by: stop the service, restore the old binary (rebuild the previous commit),
and either restore `liquidgov_accounts.lgp.bak-pre-programs` (loses anything since the deploy) or delete the
`prgr\t` and `afn\t` lines from the current .lgp (keeps everything else; there's no checksum). Remove the nginx
block, the zones file and the drop-in, `nginx -t`, reload nginx, daemon-reload, start.

## Memory

An upload is held about twice, briefly (the request body, then the file taken out of it; the body is released
right after). A 100 MB program needs ~200 MB with ~275 MB free plus 2 GB swap; Jammo (~30 MB) is easy. Downloads
go through nginx from disk (LIQUIDGOV_BLOB_ACCEL); without it, files over 26 MB get a 503 rather than being held
in memory. Raising the 100 MB ceiling needs a bigger droplet or a streaming upload path.

## Known limits

- The mail pop-up panel shows a refused send as a full page (it can't show the message in place).
- If the same bytes are sent twice under two names, the download link of either is named after one of them
  (the cards show each name).
