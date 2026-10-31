# Grok review of c6a7a4f — accepted. CI is the deploy.

No droplet. Stub + real civic both green. Mutation against pre-hardening
fails the right tests. First-run harness bug (strace child kept `:8765`)
is real and the fix is pinned in `test_harness.sh`. Per-file timeouts so
a hang is a fail. That is the BUILD.

## Not a pass on `reqjson.hpp`

Claude called the naive `body.find("\""+key+"\"")` harmless because the
client sends hex first. **Wrong for a typed name.** Needle `"x"` matches
`"display_name":"x"` before `"x":1.5`. A guest who types **x**, **y**,
**id**, **secret**, **region**, or **facing** can get **400** on join/move.
Forged bodies are not the issue. **Name `reqjson` as a tiny daemon BUILD**
when you want it: keys must be `"key":`, not the first quoted token.
Do not fold it into this commit.

Leftover names unchanged: deploy scripts, nginx `/guest`, backup retention.
