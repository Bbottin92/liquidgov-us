struct MapNode {
    std::string id;     // "c", "g0", "u0", "d0", "p0", or "more-..."
    std::string label;
    std::string title;  // full text for the hover tooltip
    std::string href;   // empty: not a link
    std::string kind;   // city, group, person, dept, prop, more
    int col = 0;
    int row = 0;
};
struct MapEdge { std::string a, b, kind; }; // kind: in, member, touches, wrote, commented, reacted

std::string city_map_svg(const std::string& key, const std::string& name, const std::string& viewer) {
    constexpr std::size_t kCap = 20; // boxes per column before "+N more"
    std::vector<MapNode> nodes;
    std::vector<MapEdge> edges;
    std::map<std::string, std::string> group_id, person_id, dept_id; // value -> node id
    std::map<std::uint64_t, std::string> prop_id;
    int rows[5] = {0, 0, 0, 0, 0};
    auto add = [&](int col, const std::string& id, const std::string& label, const std::string& title,
                   const std::string& href, const std::string& kind) {
        MapNode n{id, map_label(label, 22), title, href, kind, col, rows[col]++};
        nodes.push_back(n);
    };
    auto more = [&](int col, std::size_t extra, const std::string& href) {
        if (extra == 0) return;
        add(col, "more-" + std::to_string(col), "+" + std::to_string(extra) + " more", "", href, "more");
    };
    const std::string city_href_str = "/city/" + url_encode(key);

    add(2, "c", name, name, city_href_str, "city");

    auto groups = g_store.communities_in_city(key);
    for (std::size_t i = 0; i < groups.size() && i < kCap; ++i) {
        const auto& c = groups[i];
        group_id[c.slug] = "g" + std::to_string(i);
        add(1, group_id[c.slug], c.name, c.name + " (/" + c.slug + ")", "/c/" + url_encode(c.slug), "group");
        edges.push_back({"c", group_id[c.slug], "in"});
    }
    more(1, groups.size() > kCap ? groups.size() - kCap : 0, city_href_str);

    std::vector<std::string> people;
    for (const auto& n : g_store.people_listed_in_city(key))
        if (map_person_ok(n, viewer)) people.push_back(n); // blocks, "Show me on the map", profile visibility
    for (std::size_t i = 0; i < people.size() && i < kCap; ++i) {
        person_id[people[i]] = "u" + std::to_string(i);
        add(0, person_id[people[i]], people[i], people[i], "/u/" + url_encode(people[i]), "person");
        if (people[i] != viewer && !g_store.map_prefs_of(people[i]).groups) continue; // "Show the groups I'm in"
        for (const auto& [slug, gid] : group_id)
            if (g_store.is_member(slug, people[i])) edges.push_back({person_id[people[i]], gid, "member"});
    }
    more(0, people.size() > kCap ? people.size() - kCap : 0, city_href_str);

    auto tags = g_store.city_tags(key);
    for (std::size_t i = 0; i < tags.size() && i < kCap; ++i) {
        dept_id[tags[i]] = "d" + std::to_string(i);
        add(3, dept_id[tags[i]], tags[i], tags[i], "", "dept");
        edges.push_back({"c", dept_id[tags[i]], "in"});
    }
    more(3, tags.size() > kCap ? tags.size() - kCap : 0, city_href_str);

    auto props = g_store.proposals_in_city(key);
    std::reverse(props.begin(), props.end()); // newest first, so the cap keeps recent ones
    const std::string props_href = "/proposals?city=" + url_encode(key);
    for (std::size_t i = 0; i < props.size() && i < kCap; ++i) {
        const auto& p = props[i];
        const std::string pid = "p" + std::to_string(i);
        prop_id[p.id] = pid;
        add(4, pid, p.title, "Proposal " + std::to_string(p.id) + ": " + p.title, props_href, "prop");
        bool tagged = false;
        for (const auto& t : p.tags) {
            auto d = dept_id.find(t);
            if (d != dept_id.end()) { edges.push_back({d->second, pid, "touches"}); tagged = true; }
        }
        if (!tagged) edges.push_back({"c", pid, "in"});
        // One line per listed person: wrote beats commented. Reactions are NOT
        // drawn: proposal pages show them only as counts, so who reacted is not
        // public, and the map must not make it so.
        std::map<std::string, std::string> role;
        for (const auto& r : g_store.proposal_replies_of(p.id))
            if (person_id.count(r.author) && (r.author == viewer || g_store.map_prefs_of(r.author).comments))
                role[r.author] = "commented";
        if (person_id.count(p.author)) role[p.author] = "wrote";
        for (const auto& [who, kind] : role) edges.push_back({person_id[who], pid, kind});
    }
    more(4, props.size() > kCap ? props.size() - kCap : 0, props_href);

    // Geometry: five columns in a 1100-wide viewBox; the city sits mid-height.
    constexpr int kColX[5] = {20, 240, 470, 700, 910};
    constexpr int kBoxW[5] = {170, 180, 160, 170, 170};
    constexpr int kRowH = 46, kBoxH = 34, kTop = 56;
    int max_rows = 1;
    for (int r : rows) max_rows = std::max(max_rows, r);
    const int height = std::max(300, kTop + max_rows * kRowH + 20);
    auto node_y = [&](const MapNode& n) {
        const int col_h = rows[n.col] * kRowH;
        const int start = kTop + (max_rows * kRowH - col_h) / 2; // centre each column
        return start + n.row * kRowH;
    };
    std::map<std::string, const MapNode*> by_id;
    for (const auto& n : nodes) by_id[n.id] = &n;

    std::ostringstream os;
    os << "<div class=\"cmapwrap\"><svg class=\"cmap\" viewBox=\"0 0 1100 " << height
       << "\" role=\"img\" aria-label=\"Map of " << html_escape(name) << "\" xmlns=\"http://www.w3.org/2000/svg\">";
    const char* heads[5] = {"People here", "Groups", "City", "Departments and councils", "Proposals"};
    for (int c = 0; c < 5; ++c)
        os << "<text class=\"cmaph\" x=\"" << kColX[c] + kBoxW[c] / 2 << "\" y=\"28\">" << heads[c] << "</text>";
    os << "<g class=\"cmapedges\">";
    for (const auto& e : edges) {
        auto ia = by_id.find(e.a), ib = by_id.find(e.b);
        if (ia == by_id.end() || ib == by_id.end()) continue;
        const MapNode* a = ia->second; const MapNode* b = ib->second;
        if (a->col > b->col) std::swap(a, b);
        const int x1 = kColX[a->col] + kBoxW[a->col], y1 = node_y(*a) + kBoxH / 2;
        const int x2 = kColX[b->col], y2 = node_y(*b) + kBoxH / 2;
        const int dx = std::max(30, (x2 - x1) / 2);
        os << "<path class=\"ce ce-" << e.kind << "\" data-a=\"" << e.a << "\" data-b=\"" << e.b << "\" d=\"M" << x1 << " " << y1
           << " C" << x1 + dx << " " << y1 << " " << x2 - dx << " " << y2 << " " << x2 << " " << y2 << "\"/>";
    }
    os << "</g>";
    for (const auto& n : nodes) {
        const int x = kColX[n.col], y = node_y(n), w = kBoxW[n.col];
        if (!n.href.empty()) os << "<a href=\"" << html_escape(n.href) << "\">";
        os << "<g class=\"cn cn-" << n.kind << "\" data-n=\"" << n.id << "\">";
        if (!n.title.empty()) os << "<title>" << html_escape(n.title) << "</title>";
        os << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w << "\" height=\"" << kBoxH << "\" rx=\"8\"/>"
           << "<text x=\"" << x + w / 2 << "\" y=\"" << y + kBoxH / 2 + 5 << "\">" << html_escape(n.label) << "</text></g>";
        if (!n.href.empty()) os << "</a>";
    }
    if (rows[0] == 0)
        os << "<text class=\"cmapnote\" x=\"" << kColX[0] + kBoxW[0] / 2 << "\" y=\"" << kTop + 20 << "\">Nobody listed yet</text>";
    os << "</svg></div>";
    return os.str();
}

// The root of the map (Brandon 2026-11-06: "a mAInstream Media Map and then things
// branch off from there"): MAINstream -> each city's Current -> a few of its groups
// and proposals, plus a Sitewide branch for proposals with no city. Clicking a city
// opens its own map (city_map_svg). Only groups and proposals, which every signed-in
// member can already see; no people at this level.
std::string main_stream_svg() {
    constexpr std::size_t kCities = 30, kLeaves = 3, kSitewide = 5;
    struct Leaf { std::string label, title, href, kind; };
    struct Branch { std::string label, title, href, kind; std::vector<Leaf> leaves; };
    std::vector<Branch> branches;
    auto cities = g_store.cities_in_use();
    for (std::size_t i = 0; i < cities.size() && i < kCities; ++i) {
        const auto& cu = cities[i];
        const std::string map_href = "/city/" + url_encode(cu.key) + "/map";
        Branch b{cu.name, cu.name + "'s Current", map_href, "city", {}};
        auto groups = g_store.communities_in_city(cu.key);
        for (std::size_t g = 0; g < groups.size() && g < kLeaves; ++g)
            b.leaves.push_back({groups[g].name, "Group: " + groups[g].name, "/c/" + url_encode(groups[g].slug), "group"});
        auto props = g_store.proposals_in_city(cu.key);
        std::reverse(props.begin(), props.end());
        for (std::size_t p = 0; p < props.size() && p < kLeaves; ++p)
            b.leaves.push_back({props[p].title, "Proposal: " + props[p].title, "/proposals?city=" + url_encode(cu.key), "prop"});
        const std::size_t extra = (groups.size() > kLeaves ? groups.size() - kLeaves : 0) +
                                  (props.size() > kLeaves ? props.size() - kLeaves : 0);
        if (extra) b.leaves.push_back({"+" + std::to_string(extra) + " more in " + cu.name, "", map_href, "more"});
        branches.push_back(std::move(b));
    }
    if (cities.size() > kCities)
        branches.push_back({"+" + std::to_string(cities.size() - kCities) + " more cities", "", "/communities", "more", {}});
    {
        Branch s{"Sitewide", "Proposals not about any one city", "/proposals", "dept", {}};
        std::vector<Proposal> sitewide;
        for (const auto& p : g_store.all_proposals()) if (p.city.empty() && !p.is_office) sitewide.push_back(p);
        std::reverse(sitewide.begin(), sitewide.end());
        for (std::size_t i = 0; i < sitewide.size() && i < kSitewide; ++i)
            s.leaves.push_back({sitewide[i].title, "Proposal: " + sitewide[i].title, "/proposals", "prop"});
        if (sitewide.size() > kSitewide)
            s.leaves.push_back({"+" + std::to_string(sitewide.size() - kSitewide) + " more", "", "/proposals", "more"});
        branches.push_back(std::move(s));
    }

    // Geometry: root | branches | leaves. Each branch gets as many rows as it has
    // leaves (at least one) and sits centred on them.
    constexpr int kX[3] = {20, 250, 540}, kW[3] = {170, 210, 300};
    constexpr int kRowH = 42, kBoxH = 32, kTop = 52;
    int total_rows = 0;
    for (const auto& b : branches) total_rows += std::max<int>(1, static_cast<int>(b.leaves.size()));
    const int height = std::max(260, kTop + total_rows * kRowH + 20);
    std::ostringstream edges, boxes;
    auto box = [&](int col, int y, const std::string& id, const std::string& label, const std::string& title,
                   const std::string& href, const std::string& kind) {
        const int x = kX[col], w = kW[col];
        if (!href.empty()) boxes << "<a href=\"" << html_escape(href) << "\">";
        boxes << "<g class=\"cn cn-" << kind << "\" data-n=\"" << id << "\">";
        if (!title.empty()) boxes << "<title>" << html_escape(title) << "</title>";
        boxes << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w << "\" height=\"" << kBoxH << "\" rx=\"8\"/>"
              << "<text x=\"" << x + w / 2 << "\" y=\"" << y + kBoxH / 2 + 5 << "\">"
              << html_escape(map_label(label, col == 2 ? 38 : 26)) << "</text></g>";
        if (!href.empty()) boxes << "</a>";
    };
    auto edge = [&](int c1, int y1, int c2, int y2, const std::string& a, const std::string& b, const std::string& kind) {
        const int x1 = kX[c1] + kW[c1], x2 = kX[c2], dx = (x2 - x1) / 2;
        y1 += kBoxH / 2; y2 += kBoxH / 2;
        edges << "<path class=\"ce ce-" << kind << "\" data-a=\"" << a << "\" data-b=\"" << b << "\" d=\"M" << x1 << " " << y1
              << " C" << x1 + dx << " " << y1 << " " << x2 - dx << " " << y2 << " " << x2 << " " << y2 << "\"/>";
    };
    const int root_y = kTop + (total_rows * kRowH) / 2 - kRowH / 2;
    int row = 0;
    for (std::size_t i = 0; i < branches.size(); ++i) {
        const auto& b = branches[i];
        const int n = std::max<int>(1, static_cast<int>(b.leaves.size()));
        const int by = kTop + row * kRowH + (n - 1) * kRowH / 2;
        const std::string bid = "b" + std::to_string(i);
        box(1, by, bid, b.label, b.title, b.href, b.kind);
        edge(0, root_y, 1, by, "root", bid, "in");
        for (std::size_t j = 0; j < b.leaves.size(); ++j) {
            const auto& l = b.leaves[j];
            const int ly = kTop + (row + static_cast<int>(j)) * kRowH;
            const std::string lid = bid + "-" + std::to_string(j);
            box(2, ly, lid, l.label, l.title, l.href, l.kind);
            edge(1, by, 2, ly, bid, lid, l.kind == "prop" ? "touches" : "in");
        }
        row += n;
    }
    box(0, root_y, "root", "MAINstream", "MAINstream", "", "city");
    std::ostringstream os;
    os << "<div class=\"cmapwrap\"><svg class=\"cmap\" viewBox=\"0 0 860 " << height
       << "\" role=\"img\" aria-label=\"MAINstream map\" xmlns=\"http://www.w3.org/2000/svg\">"
       << "<text class=\"cmaph\" x=\"" << kX[1] + kW[1] / 2 << "\" y=\"26\">Currents</text>"
       << "<text class=\"cmaph\" x=\"" << kX[2] + kW[2] / 2 << "\" y=\"26\">Groups and proposals</text>"
       << "<g class=\"cmapedges\">" << edges.str() << "</g>" << boxes.str() << "</svg></div>";
    return os.str();
}

// Styles and the hover script for the map: point at a box and its lines light up.
const char* kCityMapStyle =
    "<style>"
    ".cmapwrap{overflow-x:auto;border:1px solid var(--rule);border-radius:var(--rmd,10px);background:var(--panel)}"
    ".cmap{display:block;min-width:820px;width:100%;height:auto;font-family:var(--body,inherit)}"
    ".cmaph{font-size:13px;font-weight:600;fill:var(--stone);text-anchor:middle}"
    ".cmapnote{font-size:12px;fill:var(--stone);text-anchor:middle;font-style:italic}"
    ".cn rect{fill:var(--paper);stroke:var(--rule);stroke-width:1.5}"
    ".cn text{font-size:13px;fill:var(--ink);text-anchor:middle;pointer-events:none}"
    ".cn-city rect{fill:var(--accent);stroke:var(--accent)}.cn-city text{fill:var(--paper);font-weight:700;font-size:15px}"
    ".cn-group rect{stroke:var(--accent)}.cn-person rect{stroke:var(--ok)}.cn-dept rect{stroke:#8a5cc2}"
    ".cn-prop rect{stroke:var(--warn)}.cn-more rect{stroke-dasharray:4 3}"
    ".cn{cursor:pointer}.cn:hover rect,.cn:focus rect{stroke-width:3}"
    ".ce{fill:none;stroke:var(--rule);stroke-width:1.5;opacity:.75;transition:opacity .15s}"
    ".ce-member{stroke:var(--ok)}.ce-touches{stroke:#8a5cc2}.ce-wrote{stroke:var(--warn);stroke-width:2}"
    ".ce-commented{stroke:var(--warn);stroke-dasharray:6 4}.ce-reacted{stroke:var(--warn);stroke-dasharray:2 4}"
    ".cmap.focus .ce{opacity:.08}.cmap.focus .ce.hi{opacity:1;stroke-width:3}"
    ".cmaplegend{display:flex;flex-wrap:wrap;gap:.4rem 1rem;margin:.6rem 0 0;font-size:.85rem}"
    ".cmaplegend span::before{content:'';display:inline-block;width:22px;height:0;margin-right:.35rem;vertical-align:middle;border-top:3px solid var(--rule)}"
    ".lg-member::before{border-color:var(--ok)!important}.lg-touches::before{border-color:#8a5cc2!important}"
    ".lg-wrote::before{border-color:var(--warn)!important}.lg-commented::before{border-top-style:dashed!important;border-color:var(--warn)!important}"
    ".lg-reacted::before{border-top-style:dotted!important;border-color:var(--warn)!important}"
    "</style>"
    "<script>(function(){var s=document.querySelector('.cmap');if(!s)return;"
    "function on(id){s.classList.add('focus');s.querySelectorAll('.ce').forEach(function(e){"
    "if(e.getAttribute('data-a')===id||e.getAttribute('data-b')===id)e.classList.add('hi');});}"
    "function off(){s.classList.remove('focus');s.querySelectorAll('.ce.hi').forEach(function(e){e.classList.remove('hi');});}"
    "s.querySelectorAll('.cn').forEach(function(n){var id=n.getAttribute('data-n');"
    "var t=n.parentNode&&n.parentNode.tagName.toLowerCase()==='a'?n.parentNode:n;"
    "t.addEventListener('mouseenter',function(){on(id);});t.addEventListener('mouseleave',off);"
    "t.addEventListener('focus',function(){on(id);});t.addEventListener('blur',off);});})();</script>";

