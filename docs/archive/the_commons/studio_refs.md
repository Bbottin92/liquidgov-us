# Studio recipe references (reference pass, 2026-09-20)

Companion to `studio.md` v2. **Reference check only: no catalog rows, no code, no BUILD.** Brandon signs the
frozen table after this.

## How this was done (so it can be repeated)

I downloaded the full text of each source and searched it, so every quote below is **verbatim from the source
text (whitespace normalised)** and can be grepped. Nothing is quoted from memory or from a summariser.

| Short name | What | Where to get the text |
|---|---|---|
| **Cennini / Herringham** | *The Book of the Art of Cennino Cennini*, tr. Christiana Herringham (1899), with her introduction and Book II notes | archive.org `bookofartofcenni00cenn` |
| **Merrifield vol. I** | Mary P. Merrifield, *Original Treatises ... on the Art of Painting* (1849), the essays (Chap. VI "Yellow Pigments") | archive.org `originaltreatis00merrgoog` |
| **Merrifield vol. II** | same work; contains the **Bolognese Manuscript** (running head "VOL. II ... BOLOGNESE MANUSCRIPT") and *De multis azurris per artificium fiendis* | archive.org `originaltreatis01merrgoog` |
| **Theophilus / Hendrie** | *An Essay upon Various Arts*, tr. Robert Hendrie (1847), Book I | archive.org `essayuponvarious00hend` |
| **Pliny** | *Natural History* 35.25, Bostock & Riley translation | Perseus `1999.02.0137` (book 35, ch. 25) |

Fetch with `https://archive.org/download/<id>/<id>_djvu.txt`; search with any regex.

**Could not use:** CAMEO (returns 403 to scripts), and the AIC/`cool.culturalheritage.org` ink-corrosion page
(returned a binary blob). **Not verified by me:** Grok's "Coates' modern kiln ~10 h firing" and his "shell white
weathered for years"; I found no text for either in the sources above.

## Status legend

- **Confirmed**: a source supports the ingredient and history line (and the time, where marked).
- **Corrected**: supported, but the game line needs the change shown.
- **Unsourced (game number)**: no source in this pass gives the claim; the game may still use the number, but
  **the notebook must not present it as a real historical range** (show "here: N" only).

## Recipe by recipe

| # | Recipe | Status | Source and locator | Quote (grep this) |
|---|---|---|---|---|
| 1 | **Charcoal** (willow twigs) | **Confirmed; time supported** | Cennini/Herringham ch. 33; drawing use ch. 122 | ch. 33: "let it remain till morning"; ch. 122: "those sticks of willow charcoal which I previously taught you to make" |
| 2 | **Shell white** (seashells) | **Corrected / partly sourced** | Merrifield vol. II, Bolognese MS., azure recipe 48 | "lime made from egg-shells is better" (shell as a lime/white base; no source here for the "weathered for years" claim) |
| 3 | **Yellow ochre** | **Ingredient confirmed; process and time unsourced** | Herringham introduction (quoting Vitruvius) | "light sil (yellow ochre)" |
| 4 | **Red ochre** (yellow ochre, heated) | **Confirmed** | Herringham introduction p. xxxvii; Herringham Book II notes; Merrifield vol. II footnote to recipe 194 | "usta being properly yellow ochre burnt"; "light red, which ought to be calcined yellow ochre (also known to the ancients)"; "This must be burnt ochre." |
| 5 | **Gum solution** (gum arabic + water) | **Binder confirmed; "overnight" unsourced** | Merrifield vol. II, Bolognese MS. (white-lead painting recipe) | "as much gum-water as you think it will bear" |
| 6 | **Oak-gall tannin** | **Galls confirmed; "1-2 d" unsourced** | Merrifield vol. II, Bolognese MS. B. 134 | "3 oz. of pounded galls" |
| 7 | **Carbon ink** (soot + gum) | **Confirmed** | Pliny NH 35.25; Cennini ch. 37 (lamp black) | Pliny: "the black, for writing, having an admixture of gum"; "the ordinary soot from furnaces and baths, a substance which is also employed for the purpose of writing"; Cennini ch. 37: "Take a lampful of linseed oil" |
| 8 | **Iron acetate** (iron + vinegar) | **Unsourced: folk method** | none found in this pass | (period practice used a vitriol salt: see row 9) |
| 9 | **Iron-gall ink** | **Corrected: vitriol, not acetate** | Merrifield vol. II, Bolognese MS. B. 134 "To make a perfect black" | "Roman vitriol in powder 3 oz. and 3 oz. of pounded galls" (caution: Merrifield's own note elsewhere says "Roman Vitriol" may mean copper sulfate; the type is ambiguous) |
| 10 | **Verdigris** (copper + vinegar) | **Confirmed; time supported** | Cennini ch. 56; Theophilus Bk I ch. XXXVIII "Of Spanish green" | Cennini: "made of copper and vinegar"; Theophilus: "After two weeks examine and scrape it" (two weeks per scraping) |
| 11 | **Madder lake** | **Corrected** | Herringham Book II note on "Lacca (c. 44)"; Merrifield vol. II Bolognese MS. dye recipes 328, 367 | "Madder is scarcely mentioned in Italian documents between the Lucca MS. and the middle of the sixteenth century, though known in the north"; "when it is dry alum it" (madder dyed cloth, alum-mordanted) |
| 12 | **Weld lake** | **Confirmed (strongly)** | Merrifield vol. I ch. VI "Arzica"; Merrifield vol. II Bolognese MS. recipe 194 | "a yellow lake made from the herb"; "gualda"; "Reseda luteola"; recipe 194: "Take one pound of weld, which the dyers use"; "2 oz. of travertine finely ground"; "roche alum" |
| 13 | **Woad blue** (simplified lake) | **Corrected: drop "ash"** | Merrifield vol. II, *De multis azurris per artificium fiendis*, no. 36; Herringham Book II notes | "use the froth of dyer's woad instead of it"; "mix it with the egg-shells"; Herringham: "imitated with woad ... a very fugitive pigment" |

### Findings that change the spec

1. **Charcoal.** Cennini ch. 33 *does* give a clock: the sticks stay "till morning", i.e. **overnight**. That
   supports the game's **8 h** better than Grok's "no clock" and than my 2-4 h guess. Coates' 10 h is unverified.
2. **Verdigris.** Theophilus scrapes after **two weeks**. The "typically 1-3 weeks" label and the 7-day cap stand.
3. **Weld** is the best-supported line: Cennini's "arzica" *is* a weld lake (Merrifield), and the Bolognese
   recipe sets weld with **ground calcium carbonate (travertine)** and **alum**, exactly the game's
   weld + alum stone + shell white.
4. **Woad.** A period azure really did use the froth of the woad dyer's vat with shell-derived lime
   (no. 36), so "woad + shell white" is historically grounded. **There is no "ash" in that source**, so the
   ash ingredient is unsupported here. (Ash also drags in lye, which the safety line excludes.)
5. **Madder.** Madder-as-alum-mordanted-dye is well attested; **madder lakes for painters are later and
   northern**, and Cennini's Italy used kermes, lac and brazilwood for its lakes.
6. **Iron acetate** has no source here. Period inks used a **vitriol salt**, and even that salt's identity is
   ambiguous. Only iron-gall's gall + vitriol pairing is sourced.

## Proposed corrections (each needs Brandon's yes; the v2 table is unchanged until then)

| # | Line | Correction |
|---|---|---|
| C1 | Woad blue | **Remove "ash"**; ingredients become woad + shell white + water. Then **drop the charcoal ash by-product** too (it would have no use), or keep it as an unused trophy: recommend drop. |
| C2 | Madder lake | Keep, but the history line must say lakes for painters are later/northern (see notebook line). |
| C3 | Iron acetate | Keep as an **explicitly folk, unsourced** step (the notebook says so) **or cut it** and give iron-gall its iron from a vitriol raw. Recommend keep + label; Brandon's call. |
| C4 | Unsourced numbers | Yellow-ochre wash/settle 1 d, tannin 1 d, gum 8 h, shell white 1 d, iron-gall 12 h are **game numbers**. The notebook shows **"here: N"** for these and shows a **real range only where a source gives one** (charcoal: overnight; verdigris: about two weeks). |
| C5 | Shell white | Drop the "weathered for years" claim from any history line (unverified here); use the sourced shell-lime line below. |
| C6 | Charcoal history line | Use the sourced line below. **Not** "hours in a closed pot": that instruction contradicts the safety rule against "closed pot" in the notebook, so I followed the safety rule. |

## Notebook lines (ingredients + time + one history line; no steps, temperatures, pH, lye or urine)

Draft text for review; every claim maps to a row above.

| Recipe | Notebook line |
|---|---|
| Charcoal | Willow twigs. Left overnight in Cennino Cennini's manual (c. 1400); here 8 hours. |
| Shell white | Seashells. Here: 1 day. Medieval painters' recipe books used egg-shell lime as a base for blues. |
| Yellow ochre | Ochre earth. Here: 1 day. Yellow ochre ("sil") is one of the oldest painters' earths. |
| Red ochre | Yellow ochre, heated. Here: 4 hours. Burnt yellow ochre ("usta") gave painters a red from antiquity. |
| Gum solution | Gum arabic and water. Here: 8 hours. Gum-water was a common medieval binder. |
| Oak-gall tannin | Oak galls and water. Here: 1 day. Galls are the tannin in old black inks. |
| Carbon ink | Soot and gum solution. Here: 30 minutes. Pliny (c. AD 77): writing black is soot with gum. |
| Iron acetate | Iron and vinegar. Here: 3 days. A folk method, not a period one; old inks used a vitriol salt. |
| Iron-gall ink | Tannin, iron acetate, gum solution. Here: 12 hours. Galls and vitriol made a black in the Bolognese MS. (c. 1400). |
| Verdigris | Copper and vinegar. Typically 1-3 weeks; here 7 days. Theophilus (c. 1100) scrapes it after two weeks. Toxic; not for eating. |
| Madder lake | Madder, alum stone, shell white. Here: 2 days. Madder dyed cloth in medieval Europe; painters' madder lakes came later, in the north. |
| Weld lake | Weld, alum stone, shell white. Here: 1 day. Cennini's yellow "arzica" was a weld lake; the Bolognese MS. sets weld with ground chalky stone and alum. |
| Woad blue | Woad, shell white, water. Here: 3 days. A period azure used the froth of the woad dyer's vat on shell lime; simplified here. |

## Cut list (unchanged; nothing here reintroduces them)

Homemade vinegar, alum-from-alum, lead white, vermilion, orpiment, Prussian blue, lime, malachite. Note the
sources above contain recipes for several of these (ceruse, minium, orpiment, quicklime): **none is used**,
and the lines above quote only the benign clauses.
