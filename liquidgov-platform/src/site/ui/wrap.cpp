#include "site/ui/wrap.hpp"
#include "site/core/adapters_store.hpp"
#include "site/ui/assets.hpp"
#include "site/ui/favicon_cookies.hpp"
#include "site/ui/shell.hpp"

namespace liquidgov::site {
 // defined below, needed here for wrap()

std::string wrap(const std::string& title, const std::string& user, int unread, const std::string& body, bool show_intro) {
    // Anonymous visitors have no prefs to consult -- they always get the
    // plain first-time-only (localStorage) behavior, same as before this
    // setting existed.
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
            "<script src=\""+asset_url("juketube.js")+"\" defer></script><script src=\""+asset_url("push.js")+"\" defer></script>";
        auto pos = page.find("</main>");
        if (pos != std::string::npos) page.insert(pos + 7, widget);
    }
    return page;
}

}  // namespace liquidgov::site
