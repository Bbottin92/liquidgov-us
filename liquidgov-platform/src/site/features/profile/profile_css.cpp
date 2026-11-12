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
    };
    auto it = kMessages.find(code);
    return it == kMessages.end() ? "That wasn't accepted." : it->second;
}

CssCheckResult sanitize_author_css(const std::string& css_in) {
    if (css_in.size() > kAuthorCssMaxBytes) return {false, "size"};
    std::string lower = css_in;
    for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    static const std::pair<const char*, const char*> kDenied[] = {
        {"@import", "import"},
        {"@font-face", "font-face"},
        {"@namespace", "namespace"},
        {"@layer", "layer"},
        {"@scope", "scope"},
        {"url(", "url"},
        {"expression(", "expression"},
        {"-moz-binding", "moz-binding"},
        {"behavior:", "behavior"},
        {"javascript:", "javascript"},
        {"</style", "style-tag"},
        {"<style", "style-tag"},
        {"<script", "script-tag"},
    };
    for (const auto& [needle, code] : kDenied) {
        if (lower.find(needle) != std::string::npos) return {false, code};
    }
    return {true, ""};
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
