#include "site/features/communities/communities_routes.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/core/http.hpp"
#include "site/core/names_categories.hpp"
#include "site/features/canvas/canvas.hpp"
#include "site/features/feed/composer.hpp"
#include "site/features/feed/post_block.hpp"
#include "site/features/pains/pains.hpp"
#include "site/features/profile/portrait.hpp"
#include "site/ui/wrap.hpp"

namespace liquidgov::site {

std::optional<std::string> communities_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
    if(method=="GET"&&path=="/communities") {
        if(!acc) return need_login();
        std::string err = flash_error(qs);
        std::string html = "<h1>Groups</h1>"+err+
            "<p class=\"lede\">Platform groups. Not AVND localities, and they do not confer standing.</p>";
        auto all = g_store.all_communities();
        std::vector<Community> mine;
        for (auto& c : all) if (g_store.is_member(c.slug, user)) mine.push_back(c);
        if (!mine.empty()) {
            html += "<div class=\"card\"><h2>Yours</h2><ul class=\"plain\">";
            for (auto& c : mine) {
                html += "<li class=\"who\" style=\"margin:.4rem 0\"><div><a href=\"/c/"+html_escape(c.slug)+"\"><strong>"+
                        html_escape(c.name)+"</strong></a> <span class=\"muted\">/"+html_escape(c.slug)+"</span></div></li>";
            }
            html += "</ul></div>";
        }
        auto cities = g_store.cities_in_use();
        if (!cities.empty()) {
            html += "<div class=\"card\"><h2>Cities</h2>"
                    "<p class=\"muted\">Groups that name the same city are listed together, so people there can find each other. "
                    "<a href=\"/map\">See them on the MAINstream map</a>.</p><p>";
            for (const auto& cu : cities) {
                html += "<a class=\"chip\" href=\"/city/"+url_encode(cu.key)+"\" style=\"text-decoration:none\">"+
                        html_escape(cu.name)+" &middot; "+std::to_string(cu.groups)+"</a> ";
            }
            html += "</p></div>";
        }
        html += "<div class=\"card\"><h2>Find a group</h2>"
            "<input id=\"commfilter\" type=\"search\" placeholder=\"Search by name or address\" "
            "oninput=\"var q=this.value.toLowerCase();document.querySelectorAll('.commrow').forEach(function(el){"
            "el.style.display=el.getAttribute('data-q').indexOf(q)>=0?'':'none';});\">";
        if (all.empty()) html += "<p class=\"empty\">None yet. Start one below.</p>";
        else {
            std::map<int, std::map<std::string, std::vector<Community>>> grouped;
            for (auto& c : all) grouped[static_cast<int>(c.category)][c.subcategory].push_back(c);
            for (const auto& ci : kCommunityCategories) {
                auto git = grouped.find(static_cast<int>(ci.value));
                if (git == grouped.end()) continue;
                std::size_t total = 0;
                for (auto& [subcat, list] : git->second) total += list.size();
                html += "<details open><summary>"+std::string(ci.label)+" &middot; "+std::to_string(total)+"</summary>";
                for (auto& [subcat, list] : git->second) {
                    html += "<div style=\"margin:.5rem 0 .5rem 1rem\">"
                            "<p class=\"eyebrow\">"+html_escape(subcat.empty() ? "General" : subcat)+"</p><ul class=\"plain\">";
                    for (auto& c : list) {
                        std::string q = c.name + " " + c.slug + " " + c.about + " " + c.subcategory + " " + c.city;
                        for (char& ch : q) if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
                        const std::string chref = city_href(c.city);
                        html += "<li class=\"commrow\" data-q=\""+html_escape(q)+"\"><a href=\"/c/"+html_escape(c.slug)+"\"><strong>"+
                                html_escape(c.name)+"</strong></a> <span class=\"muted\">/"+html_escape(c.slug)+"</span>"+
                                (chref.empty() ? std::string() : " <a class=\"muted\" href=\""+chref+"\">"+html_escape(c.city)+"</a>")+
                                "<p class=\"muted\">"+html_escape(c.about)+"</p></li>";
                    }
                    html += "</ul></div>";
                }
                html += "</details>";
            }
        }
        // /communities?city=<text> (the city page's "Start a group here")
        // opens the form with the city filled in.
        auto city_qs = qs.find("city");
        const std::string city_prefill = city_qs == qs.end() ? std::string() : clean_city(city_qs->second);
        html += "</div><div class=\"card\"><details"+std::string(city_prefill.empty() ? "" : " open")+"><summary>Start a group</summary>"
            "<form method=\"POST\" action=\"/communities\" style=\"margin-top:.7rem\">"
            "<label>Name</label><input name=\"name\" required maxlength=\"80\" placeholder=\"Neighborhood watch\">"
            "<label>Link</label><input name=\"slug\" required maxlength=\"32\" pattern=\"[A-Za-z0-9._-]+\" placeholder=\"watch\">"
            "<p class=\"muted\" style=\"margin-top:-.3rem\">Becomes /c/watch &mdash; letters, numbers, dots, dashes.</p>"
            "<label>Category</label><select name=\"category\" id=\"catselect\">";
        for (const auto& ci : kCommunityCategories) {
            html += "<option value=\""+std::to_string(static_cast<int>(ci.value))+"\">"+ci.label+"</option>";
        }
        html += "</select>"
            "<label>Subcategory</label>"
            "<input name=\"subcategory\" id=\"subcatinput\" required maxlength=\"60\" list=\"subcatlist\" "
            "placeholder=\"e.g. Neighborhood Watch\" autocomplete=\"off\">"
            "<datalist id=\"subcatlist\"></datalist>"
            "<p class=\"muted\" style=\"margin-top:-.3rem\">Pick an existing one, or type a new one.</p>"
            "<label>City <span class=\"muted\">(optional)</span></label>"
            "<input name=\"city\" maxlength=\"80\" list=\"citylist\" placeholder=\"e.g. Fresno, CA\" autocomplete=\"off\" value=\""+
            html_escape(city_prefill)+"\">"+city_datalist_html()+
            "<p class=\"muted\" style=\"margin-top:-.3rem\">Groups that name the same city are listed together. "
            "Pick an existing spelling if yours is there. A city here is a label, not a government.</p>"
            "<label>About</label><textarea name=\"about\" maxlength=\"2000\" placeholder=\"What this group is for.\"></textarea>"
            "<button>Create</button></form>"
            "<script>(function(){"
            "var SUB="+community_subcats_js()+";"
            "var sel=document.getElementById('catselect'),dl=document.getElementById('subcatlist');"
            "function refresh(){"
            "dl.innerHTML='';"
            "(SUB[sel.value]||[]).forEach(function(s){"
            "var o=document.createElement('option');o.value=s;dl.appendChild(o);"
            "});"
            "}"
            "sel.addEventListener('change',refresh);refresh();"
            "})();</script>"
            "</details></div>";
        return pg("Groups", html);
    }

    if(method=="GET"&&path.rfind("/c/",0)==0&&path.size()>10&&path.compare(path.size()-7,7,"/canvas")==0) {
        if(!acc) return need_login();
        const std::string slug = path.substr(3, path.size()-3-7);
        if (!g_store.community(slug) || !g_store.is_member(slug, user)) return http_redir("/c/"+slug);
        return pg("Compose a post", canvas_tool_page(user, "", false, "/c/"+slug));
    }

    if(method=="GET"&&path.rfind("/c/",0)==0) {
        if(!acc) return need_login();
        const std::string slug = path.substr(3);
        auto c = g_store.community(slug);
        if (!c)
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such group.</p></div>"));
        const bool member = g_store.is_member(slug, user);
        const bool owner = c->owner == user;
        std::string html = "<h1>"+html_escape(c->name)+"</h1>"
            "<p class=\"muted\">"+html_escape(category_label(c->category))+" &middot; "+
            html_escape(c->subcategory.empty() ? "General" : c->subcategory)+
            " · /"+html_escape(c->slug)+" · started by <a href=\"/u/"+html_escape(c->owner)+"\">"+html_escape(c->owner)+"</a>"+
            (city_href(c->city).empty() ? std::string()
                : " · <a href=\""+city_href(c->city)+"\">"+html_escape(c->city)+"</a>")+"</p>";
        html += flash_error(qs);
        html += "<div class=\"card\"><p>"+html_escape(c->about.empty()?"No description yet.":c->about)+"</p>";
        if (!member) {
            html += "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"/join\"><button>Join this group</button></form>";
        } else if (!owner) {
            html += "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"/leave\"><button class=\"ghost\">Leave</button></form>";
        } else {
            html += "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"/delete\" onsubmit=\"return confirm('Delete this group and its posts?');\">"
                    "<button class=\"ghost\">Delete group</button></form>";
            html += "<details style=\"margin-top:.6rem\"><summary>"+
                    std::string(c->city.empty() ? "Add a city" : "Change city")+"</summary>"
                    "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"/city\" style=\"margin-top:.5rem\">"
                    "<div class=\"row\"><input name=\"city\" maxlength=\"80\" list=\"citylist\" placeholder=\"e.g. Fresno, CA\" "
                    "autocomplete=\"off\" value=\""+html_escape(c->city)+"\"><button>Save</button></div>"+city_datalist_html()+
                    "<p class=\"muted\">Groups that name the same city are listed together so people there can find each other. "
                    "Leave it empty to remove the city.</p></form></details>";
        }
        html += "</div>";
        if (member) {
            html += "<div class=\"card\">"
                    "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"\" enctype=\"multipart/form-data\">"
                    "<textarea name=\"text\" maxlength=\"2000\" placeholder=\"Post in "+html_escape(c->name)+"\" "
                    "oninput=\"document.getElementById('cchars').textContent=(2000-this.value.length)+' left'\"></textarea>"
                    + composer_attach_bar("cchars", "/c/" + slug + "/canvas") +
                    "</form></div>";
        }
        auto posts = g_store.posts_in(slug, 80);
        if (posts.empty()) html += "<div class=\"card\"><p class=\"empty\">No posts yet.</p>"
            "<p class=\"muted\">"+(member ? std::string("You're in. Write the first one.") : "Join to post.")+"</p></div>";
        for (const auto& p : posts) html += "<div class=\"card\">"+post_block(p, user)+"</div>";
        html += "<div class=\"card\"><details><summary>Members · "+std::to_string(g_store.members_of(slug).size())+"</summary><ul class=\"plain\">";
        // Add friend beside each member you could ask (Brandon 2026-11-10): not you, a friend, a pending request or a block
        std::set<std::string> asked;
        if (!user.empty()) {
            for (const auto& u : g_store.incoming_requests(user)) asked.insert(u);
            for (const auto& u : g_store.outgoing_requests(user)) asked.insert(u);
        }
        for (auto& n : g_store.members_of(slug)) {
            const bool can_add = !user.empty() && n != user && !asked.count(n) && !g_store.are_friends(user, n) &&
                                 !g_store.is_blocked(user, n) && !g_store.is_blocked(n, user);
            html += "<li class=\"who\" style=\"margin:.35rem 0;align-items:center\">"+portrait_tag(n,"sm")+
                    "<div><a href=\"/u/"+html_escape(n)+"\">"+html_escape(n)+"</a>"+
                    (n==c->owner?" <span class=\"muted\">starter</span>":"")+"</div>";
            if (can_add)
                // same gold Add friend as People you may know (one look everywhere), centred on the row
                html += "<form class=\"inline\" style=\"margin:0 0 0 auto;align-self:center\" method=\"POST\" action=\"/friends\" data-add=\""+html_escape(n)+"\">"
                        "<input type=\"hidden\" name=\"who\" value=\""+html_escape(n)+"\">"
                        "<input type=\"hidden\" name=\"next\" value=\"/c/"+html_escape(slug)+"\">"
                        "<button type=\"submit\">Add friend</button></form>";
            html += "</li>";
        }
        html += "</ul>";
        if (member) {
            html += "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"/invite\" style=\"margin-top:.6rem\">"
                    "<label>Invite by account name</label><div class=\"row\"><input name=\"who\" required placeholder=\"account name\" autocomplete=\"off\"><button>Invite</button></div></form>";
        }
        html += "</details></div>";
        return pg(c->name, html);
    }
    if(method=="POST"&&path=="/communities") {
        if(!acc) return need_login();
        auto f=parse_form(body);
        auto slug=trim_ws(f["slug"]);
        auto name=trim_ws(f["name"]);
        auto about=f["about"]; if(about.size()>2000) about.resize(2000);
        int cat = 10;
        try { cat = std::stoi(f["category"]); } catch (...) { cat = 10; }
        if (cat < 0 || cat > 10) cat = 10;
        auto subcategory = trim_ws(f["subcategory"]);
        if (subcategory.size() > 60) subcategory.resize(60);
        if (subcategory.empty()) return http_redir("/communities?err=subcategory");
        Community c;
        c.slug = slug; c.name = name.empty()?slug:name; c.owner = user; c.about = about; c.created_at = now();
        c.category = static_cast<CommunityCategory>(cat);
        c.subcategory = subcategory;
        c.city = f["city"]; // optional; FileStore tidies it (clean_city) and drops text with no letters
        if (!g_store.create_community(c)) return http_redir("/communities?err=address");
        return http_redir("/c/"+slug);
    }
    if(method=="POST"&&path.rfind("/c/",0)==0) {
        if(!acc) return need_login();
        std::string rest = path.substr(3);
        std::string slug, action;
        auto slash = rest.find('/');
        if (slash==std::string::npos) { slug = rest; action = "post"; }
        else { slug = rest.substr(0,slash); action = rest.substr(slash+1); }
        if (!g_store.community(slug)) return http_redir("/communities");
        if (action=="join") { g_store.join_community(slug, user); return http_redir("/c/"+slug); }
        if (action=="city") {
            // Owner only (set_community_city checks); empty clears it.
            auto f=parse_form(body);
            if (!g_store.set_community_city(slug, user, f["city"])) return http_redir("/c/"+slug+"?err=city");
            return http_redir("/c/"+slug);
        }
        if (action=="leave") { g_store.leave_community(slug, user); return http_redir("/c/"+slug); }
        if (action=="delete") { g_store.delete_community(slug, user); return http_redir("/communities"); }
        if (action=="invite") {
            auto who=trim_ws(parse_form(body)["who"]);
            if (!g_store.invite_community(slug, user, who)) return http_redir("/c/"+slug+"?err=invite");
            return http_redir("/c/"+slug);
        }
        if (action=="post") {
            if (!g_store.is_member(slug, user)) return http_redir("/c/"+slug);
            auto hdr_end = req.find("\r\n\r\n");
            std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
            auto c = read_composer_post(hdrs, body);
            if (!c.ok) return http_redir("/c/"+slug+"?err="+c.err);
            auto text = trim_ws(c.text);
            if (!text.empty() || !c.attachment_blob_id.empty()) {
                const auto gid = g_store.add_post(user, text, slug, "", 0, c.attachment_blob_id, c.attachment_mime);
                if (gid) notice_pains(user, text, "gpost:" + std::to_string(gid));
            }
            return http_redir("/c/"+slug);
        }
        return http_redir("/c/"+slug);
    }
    return std::nullopt;
}

}  // namespace liquidgov::site
