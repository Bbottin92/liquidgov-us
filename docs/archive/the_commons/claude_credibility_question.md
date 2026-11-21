# Credibility: earned agency, fair consequences, no sabotage (questions before building)

Asked by Claude, 2026-11-15. Brandon: "I want to figure out a good, fair way to allow people to earn more credibility to
access more agency or face bad credibility if you do things that people disapprove of. but i want to make sure that one
person cant sabotage anothers credibility". Nothing is built.

## What exists today

- Profiles show "Standing: Associate · good": fixed text, nothing computes it.
- The site's own pages say a platform account is "not a citizen identity, not a vote, and not standing", and that
  voting weight, offices, land and judgments live in a separate governance kernel.
- Accounts are bound to a phone at signup (one person, one account, roughly). Friends are mutual. Blocking hides
  people from each other.

## The core idea: credibility is something many independent people give you, never one

1. **Many voices, each capped.** Whatever one person does can only move your credibility a little, once per period,
   with diminishing returns: the second and third marks from the same person count less than the first. Nobody can
   push you up or down alone, however hard they try.
2. **Independent agreement counts more than a crowd.** People who are closely linked (friends, the same small group,
   accounts that always act together, all new) count partly as one voice. A brigade of ten friends counts nearer to
   two people than to ten. Agreement from people who do not know each other counts most.
3. **Disapproval is about a specific act, with a reason, and is judged, not tallied.** A report on something you did
   (a post, a message, a page) does nothing by itself. Enough independent reports send it to a small jury: people
   picked at random, not connected to either side, with decent credibility of their own. Only the jury's finding moves
   your credibility, and by an amount that fits the act.
4. **False reporting costs the reporter.** If juries keep disagreeing with someone's reports, their reports weigh less,
   and repeated bad-faith reports cost their own credibility. Reporting someone right after a dispute with them (they
   reported you, rejected your proposal, unfriended you) is discounted.
5. **Nothing is permanent.** Bad marks fade over months; good standing is earned slowly and kept by being active. You
   can see why your credibility changed (which act, what the jury found), and appeal once to a fresh jury. Reporters
   stay anonymous to you, so reporting is safe.
6. **New accounts start in the middle and weigh little.** What a new account gives or takes counts for little until it
   has some history, so making accounts to attack someone does not work (the phone binding already makes that costly).
7. **A floor nobody can lose.** Some things credibility never takes away: talking to friends, reading, your own page,
   blocking, reporting, appealing. Credibility only unlocks more on top.

## What credibility could unlock (agency)

A ladder, for example: posting where strangers see it → starting a group → making the 3 blank pages → starting
proposals → wider reach in feeds and on the map → being picked for juries → moderating a group. Each step needs a
level and a little time on the site.

## Questions for Brandon

1. **Civic power or only the platform?** Should credibility ever touch votes, proposals or anything civic, or only
   platform features (reach, pages, groups, juries)? Claude's recommendation: never vote weight, ever (one person, one
   vote; a credibility-weighted vote lets the popular silence the unpopular). At most, starting a proposal needs a
   modest level, so spam stays out.
2. **What earns it?** Options: (A) endorsements people give on purpose ("this person is credible about X"); (B) earned
   from what others do with your work (proposals that pass, posts people keep, groups people stay in); (C) both.
   Claude: C, with B weighted more, since it is harder to fake.
3. **One number or many?** (A) one site-wide credibility; (B) credibility per group or topic (trusted in the gardening
   group says nothing about politics); (C) both, site-wide being a blend. Claude: C.
4. **Who sees it?** (A) everyone sees everyone's level; (B) you see your own, others see only what it unlocked;
   (C) public level, private details. Claude: C.
5. **Juries**: is a small random jury (say 5 people) the right shape for judging disapproved acts, or do you picture
   something else?

## Round 2: different aspects of credibility (Brandon, 2026-11-15)

Brandon: "also if we had like different aspects of credibility that are accounted for, so if you get a reputation for
being hyper judgemental, then maybe your judgements regarding other's credibility would have less weight accordingly".

Proposed aspects, each its own number:
- **Contribution**: what others do with your work (proposals that pass, posts people keep, groups people stay in).
- **Conduct**: the outcome of juries about things you did.
- **Judgment**: how well your reports, endorsements and jury votes match what independent juries and the wider community
  end up finding. It sets how much weight your opinions of others carry.
- **Topic standing** (optional): contribution within a group or a subject.

How Judgment works, so nobody can game it or be framed by it:
- It is measured, not voted on: nobody rates you as "judgmental". It comes from comparing your calls with independent
  outcomes over many cases. So a rival cannot lower it.
- It catches both directions: someone much harsher than independent juries (reports everything, rates everyone low) and
  someone much softer (endorses all their friends, never reports) both drift toward less weight. Being fair, agreeing
  with independent outcomes most of the time, keeps full weight.
- Disagreeing is not punished: being in the minority now and then is normal. Only a steady pattern over many cases
  moves it, and a one-off is invisible.
- It only changes how much your opinions of others count. It never lowers your own Contribution or Conduct, and never
  touches the floor.
- Juries are picked with Judgment in mind, so the people judging are the ones whose calls tend to hold up.

## Round 3: a leaning, not a decision: never vote weight; shown beside what you contribute (Brandon, 2026-11-15)

Brandon: "it shouldnt effect the weight of your votes but perhaps it could be shown in your comments or proposals to give
people an idea of how seriously they should consider what you contribute", then "take that with a grain of salt". So
question 1 leans one way (one person, one vote; credibility as information beside comments and proposals) but is open.

Claude's proposal for how it shows, so it informs without becoming a way to dismiss people:
- Beside a comment or proposal, a small mark for the aspect that matters there: Topic standing in that group or subject
  if there is one, else Contribution. Tap it to see what it rests on ("12 proposals, 9 passed; trusted in Fresno
  Neighbors").
- New accounts show "New", not a low score: no history is not bad history.
- Conduct shows only when a jury has found against someone recently, as a plain note ("a recent post was found
  misleading"), fading when the mark fades. It is never a permanent badge.
- Judgment is never shown on posts (it is about how you judge others, not about what you wrote); you see your own.
- Sorting stays yours: comments are not hidden or ranked down for low credibility unless a reader chooses that.

## Round 4: thumbs on pools feed credibility, and their weight comes from it (Brandon, 2026-11-16; a direction, not a design)

Context: Brandon's future-list item 3 (`Persistence/notes.md`, 2026-11-16): a thumb up or down on the page a pool opens, tinting the streams to it green, red or (with little feedback) blue. His answer to whether a thumbs-down on a person is wise: "i think we should wire the weight of people's thumbs up or thumbs down into the credibility discussion".

Claude's reading, to be argued with:
- **Thumbs are not votes.** Round 3's lean (credibility never touches the weight of a civic VOTE: one person, one vote) is about votes on proposals. A thumb on a pool is feedback about a thing, like a reaction, and may be weighted. Keep the two apart in every screen and every word, so nobody reads a weighted thumb as a weighted vote.
- **Weight comes from Judgment, not popularity.** A thumb weighs more from someone whose past calls matched what independent people ended up finding (Round 2's Judgment), less from a new account, and never more than a cap per person. Popularity must not buy weight: that would be a loop (the liked are believed, so they are liked).
- **Independent agreement counts more than a crowd** (Round 1, point 2): ten linked accounts thumbing the same thing count nearer to two voices. This is also what keeps a few angry accounts from turning a stream red.
- **Thumbs feed the author's Contribution, through the same weights.** What others do with your work is Contribution (Round 2); a weighted thumbs-up on your post is such a signal, a weighted thumbs-down is a signal that goes to a jury only when enough independent people agree (Round 1, point 3), never straight into a score.
- **People's pools: no thumbs on the person, thumbs on what they did.** A thumbs-down on a person invites exactly the sabotage this whole document is against. Disapproval attaches to a specific post or comment (the act), where it can be judged. The person's pool stays unthumbed.
- **Tint = balance AND amount.** Few weighted thumbs stays blue, whatever their direction; green or red needs enough independent weight. Fades with time like every other mark (Round 1, point 5).

Questions: should a person see their own thumb history and its effect (their Judgment)? Should thumbs be public or only the tint? Does a stream's tint show to everyone, or only to people the thing is visible to?

### Round 4, Brandon's answers (2026-11-16)

- **History: yes.** "i think there should be a way to see your history": you can see your own thumbs, and (with Round 2's Judgment) how they weighed.
- **Counts on the page.** "i think the thumb counts should be displayed on the page": each page a pool opens shows how many thumbs up and how many down.
- **Private thumbs.** "you should be able to make your thumb reaction private if you want."
- **Who reacted.** "if you click on the thumb count it should show you all of the people who reacted in that way": the names behind a count, except those who chose private.
- **Tint: asked Claude.** "i dont know if everyone should see the tint, what do you think".

Claude's answer on the tint (a lean): everyone who can see the pool sees the tint on the streams to it, and no one sees a tint for something they cannot see. It is a navigation signal like a stream's width, it is an aggregate (never one person), and it needs enough independent weight and fades, so it is hard to use as a public pile-on. The page's counts are the plain head count (private thumbs included, unnamed: "and 3 private"); the tint is the weighted balance; the two can differ, and the page should say so in one line. Open: whether thumbs are public or private BY DEFAULT (a public thumbs-down is easy to retaliate against; a private-by-default thumbs-down cannot be held to account).

Clarification (Brandon, 2026-11-16): the count is EVERY thumb, private ones included, and everyone sees that number. Example: 5 people thumb down, 2 chose private: the count everyone sees is 5; clicking it lists the 3 who did not choose private. So a count can be larger than its list, and the difference is the private thumbs. (Claude had sketched an "and 2 private" line under the list; whether to say so, or show only the 3 names, is open.)

Order of thumbs (Brandon, 2026-11-16): "you should see the order in which people thumbbed. i feel like that could be useful data somehow". The list behind a count shows who reacted in the order they did (first thumb first). Claude's notes: (1) the order is a signal for Judgment (Round 2): someone who thumbed early, before the count was visible, and whose call independent people later agreed with, has shown better judgment than someone who joined a crowd already going the same way, so early independent thumbs can weigh more than late ones that followed a visible count; (2) showing the order publicly can itself start a bandwagon (people copy the early ones), which argues for showing the order only in the click-through list, never on the page's headline; (3) private thumbs are not named, but they keep their place in the true order (so the order the public sees has gaps, which is honest); (4) whether to show a time (3 minutes after the post, 2 days after) beside each name, or only the order, is open.

Decided (Brandon, 2026-11-16): a time is shown beside each name in the list ("i like the time addition"), and private thumbs keep their place in the true order. Still open: the "and N private" line under the list; public or private by default; who sees the tint (Claude's lean: everyone who can see the pool).

Decided (Brandon, 2026-11-16), closing Round 4's open questions:
- **No "and N private" line** under the list ("-no"): the list shows only the people who did not choose private.
- **Public by default.**
- **A menu that expands from the corner of whatever post or thing you are looking at**, where you choose: thumbs always private; or specific people or groups who cannot see your thumbs; or thumbs private on that one thing.
- **Everyone sees the tint.**
Consequence to build for: a thumb is counted for everyone (the count and the tint), but its NAME is left out of the list for the viewers you hid it from, and for everyone if it is private. The same thumb can therefore be named to one viewer and unnamed to another.

## Round 5: many kinds, strengths-only display, the crowd, effort (Brandon, 2026-11-21; BRAINSTORM, nothing decided)

Brandon: "im just spit balling so you take all of this with a grain of salt". Everything below is ideas and leanings to
argue with. Rounds 1-4 stand; thumbs (Round 4) can be built without anything here except their weighting.

**Many kinds, avenues and uses.** Brandon: "multiple avenues for credibility, and different kinds, and different
applications of it. like positive and negative, and trust, morality, creativity maybe, obedience, bravery". A rough cut:
Trust (claims hold up, commitments kept), Creativity (things others keep or remix), Bravery (right early and against the
crowd, below), Fairness (calls that match independent juries), Contribution (Round 2), each earned its own way and each
unlocking its own things. Claude's pushback, open: score **Reliability** (kept commitments) rather than obedience, since on
a civic site rewarding obedience punishes dissent; and no **morality** score (the majority grading the minority's values);
honesty and fair judgement of others are the measurable parts.

**Strengths only in public.** Brandon: "we should be careful about how we present things though, because nobody's going to
like to see a negative score on anything". Lean: others see only what you are recognized for ("Trusted in Fresno
Neighbors", "Called it early x4"); no recognition shows nothing (like "New"). Negatives exist, lower hidden weight, show
only to you with the reason and an appeal, and fade. Open: whether Round 3's plain fading note on a post a jury found
misleading stays as the one public exception.

**Thumbs measure agreement, not credibility.** Brandon: "thumbs might indicate general opinion agreeability". Lean:
agreement belongs to the thing (counts, tint); a person's "how often you go with the majority" could exist only privately;
agreeing with the crowd never earns standing (it would reward conformity).

**Right against the crowd (Asch).** Brandon: "some kind of measurement for people who go against the grain and are later
vindicated", citing the conformity experiment. Lean: record each call with its time and how many agreed then; when it
settles against something independent (a stated outcome and date, a verified fact, a jury overturned on appeal; never
"the crowd changed its mind"), reward with a standard proper scoring rule: more when few agreed, near zero when most did;
the mirror cost when wrong (private). Start narrow: only things that settle on their own (proposals may state a checkable
outcome and a date), a minimum of ~10 settled calls before anything shows, a cap per call. Shown as "Called it early: N".

**React before you see the room.** Brandon: "maybe you cant see comments or replies until you thumbs up down or neutral
unless you opt out in which case theyre all neutral, which is a metric of its own". Claude's lean: hide the room's
OPINION (counts, tint, others' reactions) until you react, not the content (forced clicks to read produce noise); or hide
comments only for a short window on fresh posts. Neutral is a real choice. Opting out counts as "did not weigh in", not
as neutral. Long-quiet accounts: most people lurk, so never shown as suspicious; they weigh little until they have
recent activity, and a burst from long-dormant accounts on one target is flagged for review. Brandon: "i like the recent
account addition".

**Curves that throttle fairly** (Brandon: "reasonable graph curves that throttle and carbeurate things fairly"). Starting
shapes, each one knob in one settings file: weight ramps with active days in the last 90 (half weight at ~14 days, a
floor above zero); the k-th mark from one person in a period counts p^k (p ~ 0.5, so one person adds up to at most ~2);
a linked group of n counts n^a independent voices (a ~ 0.5); the per-call crowd reward is capped (~3); every mark fades
(a half-life of months). A person's weight is the product, never below the floor of Round 1 point 7.

**Effort: Stewardship** (Brandon: "something that measures peoples effort or like hard workingness. like people that
design proposals or do a certain thing to help maintain the site, like admins"; Grok consulted 2026-11-21,
scratch/2026-11-21-claude-effort-credibility-2/grok-answer.txt; Claude and Grok agree on the shape):
- A unit counts only when the work is FINISHED and someone other than you relied on it: a proposal that reached a decision
  (a capped smaller unit when it loses, so people do not only propose safe winners); a report closed and confirmed by the
  reporter or a jury; a fix the site can recheck still holds after a wait. You never confirm your own act. Posts, hours
  online, drafts and edits pay nothing.
- The Round 5 curves apply (repeats halve, linked confirmers collapse, weekly cap, fade); it saturates, no streaks, no
  public rank, so grinding and burnout do not pay.
- It feeds Contribution (shown on tap as "4 proposals decided, 3 fixes held"), not its own public number; it unlocks the
  tools that ARE the work (starting proposals, blank pages, moderation, maintenance), never reach or vote weight.
- Admin is a tool you qualify for (Stewardship floor plus decent Judgment and Conduct), not a title: no points for
  holding it, every action logged, appeals and a random sample reviewed by non-admin juries, removable.
- Open (Claude): witnessed work favors visible work; quiet help (welcoming newcomers) has no natural witness. Grok: make
  every chore the site can see loggable, leave off-site and purely social help unscored, allow a small capped thanks.

