#include "site/features/pains/pains.hpp"
#include "site/core/adapters_store.hpp"
#include "site/ui/favicon_cookies.hpp"

namespace liquidgov::site {
// ---- Wishes and pain points (first slice, per Grok's review) ------------
// One text this person wrote. Returns how many new suggestions were stored.
// The switch, an empty user, or empty text stores nothing. Crisis words
// store nothing from this text and arm the 988 note; later texts still run.
// Callers pass this person's own words: posts, proposals, proposal
// comments, and messages they sent. Not the other person's messages.
int notice_pains_added(const std::string& user, const std::string& text, const std::string& source) {
    if (user.empty() || text.empty() || !g_store.map_prefs_of(user).pains) return 0;
    const PainScan scan = find_pain_points(text);
    if (scan.crisis) { g_care_notice.insert(user); return 0; }
    int n = 0;
    for (const auto& hit : scan.hits)
        if (g_store.add_pain_suggestion(user, hit, source)) ++n;
    return n;
}
void notice_pains(const std::string& user, const std::string& text, const std::string& source) {
    notice_pains_added(user, text, source);
}

// A public source can be Flowed. Friends posts, group posts, and messages
// you sent use another prefix and stay on your map only.
bool pain_can_flow(const std::string& source) {
    return source.rfind("post:", 0) == 0 || source.rfind("prop:", 0) == 0 || source.rfind("prply:", 0) == 0;
}
std::string post_pain_source(const Post& post) {
    if (post.audience == PostAudience::Anybody)
        return "post:" + std::to_string(post.id);
    const bool grouped = post.audience == PostAudience::GroupsOnly || !post.community.empty() || !post.extra_groups.empty();
    return (grouped ? "gpost:" : "fpost:") + std::to_string(post.id);
}

// "Look at what I've written": the same read, for writing already saved.
// Own posts (not reposts), proposals they wrote (not an office race),
// proposal comments they wrote, messages they sent, and group messages
// they sent. Never the other person's messages. Returns -1 when the
// switch is off, so the page does not say the scan was empty.
int look_public_words(const std::string& user) {
    if (user.empty() || !g_store.map_prefs_of(user).pains) return -1;
    int n = 0;
    for (const auto& post : g_store.posts_by(user)) {
        if (post.author != user) continue;
        if (!post.repost_of_author.empty() || post.repost_of_id != 0) continue;
        n += notice_pains_added(user, post.text, post_pain_source(post));
    }
    for (const auto& pr : g_store.all_proposals()) {
        if (pr.author == user && !pr.is_office)
            n += notice_pains_added(user, pr.title + ". " + pr.body, "prop:" + std::to_string(pr.id));
        for (const auto& r : g_store.proposal_replies_of(pr.id)) {
            if (r.author != user) continue;
            n += notice_pains_added(user, r.text, "prply:" + std::to_string(r.id));
        }
    }
    for (const auto& m : g_store.mail_sent_by(user)) {
        if (m.from != user || m.text.empty()) continue;
        n += notice_pains_added(user, m.text, "mail:" + std::to_string(m.ts) + ":" + m.to);
    }
    if (auto who = g_store.find_by_username(user)) {
        for (const auto& conv : g_store.conversations_for(who->id)) {
            for (const auto& m : g_store.messages_in(conv.id)) {
                if (m.sender != who->id || m.text.empty()) continue;
                if (m.deletion != DeletionState::NotDeleted) continue;
                n += notice_pains_added(user, m.text, "gmsg:" + std::to_string(m.id));
            }
        }
    }
    return n;
}
PainMatches pain_matches(const PainPoint& p) {
    constexpr std::size_t kCap = 5;
    PainMatches m;
    const auto kw = pain_keywords(p.phrase);
    std::vector<std::string> keys;
    for (const auto& cu : g_store.cities_in_use()) {
        if (!g_store.in_city(cu.key, p.owner) && !g_store.is_city_listed(cu.key, p.owner)) continue;
        keys.push_back(cu.key);
        if (m.city_key.empty()) { m.city_key = cu.key; m.city_name = cu.name; }
    }
    std::set<std::uint64_t> seen_props;
    for (const auto& key : keys) {
        for (const auto& t : g_store.city_tags(key))
            if (m.depts.size() < kCap && pain_words_overlap(kw, t)) m.depts.push_back({key, t});
        for (const auto& pr : g_store.proposals_in_city(key))
            if (m.props.size() < kCap && pain_words_overlap(kw, pr.title) && seen_props.insert(pr.id).second) m.props.push_back(pr);
        for (const auto& c : g_store.communities_in_city(key))
            if (m.groups.size() < kCap && pain_words_overlap(kw, c.name + " " + c.about + " " + c.subcategory)) m.groups.push_back(c);
    }
    for (const auto& pr : g_store.all_proposals())
        if (m.props.size() < kCap && pr.city.empty() && pain_words_overlap(kw, pr.title) && seen_props.insert(pr.id).second)
            m.props.push_back(pr);
    return m;
}

}  // namespace liquidgov::site
