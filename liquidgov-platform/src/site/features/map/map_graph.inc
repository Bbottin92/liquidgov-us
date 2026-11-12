// ---- How relevant a box is to the person looking (Brandon 2026-11-06) ----
// 1 = something you are in or did (green), about .5 = near you (yellow),
// 0 = far from you (red). Built only from the viewer's own involvement, and
// only shown to that viewer: their groups, their cities (a group there, or
// listed there), their friends, proposals they wrote, voted on, reacted to or
// commented on (and those proposals' departments), and the words of their
// kept problems and wants.
struct MapCtx {
    std::string me;
    std::set<std::string> groups, cities, friends, depts; // depts as "<key>|<tag>"
    std::set<std::uint64_t> props;
    std::vector<std::string> keywords;
};
MapCtx map_ctx(const std::string& me) {
    MapCtx c;
    c.me = me;
    if (me.empty()) return c;
    for (const auto& g : g_store.communities_of(me)) {
        c.groups.insert(g.slug);
        const auto k = city_key(g.city);
        if (!k.empty()) c.cities.insert(k);
    }
    for (const auto& cu : g_store.cities_in_use()) if (g_store.is_city_listed(cu.key, me)) c.cities.insert(cu.key);
    for (const auto& f : g_store.friends_of(me)) c.friends.insert(f);
    for (const auto& p : g_store.all_proposals()) {
        bool mine = p.author == me || g_store.proposal_vote_of(p.id, me).has_value() ||
                    g_store.ballot_value_of(p.id, me).has_value() || g_store.proposal_reaction_of(p.id, me).has_value();
        if (!mine) for (const auto& r : g_store.proposal_replies_of(p.id)) if (r.author == me) { mine = true; break; }
        if (!mine) continue;
        c.props.insert(p.id);
        for (const auto& t : p.tags) c.depts.insert(p.city + "|" + t);
    }
    for (const auto& pp : g_store.pains_of(me))
        if (pp.kept) for (const auto& k : pain_keywords(pp.phrase)) c.keywords.push_back(k);
    return c;
}

void map_mine_links(const std::string& kind, const std::string& viewer,
                    const std::function<void(const std::string&, const std::string&)>& link) {
    if (viewer.empty()) return;
    if (kind == "friends") {
        for (const auto& f : g_store.friends_of(viewer)) link("user:" + f, "friends");
    } else if (kind == "groups") {
        for (const auto& c : g_store.communities_of(viewer)) link("group:" + c.slug, "member");
    } else if (kind == "places") {
        // your cities (a group of yours there, or you listed yourself), then the whole site as its own branch
        for (const auto& k : map_ctx(viewer).cities) link("city:" + k, "yours");
        link("root", "in");
    } else if (kind == "politics") {
        // proposals you wrote, voted on, reacted to or commented on: the same set the relevance colours use
        const auto ctx = map_ctx(viewer);
        for (auto it = ctx.props.rbegin(); it != ctx.props.rend(); ++it) link("prop:" + std::to_string(*it), "yours");
    } else if (kind == "problems") {
        for (const auto& p : g_store.pains_of(viewer)) if (p.kept) link("pain:" + std::to_string(p.id), "noticed");
    } else if (kind == "feeds") {
        // a feed with nothing you may see in it is left off (Drawings is never one of them, Brandon 2026-11-11)
        for (const char* f : {"friends", "groups", "anybody-near", "mine"}) {
            bool any = false;
            map_feed_posts(f, viewer, 1, [&](const Post&) { any = true; return false; });
            if (any) link(std::string("feed:") + f, "yours");
        }
    } else if (kind == "interests") {
        for (const auto& i : map_interests_of(viewer)) link("int:" + i.key, "yours");
    }
    // "events": nothing exists until M1.4, so the box stays empty (the ring still shows it, "None yet")
}

void map_feed_posts(const std::string& feed, const std::string& viewer, std::size_t limit,
                    const std::function<bool(const Post&)>& emit) {
    if (viewer.empty() || limit == 0) return;
    std::set<std::string> mygroups, near;
    if (feed == "groups") for (const auto& c : g_store.communities_of(viewer)) mygroups.insert(c.slug);
    if (feed == "anybody-near")
        for (const auto& k : map_ctx(viewer).cities)
            for (const auto& u : g_store.people_listed_in_city(k)) near.insert(u);
    auto wanted = [&](const Post& p) {
        if (feed == "mine") return p.author == viewer;
        if (p.author == viewer) return false;
        if (feed == "friends") return g_store.are_friends(viewer, p.author);
        if (feed == "groups") {
            if (mygroups.count(p.community)) return true;
            for (const auto& g : p.extra_groups) if (mygroups.count(g)) return true;
            return false;
        }
        if (feed == "anybody-near") return p.audience == PostAudience::Anybody && near.count(p.author) > 0;
        return false;
    };
    const auto posts = feed == "mine" ? g_store.posts_by(viewer) : g_store.all_posts(); // both oldest first
    std::size_t n = 0;
    for (auto it = posts.rbegin(); it != posts.rend() && n < limit; ++it) {
        if (!wanted(*it) || !map_post_ok(*it, viewer)) continue;
        ++n;
        if (!emit(*it)) return;
    }
}

void map_interest_links(const std::string& key, const std::string& viewer,
                        const std::function<void(const std::string&, const std::string&)>& link) {
    std::string label;
    for (const auto& i : map_interests_of(viewer)) if (i.key == key) label = i.label;
    if (label.empty()) return;
    constexpr std::size_t kMax = 40, kPeople = 8;
    std::size_t n = 0;
    for (const auto& c : g_store.all_communities())
        if (n < kMax && map_interest_matches(label, map_folded_words(c.name + " " + c.about + " " + c.subcategory))) {
            link("group:" + c.slug, "interest"); ++n;
        }
    auto props = g_store.all_proposals();
    n = 0;
    for (auto it = props.rbegin(); it != props.rend() && n < kMax; ++it)
        if (map_interest_matches(label, map_folded_words(it->title + " " + it->body))) {
            link("prop:" + std::to_string(it->id), "interest"); ++n;
        }
    // People (SPEC 3.7): only those who list the same interest, are on the map (map_person_ok, via link -> map_gnode),
    // findable by name, and listed in one of your cities. At most 8, newest-listed order not implied; never a count.
    std::set<std::string> seen;
    n = 0;
    for (const auto& k : map_ctx(viewer).cities) {
        for (const auto& u : g_store.people_listed_in_city(k)) {
            if (n >= kPeople) return;
            if (u == viewer || !seen.insert(u).second) continue;
            if (g_store.prefs_of(u).findability != Findability::Typeahead) continue;
            if (!map_person_ok(u, viewer)) continue;
            bool same = false;
            for (const auto& i : map_interests_of(u))
                if (map_interest_matches(label, map_folded_words(i.label)) && map_interest_matches(i.label, map_folded_words(label))) {
                    same = true; break;
                }
            if (!same) continue;
            link("user:" + u, "interest");
            ++n;
        }
    }
}

// The second line on the "you" box and your kind boxes (Grok Bot's mockup 1, Brandon 2026-11-11). Built only from the
// viewer's own connections, the same ones the kind box opens onto; nothing about anyone else's lists.
void map_add_sub(GNode& n, const std::string& viewer) {
    if (viewer.empty()) return;
    auto plural = [](std::size_t k, const char* one, const char* many) {
        return std::to_string(k) + " " + (k == 1 ? one : many);
    };
    if (n.kind == "you") { n.sub = "YOU"; return; }
    if (n.kind != "mine") return;
    const std::string kind = n.id.substr(5);
    if (kind == "friends") {
        n.sub = plural(g_store.friends_of(viewer).size(), "person", "people");
    } else if (kind == "groups") {
        const auto gs = g_store.communities_of(viewer);
        if (!gs.empty()) n.sub = gs.front().name + (gs.size() > 1 ? ", and " + std::to_string(gs.size() - 1) + " more" : "");
    } else if (kind == "places") {
        const auto ctx = map_ctx(viewer);
        std::string first;
        if (!ctx.cities.empty())
            for (const auto& cu : g_store.cities_in_use()) if (cu.key == *ctx.cities.begin()) first = cu.name;
        n.sub = first.empty() ? "the whole site" : first + ", then the rest of the site";
    } else if (kind == "politics") {
        n.sub = plural(map_ctx(viewer).props.size(), "proposal you touched", "proposals you touched");
    } else if (kind == "problems") {
        std::size_t k = 0;
        for (const auto& p : g_store.pains_of(viewer)) if (p.kept) ++k;
        n.sub = plural(k, "kept", "kept");
    } else if (kind == "interests") {
        std::string s;
        for (const auto& i : map_interests_of(viewer)) s += (s.empty() ? "" : ", ") + i.label;
        n.sub = s;
    } else if (kind == "feeds") {
        // the feeds that have something for you, by name (never Drawings)
        std::string s;
        for (const auto& [f, name] : std::initializer_list<std::pair<const char*, const char*>>{
                 {"friends", "Friends"}, {"groups", "Groups"}, {"anybody-near", "Near you"}, {"mine", "Yours"}}) {
            bool any = false;
            map_feed_posts(f, viewer, 1, [&](const Post&) { any = true; return false; });
            if (any) s += (s.empty() ? "" : " \xC2\xB7 ") + std::string(name);
        }
        n.sub = s;
    } else if (kind == "events") {
        n.sub = "None yet"; // always on the ring, even empty (Brandon 2026-11-11 4:30 PM decision 1)
    }
}
// -1 = not scored (the MAINstream itself).
double map_relevance(const GNode& n, const MapCtx& c) {
    auto colon = n.id.find(':');
    const std::string rest = colon == std::string::npos ? "" : n.id.substr(colon + 1);
    auto words = [&](const std::string& text) { return pain_words_overlap(c.keywords, text) ? 0.2 : 0.0; };
    auto clamp = [](double v) { return v < 0 ? 0.0 : v > 1 ? 1.0 : v; };
    if (n.kind == "root" || n.kind == "you") return -1; // the base boxes are not scored
    if (n.kind == "mine") return 1;                      // your own kinds of connection
    if (n.kind == "pain") {
        std::uint64_t id = 0;
        try { id = std::stoull(rest); } catch (...) { return 0.35; }
        auto p = g_store.pain_by_id(id);
        if (p && p->owner == c.me) return 1; // their own box
        return 0.35;                         // someone else's flowed subject
    }
    if (n.kind == "interest" || n.kind == "feed") return 1; // your own interests and feeds (viewer-only boxes)
    if (n.kind == "sitewide") return 0.5;
    if (n.kind == "city") return c.cities.count(rest) ? 1.0 : 0.15;
    if (n.kind == "person") {
        if (rest == c.me) return 1;
        if (c.friends.count(rest)) return 0.9;
        for (const auto& g : c.groups) if (g_store.is_member(g, rest)) return 0.75;
        for (const auto& k : c.cities) if (g_store.is_city_listed(k, rest) || g_store.in_city(k, rest)) return 0.6;
        return 0.2;
    }
    if (n.kind == "group") {
        if (c.groups.count(rest)) return 1;
        auto g = g_store.community(rest);
        if (!g) return 0.2;
        const double w = words(g->name + " " + g->about + " " + g->subcategory);
        if (!g->city.empty() && c.cities.count(city_key(g->city))) return clamp(0.65 + w);
        return clamp((g->city.empty() ? 0.35 : 0.15) + w);
    }
    if (n.kind == "dept") {
        if (c.depts.count(rest)) return 0.95;
        auto bar = rest.find('|');
        const std::string key = bar == std::string::npos ? rest : rest.substr(0, bar);
        const std::string tag = bar == std::string::npos ? "" : rest.substr(bar + 1);
        return clamp((c.cities.count(key) ? 0.65 : 0.15) + words(tag));
    }
    if (n.kind == "prop") {
        std::uint64_t id = 0;
        try { id = std::stoull(rest); } catch (...) { return 0.2; }
        if (c.props.count(id)) return 1;
        auto p = g_store.proposal_by_id(id);
        if (!p) return 0.2;
        const double w = words(p->title);
        if (!p->city.empty() && c.cities.count(p->city)) return clamp(0.6 + w);
        return clamp((p->city.empty() ? 0.4 : 0.15) + w);
    }
    if (n.kind == "post") {
        std::uint64_t id = 0;
        try { id = std::stoull(rest); } catch (...) { return 0.3; }
        auto p = g_store.post_by_id(id);
        if (!p) return 0.3;
        if (p->author == c.me) return 1;
        if (c.friends.count(p->author)) return 0.85;
        if (!p->community.empty() && c.groups.count(p->community)) return 0.75;
        for (const auto& g : p->extra_groups) if (c.groups.count(g)) return 0.75;
        return clamp(0.3 + words(p->text));
    }
    return 0.3;
}

std::string map_node_json(const GNode& n, double rel = -1) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%.2f", rel);
    return "{\"id\":\"" + json_escape(n.id) + "\",\"label\":\"" + json_escape(n.label) + "\",\"kind\":\"" +
           json_escape(n.kind) + "\",\"page\":\"" + json_escape(n.page) + "\"" +
           (n.sub.empty() ? std::string() : ",\"sub\":\"" + json_escape(n.sub) + "\"") +
           (rel < 0 ? std::string() : std::string(",\"rel\":") + buf) + "}";
}

// ---- City map view (city proposals, slice 6) ----------------------------
// A picture of one city, drawn by the server as SVG (no framework): People
// here | Groups | the city | Departments and councils | Proposals, with lines
// for who is in which group, which proposal touches which department, and how
// listed people are tied to proposals (wrote, commented; never reacted, which is
// only ever shown as a count). Only what
// the city page already shows: people appear only if they chose to be listed,
// and people blocked either way are left out, same as "People here".

// First `max` characters (UTF-8 code points) of s, with "..." when cut.
std::string map_label(const std::string& s, std::size_t max) {
    std::size_t chars = 0, i = 0;
    while (i < s.size()) {
        if (chars == max) return s.substr(0, i) + "...";
        unsigned char c = static_cast<unsigned char>(s[i]);
        std::size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
        i += len;
        ++chars;
    }
    return s;
}

