// A proportional Yes/No/Abstain bar plus one-click vote buttons, the
// current voter's own choice picked out with a filled button instead of a
// ghost one. Number / Date / Choice / Scale use the same card, with a
// median (or option counts) instead of the yes/no bar. viewer empty
// (logged out) renders the tally with no voting UI.
std::string fmt_ballot_num(double v) {
    std::ostringstream o;
    if (std::fabs(v - std::round(v)) < 1e-9) o << static_cast<long long>(std::round(v));
    else { o.setf(std::ios::fixed); o.precision(2); o << v; }
    return o.str();
}

// The one-line how-to on /proposals and /officials (Brief 7 item 2, the audit's wording).
std::string how_proposals_work_html() {
    return "<p class=\"muted\"><strong>How this works:</strong> Anyone with an account can start a question or add "
           "their answer. You can change your answer at any time. This is a straw poll, not an official election, and "
           "nothing is sent to the city.</p>";
}

// A card's own error (a refused answer or option) comes back as ?err=...#proposal-N, so the browser lands on the card,
// thousands of pixels below the flash at the top (Brief 7 item 5, measured: -3113 px on a phone). This moves that flash
// into the card, under its first line. Used by /proposals, /proposals?id= and /officials.
std::string card_flash_script(const std::map<std::string, std::string>& qs) {
    auto it = qs.find("err");
    if (it == qs.end() || (it->second != "vote" && it->second != "option" && it->second != "optfull")) return "";
    return "<script>(function(){var m=/^#proposal-[0-9]+$/.exec(location.hash);if(!m)return;"
           "var t=document.getElementById(location.hash.slice(1)),w=document.querySelector('p.warn');if(!t||!w)return;"
           "var a=t.classList.contains('card')?t.firstElementChild:t;if(!a)return;a.after(w);t.scrollIntoView();})();"
           "</script>";
}

std::string proposal_talk(const Proposal& p, const std::string& viewer, const std::string& next_in);

// "1 response" / "2 responses", then " · N skipped" when anyone skipped (Brief 7 item 4).
std::string responses_text(int n, int skipped) {
    std::string s = std::to_string(n) + (n == 1 ? " response" : " responses");
    if (skipped > 0) s += " · " + std::to_string(skipped) + " skipped";
    return s;
}

// A one-button answer form (Skip / no preference, None of these) posting a reserved value (kBallotSkip, kBallotNone).
std::string reserved_answer_form(const Proposal& p, const std::string& next_in, const char* value, const std::string& label,
                                 const std::optional<std::string>& mine) {
    const bool active = mine && *mine == value;
    return "<form class=\"inline\" method=\"POST\" action=\"/proposals/vote\">"
           "<input type=\"hidden\" name=\"id\" value=\""+std::to_string(p.id)+"\">"
           + next_in +
           "<input type=\"hidden\" name=\"value\" value=\""+std::string(value)+"\">"
           "<button class=\""+std::string(active ? "" : "ghost")+"\" type=\"submit\">"+label+
           (active ? " &#10003;" : "")+"</button></form>";
}

// The Choice answer: one button per option with its count, posting the option INDEX (payload_ok takes only an
// index). Shared by proposal_block and /officials, so a race and a Choice proposal vote and count the same way
// (Brief 7 item 1: /officials posted the name and every vote was dropped). noun: "choice" or "candidate".
std::string choice_ballot_body(const Proposal& p, const std::string& viewer, const std::string& next_in,
                               const std::string& noun) {
    auto t = g_store.value_tally_of(p.id);
    auto mine = viewer.empty() ? std::optional<std::string>{} : g_store.ballot_value_of(p.id, viewer);
    int total = t.n;
    std::string bar = "<div style=\"display:flex;height:8px;border-radius:4px;overflow:hidden;background:var(--rule);margin:.5rem 0\">";
    if (total > 0) {
        for (std::size_t i = 0; i < t.option_counts.size(); ++i) {
            if (t.option_counts[i])
                bar += "<div style=\"flex:"+std::to_string(t.option_counts[i])+";background:var(--accent)\"></div>";
        }
        if (t.none_of_these)
            bar += "<div style=\"flex:"+std::to_string(t.none_of_these)+";background:var(--stone)\"></div>";
    }
    bar += "</div>";
    std::string list = "<div class=\"row\" style=\"flex-wrap:wrap;gap:.4rem\">";
    for (std::size_t i = 0; i < p.ballot.options.size(); ++i) {
        int n = (i < t.option_counts.size()) ? t.option_counts[i] : 0;
        const bool active = mine && *mine == std::to_string(i);
        if (!viewer.empty()) {
            list += "<form class=\"inline\" method=\"POST\" action=\"/proposals/vote\">"
                    "<input type=\"hidden\" name=\"id\" value=\""+std::to_string(p.id)+"\">"
                    + next_in +
                    "<input type=\"hidden\" name=\"value\" value=\""+std::to_string(i)+"\">"
                    "<button class=\""+std::string(active ? "" : "ghost")+"\" type=\"submit\">"+
                    html_escape(p.ballot.options[i])+" · "+std::to_string(n)+
                    (active ? " &#10003;" : "")+"</button></form>";
        } else {
            list += "<span class=\"muted\">"+html_escape(p.ballot.options[i])+" · "+std::to_string(n)+"</span>";
        }
    }
    // Always offered (Brief 7 item 4): None of these (its own count) and Skip / no preference (counted as skipped).
    if (!viewer.empty()) {
        list += reserved_answer_form(p, next_in, kBallotNone, "None of these · "+std::to_string(t.none_of_these), mine)
              + reserved_answer_form(p, next_in, kBallotSkip, "Skip / no preference", mine);
    } else {
        list += "<span class=\"muted\">None of these · "+std::to_string(t.none_of_these)+"</span>";
    }
    list += "</div>";
    if (!viewer.empty() && p.ballot.open_choice) {
        bool already = false;
        for (const auto& a : p.ballot.option_authors) if (a == viewer) { already = true; break; }
        const bool can_add = (viewer == p.author) || !already;
        if (can_add) {
            list += "<form method=\"POST\" action=\"/proposals/option\" style=\"margin-top:.7rem\">"
                    "<input type=\"hidden\" name=\"id\" value=\""+std::to_string(p.id)+"\">"
                    + next_in +
                    "<label>Add a "+noun
                    + std::string(viewer == p.author ? "" : " <span class=\"muted\">(one per person)</span>")
                    + "</label>"
                    "<div class=\"row\"><input name=\"label\" maxlength=\"80\" required placeholder=\""+std::string(noun == "candidate" ? "A name" : "Your option")+"\">"
                    "<button type=\"submit\">Add</button></div></form>";
        } else {
            list += "<p class=\"muted\">You already added a "+noun+" on this one.</p>";
        }
    }
    return bar + "<p class=\"muted\">"+responses_text(total, t.skipped)+"</p>" + list;
}

std::string proposal_block(const Proposal& p, const std::string& viewer, const std::string& next = "") {
    const std::string back = safe_next_path(next);
    const std::string next_in = back.empty() ? std::string()
        : "<input type=\"hidden\" name=\"next\" value=\""+html_escape(back)+"\">";
    std::string kindEyebrow;
    switch (p.ballot.kind) {
        case BallotKind::YesNo: kindEyebrow = "Yes, no, or skip"; break;
        case BallotKind::Number: kindEyebrow = p.ballot.unit.empty() ? "Number" : html_escape(p.ballot.unit); break;
        case BallotKind::Date: kindEyebrow = "Date"; break;
        case BallotKind::Choice: kindEyebrow = p.ballot.open_choice ? "Pick one (people can add options)" : "Pick one"; break;
        case BallotKind::Scale: kindEyebrow = "Scale"; break;
    }
    std::string body;
    if (p.ballot.kind == BallotKind::YesNo) {
        auto t = g_store.tally_of(p.id);
        auto my_vote = viewer.empty() ? std::optional<ProposalVote>{} : g_store.proposal_vote_of(p.id, viewer);
        const int total = t.yes + t.no + t.abstain;
        std::string bar = "<div style=\"display:flex;height:8px;border-radius:4px;overflow:hidden;background:var(--rule);margin:.5rem 0\">";
        if (total > 0) {
            if (t.yes) bar += "<div style=\"flex:"+std::to_string(t.yes)+";background:var(--ok)\"></div>";
            if (t.no) bar += "<div style=\"flex:"+std::to_string(t.no)+";background:var(--danger)\"></div>";
            if (t.abstain) bar += "<div style=\"flex:"+std::to_string(t.abstain)+";background:var(--stone)\"></div>";
        }
        bar += "</div><p class=\"muted\">"+std::to_string(t.yes)+" yes · "+std::to_string(t.no)+" no · "+
               std::to_string(t.abstain)+" skipped · "+std::to_string(total)+" total</p>";
        std::string voteForm;
        if (!viewer.empty()) {
            auto btn = [&](ProposalVote v, const char* label) {
                const bool active = my_vote.has_value() && *my_vote == v;
                return "<form class=\"inline\" method=\"POST\" action=\"/proposals/vote\">"
                       "<input type=\"hidden\" name=\"id\" value=\""+std::to_string(p.id)+"\">"
                       + next_in +
                       "<input type=\"hidden\" name=\"choice\" value=\""+std::to_string(static_cast<int>(v))+"\">"
                       "<button class=\""+std::string(active ? "" : "ghost")+"\" type=\"submit\">"+label+
                       (active ? " &#10003;" : "")+"</button></form>";
            };
            voteForm = "<div class=\"row\">"+btn(ProposalVote::Yes,"Yes")+btn(ProposalVote::No,"No")+
                       btn(ProposalVote::Abstain,"Skip")+"</div>"; // Skip is the stored Abstain (Brief 7 item 4)
        }
        body = bar + voteForm;
    } else if (p.ballot.kind == BallotKind::Choice) {
        body = choice_ballot_body(p, viewer, next_in, "choice");
    } else {
        auto t = g_store.value_tally_of(p.id);
        auto mine = viewer.empty() ? std::optional<std::string>{} : g_store.ballot_value_of(p.id, viewer);
        const bool skipped_mine = mine && *mine == kBallotSkip;
        const std::string skipped_note = t.skipped ? " "+std::to_string(t.skipped)+" skipped." : std::string();
        std::string summary;
        if (p.ballot.kind == BallotKind::Date) {
            if (t.n == 0) summary = "<p class=\"muted\">No dates yet."+skipped_note+"</p>";
            else summary = "<p>Median date: <strong>"+html_escape(t.median_date)+"</strong></p>"
                           "<p class=\"muted\">"+responses_text(t.n, t.skipped)+" · earliest "+html_escape(t.lo_date)+
                           " · latest "+html_escape(t.hi_date)+"</p>";
        } else {
            std::string unit = p.ballot.unit.empty() ? "" : " "+html_escape(p.ballot.unit);
            if (p.ballot.kind == BallotKind::Scale) {
                std::string left = (p.ballot.options.empty() || p.ballot.options[0].empty()) ? "0" : html_escape(p.ballot.options[0]);
                std::string right = (p.ballot.options.size() < 2 || p.ballot.options[1].empty()) ? "100" : html_escape(p.ballot.options[1]);
                if (t.n == 0) summary = "<p class=\"muted\">"+left+" — "+right+". No responses yet."+skipped_note+"</p>";
                else summary = "<p>"+left+" — <strong>"+fmt_ballot_num(t.median)+"</strong> — "+right+"</p>"
                               "<p class=\"muted\">median of "+responses_text(t.n, t.skipped)+" · range "+fmt_ballot_num(t.lo)+
                               "–"+fmt_ballot_num(t.hi)+"</p>";
            } else {
                if (t.n == 0) summary = "<p class=\"muted\">No numbers yet."+skipped_note+"</p>";
                else summary = "<p>Median: <strong>"+fmt_ballot_num(t.median)+unit+"</strong></p>"
                               "<p class=\"muted\">"+responses_text(t.n, t.skipped)+" · range "+fmt_ballot_num(t.lo)+
                               "–"+fmt_ballot_num(t.hi)+unit+"</p>";
            }
        }
        std::string voteForm;
        if (!viewer.empty()) {
            voteForm = "<form method=\"POST\" action=\"/proposals/vote\">"
                       "<input type=\"hidden\" name=\"id\" value=\""+std::to_string(p.id)+"\">"
                       + next_in;
            if (p.ballot.kind == BallotKind::Date) {
                voteForm += "<label>Your date</label><input type=\"date\" name=\"value\" required";
                if (mine && !skipped_mine) voteForm += " value=\""+html_escape(*mine)+"\"";
                voteForm += ">";
            } else if (p.ballot.kind == BallotKind::Scale) {
                // No pre-filled 50 (Brief 7 item 4): until the slider moves it has no name= (an old tab or a stray Save
                // sends nothing, so nothing is stored) and Save is disabled. Your saved mark, if any, is shown and sent.
                const bool has_mark = mine && !skipped_mine;
                const std::string sid = std::to_string(p.id);
                voteForm += "<label>Your mark <span class=\"muted\" id=\"scv"+sid+"\">"
                            + (has_mark ? html_escape(*mine) : std::string("not set yet: move the slider")) + "</span></label>"
                            "<input type=\"range\"" + (has_mark ? " name=\"value\" value=\""+html_escape(*mine)+"\"" : std::string())
                            + " min=\"0\" max=\"100\" oninput=\"this.name='value';this.form.querySelector('[data-save]').disabled=false;"
                              "document.getElementById('scv"+sid+"').textContent=this.value\">";
            } else {
                voteForm += "<label>Your number";
                if (!p.ballot.unit.empty()) voteForm += " ("+html_escape(p.ballot.unit)+")";
                voteForm += "</label><input type=\"number\" name=\"value\" required step=\"any\"";
                if (p.ballot.min_v != 0 || p.ballot.max_v != 0) {
                    voteForm += " min=\""+fmt_ballot_num(p.ballot.min_v)+"\" max=\""+fmt_ballot_num(p.ballot.max_v)+"\"";
                }
                if (mine && !skipped_mine) voteForm += " value=\""+html_escape(*mine)+"\"";
                voteForm += ">";
            }
            const bool scale_unset = p.ballot.kind == BallotKind::Scale && !(mine && !skipped_mine);
            voteForm += std::string("<button data-save") + (scale_unset ? " disabled" : "") + ">Save my answer</button></form>"
                      + reserved_answer_form(p, next_in, kBallotSkip, "Skip / no preference", mine);
        }
        body = summary + voteForm;
    }
    std::string when = fmt_time(p.ts);
    std::string place;
    if (!p.city.empty() || !p.who_acts.empty()) {
        place = "<p class=\"muted\">";
        if (!p.city.empty()) {
            std::string name = p.city;
            for (const auto& cu : g_store.cities_in_use()) if (cu.key == p.city) { name = cu.name; break; }
            place += "City: <a href=\"/city/"+url_encode(p.city)+"\">"+html_escape(name)+"</a>";
        }
        if (!p.who_acts.empty()) {
            if (!p.city.empty()) place += " · ";
            place += "Who could act: "+html_escape(p.who_acts);
        }
        place += "</p>";
        if (!p.tags.empty()) {
            place += "<p class=\"muted\">";
            bool first_tag = true;
            for (const auto& tag : p.tags) {
                if (!first_tag) place += " · ";
                first_tag = false;
                place += html_escape(tag);
            }
            place += "</p>";
        }
        if (!p.city.empty())
            place += "<p class=\"muted\">Discussion only. Nothing is sent to the city.</p>";
    }
    return "<p class=\"muted eyebrow\" id=\"proposal-"+std::to_string(p.id)+"\">Proposal "+std::to_string(p.id)+" · "+kindEyebrow+"</p>"
           "<h3 style=\"margin:.1rem 0\">"+html_escape(p.title)+"</h3>"
           "<p class=\"muted\">by <a href=\"/u/"+html_escape(p.author)+"\">"+html_escape(p.author)+"</a>"+
           (when.empty()?"":" · "+html_escape(when))+"</p>"
           + place +
           "<p style=\"white-space:pre-wrap\">"+html_escape(p.body)+"</p>"+body
           + proposal_talk(p, viewer, next_in);
}

// Comments and one reaction per person. A reaction is not a vote.
// Clicking the reaction you already have removes it.
std::string proposal_talk(const Proposal& p, const std::string& viewer, const std::string& next_in) {
    auto label_of = [](const std::string& kind) {
        if (kind.empty()) return kind;
        std::string s = kind;
        if (s[0] >= 'a' && s[0] <= 'z') s[0] = static_cast<char>(s[0] - 'a' + 'A');
        return s;
    };
    const auto counts = g_store.proposal_reaction_counts(p.id);
    auto mine = viewer.empty() ? std::optional<std::string>{} : g_store.proposal_reaction_of(p.id, viewer);
    std::string row = "<div class=\"row\" style=\"flex-wrap:wrap;gap:.4rem\">";
    for (const auto& item : counts) {
        const std::string& kind = item.first;
        const int n = item.second;
        const bool active = mine && *mine == kind;
        const std::string label = label_of(kind);
        if (!viewer.empty()) {
            row += "<form class=\"inline\" method=\"POST\" action=\"/proposals/react\">"
                   "<input type=\"hidden\" name=\"id\" value=\""+std::to_string(p.id)+"\">"
                   + next_in +
                   "<input type=\"hidden\" name=\"kind\" value=\""+html_escape(kind)+"\">"
                   "<button class=\""+std::string(active ? "" : "ghost")+"\" type=\"submit\">"+
                   html_escape(label)+" · "+std::to_string(n)+
                   (active ? " &#10003;" : "")+"</button></form>";
        } else if (n > 0) {
            row += "<span class=\"muted\">"+html_escape(label)+" · "+std::to_string(n)+"</span>";
        }
    }
    // Brief 7 item 3: the reactions go last, below the comments, in their own box with a rule line and smaller buttons
    // (.reactions in css()), the note first, so Support / Oppose do not read as a second way to answer.
    row = "<div class=\"reactions\"><p class=\"muted\">A reaction is not a vote.</p>" + row + "</div></div>";
    auto replies = g_store.proposal_replies_of(p.id);
    std::string list;
    for (const auto& r : replies) {
        list += "<div class=\"who\" style=\"margin:.5rem 0\">"+portrait_tag(r.author,"sm")+
                "<div><a href=\"/u/"+html_escape(r.author)+"\"><strong>"+html_escape(r.author)+
                "</strong></a> <span class=\"muted\">"+html_escape(fmt_time(r.ts))+"</span>"
                "<p style=\"margin:.2rem 0 0\">"+html_escape(r.text)+"</p></div></div>";
    }
    std::string replyForm;
    if (!viewer.empty()) {
        replyForm = "<form method=\"POST\" action=\"/proposals/reply\" style=\"margin-top:.3rem\">"
                    "<input type=\"hidden\" name=\"id\" value=\""+std::to_string(p.id)+"\">"
                    + next_in +
                    "<textarea name=\"text\" maxlength=\"1000\" placeholder=\"Write a comment\"></textarea>"
                    "<button class=\"ghost\" type=\"submit\">Comment</button></form>";
    }
    std::string repliesBlock;
    if (!replies.empty() || !viewer.empty()) {
        repliesBlock = "<details style=\"margin-top:.4rem\"><summary class=\"muted\" style=\"cursor:pointer\">"+
            std::to_string(replies.size())+(replies.size()==1?" comment":" comments")+"</summary>"
            "<div style=\"padding-left:.8rem;border-left:2px solid var(--rule);margin-top:.3rem\">"+
            list+replyForm+"</div></details>";
    }
    return repliesBlock + row;
}

