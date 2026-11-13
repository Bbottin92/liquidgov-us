#include "site/features/pages/home.hpp"
#include "site/core/state.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/features/profile/portrait.hpp"
#include "site/features/people/people.hpp"
#include "site/features/feed/post_block.hpp"

namespace liquidgov::site {
std::string home_dashboard(const std::string& user) {
    const auto posts = g_store.posts_by(user).size();
    const auto friends = g_store.friends_of(user).size();
    const auto committees = g_store.communities_of(user);

    std::string rooms;
    for (const auto& c : committees) {
        bool on_air = false;
        {
            std::lock_guard<std::mutex> lock(g_community_mu);
            auto it = g_community_now_by_station.find(c.slug);
            on_air = (it != g_community_now_by_station.end() && it->second.active);
        }
        char ini_c = c.name.empty() ? '?' : static_cast<char>(std::toupper(static_cast<unsigned char>(c.name[0])));
        rooms += "<a class=\"roomrow\" href=\"/c/" + html_escape(c.slug) + "\"><span class=\"roomini\">" +
                 html_escape(std::string(1, ini_c)) + "</span>"
                 "<span class=\"roomname\">" + html_escape(c.name) + "<br><span class=\"muted\">/" +
                 html_escape(c.slug) + "</span></span>"
                 "<span class=\"roomlive\">" + (on_air ? "on air" : "quiet") + "</span></a>";
    }

    const bool first_hour = posts == 0 && friends == 0 && committees.empty();
    std::string next;
    if (first_hour) {
        next = "<p class=\"muted\">This is a platform account, not a citizen identity. Start with one of these:</p>"
               "<p class=\"row\"><a class=\"btn\" href=\"/friends\">Find someone</a> "
               "<a class=\"btn ghost\" href=\"/me\">Set a portrait</a> "
               "<a class=\"btn ghost\" href=\"/feed\">Write a post</a></p>";
    } else {
        next = "<p class=\"row\"><a class=\"btn\" href=\"/feed\">Open the feed</a> "
               "<a class=\"btn ghost\" href=\"/u/" + html_escape(user) + "\">Your profile</a></p>";
    }

    std::string out =
        "<div class=\"card\"><p class=\"eyebrow\">" + std::string(first_hour ? "You're in" : "Welcome back") +
        "</p><h1>@" + html_escape(user) + "</h1>"
        "<div class=\"stattiles\">"
        "<div class=\"stattile\"><strong>" + std::to_string(posts) + "</strong><span>" +
        std::string(posts==1?"Post":"Posts") + "</span></div>"
        "<div class=\"stattile\"><strong>" + std::to_string(friends) + "</strong><span>" +
        std::string(friends==1?"Friend":"Friends") + "</span></div>"
        "<div class=\"stattile\"><strong>" + std::to_string(committees.size()) + "</strong><span>" +
        std::string(committees.size()==1?"Group":"Groups") + "</span></div>"
        "</div>"
        + next + "</div>";

    auto friend_posts = g_store.feed_view(user, 6, FeedFilter::Friends);
    if (!friend_posts.empty()) {
        out += "<div class=\"card\"><h2>From people you know</h2>";
        for (const auto& p : friend_posts) out += post_block(p, user);
        out += "<p><a href=\"/feed\">Open the feed</a></p></div>";
    }

    Profile prof = g_store.profile_of(user);
    auto ci_has = [](std::string hay, std::string needle) {
        for (char& c : hay) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        for (char& c : needle) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (needle.size() < 3) return false;
        return hay.find(needle) != std::string::npos;
    };
    std::vector<std::string> interest_bits;
    {
        std::string cur;
        for (char c : prof.interests + "," + prof.hometown + "," + prof.living + "," + prof.career + "," + prof.school) {
            if (c==',' || c==';') {
                while (!cur.empty() && cur.front()==' ') cur.erase(cur.begin());
                while (!cur.empty() && cur.back()==' ') cur.pop_back();
                if (cur.size()>=3) interest_bits.push_back(cur);
                cur.clear();
            } else cur.push_back(c);
        }
        while (!cur.empty() && cur.front()==' ') cur.erase(cur.begin());
        while (!cur.empty() && cur.back()==' ') cur.pop_back();
        if (cur.size()>=3) interest_bits.push_back(cur);
    }
    std::string matched;
    for (const auto& c : g_store.all_communities()) {
        bool mine = false;
        for (const auto& m : committees) if (m.slug==c.slug) mine = true;
        if (mine) continue;
        std::string hay = c.name + " " + c.about + " " + c.subcategory + " " + c.slug;
        bool hit = false;
        for (const auto& bit : interest_bits) if (ci_has(hay, bit)) { hit = true; break; }
        if (!hit) continue;
        matched += "<a class=\"roomrow\" href=\"/c/" + html_escape(c.slug) + "\"><span class=\"roomname\">" +
                   html_escape(c.name) + "</span></a>";
    }
    if (!matched.empty())
        out += "<div class=\"card\"><h2>Might fit you</h2>" + matched + "</div>";
    if (!rooms.empty())
        out += "<div class=\"card\"><h2>Your groups</h2>" + rooms + "</div>";
    // a few people you may know, also the first thing after signup (it lands here)
    out += people_suggestions_html(user, 3, "/");

    if (friend_posts.empty() && matched.empty() && rooms.empty()) {
        auto sample = g_store.findable_sample(user, 6);
        auto allc = g_store.all_communities();
        std::string people, comms;
        for (auto& n : sample)
            people += "<div class=\"person\"><div class=\"who\">"+portrait_tag(n,"sm")+
                      "<a href=\"/u/"+html_escape(n)+"\"><strong>"+html_escape(n)+"</strong></a></div>"
                      "<a class=\"btn ghost\" href=\"/search?q="+html_escape(n)+"\">View</a></div>";
        std::size_t shown = 0;
        for (const auto& c : allc) {
            if (shown >= 6) break;
            comms += "<a class=\"roomrow\" href=\"/c/" + html_escape(c.slug) + "\"><span class=\"roomname\">" +
                     html_escape(c.name) + "</span></a>";
            ++shown;
        }
        out += "<div class=\"card\"><h2>Looking around</h2>"
               "<p class=\"muted\">Nothing is tagged to you yet, so this is a mix of people and groups on the site.</p>"
               + people + comms + "</div>";
    }
    return out;
}

}  // namespace liquidgov::site
