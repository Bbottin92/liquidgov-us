std::string person_result_html(const std::string& viewer, const std::string& name) {
    auto found = g_store.find_by_username(name);
    const bool self = viewer == name;
    const bool pals = !self && g_store.are_friends(viewer, name);
    auto outg = g_store.outgoing_requests(viewer);
    auto inc = g_store.incoming_requests(viewer);
    const bool pending_out = std::find(outg.begin(), outg.end(), name) != outg.end();
    const bool pending_in = std::find(inc.begin(), inc.end(), name) != inc.end();

    std::string meta;
    if (self) meta = "That's you";
    else if (pals) meta = "Friends";
    else if (pending_out) meta = "Request sent";
    else if (pending_in) meta = "Wants to be friends";
    else if (found) meta = "Member since " + fmt_time(found->created_at);
    auto mutual = (self || !found) ? 0 : mutual_friend_count(viewer, name);
    if (mutual) meta += (meta.empty() ? "" : " · ") + std::to_string(mutual) +
                        (mutual == 1 ? " mutual friend" : " mutual friends");
    std::string blurb;
    if (found) {
        blurb = g_store.blurb(found->id);
        if (blurb.size() > 100) blurb = blurb.substr(0, 97) + "...";
    }

    std::string acts;
    if (self) {
        acts = "<a class=\"btn ghost\" href=\"/u/" + html_escape(name) + "\">Your profile</a>";
    } else {
        if (pending_in) {
            acts += "<form class=\"inline\" method=\"POST\" action=\"/friends/accept\">"
                    "<input type=\"hidden\" name=\"who\" value=\"" + html_escape(name) + "\">"
                    "<button type=\"submit\">Accept</button></form>";
        } else if (pals) {
            /* already friends */
        } else if (pending_out) {
            acts += "<span class=\"muted\">Request sent</span>";
        } else {
            acts += "<form class=\"inline\" method=\"POST\" action=\"/friends\">"
                    "<input type=\"hidden\" name=\"who\" value=\"" + html_escape(name) + "\">"
                    "<input type=\"hidden\" name=\"next\" value=\"/search?q=" + html_escape(name) + "\">"
                    "<button type=\"submit\">Add friend</button></form>";
        }
        if (pals) {
            acts += "<a class=\"btn\" href=\"/mail?with=" + html_escape(name) + "\">Message</a>";
        } else {
            acts += "<button class=\"ghost\" type=\"button\" disabled "
                    "title=\"Mail is friends-only until they accept\">Message</button>";
        }
        acts += "<a class=\"btn ghost\" href=\"/u/" + html_escape(name) + "\">Profile</a>";
    }

    return "<div class=\"hit\">" + portrait_tag(name, "sm") +
           "<div class=\"hitinfo\"><a href=\"/u/" + html_escape(name) + "\"><strong>" +
           html_escape(name) + "</strong></a>"
           "<span class=\"muted\">" + html_escape(meta) + "</span>" +
           (blurb.empty() ? "" : "<p class=\"muted\" style=\"margin:.2rem 0 0\">" + html_escape(blurb) + "</p>") +
           "</div><div class=\"hitacts\">" + acts + "</div></div>";
}

std::string people_results_html(const std::string& viewer, const std::string& term) {
    if (term.empty())
        return "<p class=\"muted\">Type a name. Matches update as you type. "
               "Typeahead needs three letters unless the name is exact.</p>";
    auto hits = people_hits(viewer, term);
    if (hits.empty())
        return "<p class=\"muted\">No match. A miss is a miss &mdash; missing, opted out, and blocked look the same.</p>";
    std::string html;
    for (auto& n : hits) html += person_result_html(viewer, n);
    return html;
}

std::string people_search_script() {
    return "<script>(function(){"
           "var q=document.getElementById('peopleq'),box=document.getElementById('peoplehits');"
           "if(!q||!box)return;"
           "var t=null,last='';"
           "function go(){"
           "var v=q.value.trim();if(v===last)return;last=v;"
           "if(!v){box.innerHTML='';return;}"
           "fetch('/search?q='+encodeURIComponent(v)+'&partial=1',{credentials:'same-origin'})"
           ".then(function(r){return r.text();})"
           ".then(function(h){if(q.value.trim()===v)box.innerHTML=h;})"
           ".catch(function(){});"
           "}"
           "q.addEventListener('input',function(){clearTimeout(t);t=setTimeout(go,180);});"
           "if(q.value.trim())go();"
           "})();</script>";
}

// ---- People you may know (Brandon 2026-11-10) ----------------------------
// "you shouldnt have to know someones specific handle. you should be able to see suggestions before even typing
// anything in. like people from groups youre in, or from the same town, or anything that might be relevant."
// Only people findable as you type (his answer); never yourself, a friend, a pending request either way, or a
// block either way. Friends in common are NAMED, like other social sites, but only through friendships both
// friends left visible on the map (MapPrefs::friends). Strongest reason first: shared groups, friends in common,
// the same town, the same problem shared publicly. tests/e2e/friend_suggestions.sh
struct PersonSuggestion { std::string name; std::vector<std::string> why; int score = 0; };

std::vector<PersonSuggestion> people_suggestions(const std::string& me, std::size_t limit) {
    std::vector<PersonSuggestion> out;
    if (me.empty()) return out;
    const auto mine = g_store.friends_of(me);
    std::set<std::string> skip(mine.begin(), mine.end());
    skip.insert(me);
    for (const auto& u : g_store.incoming_requests(me)) skip.insert(u);
    for (const auto& u : g_store.outgoing_requests(me)) skip.insert(u);
    auto eligible = [&](const std::string& u) {
        return !skip.count(u) && !g_store.is_blocked(me, u) && !g_store.is_blocked(u, me) &&
               g_store.find_by_username(u).has_value() && g_store.prefs_of(u).findability == Findability::Typeahead;
    };
    std::map<std::string, std::vector<std::string>> groups_with, friends_with, towns, problems;
    for (const auto& c : g_store.communities_of(me))
        for (const auto& m : g_store.members_of(c.slug)) if (eligible(m)) groups_with[m].push_back(c.name);
    for (const auto& f : mine) {
        if (!g_store.map_prefs_of(f).friends) continue; // f hides their friends
        for (const auto& c : g_store.friends_of(f))
            if (eligible(c) && g_store.map_prefs_of(c).friends) friends_with[c].push_back(f);
    }
    std::set<std::string> my_towns;
    for (const auto& cu : g_store.cities_in_use()) {
        bool in = g_store.is_city_listed(cu.key, me);
        if (!in) for (const auto& c : g_store.communities_of(me)) if (city_key(c.city) == cu.key) { in = true; break; }
        if (!in || !my_towns.insert(cu.key).second) continue;
        for (const auto& u : g_store.people_listed_in_city(cu.key)) if (eligible(u)) towns[u].push_back(cu.name);
    }
    for (const auto& p : g_store.pains_of(me)) {
        if (!p.kept || !p.flowed) continue;
        for (const auto& q : g_store.flowed_with_subject(p.phrase))
            if (q.owner != me && eligible(q.owner)) problems[q.owner].push_back(p.phrase);
    }
    std::set<std::string> everyone;
    for (const auto* m : {&groups_with, &friends_with, &towns, &problems}) for (const auto& [u, v] : *m) everyone.insert(u);
    auto names = [](const std::vector<std::string>& v) {
        if (v.size() == 1) return v[0];
        if (v.size() == 2) return v[0] + " and " + v[1];
        return v[0] + ", " + v[1] + " and " + std::to_string(v.size() - 2) + (v.size() == 3 ? " other" : " others");
    };
    for (const auto& u : everyone) {
        PersonSuggestion s{u, {}, 0};
        if (groups_with.count(u)) {
            const auto& g = groups_with[u];
            s.why.push_back(g.size() == 1 ? "In " + g[0] + " with you" : "In " + std::to_string(g.size()) + " of your groups");
            s.score += 30 * static_cast<int>(g.size());
        }
        if (friends_with.count(u)) {
            s.why.push_back("Friends with " + names(friends_with[u]));
            s.score += 20 * static_cast<int>(friends_with[u].size());
        }
        if (towns.count(u)) { s.why.push_back("Also in " + towns[u][0]); s.score += 10; }
        if (problems.count(u)) { s.why.push_back("Also noticed " + problems[u][0]); s.score += 8; }
        out.push_back(std::move(s));
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        return a.score != b.score ? a.score > b.score : a.name < b.name;
    });
    if (out.size() > limit) out.resize(limit);
    return out;
}

// The card: face, name, why, and a one-tap Add friend that comes back to `back`. Empty when nobody fits.
std::string people_suggestions_html(const std::string& me, std::size_t limit, const std::string& back) {
    const auto list = people_suggestions(me, limit);
    if (list.empty()) return "";
    std::string html = "<div class=\"card\" id=\"suggest\"><h2>People you may know</h2>";
    for (const auto& s : list) {
        std::string why;
        for (const auto& w : s.why) why += (why.empty() ? "" : " &middot; ") + html_escape(w);
        html += "<div class=\"person suggest\" data-user=\"" + html_escape(s.name) + "\"><div class=\"who\">" +
                portrait_tag(s.name, "sm") + "<div><a href=\"/u/" + html_escape(s.name) + "\"><strong>" +
                html_escape(s.name) + "</strong></a><div class=\"muted why\">" + why + "</div></div></div>"
                "<form class=\"inline\" method=\"POST\" action=\"/friends\">"
                "<input type=\"hidden\" name=\"who\" value=\"" + html_escape(s.name) + "\">"
                "<input type=\"hidden\" name=\"next\" value=\"" + html_escape(back) + "\">"
                "<button type=\"submit\">Add friend</button></form></div><!--/row-->";
    }
    if (back != "/friends") html += "<p class=\"muted\"><a href=\"/friends\">More people</a></p>";
    return html + "</div><!--/suggest-->";
}

// One-time switch (Brandon 2026-11-10): everyone who never saved Map settings gets the new friends-on default
// written down, plus a note saying so. Anyone with a saved record keeps what they chose. The record it writes makes
// this run once per person. New accounts get their record at signup (no note: on was already their default).
void map_friends_default_on() {
    for (const auto& u : g_store.all_usernames()) {
        if (g_store.has_map_prefs(u)) continue;
        g_store.set_map_prefs(u, MapPrefs{});
        g_store.add_notice(u, "map_friends_on", "", "");
    }
}

