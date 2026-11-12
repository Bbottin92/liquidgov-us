#include "site/features/settings/settings_routes.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/core/http.hpp"
#include "site/features/feed/composer.hpp"
#include "site/ui/favicon_cookies.hpp"

namespace liquidgov::site {

std::optional<std::string> settings_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
    if(method=="GET"&&path=="/settings/password") {
        if(!acc) return need_login();
        // Only this page has password fields (CODE-SCAN-UX B1). The read-only account field gives the password manager
        // the username half of the pair, so it saves and fills here and stops looking elsewhere.
        std::string html = "<h1>Change password</h1>"
            "<p class=\"muted\"><a href=\"/settings\">&lsaquo; Settings</a></p>" + flash_error(qs) +
            "<div class=\"card\"><form method=\"POST\" action=\"/settings/password\">"
            "<label>Account</label><input type=\"text\" name=\"username\" autocomplete=\"username\" value=\""+html_escape(user)+"\" readonly>"
            "<label>Current password</label><input type=\"password\" name=\"current\" required autocomplete=\"current-password\">"
            "<label>New password</label><input type=\"password\" name=\"next\" required autocomplete=\"new-password\">"
            "<button>Change password</button></form></div>";
        return pg("Change password", html);
    }
    if(method=="GET"&&path=="/settings") {
        if(!acc) return need_login();
        auto p = g_store.prefs_of(user);
        auto find_sel = [&](Findability f){ return p.findability==f?" checked":""; };
        std::string html = "<h1>Settings</h1>"
            "<p class=\"muted\">Account, privacy, data, and notifications.</p>"
            // jump links (sweep 2026-11-09): nine sections were one long scroll on a phone
            "<div class=\"setidx\" role=\"navigation\" aria-label=\"Settings sections\"><a href=\"#security\">Security</a><a href=\"#sessions\">Sessions</a><a href=\"#map\">Map and connections</a><a href=\"#privacy\">Privacy</a><a href=\"#blocks\">Block list</a><a href=\"#jukebox\">Jukebox</a><a href=\"#identity\">Identity</a><a href=\"#data\">Data</a><a href=\"#notifications\">Notifications</a></div>";  // a div: the phone CSS hides every <nav> (nav{display:none} is the mobile menu)
        if (qs["ok"]=="password") html += "<p class=\"ok\">Password changed.</p>";
        html += flash_error(qs);

        // Change password is its own page (CODE-SCAN-UX B1, Brief 4 item 9): a password field anywhere on this long
        // page made iOS/Chrome treat all of it as a login form and fill saved credentials into the block, delete and
        // other boxes further down.
        html += "<div class=\"card\"><h2 id=\"security\">Security</h2>"
            "<p><a class=\"btn ghost\" href=\"/settings/password\">Change password</a></p><h2 id=\"sessions\">Sessions</h2>";
        auto all = g_store.sessions_for(acc->id);
        int shown = 0;
        for (const auto& s : all) {
            if (s.revoked) continue;
            if (s.expires_at <= now()) continue;
            ++shown;
            const bool current = s.id == sid;
            html += "<div class=\"sess\"><div><strong>"+html_escape(s.device_label.empty()?"web":s.device_label)+"</strong>"
                    +(current?" <span class=\"muted\">this device</span>":"")+
                    "<div class=\"muted\">Started "+html_escape(fmt_time(s.created_at))+
                    " · last seen "+html_escape(fmt_time(s.last_seen))+"</div></div>";
            if (!current) {
                html += "<form method=\"POST\" action=\"/settings/revoke\">"
                        "<input type=\"hidden\" name=\"sid\" value=\""+html_escape(s.id)+"\">"
                        "<button class=\"danger\" type=\"submit\">Revoke</button></form>";
            }
            html += "</div>";
        }
        if (!shown) html += "<p class=\"muted\">None.</p>";
        html += "<form method=\"POST\" action=\"/settings/logout-all\" style=\"margin-top:1rem\">"
                "<button class=\"danger\" type=\"submit\">Log out other devices</button></form></div>";

        {
            // Map and connections (slice 6): what /map may show about you.
            const auto mp = g_store.map_prefs_of(user);
            auto box = [](const char* name, bool on, const char* label, const char* help) {
                return std::string("<label class=\"row\" style=\"gap:.5rem;align-items:flex-start;margin:.45rem 0\">"
                       "<input type=\"checkbox\" name=\"") + name + "\" value=\"1\"" + (on ? " checked" : "") + "> <span><strong>" +
                       label + "</strong><br><span class=\"muted\">" + help + "</span></span></label>";
            };
            html += "<div class=\"card\" id=\"map-privacy\"><h2 id=\"map\">Map and connections</h2>"
                    "<p class=\"muted\">The <a href=\"/map\">Map</a> connects people, groups, cities and proposals so you can find "
                    "the people worth talking to. Choose what it shows about you. You always see your own.</p>"
                    "<form method=\"POST\" action=\"/settings/map\">"
                    + box("show", mp.show, "Show me on the map", "Off hides you everywhere on the map, even in groups and cities.")
                    + box("groups", mp.groups, "Show the groups I'm in", "Lines from you to your groups.")
                    + box("posts", mp.posts, "Show my posts", "Posts others can already see, as boxes on the map.")
                    + box("comments", mp.comments, "Show what I commented on", "Lines from you to proposals you commented on.")
                    + box("friends", mp.friends, "Show my friends", "Off by default. A friendship shows to others only when both of you turn this on.")
                    + box("pains", mp.pains, "Notice my problems and wants",
                          "Reads your new posts, proposals, proposal comments, and messages you sent for phrases like "
                          "&ldquo;I wish&hellip;&rdquo; or &ldquo;I hate when&hellip;&rdquo;, and suggests them to you. "
                          "Friends posts, group posts, and messages stay on your map only. The other person's messages are not read. "
                          "On your problems page, Look at what I've written reads older writing of those same kinds when you ask. "
                          "Only you see the private ones. Turning this off deletes everything it found. "
                          "Note: the site's operator can read what is stored on the server.")
                    + "<p class=\"muted\"><a href=\"/problems\">See your problems and solutions</a></p>"
                    + "<button>Save map settings</button></form></div>";
        }
        html += std::string("<div class=\"card\"><h2 id=\"privacy\">Privacy</h2><form method=\"POST\" action=\"/settings/privacy\">")
            + "<label>Who can find you</label>"
            + "<p class=\"muted\">One axis, three states. Each level includes the one above it.</p>"
            + "<p><label><input type=\"radio\" name=\"findability\" value=\"0\"" + find_sel(Findability::FriendsOnly) + "> Friends only</label>"
            + "<br><span class=\"muted\">Only people who are already your friends can find you. Strangers can't search you up at all.</span></p>"
            + "<p><label><input type=\"radio\" name=\"findability\" value=\"1\"" + find_sel(Findability::ExactMatch) + "> Exact match</label>"
            + "<br><span class=\"muted\">Anyone who types your exact username can find you, but you won't show up as they type.</span></p>"
            + "<p><label><input type=\"radio\" name=\"findability\" value=\"2\"" + find_sel(Findability::Typeahead) + "> Typeahead</label>"
            + "<br><span class=\"muted\">Anyone can find you, including as a partial match while they're still typing.</span></p>"
            + "<p><label><input type=\"checkbox\" name=\"group_invite_confirm\" value=\"1\"" + (p.group_invite_confirm?" checked":"") + "> "
            + "Don't add me to groups without asking</label></p>"
            + "<p class=\"muted\">Direct messages are friends-only until we can tell who is a minor without a checkbox. "
            + "Unfriending or blocking leaves every group conversation you share with that person. "
            + "Messages on this copy are stored so the operator can read them.</p>"
            + "<label>Visibility register</label>"
            + "<p class=\"muted\">Each level admits every level after it too: public sees everything members do, "
            + "members see everything friends do, friends see everything only-me hides from everyone else.</p>";
        auto vis_select = [&](const char* field, Visibility current) {
            auto opt = [&](Visibility v, const char* label) {
                return std::string("<option value=\"") + std::to_string(static_cast<int>(v)) + "\""
                    + (current == v ? " selected" : "") + ">" + label + "</option>";
            };
            return std::string("<select name=\"") + field + "\">"
                + opt(Visibility::Public, "Public")
                + opt(Visibility::Members, "Members")
                + opt(Visibility::Friends, "Friends")
                + opt(Visibility::OnlyMe, "Only me")
                + "</select>";
        };
        html += "<table class=\"register\"><tr><th>Surface</th><th>Who can see it</th></tr>"
                "<tr><td>Profile</td><td>" + vis_select("profile_visibility", p.profile_visibility) + "</td></tr>"
                "<tr><td>Posts</td><td>" + vis_select("posts_visibility", p.posts_visibility) + "</td></tr>"
                "<tr><td></td><td><span class=\"muted\">Each post on /feed now picks its own reach "
                "(Anybody/Friends/Groups) -- this row is the fallback for Groups-only posts on your "
                "profile, and Only me still hides everything from everyone but you.</span></td></tr>"
                "</table>"
                "<button>Save privacy</button></form><h2 id=\"blocks\">Block list</h2>";
        auto bl = g_store.block_entries_of(user);
        html += "<p class=\"muted eyebrow\">"+std::to_string(bl.size())+
                (bl.size()==1?" member":" members")+" &middot; not visible to them</p>";
        if (bl.empty()) html += "<p class=\"empty\">No one blocked.</p>";
        for (auto& b : bl) {
            html += "<div class=\"person\"><div><span>"+html_escape(b.other)+"</span>"
                    "<div class=\"muted\">Blocked "+html_escape(fmt_time(b.at))+
                    (b.reason.empty() ? "" : " · " + html_escape(b.reason))+"</div></div>"
                    "<form method=\"POST\" action=\"/settings/unblock\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(b.other)+"\">"
                    "<button class=\"ghost\">Unblock</button></form></div>";
        }
        html += "<form method=\"POST\" action=\"/settings/block\" style=\"margin-top:.75rem\">"
                "<div class=\"row\"><input name=\"who\" placeholder=\"account name\" autocomplete=\"off\" required style=\"flex:1\">"
                "<button class=\"danger\" type=\"submit\">Add block</button></div>"
                "<input name=\"reason\" placeholder=\"Reason (optional, only you see it)\" maxlength=\"200\"></form>";
        {
            // Moved off the profile page: who can queue a track on your
            // jukebox is an access-control setting like block list and
            // visibility above, not something that belongs on a page
            // visitors see (Brandon: "its not appropriate" there).
            auto jsel = [&](JukeboxPolicy pol) -> std::string { return p.jukebox_policy==pol?" checked":""; };
            html += "<h2 id=\"jukebox\">Jukebox</h2>"
                    "<form method=\"POST\" action=\"/me/jukebox/policy\">"
                    "<label>Who else can queue a track on your jukebox</label>"
                    "<p><label><input type=\"radio\" name=\"policy\" value=\"0\""+jsel(JukeboxPolicy::OwnerOnly)+"> Just me</label></p>"
                    "<p><label><input type=\"radio\" name=\"policy\" value=\"1\""+jsel(JukeboxPolicy::Friends)+"> Friends</label></p>"
                    "<p><label><input type=\"radio\" name=\"policy\" value=\"2\""+jsel(JukeboxPolicy::Anyone)+"> Anyone signed in</label></p>"
                    "<p class=\"muted\">One request at a time per person; it plays once, then it's gone. "
                    "Anyone you've blocked above can never queue, regardless.</p>"
                    "<button>Save</button></form>";
        }
        html += "</div>";

        html += "<div class=\"card\"><h2 id=\"identity\">Identity</h2>"
            "<p>No civic identity is linked to this account.</p>"
            "<p class=\"muted\">Voting is not live yet. Linking a citizen identity later will not happen from this page until it is.</p></div>";

        html += "<div class=\"card\"><h2 id=\"data\">Data</h2>"
            "<p><a class=\"btn ghost\" href=\"/export\">Download your export</a></p>"
            "<p class=\"muted\">Export includes username, blurb, profile CSS, your friend list, groups you started or joined, "
            "posts you wrote, settings, and messages you sent. It does not include other people’s text or your password hash.</p>"
            "<form method=\"POST\" action=\"/settings/delete\">"
            // the name only in the label, never as a placeholder (CODE-SCAN-UX B1: a keychain-style fill target)
            "<label>Delete account — type your account name, <strong>"+html_escape(user)+"</strong></label>"
            "<input name=\"confirm\" autocomplete=\"off\">"
            "<button class=\"danger\">Delete account</button></form>"
            "<p class=\"muted\">Sessions end immediately. Mail you sent is shown as [deleted account]. "
            "A fingerprint of the phone number is held ninety days so a deleted account cannot immediately reopen under the same number.</p></div>";

        html += std::string("<div class=\"card\"><h2 id=\"notifications\">Notifications</h2><form method=\"POST\" action=\"/settings/notify\">")
            + "<p><label><input type=\"checkbox\" name=\"notify_friend\" value=\"1\"" + (p.notify_friend?" checked":"") + "> Friend requests and accepts</label></p>"
            + "<p><label><input type=\"checkbox\" name=\"notify_message\" value=\"1\"" + (p.notify_message?" checked":"") + "> New mail</label></p>"
            + "<p><label><input type=\"checkbox\" name=\"notify_group\" value=\"1\"" + (p.notify_group?" checked":"") + "> Group invites</label></p>"
            + "<p class=\"muted\">Login alerts stay on.</p>"
            + "<p><label><input type=\"checkbox\" name=\"intro_every_visit\" value=\"1\"" + (p.intro_every_visit?" checked":"") + "> Play the landing intro every visit</label></p>"
            + "<p class=\"muted\">Off plays it just the first time you land on the site, on this browser.</p>"
            + "<button>Save notifications</button></form></div>";
        return pg("Settings", html);
    }

    if(method=="GET"&&path=="/export") {
        if(!acc) return need_login();
        std::ostringstream o;
        o << "LiquidGov.US platform export\n";
        o << "This is platform data. It is not the governance kernel.\n\n";
        o << "username: " << user << "\n";
        o << "created_at: " << fmt_time(acc->created_at) << "\n";
        o << "blurb: " << g_store.blurb(acc->id) << "\n";
        o << "profile css:\n" << g_store.author_css(acc->id) << "\n\n";
        o << "friends:\n";
        for (auto& n : g_store.friends_of(user)) o << "  " << n << "\n";
        o << "\ncommunities:\n";
        for (auto& c : g_store.communities_of(user)) o << "  " << c.slug << " " << c.name << "\n";
        o << "\nposts you wrote:\n";
        for (auto& p : g_store.posts_by(user)) {
            o << "  [" << fmt_time(p.ts) << "] ";
            if (!p.community.empty()) o << "/" << p.community << " ";
            o << p.text << "\n";
        }
        o << "\nmessages you sent:\n";
        for (auto& m : g_store.mail_sent_by(user)) {
            o << "  to " << m.to << " [" << fmt_time(m.ts) << "] " << m.text << "\n";
        }
        auto pr = g_store.prefs_of(user);
        o << "\nsettings:\n";
        o << "  findability: " << static_cast<int>(pr.findability) << "\n";
        o << "  group_invite_confirm: " << (pr.group_invite_confirm?"yes":"no") << "\n";
        std::string extra = "Content-Disposition: attachment; filename=\"liquidgov-export.txt\"\r\n";
        extra += "Cache-Control: no-store\r\n";
        return http_raw("200 OK", "text/plain; charset=utf-8", o.str(), extra);
    }
    if(method=="POST"&&path=="/settings/revoke") {
        if(!acc) return need_login();
        auto target = parse_form(body)["sid"];
        bool current = target == sid;
        bool owned = false;
        for (const auto& s : g_store.sessions_for(acc->id)) {
            if (s.id == target) { owned = true; break; }
        }
        if (owned) logout(g_store, target);
        if (current) return http_redir("/login", clear_cookie());
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/logout-all") {
        if(!acc) return need_login();
        logout_all_devices(g_store, acc->id, sid);
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/password") {
        if(!acc) return need_login();
        auto f=parse_form(body);
        // A Google-only account has no real password yet -- its hash is
        // the same dummy_hash() placeholder login() uses for a timing-
        // safe "unknown username" rejection, which by design never
        // verifies against anything. Skip the current-password check
        // only for that exact placeholder; a real account still has to
        // prove its current password like always.
        const bool has_real_password = acc->password_hash != dummy_hash();
        if ((has_real_password && !verify_password(f["current"], acc->password_hash)) || f["next"].empty())
            return http_redir("/settings/password?err=password");
        if (f["next"].size() < 8)
            return http_redir("/settings/password?err=short");
        if (g_breach.is_breached(f["next"]))
            return http_redir("/settings/password?err=password");
        g_store.set_password_hash(acc->id, hash_password(f["next"]));
        return http_redir("/settings?ok=password");
    }
    if(method=="POST"&&path=="/settings/map") {
        if(!acc) return need_login();
        auto f=parse_form(body);
        MapPrefs mp;
        mp.show = f["show"] == "1";
        mp.groups = f["groups"] == "1";
        mp.posts = f["posts"] == "1";
        mp.comments = f["comments"] == "1";
        mp.friends = f["friends"] == "1";
        mp.pains = f["pains"] == "1";
        g_store.set_map_prefs(user, mp); // pains off deletes what it found
        return http_redir("/settings#map-privacy");
    }
    if(method=="POST"&&path=="/settings/privacy") {
        if(!acc) return need_login();
        auto f=parse_form(body);
        auto p = g_store.prefs_of(user);
        int fv = 1;
        try { fv = std::stoi(f["findability"]); } catch (...) { fv = 1; }
        if (fv < 0 || fv > 2) fv = 1;
        p.findability = static_cast<Findability>(fv);
        p.group_invite_confirm = f["group_invite_confirm"] == "1";
        p.presence_hide = f["presence_hide"] == "1";
        auto vis_of = [&](const std::string& field, Visibility fallback) {
            int v = static_cast<int>(fallback);
            try { v = std::stoi(f[field]); } catch (...) { v = static_cast<int>(fallback); }
            if (v < 0 || v > 3) v = static_cast<int>(fallback);
            return static_cast<Visibility>(v);
        };
        p.profile_visibility = vis_of("profile_visibility", p.profile_visibility);
        p.posts_visibility = vis_of("posts_visibility", p.posts_visibility);
        g_store.set_prefs(user, p);
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/notify") {
        if(!acc) return need_login();
        auto f=parse_form(body);
        auto p = g_store.prefs_of(user);
        p.notify_friend = f["notify_friend"]=="1";
        p.notify_message = f["notify_message"]=="1";
        p.notify_group = f["notify_group"]=="1";
        p.intro_every_visit = f["intro_every_visit"]=="1";
        g_store.set_prefs(user, p);
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/unblock") {
        if(!acc) return need_login();
        g_store.unblock(user, parse_form(body)["who"]);
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/block") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        auto who = trim_ws(f["who"]);
        if (valid_username(who) && who != user) g_store.block(user, who, trim_ws(f["reason"]));
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/delete") {
        if(!acc) return need_login();
        auto confirm = trim_ws(parse_form(body)["confirm"]);
        if (confirm != user) return http_redir("/settings?err=delete");
        clear_avatar(g_blobs, g_store, acc->id);
        g_store.delete_account(acc->id);
        return http_redir("/", clear_cookie());
    }
    return std::nullopt;
}

}  // namespace liquidgov::site
