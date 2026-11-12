#include "site/ui/favicon_cookies.hpp"

namespace liquidgov::site {
std::string favicon_svg() {
    // Same ring-and-droplet mark as the header .mark, in literal colors
    // -- a standalone favicon document has no page CSS to read
    // var(--accent) from.
    return "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 48 48\">"
           "<rect width=\"48\" height=\"48\" fill=\"#14120f\"/>"
           "<circle cx=\"24\" cy=\"24\" r=\"21\" fill=\"none\" stroke=\"#6b6458\" stroke-width=\"1.5\"/>"
           "<circle cx=\"24\" cy=\"24\" r=\"17.5\" fill=\"none\" stroke=\"#d1a85c\" stroke-width=\"2\"/>"
           "<path d=\"M24 9c0 0 9.5 12.2 9.5 17.2a9.5 9.5 0 0 1-19 0C14.5 21.2 24 9 24 9z\" fill=\"#d1a85c\"/></svg>";
}

// Same hand-drawn-line-icon treatment as nav()'s bell/mail (stroke=
// currentColor, no emoji) -- the composer's file-attach control, standing
// in for the old "Choose File" button.
const char* paperclip_svg() {
    return "<svg width=\"18\" height=\"18\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" "
           "stroke-width=\"1.8\" stroke-linecap=\"round\" stroke-linejoin=\"round\" aria-hidden=\"true\">"
           "<path d=\"M21 11.5 12.4 20a4.2 4.2 0 0 1-6-6L14.8 5.7a2.8 2.8 0 0 1 4 4L10.4 18a1.4 1.4 0 0 1-2-2l7.6-7.6\"/></svg>";
}

// The /feed audience chips' own glyphs -- globe (Anybody, reach), two
// heads (Friends), three overlapping rooms (Groups, a destination not a
// visibility level). Same stroke-icon treatment as every other hand-drawn
// glyph on this site (currentColor, no fill).
const char* globe_svg() {
    return "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke-width=\"1.7\" stroke-linecap=\"round\" "
           "stroke-linejoin=\"round\" aria-hidden=\"true\"><circle cx=\"12\" cy=\"12\" r=\"8.5\"/>"
           "<path d=\"M3.5 12h17M12 3.5c2.6 2.6 4 5.4 4 8.5s-1.4 5.9-4 8.5c-2.6-2.6-4-5.4-4-8.5s1.4-5.9 4-8.5Z\"/></svg>";
}
const char* friends_svg() {
    return "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke-width=\"1.7\" stroke-linecap=\"round\" "
           "stroke-linejoin=\"round\" aria-hidden=\"true\"><circle cx=\"8.7\" cy=\"8.2\" r=\"2.9\"/>"
           "<path d=\"M2.8 19c.6-3.4 2.9-5.3 5.9-5.3s5.3 1.9 5.9 5.3\"/><circle cx=\"16.6\" cy=\"7.4\" r=\"2.3\"/>"
           "<path d=\"M14.8 13.6c2.3.2 4 1.9 4.6 5.1\"/></svg>";
}
const char* groups_svg() {
    return "<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke-width=\"1.6\" stroke-linecap=\"round\" "
           "stroke-linejoin=\"round\" aria-hidden=\"true\"><circle cx=\"9\" cy=\"9\" r=\"6\"/>"
           "<circle cx=\"15\" cy=\"9\" r=\"6\"/><circle cx=\"12\" cy=\"15\" r=\"6\"/></svg>";
}

// "dark", "light", or "" (no explicit choice -- prefers-color-scheme
// decides). Set once per request in handle() (the whole site's single-
// threaded synchronous entry point), read here at render time.
thread_local std::string g_theme_mode;
thread_local bool g_https = false;
// True when this page is loading inside the map's right-hand field (the
// browser sends Sec-Fetch-Dest: iframe, also on redirects and clicks inside
// it; ?embed=1 forces it for tests). wrap() then drops the site header,
// footer and jukebox so the page fits the field. Set per request in handle().
thread_local bool g_embed = false;
// People whose last public words contained crisis language ("I wish I wasn't
// here"). The pain-point feature stores nothing for them; their next page
// shows the 988 line once. In memory only, never written to disk.
std::set<std::string> g_care_notice;

std::string set_cookie(const std::string& sid) {
    std::string o = "Set-Cookie: lg_session="+sid+"; HttpOnly; Path=/; SameSite=Lax; Max-Age=2592000";
    if (g_https) o += "; Secure";
    o += "\r\n";
    return o;
}
std::string clear_cookie() {
    std::string o = "Set-Cookie: lg_session=; HttpOnly; Path=/; SameSite=Lax; Max-Age=0";
    if (g_https) o += "; Secure";
    o += "\r\n";
    return o;
}

}  // namespace liquidgov::site
