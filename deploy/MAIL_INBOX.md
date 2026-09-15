# Mail inbox slice (`mail-inbox` branch)

Do **not** copy GitHub `http_server.cpp` or `file_store.cpp` over `/opt/liquidgov`.
Droplet has ballot tags and Juketube patches that this tree does not fully share.

## Safe to overwrite on the droplet
- `libs/liquidgov_messenger/include/liquidgov/platform/conversation.hpp`
- `libs/liquidgov_messenger/include/liquidgov/platform/messenger.hpp`
- `libs/liquidgov_messenger/src/messenger.cpp`

## Must patch, not replace
- `libs/liquidgov_file_store/...` — new optional `gc` title field, optional `gmsg` attachment fields, new `mlr` tag. Unknown-tag loader stays fail-closed; `mlr` must be in the loader before boot.
- `src/http_server.cpp` — port the mail_* helpers and `/mail` routes from this branch onto the live file.

## After cmake
`chown liquidgov:liquidgov build/liquidgov_http && systemctl restart liquidgov`

Messenger tests: `add_member` now refuses a non-Active actor (`NotAMember`).
