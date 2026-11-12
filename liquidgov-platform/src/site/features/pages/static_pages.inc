// The mobile design reads the charter as a chaptered, one-screen-at-a-time
// flow ("Chapter 2 of 8 · 6 min", a "You can stop here" closer). The
// design's own 8-title outline had no body text behind it -- the real
// source turned out to be the actual AVND charter document Brandon
// pointed to directly, which already splits into 14 real, numbered
// sections. Used those 14 as-is rather than forcing the design's looser
// 8-title grouping onto text that doesn't actually divide that way.
//
// The charter describes the full Phase 2 civic mechanism (blockchain
// voting, land ledger, sortition courts, donation-based pay) that this
// platform does not implement and isn't claiming to -- but the document
// already says so itself (Section 13: "This initial adoption period
// constitutes Phase 1... Phase 2 does not begin automatically"), which
// is exactly this site's own existing posture elsewhere ("Real
// governmental authority is a later, explicit landslide -- Phase 2").
// Ported the substantive 14 sections verbatim; cut only the document's
// own closing meta paragraphs (a call for a co-builder, a P.S. email
// solicitation) since those are about writing the document, not part of
// its civic content.
struct LawChapter { std::string title; int minutes; std::string body; };

const std::vector<LawChapter>& law_chapters() {
    static const std::vector<LawChapter> chapters = {
        {"The Problem", 2,
         "<p>Every governance system in human history has faced the same fundamental vulnerability: power without consequence.</p>"
         "<p>When those who govern are insulated from the direct results of their decisions, three things reliably follow. "
         "Corruption takes root, because concentrated power attracts those seeking to exploit it. Incentives misalign, because "
         "officials become focused on keeping their position rather than serving the people who granted it. And accountability "
         "disappears, because the feedback loop between citizen dissatisfaction and official behavior is too slow, too indirect, "
         "and too easy to manipulate.</p>"
         "<p>These aren't &ldquo;people problems.&rdquo; They're structural problems. Good people placed inside broken structures "
         "produce broken outcomes. Broken people placed inside well-designed structures are naturally corrected by them.</p>"
         "<p>Representative democracy was designed for a world without the technology to allow direct citizen participation at "
         "scale. That world no longer exists. Yet we keep operating systems built on the assumption that citizens can't govern "
         "themselves — that they need professional intermediaries to translate their will into policy. Those intermediaries have, "
         "predictably, accumulated power, shielded themselves from consequence, and optimized for their own survival instead of "
         "the people they were meant to serve.</p>"
         "<p>Complex legal systems compound the problem. Law that requires professional interpretation to navigate inevitably "
         "benefits whoever can afford that navigation, while everyone else pays the toll. Law becomes a tool of consolidation "
         "instead of protection.</p>"
         "<p>This framework proposes something different — not a reform of existing systems, but a reconstruction from the ground "
         "up, built on natural law, voluntary participation, radical transparency, and real-time accountability. A system where "
         "everyone in it, from the newest citizen to the highest official, reaps exactly what they sow.</p>"},
        {"Foundational Principles", 2,
         "<p>A Voluntary Natural Democracy isn't built on ideology. It's built on observation. Ask one question — what kind of "
         "structure produces the best outcomes for the most people over the longest time? — and these principles are the answer.</p>"
         "<p><strong>Voluntarism.</strong> No participation is coerced. Citizens opt in. Officials serve because citizens choose "
         "to support them. Laws beyond the three universal laws are recommendations, not mandates. A government that has to earn "
         "its legitimacy every day is a government that can't afford to stop serving its people.</p>"
         "<p><strong>Anti-Authoritarianism.</strong> Authority here is always elected, temporary, conditional, and reversible. No "
         "official holds power that can't be withdrawn in real time. No law exists that can't be revised.</p>"
         "<p><strong>Radical Transparency.</strong> Every vote, donation, land transaction, and official decision is publicly "
         "documented on the national blockchain. Corruption requires darkness. Accountability requires light.</p>"
         "<p><strong>Natural Consequence.</strong> You reap what you sow. This runs through the whole system — the universal "
         "laws, the donation-based pay system, the land system, the judicial outcome record.</p>"
         "<p><strong>Diversity and Flexibility.</strong> Monoculture is toxic to any living system. This framework doesn't "
         "prescribe one way of living. It prescribes a structure within which communities determine those things for themselves, "
         "evolve, and correct their own mistakes.</p>"},
        {"The Three Universal Laws", 2,
         "<p>Most legal systems fail the same way governance systems fail — they accumulate complexity, and complexity "
         "concentrates power in the hands of whoever can navigate it.</p>"
         "<p>A Voluntary Natural Democracy recognizes three universal laws. They apply to every person on national territory, "
         "citizen or not, regardless of wealth or position. No one is above them, and no one is beneath their protection:</p>"
         "<p><strong>Do not steal. Do not damage. Do not deprive of liberty.</strong></p>"
         "<p>Consequence, if a court ever sits, is proportional and judicial &mdash; a ceiling, not a license for private revenge:</p>"
         "<p>If you steal, something of yours may be stolen.<br>If you give damage, you may take damage.<br>"
         "If you strip someone of their liberty, similarly, you may find your liberty being stripped from you.</p>"
         "<p><strong>&ldquo;May&rdquo; is deliberate.</strong> These laws set a ceiling on proportional response, not a mandatory "
         "sentence. A judge isn't required to impose the maximum — they're empowered to judge gracefully and creatively within "
         "the range the law allows.</p>"
         "<p><strong>Consequence is judicial, not personal.</strong> The universal laws describe what a court may impose — they "
         "are not a license for a wronged party to take matters into their own hands. Private retaliation is a new violation of "
         "the law, not an exercise of it.</p>"
         "<p><strong>The laws are self-referential.</strong> A judge who systematically strips liberty from any particular group "
         "is themselves in violation of the third law. The enforcers are bound by them as tightly as anyone they judge.</p>"
         "<p><strong>Simplicity as equality.</strong> Three laws a child can memorize are three laws any citizen can invoke "
         "without a lawyer. That simplicity is itself a form of justice.</p>"},
        {"Identity", 2,
         "<p>Every mechanism in this framework — voting, land, courts, donations — depends on one thing working correctly: "
         "knowing that each citizen is one real, unique person. Get this wrong and every other safeguard is decoration.</p>"
         "<p><strong>No national ID authority.</strong> A single issuing body is a single point of capture. Instead, identity "
         "verification is a private, competitive market, governed by the same anti-monopolism rules as any other service.</p>"
         "<p><strong>How it works.</strong> Independent verifiers confirm a registrant is a unique living person, typically "
         "through a biometric check, and bind that check to the citizen's blockchain key. The biometric itself never touches "
         "the chain — only a signed, hashed proof of &ldquo;this key belongs to one unique person&rdquo; does.</p>"
         "<p><strong>Fraud is expensive, not free.</strong> A verifier that knowingly certifies a fake or duplicate identity is "
         "enabling theft of voting power and is prosecutable under the first universal law.</p>"
         "<p><strong>Non-citizens have standing too.</strong> Legal identity and citizenship are separate things. Anyone present "
         "on the territory enrolls with a verifier to have legal standing in court, since the three universal laws bind everyone "
         "regardless of status. Citizenship — voting rights, land entitlement, point accumulation — is a separate, later opt-in "
         "layered on top.</p>"},
        {"Governance Structure", 3,
         "<p>Three branches, each with a distinct function, each checking the others, all of them accountable to the citizenry "
         "in real time. There is no legislative branch — the citizenry performs that function directly through the blockchain "
         "platform.</p>"
         "<p><strong>The Executive Branch</strong> administers the nation, enforces the universal laws, maintains the military, "
         "and implements the outcomes of citizen votes. It's headed by a president and vice president serving as a unified pair. "
         "No official draws a fixed salary — every official and service member earns exactly what citizens choose to donate to "
         "them individually.</p>"
         "<p><strong>The Officials' Contract.</strong> Before assuming office, every official signs a lawful, binding guarantee "
         "of honesty for the duration of their term, with no exception clause. If the guarantee is broken, consequence follows, "
         "determined by citizen vote.</p>"
         "<p><strong>The Judicial Branch</strong> interprets and applies the universal laws at local and national levels. Local "
         "judges handle day-to-day cases with wide creative discretion. Higher review for cases involving officials or systemic "
         "violations is handled by <strong>sortition panels</strong> — citizens randomly selected for a single case, the way a "
         "jury is called today. No permanent seat exists to bribe or pressure, because no panel outlives its case.</p>"
         "<p><strong>The Civic Branch</strong> is structurally independent from the other two. Its sole job is to maintain, "
         "communicate, and honestly document the reasoning and historical record of the nation. It holds no enforcement power "
         "and controls no resources beyond its own donation-based operating funds. It doesn't propagandize. It documents.</p>"},
        {"The Citizen Legislature and Vote Points", 2,
         "<p>There's no professional legislative class here. Citizens propose, debate, and vote on policy directly through the "
         "blockchain. Any citizen can submit a proposal; every proposal is public; the most supported proposals rise through "
         "engagement, not lobbying: citizen proposes &rarr; blockchain vote &rarr; executive implements &rarr; judiciary reviews "
         "for universal-law compliance.</p>"
         "<p><strong>Vote points.</strong> Every citizen earns one vote point per complete year of citizenship, regardless of "
         "age or background — a point total is tied to a living person and disappears at death. It can't be willed, sold, or "
         "inherited.</p>"
         "<p>Points can be applied in full to one candidate or proposal, split across several, or delegated to a representative "
         "who can use or further delegate them, and withdrawn or reassigned at any time.</p>"
         "<p><strong>Ballots are secret; delegation is not a loophole for coercion.</strong> A citizen's own vote is encrypted — "
         "they can verify it themselves but can't be compelled to prove it to anyone else. Who delegated to whom is not "
         "individually traceable, only the delegate's total power is public. No delegate may hold more than a fixed ceiling of "
         "total national vote points, and delegation chains are one-directional — no citizen can delegate back into a loop that "
         "returns power to themselves.</p>"
         "<p>There's no minimum voting age and no criminal disqualification. The only requirement is that a citizen is capable "
         "of truly and safely casting their own vote.</p>"},
        {"Succession, and the National Blockchain Platform", 1,
         "<p><strong>Succession.</strong> The president and vice president serve as a pair. If the president loses majority "
         "support, the office passes immediately to the new majority leader and their partner. For all other positions, "
         "succession moves to the next role in the organizational hierarchy in a caretaker capacity, restricted from major "
         "unilateral decisions until a new official earns majority approval.</p>"
         "<p><strong>The platform.</strong> The blockchain is the voting system, the public record, the donation mechanism, the "
         "idea registry, the judicial outcome database, the civic communication channel, and the land ledger, all at once. Every "
         "citizen's profile documents their vote history, idea record, land holdings, donations given and received, and public "
         "civic contributions — accessible continuously, because accountability doesn't keep business hours.</p>"},
        {"The Land System", 4,
         "<p>Land is the foundation of independence. A person with secure access to arable land can feed and shelter themselves "
         "without depending on an employer, landlord, or government. This framework treats that as a design constraint, not an "
         "afterthought.</p>"
         "<p>Total land divides into three pools: <strong>citizen-allocable land</strong> (the pool the per-citizen cap is "
         "calculated against), <strong>reserved nature land</strong> (held in permanent public trust, never transactable by "
         "anyone, including the government), and <strong>infrastructure land</strong> (what actual government function needs). "
         "The reserved and infrastructure pools may only grow from land that is unclaimed or voluntarily dedicated, never from "
         "land already held under an individual's bond.</p>"
         "<p><strong>The cap.</strong> Every citizen is entitled to an allotment equal to the citizen-allocable pool divided by "
         "the population, measured in total assessed property bond value — not raw acreage. No individual may hold above the "
         "cap regardless of wealth or status. Within it, land is freely divided, combined, bought, sold, or gifted.</p>"
         "<p><strong>Bonds and auctions.</strong> Every parcel is held with a bond equal to its assessed value, reassessed "
         "periodically using recent blockchain transaction history. When a parcel sells, it's listed at bond value and "
         "auctioned publicly.</p>"
         "<p><strong>Inheritance and gifting.</strong> Property passing to an heir still goes through public auction — the "
         "recipient must meet bond value or match the highest bid. <strong>Exception:</strong> an heir or long-term occupant of "
         "a primary residence has right of first refusal at bond value.</p>"
         "<p><strong>No corporate land ownership.</strong> Businesses lease from individual citizen owners at market rate. "
         "Aggregate leasehold size per business is tracked and published on the blockchain.</p>"
         "<p><strong>Community infrastructure.</strong> Roads, parks, and shared spaces are built through voluntary dedication "
         "— citizens allocate part of their personal allotment to shared use, publicly recorded.</p>"
         "<p><strong>Transition at adoption.</strong> Existing owners keep their current holdings; nothing is seized. Owners "
         "over the new cap divest the excess through the standard auction process within a transition window set by citizen "
         "vote.</p>"},
        {"The Economic Framework", 3,
         "<p>A Voluntary Natural Democracy is a free market. Nearly all goods and services come from private individuals and "
         "businesses competing openly. The framework's job is to keep the conditions for genuine competition structurally "
         "intact, not to run the economy itself.</p>"
         "<p><strong>Privatization.</strong> Everything beyond the executive office and military is privately owned and "
         "operated. Citizens fund the two institutions that can't reasonably be privatized — governance and defense — through "
         "voluntary donation.</p>"
         "<p><strong>Anti-monopolism.</strong> A monopoly isn't the product of a free market; it's the product of a market "
         "that's stopped being free. Full transparency of revenue, market share, and pricing on the blockchain lets citizens "
         "and competitors see concentration developing in real time; judicial review under the third universal law applies "
         "when control of a necessity crosses from inconvenient into a form of leverage over people's lives.</p>"
         "<p><strong>Donation-based compensation.</strong> Officials and service members earn only what citizens choose to "
         "donate them individually, fully visible on the blockchain. Reputational weight for officials rewards "
         "<strong>breadth of support over raw donation size</strong>, so an official funded by ten thousand ordinary citizens "
         "outweighs one funded by ten wealthy ones giving the same total.</p>"
         "<p><strong>Idea credit.</strong> Every adopted proposal is permanently credited to its originator, building a public "
         "record of civic contribution that naturally surfaces capable people for public roles.</p>"},
        {"Cultural Infrastructure", 2,
         "<p>Structure alone doesn't sustain a nation — culture does. A well-designed system inside a culture of apathy gets "
         "hollowed out over time. A well-designed system inside a culture of voluntary participation and honest accountability "
         "strengthens with every generation.</p>"
         "<p>The foundational values — voluntarism and anti-authoritarianism — aren't slogans. They're practical orientations "
         "toward power that have to be actively taught, not assumed. That teaching is the civic branch's entire job.</p>"
         "<p>The transition from Phase 1 into Phase 2 is, itself, ultimately a cultural shift more than a structural one — and "
         "when the time comes, it should be pushed to happen quickly and gracefully, or we risk permanent mission failure.</p>"
         "<p>Accountability alone isn't the whole picture. Once Phase 1 closes and Phase 2 begins, this culture has to actively "
         "cultivate an environment in which officials are genuinely well-regarded, not merely tolerated or presumed suspect.</p>"
         "<p>Diversity of community norms isn't a flaw here — it's how the system learns. Different communities try different "
         "approaches within the universal-law floor, generate real outcome data, and that data is visible to every other "
         "community deciding what to try next.</p>"},
        {"Population and Immigration", 2,
         "<p>Open citizenship is a core value here, but unmanaged growth has a real cost: since land entitlement is calculated "
         "per capita, every new citizen dilutes existing citizens' allotment. The fix can't be a gatekeeper deciding who's "
         "worthy of entry — that recreates exactly the kind of unaccountable authority this system exists to prevent.</p>"
         "<p><strong>The rate is set democratically, not administratively.</strong> Immigration quota is a standing proposal "
         "in the citizen legislature, voted on and re-votable through the same blockchain pipeline as any other policy.</p>"
         "<p><strong>The cap is equal for everyone, and restraint is cultural, not mandated.</strong> Every citizen is entitled "
         "to the same per-capita cap. Whatever portion of a citizen's cap they choose not to claim — their "
         "<strong>personal safety buffer</strong> — is pooled back into the citizen-allocable land, forming the practical "
         "reserve new citizens draw their entitlement from.</p>"
         "<p>There's also a natural brake built into the system for free: as population grows, per-capita land shrinks, bond "
         "values rise, and leasing gets more expensive — a price signal visible to prospective migrants before they ever "
         "arrive.</p>"},
        {"Emergency Provisions", 1,
         "<p>Direct citizen legislation is a strength in ordinary governance and a liability in a genuine crisis — a natural "
         "disaster or an active attack doesn't wait for a blockchain vote to resolve.</p>"
         "<p>To close that gap without creating a permanent emergency-powers backdoor, the executive may be granted "
         "<strong>narrow, time-boxed authority</strong> — but only under objective trigger conditions defined in advance by "
         "citizen vote, never self-declared by the executive in the moment. Emergency authority automatically sunsets after a "
         "fixed window unless renewed by a fresh citizen vote, and every action taken under it is fully logged and subject to "
         "post-hoc judicial review. An emergency clause that lets the executive decide when the emergency exists isn't an "
         "emergency clause — it's a coup provision. This one is built so it can't become that.</p>"},
        {"Initial Adoption Votes", 2,
         "<p>This framework defines the structure. It deliberately does not pre-decide every parameter a real community needs "
         "to run on — those are values a community sets for itself, through the same citizen-vote pipeline that governs "
         "everything afterward.</p>"
         "<p>This initial adoption period constitutes <strong>Phase 1</strong>. <strong>Phase 2</strong> does not begin "
         "automatically or by informal consensus — it requires a formal citizen vote demonstrating landslide support, defined "
         "as support exceeding 50% of the eligible citizen population, confirming the framework and its Phase 1 parameters as "
         "settled, ongoing practice. Until that vote passes, the nation continues operating under Phase 1, however long that "
         "takes.</p>"
         "<p>Items voted at adoption span land pool sizes and reassessment intervals, immigration quota and verifier "
         "accreditation standards, which existing government functions map to which branch, the specific trigger conditions "
         "and sunset window for emergency authority, and the selection method for sortition-based higher review panels. "
         "Citizens vote on each with full visibility into the current default, who it affects, and how reversible it is — "
         "informed consent to the rules they're about to live under.</p>"},
        {"A Living Document", 1,
         "<p>This framework is a living recommendation, not a monument. It's a starting point, not a final word. Every element "
         "of it is open to revision, improvement, and democratic evolution by the people who choose to live inside it. Good "
         "ideas that improve it should be proposed, credited to whoever proposed them, debated openly, and adopted if they "
         "earn majority support.</p>"
         "<p>The only elements not subject to revision are the three universal laws. They're the constitutional floor beneath "
         "which no majority may reach — because a majority that can vote away the prohibition on theft, damage, and liberty "
         "violation is a majority that can vote itself into tyranny. Everything else is negotiable. Everything else can be "
         "improved. That's the point.</p>"
         "<p>A Voluntary Natural Democracy isn't a destination. It's a direction — toward a society where power is always "
         "accountable, participation is always meaningful, the land beneath your feet is genuinely yours, and the most "
         "reliable path to personal prosperity is making the people around you more prosperous too.</p>"
         "<p>The rest is up to you!</p>"},
    };
    return chapters;
}

std::string laws_body() {
    const auto& chapters = law_chapters();
    std::string list;
    for (size_t i = 0; i < chapters.size(); ++i) {
        list += "<a class=\"roomrow\" href=\"/laws/" + std::to_string(i+1) + "\">"
                "<span class=\"roomini\">" + std::to_string(i+1) + "</span>"
                "<span class=\"roomname\">" + html_escape(chapters[i].title) + "</span>"
                "<span class=\"roomlive\">~" + std::to_string(chapters[i].minutes) +
                (chapters[i].minutes==1?" min":" min") + "</span></a>";
    }
    return "<h1>A Voluntary Natural Democracy</h1>"
           "<p class=\"lede\">The three universal laws are the floor — the one part of this no majority can vote away. "
           "Everything past that is proposed structure, not something this platform runs today. Read as much or as little "
           "as you want.</p>"
           "<div class=\"card\"><h2>Chapters</h2>" + list + "</div>"
           "<p class=\"muted\"><a href=\"/control\">Who holds control of this site</a> · <a href=\"/\">Genesis disclosure</a></p>";
}

// "Listen" on a Laws chapter (Brandon 2026-11-09: "i want the audio"): the chapter's recorded narration
// (/assets/chapter<n>.mp3). Shown only when the file is there. Unlike the Genesis button, a failed play (missing file,
// iPhone refusing, network) resets the button and says so instead of showing "playing" over silence.
std::string chapter_narration_html(size_t n) {
    const std::string num = std::to_string(n);
    std::ifstream probe("assets/chapter" + num + ".mp3", std::ios::binary);
    if (!probe) return "";
    return "<p class=\"row narr\"><button type=\"button\" class=\"btn ghost\" id=\"narrbtn\" aria-pressed=\"false\">"
           "&#9654; Listen to this chapter</button>"
           "<span class=\"muted\" id=\"narrmsg\" role=\"status\"></span></p>"
           "<audio id=\"narraudio\" preload=\"none\" src=\"/assets/chapter" + num + ".mp3\"></audio>"
           "<script>(function(){"
           "var b=document.getElementById('narrbtn'),a=document.getElementById('narraudio'),"
           "m=document.getElementById('narrmsg');if(!b||!a)return;"
           "function idle(){b.innerHTML='&#9654; Listen to this chapter';b.setAttribute('aria-pressed','false');}"
           "function fail(){idle();m.textContent='The narration could not play. Try again in a moment.';}"
           "a.addEventListener('ended',idle);a.addEventListener('error',fail);"
           "b.addEventListener('click',function(){m.textContent='';"
           "if(a.paused){var p=a.play();b.innerHTML='&#10074;&#10074; Pause';b.setAttribute('aria-pressed','true');"
           "if(p&&p.catch)p.catch(fail);}else{a.pause();idle();}});"
           "})();</script>";
}

std::string laws_chapter_body(size_t n) {
    const auto& chapters = law_chapters();
    const auto& ch = chapters[n-1];
    std::string nav = "<p class=\"row\"><a class=\"btn ghost\" href=\"/laws\">&larr; Back to overview</a>";
    if (n < chapters.size())
        nav += " <a class=\"btn\" href=\"/laws/" + std::to_string(n+1) + "\">Next: " +
               html_escape(chapters[n].title) + " &rarr;</a>";
    nav += "</p>";
    return "<p class=\"eyebrow\">Chapter " + std::to_string(n) + " of " + std::to_string(chapters.size()) + "</p>"
           "<h1>" + html_escape(ch.title) + "</h1>" + chapter_narration_html(n) +
           "<div class=\"card\">" + ch.body + "</div>" +
           (n == chapters.size()
             ? "<p class=\"muted\">You can stop here. Most people do, and that's fine — the rest is mechanism, "
               "and most of that is still a citizen vote.</p>"
             : "") +
           nav;
}

std::string control_body() {
    return "<h1>Who holds control</h1>"
           "<p class=\"lede\">Genesis operators run the machines and say so in public.</p>"
           "<div class=\"card\"><p><strong>Operator of this copy:</strong> Brandon Bottin.</p>"
           "<p><strong>As of:</strong> September 2026.</p>"
           "<p>That name is a disclosure of who can change this software and this store today. "
           "It is not an office. It is not a claim that the association already governs anyone.</p>"
           "<p class=\"warn\">Control of the website is meant to end when a working election exists among enrolled citizens. "
           "Governmental authority is a later landslide, not this page.</p>"
           "<p class=\"muted\">This is a Beta build of the association’s door — some of what's described here is still being built.</p></div>"
           "<div class=\"card\"><p class=\"eyebrow\">Who runs this</p>"
           "<table class=\"record\">"
           "<tr><td>This site</td><td>Held by one person. Temporary, and replaced the moment there is anything to replace it with.</td></tr>"
           "<tr><td>Your account</td><td>A platform account. Not a citizen identity, not a vote, not standing.</td></tr>"
           "<tr><td>Elections</td><td>None held. Real authority would come later, from a working election.</td></tr>"
           "<tr><td>Your data</td><td>The phone number itself is not kept &mdash; only a fingerprint, so a deleted account cannot immediately reopen under the same number. "
           "Sign in to <a href=\"/login?next=/export\">export</a> or delete from Settings.</td></tr>"
           "</table></div>"
           "<div class=\"card\"><p class=\"eyebrow\">Voting</p><h3>Nothing is awaiting your vote.</h3>"
           "<p class=\"muted\">No election has been held. This exists so the mechanism is testable, not "
           "because anyone here holds a seat.</p>"
           "<p class=\"muted\">Your account is a platform account. It is not a citizen identity and it "
           "casts no vote.</p>"
           "<p><a href=\"/login?next=/proposals\">Proposals</a> are an unofficial straw poll any member can open or vote on, "
           "explicitly not a binding vote. Sign in to see them.</p></div>";
}

// Honest, short, and already true of the live site. Not a lawyer's privacy
// policy — the same facts /control already names, on their own page so
// a shared link and a footer have somewhere to land. /about is the
// operator disclosure; /terms is how the association works.
std::string privacy_body() {
    return "<h1>What we keep</h1>"
           "<p class=\"lede\">Not a government. A platform account is not a citizen identity.</p>"
           "<div class=\"card\"><h2>Sign-in</h2>"
           "<p>Username and password. A phone is bound at signup so a deleted account cannot immediately reopen under the same number. "
           "The number itself is not kept &mdash; only a fingerprint.</p>"
           "<p>A session cookie (<code>lg_session</code>) keeps you signed in.</p></div>"
           "<div class=\"card\"><h2>What you put here</h2>"
           "<p>Posts, mail, and drawings stay until you delete them, or until an operator removes spam. "
           "Messages on this copy are stored so the operator can read them. " // Brandon 2026-11-11 decision 5
           "Friends are mutual accept. A block hides you from that person on civic pages.</p>"
           "<p>Sign in to <a href=\"/login?next=/export\">export</a> or delete from <a href=\"/login?next=/settings\">Settings</a>.</p></div>"
           "<p class=\"muted\">The operator of this copy is named on <a href=\"/control\">who holds control</a>.</p>";
}

std::string terms_body() {
    return "<h1>How this association works</h1>"
           "<p class=\"lede\">LiquidGov.US is a voluntary association. Three rules hold: do not steal, do not damage, do not deprive of liberty.</p>"
           "<div class=\"card\">"
           "<p>Nothing here is a government. Nothing here binds you unless you chose it. "
           "Proposals are an unofficial straw poll, not a binding vote. No election has been held.</p>"
           "<p>Your account is a platform account, not a citizen identity, not a vote, and not standing.</p>"
           "<p>The operator can change this software and this store today. That is disclosed on "
           "<a href=\"/control\">who holds control</a>. It is not an office.</p>"
           "</div>"
           "<p class=\"muted\"><a href=\"/laws\">The three laws</a> · <a href=\"/privacy\">What we keep</a></p>";
}

std::string install_body() {
    return "<h1>Install this as an app</h1>"
           "<p class=\"lede\">Not a download from a store &mdash; your browser turning this site into its "
           "own window, with its own icon, and no address bar in the way.</p>"
           "<div class=\"card\"><h3>iPhone or iPad (Safari)</h3>"
           "<table class=\"record\">"
           "<tr><td>1</td><td>Open liquidgov.us in Safari or Brave (Chrome on iPhone cannot add to Home Screen).</td></tr>"
           "<tr><td>2</td><td>Tap the Share icon (a square with an arrow pointing up), in the bottom toolbar.</td></tr>"
           "<tr><td>3</td><td>Scroll down and tap <strong>Add to Home Screen</strong>.</td></tr>"
           "<tr><td>4</td><td>Tap <strong>Add</strong>. The icon now opens LiquidGov.US on its own, no browser chrome.</td></tr>"
           "</table></div>"
           "<div class=\"card\"><h3>Android (Chrome)</h3>"
           "<table class=\"record\">"
           "<tr><td>1</td><td>Open liquidgov.us in Chrome.</td></tr>"
           "<tr><td>2</td><td>Tap the &vellip; menu in the top-right corner.</td></tr>"
           "<tr><td>3</td><td>Tap <strong>Install app</strong> (some versions show it as \"Add to Home screen\").</td></tr>"
           "<tr><td>4</td><td>Confirm. Chrome may also offer this itself as a banner or an install icon in the "
           "address bar &mdash; either one does the same thing.</td></tr>"
           "</table></div>"
           "<div class=\"card\"><h3>Windows, Mac, or Linux (Chrome or Edge)</h3>"
           "<table class=\"record\">"
           "<tr><td>1</td><td>Open liquidgov.us in Chrome or Edge.</td></tr>"
           "<tr><td>2</td><td>Look for an install icon in the address bar (a small monitor with a down arrow, "
           "or a &oplus;) &mdash; or open the &vellip; menu and look for <strong>Install LiquidGov.US&hellip;</strong>.</td></tr>"
           "<tr><td>3</td><td>Click <strong>Install</strong>. It opens in its own window from now on, and shows up "
           "in your Start menu, Dock, or applications list like anything else installed.</td></tr>"
           "</table></div>"
           "<div class=\"card\"><p class=\"muted\">Any other browser: look for <strong>Add to Home Screen</strong> or "
           "<strong>Install</strong> in its share or menu button &mdash; the exact wording varies, but nearly every "
           "current mobile and desktop browser has some version of it. Nothing here needs installing to just use "
           "the site in a normal browser tab; this only changes how you open it.</p></div>";
}

