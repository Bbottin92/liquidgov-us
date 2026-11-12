// Category browse for the floating widget's Search tab: which live
// jukeboxes (personal or community) a viewer can tune the widget to,
// filtered by a free-text term within one category. Not a recommendation
// engine -- just filtered listing, same posture as discover_people/
// discover_communities. "Others" and "All" route through typeahead(),
// so Findability and blocks apply exactly as they do everywhere else
// that's used -- a miss looks like a miss.
std::string jukebox_browse_json(const std::string& viewer, const std::string& category, const std::string& term) {
    struct Item { std::string id, name; };
    std::vector<Item> items;
    auto to_lower = [](std::string s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    const std::string term_lower = to_lower(term);
    auto matches = [&](const std::string& hay) {
        if (term.empty()) return true;
        return to_lower(hay).find(term_lower) != std::string::npos;
    };
    auto add_unique = [&](const std::string& id, const std::string& name) {
        for (auto& it : items) if (it.id == id) return;
        items.push_back({id, name});
    };
    if (category == "self") {
        add_unique("u:" + viewer, viewer);
    } else if (category == "group") {
        for (auto& c : g_store.communities_of(viewer)) {
            if (matches(c.name) || matches(c.slug)) add_unique(c.slug, c.name);
            if (items.size() >= 20) break;
        }
    } else if (category == "friends") {
        for (auto& f : g_store.friends_of(viewer)) {
            if (matches(f)) add_unique("u:" + f, f);
            if (items.size() >= 20) break;
        }
    } else if (category == "others") {
        if (term.size() >= 3) {
            for (auto& n : g_store.typeahead(viewer, term)) {
                if (n == viewer || g_store.are_friends(viewer, n)) continue;
                add_unique("u:" + n, n);
                if (items.size() >= 20) break;
            }
        }
    } else { // "all"
        for (auto& f : g_store.friends_of(viewer)) if (matches(f)) add_unique("u:" + f, f);
        for (auto& c : g_store.communities_of(viewer)) if (matches(c.name) || matches(c.slug)) add_unique(c.slug, c.name);
        if (term.size() >= 3) {
            for (auto& n : g_store.typeahead(viewer, term)) {
                if (n == viewer || g_store.are_friends(viewer, n)) continue;
                add_unique("u:" + n, n);
                if (items.size() >= 20) break;
            }
        }
        if (items.size() > 20) items.resize(20);
    }
    std::string out = "{\"items\":[";
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i) out += ",";
        out += "{\"id\":\"" + json_escape(items[i].id) + "\",\"name\":\"" + json_escape(items[i].name) + "\"}";
    }
    out += "]}";
    return out;
}

size_t mutual_friend_count(const std::string& a, const std::string& b) {
    auto fa = g_store.friends_of(a);
    std::set<std::string> sa(fa.begin(), fa.end());
    size_t n = 0;
    for (auto& x : g_store.friends_of(b)) if (sa.count(x)) ++n;
    return n;
}

