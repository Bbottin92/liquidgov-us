# Programs in mail: deploy and roll back (2026-10-27, branch wip/program-messaging)

What it does: admins (LIQUIDGOV_OPERATORS) can send programs in mail (zip, 7z, rar, exe, msi, apk, Linux
programs) up to the 100 MB ceiling. On /admin/accounts an admin can allow anyone else, choosing their kinds and
their own size limit. Nobody else can send programs; a plain zip no longer passes as a Word file. Mail attachments
over 1 MB start working at all (nginx had its 1 MB default).

Nothing here is live until Brandon says go for this change (hard rule: the live server is read-only otherwise).

## Deploy (in this order; lessons: sync, chown, build, restart, read the journal)

1. Backup first: run the PC's `server-backups\backup.ps1` (pulls accounts .lgp, blobs, nginx, service files).
2. On the server: `md5sum /opt/liquidgov/src/http_server.cpp` must equal the md5 of this branch's base
   (d3e075d43951379dab8f38076f58bbf3, commit a174377). If it differs, the server changed: stop and pull it down.
3. Copy the .lgp aside: `cp -p /opt/liquidgov/liquidgov_accounts.lgp /opt/liquidgov/liquidgov_accounts.lgp.bak-pre-programs`.
4. Sync the code with `deploy/sync-to-droplet.sh` (never a hand-typed rsync), dry run first.
5. `chown -R liquidgov:liquidgov /opt/liquidgov`, then `cmake --build /opt/liquidgov/build -j1`.
6. nginx: paste `deploy/nginx-program-mail.conf` into the 443 server block, `nginx -t`, `systemctl reload nginx`.
7. systemd: `deploy/liquidgov-program-mail.service.conf` to `/etc/systemd/system/liquidgov.service.d/program-mail.conf`,
   `systemctl daemon-reload`.
8. `systemctl restart liquidgov`, then `journalctl -u liquidgov -n 50`.
9. Check: an admin sends a small zip to a friend; the friend sees the card and downloads it with its name;
   a non-admin's zip is refused with the "needs an admin's permission" line.
10. `ctx log claude deploy liquidgov.us --result ok|fail --ref <commit>`.

## Roll back

The store gains two record types (`prgr` grants, `afn` download names). The old binary refuses unknown record
types at load, so once an admin grants anyone or a program is sent, **rolling back the binary alone would stop
the site from starting.** Roll back by: stop the service, restore the old binary (rebuild the previous commit),
and either restore `liquidgov_accounts.lgp.bak-pre-programs` (loses anything since the deploy) or delete the
`prgr\t` and `afn\t` lines from the current .lgp (keeps everything else). Remove the nginx block and the drop-in,
reload nginx, daemon-reload, start.

## Memory

A 100 MB upload is held about three times while it's checked and stored (the request, the body, the file);
this droplet has ~275 MB free plus 2 GB swap. Typical programs (Jammo is ~30 MB) are fine; a 100 MB one will
swap for a few seconds. Raising the ceiling needs a bigger droplet or a streaming upload path.
