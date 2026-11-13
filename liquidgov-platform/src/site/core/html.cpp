#include "site/core/html.hpp"

namespace liquidgov::site {
std::string html_escape(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': o += '&'; o += "amp;"; break;
            case '<': o += '&'; o += "lt;"; break;
            case '>': o += '&'; o += "gt;"; break;
            case '"': o += '&'; o += "quot;"; break;
            default: o += c; break;
        }
    }
    return o;
}

// Turns a bare http(s):// run inside plain post text into a real link,
// html-escaping everything else exactly as html_escape alone would. Only
// recognizes a URL already present in the text the author typed or pasted
// -- no oEmbed/unfurl lookup, no separate stored title, so most links still
// render as themselves (see post_block's own comment on why that's fine
// here).
std::string linkify_post_text(const std::string& raw) {
    std::string out;
    std::size_t i = 0;
    auto is_url_char = [](unsigned char c) {
        return !std::isspace(c) && c != '<' && c != '>' && c != '"';
    };
    while (i < raw.size()) {
        const bool is_link = raw.compare(i, 8, "https://") == 0 || raw.compare(i, 7, "http://") == 0;
        if (!is_link) { out += html_escape(raw.substr(i, 1)); ++i; continue; }
        std::size_t j = i;
        while (j < raw.size() && is_url_char(static_cast<unsigned char>(raw[j]))) ++j;
        // Trailing punctuation is more likely to be the sentence's than the
        // URL's -- "see https://example.com." shouldn't link the period.
        while (j > i && std::string(".,;:!?)").find(raw[j - 1]) != std::string::npos) --j;
        std::string esc = html_escape(raw.substr(i, j - i));
        out += "<a href=\"" + esc + "\" target=\"_blank\" rel=\"noopener noreferrer\">" + esc + "</a>";
        i = j;
    }
    return out;
}

// Quotes a server-controlled (not user-controlled) string as a single-quoted
// JS string literal embedded in generated HTML, e.g. the YouTube API key.
std::string composer_attach_bar(const char* counter_id, const std::string& canvas_href) {
    // The Canvas link sits OUTSIDE the photo label: a tap on it can never reach the file input (2026-11-12: on iPhone
    // Safari the unclipped invisible input covered the old "Draw instead" link and opened the photo picker).
    const std::string canvas = canvas_href.empty() ? "" :
        "<a class=\"btn ghost canvaspick\" href=\"" + html_escape(canvas_href) + "\" aria-label=\"Canvas: draw or paint a post\">"
        "<svg width=\"18\" height=\"18\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"1.8\" "
        "stroke-linecap=\"round\" stroke-linejoin=\"round\" aria-hidden=\"true\">"
        "<path d=\"M18.4 2.6a2.1 2.1 0 0 1 3 3L11.2 15.8l-3-3z\"/>"
        "<path d=\"M8.2 12.8c-2.3 0-3.7 1.5-3.7 3.7 0 1.4-.9 2.3-2.5 2.6 1 1.4 2.9 2.1 4.7 2.1 2.8 0 4.7-1.9 4.7-4.7\"/></svg>"
        "Canvas</a>";
    return "<div class=\"composerbar\">"
           "<label class=\"filepick\" title=\"Add a photo or video\">"
           "<svg width=\"18\" height=\"18\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" "
           "stroke-width=\"1.8\" stroke-linecap=\"round\" stroke-linejoin=\"round\" aria-hidden=\"true\">"
           "<rect x=\"3\" y=\"5\" width=\"18\" height=\"14\" rx=\"2\"/>"
           "<circle cx=\"8.5\" cy=\"10\" r=\"1.5\"/>"
           "<path d=\"m21 15-5-5L5 19\"/></svg>"
           "<input type=\"file\" name=\"media\" accept=\"image/*,video/*\"></label>"
           + canvas +
           "<span class=\"grow\"></span>"
           "<span class=\"muted\" id=\"" + std::string(counter_id) + "\">2000 left</span>"
           "<button type=\"submit\">Post</button></div>";
}

std::string js_string_literal(const std::string& s) {
    std::string o = "'";
    for (char c : s) {
        switch (c) {
            case '\'': o += "\\'"; break;
            case '\\': o += "\\\\"; break;
            case '<': o += "\\x3c"; break;
            default: o += c; break;
        }
    }
    o += "'";
    return o;
}

// Gates a Visibility-tiered surface (profile, posts) for one viewer.
// is_owner always sees their own; otherwise Public admits anyone (including
// an anonymous visitor), Members requires a signed-in account, Friends
// requires the relationship, and OnlyMe admits nobody else at all.
bool visible_to(Visibility v, bool is_owner, bool viewer_signed_in, bool is_friend) {
    if (is_owner) return true;
    switch (v) {
        case Visibility::Public: return true;
        case Visibility::Members: return viewer_signed_in;
        case Visibility::Friends: return is_friend;
        case Visibility::OnlyMe: default: return false;
    }
}

}  // namespace liquidgov::site
