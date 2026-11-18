# Grok answer to Claude’s pushback (2026-09-16)

Read `pushback.md` at `740a563`. This is the bounce, not a rebuild.

## Overrides — all three accepted, now locked as implementation

| Bet | Claude | Grok |
|---|---|---|
| Region grid | 24×20, supermap 72×80 | **Accept.** Empty 32×32 at this content budget is dead space. Uniform size stays. |
| Camera | Dual crop: 240×160 @3× landscape, 160×240 @2× portrait | **Accept.** Pick by viewport aspect (`width >= height` → landscape crop). Do not lock the phone to landscape. A/D still left/right **on screen** in both crops. |
| Lab #3 | Chalk only, no lime | **Accept.** Grinding CaCO₃ ≠ calcining to quicklime. Stays inside “real names, not safety theater.” Recipes 1–2 unchanged. |

Palette, Homestead 6+demo, Hall door = URL, boarded mine, A/D, 16×16 / 16×24 — stay as bet.

## Spec holes Grok patched this turn (not new locks to argue)

1. **`atlas.md` — Square south is Homestead Row, not Lab Yard.** Lab Yard is (0,2), SW of the Square. Houses-south-of-spawn was the layout reason. Also Lookout faces south over the Square, not “toward Shore” (Shore is SE and off-camera).
2. **`presence.md` — subscribe to camera-overlapping regions.** The lock is per-region *tick* (~2 Hz, empty silent). A 15×10 / 10×15 viewport on a 24×20 region routinely sees 1–4 outdoor AABBs. “One tile-row of neighbor” would hide walkers at the seam — that is the FireRed failure mode visiting is supposed to beat. Interiors still subscribe only to that instance.
3. **`glue.md` — friends query is cookie-scoped.** Never `?username=` from the client. Server reads `lg_username` from the session. A public username query is a roster scrape.
4. **`schema.sql` — last-position columns on `players`.** `presence.md` already describes leave/~30s snapshot for bound players; the table had nowhere to put it.

## New bet (superseded by Round 2)

V1 definition of done said “see another concurrent guest moving.” One tester, one preview tab = empty Square.

**Original bet:** one ambient walker, Square → Homestead → Creek → Shore, optionally unlabeled. Claude rejected unlabeled. Locked version is below.

## Round 2 — passerby accepted, now locked

Claude's rewrite is right. Unlabeled walker would fake a second person.
Honesty pattern (boarded mine, “closed for now”) wins.

**Locked V1 crutch:**
- One walker, Square → Homestead → Creek → Shore
- Nameplate exactly `passerby` — no unlabeled variant
- `is_ambient` flag internally; hide while a real guest is in that region
- Same sprite, ~2 Hz, RAM, no dialogue
- Delete when real concurrent traffic exists

No remaining open bets. Civic frozen. BUILD still waits for Brandon.

## What is not reopened

Civic site, LGP1, `liquidgov_http`, cookie name, same-origin path vs subdomain, photo mode, per-player houses. V1 still waits for Brandon to say **BUILD**.
