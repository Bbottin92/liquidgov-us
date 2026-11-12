#include "site/features/profile/profile_css.hpp"
#include "site/core/html.hpp"

namespace liquidgov::site {


// code -> human-readable reason, rendered server-side on the settings page.
// Codes only ever come from sanitize_author_css below, never user input, so
// this lookup is a fixed, closed set (a missing code just falls back to a
// generic message).
std::string css_reject_message(const std::string& code) {
    static const std::map<std::string, std::string> kMessages = {
        {"size", "That's over the " + std::to_string(kAuthorCssMaxBytes / 1000) + "KB limit."},
        {"import", "Not allowed in profile CSS: @import."},
        {"font-face", "Not allowed in profile CSS: @font-face."},
        {"namespace", "Not allowed in profile CSS: @namespace."},
        {"layer", "Not allowed in profile CSS: @layer."},
        {"scope", "Not allowed in profile CSS: @scope."},
        {"url", "Not allowed in profile CSS: url()."},
        {"expression", "Not allowed in profile CSS: expression()."},
        {"moz-binding", "Not allowed in profile CSS: -moz-binding."},
        {"behavior", "Not allowed in profile CSS: behavior:."},
        {"javascript", "Not allowed in profile CSS: javascript:."},
        {"style-tag", "Not allowed in profile CSS: a style tag."},
        {"script-tag", "Not allowed in profile CSS: a script tag."},
        {"escape", "Not allowed in profile CSS: backslash escapes (\\)."},
        {"nesting", "Not allowed in profile CSS: the & selector. To style the whole area, use .lg-theme."},
        {"braces", "Every { needs its matching }, and every /* comment needs its */."},
        {"image", "Not allowed in profile CSS: image-set(), image(), cross-fade() and other image functions."},
        {"at-rule", "Not allowed in profile CSS: @-rules other than @media, @supports and @container."},
    };
    auto it = kMessages.find(code);
    return it == kMessages.end() ? "That wasn't accepted." : it->second;
}

// Profile CSS runs on the REAL /u/<name> page now (Brandon 2026-11-12), nested inside .lg-profile-scope{...} (see
// author_css_block), not in the old scriptless sandbox frame -- so this filter is the defense, not a backstop:
//  - no backslash at all: CSS escapes (u\72l(, @\69mport) would walk past every word check below;
//  - braces balance outside strings and comments, and no comment is left open: a stray } would close the scope wrapper
//    and let the rest style the whole page (header, nav);
//  - no & (the nesting selector): html:has(&) header{...} reaches outside the scope;
//  - no way to load anything: url(), image-set() and the other image functions, @import, @font-face;
//  - only @media, @supports and @container; any other @-rule is refused by name.
// Words are matched on the CSS with comments and strings taken out (so content:"&" or /* url( */ is fine), except
// the tag checks, which read the whole text: "</style" inside a CSS string still ends the <style> element.
CssCheckResult sanitize_author_css(const std::string& css_in) {
    if (css_in.size() > kAuthorCssMaxBytes) return {false, "size"};
    if (css_in.find('\\') != std::string::npos) return {false, "escape"};
    std::string lower = css_in;
    for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    static const std::pair<const char*, const char*> kTags[] = {
        {"</style", "style-tag"}, {"<style", "style-tag"}, {"<script", "script-tag"},
    };
    for (const auto& [needle, code] : kTags) {
        if (lower.find(needle) != std::string::npos) return {false, code};
    }
    // bare: the lowercased CSS without comments and with every string emptied; braces counted on the way
    std::string bare;
    int depth = 0;
    bool in_comment = false;
    char quote = 0;
    for (std::size_t i = 0; i < lower.size(); ++i) {
        const char c = lower[i];
        if (in_comment) {
            if (c == '*' && i + 1 < lower.size() && lower[i + 1] == '/') { in_comment = false; ++i; }
            continue;
        }
        if (quote) {
            if (c == quote || c == '\n') quote = 0;  // a CSS string also ends at a newline (bad-string)
            continue;
        }
        if (c == '/' && i + 1 < lower.size() && lower[i + 1] == '*') { in_comment = true; ++i; bare += ' '; continue; }
        if (c == '"' || c == '\'') { quote = c; bare += "\"\""; continue; }
        if (c == '{') ++depth;
        if (c == '}' && --depth < 0) return {false, "braces"};
        bare += c;
    }
    if (in_comment || depth != 0) return {false, "braces"};
    if (bare.find('&') != std::string::npos) return {false, "nesting"};
    static const std::pair<const char*, const char*> kDenied[] = {
        {"@import", "import"},
        {"@font-face", "font-face"},
        {"@namespace", "namespace"},
        {"@layer", "layer"},
        {"@scope", "scope"},
        {"url(", "url"},
        {"image-set(", "image"},
        {"image(", "image"},
        {"cross-fade(", "image"},
        {"element(", "image"},
        {"src(", "image"},
        {"paint(", "image"},
        {"expression(", "expression"},
        {"-moz-binding", "moz-binding"},
        {"behavior:", "behavior"},
        {"javascript:", "javascript"},
    };
    for (const auto& [needle, code] : kDenied) {
        if (bare.find(needle) != std::string::npos) return {false, code};
    }
    for (std::size_t at = bare.find('@'); at != std::string::npos; at = bare.find('@', at + 1)) {
        std::size_t end = at + 1;
        while (end < bare.size() && (std::isalnum(static_cast<unsigned char>(bare[end])) || bare[end] == '-')) ++end;
        const std::string name = bare.substr(at + 1, end - at - 1);
        if (name != "media" && name != "supports" && name != "container") return {false, "at-rule"};
    }
    return {true, ""};
}

std::string author_css_block(const std::string& css) {
    return css.empty() ? "" : "<style>.lg-profile-scope{" + css + "}</style>";
}

// A profile-background drawing's CSS, for the real /u/<username> page's
// own identity card (portrait, name, bio, standing) -- not the sandboxed
// /theme iframe, which only ever shows a custom author_css theme now.
// Each background-image value is two layers: a translucent parchment
// wash (color-mix against --panel, so it themes light/dark for free)
// painted on top of the frame image via CSS's own multi-layer background
// stacking, so the card's text stays readable over whatever got drawn
// underneath without needing a separate overlay element.
std::string profile_canvas_css(const std::string& username, std::size_t frame_count, bool timelapse) {
    if (frame_count == 0) return "";
    auto frame_url = [&](std::size_t i) {
        return "/canvas-frame/" + username + "/" + std::to_string(i);
    };
    const std::string wash = "color-mix(in srgb, var(--panel) 78%, transparent)";
    auto layered = [&](std::size_t i) {
        return "linear-gradient(" + wash + "," + wash + "),url(" + frame_url(i) + ")";
    };
    std::ostringstream os;
    os << "#profilecard{background-size:cover;background-position:center;background-repeat:no-repeat}";
    if (!timelapse || frame_count == 1) {
        os << "#profilecard{background-image:" << layered(frame_count - 1) << "}";
        return os.str();
    }
    const double step_seconds = 0.6;
    os << "@keyframes lg-profile-canvas-loop{";
    for (std::size_t i = 0; i < frame_count; ++i) {
        double pct = (static_cast<double>(i) / static_cast<double>(frame_count)) * 100.0;
        os << pct << "%{background-image:" << layered(i) << "}";
    }
    os << "100%{background-image:" << layered(0) << "}";
    os << "}";
    os << "#profilecard{animation:lg-profile-canvas-loop " << (step_seconds * static_cast<double>(frame_count))
       << "s steps(1) infinite}";
    return os.str();
}

}  // namespace liquidgov::site
