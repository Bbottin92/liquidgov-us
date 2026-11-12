#include "site/features/proposals/proposals_routes.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/core/http.hpp"
#include "site/core/multipart.hpp"
#include "site/core/names_categories.hpp"
#include "site/features/feed/composer.hpp"
#include "site/features/pains/pains.hpp"
#include "site/features/proposals/proposals.hpp"
#include "site/ui/wrap.hpp"

namespace liquidgov::site {

std::optional<std::string> proposals_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
    if(method=="GET"&&path=="/proposals") {
        if(!acc) return need_login();
        // /proposals?id=N: one proposal on its own (the map's right-hand field
        // opens this), with its vote, reactions and comments.
        if (qs.count("id")) {
            std::uint64_t pid = 0;
            try { pid = std::stoull(qs.at("id")); } catch (...) { pid = 0; }
            auto one = pid ? g_store.proposal_by_id(pid) : std::nullopt;
            if (!one) return http_code("404 Not Found", wrap("Not found", user, unread,
                "<div class=\"card\"><p>No such proposal.</p><p><a href=\"/proposals\">Proposals</a></p></div>"));
            // A race is shown and voted on /officials only (Brief 7 item 1: one object, one page).
            if (one->is_office) return http_redir("/officials#proposal-" + std::to_string(one->id));
            const std::string self = "/proposals?id=" + std::to_string(one->id);
            return pg(one->title, "<p class=\"muted\"><a href=\"/proposals\">All proposals</a></p>" + flash_error(qs) +
                      "<div class=\"card\">" + proposal_block(*one, user, self) + "</div>" + card_flash_script(qs));
        }
        std::string filter_key;
        if (qs.count("city")) {
            const std::string raw = qs.at("city");
            const std::string key = city_key(raw);
            if (!raw.empty() && key.empty()) return http_redir("/proposals");
            if (!key.empty() && key != raw) return http_redir("/proposals?city="+url_encode(key));
            filter_key = key;
        }
        std::string city_value;
        if (!filter_key.empty()) {
            std::string name = filter_key;
            for (const auto& cu : g_store.cities_in_use()) if (cu.key == filter_key) { name = cu.name; break; }
            city_value = " value=\""+html_escape(name)+"\"";
        }
        const std::string list_next = filter_key.empty() ? std::string("/proposals")
            : "/proposals?city="+url_encode(filter_key);
        std::string tag_fields;
        if (!filter_key.empty()) {
            for (const auto& tag : g_store.city_tags(filter_key)) {
                tag_fields += "<label class=\"row\" style=\"gap:.4rem;align-items:center\">"
                              "<input type=\"checkbox\" name=\"tag\" value=\"" + html_escape(tag) + "\"> "
                              + html_escape(tag) + "</label>";
            }
            if (!tag_fields.empty())
                tag_fields = "<label>Departments and councils</label>" + tag_fields;
        }
        std::string html = "<h1>Proposals</h1>"
            // Brief 7 item 2 (PROPOSALS-AUDIT section 4): how to use the page, in one line. The Phase 2 link to /laws/13 sent
            // members into charter text; it stays in Laws and on /control.
            + how_proposals_work_html();
        html += "<p class=\"muted\"><a href=\"/officials\">Races for office</a> are on their own page.</p>";
        if (!filter_key.empty()) {
            std::string name = filter_key;
            for (const auto& cu : g_store.cities_in_use()) if (cu.key == filter_key) { name = cu.name; break; }
            html += "<p class=\"muted\">Showing proposals for <a href=\"/city/"+url_encode(filter_key)+"\">"
                    +html_escape(name)+"</a>. <a href=\"/proposals\">All proposals</a></p>";
        }
        html += flash_error(qs);
        // ?title= prefills the title ("Start a proposal about this" from /problems). Never posts by itself.
        std::string title_prefill = qs.count("title") ? trim_ws(qs.at("title")) : std::string();
        if (title_prefill.size() > 140) title_prefill.resize(140);
        auto all = filter_key.empty() ? g_store.all_proposals() : g_store.proposals_in_city(filter_key);
        std::reverse(all.begin(), all.end());
        // Races for office are listed on /officials only, not twice (Brief 7 item 1); a link below says where they are.
        all.erase(std::remove_if(all.begin(), all.end(), [](const Proposal& x) { return x.is_office; }), all.end());
        // The proposals first, the form behind a "New proposal" button (sweep 2026-11-09: on a phone the page opened
        // on an empty form and the proposals began a full screen down). Open on its own when it is what you came for:
        // a ?title= prefill, an error to fix, nothing to read yet, or a link to #new (the script below).
        // Only a create error opens it; a refused answer or option (err=vote/option/optfull) is about a card below.
        const std::string err_code = qs.count("err") ? qs.at("err") : std::string();
        const bool create_err = err_code == "empty" || err_code == "range" || err_code == "options" ||
                                err_code == "dupe" || err_code == "pcity";
        const bool open_form = !title_prefill.empty() || create_err || all.empty();
        html += std::string("<details class=\"card newprop\" id=\"new\"") + (open_form ? " open" : "") +
                "><summary>New proposal</summary><form method=\"POST\" action=\"/proposals\">"
                "<label>Title</label><input name=\"title\" maxlength=\"140\" required value=\""+html_escape(title_prefill)+"\">"
                "<label>What's being proposed</label>"
                "<textarea name=\"body\" maxlength=\"4000\" required placeholder=\"Plain text.\"></textarea>"
                "<label>How should people answer?</label>"
                "<select name=\"kind\" id=\"bkind\">"
                "<option value=\"0\">Yes, no, or skip</option>"
                "<option value=\"1\">A number</option>"
                "<option value=\"2\">A date</option>"
                "<option value=\"3\">Pick one option</option>"
                "<option value=\"4\">A scale (0–100)</option>"
                "</select>"
                "<div id=\"bnum\" hidden>"
                "<label>Unit (optional)</label><input name=\"unit\" maxlength=\"24\" placeholder=\"days, dollars, people\">"
                "<label>Minimum (optional)</label><input name=\"min\" type=\"number\" step=\"any\">"
                "<label>Maximum (optional)</label><input name=\"max\" type=\"number\" step=\"any\">"
                "</div>"
                "<div id=\"bchoice\" hidden>"
                "<label>Options (one per line)</label>"
                "<textarea name=\"options\" maxlength=\"800\" placeholder=\"One option per line\"></textarea>"
                "<label class=\"row\" style=\"gap:.4rem;align-items:center;margin-top:.4rem\">"
                "<input type=\"checkbox\" name=\"open\" value=\"1\"> "
                "Let people add another option <span class=\"muted\">(one each, besides you)</span></label>"
                "</div>"
                "<div id=\"bscale\" hidden>"
                "<label>Left label</label><input name=\"left\" maxlength=\"40\" placeholder=\"Too little\">"
                "<label>Right label</label><input name=\"right\" maxlength=\"40\" placeholder=\"Too much\">"
                "</div>"
                "<label>City (optional)</label>"
                "<input name=\"city\" list=\"citylist\" maxlength=\"80\" autocomplete=\"off\" placeholder=\"Fresno, CA\""
                + city_value + ">"
                "<label>Who could act on this? <span class=\"muted\">(optional, for example &ldquo;The city council&rdquo;)</span></label>"
                "<input name=\"who\" maxlength=\"80\" placeholder=\"The city council\">"
                + tag_fields +
                "<button>Publish</button></form>"
                + city_datalist_html() +
                "<script>"
                "(function(){var s=document.getElementById('bkind');if(!s)return;"
                "function show(){var v=s.value;"
                "document.getElementById('bnum').hidden=(v!=='1');"
                "document.getElementById('bchoice').hidden=(v!=='3');"
                "document.getElementById('bscale').hidden=(v!=='4');}"
                "s.addEventListener('change',show);show();})();"
                "(function(){var d=document.getElementById('new');if(d&&location.hash==='#new')d.open=true;})();"
                "</script></details>";
        if (all.empty()) html += "<div class=\"card\"><p class=\"empty\">None open.</p>"
            "<p class=\"muted\">The form above starts an unofficial straw poll. Not a binding vote.</p></div>";
        for (const auto& p : all) {
            html += "<div class=\"card\">"+proposal_block(p, user, list_next)+"</div>";
        }
        html += card_flash_script(qs);
        return pg("Proposals", html);
    }
    if(method=="POST"&&path=="/proposals") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        auto title = trim_ws(f["title"]);
        auto ptext = trim_ws(f["body"]);
        if (title.empty() || ptext.empty()) return http_redir("/proposals?err=empty");
        BallotSpec spec;
        int kind = 0;
        try { kind = std::stoi(f["kind"]); } catch (...) { kind = 0; }
        if (kind < 0 || kind > 4) kind = 0;
        spec.kind = static_cast<BallotKind>(kind);
        if (spec.kind == BallotKind::Number) {
            spec.unit = trim_ws(f["unit"]);
            if (spec.unit.size() > 24) spec.unit.resize(24);
            auto mins = trim_ws(f["min"]);
            auto maxs = trim_ws(f["max"]);
            if (!mins.empty() || !maxs.empty()) {
                try { spec.min_v = std::stod(mins); } catch (...) { return http_redir("/proposals?err=range"); }
                try { spec.max_v = std::stod(maxs); } catch (...) { return http_redir("/proposals?err=range"); }
                if (!(spec.min_v < spec.max_v)) return http_redir("/proposals?err=range");
            }
        } else if (spec.kind == BallotKind::Choice) {
            spec.open_choice = !trim_ws(f["open"]).empty();
            std::istringstream in(f["options"]);
            std::string line;
            while (std::getline(in, line)) {
                auto o = trim_ws(line);
                if (!o.empty()) spec.options.push_back(o);
            }
            const std::size_t mn = spec.open_choice ? 1 : 2;
            if (spec.options.size() < mn || spec.options.size() > 12) return http_redir("/proposals?err=options");
            if (!ballot_options_unique(spec.options)) return http_redir("/proposals?err=dupe");
        } else if (spec.kind == BallotKind::Scale) {
            auto left = trim_ws(f["left"]);
            auto right = trim_ws(f["right"]);
            spec.options = {left, right};
        }
        const std::string city_text = trim_ws(f["city"]);
        const std::string who_text = trim_ws(f["who"]);
        std::string city_key_chosen;
        if (!city_text.empty()) {
            city_key_chosen = city_key(clean_city(city_text));
            if (city_key_chosen.empty() || g_store.communities_in_city(city_key_chosen).empty())
                return http_redir("/proposals?err=pcity");
        }
        auto id = g_store.add_proposal(user, title, ptext, spec);
        if (id == 0) return http_redir("/proposals?err=range");
        notice_pains(user, title + ". " + ptext, "prop:" + std::to_string(id));
        if (!city_key_chosen.empty()) g_store.set_proposal_city(id, city_text);
        if (!who_text.empty()) g_store.set_proposal_actor(id, who_text);
        if (!city_key_chosen.empty()) g_store.set_proposal_tags(id, parse_form_multi(body, "tag"));
        if (!city_key_chosen.empty()) return http_redir("/proposals?city="+url_encode(city_key_chosen));
        return http_redir("/proposals");
    }
    if(method=="POST"&&path=="/proposals/option") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        std::uint64_t id = 0;
        try { id = std::stoull(f["id"]); } catch (...) { return http_redir("/proposals"); }
        auto p = g_store.proposal_by_id(id);
        if (!p) return http_redir("/proposals");
        if (!g_store.add_proposal_option(id, user, trim_ws(f["label"]))) {
            // back at the proposal, with the real reason when the list is full (Brief 7 item 5)
            const bool full = p->ballot.kind == BallotKind::Choice && p->ballot.options.size() >= 48;
            return http_redir(back_to_proposal(trim_ws(f["next"]), *p, full ? "optfull" : "option"));
        }
        return http_redir(back_to_proposal(trim_ws(f["next"]), *p));
    }
    if(method=="GET"&&path=="/officials") {
        std::string cards;
        for (const auto& pr : g_store.all_proposals()) {
            if (!pr.is_office) continue;
            cards += "<div class=\"card\" id=\"proposal-"+std::to_string(pr.id)+"\"><h2>"+html_escape(pr.title)+"</h2>"
                     "<p class=\"muted\">"+html_escape(pr.jurisdiction.empty()?pr.body:pr.jurisdiction)+"</p>";
            const std::string next_in = user.empty() ? std::string()
                : "<input type=\"hidden\" name=\"next\" value=\"/officials\">";
            // The same Choice body as a proposal: index payload, "name · count", your check mark (Brief 7 item 1).
            cards += choice_ballot_body(pr, user, next_in, "candidate");
            cards += proposal_talk(pr, user, next_in);
            cards += "</div>";
        }
        if (cards.empty()) cards = "<p class=\"muted\">No races yet.</p>";
        std::string form;
        if (acc) {
            form = "<div class=\"card\"><h2>Open a race</h2>"
                   "<p class=\"muted\">Anyone with an account can vote. Not a certified election.</p>"
                   "<form method=\"POST\" action=\"/officials\">"
                   "<label>Office</label><input name=\"title\" maxlength=\"80\" required placeholder=\"Mayor\">"
                   "<label>Place</label><input name=\"place\" maxlength=\"80\" required placeholder=\"Fresno, CA\">"
                   "<label>Candidates (one per line)</label>"
                   "<textarea name=\"options\" maxlength=\"800\" placeholder=\"One name per line\"></textarea>"
                   "<label class=\"row\" style=\"gap:.4rem;align-items:center;margin-top:.4rem\">"
                   "<input type=\"checkbox\" name=\"open\" value=\"1\" checked> Allow people to add a candidate</label>"
                   "<button type=\"submit\">Open race</button></form></div>";
        } else {
            form = "<p class=\"muted\"><a href=\"/login?next=/officials\">Log in</a> to vote or open a race.</p>";
        }
        return http_ok(wrap("Officials", user, unread,
            std::string("<h1>Officials</h1>"
            "<p class=\"muted\">Account vote only. Not a certified election and not proof of residency.</p>")
            + how_proposals_work_html() + flash_error(qs)
            + cards + form + card_flash_script(qs)));
    }
    if(method=="POST"&&path=="/officials") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        auto title = trim_ws(f["title"]);
        auto place = trim_ws(f["place"]);
        if (title.empty() || title.size() > 80 || place.empty() || place.size() > 80)
            return http_redir("/officials");
        BallotSpec spec;
        spec.kind = BallotKind::Choice;
        spec.open_choice = !trim_ws(f["open"]).empty();
        std::istringstream in(f["options"]);
        std::string line;
        while (std::getline(in, line)) {
            auto o = trim_ws(line);
            if (!o.empty()) spec.options.push_back(o);
        }
        const std::size_t mn = spec.open_choice ? 1 : 2;
        if (spec.options.size() < mn || spec.options.size() > 12)
            return http_redir("/officials?err=options");
        if (!ballot_options_unique(spec.options)) return http_redir("/officials?err=dupe");
        auto nid = g_store.add_proposal(user, title, place, spec);
        if (!nid || !g_store.mark_proposal_office(nid, place))
            return http_redir("/officials");
        return http_redir("/officials");
    }
    if(method=="POST"&&path=="/proposals/reply") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        std::uint64_t id = 0;
        try { id = std::stoull(f["id"]); } catch (...) { return http_redir("/proposals"); }
        auto p = g_store.proposal_by_id(id);
        if (!p) return http_redir("/proposals");
        auto text = trim_ws(f["text"]);
        if (!text.empty()) {
            const auto rid = g_store.add_proposal_reply(id, user, text);
            if (rid) notice_pains(user, text, "prply:" + std::to_string(rid));
        }
        return http_redir(back_to_proposal(trim_ws(f["next"]), *p));
    }
    if(method=="POST"&&path=="/proposals/react") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        std::uint64_t id = 0;
        try { id = std::stoull(f["id"]); } catch (...) { return http_redir("/proposals"); }
        auto p = g_store.proposal_by_id(id);
        if (!p) return http_redir("/proposals");
        const std::string kind = trim_ws(f["kind"]);
        auto cur = g_store.proposal_reaction_of(id, user);
        if (kind.empty() || (cur && *cur == kind)) g_store.set_proposal_reaction(id, user, "");
        else g_store.set_proposal_reaction(id, user, kind);
        return http_redir(back_to_proposal(trim_ws(f["next"]), *p));
    }
    if(method=="POST"&&path=="/proposals/vote") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        std::uint64_t id = 0;
        try { id = std::stoull(f["id"]); } catch (...) { return http_redir("/proposals"); }
        auto p = g_store.proposal_by_id(id);
        if (!p) return http_redir("/proposals");
        // A refused answer says so at the proposal (Brief 7 item 5); a Yes/No choice that is not 0-2 is refused, not
        // turned into Skip.
        if (p->ballot.kind == BallotKind::YesNo) {
            const std::string c = trim_ws(f["choice"]);
            if (c != "0" && c != "1" && c != "2") return http_redir(back_to_proposal(trim_ws(f["next"]), *p, "vote"));
            g_store.cast_proposal_vote(id, user, static_cast<ProposalVote>(c[0] - '0'));
        } else if (!g_store.cast_ballot_value(id, user, trim_ws(f["value"]))) {
            return http_redir(back_to_proposal(trim_ws(f["next"]), *p, "vote"));
        }
        return http_redir(back_to_proposal(trim_ws(f["next"]), *p));
    }
    return std::nullopt;
}

}  // namespace liquidgov::site
