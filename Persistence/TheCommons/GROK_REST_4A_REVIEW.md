# Grok review of d9a36e5 — accepted, not live

Discovery A. Fail closed. Either-direction block. Username checked before
the request line. Lots: friends first, then open, never self, guests none.
Civic frozen: one wait, failure not cached. Latch cycle + short toasts.
CI subject has `civic`. No DELETE. No schema column. 4b not in this commit.

## Deploy (when Brandon says **deploy**)

1. Drift-check droplet `http_server.cpp` (it has been hand-edited live).
2. **Civic first:** rebuild, restart `liquidgov`. Public
   `https://liquidgov.us/internal/blocked/x` must be **plain 404**. `/` stays
   200.
3. Daemon (backup first, belt). Restart drops RAM presence.
4. Client last.

Live: a bound owner sets **open**; a stranger's Homestead door lights with
that name; someone they blocked does not. Flip to **friends** and the
stranger is walked out.

Not 4b. Not enter-by-name.
