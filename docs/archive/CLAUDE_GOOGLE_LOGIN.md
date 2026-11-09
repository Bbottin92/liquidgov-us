# Paste this into Claude Code. One job. Do it now.

Google sign-in on https://liquidgov.us is broken because the old OAuth client was deleted (`Error 401: deleted_client`). Brandon created a new Web client. Grok cannot SSH (`publickey` denied, no `liquidgov_deploy_key` in that sandbox). You have the key. You do this.

## Do only this

On the droplet (`root@143.198.159.58`, key `~/.ssh/liquidgov_deploy_key`):

1. Read `/opt/liquidgov/.env.local`. Do not print the rest of the file.
2. Set exactly one variable, replacing the old line if present, appending if missing:

```
LIQUIDGOV_GOOGLE_CLIENT_ID=738917322917-p9hvl2s82l8esj70bc9s58dbv4viraqv.apps.googleusercontent.com
```

3. `systemctl restart liquidgov`
4. Confirm the unit is active and did not crash-loop.
5. Confirm the live login page now contains that client id and not `508630567243-f0s5gh21csd5evp1ls48j289pg14r0eb`.

```
curl -fsS https://liquidgov.us/login | grep -o 'data-client_id="[^"]*"'
```

## Do not

- Do not rsync or deploy the repo. `.env.local` is not in git and a sync must not touch it.
- Do not rebuild. The process reads this variable at start. A restart is enough.
- Do not restart `commons-daemon`. Do not edit `http_server.cpp`.
- Do not put a client secret anywhere. This flow does not use one.
- Do not print other secrets from `.env.local`.

## Report

Active or not, the `data-client_id` line from the live page, and `NRestarts`. Stop.
