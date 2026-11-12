#include "site/features/styles/styles.hpp"
#include "site/core/adapters_store.hpp"
#include "site/core/html.hpp"
#include "site/core/http.hpp"
#include "site/features/profile/profile_css.hpp"
#include "site/ui/assets.hpp"

namespace liquidgov::site {

// The profile style library (Brandon 2026-11-12, profile CSS slice 2; Persistence/notes.md). A style is a frozen copy
// of someone's Profile CSS, published under a name. "Use this style" copies it into the user's own Profile CSS (no
// live link). Every place a style's CSS is shown or copied checks it again with today's sanitize_author_css, so a rule
// tightened later also covers styles published before it.

std::size_t utf8_chars(const std::string& s) {
    std::size_t n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) ++n;
    return n;
}

std::string utf8_prefix(const std::string& s, std::size_t max_chars) {
    std::size_t n = 0, i = 0;
    for (; i < s.size(); ++i) {
        if ((static_cast<unsigned char>(s[i]) & 0xC0) != 0x80) {
            if (n == max_chars) break;
            ++n;
        }
    }
    return s.substr(0, i);
}

std::string tidy_style_name(const std::string& raw) {
    std::string out;
    bool space = false;
    for (unsigned char c : raw) {
        if (c < 0x20 || c == 0x7F) c = ' ';
        if (c == ' ') { space = !out.empty(); continue; }
        if (space) { out += ' '; space = false; }
        out += static_cast<char>(c);
    }
    return out;
}

std::string style_publish_message(const std::string& code) {
    if (code == "name") return "Give the style a name, up to " + std::to_string(kStyleNameMaxChars) + " characters.";
    if (code == "empty") return "Save some Profile CSS first: publishing shares the CSS you have saved.";
    if (code == "limit") return "You have " + std::to_string(kStylesPerAuthor) + " styles in the library, the most one "
                                "person can have. Remove one first.";
    if (code == "gone") return "That style isn't in the library any more.";
    return "Not published: " + css_reject_message(code);
}

bool style_visible_to(const ProfileStyle& s, const std::string& viewer) {
    if (!sanitize_author_css(s.css).ok) return false;
    if (!viewer.empty() && (g_store.is_blocked(viewer, s.author) || g_store.is_blocked(s.author, viewer))) return false;
    return true;
}

namespace {

std::string uses_text(std::size_t n) {
    return std::to_string(n) + (n == 1 ? " use" : " uses");
}

std::string style_card(const ProfileStyle& s, std::size_t uses, const std::string& viewer) {
    const std::string id = std::to_string(s.id);
    const std::string name = html_escape(s.name);
    std::string html = "<article class=\"card lg-style-card\" id=\"style-" + id + "\" data-style-name=\"" + name + "\">"
        // sandbox with no allow-* at all: no scripts, an opaque origin, no forms, no top navigation. The browser keeps
        // the style inside this frame, whatever the filter misses (Brandon: "no style escapes its own area").
        "<iframe class=\"lg-style-preview\" sandbox src=\"/styles/" + id + "/preview\" loading=\"lazy\" "
        "referrerpolicy=\"no-referrer\" tabindex=\"-1\" title=\"Preview of " + name + "\"></iframe>"
        "<h3 class=\"lg-style-name\">" + name + "</h3>"
        "<p class=\"muted lg-style-meta\">by <a href=\"/u/" + html_escape(s.author) + "\">" + html_escape(s.author) +
        "</a> &middot; <span class=\"lg-style-uses\">" + uses_text(uses) + "</span></p>";
    if (!viewer.empty()) {
        html += "<div class=\"row lg-style-acts\">"
                "<form class=\"inline\" method=\"POST\" action=\"/styles/use\" "
                "onsubmit=\"return confirm('Replace your Profile CSS with a copy of this style? You can change it after.');\">"
                "<input type=\"hidden\" name=\"id\" value=\"" + id + "\"><button type=\"submit\">Use this style</button></form>";
        if (viewer == s.author) {
            html += "<form class=\"inline\" method=\"POST\" action=\"/styles/remove\" "
                    "onsubmit=\"return confirm('Remove this style from the library? Copies people made stay theirs.');\">"
                    "<input type=\"hidden\" name=\"id\" value=\"" + id + "\"><button class=\"ghost\" type=\"submit\">Remove</button></form>";
        }
        html += "</div>";
        if (viewer != s.author) {
            html += "<details class=\"lg-style-report\"><summary>Report</summary>"
                    "<form method=\"POST\" action=\"/styles/report\">"
                    "<input type=\"hidden\" name=\"id\" value=\"" + id + "\">"
                    "<label for=\"why-" + id + "\">What's wrong with it? (optional)</label>"
                    "<input id=\"why-" + id + "\" name=\"reason\" maxlength=\"" + std::to_string(kStyleReportMaxChars) + "\">"
                    "<button class=\"ghost\" type=\"submit\">Send report</button></form></details>";
        }
    }
    html += "</article>";
    return html;
}

}  // namespace

std::string styles_page(const std::string& viewer, const std::string& sort, std::size_t page,
                        const std::map<std::string, std::string>& qs) {
    const bool by_use = sort == "used";
    struct Row { ProfileStyle s; std::size_t uses; };
    std::vector<Row> rows;
    for (auto& s : g_store.styles())  // newest first
        if (style_visible_to(s, viewer)) rows.push_back({s, g_store.style_use_count(s.id)});
    if (by_use)
        std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.uses > b.uses; });

    std::string html = "<h1>Profile styles</h1>"
        "<p class=\"muted\">Looks people made for their profile pages. <strong>Use this style</strong> copies it into "
        "your own Profile CSS: change your copy any time, and the author's later changes never touch it. To share "
        "yours, publish it from <a href=\"/me#profile-css\">your profile settings</a>.</p>";
    auto ok = qs.find("ok");
    if (ok != qs.end()) {
        if (ok->second == "published") html += "<p class=\"ok\" role=\"status\">Published. It's at the top of Newest.</p>";
        if (ok->second == "reported") html += "<p class=\"ok\" role=\"status\">Thanks. An operator will look at it.</p>";
        if (ok->second == "removed") html += "<p class=\"ok\" role=\"status\">Removed from the library.</p>";
    }
    if (qs.count("err") && qs.at("err") == "gone")
        html += "<p class=\"warn\" role=\"status\">" + html_escape(style_publish_message("gone")) + "</p>";
    html += std::string("<div class=\"segtabs\" role=\"tablist\">") +
        "<a class=\"btn" + (by_use ? " ghost" : "\" aria-current=\"page") + "\" href=\"/styles\">Newest</a>"
        "<a class=\"btn" + (by_use ? "\" aria-current=\"page" : " ghost") + "\" href=\"/styles?sort=used\">Most used</a></div>";
    if (rows.empty()) {
        html += "<p class=\"empty\">No styles yet. Be the first: publish yours from <a href=\"/me#profile-css\">your "
                "profile settings</a>.</p>";
        return html;
    }
    const std::size_t pages = (rows.size() + kStylesPerPage - 1) / kStylesPerPage;
    if (page < 1) page = 1;
    if (page > pages) page = pages;
    html += "<div class=\"lg-style-grid\">";
    for (std::size_t i = (page - 1) * kStylesPerPage; i < rows.size() && i < page * kStylesPerPage; ++i)
        html += style_card(rows[i].s, rows[i].uses, viewer);
    html += "</div>";
    if (pages > 1) {
        const std::string base = std::string("/styles?") + (by_use ? "sort=used&" : "") + "page=";
        html += "<p class=\"row\">";
        if (page > 1) html += "<a class=\"btn ghost\" href=\"" + base + std::to_string(page - 1) + "\">Previous</a>";
        html += "<span class=\"muted\">Page " + std::to_string(page) + " of " + std::to_string(pages) + "</span>";
        if (page < pages) html += "<a class=\"btn ghost\" href=\"" + base + std::to_string(page + 1) + "\">Next</a>";
        html += "</p>";
    }
    return html;
}

std::string style_preview_document(const ProfileStyle& s) {
    // Same nesting as the real /u page (lg-profile-outer > lg-profile-scope > lg-theme), so a style looks here as it
    // will on a profile. Sample content only: nothing about the author or the viewer.
    return "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
           "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
           "<link rel=\"stylesheet\" href=\"" + asset_url("site.css") + "\">"
           "<style>body{margin:0;padding:.6rem;overflow:hidden}.lg-profile-outer{min-height:calc(100vh - 1.2rem)}"
           ".card{margin:.6rem 0;padding:1rem}h1{margin:.2rem 0}</style>" +
           author_css_block(s.css) +
           "</head><body><div class=\"lg-profile-outer\" style=\"contain:paint;isolation:isolate;position:relative\">"
           "<div class=\"lg-profile-scope\"><div class=\"lg-theme\">"
           "<div class=\"card\"><div class=\"who\"><span class=\"portrait ph\" aria-hidden=\"true\">S</span>"
           "<div><h1>sample</h1><p class=\"lede\" style=\"margin:.2rem 0 .4rem\">Sample Person</p>"
           "<p class=\"muted\">Riverside &middot; from Fresno &middot; Librarian</p>"
           "<p>Here is how a bio reads in this style.</p>"
           "<table class=\"record\"><tr><td>Standing</td><td>Associate &middot; good</td></tr></table>"
           "</div></div></div>"
           "<p class=\"eyebrow\">Groups</p><div class=\"row\"><span class=\"chip\">Tower Garden</span>"
           "<span class=\"chip\">Friday Jam</span></div>"
           "<h2>Posts</h2><div class=\"card\"><div class=\"post\"><p>A post as it looks on this profile. "
           "<a href=\"#\">A link</a>.</p></div></div>"
           "</div></div></div></body></html>";
}

std::string styles_me_card(const std::string& user, bool has_css, const std::map<std::string, std::string>& qs) {
    std::string html;
    auto ok = qs.find("ok");
    if (ok != qs.end() && ok->second == "style-used")
        html += "<p class=\"ok\" role=\"status\">Copied into your Profile CSS. Change it any time; the original "
                "stays as it is.</p>";
    auto err = qs.find("err");
    if (err != qs.end() && err->second == "style") {
        auto why = qs.find("reason");
        html += "<p class=\"warn\" role=\"status\">" +
                html_escape(style_publish_message(why == qs.end() ? "" : why->second)) + "</p>";
    }
    // "Based on" stays through edits to the copy (Brandon 2026-11-12) and goes when the CSS is cleared or the style
    // leaves the library.
    if (has_css) {
        if (auto base = g_store.style_based_on(user)) {
            html += "<p class=\"muted lg-based-on\">Based on <a href=\"/u/" + html_escape(base->author) + "\">" +
                    html_escape(base->author) + "</a>&rsquo;s <a href=\"/styles#style-" + std::to_string(base->id) +
                    "\">" + html_escape(base->name) + "</a>.</p>";
        }
        html += "<form class=\"lg-style-publish\" method=\"POST\" action=\"/styles/publish\">"
                "<label for=\"style-name\">Share your saved CSS in the <a href=\"/styles\">style library</a></label>"
                "<div class=\"row\"><input id=\"style-name\" name=\"name\" required maxlength=\"" +
                std::to_string(kStyleNameMaxChars) + "\" placeholder=\"Name it, e.g. Night Paper\" style=\"flex:1 1 12rem;width:auto\">"
                "<button type=\"submit\">Publish</button></div></form>"
                "<p class=\"muted\">Publishing shares a copy of your CSS as it is now, with your username. Later "
                "changes to your own CSS don't change it; you can remove it from the library.</p>";
    } else {
        html += "<p class=\"muted\">Or start from someone else's: browse the <a href=\"/styles\">style library</a>.</p>";
    }
    return html;
}

}  // namespace liquidgov::site
