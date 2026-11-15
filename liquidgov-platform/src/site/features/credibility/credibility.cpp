#include "site/features/credibility/credibility.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"

#include <algorithm>
#include <ctime>
#include <map>
#include <set>

namespace liquidgov::site {

// Contribution: how many different people engaged with what you made. The rules that keep one person from moving it
// (the credibility doc's "many voices, each capped" and "independent agreement counts more"): every person counts once
// however often they reply, vote or react; nobody counts for their own work; people who are not your friends count
// double, since agreement from people who do not know you is harder to arrange. Blocked people (either way) do not
// count. Levels: New until there is any history (or in your first week with fewer than 3 people), then by
// weight = friends + 2 x others: Emerging under 10, Established under 30, Trusted from 30.
Contribution contribution_of(const std::string& user) {
    Contribution c;
    if (auto acc = g_store.find_by_username(user)) c.days = std::max<std::int64_t>(0, (std::time(nullptr) - acc->created_at) / 86400);
    std::set<std::string> people;
    std::map<std::string, std::set<std::string>> by_group;
    auto counts = [&](const std::string& who) {
        return !who.empty() && who != user && !g_store.is_blocked(user, who) && !g_store.is_blocked(who, user);
    };
    for (const auto& p : g_store.posts_by(user)) {
        ++c.posts;
        for (const auto& r : g_store.replies_of(p.id))
            if (counts(r.author)) { people.insert(r.author); if (!p.community.empty()) by_group[p.community].insert(r.author); }
    }
    const auto everyone = g_store.all_usernames();
    for (const auto& p : g_store.all_proposals()) {
        if (p.author != user) continue;
        ++c.proposals;
        for (const auto& r : g_store.proposal_replies_of(p.id)) if (counts(r.author)) people.insert(r.author);
        for (const auto& u : everyone) {
            if (!counts(u) || people.count(u)) continue;
            if (g_store.proposal_vote_of(p.id, u) || g_store.proposal_reaction_of(p.id, u) || g_store.ballot_value_of(p.id, u))
                people.insert(u);
        }
    }
    const auto friends = g_store.friends_of(user);
    const std::set<std::string> fset(friends.begin(), friends.end());
    c.people = static_cast<int>(people.size());
    for (const auto& u : people) if (!fset.count(u)) ++c.beyond_friends;
    for (const auto& [slug, who] : by_group) {
        auto g = g_store.community(slug);
        c.groups.emplace_back(g ? g->name : slug, static_cast<int>(who.size()));
    }
    std::sort(c.groups.begin(), c.groups.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    const int weight = (c.people - c.beyond_friends) + 2 * c.beyond_friends;
    if (c.people == 0 || (c.days < 7 && c.people < 3)) c.level = "New";
    else if (weight < 10) c.level = "Emerging";
    else if (weight < 30) c.level = "Established";
    else c.level = "Trusted";
    return c;
}

std::string credibility_settings_card(const std::string& user) {
    const Contribution c = contribution_of(user);
    std::string groups;
    for (std::size_t i = 0; i < c.groups.size() && i < 5; ++i)
        groups += "<li>" + html_escape(c.groups[i].first) + ": " + std::to_string(c.groups[i].second) +
                  (c.groups[i].second == 1 ? " person" : " people") + "</li>";
    return "<div class=\"card\" id=\"credibility\"><h2>Your credibility <span class=\"muted\" style=\"font-size:.8em\">(preview, only you see this)</span></h2>"
           "<p class=\"muted\">How many different people engage with what you make. Each person counts once, however often "
           "they reply, vote or react, so nobody can push it up or down alone; people who aren't your friends count double. "
           "It never changes the weight of your vote.</p>"
           "<p><strong>" + html_escape(c.level) + "</strong></p>"
           "<ul><li>" + std::to_string(c.proposals) + (c.proposals == 1 ? " proposal, " : " proposals, ") +
           std::to_string(c.posts) + (c.posts == 1 ? " post" : " posts") + "</li>"
           "<li>" + std::to_string(c.people) + (c.people == 1 ? " person" : " people") + " engaged with them (" +
           std::to_string(c.beyond_friends) + " beyond your friends)</li></ul>" +
           (groups.empty() ? std::string() : "<p class=\"muted\" style=\"margin-bottom:.2rem\">In your groups</p><ul>" + groups + "</ul>") +
           "</div>";
}

}  // namespace liquidgov::site
