#pragma once
// styles.hpp: the profile style library (Brandon 2026-11-12, profile CSS slice 2): pages and rules other modules use.
#include "site/common.hpp"

namespace liquidgov::site {

inline constexpr std::size_t kStyleNameMaxChars = 60;   // code points
inline constexpr std::size_t kStylesPerAuthor = 20;
inline constexpr std::size_t kStyleReportMaxChars = 300;
inline constexpr std::size_t kStylesPerPage = 24;

// A style a viewer may see in the library: it exists, its CSS passes today's sanitize_author_css, and neither the
// viewer nor the author has blocked the other. viewer is "" when signed out.
bool style_visible_to(const ProfileStyle& s, const std::string& viewer);
// The /styles page body. sort is "used" or anything else (newest).
std::string styles_page(const std::string& viewer, const std::string& sort, std::size_t page,
                        const std::map<std::string, std::string>& qs);
// The whole preview document for one style: the profile page's layout with sample content, the site stylesheet and
// the style nested in .lg-profile-scope. No scripts. Shown only inside a sandboxed iframe on /styles.
std::string style_preview_document(const ProfileStyle& s);
// The library part of /me's Profile CSS card: "based on ..." (while the CSS is not empty), the publish form, the
// publish result and a link to /styles.
std::string styles_me_card(const std::string& user, bool has_css, const std::map<std::string, std::string>& qs);
// Why a publish was refused (?err=style&reason=<code>): codes from styles_routes or sanitize_author_css.
std::string style_publish_message(const std::string& code);
// Tidy a style name: control characters dropped, runs of spaces collapsed, trimmed. "" when nothing is left.
std::string tidy_style_name(const std::string& raw);
std::size_t utf8_chars(const std::string& s);
// The first max_chars code points of s (never cuts a character in half).
std::string utf8_prefix(const std::string& s, std::size_t max_chars);

}  // namespace liquidgov::site
