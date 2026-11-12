#include "site/features/profile/profile_routes.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/core/http.hpp"
#include "site/core/multipart.hpp"
#include "site/features/feed/composer.hpp"
#include "site/features/feed/post_block.hpp"
#include "site/features/jukebox/jukebox_widget.hpp"
#include "site/features/profile/portrait.hpp"
#include "site/features/profile/profile_css.hpp"
#include "site/features/styles/styles.hpp"
#include "site/ui/wrap.hpp"

namespace liquidgov::site {

std::optional<std::string> profile_routes(Route& r) {
    LG_ROUTE_LOCALS(r)
    if(method=="GET"&&path=="/me") {
        if(!acc) return need_login();
        std::string err = flash_error(qs);
        std::string html;
        // No Publish/Discard bar any more (Brandon 2026-11-11 4:50 PM, Brief 4 item 11): every form[data-draft] here
        // saves itself, see draft_publish_script.
        html += "<div class=\"card\"><div class=\"who\">"+portrait_tag(user,"")+"<div><h1>"+html_escape(user)+"</h1>"
            "<p class=\"muted\">Platform profile · <a href=\"/u/"+html_escape(user)+"\">public page</a> · member since "+
            html_escape(fmt_time(acc->created_at))+"</p></div></div>"+err+
            "<p class=\"muted\">Self-portrait &mdash; draw one with the same tools as the "
            "<a href=\"/me/portrait\">portrait tool</a>.</p>";
        html += "<p class=\"muted\">A platform account is not a citizen identity.</p></div>";
        {
            auto prof = g_store.profile_of(user);
            auto bio = g_store.blurb(acc->id);
            html += "<div class=\"card\"><h2>About you</h2>"
                "<form method=\"POST\" action=\"/me/profile\" data-draft=\"1\">"
                "<label>Display name</label><input name=\"display_name\" required maxlength=\"80\" autocomplete=\"nickname\" value=\""+html_escape(prof.display_name)+"\">"
                "<label>Hometown</label><input name=\"hometown\" maxlength=\"80\" value=\""+html_escape(prof.hometown)+"\">"
                "<label>Where you live now</label><input name=\"living\" maxlength=\"80\" value=\""+html_escape(prof.living)+"\">"
                "<label>Bio</label><textarea name=\"bio\" maxlength=\"500\">"+html_escape(bio)+"</textarea>"
                "<label>Interests</label><input name=\"interests\" maxlength=\"200\" placeholder=\"comma-separated\" value=\""+html_escape(prof.interests)+"\">"
                "<label>Career</label><input name=\"career\" maxlength=\"80\" value=\""+html_escape(prof.career)+"\">"
                "<label>School</label><input name=\"school\" maxlength=\"80\" value=\""+html_escape(prof.school)+"\">"
                "</form>"
                "<p class=\"muted\">Email stays a fingerprint from signup. It is not shown and cannot be changed here.</p></div>";
        }
        {
            auto n_frames = g_store.canvas_frames(acc->id).size();
            std::string canvas_status = n_frames == 0
                ? "No drawing yet &mdash; your profile uses the default background."
                : (std::to_string(n_frames) + " frame" + (n_frames==1?"":"s") + " saved, "
                   + (g_store.canvas_timelapse(acc->id) ? "looping as your background." : "last one shown as your background."));
            html += "<div class=\"card\"><h2>Canvas</h2>"
                "<p class=\"muted\">"+canvas_status+"</p>"
                "<p class=\"row\"><a class=\"btn\" href=\"/me/canvas\">Open the canvas</a></p></div>";
        }
        {
            auto current_css = g_store.author_css(acc->id);
            std::string css_msg;
            // The page-top banner (see `err` above) already covers ?err=css
            // -- this is just the success case, which has no other home.
            if (qs["ok"]=="css") css_msg = "<p class=\"ok\">Saved. See it at <a href=\"/u/"+html_escape(user)+"\">your public profile</a>.</p>";
            // CSS saved under the older sandbox-era filter can fail today's rules; the profile then shows the plain
            // look, and this says why (Brandon 2026-11-12).
            if (!current_css.empty()) {
                auto now = sanitize_author_css(current_css);
                if (!now.ok)
                    css_msg += "<p class=\"err\">Your saved CSS isn't shown on your profile: " +
                               html_escape(css_reject_message(now.code)) + " Change it and save again.</p>";
            }
            html += "<div class=\"card\" id=\"profile-css\"><h2>Profile CSS</h2>"
                "<p class=\"muted\">Styles your whole profile page (/u/"+html_escape(user)+
                ") below the site header. Use .lg-theme to style the whole area. Visitors can always switch to a "
                "plain view. Escapes (\\), the &amp; selector, url(), image functions, @import and @-rules other "
                "than @media, @supports and @container are rejected outright, not silently stripped. "
                "Limit " + std::to_string(kAuthorCssMaxBytes/1000) + "KB. Leave empty to use the default look.</p>"
                + css_msg +
                "<form method=\"POST\" action=\"/me/css\" data-draft=\"1\">"
                "<textarea name=\"css\" rows=\"8\" spellcheck=\"false\" "
                // 16px, not .85rem: under 16px iOS Safari zooms the page in on focus and leaves it zoomed (2026-11-07)
                "style=\"font-family:ui-monospace,Consolas,monospace;font-size:16px\" "
                "placeholder=\".lg-theme h1{color:#a23b3b}\">"+html_escape(current_css)+"</textarea>"
                "</form>" + styles_me_card(user, !current_css.empty(), qs) + "</div>";
        }
        html += jukebox_widget(user, true, user, qs);
        {
            // Off by default -- visiting a profile shouldn't ambush anyone
            // with sound. "Play this Juketube instead" is stored as-is;
            // an invalid value (self, a stranger who blocked you, a cycle)
            // is never rejected here, just quietly inert at read time (see
            // jukebox_effective_owner) -- same posture as findability
            // elsewhere: a miss looks like a miss, not an error message.
            std::string jmsg = qs["ok"]=="jplay" ? "<p class=\"ok\">Saved.</p>" : "";
            html += "<div class=\"card\"><h2>Play settings</h2>"+jmsg+
                "<form method=\"POST\" action=\"/me/jukebox/play-settings\" data-draft=\"1\">"
                "<label><input type=\"checkbox\" name=\"autoplay\" value=\"1\""+
                (g_store.jukebox_autoplay(user) ? " checked" : "")+
                "> Play my Juketube when someone opens my profile</label>"
                "<label style=\"display:block;margin-top:.5rem\">Play this Juketube instead</label>"
                "<input name=\"relay\" placeholder=\"another person&rsquo;s Juketube, or empty for your own\" autocomplete=\"off\" "
                "value=\""+html_escape(g_store.jukebox_relay_target(user))+"\">"
                "</form></div>";
        }
        html += draft_publish_script();
        return pg("Profile", html);
    }

    // Checked before the general /u/ handler below, since that one treats
    // everything after "/u/" as a bare username.
    if(method=="GET"&&path.rfind("/u/",0)==0&&path.size()>7&&path.compare(path.size()-6,6,"/theme")==0) {
        const std::string who = path.substr(3, path.size()-3-6);
        std::string body;
        if (!valid_username(who)) {
            body = "<!doctype html><html><head><meta charset=\"utf-8\"></head>"
                   "<body><p>No such profile.</p></body></html>";
        } else {
            auto found = g_store.find_by_username(who);
            if (!found) {
                body = "<!doctype html><html><head><meta charset=\"utf-8\"></head>"
                       "<body><p>No such profile.</p></body></html>";
            } else {
                auto author_css = g_store.author_css(found->id);
                // Deliberately not wrap()/page_wrap(): no nav, no jukebox
                // widget, no session cookie context, no scripts at all --
                // this document only ever renders inside the sandboxed,
                // scriptless iframe below. See sanitize_author_css's
                // comment for what's actually defended here and why a gap
                // in the text filter still can't reach anything.
                // No canvas background here anymore -- a profile-background
                // drawing renders directly on the real /u/<username> page
                // now (see profile_canvas_css), not through this document.
                // This route/iframe exists purely for a custom .lg-theme
                // author_css, so it's only ever reached when that's set.
                body = "<!doctype html><html><head><meta charset=\"utf-8\">"
                       "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                       "<style>body{margin:0;padding:1rem;font:16px/1.5 system-ui,sans-serif;"
                       "background:#fff;color:#1a1814}h1{margin:.2rem 0}"
                       "p{white-space:pre-wrap}</style>"
                       "<style>" + author_css + "</style></head>"
                       "<body><div class=\"lg-theme\"><h1>" + html_escape(found->username) + "</h1>"
                       "</div></body></html>";
            }
        }
        return http_raw("200 OK", "text/html; charset=utf-8", body);
    }
    if(method=="GET"&&path.rfind("/u/",0)==0) {
        const std::string who = path.substr(3);
        if (!valid_username(who))
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such profile.</p></div>"));
        auto found = g_store.find_by_username(who);
        if (!found)
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such profile.</p></div>"));
        if (acc && g_store.is_blocked(user, found->username))
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such profile.</p></div>"));
        const bool is_owner = acc && acc->username == found->username;
        const bool is_friend = acc && !is_owner && g_store.are_friends(user, found->username);
        auto target_prefs = g_store.prefs_of(found->username);
        if (!visible_to(target_prefs.profile_visibility, is_owner, acc.has_value(), is_friend)) {
            return http_code("200 OK", wrap(found->username, user, unread,
                "<div class=\"card\"><p>This profile is private.</p></div>"));
        }
        auto committees = g_store.communities_of(found->username);
        std::string committees_html;
        if (!committees.empty()) {
            committees_html = "<p class=\"eyebrow\" style=\"margin-top:1rem\">Groups</p>"
                               "<div style=\"display:flex;flex-wrap:wrap;gap:.4rem\">";
            for (const auto& c : committees) {
                committees_html += "<a class=\"chip\" href=\"/c/"+html_escape(c.slug)+"\" style=\"text-decoration:none\">"+
                                    html_escape(c.name)+"</a>";
            }
            committees_html += "</div>";
        }
        // A real button, centered at the top of the page -- not the old
        // underlined text link buried under the portrait, easy to miss
        // and inconsistent with every other primary action on the site.
        // Owner-only: is_owner is the same check the rest of this page
        // already uses to decide what a stranger versus the account
        // itself gets to see here.
        std::string html = !is_owner ? "" :
            "<p class=\"row\" style=\"justify-content:center;margin:0 0 1rem\">"
            "<a class=\"btn\" href=\"/me\">Edit profile</a></p>";
        // Profile CSS styles this whole page below the site header (Brandon 2026-11-12). Stored CSS is checked again
        // with today's rules: CSS saved under the older, sandbox-era filter that fails them is not shown (its owner
        // is told on /me). The "plain view" link sits OUTSIDE the styled area, so no style can hide or fake it, and
        // ?plain=1 drops the CSS entirely (unreadable colors, accessibility, a hostile style).
        const std::string stored_css = g_store.author_css(found->id);
        const bool css_ok = !stored_css.empty() && sanitize_author_css(stored_css).ok;
        const bool plain = qs["plain"] == "1";
        const std::string self_url = "/u/" + html_escape(found->username);
        if (css_ok) {
            html += plain
                ? "<p class=\"muted lg-plain-view\" style=\"text-align:right;margin:0 0 .5rem\">Plain view. <a href=\""
                      + self_url + "\">Show " + html_escape(found->username) + "'s style</a></p>"
                : "<p class=\"muted lg-plain-view\" style=\"text-align:right;margin:0 0 .5rem\"><a href=\""
                      + self_url + "?plain=1\">Plain view</a></p>";
            if (!plain) html += author_css_block(stored_css);
        }
        // contain:paint makes this outer div the box every fixed/absolute child is positioned and clipped in, so no
        // style can cover the header or nav; author selectors can't reach it (they are all nested under the scope).
        // .lg-theme stays inside the scope so CSS written for the old frame (.lg-theme h1{...}) still matches.
        html += "<div class=\"lg-profile-outer\" style=\"contain:paint;isolation:isolate;position:relative\">"
                "<div class=\"lg-profile-scope\"><div class=\"lg-theme\">";
        // A profile-background drawing is the identity card's own
        // background now, not a separate sandboxed iframe (that iframe
        // used to show a near-empty white box -- just a duplicate
        // username heading plus the drawing bleeding through -- for
        // anyone with frames but no custom theme). #profilecard only
        // gets the id (and the <style> block only gets emitted) when
        // there's actually a drawing to show.
        auto canvas_frame_ids = g_store.canvas_frames(found->id);
        std::string card_css = profile_canvas_css(found->username, canvas_frame_ids.size(),
                                                    g_store.canvas_timelapse(found->id));
        html += card_css.empty() ? "" : "<style>"+card_css+"</style>";
        html += std::string("<div class=\"card\"") + (canvas_frame_ids.empty() ? "" : " id=\"profilecard\"") +
            "><div class=\"who\">"+portrait_tag(found->username,"")+
            "<div><h1>"+html_escape(found->username)+"</h1>";
        {
            auto pr = g_store.profile_of(found->username);
            if (!pr.display_name.empty())
                html += "<p class=\"lede\" style=\"margin:.2rem 0 .4rem\">"+html_escape(pr.display_name)+"</p>";
            auto bio = g_store.blurb(found->id);
            std::string bits;
            if (!pr.living.empty()) bits += html_escape(pr.living);
            if (!pr.hometown.empty()) bits += (bits.empty()?"":" · ")+std::string("from ")+html_escape(pr.hometown);
            if (!pr.career.empty()) bits += (bits.empty()?"":" · ")+html_escape(pr.career);
            if (!pr.school.empty()) bits += (bits.empty()?"":" · ")+html_escape(pr.school);
            if (!bits.empty()) html += "<p class=\"muted\">"+bits+"</p>";
            if (!bio.empty()) html += "<p>"+html_escape(bio)+"</p>";
            if (!pr.interests.empty()) html += "<p class=\"muted\">"+html_escape(pr.interests)+"</p>";
        }
        html +=
            "<table class=\"record\">"
            "<tr><td>Standing</td><td>Associate &middot; good</td></tr>"
            "<tr><td>Enrolled</td><td>"+html_escape(fmt_time(found->created_at))+"</td></tr>"
            "</table></div></div>"+committees_html;
        // (The old sandboxed /u/<name>/theme frame is gone from this page: the CSS styles the page itself now. The
        // /theme route still answers; it is on the bloat list.)
        if (is_owner) {
            // Edit profile is now the centered button at the top of the
            // page (see above) -- nothing further needed here.
        } else if (acc && g_store.are_friends(user, found->username)) {
            html += "<p class=\"row\"><a class=\"btn\" href=\"/mail?with="+html_escape(found->username)+"\">Message</a>"
                    "<form class=\"inline\" method=\"POST\" action=\"/friends/unfriend\" "
                    "onsubmit=\"return confirm('Unfriend "+html_escape(found->username)+"? You will leave every group you share with them. They stay in those groups.');\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(found->username)+"\">"
                    "<button class=\"ghost\" type=\"submit\">Unfriend</button></form></p>";
        } else if (acc) {
            html += "<form method=\"POST\" action=\"/friends\"><input type=\"hidden\" name=\"who\" value=\""+
                    html_escape(found->username)+"\"><button>Send friend request</button></form>";
        }
        html += "</div>";
        html += jukebox_widget(found->username, is_owner, user, qs);
        // "Play my Juketube when someone opens my profile" -- off by
        // default. Signed-in visitors only (the floating widget doesn't
        // exist for a logged-out guest, so there'd be nothing to tune),
        // and never for the owner's own visit to their own page --
        // is_owner is already computed above for exactly this branch.
        if (acc && !is_owner && g_store.jukebox_autoplay(found->username)) {
            // wrap() appends the floating widget's own markup+script right
            // after </main> (see its comment -- keeps the widget alive
            // across soft-nav, which only ever touches <main>'s innerHTML),
            // which means __lgTuneStation isn't defined yet at the point
            // this inline script -- inside <main> -- actually runs.
            // setTimeout(0) pushes the call past the rest of this initial
            // parse, by which point the widget's script further down the
            // document has already executed and defined it.
            html += "<script>(function(){"
                    "setTimeout(function(){"
                    "if(window.__lgTuneStation)window.__lgTuneStation('u:"+found->username+"');"
                    "},0);"
                    "})();</script>";
        }
        {
            // A post's own audience (the /feed "Anybody"/"Friends"/"Groups"
            // chips) gates it here, not the account-wide posts_visibility
            // setting -- that blanket still governs only a GroupsOnly post
            // shown on this list (unchanged; a community's real audience is
            // its membership, this is just "does this profile show its
            // posts at all" for those).
            auto theirPosts = g_store.posts_by(found->username);
            std::reverse(theirPosts.begin(), theirPosts.end());
            std::vector<Post> shown;
            for (const auto& p : theirPosts) {
                bool ok;
                if (is_owner) {
                    ok = true;
                } else if (p.audience == PostAudience::GroupsOnly) {
                    ok = visible_to(target_prefs.posts_visibility, is_owner, acc.has_value(), is_friend);
                } else if (p.audience == PostAudience::Anybody) {
                    ok = std::find(p.exceptions.begin(), p.exceptions.end(), user) == p.exceptions.end();
                } else { // Friends, possibly also posted into a community
                    ok = is_friend && target_prefs.posts_visibility != Visibility::OnlyMe &&
                         (p.friend_allowlist.empty() ||
                          std::find(p.friend_allowlist.begin(), p.friend_allowlist.end(), user) !=
                              p.friend_allowlist.end());
                }
                if (ok) shown.push_back(p);
            }
            if (is_owner || !shown.empty()) {
                html += "<h2>Posts</h2><p class=\"muted eyebrow\">"+std::to_string(shown.size())+
                        (shown.size()==1?" entry":" entries")+"</p>";
                for (const auto& p : shown) html += "<div class=\"card\">"+post_block(p, user)+"</div>";
            }
        }
        html += "</div></div></div>";  // .lg-theme, .lg-profile-scope, .lg-profile-outer
        return pg(found->username, html);
    }
    if(method=="POST"&&path=="/me/profile") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        auto display = trim_ws(f["display_name"]);
        if (display.empty() || display.size() > 80) return http_redir("/me?err=profile");
        Profile p = g_store.profile_of(user);
        p.display_name = display;
        p.hometown = trim_ws(f["hometown"]).substr(0, 80);
        p.living = trim_ws(f["living"]).substr(0, 80);
        p.interests = trim_ws(f["interests"]).substr(0, 200);
        p.career = trim_ws(f["career"]).substr(0, 80);
        p.school = trim_ws(f["school"]).substr(0, 80);
        g_store.set_profile(user, p);
        g_store.set_blurb(acc->id, trim_ws(f["bio"]).substr(0, 500));
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/me/css") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        const std::string& raw_css = f["css"];
        auto check = sanitize_author_css(raw_css);
        if (!check.ok) return http_redir("/me?err=css&reason=" + check.code);
        g_store.set_author_css(acc->id, raw_css);
        return http_redir("/me?ok=css");
    }
    if(method=="POST"&&path=="/me/avatar") {
        if(!acc) return need_login();
        auto hdr_end = req.find("\r\n\r\n");
        std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
        auto file = multipart_file(header_value(hdrs, "Content-Type"), body, "photo");
        if (!file || file->empty()) return http_redir("/me?err=image");
        auto r = save_avatar(g_blobs, g_store, g_scanner, acc->id, *file);
        if (r.error == SaveAvatarError::TooLarge) return http_redir("/me?err=portraitlarge");
        if (r.error == SaveAvatarError::FailedScan) return http_redir("/me?err=scan");
        if (r.error != SaveAvatarError::None) return http_redir("/me?err=image");
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/me/avatar/clear") {
        if(!acc) return need_login();
        clear_avatar(g_blobs, g_store, acc->id);
        return http_redir("/me");
    }
    return std::nullopt;
}

}  // namespace liquidgov::site
