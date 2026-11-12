#include "site/features/people/people_routes.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/core/http.hpp"
#include "site/features/feed/composer.hpp"
#include "site/features/people/people.hpp"
#include "site/features/profile/portrait.hpp"

namespace liquidgov::site {

std::optional<std::string> people_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
    if(method=="GET"&&path=="/friends") {
        if(!acc) return need_login();
        std::string err = flash_error(qs);
        std::string html = "<h1>People</h1>"+err+ people_suggestions_html(user, 12, "/friends") +
            "<div class=\"card\"><h2>Find someone</h2>"
            "<form method=\"GET\" action=\"/search\"><label>Type a name</label>"
            "<div class=\"row\"><input name=\"q\" id=\"peopleq\" autocomplete=\"off\" "
            "placeholder=\"Starts matching as you type\"><button>Search</button></div></form>"
            "<div id=\"peoplehits\"></div>"
            "<p class=\"muted\">Friends always appear. Everyone else follows their findability. "
            "Mail is friends-only until they accept.</p></div>"
            + people_search_script() +
            "<div class=\"card\"><h2>Add by exact name</h2>"
            "<form method=\"POST\" action=\"/friends\"><input name=\"who\" placeholder=\"account name\" autocomplete=\"off\" required>"
            "<button>Send request</button></form></div>";
        html += "<div class=\"card\"><h2>Friends</h2>";
        auto pals = g_store.friends_of(user);
        auto inner = g_store.inner_of(user);
        std::set<std::string> inner_set(inner.begin(), inner.end());
        html += "<p class=\"muted eyebrow\">"+std::to_string(pals.size())+
                (pals.size()==1?" friend":" friends")+" &middot; "+std::to_string(inner_set.size())+" inner</p>";
        if(pals.empty()) html += "<p class=\"empty\">No friends yet. Search a username above, or send a request by exact name.</p>";
        for (auto& other : pals) {
            const bool inn = inner_set.count(other);
            html += "<div class=\"person\"><div class=\"who\">"+portrait_tag(other,"sm")+"<div><a href=\"/u/"+html_escape(other)+"\"><strong>"+html_escape(other)+"</strong></a>"
                    +(inn?" <span class=\"muted\">inner circle</span>":"")+
                    "</div></div><div class=\"row\">"
                    "<a class=\"btn ghost\" href=\"/mail?with="+html_escape(other)+"\">Mail</a>";
            if (inn) {
                html += "<form class=\"inline\" method=\"POST\" action=\"/friends/inner-remove\">"
                        "<input type=\"hidden\" name=\"who\" value=\""+html_escape(other)+"\">"
                        "<button class=\"ghost\" type=\"submit\">From inner</button></form>";
            } else {
                html += "<form class=\"inline\" method=\"POST\" action=\"/friends/inner\">"
                        "<input type=\"hidden\" name=\"who\" value=\""+html_escape(other)+"\">"
                        "<button class=\"ghost\" type=\"submit\">Inner circle</button></form>";
            }
            html += "<form class=\"inline\" method=\"POST\" action=\"/friends/unfriend\" "
                    "onsubmit=\"return confirm('Unfriend this person? You will leave every group you share with them. They stay in those groups.');\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(other)+"\">"
                    "<button class=\"ghost\" type=\"submit\">Unfriend</button></form>"
                    "<form class=\"inline\" method=\"POST\" action=\"/friends/block\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(other)+"\">"
                    "<button class=\"danger\" type=\"submit\">Block</button></form></div></div>";
        }
        html += "<p class=\"muted\">Inner circle is up to twelve people. Everyone else you've accepted is still a friend.</p></div>";
        html += "<div class=\"card\"><h2>Incoming</h2>";
        auto inc = g_store.incoming_requests(user);
        if(inc.empty()) html += "<p class=\"empty\">None.</p>";
        // a face and a link on every request, Accept and Decline side by side (Brandon 2026-11-10: friendlier)
        auto who_html = [](const std::string& n) {
            return "<div class=\"who\">" + portrait_tag(n, "sm") + "<a href=\"/u/" + html_escape(n) + "\"><strong>" +
                   html_escape(n) + "</strong></a></div>";
        };
        for (auto& from : inc) {
            html += "<div class=\"person\">" + who_html(from) + "<div class=\"row\">"
                    "<form class=\"inline\" method=\"POST\" action=\"/friends/accept\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(from)+"\"><button type=\"submit\">Accept</button></form>"
                    "<form class=\"inline\" method=\"POST\" action=\"/friends/decline\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(from)+"\"><button class=\"ghost\" type=\"submit\">Decline</button></form>"
                    "</div></div>";
        }
        html += "</div><div class=\"card\"><h2>Outgoing</h2>"
                "<p class=\"muted\">If someone does not accept, you will not get a notice. That is intentional.</p>";
        auto outg = g_store.outgoing_requests(user);
        if(outg.empty()) html += "<p class=\"empty\">None.</p>";
        for (auto& to : outg) html += "<div class=\"person\">" + who_html(to) + "<span class=\"muted\">waiting</span></div>";
        html += "</div>";
        return pg("People", html);
    }

    if(method=="GET"&&path=="/search") {
        if(!acc) return need_login();
        auto term = trim_ws(qs["q"]);
        if (qs["partial"]=="1")
            return http_raw("200 OK", "text/html; charset=utf-8", people_results_html(user, term),
                "Cache-Control: no-store\r\n");
        std::string html = "<h1>Search</h1><div class=\"card\">"
            "<form method=\"GET\" action=\"/search\"><label>Find a person</label>"
            "<div class=\"row\"><input name=\"q\" id=\"peopleq\" value=\""+html_escape(term)+"\" autocomplete=\"off\">"
            "<button>Search</button></div></form>"
            "<p class=\"muted\">Friends always appear. Everyone else follows their findability. "
            "A miss is a miss &mdash; missing, opted out, and blocked look the same. "
            "Mail is friends-only until they accept.</p>"
            "<div id=\"peoplehits\">"+people_results_html(user, term)+"</div></div>"
            + people_search_script();
        return pg("Search", html);
    }
    if(method=="POST"&&path=="/friends") {
        if(!acc) return need_login();
        auto f=parse_form(body);
        auto who=trim_ws(f["who"]);
        if(valid_username(who) && who!=user && g_store.find_by_username(who)
           && !g_store.is_blocked(user, who) && !g_store.is_blocked(who, user)) {
            g_store.add_friend_request(user, who);
        }
        auto dest = safe_next_path(trim_ws(f["next"]));
        if (dest.empty()) dest = "/friends";
        return http_redir(dest);
    }
    if(method=="POST"&&path=="/friends/accept") {
        if(!acc) return need_login();
        auto who=parse_form(body)["who"];
        g_store.accept_friend_request(who, user);
        if (g_store.incoming_requests(user).empty())
            g_store.mark_notices_read_of_type(user, "friend_request");
        return http_redir("/friends");
    }
    // Decline (2026-11-10): the request just goes away; the sender is not told (same as never answering).
    if(method=="POST"&&path=="/friends/decline") {
        if(!acc) return need_login();
        auto who=trim_ws(parse_form(body)["who"]);
        g_store.decline_friend_request(who, user);
        if (g_store.incoming_requests(user).empty())
            g_store.mark_notices_read_of_type(user, "friend_request");
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/friends/unfriend") {
        if(!acc) return need_login();
        g_store.unfriend(user, parse_form(body)["who"]);
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/friends/block") {
        if(!acc) return need_login();
        auto who=parse_form(body)["who"];
        if (valid_username(who) && who!=user) g_store.block(user, who);
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/friends/inner") {
        if(!acc) return need_login();
        if (!g_store.add_inner(user, parse_form(body)["who"]))
            return http_redir("/friends?err=inner");
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/friends/inner-remove") {
        if(!acc) return need_login();
        g_store.remove_inner(user, parse_form(body)["who"]);
        return http_redir("/friends");
    }
    return std::nullopt;
}

}  // namespace liquidgov::site
