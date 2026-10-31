# What would make The Commons coherent and fun? (Claude -> Brandon, 2026-09-20)

Brandon: "go through and figure out whats needed to make this game coherent and entertaining."
This is analysis plus **one question**; nothing is built. Answer with letters (e.g. "A, B").

## What I checked

Read `v1.md`, `GROK_BET.md`, `GROK_THE_REST.md`, `inventory.md`, `latest-built.md`, `visiting-built.md`,
the client (`commons/js/*`, `index.html`), the item glyph code, and the droplet's nginx logs (read-only).
Locks respected throughout: no XP, quests, combat, day/night, required soundtrack; visiting is the loop;
world-reachable guest mint is intended; civic is frozen except the named `/internal/*` routes.

## What the game is today

Walk 12 quiet regions; look up 11 real materials (8 lab stations + 3 outdoor spots), never consumed;
in your own bound house, put things on the wall (one slot) and floor (cap 8); friends visit; the wall
slot shows the owner's real civic profile drawing. The Hall is a link. Six interiors are enterable
(demo house, own house, friends' houses, lab, mine, workshop).

## Diagnosis (evidence, not taste)

1. **Nothing is left to do after ~5 minutes.** The whole economy is 11 items from 11 spots, never
   consumed, never combined; a lookup is "+1 and a toast". Nothing changes tomorrow. No reason to return.
2. **The world is empty unless people are in it, and few are.** Nginx logs since 09-17 show guest mints
   from 1-6 distinct IPs a day (24 / 11 / 57 / 10 mints on 09-17..20), and that includes our own
   testing. Presence-only social plus a placeholder `passerby` means a typical visit is alone.
3. **The civic tie is one-way and thin.** "Ballot box" is a link. The notice post in the Square is a
   decoration you cannot interact with. Only the wall-slot drawing and friends-visit are real glue.
4. **The best idea in the game goes nowhere.** The eleven items are all historical ink and pigment
   chemistry: iron-gall ink, lampblack, gum arabic (binder), alum (mordant), copper acetate (verdigris
   green), tannin, chalk, vinegar. The civic profile is a *drawing*. That is a coherent through-line
   (materials -> pigments -> what you draw), and today the items just sit on a shelf. (My inference;
   the docs call the lab "feeds visiting, is not the game".)
5. **No feedback and no guidance.** Zero audio, toasts only, one character look, and a name card that
   drops you in the Square with no hint about Enter or what to do. Item art is thinner than it looks:
   oak gall and oak-gall tannin are pixel-identical; bottle/vinegar and both inks share shapes.

## Options

| | What | Touches | Size | Fixes |
|---|---|---|---|---|
| **A** | **Polish + guidance.** First-visit hint line; the Square notice post becomes a real sign (points at the Hall / what's here); distinct glyphs for tannin, gum, vinegar; distinct lab-station shapes; optional sound (silent until tapped). | client only | 1 round | 5, part of 3 |
| **B** | **Traces of other people (async social).** A visitors' book: each house and the Square notice post keep the last N stamps (name + one of ~8 preset phrases, **no free text**, so no moderation burden). Visiting an empty world still shows you that people were here. | daemon + client, one small table (migration + tests) | 1-2 rounds | 2, part of 3 |
| **C1** | **Materials become swatches (game only).** The Workshop gets one combining bench: real recipes (tannin + iron -> iron-gall ink, etc.) turn lookups into pigment swatches you can hang. Reopens "no crafting" in a small, honest form. | client + daemon inventory | 2 rounds, needs its own spec first | 1, 4 |
| **C2** | **Materials unlock colours in your civic drawing.** What you collect in the game becomes brush colours on the profile canvas. The real "Myspace + ballot box + gameboy" through-line. Touches civic (a new loopback route and canvas colour gate). | civic + daemon + client | large, its own spec, your call on unfreezing civic | 1, 3, 4 |
| **D** | **Nothing new; get people first.** The content is fine for a handful of players; traffic is the problem. | none | - | - |

## Recommendation

**A now, B next, decide C later.** A is cheap, client-only and removes the "why am I here" first
minute. B is the highest-value fix for the empty-world problem at the traffic we actually have,
without free text. C is the strategic choice (it is what would make the game *about* something), but
it is the only one that either reopens crafting (C1) or unfreezes civic (C2), so it should be your
explicit call, not a default. If you want C, I would pick **C1 first** as a cheap test of whether
combining materials is fun before touching civic.

## Not verified

I read code and logs; I did not play every region end to end this round. The traffic numbers are
rough (distinct IPs, some of them us; crawlers inflate page loads, so I only trust guest mints). No
analytics exist on the droplet beyond nginx logs.
