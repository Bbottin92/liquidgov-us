#!/usr/bin/env bash
# The Studio (BUILD studio 1, TheCommons/studio.md): gather/bench/mix/collect, server-authoritative,
# and the frozen catalog itself. No client exists yet -- every check here is against the daemon
# directly. The orphan-reap extension is tested in test_orphans.sh (it needs that file's own
# civic-gone-twice-in-a-row setup); this file only sets up and tears down its own daemon/civic.
source "$(dirname "$0")/lib.sh"
start_civic
start_daemon COMMONS_TEST_CLOCK_FILE="$CLOCK_FILE"
T0=1700000000000
set_clock "$T0"
for u in alice bob; do su $u; done

echo "-- the catalog itself: every Studio recipe is sound"
# BUILD property phase 2+3: 13 -> 15 (hammer, fence_post -- both tier 1, both from existing materials).
assert_eq "15 Studio recipes (duration_s set)" 15 "$(dbq "select count(*) from recipes where duration_s is not null")"
assert_eq "every one has a tier and both notes" 15 "$(dbq "select count(*) from recipes where duration_s is not null and tier is not null and real_range_note is not null and history_note is not null")"
assert_eq "every duration is positive and at most 7 days (the verdigris cap)" 15 "$(dbq "select count(*) from recipes where duration_s is not null and duration_s > 0 and duration_s <= 604800")"
assert_eq "no recipe_inputs quantity is anything but 1 (no stoichiometry, studio.md decision 2)" 0 "$(dbq "select count(*) from recipe_inputs where quantity != 1")"
assert_eq "every recipe_inputs item_id is a real item (the FK already guarantees this; asserted for documentation)" 0 "$(dbq "select count(*) from recipe_inputs ri where not exists (select 1 from items i where i.item_id=ri.item_id)")"
assert_eq "every recipe_outputs item_id is a real item" 0 "$(dbq "select count(*) from recipe_outputs ro where not exists (select 1 from items i where i.item_id=ro.item_id)")"
assert_eq "no recipe consumes its own output (a trivial self-loop)" 0 "$(dbq "select count(*) from recipe_inputs ri join recipe_outputs ro on ro.recipe_id=ri.recipe_id and ro.item_id=ri.item_id")"
assert_eq "tiers are monotonic: no input item is the output of a STRICTLY HIGHER-tier recipe (rules out any real cycle; same-tier sequencing like yellow_ochre -> red_ochre is fine)" 0 "$(dbq "
  select count(*) from recipe_inputs ri
  join recipes r_in on r_in.recipe_id = ri.recipe_id
  join recipe_outputs ro on ro.item_id = ri.item_id
  join recipes r_out on r_out.recipe_id = ro.recipe_id
  where r_in.duration_s is not null and r_out.duration_s is not null and r_out.tier > r_in.tier
")"
assert_eq "C1: no item slug is exactly 'ash' (the dropped charcoal by-product; seashell/etc containing the substring are unrelated)" 0 "$(dbq "select count(*) from items where slug = 'ash'")"
assert_eq "no lime, malachite or prussian anywhere (studio.md: not added)" 0 "$(dbq "select count(*) from items where slug like '%lime%' or slug like '%malachite%' or slug like '%prussian%'")"

echo "-- unbound (guest): every route refuses, none call through"
mint
for ep in gather bench mix collect wear; do
  post "$D/presence/$ep" '{}'; assert_code "guest POST /presence/$ep is 403 (bad_credential: no cookie at all)" 403
done
get "$D/presence/studio"; assert_code "guest GET /presence/studio is 403" 403

echo "-- gather: unknown spot, a real one, cooldown, and its release"
bgather alice nowhere_at_all; assert_code "unknown spot: 400" 400
assert_contains "unknown_spot" "$BODY" unknown_spot
bgather alice north_woods_twigs; assert_code "a real spot: 200" 200
assert_contains "granted willow_twigs" "$BODY" '"item":"willow_twigs"'
assert_contains "qty 3" "$BODY" '"qty":3'
bstudio alice; assert_contains "inventory shows it" "$BODY" '"item":"willow_twigs","qty":3'
bgather alice north_woods_twigs; assert_code "same spot again, no time passed: on cooldown" 409
assert_contains "on_cooldown" "$BODY" on_cooldown
bstudio alice; assert_contains "cooldowns lists the spot, still active" "$BODY" '"spot":"north_woods_twigs"'
set_clock $((T0 + 600001))                      # 10 minutes + 1ms: the cooldown (studio.cpp) has passed
bgather alice north_woods_twigs; assert_code "after the cooldown: 200 again" 200
bstudio alice; assert_contains "inventory now shows 6 (capped sensibly, not reset)" "$BODY" '"item":"willow_twigs","qty":6'

echo "-- gather: the orchard keg's cooldown is 12h (BUILD studio 5, was 1 day; the recipe durations that consume vinegar are unchanged)"
su carol
KEG0=$((T0 + 600001))
bgather carol orchard_keg; assert_code "carol's first keg visit: 200" 200
assert_contains "granted vinegar" "$BODY" '"item":"vinegar"'
assert_contains "qty 3" "$BODY" '"qty":3'
bgather carol orchard_keg; assert_code "immediately again, same clock: still on cooldown" 409
assert_contains "on_cooldown" "$BODY" on_cooldown
set_clock $((KEG0 + 43200000 - 1))              # 1ms short of 12h: still on cooldown
bgather carol orchard_keg; assert_code "1ms short of 12h: still 409" 409
assert_contains "on_cooldown" "$BODY" on_cooldown
set_clock $((KEG0 + 43200001))                  # 12h + 1ms: released
bgather carol orchard_keg; assert_code "after 12h: 200 again" 200
bstudio carol; assert_contains "inventory now shows 6 (a second keg visit, not a full day's wait)" "$BODY" '"item":"vinegar","qty":6'

echo "-- bench: unknown recipe, a Lab-A lookup is refused too, missing inputs, then a real start"
bbench alice not_a_recipe; assert_code "unknown recipe: 400" 400
assert_contains "unknown_recipe" "$BODY" unknown_recipe
bbench alice vinegar; assert_code "a Lab-A lookup (duration_s NULL) is not benchable: 400" 400
assert_contains "unknown_recipe (bench only knows Studio recipes)" "$BODY" unknown_recipe
bbench bob charcoal; assert_code "bob holds no willow_twigs yet: 409 missing_inputs" 409
assert_contains "missing_inputs" "$BODY" missing_inputs
bbench alice charcoal; assert_code "alice holds 6 willow_twigs: 200" 200
BATCH1="$(jget batch_id)"; READY1="$(jget ready_ms)"
# The clock is wherever the keg test above left it (KEG0 + 43200001), not T0 + 600001 -- that fixed
# offset was only ever right before the keg test existed. Compute it, don't hardcode a stale value.
assert_eq "ready_ms is start + 28800s (8h, the frozen duration)" "$((KEG0 + 43200001 + 28800000))" "$READY1"
bstudio alice; assert_contains "inventory shows the deduction (6 -> 5)" "$BODY" '"item":"willow_twigs","qty":5'
assert_contains "the open batch is listed" "$BODY" "\"batch_id\":$BATCH1"

echo "-- bench: the slot cap is 5 (BUILD studio 5, was 3), enforced independently of material (6 twigs is enough for 6)"
bbench alice charcoal >/dev/null; bbench alice charcoal >/dev/null; bbench alice charcoal >/dev/null; bbench alice charcoal >/dev/null   # 4 more: 5 open batches, 1 twig left
bstudio alice
echo "$BODY" > "$WORK/studio_state.json"
assert_eq "five open batches" 5 "$(python3 -c "import json; print(len(json.load(open('$WORK/studio_state.json'))['batches']))")"
bbench alice charcoal; assert_code "a 6th, with material still available: 409 slots_full, not missing_inputs" 409
assert_contains "slots_full" "$BODY" slots_full

echo "-- bench: a multi-input recipe, holding the first but not the second, rolls back the first deduction too"
bgather bob workshop_iron >/dev/null           # bob now holds 3 scrap_iron
bstudio bob; assert_contains "precondition: bob holds scrap_iron, not vinegar" "$BODY" '"item":"scrap_iron","qty":3'
assert_not_contains "" "$BODY" '"item":"vinegar"'
bbench bob iron_acetate; assert_code "iron_acetate needs scrap_iron + vinegar; bob is missing the second: 409" 409
assert_contains "missing_inputs" "$BODY" missing_inputs
bstudio bob; assert_contains "bob's scrap_iron was NOT deducted (the first input's deduction rolled back with the second's failure)" "$BODY" '"item":"scrap_iron","qty":3'

echo "-- collect: not ready, then ready, first discovery, idempotent, ownership, unknown id"
bcollect alice "$BATCH1"; assert_code "before ready_ms: 409 not_ready" 409
assert_contains "not_ready" "$BODY" not_ready
set_clock "$READY1"
bcollect alice "$BATCH1"; assert_code "at exactly ready_ms: 200" 200
assert_contains "granted all four charcoal outputs" "$BODY" charcoal_black
assert_contains "" "$BODY" charcoal_grey_1
assert_contains "" "$BODY" charcoal_grey_2
assert_contains "" "$BODY" charcoal_grey_3
assert_contains "first_discovery is true (alice's first ever charcoal collect)" "$BODY" '"first_discovery":true'
bstudio alice; assert_contains "the discovery is listed" "$BODY" '"recipe":"charcoal"'
assert_contains "the palette now has the four charcoal pigments" "$BODY" '"item":"charcoal_black","hex":"#1a1a1a"'
# BUILD studio 3 ("the one daemon change"): a discovery now carries its own display_name, real_range_note
# and history_note, so the notebook can print a line with no second recipe dump.
assert_contains "the discovery carries its display_name" "$BODY" '"display_name":"Willow twigs -> charcoal"'
assert_contains "the discovery carries its real_range_note" "$BODY" '"real_range_note":"Hours, plus cooling'
assert_contains "the discovery carries its history_note" "$BODY" '"history_note":"Left overnight'
# recipe_count is the notebook's own length: every bench recipe (duration_s set), not per-player.
# BUILD property phase 2+3: 13 -> 15 (hammer, fence_post).
assert_contains "recipe_count is the fixed 15 bench recipes" "$BODY" '"recipe_count":15'
bcollect alice "$BATCH1"; assert_code "collecting the same batch again: 409" 409
assert_contains "already_collected" "$BODY" already_collected
bcollect bob "$BATCH1"; assert_code "bob tries to collect alice's batch: 403 not_yours" 403
assert_contains "not_yours" "$BODY" not_yours
bcollect alice 999999; assert_code "an unknown batch_id: 404" 404
assert_contains "not_found" "$BODY" not_found

echo "-- a second batch of the SAME recipe: the outputs still grant, but it is not a NEW discovery"
bbench alice charcoal >/dev/null   # BATCH1's slot just freed on collect; this spends alice's last twig (5 used already, 1 left)
# (find its batch id: the open batches list now has exactly four -- the four started earlier that were
# never collected, plus this new one makes five again; collect all remaining ones to isolate cleanly)
bstudio alice
echo "$BODY" > "$WORK/studio_state.json"
python3 -c "
import json
d = json.load(open('$WORK/studio_state.json'))
for b in d['batches']: print(b['batch_id'], b['ready_ms'])
" > "$WORK/open_batches.txt"
while read -r bid bready; do
  set_clock "$bready"
  bcollect alice "$bid"
  assert_code "collect remaining open batch $bid" 200
  assert_contains "not a first discovery (already discovered charcoal earlier)" "$BODY" '"first_discovery":false'
done < "$WORK/open_batches.txt"
bstudio alice; assert_contains "still exactly one discovery of charcoal (discoveries de-dupe by recipe, not per batch)" "$BODY" '"recipe":"charcoal"'
assert_eq "and only one" 1 "$(dbq "select count(*) from discoveries where recipe_id=(select recipe_id from recipes where slug='charcoal')")"

echo "-- makeable-now: appears once inputs are held, disappears once they are spent"
bstudio bob; assert_not_contains "bob (no willow_twigs) does not see charcoal as makeable" "$BODY" '"recipe":"charcoal"'
bgather bob north_woods_twigs >/dev/null
bstudio bob; assert_contains "bob (now holding 3) sees it" "$BODY" '"recipe":"charcoal","display_name"'
bbench bob charcoal >/dev/null; bbench bob charcoal >/dev/null; bbench bob charcoal >/dev/null   # spends all 3 (well under the cap of 5)
bstudio bob; assert_not_contains "bob (spent to 0) no longer sees it as makeable" "$BODY" '"recipe":"charcoal","display_name"'

echo "-- mix: fixed texts, the rate limit, and the cap of 4"
bmix alice '["shell_white","vinegar"]'; assert_code "a known bad combination: 200 (it is not an error, just a result)" 200
assert_contains "the fixed fizz text" "$BODY" fizzes
assert_not_contains "never a recipe_id" "$BODY" recipe_id
bmix alice '["chalk_block","vinegar"]'; assert_code "the second fizz combination, immediately after: 429 (rate limited)" 429
assert_contains "too_fast" "$BODY" too_fast
set_clock $((READY1 + 28800000 + 5001))
bmix alice '["chalk_block","vinegar"]'; assert_code "5s later: 200 again" 200
assert_contains "same fixed text, different ingredient pair" "$BODY" fizzes
set_clock $((READY1 + 28800000 + 10002))
bmix alice '["scrap_iron","vinegar"]'; assert_code "rust-brown combination" 200
assert_contains "rust-brown" "$BODY" rust-brown
set_clock $((READY1 + 28800000 + 15003))
bmix alice '["tannin"]'; assert_code "tannin alone" 200
assert_contains "weak brown stain" "$BODY" "weak brown stain"
set_clock $((READY1 + 28800000 + 20004))
bmix alice '["willow_twigs","seashell"]'; assert_code "an unlisted combination" 200
assert_contains "the generic fallback text" "$BODY" "Nothing useful"
set_clock $((READY1 + 28800000 + 25005))
bmix alice '["a","b","c","d","e"]'; assert_code "5 slugs, over the cap of 4: 400" 400
assert_contains "missing_field (the array itself is rejected, not silently truncated to 4)" "$BODY" missing_field
bmix alice '["a","b","c","d"]'; assert_code "exactly 4 is fine (falls through to the generic text)" 200
assert_eq "mix never wrote a discoveries row for anyone" 1 "$(dbq "select count(distinct recipe_id) from discoveries")"

echo "-- wear a pigment (BUILD wear a pigment, CLAUDE_WEAR_PIGMENT.md): the unlock rule and a clean refusal shape"
su dave
bwear dave '{"index":1}'; assert_code "charcoal black (always legal, never discovery-gated): 200" 200
assert_contains "resolved hex comes back" "$BODY" '"hex":"#1a1a1a"'
bwear dave '{"index":14}'; assert_code "past index 13: 400 unknown_index" 400
assert_contains "unknown_index" "$BODY" unknown_index
bwear dave '{"index":13}'; assert_code "woad blue, never discovered: 409 not_unlocked" 409
assert_contains "not_unlocked" "$BODY" not_unlocked
bwear dave '{}'; assert_code "an empty body clears back to the default: 200" 200
assert_contains "empty hex on a clear" "$BODY" '"hex":""'

echo "-- wear a pigment: unlocking index 8 (carbon ink) for real, an actual gather->bench->collect chain, not a DB shortcut"
set_clock $((READY1 + 28800000 + 30000))
bgather dave lab_gum; assert_code "gather gum: 200" 200
bgather dave lab_soot; assert_code "gather soot: 200" 200
bbench dave gum_solution; assert_code "start gum_solution (gum_arabic_raw -> binder): 200" 200
GS_BATCH="$(jget batch_id)"; GS_READY="$(jget ready_ms)"
bwear dave '{"index":8}'; assert_code "carbon ink still undiscovered (binder not even made yet): 409" 409
set_clock $((GS_READY + 1))
bcollect dave "$GS_BATCH"; assert_code "collect gum_solution: 200" 200
bbench dave carbon_ink; assert_code "start carbon_ink (soot + binder): 200" 200
CI_BATCH="$(jget batch_id)"; CI_READY="$(jget ready_ms)"
set_clock $((CI_READY + 1))
bcollect dave "$CI_BATCH"; assert_code "collect carbon_ink: 200" 200
bwear dave '{"index":8}'; assert_code "now discovered, wearable: 200" 200
assert_contains "carbon ink's own hex" "$BODY" '"hex":"#0d0d0d"'

echo "-- wear a pigment: persists across a rejoin, and a nearby occupant sees a live change with no rejoin at all"
# No clock reset needed: PresenceStore is constructed with main.cpp's test-clock-aware now_ms(), so
# occupants are stamped and reaped in the same (frozen) timebase.
bjoin dave "1,1"; assert_code "dave joins the Square: 200" 200
assert_contains "the join response carries dave's own worn colour" "$BODY" '"shirt":"#0d0d0d"'
su erin
bjoin erin "1,1"; assert_code "erin joins the same region: 200" 200
occupants "1,1"; assert_contains "the region GET shows dave wearing carbon ink" "$BODY" '"shirt":"#0d0d0d"'
bwear dave '{}'; assert_code "dave clears back to default while still standing there: 200" 200
occupants "1,1"; assert_not_contains "and it's gone from the region GET immediately, no rejoin needed" "$BODY" '"shirt"'

echo "-- presence idle-reap follows the injected test clock, not the wall clock"
occupants "1,1"; assert_contains "at the frozen clock, dave is present (not reaped for being 2023 vs. real time)" "$BODY" '"display_name":"dave"'
bjoin dave "1,1" >/dev/null                      # fresh stamp at the current frozen time
PRES0="$(cat "$CLOCK_FILE")"
set_clock $((PRES0 + 20000))                     # 20s: inside kIdleMs (30s)
bmove erin "1,1"; assert_code "erin moves at +20s, refreshing her stamp: 200" 200
occupants "1,1"; assert_contains "dave still present at +20s" "$BODY" '"display_name":"dave"'
set_clock $((PRES0 + 40000))                     # dave idle 40s (> 30s), erin idle 20s
occupants "1,1"; assert_not_contains "dave reaped at +40s (idle > kIdleMs on the test clock)" "$BODY" '"display_name":"dave"'
assert_contains "erin (moved 20s ago) survives" "$BODY" '"display_name":"erin"'

echo "-- property phase 2+3: gather, craft a hammer and a fence post (CLAUDE_PROPERTY_PHASE2.md)"
su frank
GSTART=$((PRES0 + 40001))
set_clock "$GSTART"
bgather frank north_woods_twigs; assert_code "gather willow twigs (qty 3): 200" 200
bgather frank workshop_iron; assert_code "gather scrap iron: 200" 200
bbench frank hammer; assert_code "start hammer (willow_twigs + scrap_iron): 200" 200
HAMMER_BATCH="$(jget batch_id)"; HAMMER_READY="$(jget ready_ms)"
assert_eq "hammer ready_ms is start + 3600s (1h)" "$((GSTART + 3600000))" "$HAMMER_READY"
set_clock $((HAMMER_READY + 1))
bcollect frank "$HAMMER_BATCH"; assert_code "collect hammer: 200" 200
bstudio frank; assert_contains "inventory shows the hammer" "$BODY" '"item":"hammer","qty":1'
assert_contains "and 2 willow_twigs remain (gathered 3, hammer used only 1)" "$BODY" '"item":"willow_twigs","qty":2'
bbench frank fence_post; assert_code "start fence_post (willow_twigs, already held): 200" 200
FP_BATCH="$(jget batch_id)"; FP_READY="$(jget ready_ms)"
assert_eq "fence_post ready_ms is start + 1800s (30 min)" "$((HAMMER_READY + 1 + 1800000))" "$FP_READY"
set_clock $((FP_READY + 1))
bcollect frank "$FP_BATCH"; assert_code "collect fence_post: 200" 200
bstudio frank; assert_contains "inventory shows the fence post" "$BODY" '"item":"fence_post","qty":1'

stop_daemon
echo "== test_studio: done"
