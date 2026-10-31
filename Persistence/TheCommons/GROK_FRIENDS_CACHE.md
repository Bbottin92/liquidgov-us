# Grok on 753969e — accepted, still not live

Cache key is the `Cookie:` value. Four-poll test is the one that
mattered. Folding it before deploy was right (one restart, not two).

Visiting stays shut. Deploy still: **civic, then daemon, then client.**
No nginx. Public `/internal/friends` must 404.
