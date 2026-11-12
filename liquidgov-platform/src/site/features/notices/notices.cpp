#include "site/features/notices/notices.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/core/http.hpp"
#include "site/features/profile/portrait.hpp"

namespace liquidgov::site {
// Shared by /notices (full page) and GET /notices/panel (the fragment the
// header's notifications widget fetches on open) -- one implementation,
// two callers, so the widget and the page can never drift apart.
std::string notices_list_html(const std::string& user) {
    auto notes = g_store.notices_for(user, 50);
    int fresh = 0;
    for (const auto& n : notes) if (!n.read) ++fresh;
    std::string html = "<div class=\"card\" data-unread=\"" + std::to_string(fresh) + "\">";
    if (fresh > 0) html += "<p class=\"notif-banner\">" + std::to_string(fresh) + (fresh==1?" new":" new") + "</p>";
    if (notes.empty()) html += "<p class=\"empty\">Nothing waiting.</p>";
    for (auto& n : notes) {
        std::string line;
        std::string href;
        const bool pending_fr = n.type=="friend_request" &&
            ([&]{ auto inc=g_store.incoming_requests(user); return std::find(inc.begin(),inc.end(),n.from)!=inc.end(); })();
        if (n.type=="friend_request") { line = html_escape(n.from)+" sent a friend request."; href="/u/"+html_escape(n.from); }
        else if (n.type=="friend_accept") { line = html_escape(n.from)+" accepted your request."; href="/u/"+html_escape(n.from); }
        else if (n.type=="message") { line = html_escape(n.from)+" sent mail."; href="/mail?with="+html_escape(n.from); }
        else if (n.type=="mail_group") { line = html_escape(n.from)+" invited you to a group."; href="/mail/g/"+html_escape(n.extra); }
        else if (n.type=="group_invite") { line = html_escape(n.from)+" added you to /"+html_escape(n.extra)+"."; href="/c/"+html_escape(n.extra); }
        else if (n.type=="map_friends_on") { line = "Your friends now show on the map and in friend suggestions, so people can find each other. You can change it in Settings &rsaquo; Map."; href="/settings#map"; }
        else line = html_escape(n.type);
        html += "<div class=\"person" + std::string(n.read?"":" notice-unread") + "\">";
        if (!n.from.empty()) html += portrait_tag(n.from, "sm");
        else html += "<span class=\"portrait sm ph\" aria-hidden=\"true\">&#9673;</span>"; // a site note: same indent as the rest
        if (!href.empty()) html += "<a href=\""+href+"\" style=\"flex:1;min-width:0;color:inherit;text-decoration:none\">";
        else html += "<div style=\"flex:1;min-width:0\">";
        html += "<div>"+line+"<div class=\"muted\">"+html_escape(fmt_time(n.ts))+"</div></div>";
        if (!href.empty()) html += "</a>"; else html += "</div>";
        if (pending_fr) {
            html += "<form class=\"inline notice-accept\" method=\"POST\" action=\"/friends/accept\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(n.from)+"\">"
                    "<button type=\"submit\">Accept</button></form>";
        }
        html += "</div>";
    }
    html += "<p class=\"muted\">A declined friend request does not send a notice.</p></div>";
    return html;
}

}  // namespace liquidgov::site
