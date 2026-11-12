#include "site/core/adapters_store.hpp"

namespace liquidgov::site {


const char* store_path_from_env() {
    const char* p = std::getenv("LIQUIDGOV_STORE");
    return p && p[0] ? p : "liquidgov_accounts.lgp";
}
const char* blob_path_from_env() {
    const char* p = std::getenv("LIQUIDGOV_BLOBS");
    return p && p[0] ? p : "liquidgov_blobs";
}
// Client-side YouTube search (Data API v3 search.list) needs a key. Read-only
// key, meant to be HTTP-referrer-restricted in Google Cloud Console to this
// site's origin — same trust level as any client-embedded map/analytics key.
// Empty means "not configured"; the search box degrades to a message instead
// of failing silently.
const char* youtube_api_key_from_env() {
    const char* p = std::getenv("YOUTUBE_API_KEY");
    return p && p[0] ? p : "";
}
// Google Identity Services client ID (a public identifier, not a secret
// -- the Client Secret this app never touches at all, matching "Not a
// second auth stack" / "Don't put Client Secret in HTML"). Empty means
// "not configured yet"; the Google sign-in button just doesn't render
// rather than shipping a broken one (see google_signin_button_html()).
const char* google_client_id_from_env() {
    const char* p = std::getenv("LIQUIDGOV_GOOGLE_CLIENT_ID");
    return p && p[0] ? p : "";
}
// Facebook App ID (a public identifier, not a secret) for the fb:app_id
// meta tag -- lets Facebook's Sharing Debugger attribute like/share/comment
// counts on a shared link to a real app instead of flagging it as a
// missing property. Empty means "not configured yet"; the tag just
// doesn't render rather than shipping a made-up or wrong app id (see
// page_wrap's fb_app_id).
const char* facebook_app_id_from_env() {
    const char* p = std::getenv("LIQUIDGOV_FACEBOOK_APP_ID");
    return p && p[0] ? p : "";
} // defined below (needs trim_ws), used here by nav()

FileStore g_store{store_path_from_env()};
FilesystemBlobStore g_blobs{blob_path_from_env()};
PngMagicScanner g_scanner;
MailAttachmentScanner g_mail_scanner;
SizeCaps g_mail_caps; // defaults from file_transfer.hpp
NeverBreached g_breach;
AllowLimiter g_limiter;
SiteSocialGraph g_social_graph;

bool SiteSocialGraph::are_friends(AccountId a, AccountId b) const {
    auto ua = g_store.find_by_id(a);
    auto ub = g_store.find_by_id(b);
    if (!ua || !ub) return false;
    return g_store.are_friends(ua->username, ub->username);
}

}  // namespace liquidgov::site
