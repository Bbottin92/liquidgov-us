#include "site/ui/shell.hpp"
#include "site/features/preview/preview.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/ui/assets.hpp"
#include "site/ui/favicon_cookies.hpp"
#include "site/core/http.hpp"
#include "site/features/profile/portrait.hpp"
#include "site/features/mail/mail_pages.hpp"

namespace liquidgov::site {
// A full-bleed, muted, autoplaying clip on the landing page (/), on EVERY visit by default (Brandon 2026-11-13
// 5:04 AM; it used to be the first visit only). Skippable; ends by fading out and removing itself, revealing the
// page underneath (already rendered, not reloaded). Its script is assets/intro.js, loaded synchronously right after
// it so a browser that turned it off drops it before the first paint. The first visit (per browser) shows the line
// "Plays every visit. Turn off in Settings" (#lgintronote). Its look is site.css .lg-intro (Brandon 5:46 AM: fits every
// screen): the whole viewport (dvh, safe-area insets for Skip and the line), the clip contained, never stretched or
// cropped, on its own background color.
//
// account_mode: the signed-in landing; the server already checked the account's pref (wrap()/pages_routes) and only
// renders this when it is on, so there is no local gate. Otherwise (logged out) intro.js reads this browser's pref
// (localStorage 'liquidgov.' + kIntroPref, on unless set to '0'). Only reachable when show_intro is true (the
// landing page -- see page_wrap), so this never fires on an internal page.
std::string intro_overlay_html(bool account_mode) {
    return std::string("<div id=\"lgintro\" class=\"lg-intro\" data-mode=\"") + (account_mode ? "account" : "browser") +
           "\" data-pref=\"" + kIntroPref + "\">"
           "<video id=\"lgintrovid\" autoplay muted playsinline src=\"/assets/intro.mp4?v=2\"></video>"
           "<p id=\"lgintronote\" class=\"lg-intro-note\" hidden>Plays every visit. "
           "<a href=\"/settings/intro\">Turn off in Settings</a></p>"
           "<button type=\"button\" id=\"lgintroskip\" class=\"lg-intro-skip\">Skip</button>"
           "</div>"
           "<script src=\"" + asset_url("intro.js") + "\"></script>";
}

std::string intro_setting_card(const std::string& user, bool on) {
    const std::string pref = kIntroPref;
    std::string html = "<div class=\"card\"><h2 id=\"intro\">Intro video</h2>";
    if (!user.empty()) {
        html += "<form method=\"POST\" action=\"/settings/intro\">"
                "<p><label><input type=\"checkbox\" name=\"" + pref + "\" value=\"1\"" + (on ? " checked" : "") +
                "> Play intro video on each visit</label></p>"
                "<p class=\"muted\">Saved to your account: the intro on liquidgov.us plays (or not) on every browser you sign in on.</p>"
                "<button>Save intro setting</button></form></div>";
        return html;
    }
    return html + "<p><label><input type=\"checkbox\" data-lg-intro-toggle data-pref=\"" + pref + "\" checked"
                  "> Play intro video on each visit</label></p>"
                  "<p class=\"muted\" id=\"lgintrosaved\" aria-live=\"polite\">Kept on this browser. Signed in, it is saved to your account.</p>"
                  "<script src=\"" + asset_url("intro.js") + "\"></script></div>";
}

// The iOS 'Get the app' bar that used to be built here (ios_install_hint) was removed 2026-11-07: assets/setup.js
// shows the one get-app prompt (iPhone Safari: "Add to Home Screen to get alerts"), and setup.js used to hide this
// one every second, so on a real iPhone it flashed up and vanished.

// Shown once, only inside /mail's own body (never site-wide via
// wrap() -- see that route) -- Add to Home Screen bookmarks whatever
// page is actually on screen, so the nudge has to land after the
// visitor is already on the real inbox, not on the /app/mail install
// page that may have sent them here. Same dismiss-once/style.display/
// real-<button> conventions the removed site-wide iOS hint used, but covers
// Android too (a real menu item there, not an iOS-only gesture) and
// keys off its own localStorage entry so dismissing one hint doesn't
// silently dismiss the other.
std::string mail_install_hint() {
    return
        "<div id=\"lgmailhint\" "
        "style=\"position:fixed;left:.6rem;right:.6rem;"
        "bottom:calc(.6rem + env(safe-area-inset-bottom,0px));"
        "z-index:50;max-width:26rem;margin:0 auto;background:var(--parchment);border:1px solid var(--rule);"
        "border-radius:var(--rmd);padding:.6rem .7rem;box-shadow:0 6px 20px rgba(0,0,0,.18);"
        "display:none;align-items:center;gap:.6rem\">"
        "<div style=\"flex:1;min-width:0\">"
        "<p style=\"margin:0;font-family:var(--display);font-weight:700;font-size:1rem;color:var(--ink)\">You're on Messages</p>"
        // Chrome on iPhone has no Add to Home Screen action at all --
        // the button just isn't in its share sheet, same reason
        // apps.html and the old app-mail.html copy both named Safari/
        // Brave specifically. The removed site-wide iOS hint skipped naming a
        // browser (it's the generic site-wide nudge, kept short on
        // purpose) but this one is telling someone to do a specific
        // thing right now, so the caveat that saves a dead-end tap
        // belongs in it.
        "<p id=\"lgmailhinttext\" style=\"margin:.1rem 0 0;font-size:.78rem;color:var(--stone)\">In "
        "<strong>Safari</strong> (or Brave), tap Share &rarr; "
        "<strong>Add to Home Screen</strong> (name it Messages). Chrome on iPhone can't do this.</p></div>"
        "<button type=\"button\" id=\"lgmailhintclose\" class=\"ghost\" aria-label=\"Dismiss\" "
        "style=\"min-height:44px;min-width:44px;padding:0;flex-shrink:0;font-size:1.1rem\">"
        "&#10005;</button></div>"
        "<script>(function(){"
        "try{if(localStorage.getItem('liquidgov.mailhint.dismissed'))return;}catch(e){return;}"
        "var isIOS=/iP(hone|od|ad)/.test(navigator.userAgent)||"
        "(navigator.platform==='MacIntel'&&navigator.maxTouchPoints>1);"
        "var isAndroid=/Android/.test(navigator.userAgent);"
        "if(!isIOS&&!isAndroid)return;"
        "var standalone=window.navigator.standalone===true||"
        "(window.matchMedia&&matchMedia('(display-mode: standalone)').matches);"
        "if(standalone)return;"
        "var el=document.getElementById('lgmailhint');"
        "if(isAndroid){document.getElementById('lgmailhinttext').innerHTML="
        "'Now tap <strong>\\u22ee</strong> then <strong>Add to Home screen</strong> or <strong>Install app</strong>.';}"
        "el.style.display='flex';"
        "function dismiss(){"
        "el.style.display='none';"
        "try{localStorage.setItem('liquidgov.mailhint.dismissed','1');}catch(e){}"
        "}"
        "var btn=document.getElementById('lgmailhintclose');"
        "btn.addEventListener('touchstart',dismiss,{passive:true});"
        "btn.addEventListener('click',dismiss);"
        "})();</script>";
}

std::string page_wrap(const std::string& title, const std::string& navhtml, const std::string& body,
                       bool show_intro, bool intro_always_play) {
    const std::string theme_attr = g_theme_mode.empty() ? "" : " data-theme=\""+g_theme_mode+"\"";
    const std::string next_theme = g_theme_mode == "dark" ? "light" : "dark";
    std::string toggle = "<a class=\"themebtn\" id=\"themetoggle\" href=\"/theme/"+next_theme+
        "\" aria-label=\"Switch to "+next_theme+"\" title=\"Switch to "+next_theme+"\">"+
        (g_theme_mode == "dark" ? "&#9728;" : "&#9789;")+"</a>";
    if (g_theme_mode.empty()) {
        // With no explicit lg_theme cookie, the page is actually dark or
        // light by prefers-color-scheme -- something the server can't see.
        // Guessing "dark" as the next theme (the code above's only option
        // without that information) is wrong for exactly the visitors
        // whose system is already in dark mode: the button shows a moon
        // ("switch to dark") while the page is already dark, and clicking
        // it sets an explicit dark cookie that changes nothing on screen --
        // reads as "the toggle doesn't work." Corrected client-side, once,
        // against the one thing only the browser actually knows.
        toggle +=
            "<script>(function(){"
            "if(window.matchMedia&&matchMedia('(prefers-color-scheme:dark)').matches){"
            "var t=document.getElementById('themetoggle');"
            "t.href='/theme/light';t.textContent='\\u2600';"
            "t.setAttribute('aria-label','Switch to light');t.title='Switch to light';"
            "}"
            "})();</script>";
    }
    const std::string full_title = html_escape(title) + " — LiquidGov.US";
    // What a link unfurls to on Facebook/X/Slack/etc. Without these tags a
    // shared liquidgov.us link shows nothing but the bare URL. One shared
    // description site-wide, not a per-page one -- there's no per-page
    // summary tracked anywhere to draw from. The line is the landing hero
    // ("This is YOUR government!") plus the limit the page itself states:
    // a voluntary association, not a replacement for the government you
    // already live under. The old Myspace joke stays on the genesis quote
    // card. It is not what a stranger should see in a share preview.
    // (2026-11-16: Brandon edits these from /admin/preview; a blank field keeps the built-in text and picture, which now
    // live in features/preview/preview.cpp.)
    const SitePreview preview = site_preview();
    const std::string og_desc = html_escape(preview.description.empty() ? preview_builtin_description() : preview.description);
    const std::string og_title = preview.title.empty() ? full_title : html_escape(preview.title);
    // One shared image site-wide. v4 is a new path because v3 (and v2
    // before it) is cached immutable for a year on crawlers.
    const std::string og_image = preview.image_mime.empty() ? preview_builtin_image_url()
                                                            : "https://liquidgov.us/preview-image?v=" + preview.image_version;
    // Facebook's Sharing Debugger flags og:url as a missing-but-recommended
    // property if it's absent -- it's what the scraper treats as this
    // page's canonical identity (and what like/share counts attribute to).
    // page_wrap has no request path in scope (it's called from inside each
    // route handler, which would all need threading through to do this
    // per-page) -- same "one shared value site-wide" posture as og_desc/
    // og_image above, not a half-finished per-page version of this.
    const std::string og_url = "https://liquidgov.us/";
    const std::string fb_app_id = facebook_app_id_from_env();
    return "<!doctype html><html lang=\"en\""+theme_attr+"><head><meta charset=\"utf-8\">"
           // viewport-fit=cover: without it, env(safe-area-inset-bottom)
           // resolves to 0 everywhere per spec -- every bottom-fixed
           // element's safe-area term (jbtoggle, jbpanel, jbhint)
           // has silently been adding zero this whole time.
           "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,viewport-fit=cover\">"
           "<meta name=\"referrer\" content=\"strict-origin-when-cross-origin\">"
           "<title>"+full_title+"</title>"
           "<meta property=\"og:type\" content=\"website\">"
           "<meta property=\"og:site_name\" content=\"LiquidGov.US\">"
           "<meta property=\"og:url\" content=\""+og_url+"\">"+
           (fb_app_id.empty() ? "" : "<meta property=\"fb:app_id\" content=\""+fb_app_id+"\">") +
           "<meta property=\"og:title\" content=\""+og_title+"\">"
           "<meta property=\"og:description\" content=\""+og_desc+"\">"
           "<meta property=\"og:image\" content=\""+og_image+"\">"
           "<meta property=\"og:image:width\" content=\"1200\">"
           "<meta property=\"og:image:height\" content=\"630\">"
           "<meta name=\"twitter:card\" content=\"summary_large_image\">"
           "<meta name=\"twitter:title\" content=\""+og_title+"\">"
           "<meta name=\"twitter:description\" content=\""+og_desc+"\">"
           "<meta name=\"twitter:image\" content=\""+og_image+"\">"
           "<link rel=\"icon\" href=\"/favicon.svg\" type=\"image/svg+xml\">"
           // Installable as a standalone app (see /install for the actual
           // per-platform instructions): a manifest is what turns Chrome/
           // Edge/Android's browser-menu "Install"/"Add to Home screen"
           // from a plain bookmark into a real standalone window with this
           // name and icon. iOS Safari doesn't read this file at all --
           // its own "Add to Home Screen" (Share sheet) works from nothing
           // more than the page itself, which is why /install still needs
           // to spell that path out separately.
           // One app (Brandon 2026-11-06: "lets get rid of the multiple app
           // icons thing. that was a bad idea"): every page offers the same
           // manifest, icon and name, so Add to Home Screen always gives the
           // one LiquidGov icon. The old Messages/Social/Civic manifests and
           // icons are still served (icons already on phones keep working,
           // and deploy/nginx-www-apex.sh checks one) but nothing links them.
           "<link rel=\"manifest\" href=\"/manifest.webmanifest\">"
           "<meta name=\"apple-mobile-web-app-capable\" content=\"yes\">"
           "<meta name=\"mobile-web-app-capable\" content=\"yes\">"
           "<meta name=\"apple-mobile-web-app-title\" content=\"LiquidGov\">"
           "<link rel=\"apple-touch-icon\" href=\"/apple-touch-icon.png\">"
           "<meta name=\"theme-color\" content=\"#8a6620\">"
           "<link rel=\"preconnect\" href=\"https://fonts.googleapis.com\">"
           "<link rel=\"preconnect\" href=\"https://fonts.gstatic.com\" crossorigin>"
           "<link rel=\"stylesheet\" href=\"https://fonts.googleapis.com/css2?family=Playfair+Display:wght@400;700&display=swap\">"
           "<link rel=\"stylesheet\" href=\""+asset_url("site.css")+"\"><link rel=\"stylesheet\" href=\""+asset_url("setup.css")+"\"><script src=\""+asset_url("setup.js")+"\" defer></script></head><body>"
           +(show_intro ? intro_overlay_html(intro_always_play) : std::string())+
           "<header class=\"top\"><div class=\"topinner\">"
           "<a class=\"brand\" href=\"/\">"
           "<svg class=\"mark\" viewBox=\"0 0 48 48\" width=\"28\" height=\"28\" aria-hidden=\"true\">"
           "<circle cx=\"24\" cy=\"24\" r=\"22.5\" fill=\"none\" style=\"stroke:var(--stone)\" stroke-width=\"1\"/>"
           "<circle cx=\"24\" cy=\"24\" r=\"19\" fill=\"none\" style=\"stroke:var(--accent)\" stroke-width=\"1.2\"/>"
           "<path d=\"M24 9c0 0 9.5 12.2 9.5 17.2a9.5 9.5 0 0 1-19 0C14.5 21.2 24 9 24 9z\" style=\"fill:var(--accent)\"/>"
           "</svg>"
           "<div><div class=\"word\"><strong>LiquidGov</strong><span class=\"tld\">.US</span></div>"
           "<div class=\"tagline\">Civic association &middot; Beta</div></div></a>"
           +[&]{
           std::string nav_out = navhtml;
           std::string header_toggle = toggle;
           const std::string open = "<nav class=\"dropdown-panel accountmenu\">";
           auto at = nav_out.find(open);
           if (at != std::string::npos) {
               nav_out.insert(at + open.size(),
                   "<div class=\"menu-theme-row\">"+toggle+"</div>");
               header_toggle.clear();
           }
           return nav_out+header_toggle;
       }()+"</div></header>"
           "<main class=\"wrap\">"+body+"</main>"
           +
           // "Who holds control" used to sit in this row as a plain link, reading like just another page (Laws, Privacy).
           // It stays reachable everywhere (/control is unchanged) but tucked behind a closed disclosure, same
           // <details><summary> pattern this file already uses for "Delete…" (see e.g. the post-menu one above) --
           // closed by default, one click to reveal, not a routine nav item.
           "<footer class=\"site\"><a href=\"/\">Genesis</a> · <a href=\"/laws\">Laws</a> · "
           "<a href=\"/install\">Install this as an app</a> · "
           "<a href=\"/privacy\">Privacy</a> · "
           // /terms had no link anywhere (site inventory 2026-11-17, q104): it sits beside Privacy, its sibling page
           "<a href=\"/terms\">Terms</a> · "
           "<details style=\"display:inline-block\"><summary class=\"muted\" style=\"cursor:pointer;display:inline\">Disclaimer</summary> "
           "<a href=\"/control\">Who holds control</a></details><br>"
           "Not a government. A platform account is not a citizen identity. "
           "LiquidGov.US is in Beta — some of what's here is still being built.</footer>"
           "</body></html>";
} // defined below, needed here for nav()
 // defined below, needed here for nav()
 // defined below, needed here for jukebox_widget()

std::string nav(const std::string& user, int unread) {
    if (user.empty()) {
        std::string burger = "<label class=\"navbtn\" for=\"navc\">Menu</label><input id=\"navc\" type=\"checkbox\">";
        // "Who holds control" is reachable from here too, but as a closed disclosure, not a top-level nav item --
        // same reasoning and the same <details><summary> pattern as the footer's, just below.
        return burger+"<nav><a href=\"/\">Genesis</a><a href=\"/laws\">Laws</a><a href=\"/styles\">Styles</a>"
               "<details style=\"display:inline-block\"><summary class=\"muted\" style=\"cursor:pointer;display:inline;min-height:44px;align-items:center\">Disclaimer</summary>"
               "<a href=\"/control\">Who holds control</a></details>"
               "<a href=\"/apps\">App</a>"
               "<a href=\"/signup\">Sign up</a><a href=\"/login\">Log in</a></nav>";
    }
    // Notifications and Messages are their own always-visible icon buttons
    // next to the avatar (checked constantly), not entries in the account
    // dropdown (opened a few times a session). unread splits the same way:
    // "message" notices badge Messages, everything else badges
    // Notifications -- see mark_notices_read_of_type/_except_type at the
    // /mail and /notices routes for the matching read-marking split.
    const int msg_unread = g_store.unread_message_count(user);
    const int other_unread = unread - msg_unread;
    // Hand-drawn line icons, not emoji: an emoji glyph's size and even
    // whether it renders at all is up to the platform's emoji font, which
    // proved unreliable here (the bell rendered far oversized, the
    // envelope as a missing-glyph box). An inline SVG with stroke=
    // currentColor renders identically everywhere and themes for free,
    // same reasoning as the brand mark/favicon already being hand-drawn
    // SVG rather than an emoji or icon-font glyph.
    const char* bell_svg =
        "<svg width=\"18\" height=\"18\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.8\" stroke-linecap=\"round\" stroke-linejoin=\"round\" aria-hidden=\"true\">"
        "<path d=\"M18 8a6 6 0 0 0-12 0c0 7-3 9-3 9h18s-3-2-3-9\"/>"
        "<path d=\"M13.73 21a2 2 0 0 1-3.46 0\"/></svg>";
    const char* mail_svg =
        "<svg width=\"18\" height=\"18\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.8\" stroke-linecap=\"round\" stroke-linejoin=\"round\" aria-hidden=\"true\">"
        "<rect x=\"2\" y=\"4\" width=\"20\" height=\"16\" rx=\"2\"/><path d=\"m22 6-10 7L2 6\"/></svg>";
    // Buttons, not links: opening either one has to fire a fetch for the
    // panel's content (see mail_widget_script below), not navigate to
    // /notices or /mail -- those full pages still exist (direct links,
    // bookmarks, JS-off fallback) but are no longer how the header itself
    // gets you there.
    std::string bell = std::string("<button type=\"button\" class=\"iconbtn\" id=\"notiftoggle\" "
        "aria-label=\"Notifications\" title=\"Notifications\">")+bell_svg;
    if (other_unread > 0) bell += "<span class=\"badge\">"+std::to_string(other_unread)+"</span>";
    bell += "</button><div class=\"dropdown-panel notifpanel\" id=\"notifpanel\" hidden>"
            "<div class=\"panelhead\">Notifications</div>"
            "<div id=\"notifroot\"><p class=\"muted\" style=\"padding:.7rem 1.1rem\">Loading&hellip;</p></div>"
            "</div>";
    std::string mail = std::string("<button type=\"button\" class=\"iconbtn\" id=\"mailtoggle\" "
        "aria-label=\"Messages\" title=\"Messages\">")+mail_svg;
    if (msg_unread > 0) mail += "<span class=\"badge\">"+std::to_string(msg_unread)+"</span>";
    mail += "</button><div class=\"dropdown-panel mailpanel\" id=\"mailpanel\" hidden>"
            "<div class=\"panelhead\">Messages</div>"
            "<div id=\"mailroot\"><p class=\"muted\" style=\"padding:.7rem 1.1rem\">Loading&hellip;</p></div>"
            "</div>";
    // Same #navc checkbox-toggle trick the mobile hamburger already uses
    // (see the CSS), just triggered by the avatar instead of a "Menu"
    // label, and not gated to narrow viewports -- clicking your own
    // picture is the trigger at every screen size now.
    std::string avatar = "<label class=\"navavatar\" for=\"navc\">"+portrait_tag(user,"sm")+"</label>"
                          "<input id=\"navc\" type=\"checkbox\">";
    // The menu's own name link goes to the public page anyone else sees
    // (/u/<username>), not the edit screen -- "Edit profile" on that
    // public page and the "Your profile" link on the home dashboard are
    // the way to actually reach /me.
    std::string menu = "<nav class=\"dropdown-panel accountmenu\">"
        "<a class=\"menu-name\" href=\"/u/"+html_escape(user)+"\">"+html_escape(user)+"</a>"
        "<a href=\"/settings\">Settings</a><a href=\"/apps\">App</a>"
        +(is_operator(user) ? "<a href=\"/admin\">Operator</a>" : "")+
        "<hr>"
        "<a href=\"/feed\">Feed</a>"
        "<a href=\"/friends\">People</a>"
        "<a href=\"/map\">Map</a>"
        "<a href=\"/communities\">Groups</a>"
        "<a href=\"/styles\">Styles</a>"
        "<a href=\"/proposals\">Proposals</a>"
        "<a href=\"/officials\">Officials</a>"
        "<a href=\"/laws\">Laws</a>"
        "<form method=\"POST\" action=\"/settings/hints\" class=\"inline\"><input type=\"hidden\" name=\"on\" value=\""+
        std::string(g_store.hints_of(user).on ? "0" : "1")+"\"><button type=\"submit\" class=\"ghost\" "
        "style=\"width:100%;justify-content:flex-start;border:0;border-radius:0;padding:.55rem 1.1rem;min-height:40px;"
        "font-weight:400;font-size:.92rem;color:var(--ink);background:transparent\">Hints: "+
        std::string(g_store.hints_of(user).on ? "on &middot; turn off" : "off &middot; turn on")+"</button></form>"
        "<a href=\"/logout\">Log out</a>"
        "</nav>";
    return bell+mail+avatar+menu+mail_widget_script();
}

}  // namespace liquidgov::site
