#include "site/features/jukebox/jukebox_script.hpp"
#include "site/ui/assets.hpp"
#include "site/core/html.hpp"

namespace liquidgov::site {
// Floating jukebox widget: a phone-icon button, present on every logged-in
// page (injected from wrap()), that expands into a panel with two modes —
// "Community" (synced to the site-wide queue over /community/ws, same
// real-time pattern as /here/ws) and "My search" (private, local-only
// playback, never touches the server). One YT.Player instance is reused
// across both modes so play/pause/mute/volume always act on whatever's
// currently loaded.
//
// Honest limitation, not hidden: this site is deliberately a multi-page
// app (liquidgov_context_handoff.md's Jukebox section — no site-wide SPA).
// Navigating to a different page resets local playback; the widget
// reconnects and re-syncs to the live community track next time it's
// opened. True across-navigation persistence would need the browser's
// Picture-in-Picture API, deliberately not added here.
// Shared panel guts (player, tabs, controls, community/search sections) —
// identical whether embedded in the floating widget or filling the whole
// /listen popout window.
std::string jukebox_panel_body(bool standalone) {
    return
        // First child of #jbpanel/#jbroot either way this gets embedded
        // (see global_jukebox_widget and jukebox_standalone_page, both of
        // which wrap this same body) -- one shared change covers the
        // floating widget and the /listen popout at once.
        "<div class=\"juketube-plate\">Juketube</div>"
        "<div class=\"row\" style=\"justify-content:space-between;align-items:center\">"
        "<div class=\"row\" style=\"gap:.3rem\">"
        "<button type=\"button\" class=\"ghost\" id=\"jbtab-community\">Stations</button>"
        "<button type=\"button\" class=\"ghost\" id=\"jbtab-search\">Search</button>"
        "</div>"
        "<div class=\"row\" style=\"gap:.3rem\">"
        // Embedded (the phone widget on every page): collapse/expand is
        // the round phone button alone, so neither Pop out nor a second
        // close control belongs here. Standalone (/listen, its own
        // window) keeps both -- Float above to go always-on-top, and its
        // own Close to actually close that window.
        + std::string(standalone
            ? "<button type=\"button\" class=\"ghost\" id=\"jbfloat\" hidden>Float above</button>"
              "<button type=\"button\" class=\"ghost\" id=\"jbclose\" aria-label=\"Close\">\xE2\x9C\x95</button>"
            : "")
        + "</div>"
        "</div>"
        "<div id=\"jbplayer\" style=\"margin-top:.6rem;aspect-ratio:16/9;background:#000;"
        "border-radius:var(--rsm)\"></div>"
        "<div class=\"row\" style=\"margin-top:.5rem;align-items:center;gap:.4rem\">"
        "<button type=\"button\" class=\"ghost\" id=\"jbplaypause\">Play</button>"
        "<button type=\"button\" class=\"ghost\" id=\"jbmute\">Mute</button>"
        "<input type=\"range\" id=\"jbvolume\" min=\"0\" max=\"100\" value=\"70\" style=\"flex:1\">"
        "</div>"
        "<div id=\"jbcommunity-ui\">"
        // Priority-ordered, drag-to-reorder list of stations tuned in on
        // *this* widget -- see jukebox_script's stations block for the
        // reorder/click-to-temporarily-prioritize logic. Deliberately not
        // an account preference: "on the phone" is this device's own
        // widget config, in localStorage, same as it staying collapsed
        // across a soft-nav is this tab's own state.
        "<p class=\"muted\" style=\"margin:.5rem 0 .15rem;font-size:.72rem;letter-spacing:.08em;"
        "text-transform:uppercase\">Stations &mdash; drag to reorder, tap to tune in now</p>"
        "<div id=\"jbstationlist\" style=\"display:flex;flex-direction:column;gap:.2rem\"></div>"
        "<select id=\"jbstationadd\" style=\"margin-top:.35rem\"><option value=\"\">Add a station&hellip;</option></select>"
        "<div class=\"row\" style=\"margin-top:.3rem;align-items:flex-start\">"
        "<input id=\"jbfollow\" placeholder=\"Follow someone's Juketube\" autocomplete=\"off\" style=\"flex:1\">"
        "<button type=\"button\" class=\"ghost\" id=\"jbfollowbtn\">Follow</button>"
        "</div>"
        "<p class=\"muted\" id=\"jbfollowerr\" style=\"margin:.2rem 0 0\" hidden></p>"
        "<p class=\"muted\" id=\"jbnow\" style=\"margin:.6rem 0 0\">Connecting&hellip;</p>"
        "<p class=\"muted\" id=\"jbupcoming\" style=\"margin:.1rem 0 0\"></p>"
        "<input id=\"jbadd\" placeholder=\"Add to the queue\" autocomplete=\"off\" "
        "style=\"margin-top:.4rem;width:100%\">"
        "<div id=\"jbaddresults\" style=\"margin-top:.3rem;display:flex;flex-direction:column;gap:.15rem\"></div>"
        "<p class=\"muted\" id=\"jbadderr\" style=\"margin:.2rem 0 0\" hidden></p>"
        "</div>"
        "<div id=\"jbsearch-ui\" hidden>"
        "<select id=\"jbsearchcat\" style=\"width:100%\">"
        "<option value=\"all\">All</option><option value=\"group\">Group</option>"
        "<option value=\"friends\">Friends</option><option value=\"others\">Others</option>"
        "<option value=\"self\">Self</option></select>"
        "<input id=\"jbsearch\" placeholder=\"Search people and groups\" autocomplete=\"off\" "
        "style=\"margin-top:.4rem;width:100%\">"
        "<div id=\"jbsearchresults\" style=\"margin-top:.3rem;display:flex;flex-direction:column;gap:.15rem\"></div>"
        "<label class=\"muted\" style=\"display:block;margin-top:.7rem;font-size:.72rem;letter-spacing:.08em;"
        "text-transform:uppercase\">History</label>"
        "<select id=\"jbhistorysel\" style=\"width:100%;margin-top:.2rem\">"
        "<option value=\"\">Something you&rsquo;ve played before&hellip;</option></select>"
        "</div>";
}

// Shared controller JS. `standalone` selects between the two hosting
// contexts: embedded (toggle button + collapsible panel, Pop-out button
// opens /listen) vs the /listen window itself (always visible, Close button
// really closes the window, and — since *this* window's own document never
// navigates away as long as it stays open — a working "Float above" button
// using the real Document Picture-in-Picture API, moving the whole panel
// into an always-on-top OS-level window for as long as this tab stays open).
std::string jukebox_script(const std::string& yt_key, bool standalone, const std::string& stations_js,
                            const std::string& own_username) {
    (void)yt_key; // search goes through /jukebox/search so the key never ships in HTML
    // The script is assets/jukebox.js (was inlined here; moved 2026-11-12, same code). Its 3 per-page values
    // go in LG_JUKEBOX first, from the same expressions as before. No defer: it runs where the inline one did.
    return "<script>var LG_JUKEBOX={standalone:" + std::string(standalone ? "true" : "false") +
           ",ownUsername:" + js_string_literal(own_username) + ",stations:" + stations_js +
           "};</script><script src=\"" + asset_url("jukebox.js") + "\"></script>";
}

}  // namespace liquidgov::site
