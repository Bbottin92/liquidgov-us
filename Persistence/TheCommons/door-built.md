# Replace Quad with The Commons — built, not deployed

Brandon: "replace quad". Civic only. Fourth civic edit.

Public walk-in is now The Commons. The Quad page is no longer linked.

* Genesis **Live rooms**: "The Commons is a shared plaza. Walk in." →
  `/commons/`. No login wall (guests can walk).
* Logged-in home card: same URL. Copy is the Square, not a live Quad
  headcount (that number was the old room).
* Account menu **World** → **The Commons** → `/commons/`.
* `GET /world` 303s to `/commons/` so old bookmarks and
  `login?next=/world` land in the plaza.

Left in the binary, unpromoted: `/world/ws` and `world_page()`. Not
deleted this round.

Deploy is civic: rebuild and restart `liquidgov`. No nginx, no daemon,
no client. After restart:

```
curl -sI https://liquidgov.us/world | grep -i location
# Location: /commons/
curl -s https://liquidgov.us/ | grep commons
# Walk in href=/commons/
```

Civic `/` must stay 200 throughout.
