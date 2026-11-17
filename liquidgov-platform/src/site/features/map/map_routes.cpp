#include "site/features/map/map_routes.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/core/http.hpp"
#include "site/core/names_categories.hpp"
#include "site/core/text.hpp"
#include "site/features/feed/composer.hpp"
#include "site/features/map/map_graph.hpp"
#include "site/features/map/map_page.hpp"
#include "site/features/map/map_rules.hpp"
#include "site/features/map/map_search.hpp"
#include "site/features/map/map_svg.hpp"
#include "site/features/outside/outside.hpp"
#include "site/features/pains/pains.hpp"
#include "site/features/profile/profile_css.hpp"
#include "site/features/profile/portrait.hpp"
#include "site/features/proposals/proposals.hpp"
#include "site/ui/assets.hpp"
#include "site/ui/wrap.hpp"

namespace liquidgov::site {

namespace {
// Spatial step 10: instance id -> the admin's CSS. Only touched inside route handlers, which run under g_mu.
std::map<std::string, std::string>& instance_css() { static std::map<std::string, std::string> m; return m; }
// the node's JSON with its instance CSS added as "css" (checked again here: stored text is never trusted blind)
std::string with_instance_css(std::string node_json, const std::string& id) {
    auto it = instance_css().find(id);
    if (it == instance_css().end() || node_json.empty() || node_json.back() != '}' || !sanitize_author_css(it->second).ok) return node_json;
    node_json.pop_back();
    return node_json + ",\"css\":\"" + json_escape(it->second) + "\"}";
}
}  // namespace

std::optional<std::string> map_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
    // /city/<key>: every group that names this city (city proposals build
    // plan, slice 1). A city is a label people chose, not an AVND locality.
    // Map view (slice 6): /map is the MAINstream; /city/<key>/map is one city's branch.
    if(method=="GET"&&path=="/map") {
        if(!acc) return need_login();
        // Interactive map (assets/lgmap.js): click a box and its connections
        // grow out of it; its page opens in the field on the right, which can
        // fill the window; MAINstream shrinks it back. <noscript> keeps the
        // plain drawing for browsers without JavaScript.
        std::string focus;
        if (qs.count("focus") && map_gnode(qs.at("focus"), user)) focus = qs.at("focus");
        return pg("Map", map_page_body(user, focus));
    }
    // Your own wishes and pain points. Owner only; nobody else has a way in.
    if(method=="GET"&&path=="/problems") {
        if(!acc) return need_login();
        const bool on = g_store.map_prefs_of(user).pains;
        std::string html = "<h1>Your problems and solutions</h1>"
            "<p class=\"lede\">Things you said you wish for or are tired of, so you can find what and who could help. "
            "Only you can see this page. Flow puts one kept subject on the map for people who can see you. "
            "Flowing it also shows you who else Flowed that same subject.</p>";
        const bool looked_none = on && qs.count("looked") && qs.at("looked") == "none";
        if (!on) html += "<div class=\"card\"><p>This is turned off, so nothing is being noticed. "
                         "<a href=\"/settings#map-privacy\">Turn it on in Settings</a>.</p></div>";
        else {
            html += "<p class=\"muted\">It reads your new posts, proposals, proposal comments, and messages you sent. "
                    "<a href=\"/settings#map-privacy\">Change this</a></p>";
            html += "<form method=\"POST\" action=\"/problems/look\">"
                    "<button type=\"submit\">Look at what I've written</button></form>";
            html += "<p class=\"muted\">This reads posts, proposals, proposal comments, and messages you already wrote. "
                    "Each one is offered here as Keep or Forget before it is a box. "
                    "Friends posts, group posts, and messages stay on your map only. "
                    "It does not read the other person's messages, or reposts.</p>";
            if (looked_none) html += "<p class=\"muted\">Nothing new in what you've written.</p>";
        }
        auto all = g_store.pains_of(user);
        auto source_link = [](const PainPoint& p) -> std::string {
            auto post_href = [](const std::string& source, std::size_t skip, const char* label) {
                return " <a class=\"muted\" href=\"/post/" + html_escape(source.substr(skip)) + "\">" + label + "</a>";
            };
            if (p.source.rfind("post:", 0) == 0) return post_href(p.source, 5, "from your post");
            if (p.source.rfind("fpost:", 0) == 0) return post_href(p.source, 6, "from a friends post");
            if (p.source.rfind("gpost:", 0) == 0) return post_href(p.source, 6, "from a group post");
            if (p.source.rfind("prop:", 0) == 0) return " <a class=\"muted\" href=\"/proposals?id=" + html_escape(p.source.substr(5)) + "\">from your proposal</a>";
            if (p.source.rfind("mail:", 0) == 0) return " <span class=\"muted\">from a message you sent</span>";
            if (p.source.rfind("gmsg:", 0) == 0) return " <span class=\"muted\">from a group message you sent</span>";
            return " <span class=\"muted\">from your comment</span>";
        };
        auto label = [](const PainPoint& p) { return std::string(p.kind == "wish" ? "Want" : "Problem"); };
        auto button = [](const char* action, std::uint64_t id, const char* text, bool ghost) {
            return std::string("<form method=\"POST\" action=\"/problems/") + action + "\" style=\"display:inline\">"
                   "<input type=\"hidden\" name=\"id\" value=\"" + std::to_string(id) + "\"><button" +
                   (ghost ? " class=\"ghost\"" : "") + ">" + text + "</button></form> ";
        };
        std::string sugg, kept;
        for (const auto& p : all) {
            if (!p.kept) {
                sugg += "<li style=\"margin:.6rem 0\"><span class=\"eyebrow\">" + label(p) + "</span> <strong>" +
                        html_escape(p.phrase) + "</strong>" + source_link(p) + "<div style=\"margin-top:.3rem\">" +
                        button("keep", p.id, "Keep", false) + button("forget", p.id, "Forget", true) + "</div></li>";
                continue;
            }
            const auto m = pain_matches(p);
            std::string body;
            if (!m.depts.empty()) {
                body += "<p class=\"muted\" style=\"margin:.4rem 0 .1rem\">Who could act</p><p>";
                for (const auto& [key, tag] : m.depts)
                    body += "<a class=\"chip\" href=\"/proposals?city=" + url_encode(key) + "\">" + html_escape(tag) + "</a> ";
                body += "</p>";
            }
            if (!m.props.empty()) {
                body += "<p class=\"muted\" style=\"margin:.4rem 0 .1rem\">Proposals about it</p><ul class=\"plain\">";
                for (const auto& pr : m.props)
                    body += "<li><a href=\"/proposals?id=" + std::to_string(pr.id) + "\">" + html_escape(pr.title) + "</a></li>";
                body += "</ul>";
            }
            if (!m.groups.empty()) {
                body += "<p class=\"muted\" style=\"margin:.4rem 0 .1rem\">Groups working on it</p><p>";
                for (const auto& c : m.groups)
                    body += "<a class=\"chip\" href=\"/c/" + url_encode(c.slug) + "\">" + html_escape(c.name) + "</a> ";
                body += "</p>";
            }
            if (body.empty()) body = "<p class=\"muted\">No possible solutions on the site yet. You could start one.</p>";
            else body = "<p class=\"eyebrow\" style=\"margin-top:.6rem\">Possible solutions</p>" + body;
            const std::string start = "/proposals?title=" + url_encode(p.phrase) +
                                      (m.city_key.empty() ? std::string() : "&city=" + url_encode(m.city_key)) + "#new";
            const bool shareable = pain_can_flow(p.source);
            const std::string flow_note = !shareable
                ? std::string("This stays on your map. Other people do not see it.")
                : p.flowed
                    ? std::string("This subject is on the map for people who can see you.")
                    : std::string("Flow puts this subject on the map for people who can see you.");
            const std::string flow_btn = !shareable ? std::string()
                : p.flowed ? button("still", p.id, "Still", true)
                           : button("flow", p.id, "Flow", false);
            const std::string also = also_flowed_html(subject_matches(user, p));
            kept += "<div class=\"card\" id=\"pain" + std::to_string(p.id) + "\"><span class=\"eyebrow\">" + label(p) +
                    "</span><h3 style=\"margin:.1rem 0\">" + html_escape(p.phrase) + "</h3>" + source_link(p) + body +
                    "<p style=\"margin-top:.6rem\"><a href=\"" + html_escape(start) + "\">Start a proposal about this</a> &middot; "
                    "<a href=\"/map?focus=pain:" + std::to_string(p.id) + "\">See it on your map</a></p>" +
                    "<p class=\"muted\">" + flow_note + "</p>" + also +
                    flow_btn + button("forget", p.id, "Forget", true) + "</div>";
        }
        if (!sugg.empty())
            html += "<div class=\"card\"><h2>Noticed</h2><p class=\"muted\">Keep the ones that are real. "
                    "Ones you don't keep disappear after 30 days.</p><ul class=\"plain\">" + sugg + "</ul></div>";
        if (!kept.empty()) html += "<h2>Kept</h2>" + kept;
        if (all.empty() && on && !looked_none)
            html += "<div class=\"card\"><p class=\"empty\">Nothing yet.</p><p class=\"muted\">When you write something like "
                    "&ldquo;I wish the buses ran later&rdquo; in a public post, a proposal or a comment, it shows up here.</p></div>";
        return pg("Problems and solutions", html);
    }
    if(method=="POST"&&(path=="/problems/keep"||path=="/problems/forget"||path=="/problems/flow"||path=="/problems/still")) {
        if(!acc) return need_login();
        auto f = parse_form(body);
        std::uint64_t id = 0;
        const std::string raw = f["id"];
        if (!raw.empty() && raw.size() <= 19 && raw.find_first_not_of("0123456789") == std::string::npos)
            try { id = std::stoull(raw); } catch (...) { id = 0; }
        // Owner-only checks live in the store; someone else's id just does nothing.
        if (id && path == "/problems/keep") g_store.keep_pain(user, id);
        else if (id && path == "/problems/forget") g_store.forget_pain(user, id);
        else if (id && path == "/problems/flow") {
            auto owned = g_store.pain_by_id(id);
            if (owned && owned->owner == user && pain_can_flow(owned->source)) g_store.flow_pain(user, id, true);
        }
        else if (id && path == "/problems/still") g_store.flow_pain(user, id, false);
        const bool stay = id && path != "/problems/forget";
        return http_redir(stay ? "/problems#pain" + std::to_string(id) : std::string("/problems"));
    }
    // Older public writing, only when this person asks. Same Keep/Forget
    // suggestions as a new post. A switch that is off reads nothing.
    if(method=="POST"&&path=="/problems/look") {
        if(!acc) return need_login();
        const int added = look_public_words(user);
        if (added < 0 || added > 0) return http_redir("/problems");
        return http_redir("/problems?looked=none");
    }
    // One flowed subject. The owner is sent back to their private list.
    // Anyone else sees the phrase and who flowed it, and only when that
    // person is visible to them. A Still box looks the same as a missing one.
    if(method=="GET"&&path.rfind("/pain/",0)==0&&path.size()>6&&path.find('/',6)==std::string::npos) {
        if(!acc) return need_login();
        std::uint64_t id = 0;
        const std::string digits = path.substr(6);
        if (digits.size() <= 19 && digits.find_first_not_of("0123456789") == std::string::npos)
            try { id = std::stoull(digits); } catch (...) { id = 0; }
        auto p = id ? g_store.pain_by_id(id) : std::nullopt;
        auto missing = [&]() {
            return http_code("404 Not Found", wrap("Not found", user, unread,
                "<div class=\"card\"><p>No such subject.</p></div>"));
        };
        if (!p) return missing();
        if (p->owner == user) return http_redir("/problems#pain" + std::to_string(p->id));
        if (!p->kept || !p->flowed || !map_person_ok(p->owner, user)) return missing();
        const std::string eyebrow = p->kind == "wish" ? std::string("Want") : std::string("Problem");
        std::string html = "<div class=\"card\"><span class=\"eyebrow\">" + eyebrow +
            "</span><h1 style=\"margin:.1rem 0\">" + html_escape(p->phrase) + "</h1>"
            "<p>Flowed by <a href=\"/u/" + html_escape(p->owner) + "\">" + html_escape(p->owner) + "</a>.</p>";
        html += also_flowed_html(subject_matches(user, *p));
        html += "<p class=\"muted\">This is the subject they chose to show.</p>"
            "<p><a href=\"/map?focus=pain:" + std::to_string(p->id) + "\">See it on the map</a></p></div>";
        return pg(p->phrase, html);
    }
    // Spatial step 10: an admin's CSS for an instance (a map box), shown only while a droplet is Inside it. One operator-only
    // edit path: id = a box on the map, css = the CSS ("" clears it). It must pass the same filter as profile CSS
    // (sanitize_author_css: no scripts or loads, no escapes, braces balanced) and is checked again on every delivery.
    // Kept in memory (gone at a restart): a stored record would be a store-format change.
    if(method=="POST"&&path=="/map/instance-css") {
        if(!acc) return http_raw("401 Unauthorized","application/json; charset=utf-8","{\"error\":\"login\"}");
        if(!is_operator(user)) return http_raw("403 Forbidden","application/json; charset=utf-8","{\"error\":\"operators only\"}");
        auto f = parse_form(body);
        const std::string id = f["id"], css = f["css"];
        if (id.empty() || id.size() > 64 || !map_gnode(id, user))
            return http_raw("404 Not Found","application/json; charset=utf-8","{\"error\":\"not a box on the map\"}");
        if (css.size() > 4000) return http_raw("400 Bad Request","application/json; charset=utf-8","{\"error\":\"too long\"}");
        if (css.empty()) { instance_css().erase(id); return http_raw("200 OK","application/json; charset=utf-8","{\"ok\":true,\"css\":false}"); }
        const auto check = sanitize_author_css(css);
        if (!check.ok) return http_raw("400 Bad Request","application/json; charset=utf-8","{\"error\":\"" + json_escape(check.code) + "\"}");
        instance_css()[id] = css;
        return http_raw("200 OK","application/json; charset=utf-8","{\"ok\":true,\"css\":true}");
    }
    // One box and its connections, as JSON, for assets/lgmap.js.
    // Search on the map (Brandon 2026-11-15: "there should be a search bar at the top where you can search for stuff"):
    // then words in posts, comments and your messages, and dates and times (map_search.cpp). tz: the searcher's offset
    // from UTC in minutes east, so "yesterday" and "3pm" are their own.
    if(method=="GET"&&path=="/map/search") {
        if(!acc) return http_raw("401 Unauthorized","application/json; charset=utf-8","{\"error\":\"login\"}");
        int tz = 0;
        try { tz = qs.count("tz") ? std::stoi(qs.at("tz")) : 0; } catch (...) { tz = 0; }
        return http_raw("200 OK","application/json; charset=utf-8",
                        map_search_json(user, qs.count("q") ? qs.at("q") : std::string(), tz), "Cache-Control: no-store\r\n");
    }
    if(method=="GET"&&path=="/map/node") {
        if(!acc) return http_raw("401 Unauthorized","application/json; charset=utf-8","{\"error\":\"login\"}");
        const std::string id = qs.count("n") ? qs.at("n") : std::string("root");
        auto n = map_gnode(id, user);
        if (!n) return http_raw("404 Not Found","application/json; charset=utf-8","{\"error\":\"not found\"}");
        const MapCtx ctx = map_ctx(user); // relevance colours, for this viewer only
        map_add_sub(*n, user);
        outside_fill_interest(*n);   // an interest of yours with a Wikipedia article carries its short summary (outside.hpp)
        std::string out = "{\"node\":" + with_instance_css(map_node_json(*n, map_relevance(*n, ctx)), id) + ",\"links\":[";
        bool first = true;
        auto linked = map_links(id, user);
        outside_fill_interests(linked);   // your interest boxes carry their Wikipedia summaries when the list is drawn
        for (auto l : linked) {
            if (!first) out += ",";
            first = false;
            map_add_sub(l.to, user);
            out += "{\"node\":" + with_instance_css(map_node_json(l.to, map_relevance(l.to, ctx)), l.to.id) + ",\"edge\":\"" + json_escape(l.edge) + "\"}";
        }
        out += "]}";
        return http_raw("200 OK","application/json; charset=utf-8", out, "Cache-Control: no-store\r\n");
    }
    if(method=="GET"&&path.rfind("/city/",0)==0&&path.size()>10&&path.compare(path.size()-4,4,"/map")==0) {
        if(!acc) return need_login();
        const std::string raw = url_decode(path.substr(6, path.size()-6-4));
        const std::string key = city_key(raw);
        if (key.empty()) return http_redir("/map");
        if (key != raw) return http_redir("/city/"+url_encode(key)+"/map");
        auto groups = g_store.communities_in_city(key);
        if (groups.empty() && g_store.proposals_in_city(key).empty())
            return http_code("404 Not Found", wrap("No map here yet", user, unread,
                "<h1>Nothing names this city yet</h1><div class=\"card\"><p><a href=\"/map\">MAINstream</a></p></div>"));
        std::string name = groups.empty() ? key : groups.front().city;
        for (const auto& cu : g_store.cities_in_use()) if (cu.key == key) { name = cu.name; break; }
        std::string html = "<p class=\"muted\"><a href=\"/map\">MAINstream</a> &rsaquo; "+html_escape(name)+"</p>"
            "<h1>"+html_escape(name)+"</h1>"
            "<p class=\"lede\">Point at a box to light up its lines. Click it to open it. "
            "<a href=\"/city/"+url_encode(key)+"\">List view</a></p>"
            + city_map_svg(key, name, user) + kCityMapStyle +
            "<p class=\"cmaplegend\"><span class=\"lg-in\">in this city</span><span class=\"lg-member\">member of</span>"
            "<span class=\"lg-touches\">touches</span><span class=\"lg-wrote\">wrote</span>"
            "<span class=\"lg-commented\">commented</span></p>"
            "<p class=\"muted\">Only people who chose to be listed here appear. A city on LiquidGov is a label, not a government.</p>";
        return pg(name+" map", html);
    }
    if(method=="GET"&&path=="/city") return http_redir("/communities"); // the city list lives on Groups
    if(method=="GET"&&path.rfind("/city/",0)==0) {
        if(!acc) return need_login();
        const std::string raw = url_decode(path.substr(6));
        const std::string key = city_key(raw);
        if (key.empty()) return http_redir("/communities");
        if (key != raw) return http_redir("/city/"+url_encode(key)); // /city/Fresno,%20CA -> /city/fresno-ca
        auto groups = g_store.communities_in_city(key);
        if (groups.empty()) {
            // Proposals and tags for a city are kept while no group names it
            // (Brandon 2026-11-06), so point to them instead of hiding them.
            const auto kept = g_store.proposals_in_city(key).size();
            std::string kept_html;
            if (kept > 0) {
                kept_html = "<p>"+std::to_string(kept)+(kept==1?" proposal is":" proposals are")+
                    " still about this city. <a href=\"/proposals?city="+url_encode(key)+"\">See "+
                    (kept==1?"it":"them")+"</a>. They show here again once a group names the city.</p>";
            }
            return http_code(kept > 0 ? "200 OK" : "404 Not Found", wrap("No groups here yet", user, unread,
                "<h1>No groups name this city yet</h1><div class=\"card\">"
                "<p>Anyone can start one. Give it a city when you do, and it shows up here.</p>"+kept_html+
                "<p><a href=\"/communities\">Groups</a></p></div>"));
        }
        std::string name = groups.front().city;
        for (const auto& cu : g_store.cities_in_use()) if (cu.key == key) { name = cu.name; break; }
        std::set<std::string> people;
        for (const auto& c : groups) for (const auto& m : g_store.members_of(c.slug)) people.insert(m);
        std::string html = "<h1>"+html_escape(name)+"</h1>"
            "<p class=\"lede\">People here started these groups. A city on LiquidGov is a label, not a government.</p>"
            "<p><a href=\"/city/"+url_encode(key)+"/map\">Map view</a> &middot; <a href=\"/map\">MAINstream</a></p>"
            + flash_error(qs) +
            "<p class=\"muted\">"+std::to_string(groups.size())+(groups.size()==1?" group":" groups")+" &middot; "+
            std::to_string(people.size())+(people.size()==1?" person":" people")+" in them</p>"
            "<div class=\"card\"><ul class=\"plain\">";
        for (const auto& c : groups) {
            const auto members = g_store.members_of(c.slug).size();
            html += "<li style=\"margin:.6rem 0\"><a href=\"/c/"+html_escape(c.slug)+"\"><strong>"+html_escape(c.name)+"</strong></a> "
                    "<span class=\"muted\">/"+html_escape(c.slug)+" &middot; "+html_escape(category_label(c.category))+
                    (c.subcategory.empty() ? std::string() : " &middot; "+html_escape(c.subcategory))+" &middot; "+
                    std::to_string(members)+(members==1?" member":" members")+
                    (g_store.is_member(c.slug, user) ? " &middot; you're in" : "")+"</span>"+
                    (c.about.empty() ? std::string() : "<p class=\"muted\">"+html_escape(c.about)+"</p>")+"</li>";
        }
        html += "</ul></div>";
        {
            const auto tags = g_store.city_tags(key);
            bool viewer_in_city = false;
            for (const auto& c : groups) if (g_store.is_member(c.slug, user)) { viewer_in_city = true; break; }
            html += "<div class=\"card\"><h2>Departments and councils</h2>"
                    "<p class=\"muted\">Named by people in this city's groups. A proposal can say which ones it touches. "
                    "Nothing here is sent to the city.</p>";
            if (viewer_in_city) {
                html += "<form method=\"POST\" action=\"/city/"+url_encode(key)+"/tag\">"
                        "<label>Add one</label><div class=\"row\">"
                        "<input name=\"tag\" maxlength=\"80\" required placeholder=\"Parks, city council\">"
                        "<button>Add</button></div></form>";
            } else {
                html += "<p class=\"muted\">Join a group in this city to add one.</p>";
            }
            if (tags.empty()) html += "<p class=\"empty\">None named yet.</p>";
            else {
                html += "<ul class=\"plain\">";
                for (const auto& tag : tags) html += "<li>"+html_escape(tag)+"</li>";
                html += "</ul>";
            }
            html += "</div>";

            auto city_props = g_store.proposals_in_city(key);
            std::reverse(city_props.begin(), city_props.end());
            const std::string back = "/city/"+url_encode(key);
            html += "<div class=\"card\"><h2>Proposals</h2>"
                    "<p class=\"muted\">Unofficial straw poll. Discussion only. Nothing is sent to the city.</p>";
            if (city_props.empty()) html += "<p class=\"empty\">None for this city yet.</p>";
            else {
                auto carries = [](const Proposal& p, const std::string& tag) {
                    for (const auto& have : p.tags) if (have == tag) return true;
                    return false;
                };
                for (const auto& tag : tags) {
                    std::vector<Proposal> bucket;
                    for (const auto& p : city_props) if (carries(p, tag)) bucket.push_back(p);
                    if (bucket.empty()) continue;
                    html += "<h3>"+html_escape(tag)+"</h3>";
                    for (const auto& p : bucket) html += proposal_block(p, user, back);
                }
                std::vector<Proposal> untagged;
                for (const auto& p : city_props) if (p.tags.empty()) untagged.push_back(p);
                if (!untagged.empty()) {
                    html += "<h3>Not tagged</h3>";
                    for (const auto& p : untagged) html += proposal_block(p, user, back);
                }
            }
            html += "<p><a href=\"/proposals?city="+url_encode(key)+"\">Open a proposal for this city</a></p></div>";
        }
        html += "<div class=\"card\"><h2>People here</h2>"
                "<p class=\"muted\">People in these groups who chose to be listed. Nobody is listed unless they ask.</p>";
        bool viewer_in = false;
        for (const auto& c : groups) if (g_store.is_member(c.slug, user)) { viewer_in = true; break; }
        if (viewer_in) {
            const bool listed = g_store.is_city_listed(key, user);
            html += "<form method=\"POST\" action=\"/city/"+url_encode(key)+"/list\" style=\"margin:.4rem 0\">"
                    "<input type=\"hidden\" name=\"on\" value=\""+std::string(listed ? "0" : "1")+"\">"
                    "<button>"+std::string(listed ? "Stop listing me" : "List me here")+"</button></form>";
        } else {
            html += "<p class=\"muted\">Join a group in this city to be listed here.</p>";
        }
        auto listed = g_store.people_listed_in_city(key);
        if (listed.empty()) html += "<p class=\"empty\">Nobody has chosen to be listed yet.</p>";
        else {
            html += "<ul class=\"plain\">";
            for (const auto& name : listed) {
                if (name != user && (g_store.is_blocked(user, name) || g_store.is_blocked(name, user))) continue;
                html += "<li class=\"who\" style=\"margin:.4rem 0\">"+portrait_tag(name, "sm")+
                        "<div><a href=\"/u/"+html_escape(name)+"\">"+html_escape(name)+"</a>";
                if (name == user) html += " <span class=\"muted\">you</span>";
                else if (g_store.are_friends(user, name)) html += " <span class=\"muted\">Friends</span>";
                else {
                    html += "<form class=\"inline\" method=\"POST\" action=\"/friends\">"
                            "<input type=\"hidden\" name=\"who\" value=\""+html_escape(name)+"\">"
                            "<input type=\"hidden\" name=\"next\" value=\""+html_escape("/city/"+url_encode(key))+"\">"
                            "<button type=\"submit\">Add friend</button></form>";
                }
                html += "</div></li>";
            }
            html += "</ul>";
        }
        html += "</div><div class=\"card\"><p><a href=\"/communities?city="+url_encode(name)+"\">Start a group in "+
                html_escape(name)+"</a></p><p class=\"muted\">Anyone can start one. There's no minimum size.</p></div>";
        return pg(name, html);
    }

    if (method=="POST" && path.rfind("/city/", 0)==0 && path.size() > 11
        && path.compare(path.size()-5, 5, "/list")==0) {
        if (!acc) return need_login();
        const std::string raw = url_decode(path.substr(6, path.size() - 6 - 5));
        const std::string key = city_key(raw);
        if (key.empty()) return http_redir("/communities");
        const bool on = parse_form(body)["on"] != "0";
        if (!g_store.set_city_listed(key, user, on))
            return http_redir("/city/"+url_encode(key)+"?err=list");
        return http_redir("/city/"+url_encode(key));
    }

    if (method=="POST" && path.rfind("/city/", 0)==0 && path.size() > 10
        && path.compare(path.size()-4, 4, "/tag")==0) {
        if (!acc) return need_login();
        const std::string raw = url_decode(path.substr(6, path.size() - 6 - 4));
        const std::string key = city_key(raw);
        if (key.empty()) return http_redir("/communities");
        const std::string dest = "/city/"+url_encode(key);
        if (!g_store.in_city(key, user)) return http_redir(dest+"?err=ctag");
        if (!g_store.add_city_tag(key, user, parse_form(body)["tag"]))
            return http_redir(dest+"?err=ctagname");
        return http_redir(dest);
    }
    return std::nullopt;
}

}  // namespace liquidgov::site
