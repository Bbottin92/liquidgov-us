#pragma once
// preview.hpp: the site's link preview, the card Facebook, X, Slack and the rest draw when someone shares a liquidgov.us
// link. Brandon 2026-11-16: "i just want to be able to change the preview label thing myself from the admin controls
// page": one site-wide title, description and image, edited on /admin/preview; blank means the built-in preview
// (shell.cpp). Kept in two small files next to the store (<store>.preview and <store>.preview-image), not a new LGP1
// record type: an older program ignores extra files, so a rollback needs no undo script, and a missing file simply
// means the built-in preview.
#include "site/common.hpp"

namespace liquidgov::site {

struct SitePreview {
    std::string title, description;   // empty = the built-in one
    std::string image_mime;           // "" = no custom image
    std::string image_version;        // changes when the image does (the address carries it, so crawlers refetch)
};
inline constexpr std::size_t kPreviewTitleMax = 120, kPreviewDescMax = 300, kPreviewImageMax = 2 * 1024 * 1024;

// The built-in preview, used for anything left blank (it was in shell.cpp).
std::string preview_builtin_description();
std::string preview_builtin_image_url();
// What is set now. Cached for a second, so a page render does not read the file every time.
SitePreview site_preview();
// Save the text (trimmed, control characters removed, cut to the limits). False if the file cannot be written.
bool site_preview_save_text(const std::string& title, const std::string& description);
// Save the image if it is a JPEG or PNG within kPreviewImageMax; returns "" on success, else why not ("type", "size", "write").
std::string site_preview_set_image(const std::vector<std::uint8_t>& bytes);
void site_preview_remove_image();
void site_preview_reset();   // back to the built-in preview
// The image bytes and mime, if one is set.
std::optional<std::pair<std::string, std::string>> site_preview_image();

}  // namespace liquidgov::site
