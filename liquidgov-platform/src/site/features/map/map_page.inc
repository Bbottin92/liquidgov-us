// A real dashboard for a returning user, in the spirit of the mobile
// design's home screen (stat tiles, a live "rooms open now" list) --
// built from data the site already has (posts_by/friends_of/communities_of,
// the live /world player map, per-station jukebox now-playing state)
// rather than any of that design's new, unbuilt concepts (Studio, geolocated
// "people near you").
// The MAINstream map page (assets/lgmap.js draws it). Shared by /map and, for a signed-in person, by "/":
// Brandon 2026-11-07, "pages should be accessed through the map". <noscript> keeps the plain drawing.
std::string map_page_body(const std::string& user, const std::string& focus) {
    return
            "<link rel=\"stylesheet\" href=\""+asset_url("lgmap.css")+"\">"
            "<div id=\"lgmap\" class=\"lgmap\" data-focus=\""+html_escape(focus)+"\" data-me=\""+html_escape(user)+"\">"
            "<div class=\"lgmap-bar\">"
            "<button type=\"button\" id=\"lgm-main\" class=\"lgm-main\" title=\"Back to the MAINstream; shrinks the field\">MAINstream</button>"
            // the "Turn on alerts" pill sits here, beside MAINstream, not in its own row above the map: that row pushed a
            // phone's first view down ~60 px, under the Juketube tip and past the fold (CI phone 4/4b, bar B5, hidden H6)
            "<span data-lg-alerts-slot></span>"
            // Phones: a tap grows connections, a hold opens the page (Brandon 2026-11-09); the hint says so, since a
            // long press is invisible unless something tells you about it (side-agent note).
            "<span class=\"lgm-hint muted\"><span class=\"lgm-hint-wide\">Click a box to grow its connections and open its page.</span>"
            "<span class=\"lgm-hint-narrow\">Tap a box to see its connections &middot; double-tap or <strong>hold it</strong> to open its page.</span>"
            " Bigger boxes are closer to you.</span>"
            "<span id=\"lgm-status\" class=\"lgm-status\" role=\"status\"></span>"
            "<a class=\"lgm-privacy\" href=\"/settings#map-privacy\">Map privacy</a></div>"
            "<div class=\"lgmap-body\">"
            // the trail (M1.1 look): You > Groups > ... ; a div, since the phone CSS hides every <nav>
            // map SPEC phase 1 (Brandon 2026-11-11): Back, the trail and Zoom out on one row, then the filter bar for
            // this level (lgmap.js fills it); both stay at the top on a phone (position:sticky, lgmap.css)
            "<div class=\"lgmap-canvas\"><div id=\"lgm-bars\" class=\"lgm-bars\">"
            "<div class=\"lgm-navrow\"><button type=\"button\" id=\"lgm-back\" class=\"lgm-navbtn\" disabled>&lsaquo; Back</button>"
            "<div id=\"lgm-trail\" class=\"lgm-trail\" role=\"navigation\" aria-label=\"Where you are on the map\"></div>"
            "<button type=\"button\" id=\"lgm-zoomout\" class=\"lgm-navbtn\" title=\"Up one level; from You, the MAINstream\">Zoom out</button></div>"
            "<div id=\"lgm-filter\" class=\"lgm-filter\" role=\"group\" aria-label=\"Show at this level\"></div>"
            "<p id=\"lgm-filter-note\" class=\"lgm-filter-note\" hidden>Hiding only changes what this map shows on this device. "
            "Nothing is deleted, and nobody else's view changes.</p></div>"
            "<svg id=\"lgm-svg\" xmlns=\"http://www.w3.org/2000/svg\" aria-label=\"Map\">"
            "<g id=\"lgm-view\"><g id=\"lgm-edges\"></g><g id=\"lgm-nodes\"></g></g></svg></div>"
            "<aside id=\"lgm-panel\" class=\"lgmap-panel\" hidden>"
            "<div class=\"lgm-ptop\"><strong id=\"lgm-ptitle\"></strong>"
            // view-only until Edit (Brandon 2026-11-11 4:30 PM decision 2; live map #11): lgmap.js shows this button
            // only when the page in the panel has edit forms
            "<button type=\"button\" id=\"lgm-edit\" hidden>Edit</button>"
            "<button type=\"button\" id=\"lgm-expand\" title=\"Fill the window\">Expand</button>"
            // no href until lgmap.js's openPanel sets the box's page (CODE-SCAN-UX A3: href="#" jumped to the top)
            "<a id=\"lgm-open\" target=\"_top\">Open page</a>"
            "<button type=\"button\" id=\"lgm-close\" aria-label=\"Close\">&times;</button></div>"
            "<iframe id=\"lgm-frame\" title=\"Selected item\"></iframe></aside></div>"
            "<noscript>"+main_stream_svg()+kCityMapStyle+"</noscript>"
            "<div class=\"lgm-legend muted\">"
            "<span class=\"lgm-scale\">Far from you <i aria-hidden=\"true\"></i> Close to you</span>"
            "<span>Thicker lines: paths you use often (kept on this device only)</span>"
            "<button type=\"button\" id=\"lgm-forget\" class=\"ghost\">Forget my visits</button>"
            "<span class=\"lgm-shapes\" aria-label=\"Shapes\">""<span><svg viewBox=\"0 0 26 12\"><rect class=\"lgs\" width=\"26\" height=\"12\" rx=\"6\"/></svg>people</span>""<span><svg viewBox=\"0 0 26 12\"><rect class=\"lgs\" width=\"26\" height=\"12\" rx=\"1.5\"/></svg>groups</span>""<span><svg viewBox=\"0 0 26 12\"><ellipse class=\"lgs\" cx=\"13\" cy=\"6\" rx=\"13\" ry=\"6\"/></svg>cities</span>""<span><svg viewBox=\"0 0 26 12\"><polygon class=\"lgs\" points=\"4,0 22,0 26,6 22,12 4,12 0,6\"/></svg>proposals</span>""<span><svg viewBox=\"0 0 26 12\"><polygon class=\"lgs\" points=\"3,0 23,0 26,3 26,9 23,12 3,12 0,9 0,3\"/></svg>offices</span>""<span><svg viewBox=\"0 0 26 12\"><polygon class=\"lgs\" points=\"0,0 20,0 26,6 20,12 0,12\"/></svg>problems</span>""<span><svg viewBox=\"0 0 26 12\"><rect class=\"lgs\" width=\"26\" height=\"12\" rx=\"3\"/></svg>posts</span></span>""<a href=\"/home\">Your dashboard</a>""</div>"
            "<p class=\"muted lgm-foot\">Colors come from what you're in and have done; only you see them. "
            "Cities are labels people chose, not governments. You only see what people have made visible.</p></div>"
            // People you may know, under the map: a signed-in person lands on this page, also right after signup
            + people_suggestions_html(user, 3, "/") +
            "<script src=\""+asset_url("lgmap.js")+"\" defer></script>";
}

