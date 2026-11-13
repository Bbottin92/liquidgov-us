#include "site/ui/wrap.hpp"
#include "site/core/adapters_store.hpp"
#include "site/ui/assets.hpp"
#include "site/ui/favicon_cookies.hpp"
#include "site/ui/shell.hpp"
#include "site/core/text.hpp"

namespace liquidgov::site {
 // defined below, needed here for wrap()

// Your droplet on every page (Brandon 2026-11-15: "every page should just include the droplet that way we keep things
// universal and standardized and consolidated"). A page that already has a walker (a profile page, with other people's
// dots: here_page_bits; the map; the home page) keeps its own; every other page gets yours on its own (here.js with no
// room: nothing is sent anywhere). Not when this browser hid it (Settings > Your droplet, the lg_drop=off cookie), and
// not on a page that asks not to (<!--lg:no-drop-->: the drawing tool, where the pad would sit on the canvas).
// Before </body>, outside <main>, so it is never inside what a page swaps out.
// Video links play where they are shared (Brandon 2026-11-15; assets/video_links.js): on every page this builds.
static void add_video_links(std::string& page) {
    auto pos = page.rfind("</body>");
    if (pos != std::string::npos) page.insert(pos, "<script src=\"" + asset_url("video_links.js") + "\" defer></script>");
}

static void add_drop(std::string& page, const std::string& user) {
    if (g_drop_off || page.find("/walker.js") != std::string::npos || page.find("<!--lg:no-drop-->") != std::string::npos) return;
    auto pos = page.rfind("</body>");
    if (pos == std::string::npos) return;
    // usernames are [A-Za-z0-9._-] only, so JSON-escaped text cannot end the script
    page.insert(pos, "<script>window.LG_HERE={\"me\":\"" + json_escape(user) + "\",\"page\":\"\",\"box\":\"\"};</script>"
                     "<script src=\"" + asset_url("walker.js") + "\" defer></script>"
                     "<script src=\"" + asset_url("here.js") + "\" defer></script>");
}

// Hints for newcomers (assets/hints.js): only for a signed-in person whose Hints setting is on, outside framed pages.
static void add_hints(std::string& page, const std::string& user) {
    if (user.empty()) return;
    const HintPrefs hp = g_store.hints_of(user);
    if (!hp.on) return;
    auto pos = page.rfind("</body>");
    if (pos == std::string::npos) return;
    std::string ids;   // ids are letters, digits, - and _ only (set_hints), so they cannot end the script
    for (const auto& d : hp.dismissed) ids += std::string(ids.empty() ? "" : ",") + "\"" + d + "\"";
    page.insert(pos, "<script>window.LG_HINTS={\"done\":[" + ids + "]};</script>"
                     "<script src=\"" + asset_url("hints.js") + "\" defer></script>");
}

std::string wrap(const std::string& title, const std::string& user, int unread, const std::string& body, bool show_intro) {
    // Signed in: the account's pref decides on the server (pages_routes only asks for the intro when it is on), so the
    // overlay has no local gate. Anonymous visitors: their browser's pref, read by intro.js.
    const bool intro_always_play = show_intro && !user.empty() && g_store.prefs_of(user).intro_every_visit;
    // The one-time 988 notice goes on the next page this person sees, framed or not. Framed pages (the map's field,
    // and every page while Juketube's persistent shell keeps music playing) used to skip it, so someone browsing
    // with music on could go a long time without it (side-agent catch, Brandon 2026-11-08: fix it). Only visible
    // frames render wrap() in embed mode: the profile-theme preview frame (/u/<name>/theme) uses http_raw, not wrap.
    std::string care;
    if (!user.empty() && g_care_notice.erase(user)) {
        care = "<div class=\"card\" role=\"note\" style=\"border-left:4px solid var(--accent)\">"
               "<p><strong>If things feel heavy right now, you don't have to carry it alone.</strong> "
               "In the US you can call or text <a href=\"tel:988\"><strong>988</strong></a> "
               "(Suicide &amp; Crisis Lifeline) any time. It's free and confidential.</p>"
               "<p class=\"muted\">Crisis words are not saved as a problem, and nobody is told.</p></div>";
    }
    if (g_embed) {
        // Inside a frame (the map's right-hand field, or the Juketube shell): the page alone, no header,
        // footer, intro or jukebox. Links and forms keep working in the field.
        std::string page = page_wrap(title, "", care + body, false, false);
        const std::string fit =
            "<style>header.top,footer.site{display:none!important}"
            "main.wrap{padding-top:.75rem;padding-bottom:1.5rem}</style>";
        auto head = page.find("</head>");
        if (head != std::string::npos) page.insert(head, fit);
        add_drop(page, user);
        add_video_links(page);
        return page;
    }
    std::string page = page_wrap(title, nav(user, unread), care + body, show_intro, intro_always_play);
    // Widget goes *after* </main>, not inside it: soft-navigation (see
    // jukebox_script's nav-interception) only ever replaces <main>'s
    // contents, so a sibling widget's live WebSocket/YT player survive a
    // page-to-page click untouched instead of being torn down and rebuilt.
    if (!user.empty()) {
        std::string widget =
            "<link rel=\"stylesheet\" href=\""+asset_url("juketube.css")+"\">"
            "<div id=\"juketube-root\"></div>"
            "<script src=\""+asset_url("tip.js")+"\" defer></script><script src=\""+asset_url("juketube.js")+"\" defer></script><script src=\""+asset_url("push.js")+"\" defer></script>";
        auto pos = page.find("</main>");
        if (pos != std::string::npos) page.insert(pos + 7, widget);
    }
    add_drop(page, user);
    add_hints(page, user);
    add_video_links(page);
    return page;
}

}  // namespace liquidgov::site
