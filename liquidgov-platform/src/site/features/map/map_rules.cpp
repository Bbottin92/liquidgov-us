#include "site/features/map/map_rules.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/core/http.hpp"
#include "site/features/pains/pains.hpp"
#include "site/features/map/map_graph.hpp"

namespace liquidgov::site {
// ---- Map privacy rules, shared by every map view ------------------------
// May `viewer` see person `u` as a box on the map? Never someone blocked
// either way; never someone who turned "Show me on the map" off or whose
// profile is hidden from this viewer. You always see yourself.
bool map_person_ok(const std::string& u, const std::string& viewer) {
    if (u.empty()) return false;
    if (u == viewer) return true;
    if (!g_store.find_by_username(u)) return false; // deleted account
    if (!viewer.empty() && (g_store.is_blocked(viewer, u) || g_store.is_blocked(u, viewer))) return false;
    if (!g_store.map_prefs_of(u).show) return false;
    return visible_to(g_store.prefs_of(u).profile_visibility, false, !viewer.empty(),
                      !viewer.empty() && g_store.are_friends(viewer, u));
}

// The viewer has Flowed this same subject on a box of their own.
// Anyone who has not gets no match list: the subject can be visible
// without turning it into a directory of who else shares it.
bool viewer_flowed_subject(const std::string& viewer, const std::string& phrase) {
    const std::string key = pain_subject_key(phrase);
    if (key.empty() || viewer.empty()) return false;
    for (const auto& p : g_store.pains_of(viewer))
        if (p.kept && p.flowed && pain_subject_key(p.phrase) == key) return true;
    return false;
}

// Other people who Flowed this subject and whom the viewer may see.
// One row per person. The viewer and this box's owner are left out.
// Empty unless the viewer Flowed it too. No count is returned.
std::vector<PainPoint> subject_matches(const std::string& viewer, const PainPoint& p) {
    std::vector<PainPoint> out;
    if (!p.kept || !p.flowed || !viewer_flowed_subject(viewer, p.phrase)) return out;
    std::set<std::string> seen;
    seen.insert(viewer);
    seen.insert(p.owner);
    for (const auto& q : g_store.flowed_with_subject(p.phrase)) {
        if (!seen.insert(q.owner).second) continue;
        if (!map_person_ok(q.owner, viewer)) continue;
        out.push_back(q);
        if (out.size() >= 20) break; // leftover names are omitted, never counted
    }
    return out;
}

std::string also_flowed_html(const std::vector<PainPoint>& people) {
    if (people.empty()) return {};
    std::string h = "<p class=\"muted\">Also flowed by ";
    for (std::size_t i = 0; i < people.size(); ++i) {
        if (i) h += ", ";
        h += "<a href=\"/u/" + html_escape(people[i].owner) + "\">" + html_escape(people[i].owner) + "</a>";
    }
    h += ".</p>";
    return h;
}

// Is `viewer` a member of a group this post was shared to (its page's group or one of its extra groups)? Membership
// is read from g_store.is_member, its one source.
bool map_post_in_viewers_group(const Post& p, const std::string& viewer) {
    if (viewer.empty()) return false;
    if (!p.community.empty() && g_store.is_member(p.community, viewer)) return true;
    for (const auto& g : p.extra_groups) if (g_store.is_member(g, viewer)) return true;
    return false;
}

// May `viewer` see this post as a box? The same audience rules as the
// author's profile page, plus the author's "Show my posts" map setting.
bool map_post_ok(const Post& p, const std::string& viewer) {
    if (p.author == viewer) return true;
    if (!map_person_ok(p.author, viewer) || !g_store.map_prefs_of(p.author).posts) return false;
    switch (p.audience) {
        case PostAudience::Anybody:
            return std::find(p.exceptions.begin(), p.exceptions.end(), viewer) == p.exceptions.end();
        case PostAudience::GroupsOnly:
            return map_post_in_viewers_group(p, viewer);
        default: { // Friends
            // Shared to a group too (posted on its page, or Friends + Groups from /feed): that group's members see it,
            // friends or not (Brandon 2026-11-12 "show group posts to group members"). Blocks and the author's
            // "Show my posts" were checked above, so they still hide it.
            if (map_post_in_viewers_group(p, viewer)) return true;
            if (!g_store.are_friends(viewer, p.author)) return false;
            if (g_store.prefs_of(p.author).posts_visibility == Visibility::OnlyMe) return false;
            return p.friend_allowlist.empty() ||
                   std::find(p.friend_allowlist.begin(), p.friend_allowlist.end(), viewer) != p.friend_allowlist.end();
        }
    }
}

// ---- Interests on the map (map SPEC phase 1, section 3.7; Brandon 2026-11-11 "yes, build phase 1") ----------
// Your interests are the comma items of your profile's Interests line, read at view time (no record type). A box id is
// int:<key>: the item's words, lowercase, joined by '-' ("Public Transit" -> public-transit); its label is the item as
// typed. Matching is by whole words, so "parks" never matches "Sparkshop" or "Parking"; a simple plural counts as the
// same word ("park" = "parks", "bus" = "buses"; side agent 2026-11-11), nothing looser.
std::vector<std::string> map_words(const std::string& text) {
    std::vector<std::string> out;
    std::string cur;
    auto flush = [&]() { if (!cur.empty()) out.push_back(cur); cur.clear(); };
    const auto* s = reinterpret_cast<const unsigned char*>(text.data());
    for (std::size_t i = 0, n = text.size(); i < n; ++i) {
        const unsigned char c = s[i];
        // curly quotes and apostrophes (U+2018/2019/201C/201D, what an iPhone types: "Kids’") separate words like ' does
        if (c == 0xE2 && i + 2 < n && s[i + 1] == 0x80 && (s[i + 2] == 0x98 || s[i + 2] == 0x99 || s[i + 2] == 0x9C || s[i + 2] == 0x9D)) {
            flush(); i += 2; continue;
        }
        // accented capitals (Latin-1, U+00C0-00DE except the multiplication sign) lowercase like ASCII: CAFÉ = café
        if (c == 0xC3 && i + 1 < n && s[i + 1] >= 0x80 && s[i + 1] <= 0x9E && s[i + 1] != 0x97) {
            cur.push_back(static_cast<char>(0xC3)); cur.push_back(static_cast<char>(s[i + 1] + 0x20)); ++i; continue;
        }
        if (std::isalnum(c) || c >= 0x80) cur.push_back(static_cast<char>(std::tolower(c))); // bytes >= 0x80: UTF-8 letters
        else flush();
    }
    flush();
    return out;
}
// A word and the singulars it could be: parks -> {parks, park}, buses -> {buses, buse, bus}, houses -> {houses, house,
// hous}, boxes -> {boxes, boxe, box}, cities -> {cities, city}. Two words match when they share a form, so park = parks,
// bus = buses, house = houses; "parking" and "sparks" never meet "park". "-es" is dropped only after s, x, z, ch, sh
// (where English adds it), so James never becomes "jam" (Grok Bot's check 2026-11-11); a few words that only look
// plural keep their s ("news" is not "new"). Words of 3 letters or fewer are left alone ("bus", "gas").
std::vector<std::string> map_word_forms(const std::string& w) {
    static const std::set<std::string> kNotPlural{"news", "series", "species"};
    std::vector<std::string> f{w};
    const auto n = w.size();
    if (kNotPlural.count(w)) return f;
    auto ends = [&](const char* s) { const std::string e(s); return n >= e.size() && w.compare(n - e.size(), e.size(), e) == 0; };
    if (n > 3 && ends("s") && !ends("ss")) f.push_back(w.substr(0, n - 1));
    if (n > 4 && (ends("ses") || ends("xes") || ends("zes") || ends("ches") || ends("shes"))) f.push_back(w.substr(0, n - 2));
    if (n > 4 && ends("ies")) f.push_back(w.substr(0, n - 3) + "y");
    return f;
}
std::set<std::string> map_folded_words(const std::string& text) {
    std::set<std::string> out;
    for (const auto& w : map_words(text)) for (auto& f : map_word_forms(w)) out.insert(std::move(f));
    return out;
}
// Every word of the interest appears as a whole word in the text (any shared form).
bool map_interest_matches(const std::string& interest, const std::set<std::string>& text_words) {
    const auto words = map_words(interest);
    if (words.empty()) return false;
    for (const auto& w : words) {
        bool hit = false;
        for (const auto& f : map_word_forms(w)) if (text_words.count(f)) { hit = true; break; }
        if (!hit) return false;
    }
    return true;
}
std::vector<MapInterest> map_interests_of(const std::string& user) {
    std::vector<MapInterest> out;
    std::set<std::string> seen;
    auto add = [&](std::string item) {
        while (!item.empty() && std::isspace(static_cast<unsigned char>(item.front()))) item.erase(item.begin());
        while (!item.empty() && std::isspace(static_cast<unsigned char>(item.back()))) item.pop_back();
        std::string key;
        for (const auto& w : map_words(item)) key += (key.empty() ? "" : "-") + w;
        if (key.empty() || key.size() > 60 || out.size() >= 12 || !seen.insert(key).second) return;
        std::string label;  // runs of spaces inside an item become one ("Public   Transit" -> "Public Transit")
        for (char c : item) if (!(c == ' ' && !label.empty() && label.back() == ' ')) label.push_back(c);
        out.push_back({key, label});
    };
    std::string cur;
    for (char c : g_store.profile_of(user).interests) {
        if (c == ',' || c == ';') { add(cur); cur.clear(); } else cur.push_back(c);
    }
    add(cur);
    return out;
}

std::optional<GNode> map_gnode(const std::string& id, const std::string& viewer) {
    auto colon = id.find(':');
    const std::string type = colon == std::string::npos ? id : id.substr(0, colon);
    const std::string rest = colon == std::string::npos ? "" : id.substr(colon + 1);
    auto as_u64 = [](const std::string& s) -> std::optional<std::uint64_t> {
        if (s.empty() || s.size() > 19) return std::nullopt;
        for (char c : s) if (c < '0' || c > '9') return std::nullopt;
        return std::stoull(s);
    };
    if (id == "root") return GNode{"root", "MAINstream", "root", ""};
    if (id == "sitewide") return GNode{"sitewide", "Sitewide", "sitewide", "/proposals"};
    // M1.1 (Brandon 2026-11-10): the signed-in map starts at you, and its first ring is kinds of connection. Both are
    // the viewer's own, derived at read time (no record type); "you" is always the person asking, never anyone else.
    if (id == "you") {
        if (viewer.empty()) return std::nullopt;
        return GNode{"you", viewer, "you", "/u/" + url_encode(viewer)};
    }
    if (type == "mine") {
        if (viewer.empty()) return std::nullopt;
        if (rest == "friends") return GNode{id, "Friends", "mine", "/friends"};
        if (rest == "groups") return GNode{id, "Groups", "mine", "/communities"};
        if (rest == "places") return GNode{id, "Places", "mine", ""};
        if (rest == "politics") return GNode{id, "Politics", "mine", "/proposals"};
        if (rest == "problems") return GNode{id, "Problems", "mine", "/problems"};
        // map SPEC phase 1 (Brandon 2026-11-11): Events stays empty until M1.4 (shown anyway, "None yet"), Feeds
        // (never Drawings), Interests from your profile
        if (rest == "events") return GNode{id, "Events", "mine", ""};
        if (rest == "feeds") return GNode{id, "Feeds", "mine", "/feed"};
        if (rest == "interests") return GNode{id, "Interests", "mine", "/me"};
        return std::nullopt;
    }
    if (type == "int") { // one of YOUR interests; anyone else asking for it gets nothing
        if (viewer.empty()) return std::nullopt;
        for (const auto& i : map_interests_of(viewer))
            if (i.key == rest) return GNode{id, i.label, "interest", ""};
        return std::nullopt;
    }
    if (type == "feed") { // one of your feeds; its posts are what you may see anyway (map_post_ok)
        if (viewer.empty()) return std::nullopt;
        if (rest == "friends") return GNode{id, "Friends' posts", "feed", "/feed"};
        if (rest == "groups") return GNode{id, "Your groups' posts", "feed", "/feed"};
        if (rest == "anybody-near") return GNode{id, "Posts near you", "feed", "/feed"};
        if (rest == "mine") return GNode{id, "Your posts", "feed", "/u/" + url_encode(viewer)};
        return std::nullopt;
    }
    if (type == "city") {
        if (rest.empty() || city_key(rest) != rest) return std::nullopt;
        std::string name;
        for (const auto& cu : g_store.cities_in_use()) if (cu.key == rest) name = cu.name;
        if (name.empty() && g_store.proposals_in_city(rest).empty()) return std::nullopt;
        return GNode{id, name.empty() ? rest : name, "city", "/city/" + url_encode(rest)};
    }
    if (type == "group") {
        auto c = g_store.community(rest);
        if (!c) return std::nullopt;
        return GNode{id, c->name, "group", "/c/" + url_encode(c->slug)};
    }
    if (type == "user") {
        if (!map_person_ok(rest, viewer)) return std::nullopt;
        // the name they show (Brandon 2026-11-16: "theres nowhere for people to type in their actual name"); the handle stays
        // as the second line so two people with the same name can be told apart
        const std::string shown = g_store.profile_of(rest).display_name;
        GNode g{id, shown.empty() ? rest : shown, "person", "/u/" + url_encode(rest)};
        if (!shown.empty() && shown != rest) g.sub = "@" + rest;
        return g;
    }
    if (type == "dept") {
        auto bar = rest.find('|');
        if (bar == std::string::npos) return std::nullopt;
        const std::string key = rest.substr(0, bar), tag = rest.substr(bar + 1);
        for (const auto& t : g_store.city_tags(key))
            if (t == tag) return GNode{id, tag, "dept", "/proposals?city=" + url_encode(key)};
        return std::nullopt;
    }
    if (type == "prop") {
        auto n = as_u64(rest);
        if (!n) return std::nullopt;
        auto p = g_store.proposal_by_id(*n);
        if (!p) return std::nullopt;
        return GNode{id, p->title, "prop", "/proposals?id=" + std::to_string(p->id), "", p->body, p->author, p->ts};
    }
    // A comment is a pool of its own (Brandon 2026-11-16): pcom:<id> on a post, rcom:<id> on a proposal. Shown to whoever
    // may see the post (map_post_ok; proposals are public), minus a commenter blocked either way with the viewer.
    if (type == "pcom" || type == "rcom") {
        auto n = as_u64(rest);
        if (!n) return std::nullopt;
        std::string text, author, page;
        std::int64_t ts = 0;
        if (type == "pcom") {
            auto r = g_store.post_reply_by_id(*n);
            if (!r) return std::nullopt;
            auto post = g_store.post_by_id(r->post_id);
            if (!post || !map_post_ok(*post, viewer)) return std::nullopt;
            text = r->text; author = r->author; ts = r->ts; page = "/post/" + std::to_string(post->id);
        } else {
            auto r = g_store.proposal_reply_by_id(*n);
            if (!r || !g_store.proposal_by_id(r->proposal_id)) return std::nullopt;
            text = r->text; author = r->author; ts = r->ts; page = "/proposals?id=" + std::to_string(r->proposal_id);
        }
        if (author != viewer && (g_store.is_blocked(viewer, author) || g_store.is_blocked(author, viewer))) return std::nullopt;
        std::string label = text;
        for (char& ch : label) if (ch == '\n' || ch == '\r' || ch == '\t') ch = ' ';
        return GNode{id, label, "comment", page, "", text, author, ts};
    }
    if (type == "pain") { // a kept wish or pain point. Private until its owner Flows it.
        auto n = as_u64(rest);
        if (!n || viewer.empty()) return std::nullopt;
        auto p = g_store.pain_by_id(*n);
        if (!p || !p->kept) return std::nullopt;
        if (p->owner == viewer)
            return GNode{id, p->phrase, "pain", "/problems#pain" + std::to_string(p->id)};
        // Other people see the subject only. The href is not the owner's list.
        if (!p->flowed || !map_person_ok(p->owner, viewer)) return std::nullopt;
        return GNode{id, p->phrase, "pain", "/pain/" + std::to_string(p->id)};
    }
    if (type == "post") {
        auto n = as_u64(rest);
        if (!n) return std::nullopt;
        auto p = g_store.post_by_id(*n);
        if (!p || !map_post_ok(*p, viewer)) return std::nullopt;
        std::string label = p->text.empty() ? "(attachment)" : p->text;
        for (char& ch : label) if (ch == '\n' || ch == '\r' || ch == '\t') ch = ' ';
        return GNode{id, label, "post", "/post/" + std::to_string(p->id), "", p->text, p->author, p->ts};
    }
    return std::nullopt;
}

// The items under one of your kind boxes (defined after map_ctx, which it uses).

// The posts of one of your feeds, newest first, at most `limit`, each one you may see (map_post_ok). emit returns false
// to stop early. Defined after map_ctx (Near you uses your cities).

// The groups, proposals and people under one of your interests (defined after map_ctx).


std::vector<GLink> map_links(const std::string& id, const std::string& viewer) {
    constexpr std::size_t kMax = 40; // per kind, newest first where it matters
    std::vector<GLink> out;
    std::set<std::string> seen;
    auto link = [&](const std::string& to, const std::string& edge) {
        if (to == id || seen.count(to)) return;
        auto n = map_gnode(to, viewer);
        if (!n) return;
        seen.insert(to);
        out.push_back({*n, edge});
    };
    auto person_groups_ok = [&](const std::string& u) { return u == viewer || g_store.map_prefs_of(u).groups; };
    auto person_comments_ok = [&](const std::string& u) { return u == viewer || g_store.map_prefs_of(u).comments; };
    auto prop_links = [&](const Proposal& p) {
        if (!p.city.empty()) link("city:" + p.city, "in");
        for (const auto& t : p.tags) link("dept:" + p.city + "|" + t, "touches");
        link("user:" + p.author, "wrote");
        for (const auto& r : g_store.proposal_replies_of(p.id))
            if (person_comments_ok(r.author)) link("user:" + r.author, "commented");
        {   // and the comments themselves, each a pool (the newest 12)
            auto rs = g_store.proposal_replies_of(p.id);
            for (std::size_t i = rs.size() > 12 ? rs.size() - 12 : 0; i < rs.size(); ++i) link("rcom:" + std::to_string(rs[i].id), "commented");
        }
    };
    auto colon = id.find(':');
    const std::string type = colon == std::string::npos ? id : id.substr(0, colon);
    const std::string rest = colon == std::string::npos ? "" : id.substr(colon + 1);

    if (id == "you") {
        if (viewer.empty()) return out;
        // A kind with nothing in it is left off; Places always has the whole site. Clockwise from the top in the order
        // of the map SPEC's mockup A (Brandon's seven kinds, then your own Problems). Events always shows, even empty
        // (Brandon 2026-11-11 4:30 PM decision 1), so people see the map has a place for events before M1.4.
        for (const char* k : {"politics", "friends", "events", "feeds", "places", "groups", "interests", "problems"}) {
            bool any = std::string(k) == "events";
            map_mine_links(k, viewer, [&](const std::string& to, const std::string&) {
                if (!any && to != "you" && map_gnode(to, viewer)) any = true;
            });
            if (any) link(std::string("mine:") + k, "yours");
        }
    } else if (type == "mine") {
        if (!map_gnode(id, viewer)) return out;
        link("you", "yours");
        map_mine_links(rest, viewer, link);
    } else if (type == "feed") {
        if (!map_gnode(id, viewer)) return out;
        link("mine:feeds", "yours");
        map_feed_posts(rest, viewer, 12, [&](const Post& p) { link("post:" + std::to_string(p.id), "posted"); return true; });
    } else if (type == "int") {
        if (!map_gnode(id, viewer)) return out; // only your own interests
        link("mine:interests", "yours");
        map_interest_links(rest, viewer, link);
    } else if (id == "root") {
        if (!viewer.empty()) link("you", "in");
        for (const auto& cu : g_store.cities_in_use()) link("city:" + cu.key, "in");
        link("sitewide", "in");
    } else if (id == "sitewide") {
        auto props = g_store.all_proposals();
        std::reverse(props.begin(), props.end());
        std::size_t n = 0;
        for (const auto& p : props) if (p.city.empty() && n++ < kMax) link("prop:" + std::to_string(p.id), "in");
        n = 0;
        for (const auto& c : g_store.all_communities()) if (c.city.empty() && n++ < kMax) link("group:" + c.slug, "in");
    } else if (type == "city") {
        link("root", "in");
        for (const auto& c : g_store.communities_in_city(rest)) link("group:" + c.slug, "in");
        for (const auto& t : g_store.city_tags(rest)) link("dept:" + rest + "|" + t, "in");
        auto props = g_store.proposals_in_city(rest);
        std::reverse(props.begin(), props.end());
        for (std::size_t i = 0; i < props.size() && i < kMax; ++i)
            if (props[i].tags.empty()) link("prop:" + std::to_string(props[i].id), "in");
        for (const auto& u : g_store.people_listed_in_city(rest)) link("user:" + u, "lives");
    } else if (type == "group") {
        auto c = g_store.community(rest);
        if (!c) return out;
        link(c->city.empty() ? std::string("sitewide") : "city:" + city_key(c->city), "in");
        for (const auto& m : g_store.members_of(rest)) if (person_groups_ok(m)) link("user:" + m, "member");
        auto posts = g_store.posts_in(rest, kMax);
        for (const auto& p : posts) if (map_post_ok(p, viewer)) link("post:" + std::to_string(p.id), "posted");
    } else if (type == "user") {
        if (!map_person_ok(rest, viewer)) return out;
        if (person_groups_ok(rest))
            for (const auto& c : g_store.communities_of(rest)) link("group:" + c.slug, "member");
        for (const auto& cu : g_store.cities_in_use())
            if (g_store.is_city_listed(cu.key, rest)) link("city:" + cu.key, "lives");
        for (const auto& p : g_store.all_proposals()) {
            if (p.author == rest) link("prop:" + std::to_string(p.id), "wrote");
            else if (person_comments_ok(rest))
                for (const auto& r : g_store.proposal_replies_of(p.id))
                    if (r.author == rest) { link("prop:" + std::to_string(p.id), "commented"); break; }
        }
        auto posts = g_store.posts_by(rest);
        std::reverse(posts.begin(), posts.end());
        std::size_t n = 0;
        for (const auto& p : posts) if (n < kMax && map_post_ok(p, viewer)) { link("post:" + std::to_string(p.id), "posted"); ++n; }
        // Kept boxes on this person. Theirs are all visible to them. Anyone
        // else's map shows a box only after that person Flows it (map_gnode
        // also applies the block and show-me gates).
        for (const auto& p : g_store.pains_of(rest)) {
            if (!p.kept) continue;
            if (rest != viewer && !p.flowed) continue;
            link("pain:" + std::to_string(p.id), "noticed");
        }
        // Friends: you always see your own; anyone else's only when both agreed.
        for (const auto& f : g_store.friends_of(rest)) {
            const bool mine = rest == viewer || f == viewer;
            if (mine || (g_store.map_prefs_of(rest).friends && g_store.map_prefs_of(f).friends)) link("user:" + f, "friends");
        }
    } else if (type == "dept") {
        auto bar = rest.find('|');
        if (bar == std::string::npos) return out;
        const std::string key = rest.substr(0, bar), tag = rest.substr(bar + 1);
        link("city:" + key, "in");
        for (const auto& p : g_store.proposals_in_city(key))
            if (std::find(p.tags.begin(), p.tags.end(), tag) != p.tags.end()) link("prop:" + std::to_string(p.id), "touches");
    } else if (type == "prop") {
        auto n = map_gnode(id, viewer);
        if (!n) return out;
        auto p = g_store.proposal_by_id(std::stoull(rest));
        if (!p) return out;
        if (p->city.empty()) link("sitewide", "in");
        prop_links(*p);
    } else if (type == "pain") {
        if (!map_gnode(id, viewer)) return out; // private, hidden, or missing
        auto p = g_store.pain_by_id(std::stoull(rest));
        if (!p) return out;
        link("user:" + p->owner, "noticed");
        // Departments, proposals and groups stay on the owner's own view.
        if (viewer == p->owner) {
            const auto m = pain_matches(*p);
            for (const auto& [key, tag] : m.depts) link("dept:" + key + "|" + tag, "could-act");
            for (const auto& pr : m.props) link("prop:" + std::to_string(pr.id), "about");
            for (const auto& c : m.groups) link("group:" + c.slug, "working-on");
        }
        // Same subject, only for someone who Flowed it themselves. A person
        // who can see the box but did not Flow it gets no match edges.
        // Hidden people fail map_gnode and are skipped. No edge carries a count.
        if (p->kept && p->flowed && viewer_flowed_subject(viewer, p->phrase)) {
            std::set<std::string> owners;
            owners.insert(p->owner);
            std::size_t n = 0;
            for (const auto& q : g_store.flowed_with_subject(p->phrase)) {
                if (n >= 8) break;
                if (!owners.insert(q.owner).second) continue;
                const auto before = out.size();
                link("pain:" + std::to_string(q.id), "same subject");
                if (out.size() > before) ++n;
            }
        }
    } else if (type == "post") {
        auto n = map_gnode(id, viewer);
        if (!n) return out;
        auto p = g_store.post_by_id(std::stoull(rest));
        if (!p) return out;
        link("user:" + p->author, "posted");
        if (!p->community.empty()) link("group:" + p->community, "posted");
        for (const auto& g : p->extra_groups) link("group:" + g, "posted");
        if (p->repost_of_id) link("post:" + std::to_string(p->repost_of_id), "shared");
        // its comments, each a pool (the newest 12)
        auto rs = g_store.replies_of(p->id);
        for (std::size_t i = rs.size() > 12 ? rs.size() - 12 : 0; i < rs.size(); ++i) link("pcom:" + std::to_string(rs[i].id), "commented");
    } else if (type == "pcom" || type == "rcom") {
        auto n = map_gnode(id, viewer);
        if (!n) return out;
        const auto k = std::stoull(rest);
        if (type == "pcom") { if (auto r = g_store.post_reply_by_id(k)) link("post:" + std::to_string(r->post_id), "on"); }
        else if (auto r = g_store.proposal_reply_by_id(k)) link("prop:" + std::to_string(r->proposal_id), "on");
        if (!n->by.empty()) link("user:" + n->by, "wrote");
    }
    return out;
}

}  // namespace liquidgov::site
