// LiquidGov.US site — C++ HTTP + platform auth.
// Persistence: FileStore (LGP1 snapshot). Not liquidgov-core's ledger.
#include "liquidgov/platform/auth.hpp"
#include "liquidgov/platform/avatar.hpp"
#include "liquidgov/platform/blob.hpp"
#include "liquidgov/platform/clock.hpp"
#include "liquidgov/platform/file_store.hpp"
#include "liquidgov/platform/file_transfer.hpp"
#include "liquidgov/platform/file_type.hpp"
#include "liquidgov/platform/messenger.hpp"
#include "liquidgov/platform/websocket.hpp"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <functional>
#include <thread>
#include <unordered_map>
#include <vector>
#include "lg_log.hpp"
using namespace liquidgov::platform;
namespace {
constexpr int kPort = 8080;
// Mail attachments run through file_transfer's default caps (image 10 MiB,
// document/PDF/OOXML 25 MiB — see SizeCaps in file_transfer.hpp). The read
// loop's ceiling has to cover the largest of those plus multipart overhead.
constexpr std::size_t kMaxBody = 25ull * 1024 * 1024 + 64 * 1024;
// Room for the multipart wrapping and form fields around a program upload on /mail/big.
constexpr std::size_t kMultipartRoom = 1024 * 1024;
std::mutex g_mu;

// --- World: one shared room, connected users see each other move. ---
// Deliberately separate from g_mu / g_store: this is ephemeral in-memory
// position state, never persisted, touched only by world session threads
// (one per connected /world/ws client) — the rest of the server stays on
// its existing single-threaded accept loop, untouched by this feature.
struct WorldPlayer {
    double x = 128, y = 128;
    std::string username;
    std::string color;
};
std::mutex g_world_mu;
std::unordered_map<int, WorldPlayer> g_world_players; // keyed by client socket fd

std::string world_color_for(const std::string& username) {
    static const char* kPalette[] = {"#a23b3b", "#3b6ea2", "#3ba25e", "#a2823b",
                                      "#6e3ba2", "#3ba297", "#a23b8f", "#5f7a2e"};
    std::size_t h = std::hash<std::string>{}(username);
    return kPalette[h % 8];
}

// --- Live jukebox "stations": one shared, synced-elapsed-position queue
// per station that everyone connected to that station's socket hears
// "live" (new joiners seek to the current track's elapsed position).
// Stations are keyed by slug -- "" is the one original site-wide station
// (still /community/ws, unchanged for existing callers), any other slug
// is that community's own jukebox (/c/<slug>/jukebox/ws), gated to
// members of that community since "everyone in the group can hear" means
// the group, not the whole site the way the site-wide one is. One mutex,
// one pair of maps, same one-thread-per-connection pattern as /world/ws.
struct CommunityTrack {
    std::string video_id;
    std::string queued_by;
};
// A shared freeze, not a radio that keeps running in an empty room: while
// at least one listener is connected there's one playhead, ticking off
// resumed_at; the moment the last one disconnects it's frozen at whatever
// elapsed had reached (frozen_offset), and stays there -- no wall-clock
// addition -- until someone shows up again. See community_elapsed_locked,
// the one formula every reader shares so they can't drift apart from each
// other. Durable copy lives in FileStore under the jpos tag (keyed the
// same as station: "" for site-wide, "c:<slug>" for a community, "u:
// <username>" for a personal jukebox), written on last-listener-leave and
// roughly every 15s while playing, so a process restart resumes from
// there too instead of jumping back to 0:00.
struct CommunityNowPlaying {
    std::string video_id;
    std::string queued_by;
    std::int64_t frozen_offset = 0; // seconds elapsed before the current listening session (all of it, if nobody's connected)
    std::int64_t resumed_at = 0;    // wall-clock unix when listener_count last went 0->1; meaningless while listener_count==0
    bool active = false;
};
struct YtCacheEntry { std::string body; std::int64_t exp = 0; };
std::mutex g_yt_mu;
std::map<std::string, YtCacheEntry> g_yt_cache;
std::mutex g_community_mu;
std::map<std::string, CommunityNowPlaying> g_community_now_by_station;
std::map<std::string, std::deque<CommunityTrack>> g_community_upcoming_by_station;
std::map<std::string, int> g_community_listeners_by_station;
std::map<std::string, std::int64_t> g_community_last_flush_by_station;
// Recently-played, per station (bug: the History section never showed a
// shared station's actual history -- it fell back to the viewer's OWN
// personal plays instead, since nothing ever recorded a shared station's
// plays anywhere and the one route that served history only understood a
// personal owner). In-memory only, capped (kCommunityHistoryCap), same
// posture as every other g_community_* map here: real history for a
// personal jukebox is durable (FileStore), a shared station's "now
// playing" position is durable too (jpos), but a shared station's past
// tracklist resetting across a restart is an honest, low-risk trade-off
// for not touching the LGP file format for this.
constexpr std::size_t kCommunityHistoryCap = 15;
// A saved spot past this is not resumed. The client used to restart it at
// 0:00, so the track never reached its end and every later click was queued
// behind it. Three hours matches the old client cap.
constexpr std::int64_t kJukeboxResumeMaxSec = 3 * 3600;
std::map<std::string, std::deque<CommunityTrack>> g_community_history_by_station;
// Juketube fix (Brandon 2026-11-08, "do a and b"): one listener's player error or a bare {"ended":true} used to end the
// track for the whole station. Now a playback error is a vote: {"unplayable":"<id>"} per connection, and the track is
// skipped once at least 25% of connected listeners have reported it. Keyed by station: the track it counts for, and how many.
std::map<std::string, std::pair<std::string, int>> g_community_unplayable_by_station;
// A named "ended" from someone who is not alone counts only after this many seconds of the track, so a crafted message
// can't skip a song the room just started.
constexpr std::int64_t kCommunityMinEndedSec = 20;

// Caller must hold g_community_mu.
std::int64_t community_elapsed_locked(const std::string& station, std::int64_t now) {
    const auto& np = g_community_now_by_station[station];
    const auto count = g_community_listeners_by_station[station];
    return np.frozen_offset + (count > 0 ? std::max<std::int64_t>(0, now - np.resumed_at) : 0);
}

// Caller must hold g_community_mu. A fresh track always starts at 0 --
// jpos for it gets written by the caller right after (see
// run_community_session), not here, since this file doesn't have g_store
// declared yet at this point in the translation unit.
void community_advance_locked(const std::string& station, std::int64_t now) {
    auto& upcoming = g_community_upcoming_by_station[station];
    if (upcoming.empty()) {
        g_community_now_by_station[station] = CommunityNowPlaying{};
        return;
    }
    auto next = upcoming.front();
    upcoming.pop_front();
    g_community_now_by_station[station] = CommunityNowPlaying{next.video_id, next.queued_by, 0, now, true};
}

class NeverBreached : public BreachChecker {
public:
    bool is_breached(const std::string&) const override { return false; }
};
class AllowLimiter : public RateLimiter {
public:
    bool allow(const std::string&, const std::string&) override { return true; }
    void record_attempt(const std::string&, const std::string&) override {}
};
// Not a malware engine. Normalized avatars are re-encoded PNG; this
// rejects anything that is not a PNG signature. Always-true would be a lie.
class PngMagicScanner : public MalwareScanner {
public:
    bool is_clean(const std::vector<std::uint8_t>& bytes) const override {
        static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
        if (bytes.size() < 8) return false;
        return std::memcmp(bytes.data(), sig, 8) == 0;
    }
};
// Same category of placeholder as PngMagicScanner, generalized to every
// type ingest_file allows. Not a malware engine — a real deployment plugs
// one in here. This re-checks that the bytes actually being stored still
// match a known-good signature; always-true would be a lie.
class MailAttachmentScanner : public MalwareScanner {
public:
    bool is_clean(const std::vector<std::uint8_t>& bytes) const override {
        return detect_type(bytes) != DetectedType::Unknown;
    }
};

// Adapter over FileStore's username-keyed friend graph, mapping through
// AccountId as messenger.hpp requires. is_minor has no AgeAssurance
// backing yet — returns false for everyone rather than fail-safe-true,
// per the closed pin: treating everyone as a minor would brick the site.
// The compensating control is that 1:1 mail stays friends-only regardless
// of this predicate (unchanged, enforced separately in the /mail routes
// below) — this predicate only ever matters for the group minor
// invariant, which is consequently unpaid until AgeAssurance lands.
class SiteSocialGraph : public SocialGraph {
public:
    bool are_friends(AccountId a, AccountId b) const override;
    bool is_minor(AccountId) const override { return false; }
};

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
}
bool is_operator(const std::string& username); // defined below (needs trim_ws), used here by nav()

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

std::string json_escape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    o += buf;
                } else {
                    o += static_cast<char>(c);
                }
        }
    }
    return o;
}

// Pulls one top-level "key":"value" string field out of a small JSON blob
// -- not a real parser, just enough to read YouTube's oEmbed response
// (see GET /jukebox/oembed) without pulling in a JSON library for one
// field. Handles the few escapes oEmbed titles actually contain
// (backslash, quote, \n) and skips \uXXXX rather than decoding it, which
// is rare enough in a video title that the fallback of leaving nothing
// there is an acceptable, honest gap.
std::string oembed_json_string_field(const std::string& json, const std::string& key) {
    auto keypos = json.find("\"" + key + "\"");
    if (keypos == std::string::npos) return "";
    auto colon = json.find(':', keypos);
    if (colon == std::string::npos) return "";
    auto q1 = json.find('"', colon);
    if (q1 == std::string::npos) return "";
    std::string out;
    for (std::size_t i = q1 + 1; i < json.size(); ++i) {
        char c = json[i];
        if (c == '"') break;
        if (c == '\\' && i + 1 < json.size()) {
            char nc = json[++i];
            if (nc == 'n') out += ' ';
            else if (nc == 'u' && i + 4 < json.size()) i += 4;
            else out += nc;
        } else {
            out += c;
        }
    }
    return out;
}

// Safely wraps s as a single POSIX shell argument by replacing every
// embedded ' with '\'' (close quote, escaped literal quote, reopen
// quote) -- safe against any input regardless of what characters it
// contains. Needed anywhere user-controlled content (a submitted email
// address, in particular -- looks_like_email() only checks shape, not
// charset, so a crafted address containing a shell metacharacter is
// otherwise a real command-injection risk in a popen() call) lands in a
// shell command this file builds. The existing curl call sites that
// url_encode their input first don't need this separately -- url_encode's
// own output charset is already shell-safe -- this is for content (like
// a JSON POST body) that isn't URL-encoded.
std::string shell_single_quote(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

// One shared "GET this HTTPS URL via the system curl binary" helper --
// see /jukebox/search's own comment for why curl-via-popen rather than a
// linked TLS/HTTP stack (this binary doesn't link libcurl, and curl is
// already on the box). Factored out of what used to be two near-
// identical inline popen() blocks; a third call site (Google tokeninfo
// verification) is why it's a real function now instead of a third
// copy. Caller must url_encode anything untrusted before it lands in
// url -- this does not escape anything itself.
// curl_exit (optional) gets curl's exit status: 0 when an answer came back (an HTTP error page too: no -f), curl's
// own code when nothing did (6 no DNS, 7 refused, 28 timeout, 35 TLS...), -1 when curl could not be started. Site
// sweep 2026-11-09: it was thrown away, so "unreachable" looked like an empty answer. curl's error line itself
// already reaches the service log (-sS prints it to our stderr).
std::string curl_get(const std::string& url, int timeout_sec, int* curl_exit = nullptr) {
    std::string cmd = "curl -sS --max-time " + std::to_string(timeout_sec) + " '" + url + "'";
    std::string resp;
    int status = -1;
    if (FILE* p = popen(cmd.c_str(), "r")) {
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), p)) > 0) resp.append(buf, n);
        int st = pclose(p);
        status = (st != -1 && WIFEXITED(st)) ? WEXITSTATUS(st) : -1;
    }
    if (curl_exit) *curl_exit = status;
    return resp;
}

// Best-effort password-reset delivery, tried in this order:
//   1. RESEND_API_KEY set -> Resend's HTTPS API (POST, JSON, via curl --
//      the one call site in this file that needs curl to POST, not GET).
//   2. LIQUIDGOV_SMTP_URL set -> a raw SMTP send via curl's own smtp://
//      support, message piped over the child's stdin (popen in "w" mode)
//      rather than ever appearing in the command line. UNVERIFIED beyond
//      code review -- there's no SMTP server or real credentials
//      anywhere in this dev environment to test it against. Test path 3
//      below first; only trust this once it's been tried against a real
//      mailbox.
//   3. Neither set -> log the reset URL to stderr (captured by journald
//      under systemd: `journalctl -u liquidgov`) instead of mailing
//      anything. This is the actually-testable path and is what "so
//      Brandon can test" in the spec refers to.
// Never throws, never returns anything the caller could use to change
// the HTTP response -- POST /forgot shows the same page regardless of
// whether this silently failed, on purpose (see that route's comment on
// not becoming an account-enumeration oracle via mail-delivery status).
void send_password_reset_email(const std::string& to_email, const std::string& reset_url) {
    const char* resend_key = std::getenv("RESEND_API_KEY");
    const char* smtp_url = std::getenv("LIQUIDGOV_SMTP_URL");
    const std::string subject = "Reset your LiquidGov.US password";
    const std::string body =
        "Someone (hopefully you) asked to reset the password on your LiquidGov.US account.\n\n"
        "This link works once, for the next hour:\n" + reset_url + "\n\n"
        "If you didn't ask for this, you can ignore it -- your password hasn't changed.";

    if (resend_key && resend_key[0]) {
        std::string json = "{\"from\":\"LiquidGov.US <noreply@liquidgov.us>\",\"to\":[\"" +
            json_escape(to_email) + "\"],\"subject\":\"" + json_escape(subject) +
            "\",\"text\":\"" + json_escape(body) + "\"}";
        std::string cmd = "curl -sS --max-time 8 -X POST https://api.resend.com/emails "
            "-H " + shell_single_quote("Authorization: Bearer " + std::string(resend_key)) + " "
            "-H 'Content-Type: application/json' "
            "-d " + shell_single_quote(json) + " -o /dev/null -w '%{http_code}'";
        std::string code;
        if (FILE* p = popen(cmd.c_str(), "r")) {
            char buf[16]; size_t n;
            while ((n = fread(buf, 1, sizeof(buf), p)) > 0) code.append(buf, n);
            pclose(p);
        }
        std::cerr << "[password-reset] resend send to " << (code == "200" || code == "202" ? "ok" : "FAILED (http " + code + ")")
                   << std::endl;
        return;
    }

    if (smtp_url && smtp_url[0]) {
        const std::string from = "noreply@liquidgov.us";
        std::string message = "From: LiquidGov.US <" + from + ">\r\n"
            "To: <" + to_email + ">\r\n"
            "Subject: " + subject + "\r\n"
            "Content-Type: text/plain; charset=utf-8\r\n\r\n" + body + "\r\n";
        std::string cmd = "curl -sS --max-time 10 " + shell_single_quote(smtp_url) +
            " --mail-from " + shell_single_quote(from) +
            " --mail-rcpt " + shell_single_quote(to_email) +
            " --upload-file - -o /dev/null";
        int status = -1;
        if (FILE* p = popen(cmd.c_str(), "w")) {
            fwrite(message.data(), 1, message.size(), p);
            status = pclose(p);
        }
        std::cerr << "[password-reset] smtp send " << (status == 0 ? "ok" : "FAILED (status " + std::to_string(status) + ")")
                   << std::endl;
        return;
    }

    // No mailer configured: the URL goes to stderr only, never to the
    // browser (that's how POST /forgot's own page copy already reads --
    // this is the deliberate "Brandon can still test locally" fallback).
    std::cerr << "[password-reset] no mailer configured -- reset URL for "
               << to_email << ": " << reset_url << std::endl;
}

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
std::string composer_attach_bar(const char* counter_id) {
    return "<div class=\"composerbar\">"
           "<label class=\"filepick\" title=\"Add a photo or video\">"
           "<svg width=\"18\" height=\"18\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" "
           "stroke-width=\"1.8\" stroke-linecap=\"round\" stroke-linejoin=\"round\" aria-hidden=\"true\">"
           "<rect x=\"3\" y=\"5\" width=\"18\" height=\"14\" rx=\"2\"/>"
           "<circle cx=\"8.5\" cy=\"10\" r=\"1.5\"/>"
           "<path d=\"m21 15-5-5L5 19\"/></svg>"
           "<input type=\"file\" name=\"media\" accept=\"image/*,video/*\"></label>"
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

// Author CSS for /u/<username>/theme. This is a deliberately narrow v1 of
// the "Author CSS isolation" design in LIQUIDGOV_CONTEXT_HANDOFF.md, not
// the full spec (no CSSOM-walking parser, no compute budget, no gallery) --
// see that doc's "Still open on this layer" note. What this v1 keeps from
// the spec, because skipping it would be a real hole, not a shortcut:
//   - Reject outright rather than best-effort strip. A rejected save with
//     a clear reason beats a silently mangled stylesheet.
//   - Deny @import, @font-face, @namespace, @layer, @scope, url(),
//     expression(), -moz-binding, behavior:, javascript: -- the constructs
//     that let CSS alone exfiltrate data, load arbitrary resources, or run
//     script. url() is denied outright (no "already in the upload
//     pipeline" allowance yet -- that needs the blob pipeline wired in,
//     not just a text filter).
//   - The theme document itself renders in a sandboxed iframe with no
//     allow-same-origin and no allow-scripts (see /u/<username>/theme and
//     its embedding below), so even a gap in this text filter can't reach
//     cookies, storage, or script execution -- belt and suspenders, not
//     the only defense.
// Not defended against in this v1 (named, not hidden): :has()/sibling
// combinators aimed at platform-injected nodes inside the themed document.
// Lower severity here specifically because the sandboxed iframe has no
// platform chrome/badges inside it to target in the first place.
constexpr std::size_t kAuthorCssMaxBytes = 20000;
// Caps how many snapshots one drawing session can capture -- keeps the
// timelapse @keyframes CSS (one background-image per frame) and blob
// storage bounded regardless of how long someone leaves the canvas open.
constexpr std::size_t kCanvasMaxFrames = 24;

struct CssCheckResult {
    bool ok = true;
    std::string code; // short token, safe to carry in a redirect query string as-is
};

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

// Asset URLs with a content version (2026-11-07 iPhone sweep): every script and stylesheet was served no-store and
// re-downloaded on every page view, and the hand-typed ?v=N had drifted (one place ?v=14, another none). asset_url
// returns "/assets/<name>?v=<hash of the file>". The hash is redone whenever the file's modification time changes, not
// only at startup: a file copied in by hand without a restart still gets a new URL, so no phone can stay on an old copy
// for the year it may cache it (side-agent catch). A request that carries ?v= may be cached for a year (asset_cache_header).
std::string asset_url(const std::string& name) {
    static std::mutex mu;
    static std::map<std::string, std::pair<long long, std::string>> done;   // name -> (mtime ns, url)
    std::lock_guard<std::mutex> lock(mu);
    struct stat st{};
    const long long mtime = ::stat(("assets/" + name).c_str(), &st) == 0
        ? static_cast<long long>(st.st_mtim.tv_sec) * 1000000000LL + st.st_mtim.tv_nsec : -1;
    auto it = done.find(name);
    if (it != done.end() && it->second.first == mtime) return it->second.second;
    std::ifstream in("assets/" + name, std::ios::binary);
    std::string url = "/assets/" + name;
    if (in) {
        std::ostringstream ss; ss << in.rdbuf();
        const std::string data = ss.str();
        unsigned long long h = 1469598103934665603ULL;            // FNV-1a 64
        for (unsigned char c : data) { h ^= c; h *= 1099511628211ULL; }
        char hex[17];
        std::snprintf(hex, sizeof hex, "%016llx", h);
        url += "?v=" + std::string(hex, 10);
    } else {
        // missing file: no version, so it is never cached for a year; the route itself answers 404
        std::cerr << "asset_url: assets/" << name << " not readable; serving it unversioned\n";
    }
    done[name] = {mtime, url};
    return url;
}

// Cache-Control for an asset request: a versioned URL (?v=...) never changes, so a year and immutable; a bare one
// (an old page, a bookmark) keeps today's no-store so it can never go stale.
std::string asset_cache_header(const std::string& rawpath) {
    return rawpath.find("?v=") != std::string::npos || rawpath.find("&v=") != std::string::npos
        ? "Cache-Control: public, max-age=31536000, immutable\r\n"
        : "Cache-Control: no-store\r\n";
}

std::string css() {
    return
        ":root{--paper:#f7f1e4;--parchment:#efe8d8;--panel:#f3ecdd;--ink:#1a1814;"
        "--accent:#8a6620;--stone:#6b6458;--rule:rgba(26,24,20,.14);--danger:#6b2424;"
        "--warn:#7a4a12;--ok:#1e3d32;--rsm:6px;--rmd:8px;--rlg:10px;"
        // Night river 2026-11-10: sections are near-white cards that lift off the page
        "--card:#fffdf8;--cardshadow:0 2px 6px rgba(60,44,12,.08),0 10px 28px rgba(60,44,12,.08);"
        "--display:\"Playfair Display\",\"Iowan Old Style\",Palatino,\"Palatino Linotype\",Georgia,serif;"
        "--body:\"Segoe UI\",system-ui,-apple-system,sans-serif}"
        // Same tokens, ink warmed rather than pure black -- system dark
        // mode applies automatically unless an explicit choice (data-theme,
        // set by the header toggle, see page_wrap) overrides it either way.
        // --accent was --navy (a cool blue) until this project's real,
        // separately-deployed liquidgov.us site turned up in the mobile
        // design's own reference uploads -- same near-black/parchment
        // structure this theme already had, but a warm brass/gold accent
        // instead of blue. Recolored to match rather than invented fresh.
        "@media (prefers-color-scheme:dark){:root:not([data-theme=\"light\"]){"
        "--paper:#14120f;--parchment:#241f18;--panel:#1b1813;--ink:#f2ead8;"
        "--accent:#d1a85c;--stone:#a89c88;--rule:rgba(242,234,216,.14);"
        "--danger:#c9776a;--warn:#d9a866;--ok:#7fa891;"
        "--card:#221d16;--cardshadow:0 2px 6px rgba(0,0,0,.35),0 10px 28px rgba(0,0,0,.35)}}"
        ":root[data-theme=\"dark\"]{"
        "--paper:#14120f;--parchment:#241f18;--panel:#1b1813;--ink:#f2ead8;"
        "--accent:#d1a85c;--stone:#a89c88;--rule:rgba(242,234,216,.14);"
        "--danger:#c9776a;--warn:#d9a866;--ok:#7fa891;"
        "--card:#221d16;--cardshadow:0 2px 6px rgba(0,0,0,.35),0 10px 28px rgba(0,0,0,.35)}"
        "*{box-sizing:border-box}html{-webkit-font-smoothing:antialiased}"
        "body{margin:0;background:var(--paper);color:var(--ink);font:16px/1.5 var(--body);"
        "transition:background .15s ease,color .15s ease}"
        "a{color:var(--accent)}a:hover{color:var(--ink)}"
        "h1,h3{font-family:var(--display);font-weight:600;letter-spacing:-.02em;"
        "text-wrap:balance;line-height:1.2}"
        // bigger page titles (Night river 2026-11-10)
        "h1{font-size:clamp(2rem,1.5rem + 2.2vw,3rem);letter-spacing:-.03em;margin:.8rem 0 .4rem}"
        "h3{font-size:1.15rem;margin:0 0 .3rem}"
        // A register, not app chrome: every card/section title is a small
        // tracked-out label naming what the block below it is, not a
        // headline competing with the page's own h1.
        "h2{font-family:var(--display);font-weight:400;font-size:.75rem;letter-spacing:.12em;"
        "text-transform:uppercase;color:var(--stone);margin:1.5rem 0 .7rem;line-height:1.3}"
        "p{text-wrap:pretty}"
        // A single fixed max-width (928px, then 1152px) kept falling
        // short on an actually wide monitor -- every bump just moved
        // where the complaint landed, since a fixed value is either too
        // narrow on a big screen or too wide on a normal one. clamp()
        // scales with the real viewport instead of guessing one number:
        // 58rem (928px) is the floor, so mobile/normal windows are
        // untouched (max-width alone never forces something wider than
        // its actual container); 96rem (1536px) is the ceiling, so even
        // an ultrawide monitor doesn't stretch prose to unreadable line
        // lengths; 88vw fills the space between.
        ".wrap{max-width:clamp(58rem,88vw,96rem);margin:0 auto;padding:0 1.1rem 3.5rem}"
        // position:sticky + z-index together make header.top its own
        // stacking context, so anything nested inside it (the account
        // dropdown included) is capped at THIS z-index when compared
        // against ordinary page content, no matter how high a z-index the
        // nested element itself gets -- caught live against #medraftbar's
        // sticky z-index:8 on /me, which kept painting over the dropdown's
        // own top rows even after raising the dropdown's z-index well past
        // it. Above every sticky in-page element (#medraftbar's 8 is the
        // highest), below the floating jukebox widget (30) and the intro
        // overlay (999), which should stay on top of the header regardless.
        "header.top{border-bottom:1px solid var(--rule);background:linear-gradient(180deg,var(--parchment),var(--paper));"
        "box-shadow:0 1px 0 rgba(138,102,32,.18);"
        "position:sticky;top:0;z-index:10}"
        // A link to #something scrolled it under this sticky header (/pain/<id> -> /problems#painN: the post link
        // and Keep/Forget were hidden; sweep 2026-11-09). Every jump target stops below the header instead.
        "[id]{scroll-margin-top:5.5rem}"
        ".setidx{display:flex;flex-wrap:wrap;gap:.4rem;margin:0 0 1rem}"
        ".setidx a{display:inline-flex;align-items:center;min-height:36px;padding:0 .75rem;border:1px solid var(--rule);"
        "border-radius:999px;text-decoration:none;color:var(--accent);font-size:.9rem;background:var(--panel)}"
        // "New proposal" (a <details> on /proposals) looks like the site's buttons, not a bare disclosure triangle
        "details.newprop>summary{display:inline-flex;align-items:center;min-height:44px;padding:0 1.1rem;"
        "border-radius:var(--rmd);background:var(--accent);color:var(--paper);font-weight:600;cursor:pointer;"
        "list-style:none}"
        "details.newprop>summary::-webkit-details-marker{display:none}"
        "details.newprop[open]>summary{margin-bottom:.6rem;background:transparent;color:var(--accent);"
        "border:1px solid var(--rule)}"
        ".topinner{max-width:clamp(58rem,88vw,96rem);margin:0 auto;padding:.7rem 1.1rem;display:flex;"
        "align-items:center;flex-wrap:wrap;gap:.45rem 1rem}"
        ".brand{display:flex;align-items:center;gap:.65rem;text-decoration:none;color:var(--ink);margin-right:auto}"
        ".mark{flex-shrink:0;display:block}"
        ".brand strong{font-family:var(--display);font-weight:600;font-size:1.35rem;letter-spacing:-.03em}"
        ".brand .tld{color:var(--stone);font-size:.72rem;letter-spacing:.12em;text-transform:uppercase;margin-left:.15rem}"
        // Keeps "LiquidGov" and ".US" one token -- without this, a squeeze
        // at a mid-width viewport (nav not yet collapsed, but crowded) can
        // wrap the two spans onto separate lines.
        ".brand .word{white-space:nowrap}"
        ".brand .tagline{color:var(--stone);font-size:.62rem;letter-spacing:.14em;text-transform:uppercase;"
        "margin-top:.1rem}"
        // Not parked at left:-999px (CODE-SCAN-UX A2, Brief 4 item 9): iOS Safari scrolls a toggled form control into
        // view, so an off-screen checkbox yanked the page when the avatar/Menu label was tapped mid-page. It stays where
        // it sits in the header (on screen), 1px, invisible and untappable; the label still toggles it.
        "#navc{position:absolute;width:1px;height:1px;margin:0;padding:0;border:0;opacity:0;pointer-events:none;"
        "overflow:hidden;clip-path:inset(50%)}"
        ".navbtn{display:none;min-height:44px;min-width:44px;align-items:center;justify-content:center;"
        "border:1px solid var(--rule);background:var(--paper);border-radius:var(--rsm);color:var(--ink);"
        "font:600 .85rem var(--body);cursor:pointer}"
        "nav{display:flex;flex-wrap:wrap;gap:.1rem .85rem;align-items:center;flex:1 1 16rem}"
        ".themebtn{min-height:36px;min-width:36px;display:inline-flex;align-items:center;justify-content:center;"
        "border:1px solid var(--rule);border-radius:50%;color:var(--ink);text-decoration:none;"
        "font-size:1rem;margin-left:.4rem;flex-shrink:0}"
        "nav a{color:var(--accent);text-decoration:none;font-size:.92rem;padding:.35rem 0;min-height:44px;"
        "display:inline-flex;align-items:center}nav a:hover{text-decoration:underline}"
        // Notifications/Messages: always-visible icon buttons next to the
        // avatar, not buried in the account dropdown -- the dropdown is for
        // things you open a few times a session, these are things you
        // check constantly. Now real <button> elements (needed a JS click
        // handler, not just an href -- see mail_widget_script), which
        // means the sitewide button{} rule (the big accent-filled pill
        // used for Post/Send/Create) applies too unless overridden: caught
        // live as an oversized amber stadium shape instead of a small
        // circle, since button{}'s padding widened the box past its
        // min-width before border-radius:50% ever got a square to round.
        ".iconbtn{position:relative;min-height:36px;min-width:36px;display:inline-flex;align-items:center;"
        "justify-content:center;border:1px solid var(--rule);border-radius:50%;background:transparent;"
        "color:var(--ink);padding:0;text-decoration:none;font-size:1.05rem;flex-shrink:0}"
        ".iconbtn .badge{position:absolute;top:-5px;right:-5px;margin:0;font-size:.7rem;"
        "min-width:1.25rem;height:1.25rem;padding:0 .28rem;box-shadow:0 0 0 2px var(--parchment)}"
        ".notif-banner{margin:0 0 .6rem;padding:.35rem .6rem;border-radius:var(--rsm);"
        "background:var(--accent);color:var(--paper);font-size:.8rem;font-weight:600}"
        ".notice-unread{background:rgba(196,165,116,.14);margin:0 -.4rem;padding:.7rem .4rem;"
        "border-radius:var(--rsm)}"
        ".person form.notice-accept{flex-shrink:0;margin:0 0 0 .4rem}"
        ".person form.notice-accept button{min-height:36px;width:auto;padding:.3rem .75rem}"
        ".navavatar{cursor:pointer;display:inline-flex;flex-shrink:0}"
        ".navavatar:hover .portrait{filter:brightness(1.08)}"
        // Shared chrome for every header dropdown -- the account menu, and
        // the Notifications/Messages widgets below. Anchored to the
        // header's own right edge (not each icon's own position: header.top
        // is the nearest positioned ancestor since .topinner itself isn't
        // positioned, so that's what top:100% actually measures from) --
        // simpler than computing a per-icon offset, and it's a common
        // enough pattern (most sites' notification dropdowns anchor to the
        // header edge, not precisely under the bell) that it reads as
        // intentional rather than misaligned.
        // Above #medraftbar's sticky z-index:8 (the /me "Editing /me" bar)
        // -- caught live: a dropdown's top rows rendered underneath that
        // bar instead of over it until this beat it.
        ".dropdown-panel{position:absolute;top:100%;right:1.1rem;left:auto;background:var(--parchment);"
        "border:1px solid var(--rule);border-radius:var(--rmd);box-shadow:0 6px 20px rgba(0,0,0,.18);"
        "z-index:9;max-height:calc(100vh - 60px);overflow-y:auto}"
        // The two JS-driven panels (below) toggle visibility via the
        // `hidden` attribute rather than the #navc checkbox trick, since
        // opening either one also has to fire a fetch -- but a plain class
        // selector and the [hidden] attribute selector have equal
        // specificity, so without this, this stylesheet's own
        // .notifpanel/.mailpanel{display:flex} would beat the browser's
        // default [hidden]{display:none} (author styles always beat UA
        // defaults) and the panel would show even while hidden.
        ".dropdown-panel[hidden],.mailgroupbox[hidden]{display:none}"
        ".mailgroupbtn{margin:.15rem 0 .55rem}"
        ".grouppickrow:hover{background:var(--panel)}"
        // Same #navc:checked~X sibling-toggle trick the mobile hamburger
        // already uses (see nav a{} below and the max-width:720px block),
        // just aimed at the avatar instead and not gated to narrow
        // viewports -- this panel is always collapsed until you click your
        // own picture, at any screen size.
        "nav.accountmenu{display:none;flex-direction:column;flex-wrap:nowrap;align-items:stretch;gap:0;"
        "min-width:13rem;padding:.5rem 0}"
        "#navc:checked~nav.accountmenu{display:flex}"
        ".accountmenu a{color:var(--ink);text-decoration:none;font-size:.92rem;padding:.55rem 1.1rem;"
        "min-height:40px;display:flex;align-items:center}"
        ".accountmenu a:hover{background:var(--panel)}"
        ".accountmenu a.menu-name{font-family:var(--display);font-weight:600;font-size:1.05rem;padding-bottom:.2rem;padding-right:2.8rem}"
        ".accountmenu .menu-theme-row{position:absolute;top:.35rem;right:.4rem;z-index:1}"
        ".accountmenu .themebtn{margin:0}"
        ".accountmenu hr{border:0;border-top:1px solid var(--rule);margin:.4rem 0}"
        ".notifpanel,.mailpanel{display:flex;flex-direction:column;width:21rem;max-width:88vw}"
        ".panelhead{font-family:var(--display);font-weight:600;font-size:.95rem;"
        "padding:.7rem 1.1rem .5rem;border-bottom:1px solid var(--rule);flex-shrink:0}"
        ".mailsection{padding:.6rem 1.1rem}"
        ".mailfriends li{padding:0}"
        ".mailfriends a{display:flex;align-items:center;gap:.5rem;padding:.4rem 0;"
        "color:var(--ink);text-decoration:none}"
        ".mailfriends a:hover{color:var(--accent)}"
        ".mailnewgroup{display:flex;flex-direction:column;gap:.4rem;margin-top:.5rem}"
        ".inboxrow{display:flex;align-items:center;gap:.55rem;padding:.45rem 0;color:var(--ink);"
        "text-decoration:none;border-bottom:1px solid var(--rule)}"
        ".inboxrow:last-child{border-bottom:0}"
        ".inboxrow .inboxtitle{font-weight:600}"
        ".inboxrow.unread .inboxtitle{font-weight:700}"
        ".inboxrow .inboxpreview{display:block;color:var(--stone);font-size:.78rem;"
        "white-space:nowrap;overflow:hidden;text-overflow:ellipsis;max-width:14rem}"
        ".inboxrow .inboxmeta{margin-left:auto;font-size:.68rem;color:var(--stone);flex-shrink:0}"
        ".mailattbtn{display:inline-flex;align-items:center;justify-content:center;width:2.6rem;height:2.6rem;"
        "border:1px solid var(--rule);border-radius:var(--rsm);background:var(--paper);cursor:pointer;flex-shrink:0}"
        ".mailattbtn input{display:none}"
        ".mailcomposer textarea,#mailtext{background:#fff;color:#14110c;caret-color:#14110c}"
        ".mailpending{background:var(--paper);border:1px dashed var(--rule);padding:.7rem;border-radius:var(--rmd);margin:.6rem 0}"
        ".maildisclose{font-size:.78rem;color:var(--stone);margin:.4rem 0 0}"
        // The instant-messenger part: sent bubbles lean right and pick up
        // the accent color, received ones lean left in a neutral panel --
        // the one visual cue that actually makes a flat list of divs read
        // as a chat instead of a log.
        ".mailthreadhead{display:flex;align-items:center;gap:.5rem;padding:.6rem 1.1rem;"
        "border-bottom:1px solid var(--rule)}"
        ".mailback{color:var(--ink);text-decoration:none;font-size:1.1rem}"
        ".msglist{padding:.7rem 1.1rem;display:flex;flex-direction:column;gap:.5rem;max-height:18rem;overflow-y:auto}"
        ".msg{display:flex}.msg.mine{justify-content:flex-end}.msg.theirs{justify-content:flex-start}"
        ".msg .bubble{max-width:80%;padding:.5rem .7rem;border-radius:var(--rmd);font-size:.88rem;"
        "line-height:1.35;white-space:pre-wrap;word-break:break-word}"
        ".msg.mine .bubble{background:var(--accent);color:var(--paper);border-bottom-right-radius:.2rem}"
        ".msg.theirs .bubble{background:var(--panel);border:1px solid var(--rule);border-bottom-left-radius:.2rem}"
        ".bubtime{display:block;margin-top:.2rem;font-size:.65rem;opacity:.7}"
        ".mailcomposer{padding:.6rem 1.1rem;border-top:1px solid var(--rule)}"
        // The full Messages page (/mail, .mailpage; Brief 4 item 8): the page is the one scroll region (the transcript
        // has no scrollbar of its own), the composer stays pinned at the bottom of the window under the thread, and
        // from 900 px the conversation list sits beside the open thread. The header's message panel is unchanged.
        ".mailage{margin:.2rem 0 .6rem}.mailmore{display:inline}"
        ".mailmore summary{display:inline;cursor:pointer;color:var(--accent);text-decoration:underline;list-style:none}"
        ".mailmore summary::-webkit-details-marker{display:none}"
        ".mailpage .msglist{max-height:none;overflow:visible}"
        ".mailpage .msg .bubble{min-width:0;overflow-wrap:anywhere}"
        ".mailpage .mailcomposer{position:sticky;bottom:0;z-index:2;background:var(--card)}"
        "@media(min-width:900px){"
        ".mailpage .mailwidget.hasthread{display:grid;grid-template-columns:minmax(15rem,19rem) minmax(0,1fr);align-items:start;padding:0}"
        ".mailpage .hasthread .mailsection{grid-column:1;grid-row:1;position:sticky;top:5.5rem;max-height:calc(100vh - 7rem);"
        "overflow-y:auto;padding:1rem;border-right:1px solid var(--rule)}"
        ".mailpage .hasthread .mailthread{grid-column:2;grid-row:1;min-width:0}"
        "}"
        ".mailattach{max-width:6.5rem;font-size:.7rem;padding:.3rem}"
        ".mailprog{margin-top:.35rem;padding:.45rem .6rem;border:1px solid var(--rule);border-radius:var(--rsm);"
        "overflow-wrap:anywhere}.mailprog p{margin:.3rem 0 0;font-size:.8rem}"
        ".badge{display:inline-flex;align-items:center;justify-content:center;min-width:1.3rem;height:1.3rem;"
        "padding:0 .35rem;border-radius:999px;background:var(--accent);color:var(--paper);font-size:.7rem;"
        "margin-left:.25rem;font-variant-numeric:tabular-nums}"
        // Polish 2026-11-09 (Brandon: "cleaner and more esthetic"): even padding, a softer corner and a hair of depth;
        // the first heading inside no longer adds a gap at the top ("YOURS"/"CITIES" sat a line low), nor the last
        // child one at the bottom.
        // Night river (2026-11-10): a near-white card that clearly lifts off the beige page (in dark mode a warm
        // charcoal one); the first pass's barely-there panel was too subtle to see
        ".card{background:var(--card);border:1px solid rgba(138,102,32,.16);border-radius:18px;padding:1.3rem 1.4rem;"
        "margin:1.1rem 0;box-shadow:var(--cardshadow)}"
        ".card>:first-child{margin-top:0}.card>:last-child{margin-bottom:0}"
        ".card h2{margin:1.1rem 0 .55rem}"
        ".card .card{border-radius:var(--rmd);box-shadow:none;background:var(--panel)}"
        // main actions only (a form's submit, a filled link button, the active feed tab) get a gold gradient; every
        // <button> did in the prototype and the alerts pill's text vanished
        // (a form <button> with no type is a submit button too: "Change password" was missed by [type=submit] alone;
        // "New proposal" is a <details> summary styled as a button)
        "form button:not([type=button]):not([type=reset]):not(.ghost):not(.danger):not(.ichip):not(.iconbtn),"
        "a.btn:not(.ghost):not(.danger),.segtabs>.btn:not(.ghost),details.newprop:not([open])>summary{"
        "background:linear-gradient(180deg,#a37a2c,#7d5a17);color:#fffdf8;border-radius:12px;"
        "box-shadow:inset 0 1px 0 rgba(255,255,255,.25),0 2px 6px rgba(125,90,23,.35)}"
        ".lede{font-family:var(--display);font-size:1.15rem;line-height:1.4;color:var(--accent)}"
        ".muted{color:var(--stone);font-size:.9rem}.warn{color:var(--warn)}.ok{color:var(--ok)}"
        "label{display:block;font-size:.82rem;color:var(--stone);margin:.4rem 0 .15rem;font-weight:500}"
        // Form fields get room between them, and the submit button doesn't touch the last field (polish 2026-11-09).
        // Scoped to forms: the header avatar is a <label> too (it opens the account menu) and must not move.
        "form label{margin:.85rem 0 .3rem;font-size:.85rem}form>label:first-child{margin-top:0}"
        // any button right after a field (Settings' "Change password" touched its field: the dark-mode pass caught it).
        // Not one-button inline forms, and a hidden field is not a field (2026-11-10: Add friend / Accept sat .9rem low)
        "form:not(.inline)>button[type=submit]:last-child,form:not(.inline)>input:not([type=hidden])+button,"
        "form:not(.inline)>textarea+button,form:not(.inline)>select+button{margin-top:.9rem}"
        "input,textarea,select{width:100%;padding:.65rem .7rem;border-radius:var(--rsm);border:1px solid var(--rule);"
        "background:var(--paper);color:var(--ink);font:1rem var(--body);min-height:44px}"
        // Checkboxes and radios are natively tiny; the width:100%/min-height:44px
        // above (meant for text fields) stretches them into giant boxes if not
        // overridden -- this bit us once already on input[type=color] (fixed
        // with an inline style there), so fix it globally here instead of
        // per-control at every future callsite.
        "input[type=checkbox],input[type=radio]{width:1.1rem;min-height:0;height:1.1rem;padding:0;flex:0 0 auto}"
        "textarea{min-height:6rem;resize:vertical}button,.btn{display:inline-flex;align-items:center;justify-content:center;"
        "gap:.35rem;background:var(--accent);color:var(--paper);border:0;padding:.6rem 1rem;border-radius:var(--rsm);"
        "cursor:pointer;font:600 .92rem var(--body);min-height:44px;text-decoration:none}"
        // hover:hover guards this -- a touch tap on iOS synthesizes :hover
        // with no matching "leave" event, so an unguarded rule here stays
        // stuck brighter until something else steals focus/hover. A mouse
        // still gets the real hover feedback; a touch never triggers it.
        "@media(hover:hover){button:hover,.btn:hover{filter:brightness(1.08)}}"
        "button:active,.btn:active{transform:scale(.98)}"
        // Secondary buttons are quiet: ink text, normal weight, the outline turns gold on hover (polish 2026-11-09;
        // a row of gold-bold outlines competed with the one gold primary button)
        "button.ghost,.btn.ghost{background:transparent;color:var(--ink);font-weight:500;border:1px solid var(--rule)}"
        "@media(hover:hover){button.ghost:hover,.btn.ghost:hover{filter:none;border-color:var(--accent);color:var(--accent)}}"
        // the feed's filter (All / Friends / Communities): one segmented control that never wraps on a phone
        ".segtabs{display:inline-flex;flex-wrap:nowrap;gap:0;padding:3px;border:1px solid var(--rule);border-radius:12px;"
        "background:var(--panel);max-width:100%;overflow-x:auto;margin:.5rem 0 1rem}"
        ".segtabs>.btn{min-height:38px;padding:.35rem .9rem;border:0;border-radius:9px;font-size:.9rem;white-space:nowrap}"
        ".segtabs>.btn.ghost{background:transparent;color:var(--stone)}"
        // a post's actions (Repost, seconded, Delete) are small and quiet
        ".post form.inline button{min-height:36px;padding:.3rem .75rem;font-size:.85rem}"
        "button.danger,.btn.danger{background:var(--danger);color:#f7f1e4}"
        ".row{display:flex;gap:.5rem;flex-wrap:wrap;align-items:center}"
        ".toolgroup-sep{width:1px;align-self:stretch;background:var(--rule);margin:0 .15rem}"
        // Drawing canvas (Brandon 2026-11-11 4:37 PM, Brief 4 item 7; site-look2 FINDINGS 1 found the surface using
        // under half the card and the controls on three sides): one toolbar above a drawing surface that fills the
        // card's width at its own shape. The toolbar's four groups (tools | size, opacity, colour | layers | undo and
        // clipboard) sit side by side with a hairline between, and wrap whole lines at narrow widths, so nothing is cut
        // off sideways. On a phone the inner frame drops its own card padding so the surface stays large.
        ".ctoolbar{display:flex;flex-wrap:wrap;gap:.5rem .7rem;align-items:center;margin-bottom:.7rem}"
        ".ctgroup{display:flex;flex-wrap:wrap;gap:.3rem;align-items:center;min-width:0;max-width:100%;"
        "padding-right:.7rem;border-right:1px solid var(--rule)}"
        ".ctgroup:last-child{padding-right:0;border-right:0}"
        ".cprops .row{margin-bottom:0!important}"
        ".clayers>div{flex-direction:column}"
        ".ccanvasrow #ccanvaswrap{width:100%}"
        "@media(max-width:600px){.canvascard{padding:.8rem .7rem}"
        ".canvas-frame{padding:0;border:0;box-shadow:none;background:none}"
        ".ctgroup{padding-right:0;border-right:0}}"
        ".person,.sess,.msg,.post{border-top:1px solid var(--rule);padding:.7rem 0;display:flex;"
        "justify-content:space-between;gap:.8rem;flex-wrap:wrap;align-items:flex-start}"
        ".person:first-of-type,.sess:first-of-type,.msg:first-of-type,.post:first-of-type{border-top:0}"
        ".post p,.post a{overflow-wrap:anywhere;word-break:break-word}"
        ".composerbar{display:flex;align-items:center;gap:.55rem;margin-top:.45rem}"
        ".composerbar .grow{flex:1}"
        ".filepick{position:relative;width:2.4rem;height:2.4rem;flex:0 0 auto;display:inline-flex;"
        "align-items:center;justify-content:center;border:1px solid var(--rule);border-radius:var(--rsm);"
        "background:var(--panel);cursor:pointer}"
        ".filepick input{position:absolute;inset:0;opacity:0;cursor:pointer;font-size:4rem}"
        ".filepick svg{pointer-events:none}"
        "table.register{width:100%;border-collapse:collapse;margin:.6rem 0 1rem}"
        "table.register th{text-align:left;font:600 .7rem var(--body);letter-spacing:.08em;"
        "text-transform:uppercase;color:var(--stone);padding:.4rem .3rem;border-bottom:1px solid var(--rule)}"
        "table.register td{padding:.5rem .3rem;border-bottom:1px solid var(--rule)}"
        "table.register select{width:auto;min-height:auto;padding:.3rem .5rem}"
        ".eyebrow{font-family:var(--display);font-weight:400;font-size:.75rem;letter-spacing:.12em;"
        "text-transform:uppercase;color:var(--stone);margin:0 0 .3rem}"
        ".chip{display:inline-block;border:1px solid var(--rule);background:var(--parchment);"
        "border-radius:999px;padding:.25rem .7rem;font-size:.78rem;color:var(--ink)}"
        // The /feed audience picker: an icon over a label, not a text-only
        // pill -- the glyph carries the meaning (globe/people/rooms) so the
        // reach doesn't need a sentence spelled out underneath it.
        "button.ichip{display:flex;flex-direction:column;align-items:center;gap:.3rem;"
        "width:4.4rem;min-height:0;padding:.55rem .3rem .5rem;border-radius:var(--rmd);"
        "border:1px solid var(--rule);background:var(--parchment);color:var(--stone);"
        "cursor:pointer;font:600 .68rem var(--body);letter-spacing:.02em}"
        "button.ichip svg{width:20px;height:20px;stroke:currentColor;flex-shrink:0}"
        "button.ichip:hover{border-color:var(--accent)}"
        "button.ichip.on{background:var(--accent);border-color:var(--accent);color:var(--paper)}"
        // Icon-only attach control paired with the composer's Post button --
        // same .iconbtn footprint as the header's bell/mail toggles, just
        // sized down slightly and without their absolute-positioned badge.
        ".attachbtn{position:relative;min-height:44px;min-width:44px;display:inline-flex;"
        "align-items:center;justify-content:center;background:transparent;color:var(--ink);"
        "border:1px solid var(--rule);border-radius:var(--rsm);cursor:pointer;flex-shrink:0}"
        ".attachbtn.has-file{border-color:var(--accent);color:var(--accent)}"
        "table.record{border-collapse:collapse;margin:.6rem 0}"
        "table.record td{padding:.2rem .6rem .2rem 0;border:0;font-size:.9rem}"
        "table.record td:first-child{color:var(--stone);font-size:.72rem;letter-spacing:.08em;"
        "text-transform:uppercase;white-space:nowrap}"
        ".portrait{width:96px;height:96px;border-radius:50%;object-fit:cover;border:1px solid var(--rule);"
        "background:var(--parchment);flex-shrink:0;display:inline-block;vertical-align:middle}"
        ".portrait.sm{width:36px;height:36px}"
        ".portrait.ph{box-sizing:border-box;display:inline-flex;align-items:center;justify-content:center;"
        "font-family:var(--display);font-weight:600;color:var(--accent);font-size:2.6rem;line-height:1;"
        "user-select:none}"
        ".portrait.sm.ph{font-size:1rem}"
        ".hit{display:flex;gap:.7rem;align-items:flex-start;border-top:1px solid var(--rule);padding:.75rem 0;flex-wrap:wrap}"
        ".hit:first-child{border-top:0}"
        ".hit .hitinfo{flex:1;min-width:8rem}"
        ".hit .hitinfo strong{display:block}"
        ".hit .hitacts{display:flex;gap:.4rem;flex-wrap:wrap;align-items:center}"
        "#peoplehits{margin-top:.6rem}"
        // The info div next to the avatar is a flex item with no
        // flex-grow, so by default it only ever sizes to fit its own
        // text (a short "name · timestamp" line) -- fine until a post's
        // width:100% image/video landed inside it and sized itself
        // against that narrow shrink-wrapped box instead of the actual
        // card. flex:1 makes it fill the row like the rest of the
        // layout already assumes; min-width:0 is required alongside it
        // so a flex item is actually allowed to be narrower than its
        // content's intrinsic width instead of blowing out the row.
        ".who{display:flex;gap:.75rem;align-items:flex-start}"
        // A flex item's default min-width is its content's intrinsic width,
        // not 0 -- without this, a long unbroken token (a pasted URL, the
        // actual bug this fixes) pushes .who wider than the card instead of
        // wrapping inside it, no matter what overflow-wrap says below.
        ".who>div{min-width:0;flex:1}"
        ".post p{overflow-wrap:anywhere}"
        ".who>div{flex:1;min-width:0}"
        ".post p{margin:.25rem 0 0;white-space:pre-wrap}"
        "ul.plain{list-style:none;padding:0;margin:0}ul.plain li{border-top:1px solid var(--rule);padding:.65rem 0}"
        "footer.site{max-width:clamp(58rem,88vw,96rem);margin:0 auto;padding:1.2rem 1.1rem 2.5rem;color:var(--stone);font-size:.82rem;"
        "border-top:1px solid var(--rule)}"
        "footer.site a{color:var(--stone)}"
        // the "Disclaimer" toggle matches the footer links beside it (it was a size larger; sweep 2026-11-09)
        "footer.site summary{font-size:inherit;color:var(--stone);text-decoration:underline}"
        ".empty{color:var(--stone);padding:.4rem 0}"
        "form.inline{display:inline}form.stack{margin:0}"
        ".clocks{display:grid;gap:.7rem}@media(min-width:640px){.clocks{grid-template-columns:1fr 1fr}}"
        ".jukebox iframe{width:100%;max-width:480px;aspect-ratio:16/9;border:0;border-radius:var(--rsm);display:block}"
        ".jbstation-row{display:flex;align-items:center;gap:.4rem;background:var(--paper);"
        "border:1px solid var(--rule);border-radius:var(--rsm);padding:.35rem .5rem;touch-action:none}"
        ".jbstation-row.active{border-color:var(--accent);background:var(--parchment)}"
        ".jbdraghandle{cursor:grab;color:var(--stone);font-size:1rem;line-height:1;user-select:none;padding:0 .2rem}"
        ".jbstation-name{flex:1;font-size:.85rem;cursor:pointer;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
        ".jbstation-remove{background:none;border:0;color:var(--stone);cursor:pointer;font-size:.9rem;padding:0 .2rem}"
        // Not a second h1 -- one quiet line naming the thing, same display
        // face as the site wordmark, small enough to sit above the tab
        // row without competing with it.
        ".juketube-plate{text-align:center;font-family:var(--display);font-size:1rem;"
        "font-weight:600;letter-spacing:.05em;color:var(--ink);margin:0 0 .6rem;"
        "padding-bottom:.5rem;border-bottom:1px solid var(--rule)}"
        ".jukebox .row{margin-top:.5rem}"
        ".stattiles{display:grid;grid-template-columns:repeat(3,1fr);gap:.6rem;margin:.7rem 0}"
        ".stattile{background:var(--paper);border:1px solid var(--rule);border-radius:var(--rmd);"
        "padding:.7rem .5rem;text-align:center}"
        ".stattile strong{display:block;font-family:var(--display);font-size:1.3rem;line-height:1.2}"
        ".stattile span{display:block;font-size:.65rem;letter-spacing:.08em;text-transform:uppercase;"
        "color:var(--stone);margin-top:.2rem}"
        ".roomrow{display:flex;align-items:center;gap:.7rem;padding:.6rem 0;border-top:1px solid var(--rule);"
        "text-decoration:none;color:var(--ink)}.roomrow:first-of-type{border-top:0}"
        ".roomrow:hover .roomname{text-decoration:underline}"
        ".roomini{width:34px;height:34px;border-radius:50%;background:var(--parchment);border:1px solid var(--rule);"
        "display:inline-flex;align-items:center;justify-content:center;font-family:var(--display);"
        "font-weight:600;flex-shrink:0}"
        ".roomname{flex:1;font-weight:600;min-width:0}.roomname .muted{display:block;font-weight:400;font-size:.78rem}"
        ".roomlive{color:var(--stone);font-size:.8rem;white-space:nowrap;flex-shrink:0}"
        // A fixed height here can never track the card beside it: the
        // card's own height changes with viewport width (its text
        // reflows into fewer/more lines as the row grows or shrinks),
        // so any single guessed number only matches at one width and
        // leaves dead air below the card at every other width. Instead
        // the ticker column is a flex column (see solution() below) and
        // this fills whatever height that column is stretched to by its
        // taller sibling -- min-height only guards the case where the
        // ticker sits alone on its own wrapped line.
        // flex:1 (== flex-basis:0%) would fall back to the ticker's real,
        // un-clipped content height (all items stacked twice, ~670px)
        // whenever the column's own height is still being worked out --
        // a percentage basis resolves against an indefinite container by
        // using content size instead, exactly the runaway height this
        // caused. flex-basis:0 (a length, not a percentage) stays
        // definite regardless, so growth is real available space, not a
        // content-size leak.
        ".ticker-wrap{margin-top:.6rem;flex:1 1 0;min-height:10rem;overflow:hidden;"
        "-webkit-mask-image:linear-gradient(180deg,transparent,#000 20%,#000 80%,transparent);"
        "mask-image:linear-gradient(180deg,transparent,#000 20%,#000 80%,transparent)}"
        ".ticker-roll{animation:lg-roll 26s linear infinite}"
        // Centered, not left-aligned: each item is still a full-width
        // block (so a real fill colour or hover state could use the
        // whole row later), and growing the column wider on a big screen
        // just meant more blank tail after a short left-aligned phrase.
        // Centered, the growth reads as breathing room instead.
        // line-height stays tight (1.35) since items are now full phrases
        // that wrap to 2+ lines on narrower screens -- the old 1.9 was
        // tuned for single-line words and doubled as inter-item spacing;
        // padding does that job explicitly now so wrapped lines within
        // one item don't get the same gap as the space between items.
        ".ticker-roll div{font-family:var(--display);font-size:1.2rem;line-height:1.35;"
        "color:var(--ink);text-align:center;padding:.5rem 0}"
        "@keyframes lg-roll{0%{transform:translateY(0)}100%{transform:translateY(-50%)}}"
        // The list is rendered twice back-to-back (see its own comment) so
        // the scroll loop has no visible seam -- stopping the animation
        // alone would leave both copies sitting there at once, visibly
        // doubled. nth-child(n+12) hides the second copy specifically,
        // matching the 11-item PROBLEMS list's own fixed length.
        "@media(prefers-reduced-motion:reduce){.ticker-roll{animation:none}"
        ".ticker-roll div:nth-child(n+12){display:none}}"
        // Real <button>, not a div playing role="button" -- that exact
        // substitution already cost a working dismiss control elsewhere on
        // this site (iOS WebKit doesn't reliably fire `click` on a div).
        // position:absolute (not fixed) -- pinned to its own hero block's
        // top-right corner (see genesis_body's position:relative wrapper),
        // not the viewport, so it scrolls away normally and can't end up
        // sitting over the sticky header or the iOS install banner.
        ".listen-pulse{display:inline-flex;align-items:center;justify-content:center;"
        "min-width:44px;min-height:44px;width:44px;height:44px;border-radius:50%;flex-shrink:0;"
        "border:none;background:var(--accent);color:var(--paper);cursor:pointer;"
        "position:absolute;top:0;right:0}"
        // inset -4px and scale(1.35), not -6px and 1.7: the bigger ring reached ~3-8 px past a phone's right edge,
        // so the logged-out home page grew and shrank sideways with every pulse (2026-11-07 iPhone sweep).
        ".listen-pulse::after{content:'';position:absolute;inset:-4px;border-radius:50%;pointer-events:none;"
        "border:2px solid var(--accent);opacity:.55;animation:lg-listenpulse 1.8s ease-out infinite}"
        // Pulsing while it's already playing would read as "still waiting
        // to be tapped" -- the ring only means "tap for sound".
        ".listen-pulse.playing::after{animation:none;opacity:0}"
        "@media(prefers-reduced-motion:reduce){.listen-pulse::after{animation:none}}"
        "@keyframes lg-listenpulse{0%{transform:scale(.7);opacity:.55}100%{transform:scale(1.35);opacity:0}}"
        // A few nudges toward the corner, not an indefinite distraction --
        // the animation runs its fixed count and settles, same spirit as
        // the install-hint pulse being a "tap for sound" cue rather than
        // something that runs forever.
        "#jbhint.show{animation:lg-jthint-bounce 1.6s ease-in-out 3}"
        "#jbtoggle.jbnudge{animation:lg-jbtoggle-nudge 1.6s ease-in-out 3}"
        "@keyframes lg-jthint-bounce{0%,100%{transform:translateY(0)}50%{transform:translateY(-6px)}}"
        "@keyframes lg-jbtoggle-nudge{0%,100%{transform:scale(1)}50%{transform:scale(1.08)}}"
        "@media(prefers-reduced-motion:reduce){#jbhint.show{animation:none}#jbtoggle.jbnudge{animation:none}}"
        ".quote.centered{text-align:center;background:var(--parchment)}"
        ".quote p:not(.attrib):not(.stars){font-family:var(--display);font-size:1.15rem;line-height:1.5}"
        ".quote .stars{color:var(--accent);letter-spacing:.12em;margin:.3rem 0}"
        ".quote .attrib{font-size:.68rem;letter-spacing:.16em;text-transform:uppercase;color:var(--accent);margin:.5rem 0 0}"
        ".quote .attrib.muted{color:var(--stone)}"
        // Below the breakpoint, the ticker and this card can't both fit
        // at their intended widths, so .row's flex-wrap already drops the
        // card to its own line -- but a bare flex:0 0 12rem keeps it
        // pinned at that fixed width even alone on a full-width line,
        // leaving dead space beside it exactly as wide as the room it
        // gave up. Full width by default; the narrow fixed sidebar shape
        // only above the width where it actually sits beside the ticker.
        ".isnt-card{flex:1 1 100%}"
        // Both columns grow together above the breakpoint (ticker at 2x
        // weight, card at 1x, roughly matching their original visual
        // proportions) instead of the card staying pinned to a fixed
        // 12rem -- capping the row's own width just relocated the gap
        // from "beside the card" to "beside the row" once .wrap itself
        // scaled wider (clamp() above), since a capped-width row sitting
        // in a much wider column looks exactly like the dead space it
        // was supposed to fix. Growing both children means the pair
        // always fills whatever width the row actually has, the same way
        // every other full-width section on this page already does.
        "@media(min-width:34rem){.isnt-card{flex:1 1 12rem}}"
        ".lawstrip{display:flex}"
        ".lawstrip>div{flex:1;text-align:center;padding:0 .5rem;border-left:1px solid var(--rule)}"
        ".lawstrip>div:first-child{border-left:0}"
        ".lawstrip .n{margin:0;font-size:.7rem;letter-spacing:.1em;text-transform:uppercase;color:var(--accent)}"
        ".lawstrip .t{margin:.4rem 0 0;font-size:.85rem;line-height:1.3}"
        ".quadcard{display:block;text-decoration:none;color:var(--ink);"
        "background:linear-gradient(160deg,var(--parchment),var(--panel))}"
        ".quadcard:hover{filter:brightness(1.05)}"
        ".quadcard .quadhead{font-family:var(--display);font-size:1.3rem;line-height:1.2;margin:.5rem 0 0;color:var(--ink)}"
        // nav used to expand in normal document flow, inside the sticky
        // header -- fine until the link list itself got taller than a
        // small viewport, at which point the whole (now very tall)
        // header was still sticky, so it stayed pinned over the page as
        // you scrolled past it, covering everything underneath instead
        // of scrolling away. position:absolute here keeps header.top's
        // own box always just the slim top bar; the expanded menu floats
        // over the content below as a real dropdown instead of dragging
        // the sticky container's height along with it.
        "@media(max-width:720px){.navbtn{display:inline-flex}"
        "nav{display:none;position:absolute;top:100%;left:0;right:0;flex-direction:column;"
        "align-items:stretch;background:var(--parchment);border-bottom:1px solid var(--rule);"
        "padding:0 1.1rem;max-height:calc(100vh - 60px);overflow-y:auto;z-index:6}"
        "nav a{min-height:44px}#navc:checked~nav{display:flex}}"
        // The always-visible bell/envelope/avatar/theme cluster (logged in)
        // is four more inline items than this header ever had at narrow
        // widths before -- the decorative tagline was the difference
        // between fitting on one row and wrapping to two in testing.
        "@media(max-width:480px){.brand .tagline{display:none}.topinner{gap:.45rem .3rem;padding-left:.7rem;padding-right:.7rem}}"
        "@media(prefers-reduced-motion:reduce){*{transition:none!important;animation:none!important}}"
        // Last, so it beats every element and one-class rule above (button,.btn{display:inline-flex}, .row{display:
        // flex}, ...) that would otherwise beat the browser's own [hidden]{display:none}: a hidden Retry showed on
        // /me (Brief 6). Not !important, so a deliberate [hidden] rule with more weight (lgmap.css's
        // .lgm-filter-note[hidden], visibility only) still wins. Covers the case the .dropdown-panel[hidden] rule and
        // ctextrow's style.display work around one at a time.
        "[hidden]{display:none}";
}

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

// A full-bleed, muted, autoplaying clip shown the first time a browser
// ever lands on the site -- gated entirely client-side (localStorage),
// since "have I seen this before" is per-browser state with no reason to
// touch the account store or work for a logged-out visitor differently
// from a member. Skippable; ends by fading out and removing itself,
// revealing the page underneath (already rendered, not reloaded). Storage
// being blocked (private mode, some browsers) fails safe by just never
// showing it, rather than showing it on every single load.
//
// always_play (Settings > Notifications, off by default) skips the
// localStorage gate entirely, so a logged-in account that's opted in sees
// it on every visit instead of just the first -- computed server-side from
// that account's own prefs (see wrap()), so it's still just this one bool
// baked into the page, not a second client-side code path to keep in sync.
// Only reachable at all when show_intro is true (the Genesis page -- see
// page_wrap), so this never fires on an internal page regardless.
std::string intro_overlay_html(bool always_play) {
    std::string gate = always_play ? "" :
           "try{if(localStorage.getItem('liquidgov.intro.seen')){el.remove();return;}}"
           "catch(e){el.remove();return;}";
    return "<div id=\"lgintro\" style=\"position:fixed;inset:0;z-index:999;background:#14120f;"
           "display:flex;align-items:center;justify-content:center;overflow:hidden;cursor:pointer\">"
           "<video id=\"lgintrovid\" autoplay muted playsinline "
           "style=\"width:100%;height:100%;object-fit:cover\" src=\"/assets/intro.mp4?v=2\"></video>"
           "<button type=\"button\" id=\"lgintroskip\" style=\"position:absolute;right:1rem;bottom:1rem;"
           "z-index:1;min-height:44px;padding:.5rem 1rem;border:1px solid rgba(242,234,216,.35);"
           "border-radius:8px;background:rgba(20,18,15,.7);color:#f2ead8;font:600 .9rem system-ui,sans-serif;"
           "cursor:pointer\">Skip</button>"
           "</div>"
           "<script>(function(){"
           "var el=document.getElementById('lgintro');"
           +gate+
           "var vid=document.getElementById('lgintrovid');"
           "var done=false;"
           "function finish(){"
           "if(done||!el)return;done=true;"
           "try{localStorage.setItem('liquidgov.intro.seen','1');}catch(e){}"
           "el.style.transition='opacity .7s ease';el.style.opacity='0';"
           "setTimeout(function(){if(el&&el.parentNode)el.remove();},720);"
           "}"
           "vid.addEventListener('ended',finish);"
           "vid.addEventListener('error',finish);"
           "el.addEventListener('click',finish);"
           "var skip=document.getElementById('lgintroskip');"
           "if(skip)skip.addEventListener('click',function(e){e.stopPropagation();finish();});"
           // iOS Low Power Mode (and some other autoplay-declining browsers) silently never
           // starts an attribute-only `autoplay` video: no error event fires and the play
           // promise this explicit call() gives us is the only signal. Without it, a blocked
           // autoplay just sits on a black frame (el's own background-color) until the 12s
           // fallback below finally removes it -- 12 seconds of a stuck black screen is what
           // "not working" looks like from the outside, found live on iPhone Safari 2026-09-21.
           // A rejected promise is the fast path; the 1.5s watchdog below also catches the
           // rarer case actually observed live: the promise resolves and currentTime even
           // advances a little (0.445s in that trace), then playback silently freezes with
           // no error and no further events -- checking `paused` alone (not also requiring
           // currentTime===0) is what catches that case, and 1.5s is ample time for any
           // browser that is really going to play this to have left the paused state by then.
           "var playPromise=vid.play();"
           "if(playPromise&&playPromise.catch)playPromise.catch(finish);"
           "setTimeout(function(){if(vid.paused)finish();},1500);"
           "setTimeout(finish,12000);"
           "})();</script>";
}

// The iOS 'Get the app' bar that used to be built here (ios_install_hint) was removed 2026-11-07: assets/setup.js
// shows the one get-app prompt (iPhone Safari: "Add to Home Screen to get alerts"), and setup.js used to hide this
// one every second, so on a real iPhone it flashed up and vanished.

// Shown once, only inside /mail's own body (never site-wide via
// wrap() -- see that route) -- Add to Home Screen bookmarks whatever
// page is actually on screen, so the nudge has to land after the
// visitor is already on the real inbox, not on the /app/mail install
// page that may have sent them here. Same dismiss-once/style.display/
// real-<button> conventions the removed site-wide iOS hint used, but covers
// Android too (a real menu item there, not an iOS-only gesture) and
// keys off its own localStorage entry so dismissing one hint doesn't
// silently dismiss the other.
std::string mail_install_hint() {
    return
        "<div id=\"lgmailhint\" "
        "style=\"position:fixed;left:.6rem;right:.6rem;"
        "bottom:calc(.6rem + env(safe-area-inset-bottom,0px));"
        "z-index:50;max-width:26rem;margin:0 auto;background:var(--parchment);border:1px solid var(--rule);"
        "border-radius:var(--rmd);padding:.6rem .7rem;box-shadow:0 6px 20px rgba(0,0,0,.18);"
        "display:none;align-items:center;gap:.6rem\">"
        "<div style=\"flex:1;min-width:0\">"
        "<p style=\"margin:0;font-family:var(--display);font-weight:700;font-size:1rem;color:var(--ink)\">You're on Messages</p>"
        // Chrome on iPhone has no Add to Home Screen action at all --
        // the button just isn't in its share sheet, same reason
        // apps.html and the old app-mail.html copy both named Safari/
        // Brave specifically. The removed site-wide iOS hint skipped naming a
        // browser (it's the generic site-wide nudge, kept short on
        // purpose) but this one is telling someone to do a specific
        // thing right now, so the caveat that saves a dead-end tap
        // belongs in it.
        "<p id=\"lgmailhinttext\" style=\"margin:.1rem 0 0;font-size:.78rem;color:var(--stone)\">In "
        "<strong>Safari</strong> (or Brave), tap Share &rarr; "
        "<strong>Add to Home Screen</strong> (name it Messages). Chrome on iPhone can't do this.</p></div>"
        "<button type=\"button\" id=\"lgmailhintclose\" class=\"ghost\" aria-label=\"Dismiss\" "
        "style=\"min-height:44px;min-width:44px;padding:0;flex-shrink:0;font-size:1.1rem\">"
        "&#10005;</button></div>"
        "<script>(function(){"
        "try{if(localStorage.getItem('liquidgov.mailhint.dismissed'))return;}catch(e){return;}"
        "var isIOS=/iP(hone|od|ad)/.test(navigator.userAgent)||"
        "(navigator.platform==='MacIntel'&&navigator.maxTouchPoints>1);"
        "var isAndroid=/Android/.test(navigator.userAgent);"
        "if(!isIOS&&!isAndroid)return;"
        "var standalone=window.navigator.standalone===true||"
        "(window.matchMedia&&matchMedia('(display-mode: standalone)').matches);"
        "if(standalone)return;"
        "var el=document.getElementById('lgmailhint');"
        "if(isAndroid){document.getElementById('lgmailhinttext').innerHTML="
        "'Now tap <strong>\\u22ee</strong> then <strong>Add to Home screen</strong> or <strong>Install app</strong>.';}"
        "el.style.display='flex';"
        "function dismiss(){"
        "el.style.display='none';"
        "try{localStorage.setItem('liquidgov.mailhint.dismissed','1');}catch(e){}"
        "}"
        "var btn=document.getElementById('lgmailhintclose');"
        "btn.addEventListener('touchstart',dismiss,{passive:true});"
        "btn.addEventListener('click',dismiss);"
        "})();</script>";
}

std::string page_wrap(const std::string& title, const std::string& navhtml, const std::string& body,
                       bool show_intro = false, bool intro_always_play = false) {
    const std::string theme_attr = g_theme_mode.empty() ? "" : " data-theme=\""+g_theme_mode+"\"";
    const std::string next_theme = g_theme_mode == "dark" ? "light" : "dark";
    std::string toggle = "<a class=\"themebtn\" id=\"themetoggle\" href=\"/theme/"+next_theme+
        "\" aria-label=\"Switch to "+next_theme+"\" title=\"Switch to "+next_theme+"\">"+
        (g_theme_mode == "dark" ? "&#9728;" : "&#9789;")+"</a>";
    if (g_theme_mode.empty()) {
        // With no explicit lg_theme cookie, the page is actually dark or
        // light by prefers-color-scheme -- something the server can't see.
        // Guessing "dark" as the next theme (the code above's only option
        // without that information) is wrong for exactly the visitors
        // whose system is already in dark mode: the button shows a moon
        // ("switch to dark") while the page is already dark, and clicking
        // it sets an explicit dark cookie that changes nothing on screen --
        // reads as "the toggle doesn't work." Corrected client-side, once,
        // against the one thing only the browser actually knows.
        toggle +=
            "<script>(function(){"
            "if(window.matchMedia&&matchMedia('(prefers-color-scheme:dark)').matches){"
            "var t=document.getElementById('themetoggle');"
            "t.href='/theme/light';t.textContent='\\u2600';"
            "t.setAttribute('aria-label','Switch to light');t.title='Switch to light';"
            "}"
            "})();</script>";
    }
    const std::string full_title = html_escape(title) + " — LiquidGov.US";
    // What a link unfurls to on Facebook/X/Slack/etc. Without these tags a
    // shared liquidgov.us link shows nothing but the bare URL. One shared
    // description site-wide, not a per-page one -- there's no per-page
    // summary tracked anywhere to draw from. The line is the landing hero
    // ("This is YOUR government!") plus the limit the page itself states:
    // a voluntary association, not a replacement for the government you
    // already live under. The old Myspace joke stays on the genesis quote
    // card. It is not what a stranger should see in a share preview.
    const std::string og_desc =
        "This is YOUR government! Represent yourself — a voluntary "
        "association, not a replacement for the one you already live under.";
    // One shared image site-wide. v4 is a new path because v3 (and v2
    // before it) is cached immutable for a year on crawlers.
    const std::string og_image = "https://liquidgov.us/assets/og-image-v4.jpg";
    // Facebook's Sharing Debugger flags og:url as a missing-but-recommended
    // property if it's absent -- it's what the scraper treats as this
    // page's canonical identity (and what like/share counts attribute to).
    // page_wrap has no request path in scope (it's called from inside each
    // route handler, which would all need threading through to do this
    // per-page) -- same "one shared value site-wide" posture as og_desc/
    // og_image above, not a half-finished per-page version of this.
    const std::string og_url = "https://liquidgov.us/";
    const std::string fb_app_id = facebook_app_id_from_env();
    return "<!doctype html><html lang=\"en\""+theme_attr+"><head><meta charset=\"utf-8\">"
           // viewport-fit=cover: without it, env(safe-area-inset-bottom)
           // resolves to 0 everywhere per spec -- every bottom-fixed
           // element's safe-area term (jbtoggle, jbpanel, jbhint)
           // has silently been adding zero this whole time.
           "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,viewport-fit=cover\">"
           "<meta name=\"referrer\" content=\"strict-origin-when-cross-origin\">"
           "<title>"+full_title+"</title>"
           "<meta property=\"og:type\" content=\"website\">"
           "<meta property=\"og:site_name\" content=\"LiquidGov.US\">"
           "<meta property=\"og:url\" content=\""+og_url+"\">"+
           (fb_app_id.empty() ? "" : "<meta property=\"fb:app_id\" content=\""+fb_app_id+"\">") +
           "<meta property=\"og:title\" content=\""+full_title+"\">"
           "<meta property=\"og:description\" content=\""+og_desc+"\">"
           "<meta property=\"og:image\" content=\""+og_image+"\">"
           "<meta property=\"og:image:width\" content=\"1200\">"
           "<meta property=\"og:image:height\" content=\"630\">"
           "<meta name=\"twitter:card\" content=\"summary_large_image\">"
           "<meta name=\"twitter:title\" content=\""+full_title+"\">"
           "<meta name=\"twitter:description\" content=\""+og_desc+"\">"
           "<meta name=\"twitter:image\" content=\""+og_image+"\">"
           "<link rel=\"icon\" href=\"/favicon.svg\" type=\"image/svg+xml\">"
           // Installable as a standalone app (see /install for the actual
           // per-platform instructions): a manifest is what turns Chrome/
           // Edge/Android's browser-menu "Install"/"Add to Home screen"
           // from a plain bookmark into a real standalone window with this
           // name and icon. iOS Safari doesn't read this file at all --
           // its own "Add to Home Screen" (Share sheet) works from nothing
           // more than the page itself, which is why /install still needs
           // to spell that path out separately.
           // One app (Brandon 2026-11-06: "lets get rid of the multiple app
           // icons thing. that was a bad idea"): every page offers the same
           // manifest, icon and name, so Add to Home Screen always gives the
           // one LiquidGov icon. The old Messages/Social/Civic manifests and
           // icons are still served (icons already on phones keep working,
           // and deploy/nginx-www-apex.sh checks one) but nothing links them.
           "<link rel=\"manifest\" href=\"/manifest.webmanifest\">"
           "<meta name=\"apple-mobile-web-app-capable\" content=\"yes\">"
           "<meta name=\"mobile-web-app-capable\" content=\"yes\">"
           "<meta name=\"apple-mobile-web-app-title\" content=\"LiquidGov\">"
           "<link rel=\"apple-touch-icon\" href=\"/apple-touch-icon.png\">"
           "<meta name=\"theme-color\" content=\"#8a6620\">"
           "<link rel=\"preconnect\" href=\"https://fonts.googleapis.com\">"
           "<link rel=\"preconnect\" href=\"https://fonts.gstatic.com\" crossorigin>"
           "<link rel=\"stylesheet\" href=\"https://fonts.googleapis.com/css2?family=Playfair+Display:wght@400;700&display=swap\">"
           "<style>"+css()+"</style><link rel=\"stylesheet\" href=\""+asset_url("setup.css")+"\"><script src=\""+asset_url("setup.js")+"\" defer></script></head><body>"
           +(show_intro ? intro_overlay_html(intro_always_play) : std::string())+
           "<header class=\"top\"><div class=\"topinner\">"
           "<a class=\"brand\" href=\"/\">"
           "<svg class=\"mark\" viewBox=\"0 0 48 48\" width=\"28\" height=\"28\" aria-hidden=\"true\">"
           "<circle cx=\"24\" cy=\"24\" r=\"22.5\" fill=\"none\" style=\"stroke:var(--stone)\" stroke-width=\"1\"/>"
           "<circle cx=\"24\" cy=\"24\" r=\"19\" fill=\"none\" style=\"stroke:var(--accent)\" stroke-width=\"1.2\"/>"
           "<path d=\"M24 9c0 0 9.5 12.2 9.5 17.2a9.5 9.5 0 0 1-19 0C14.5 21.2 24 9 24 9z\" style=\"fill:var(--accent)\"/>"
           "</svg>"
           "<div><div class=\"word\"><strong>LiquidGov</strong><span class=\"tld\">.US</span></div>"
           "<div class=\"tagline\">Civic association &middot; Beta</div></div></a>"
           +[&]{
           std::string nav_out = navhtml;
           std::string header_toggle = toggle;
           const std::string open = "<nav class=\"dropdown-panel accountmenu\">";
           auto at = nav_out.find(open);
           if (at != std::string::npos) {
               nav_out.insert(at + open.size(),
                   "<div class=\"menu-theme-row\">"+toggle+"</div>");
               header_toggle.clear();
           }
           return nav_out+header_toggle;
       }()+"</div></header>"
           "<main class=\"wrap\">"+body+"</main>"
           +
           // "Who holds control" used to sit in this row as a plain link, reading like just another page (Laws, Privacy).
           // It stays reachable everywhere (/control is unchanged) but tucked behind a closed disclosure, same
           // <details><summary> pattern this file already uses for "Delete…" (see e.g. the post-menu one above) --
           // closed by default, one click to reveal, not a routine nav item.
           "<footer class=\"site\"><a href=\"/\">Genesis</a> · <a href=\"/laws\">Laws</a> · "
           "<a href=\"/install\">Install this as an app</a> · "
           "<a href=\"/privacy\">Privacy</a> · "
           "<details style=\"display:inline-block\"><summary class=\"muted\" style=\"cursor:pointer;display:inline\">Disclaimer</summary> "
           "<a href=\"/control\">Who holds control</a></details><br>"
           "Not a government. A platform account is not a citizen identity. "
           "LiquidGov.US is in Beta — some of what's here is still being built.</footer>"
           "</body></html>";
}

std::string portrait_tag(const std::string& username, const char* cls); // defined below, needed here for nav()
std::string post_block(const Post& p, const std::string& viewer);
std::string mail_widget_script(); // defined below, needed here for nav()
std::string flash_error(const std::map<std::string,std::string>& qs); // defined below, needed here for jukebox_widget()

std::string nav(const std::string& user, int unread) {
    if (user.empty()) {
        std::string burger = "<label class=\"navbtn\" for=\"navc\">Menu</label><input id=\"navc\" type=\"checkbox\">";
        // "Who holds control" is reachable from here too, but as a closed disclosure, not a top-level nav item --
        // same reasoning and the same <details><summary> pattern as the footer's, just below.
        return burger+"<nav><a href=\"/\">Genesis</a><a href=\"/laws\">Laws</a>"
               "<details style=\"display:inline-block\"><summary class=\"muted\" style=\"cursor:pointer;display:inline;min-height:44px;align-items:center\">Disclaimer</summary>"
               "<a href=\"/control\">Who holds control</a></details>"
               "<a href=\"/apps\">App</a>"
               "<a href=\"/signup\">Sign up</a><a href=\"/login\">Log in</a></nav>";
    }
    // Notifications and Messages are their own always-visible icon buttons
    // next to the avatar (checked constantly), not entries in the account
    // dropdown (opened a few times a session). unread splits the same way:
    // "message" notices badge Messages, everything else badges
    // Notifications -- see mark_notices_read_of_type/_except_type at the
    // /mail and /notices routes for the matching read-marking split.
    const int msg_unread = g_store.unread_message_count(user);
    const int other_unread = unread - msg_unread;
    // Hand-drawn line icons, not emoji: an emoji glyph's size and even
    // whether it renders at all is up to the platform's emoji font, which
    // proved unreliable here (the bell rendered far oversized, the
    // envelope as a missing-glyph box). An inline SVG with stroke=
    // currentColor renders identically everywhere and themes for free,
    // same reasoning as the brand mark/favicon already being hand-drawn
    // SVG rather than an emoji or icon-font glyph.
    const char* bell_svg =
        "<svg width=\"18\" height=\"18\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.8\" stroke-linecap=\"round\" stroke-linejoin=\"round\" aria-hidden=\"true\">"
        "<path d=\"M18 8a6 6 0 0 0-12 0c0 7-3 9-3 9h18s-3-2-3-9\"/>"
        "<path d=\"M13.73 21a2 2 0 0 1-3.46 0\"/></svg>";
    const char* mail_svg =
        "<svg width=\"18\" height=\"18\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.8\" stroke-linecap=\"round\" stroke-linejoin=\"round\" aria-hidden=\"true\">"
        "<rect x=\"2\" y=\"4\" width=\"20\" height=\"16\" rx=\"2\"/><path d=\"m22 6-10 7L2 6\"/></svg>";
    // Buttons, not links: opening either one has to fire a fetch for the
    // panel's content (see mail_widget_script below), not navigate to
    // /notices or /mail -- those full pages still exist (direct links,
    // bookmarks, JS-off fallback) but are no longer how the header itself
    // gets you there.
    std::string bell = std::string("<button type=\"button\" class=\"iconbtn\" id=\"notiftoggle\" "
        "aria-label=\"Notifications\" title=\"Notifications\">")+bell_svg;
    if (other_unread > 0) bell += "<span class=\"badge\">"+std::to_string(other_unread)+"</span>";
    bell += "</button><div class=\"dropdown-panel notifpanel\" id=\"notifpanel\" hidden>"
            "<div class=\"panelhead\">Notifications</div>"
            "<div id=\"notifroot\"><p class=\"muted\" style=\"padding:.7rem 1.1rem\">Loading&hellip;</p></div>"
            "</div>";
    std::string mail = std::string("<button type=\"button\" class=\"iconbtn\" id=\"mailtoggle\" "
        "aria-label=\"Messages\" title=\"Messages\">")+mail_svg;
    if (msg_unread > 0) mail += "<span class=\"badge\">"+std::to_string(msg_unread)+"</span>";
    mail += "</button><div class=\"dropdown-panel mailpanel\" id=\"mailpanel\" hidden>"
            "<div class=\"panelhead\">Messages</div>"
            "<div id=\"mailroot\"><p class=\"muted\" style=\"padding:.7rem 1.1rem\">Loading&hellip;</p></div>"
            "</div>";
    // Same #navc checkbox-toggle trick the mobile hamburger already uses
    // (see the CSS), just triggered by the avatar instead of a "Menu"
    // label, and not gated to narrow viewports -- clicking your own
    // picture is the trigger at every screen size now.
    std::string avatar = "<label class=\"navavatar\" for=\"navc\">"+portrait_tag(user,"sm")+"</label>"
                          "<input id=\"navc\" type=\"checkbox\">";
    // The menu's own name link goes to the public page anyone else sees
    // (/u/<username>), not the edit screen -- "Edit profile" on that
    // public page and the "Your profile" link on the home dashboard are
    // the way to actually reach /me.
    std::string menu = "<nav class=\"dropdown-panel accountmenu\">"
        "<a class=\"menu-name\" href=\"/u/"+html_escape(user)+"\">"+html_escape(user)+"</a>"
        "<a href=\"/settings\">Settings</a><a href=\"/apps\">App</a>"
        +(is_operator(user) ? "<a href=\"/admin\">Operator</a>" : "")+
        "<hr>"
        "<a href=\"/feed\">Feed</a>"
        "<a href=\"/friends\">People</a>"
        "<a href=\"/map\">Map</a>"
        "<a href=\"/communities\">Groups</a>"
        "<a href=\"/proposals\">Proposals</a>"
        "<a href=\"/officials\">Officials</a>"
        "<a href=\"/laws\">Laws</a>"
        "<a href=\"/logout\">Log out</a>"
        "</nav>";
    return bell+mail+avatar+menu+mail_widget_script();
}

std::string url_decode(const std::string& s) {
    std::string o;
    for (size_t i=0;i<s.size();++i) {
        if (s[i]=='+') o+=' ';
        else if (s[i]=='%' && i+2<s.size()) { int v=0; std::stringstream ss; ss<<std::hex<<s.substr(i+1,2); ss>>v; o+=char(v); i+=2; }
        else o+=s[i];
    }
    return o;
}
// Percent-encodes for safe embedding in a query string. Needed specifically
// for values that didn't come through our own validation (e.g. a raw login
// username attempt, which -- unlike a stored account's username -- isn't
// guaranteed to be in valid_username's restricted charset).
std::string url_encode(const std::string& s) {
    static const char* kHex = "0123456789ABCDEF";
    std::string o;
    for (unsigned char c : s) {
        if ((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~') o += char(c);
        else { o += '%'; o += kHex[c >> 4]; o += kHex[c & 0xf]; }
    }
    return o;
}

// Relative path only. Used as a post-login landing so /world and /feed
// don't dump a new member on a generic /login with no way back.
std::string safe_next_path(const std::string& n) {
    if (n.size() < 2 || n[0] != '/' || n[1] == '/') return {};
    if (n.find('\r') != std::string::npos || n.find('\n') != std::string::npos) return {};
    if (n.find('\\') != std::string::npos || n.find("://") != std::string::npos) return {};
    if (n.size() > 200) return {};
    if (n.rfind("/login", 0) == 0 || n.rfind("/signup", 0) == 0 || n.rfind("/logout", 0) == 0) return {};
    return n;
}
std::string login_url(const std::string& next) {
    auto n = safe_next_path(next);
    if (n.empty()) return "/login";
    return "/login?next=" + url_encode(n);
}
int count_digits(const std::string& s) {
    int n = 0;
    for (unsigned char c : s) if (c >= '0' && c <= '9') ++n;
    return n;
}

std::map<std::string,std::string> parse_form(const std::string& body) {
    std::map<std::string,std::string> m; std::stringstream ss(body); std::string part;
    while (std::getline(ss,part,'&')) { auto eq=part.find('='); if(eq==std::string::npos) continue;
        m[url_decode(part.substr(0,eq))]=url_decode(part.substr(eq+1)); }
    return m;
}
std::map<std::string,std::string> parse_qs(const std::string& path) {
    std::map<std::string,std::string> m;
    auto q = path.find('?');
    if (q==std::string::npos) return m;
    return parse_form(path.substr(q+1));
}
std::string cookie_of(const std::string& headers) {
    auto pos=headers.find("Cookie:"); if(pos==std::string::npos) pos=headers.find("cookie:");
    if(pos==std::string::npos) return {};
    auto end=headers.find("\r\n",pos); std::string line=headers.substr(pos,end-pos);
    auto k=line.find("lg_session="); if(k==std::string::npos) return {};
    k+=11; auto sc=line.find(';',k); return line.substr(k, sc==std::string::npos?std::string::npos:sc-k);
}
// "dark", "light", or "" -- "" means no explicit choice was ever saved, so
// the page stamps no data-theme attribute at all and prefers-color-scheme
// alone decides (see css()'s dark block). Set once per request (handle()
// is the whole site's single-threaded synchronous entry point, so a
// thread_local read at render time is exactly one request's worth of
// state, never shared or stale across requests).
std::string theme_of(const std::string& headers) {
    auto pos=headers.find("Cookie:"); if(pos==std::string::npos) pos=headers.find("cookie:");
    if(pos==std::string::npos) return {};
    auto end=headers.find("\r\n",pos); std::string line=headers.substr(pos,end-pos);
    auto k=line.find("lg_theme="); if(k==std::string::npos) return {};
    k+=9; auto sc=line.find(';',k);
    std::string v = line.substr(k, sc==std::string::npos?std::string::npos:sc-k);
    return (v=="dark"||v=="light") ? v : std::string{};
}
std::string header_value(const std::string& headers, const std::string& name) {
    std::string key = name + ":";
    auto pos = headers.find(key);
    if (pos == std::string::npos) {
        key = name;
        for (char& c : key) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        pos = headers.find(key + ":");
        if (pos == std::string::npos) return {};
        key += ":";
    }
    pos += key.size();
    while (pos < headers.size() && headers[pos]==' ') ++pos;
    auto end = headers.find("\r\n", pos);
    if (end == std::string::npos) return headers.substr(pos);
    return headers.substr(pos, end-pos);
}
std::string forwarded_ip(const std::string& req, const std::string& peer) {
    auto xff = header_value(req, "X-Forwarded-For");
    if (xff.empty()) return peer;
    auto comma = xff.find(',');
    std::string first = comma == std::string::npos ? xff : xff.substr(0, comma);
    while (!first.empty() && (first.front()==' '||first.front()=='\t')) first.erase(first.begin());
    while (!first.empty() && (first.back()==' '||first.back()=='\t')) first.pop_back();
    return first.empty() ? peer : first;
}
std::string device_label_from(const std::string& headers, const std::string& ip) {
    auto ua = header_value(headers, "User-Agent");
    std::string label;
    if (ua.find("Firefox") != std::string::npos) label = "Firefox";
    else if (ua.find("Edg/") != std::string::npos) label = "Edge";
    else if (ua.find("Chrome") != std::string::npos) label = "Chrome";
    else if (ua.find("Safari") != std::string::npos) label = "Safari";
    else if (ua.find("curl") != std::string::npos) label = "curl";
    else if (!ua.empty()) label = "web";
    else label = "web";
    if (!ip.empty()) label += " · " + ip;
    if (label.size() > 80) label.resize(80);
    return label;
}
// Date only, not a UTC timestamp -- the server can't know a visitor's
// timezone, so "2026-09-13 05:18 UTC" was just engineer-speak with no way
// to be locally correct. A bare date reads honestly everywhere it's used
// (posts, "member since", session activity, jukebox history) without
// needing a client-side reformat script for something this minor.
// Back to the page a post action came from, at that post (CODE-SCAN-UX A1, Brief 4 item 9: a 303 without a fragment
// lands at the top on iPhone Safari). Any #fragment the page had is replaced; no usable Referer is /feed.
// with_anchor: the same rule for every action that should come back at its item (posts, proposals).
std::string with_anchor(std::string url, const std::string& anchor) {
    auto hash = url.find('#');
    if (hash != std::string::npos) url.erase(hash);
    return url + "#" + anchor;
}
std::string back_to_post(const std::string& referer, std::uint64_t pid) {
    std::string back = referer;
    if (back.empty() || back.rfind("http", 0) != 0) back = "/feed";
    return with_anchor(back, "post-" + std::to_string(pid));
}
// Back to the page a proposal action came from (its next=, else /officials for a race, else /proposals), at that
// proposal (CODE-SCAN-UX A1, Brief 6 item 3: votes and reactions sit mid-page on a long list).
std::string back_to_proposal(const std::string& next, const Proposal& p) {
    std::string back = safe_next_path(next);
    if (back.empty()) back = p.is_office ? "/officials" : "/proposals";
    return with_anchor(back, "proposal-" + std::to_string(p.id));
}

std::string fmt_time(std::int64_t t) {
    if (t <= 0) return "";
    std::time_t tt = static_cast<std::time_t>(t);
    std::tm tm{};
    gmtime_r(&tt, &tm);
    char buf[40];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return buf;
}

std::optional<PlatformAccount> account_from_req(const std::string& headers) {
    auto sid = cookie_of(headers);
    auto sess=g_store.get_session(sid);
    if(!sess) return std::nullopt;
    g_store.touch_session(sid);
    return g_store.find_by_id(sess->account);
}
// Any page loaded into a frame (g_embed: Sec-Fetch-Dest: iframe) may be framed
// only by liquidgov.us itself -- the map's right-hand field. Another site's
// frame is refused, so nobody can disguise our buttons (clickjacking).
std::string frame_guard() {
    return g_embed ? "Content-Security-Policy: frame-ancestors 'self'\r\nX-Frame-Options: SAMEORIGIN\r\n" : "";
}
std::string http_ok(const std::string& body, const std::string& extra={}) {
    std::ostringstream o;
    o<<"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\nContent-Length: "<<body.size()<<"\r\n"<<frame_guard()<<extra<<"\r\n"<<body;
    return o.str();
}
std::string http_raw(const char* status, const char* ctype, const std::string& body, const std::string& extra={}) {
    std::ostringstream o;
    o<<"HTTP/1.1 "<<status<<"\r\nContent-Type: "<<ctype<<"\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\nContent-Length: "<<body.size()<<"\r\n"<<frame_guard()<<extra<<"\r\n"<<body;
    return o.str();
}
std::string http_code(const char* status, const std::string& body) {
    return http_raw(status, "text/html; charset=utf-8", body);
}
std::string http_redir(const std::string& loc, const std::string& extra={}) {
    std::ostringstream o;
    o<<"HTTP/1.1 303 See Other\r\nLocation: "<<loc<<"\r\nContent-Length: 0\r\nConnection: close\r\n"<<frame_guard()<<extra<<"\r\n";
    return o.str();
}

std::string trim_ws(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && (s[a]==' '||s[a]=='\t'||s[a]=='\r'||s[a]=='\n')) ++a;
    while (b > a && (s[b-1]==' '||s[b-1]=='\t'||s[b-1]=='\r'||s[b-1]=='\n')) --b;
    return s.substr(a, b-a);
}

// ASCII-only is fine here -- every email this ever runs on already passed
// looks_like_email (an ASCII shape check), and lowercasing is only ever
// used for the ml-tag dedup key, not for anything that has to be
// Unicode-correct.
std::string lowercase_ascii(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

// sha256_hex (liquidgov_blob) takes bytes, not a std::string -- this is
// the one place that conversion happens, for hashing a password-reset
// token before it's stored (see POST /forgot / POST /reset).
std::string sha256_hex_of(const std::string& s) {
    return sha256_hex(std::vector<std::uint8_t>(s.begin(), s.end()));
}

// Genesis operator tools (/admin, see its own comment) gate on this, not a
// second login -- same session as everyone else, just a username on a
// list. LIQUIDGOV_OPERATORS is comma-separated exact usernames (no
// wildcards, no roles); unset means the one-person default this site has
// actually always had. Checked fresh on every request, not cached, so
// changing the env var and restarting is the entire revocation story.
bool is_operator(const std::string& username) {
    if (username.empty()) return false;
    const char* env = std::getenv("LIQUIDGOV_OPERATORS");
    std::string list = (env && env[0]) ? env : "Bbottin";
    std::size_t i = 0;
    while (i <= list.size()) {
        auto comma = list.find(',', i);
        std::string name = trim_ws(list.substr(i, comma == std::string::npos ? std::string::npos : comma - i));
        if (!name.empty() && name == username) return true;
        if (comma == std::string::npos) break;
        i = comma + 1;
    }
    return false;
}

// The operator panel's delete confirmations (accounts, communities) ask
// for the word "delete" typed back, not the username/slug -- short enough
// to not be annoying, still enough friction that a stray click can't fire
// it. Case-insensitive so "Delete" doesn't quietly fail.
bool confirm_says_delete(const std::string& s) {
    if (s.size() != 6) return false;
    for (std::size_t i = 0; i < 6; ++i) {
        char c = s[i];
        char lc = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
        if (lc != "delete"[i]) return false;
    }
    return true;
}

std::string header_line_value(const std::string& headers, const char* name) {
    const std::size_t nlen = std::strlen(name);
    std::size_t i = 0;
    while (i < headers.size()) {
        std::size_t e = headers.find('\n', i);
        if (e == std::string::npos) e = headers.size();
        if (e - i >= nlen) {
            bool match = true;
            for (std::size_t k = 0; k < nlen; ++k) {
                char a = headers[i+k], b = name[k];
                if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
                if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
                if (a != b) { match = false; break; }
            }
            if (match && i + nlen < headers.size() && headers[i+nlen] == ':') {
                std::size_t v = i + nlen + 1;
                while (v < e && (headers[v]==' '||headers[v]=='\t')) ++v;
                std::size_t end = e;
                if (end > v && headers[end-1]=='\r') --end;
                return headers.substr(v, end-v);
            }
        }
        i = e + 1;
    }
    return {};
}

bool chunked_complete(const std::string& body) {
    std::size_t i = 0;
    while (i < body.size()) {
        auto nl = body.find("\r\n", i);
        if (nl == std::string::npos) return false;
        std::string sz = body.substr(i, nl-i);
        auto semi = sz.find(';');
        if (semi != std::string::npos) sz = sz.substr(0, semi);
        std::size_t n = 0;
        try { n = std::stoul(sz, nullptr, 16); } catch (...) { return false; }
        i = nl + 2;
        if (n == 0) return true;
        if (i + n + 2 > body.size()) return false;
        i += n + 2;
    }
    return false;
}

std::string decode_chunked(const std::string& body) {
    std::string out;
    std::size_t i = 0;
    while (i < body.size()) {
        auto nl = body.find("\r\n", i);
        if (nl == std::string::npos) break;
        std::string sz = body.substr(i, nl-i);
        auto semi = sz.find(';');
        if (semi != std::string::npos) sz = sz.substr(0, semi);
        std::size_t n = 0;
        try { n = std::stoul(sz, nullptr, 16); } catch (...) { break; }
        i = nl + 2;
        if (n == 0) break;
        if (i + n > body.size()) break;
        out.append(body, i, n);
        i += n;
        if (i + 2 <= body.size()) i += 2;
    }
    return out;
}

// Reads one request: `req` gets the request line and headers (ending in
// "\r\n\r\n"), `body` the body, kept apart so a 100 MB upload is held
// once, not copied into a header+body string and again for handle() (a
// 0.5 GB server, review 2026-10-27). max_body_for(headers) is this
// request's body limit (kMaxBody, or a program sender's /mail/big limit,
// see request_body_limit). Over it, *too_large is set and *declared gets
// the Content-Length (0 if unknown) so the caller can answer 413 and drain
// what the client still sends instead of handling a cut-off upload.
bool read_http_request(int fd, std::string& req, std::string& body,
                       const std::function<std::size_t(const std::string&)>& max_body_for,
                       bool* too_large, std::size_t* declared) {
    req.clear();
    body.clear();
    *too_large = false;
    *declared = 0;
    char buf[8192];
    auto more = [&]() -> ssize_t {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n > 0) req.append(buf, static_cast<std::size_t>(n));
        return n;
    };
    while (req.find("\r\n\r\n") == std::string::npos && req.find("\n\n") == std::string::npos) {
        if (more() <= 0) return !req.empty();
        if (req.size() > 1024 * 1024) return false;
    }
    if (req.find("\r\n\r\n") == std::string::npos) {
        std::string n;
        n.reserve(req.size() + 16);
        for (std::size_t i = 0; i < req.size(); ++i) {
            if (req[i] == '\n' && (i == 0 || req[i-1] != '\r')) n += "\r\n";
            else n += req[i];
        }
        req.swap(n);
    }
    auto sep = req.find("\r\n\r\n");
    if (sep == std::string::npos) return false;
    const std::size_t body_start = sep + 4;
    std::string headers = req.substr(0, sep);
    const std::size_t max_body = max_body_for(headers);

    std::string expect = header_line_value(headers, "Expect");
    std::string te = header_line_value(headers, "Transfer-Encoding");
    std::string cl = header_line_value(headers, "Content-Length");
    for (char& c : te) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    bool chunked = te.find("chunked") != std::string::npos;

    body.assign(req, body_start, std::string::npos); // what arrived with the headers (a few KB at most)
    req.resize(body_start);

    if (!chunked && !cl.empty()) {
        std::size_t need = 0;
        try { need = static_cast<std::size_t>(std::stoull(cl)); } catch (...) { need = 0; }
        if (need > max_body) { *too_large = true; *declared = need; return false; }
        if (!expect.empty() && body.empty()) {
            std::string low = expect;
            for (char& c : low) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
            if (low.find("100-continue") != std::string::npos) {
                const char msg[] = "HTTP/1.1 100 Continue\r\n\r\n";
                send(fd, msg, sizeof(msg) - 1, 0);
            }
        }
        body.reserve(need);
        while (body.size() < need) {
            ssize_t n = recv(fd, buf, sizeof(buf), 0);
            if (n <= 0) break;
            body.append(buf, static_cast<std::size_t>(n));
        }
        if (body.size() > need) body.resize(need);
        return true;
    }

    if (!expect.empty() && body.empty()) {
        std::string low = expect;
        for (char& c : low) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (low.find("100-continue") != std::string::npos) {
            const char msg[] = "HTTP/1.1 100 Continue\r\n\r\n";
            send(fd, msg, sizeof(msg) - 1, 0);
        }
    }

    if (chunked) {
        while (!chunked_complete(body)) {
            ssize_t n = recv(fd, buf, sizeof(buf), 0);
            if (n <= 0) break;
            body.append(buf, static_cast<std::size_t>(n));
            if (body.size() > max_body) { *too_large = true; return false; } // chunked: no length to drain to
        }
        body = decode_chunked(body);
        if (body.size() > max_body) { *too_large = true; return false; }
    }
    return true;
}

std::string global_jukebox_widget(const std::string& yt_key, const std::string& user); // defined below, needed here for wrap()

std::string wrap(const std::string& title, const std::string& user, int unread, const std::string& body, bool show_intro = false) {
    // Anonymous visitors have no prefs to consult -- they always get the
    // plain first-time-only (localStorage) behavior, same as before this
    // setting existed.
    const bool intro_always_play = show_intro && !user.empty() && g_store.prefs_of(user).intro_every_visit;
    // The one-time 988 notice goes on the next page this person sees, framed or not. Framed pages (the map's field,
    // and every page while Juketube's persistent shell keeps music playing) used to skip it, so someone browsing
    // with music on could go a long time without it (side-agent catch, Brandon 2026-11-08: fix it). Only visible
    // frames render wrap() in embed mode: the profile-theme preview frame (/u/<name>/theme) uses http_raw, not wrap.
    std::string care;
    if (!user.empty() && g_care_notice.erase(user)) {
        care = "<div class=\"card\" role=\"note\" style=\"border-left:4px solid var(--accent)\">"
               "<p><strong>If things feel heavy right now, you don't have to carry it alone.</strong> "
               "In the US you can call or text <a href=\"tel:988\"><strong>988</strong></a> "
               "(Suicide &amp; Crisis Lifeline) any time. It's free and confidential.</p>"
               "<p class=\"muted\">Crisis words are not saved as a problem, and nobody is told.</p></div>";
    }
    if (g_embed) {
        // Inside a frame (the map's right-hand field, or the Juketube shell): the page alone, no header,
        // footer, intro or jukebox. Links and forms keep working in the field.
        std::string page = page_wrap(title, "", care + body, false, false);
        const std::string fit =
            "<style>header.top,footer.site{display:none!important}"
            "main.wrap{padding-top:.75rem;padding-bottom:1.5rem}</style>";
        auto head = page.find("</head>");
        if (head != std::string::npos) page.insert(head, fit);
        return page;
    }
    std::string page = page_wrap(title, nav(user, unread), care + body, show_intro, intro_always_play);
    // Widget goes *after* </main>, not inside it: soft-navigation (see
    // jukebox_script's nav-interception) only ever replaces <main>'s
    // contents, so a sibling widget's live WebSocket/YT player survive a
    // page-to-page click untouched instead of being torn down and rebuilt.
    if (!user.empty()) {
        std::string widget =
            "<link rel=\"stylesheet\" href=\""+asset_url("juketube.css")+"\">"
            "<div id=\"juketube-root\"></div>"
            "<script src=\""+asset_url("juketube.js")+"\" defer></script><script src=\""+asset_url("push.js")+"\" defer></script>";
        auto pos = page.find("</main>");
        if (pos != std::string::npos) page.insert(pos + 7, widget);
    }
    return page;
}

std::string trim_copy(std::string s) {
    while (!s.empty() && (s.back()==' '||s.back()=='\t'||s.back()=='"'||s.back()=='\'')) s.pop_back();
    std::size_t i = 0;
    while (i < s.size() && (s[i]==' '||s[i]=='\t'||s[i]=='"'||s[i]=='\'')) ++i;
    return s.substr(i);
}

// The first multipart part named `field`: its header block and its data,
// as views into `body` -- no copies, since a part can be a 100 MB program.
struct MultipartPart { std::string_view headers, data; };
std::optional<MultipartPart> multipart_part(const std::string& content_type, const std::string& body_str,
                                            const char* field) {
    auto bpos = content_type.find("boundary=");
    if (bpos == std::string::npos) return std::nullopt;
    std::string boundary = trim_copy(content_type.substr(bpos + 9));
    auto semi = boundary.find(';');
    if (semi != std::string::npos) boundary.resize(semi);
    boundary = trim_copy(boundary);
    if (boundary.empty()) return std::nullopt;
    const std::string delim = "--" + boundary;
    const std::string needle = std::string("name=\"") + field + "\"";
    const std::string_view body(body_str);
    std::size_t i = 0;
    while (i < body.size()) {
        auto start = body.find(delim, i);
        if (start == std::string_view::npos) break;
        start += delim.size();
        if (start + 1 < body.size() && body[start] == '-' && body[start + 1] == '-') break;
        if (start + 1 < body.size() && body[start] == '\r' && body[start + 1] == '\n') start += 2;
        else if (start < body.size() && body[start] == '\n') start += 1;
        auto next = body.find(delim, start);
        if (next == std::string_view::npos) break;
        std::string_view part = body.substr(start, next - start);
        while (part.size() >= 2 && part[part.size() - 2] == '\r' && part.back() == '\n')
            part.remove_suffix(2);
        auto hdr_end = part.find("\r\n\r\n");
        std::size_t skip = 4;
        if (hdr_end == std::string_view::npos) { hdr_end = part.find("\n\n"); skip = 2; }
        if (hdr_end == std::string_view::npos) { i = next; continue; }
        std::string_view hdrs = part.substr(0, hdr_end);
        if (hdrs.find(needle) == std::string_view::npos) { i = next; continue; }
        return MultipartPart{hdrs, part.substr(hdr_end + skip)};
    }
    return std::nullopt;
}

std::optional<std::vector<std::uint8_t>> multipart_file(const std::string& content_type,
                                                        const std::string& body,
                                                        const char* field) {
    auto part = multipart_part(content_type, body, field);
    if (!part) return std::nullopt;
    return std::vector<std::uint8_t>(part->data.begin(), part->data.end());
}

// The filename="..." a browser sent with a file part, or "" (no name, or
// not a file part). Untrusted: sanitize_download_name before keeping it.
std::string multipart_filename(const std::string& content_type, const std::string& body, const char* field) {
    auto part = multipart_part(content_type, body, field);
    if (!part) return {};
    auto k = part->headers.find("filename=\"");
    if (k == std::string_view::npos) return {};
    k += 10;
    auto end = part->headers.find('"', k);
    if (end == std::string_view::npos) return {};
    return std::string(part->headers.substr(k, end - k));
}

// A multipart text field (not a file) is the same wire shape as a file
// part — a header block, blank line, then the value — so multipart_file
// already extracts it correctly; this just gives that a string-typed name
// at call sites that want a form field, not bytes.
std::string multipart_field(const std::string& content_type, const std::string& body,
                             const char* field) {
    auto v = multipart_file(content_type, body, field);
    if (!v) return {};
    return std::string(v->begin(), v->end());
}

// Same walk as multipart_file, but collects every part named `field`
// instead of stopping at the first -- for a checkbox group (except/friend/
// group on the /feed composer) where the same name repeats once per
// checked box.
std::vector<std::string> multipart_field_all(const std::string& content_type, const std::string& body,
                                              const char* field) {
    std::vector<std::string> out;
    auto bpos = content_type.find("boundary=");
    if (bpos == std::string::npos) return out;
    std::string boundary = trim_copy(content_type.substr(bpos + 9));
    auto semi = boundary.find(';');
    if (semi != std::string::npos) boundary.resize(semi);
    boundary = trim_copy(boundary);
    if (boundary.empty()) return out;
    const std::string delim = "--" + boundary;
    const std::string needle = std::string("name=\"") + field + "\"";
    std::size_t i = 0;
    while (i < body.size()) {
        auto start = body.find(delim, i);
        if (start == std::string::npos) break;
        start += delim.size();
        if (start + 1 < body.size() && body[start] == '-' && body[start + 1] == '-') break;
        if (start + 1 < body.size() && body[start] == '\r' && body[start + 1] == '\n') start += 2;
        else if (start < body.size() && body[start] == '\n') start += 1;
        auto next = body.find(delim, start);
        if (next == std::string::npos) break;
        std::string part = body.substr(start, next - start);
        while (part.size() >= 2 && part[part.size() - 2] == '\r' && part.back() == '\n')
            part.resize(part.size() - 2);
        auto hdr_end = part.find("\r\n\r\n");
        std::size_t skip = 4;
        if (hdr_end == std::string::npos) { hdr_end = part.find("\n\n"); skip = 2; }
        if (hdr_end != std::string::npos) {
            std::string hdrs = part.substr(0, hdr_end);
            if (hdrs.find(needle) != std::string::npos) out.push_back(part.substr(hdr_end + skip));
        }
        i = next;
    }
    return out;
}

// Same idea for a urlencoded body (parse_form itself keeps only the last
// value per key, which is right for every single-valued field on this
// site -- this is only for a checkbox group's repeated name).
std::vector<std::string> parse_form_multi(const std::string& body, const std::string& key) {
    std::vector<std::string> out;
    std::stringstream ss(body);
    std::string part;
    while (std::getline(ss, part, '&')) {
        auto eq = part.find('=');
        if (eq == std::string::npos) continue;
        if (url_decode(part.substr(0, eq)) == key) out.push_back(url_decode(part.substr(eq + 1)));
    }
    return out;
}

// Group rosters and messages key on AccountId, which can outlive the
// account itself (delete_account() does not prune conversations_/
// messages_ — see file_store.cpp). Same "relabel, don't erase" choice
// already used for mail_/posts_.
std::string display_name_for(AccountId id) {
    auto acc = g_store.find_by_id(id);
    if (!acc) return "[deleted account]";
    return acc->username;
}

// List label: named groups use the title; unnamed ones list everyone else.
std::string group_label(const Conversation& conv, AccountId viewer) {
    if (!conv.title.empty()) return conv.title;
    std::string label;
    for (const auto& m : conv.members) {
        if (m.account == viewer) continue;
        if (!label.empty()) label += ", ";
        label += display_name_for(m.account);
    }
    return label.empty() ? "(just you)" : label;
}

// The fixed top-level list a community's creator picks from -- see
// CommunityCategory's own comment in file_store.hpp for why this list is
// closed while subcategory (free text) stays open. Order here is display
// order everywhere the list is rendered.
struct CategoryInfo { CommunityCategory value; const char* label; };
constexpr CategoryInfo kCommunityCategories[] = {
    {CommunityCategory::LocalCivic, "Local & Civic"},
    {CommunityCategory::PolicyGovernment, "Policy & Government"},
    {CommunityCategory::ArtsCulture, "Arts & Culture"},
    {CommunityCategory::HobbiesInterests, "Hobbies & Interests"},
    {CommunityCategory::SportsRecreation, "Sports & Recreation"},
    {CommunityCategory::Technology, "Technology"},
    {CommunityCategory::BusinessTrade, "Business & Trade"},
    {CommunityCategory::LearningEducation, "Learning & Education"},
    {CommunityCategory::SupportWellbeing, "Support & Wellbeing"},
    {CommunityCategory::FaithSpirituality, "Faith & Spirituality"},
    {CommunityCategory::General, "General"},
};
const char* category_label(CommunityCategory c) {
    for (const auto& ci : kCommunityCategories) if (ci.value == c) return ci.label;
    return "General";
}

// Every category's currently-used subcategory names, as a JS object literal
// ({0:['Neighborhood Watch',...], ...}) embedded into the creation form's
// own inline script -- lets picking a category filter the subcategory
// datalist to just that category's existing names, entirely client-side,
// no fetch needed. Same js_string_literal used for every other inline-JS
// string on this site (see jukebox_stations_js for the identical shape).
std::string community_subcats_js() {
    std::ostringstream os;
    os << "{";
    bool first_cat = true;
    for (const auto& ci : kCommunityCategories) {
        if (!first_cat) os << ",";
        first_cat = false;
        os << static_cast<int>(ci.value) << ":[";
        bool first_sub = true;
        for (const auto& s : g_store.subcategories_of(ci.value)) {
            if (!first_sub) os << ",";
            first_sub = false;
            os << js_string_literal(s);
        }
        os << "]";
    }
    os << "}";
    return os.str();
}

// "/city/<key>" for a group's city text, percent-encoded (keys can hold
// UTF-8). Empty when the text has no city key.
std::string city_href(const std::string& city) {
    const std::string key = city_key(city);
    return key.empty() ? std::string() : "/city/" + url_encode(key);
}

// <datalist id="citylist"> of every city already in use, so "Fresno, CA"
// isn't split three ways by spelling (same idea as the subcategory list).
std::string city_datalist_html() {
    std::string html = "<datalist id=\"citylist\">";
    for (const auto& cu : g_store.cities_in_use()) html += "<option value=\"" + html_escape(cu.name) + "\">";
    return html + "</datalist>";
}

// ---- Wishes and pain points (first slice, per Grok's review) ------------
// One text this person wrote. Returns how many new suggestions were stored.
// The switch, an empty user, or empty text stores nothing. Crisis words
// store nothing from this text and arm the 988 note; later texts still run.
// Callers pass this person's own words: posts, proposals, proposal
// comments, and messages they sent. Not the other person's messages.
int notice_pains_added(const std::string& user, const std::string& text, const std::string& source) {
    if (user.empty() || text.empty() || !g_store.map_prefs_of(user).pains) return 0;
    const PainScan scan = find_pain_points(text);
    if (scan.crisis) { g_care_notice.insert(user); return 0; }
    int n = 0;
    for (const auto& hit : scan.hits)
        if (g_store.add_pain_suggestion(user, hit, source)) ++n;
    return n;
}
void notice_pains(const std::string& user, const std::string& text, const std::string& source) {
    notice_pains_added(user, text, source);
}

// A public source can be Flowed. Friends posts, group posts, and messages
// you sent use another prefix and stay on your map only.
bool pain_can_flow(const std::string& source) {
    return source.rfind("post:", 0) == 0 || source.rfind("prop:", 0) == 0 || source.rfind("prply:", 0) == 0;
}
std::string post_pain_source(const Post& post) {
    if (post.audience == PostAudience::Anybody)
        return "post:" + std::to_string(post.id);
    const bool grouped = post.audience == PostAudience::GroupsOnly || !post.community.empty() || !post.extra_groups.empty();
    return (grouped ? "gpost:" : "fpost:") + std::to_string(post.id);
}

// "Look at what I've written": the same read, for writing already saved.
// Own posts (not reposts), proposals they wrote (not an office race),
// proposal comments they wrote, messages they sent, and group messages
// they sent. Never the other person's messages. Returns -1 when the
// switch is off, so the page does not say the scan was empty.
int look_public_words(const std::string& user) {
    if (user.empty() || !g_store.map_prefs_of(user).pains) return -1;
    int n = 0;
    for (const auto& post : g_store.posts_by(user)) {
        if (post.author != user) continue;
        if (!post.repost_of_author.empty() || post.repost_of_id != 0) continue;
        n += notice_pains_added(user, post.text, post_pain_source(post));
    }
    for (const auto& pr : g_store.all_proposals()) {
        if (pr.author == user && !pr.is_office)
            n += notice_pains_added(user, pr.title + ". " + pr.body, "prop:" + std::to_string(pr.id));
        for (const auto& r : g_store.proposal_replies_of(pr.id)) {
            if (r.author != user) continue;
            n += notice_pains_added(user, r.text, "prply:" + std::to_string(r.id));
        }
    }
    for (const auto& m : g_store.mail_sent_by(user)) {
        if (m.from != user || m.text.empty()) continue;
        n += notice_pains_added(user, m.text, "mail:" + std::to_string(m.ts) + ":" + m.to);
    }
    if (auto who = g_store.find_by_username(user)) {
        for (const auto& conv : g_store.conversations_for(who->id)) {
            for (const auto& m : g_store.messages_in(conv.id)) {
                if (m.sender != who->id || m.text.empty()) continue;
                if (m.deletion != DeletionState::NotDeleted) continue;
                n += notice_pains_added(user, m.text, "gmsg:" + std::to_string(m.id));
            }
        }
    }
    return n;
}

// What a kept pain point could connect to, in the cities this person is in
// (a group there, or listed there) plus sitewide proposals: departments whose
// name shares a word, proposals whose title does, groups whose name, about or
// subcategory does. No overlap = no suggestion (a wrong label is worse).
struct PainMatches {
    std::vector<std::pair<std::string, std::string>> depts; // city key, tag
    std::vector<Proposal> props;
    std::vector<Community> groups;
    std::string city_key;   // first city they are in, to prefill "Start a proposal"
    std::string city_name;
};
PainMatches pain_matches(const PainPoint& p) {
    constexpr std::size_t kCap = 5;
    PainMatches m;
    const auto kw = pain_keywords(p.phrase);
    std::vector<std::string> keys;
    for (const auto& cu : g_store.cities_in_use()) {
        if (!g_store.in_city(cu.key, p.owner) && !g_store.is_city_listed(cu.key, p.owner)) continue;
        keys.push_back(cu.key);
        if (m.city_key.empty()) { m.city_key = cu.key; m.city_name = cu.name; }
    }
    std::set<std::uint64_t> seen_props;
    for (const auto& key : keys) {
        for (const auto& t : g_store.city_tags(key))
            if (m.depts.size() < kCap && pain_words_overlap(kw, t)) m.depts.push_back({key, t});
        for (const auto& pr : g_store.proposals_in_city(key))
            if (m.props.size() < kCap && pain_words_overlap(kw, pr.title) && seen_props.insert(pr.id).second) m.props.push_back(pr);
        for (const auto& c : g_store.communities_in_city(key))
            if (m.groups.size() < kCap && pain_words_overlap(kw, c.name + " " + c.about + " " + c.subcategory)) m.groups.push_back(c);
    }
    for (const auto& pr : g_store.all_proposals())
        if (m.props.size() < kCap && pr.city.empty() && pain_words_overlap(kw, pr.title) && seen_props.insert(pr.id).second)
            m.props.push_back(pr);
    return m;
}

// ---- Map privacy rules, shared by every map view ------------------------
// May `viewer` see person `u` as a box on the map? Never someone blocked
// either way; never someone who turned "Show me on the map" off or whose
// profile is hidden from this viewer. You always see yourself.
bool map_person_ok(const std::string& u, const std::string& viewer) {
    if (u.empty()) return false;
    if (u == viewer) return true;
    if (!g_store.find_by_username(u)) return false; // deleted account
    if (!viewer.empty() && (g_store.is_blocked(viewer, u) || g_store.is_blocked(u, viewer))) return false;
    if (!g_store.map_prefs_of(u).show) return false;
    return visible_to(g_store.prefs_of(u).profile_visibility, false, !viewer.empty(),
                      !viewer.empty() && g_store.are_friends(viewer, u));
}

// The viewer has Flowed this same subject on a box of their own.
// Anyone who has not gets no match list: the subject can be visible
// without turning it into a directory of who else shares it.
bool viewer_flowed_subject(const std::string& viewer, const std::string& phrase) {
    const std::string key = pain_subject_key(phrase);
    if (key.empty() || viewer.empty()) return false;
    for (const auto& p : g_store.pains_of(viewer))
        if (p.kept && p.flowed && pain_subject_key(p.phrase) == key) return true;
    return false;
}

// Other people who Flowed this subject and whom the viewer may see.
// One row per person. The viewer and this box's owner are left out.
// Empty unless the viewer Flowed it too. No count is returned.
std::vector<PainPoint> subject_matches(const std::string& viewer, const PainPoint& p) {
    std::vector<PainPoint> out;
    if (!p.kept || !p.flowed || !viewer_flowed_subject(viewer, p.phrase)) return out;
    std::set<std::string> seen;
    seen.insert(viewer);
    seen.insert(p.owner);
    for (const auto& q : g_store.flowed_with_subject(p.phrase)) {
        if (!seen.insert(q.owner).second) continue;
        if (!map_person_ok(q.owner, viewer)) continue;
        out.push_back(q);
        if (out.size() >= 20) break; // leftover names are omitted, never counted
    }
    return out;
}

std::string also_flowed_html(const std::vector<PainPoint>& people) {
    if (people.empty()) return {};
    std::string h = "<p class=\"muted\">Also flowed by ";
    for (std::size_t i = 0; i < people.size(); ++i) {
        if (i) h += ", ";
        h += "<a href=\"/u/" + html_escape(people[i].owner) + "\">" + html_escape(people[i].owner) + "</a>";
    }
    h += ".</p>";
    return h;
}

// May `viewer` see this post as a box? The same audience rules as the
// author's profile page, plus the author's "Show my posts" map setting.
bool map_post_ok(const Post& p, const std::string& viewer) {
    if (p.author == viewer) return true;
    if (!map_person_ok(p.author, viewer) || !g_store.map_prefs_of(p.author).posts) return false;
    switch (p.audience) {
        case PostAudience::Anybody:
            return std::find(p.exceptions.begin(), p.exceptions.end(), viewer) == p.exceptions.end();
        case PostAudience::GroupsOnly: {
            if (!p.community.empty() && g_store.is_member(p.community, viewer)) return true;
            for (const auto& g : p.extra_groups) if (g_store.is_member(g, viewer)) return true;
            return false;
        }
        default: { // Friends
            if (!g_store.are_friends(viewer, p.author)) return false;
            if (g_store.prefs_of(p.author).posts_visibility == Visibility::OnlyMe) return false;
            return p.friend_allowlist.empty() ||
                   std::find(p.friend_allowlist.begin(), p.friend_allowlist.end(), viewer) != p.friend_allowlist.end();
        }
    }
}

// ---- Interests on the map (map SPEC phase 1, section 3.7; Brandon 2026-11-11 "yes, build phase 1") ----------
// Your interests are the comma items of your profile's Interests line, read at view time (no record type). A box id is
// int:<key>: the item's words, lowercase, joined by '-' ("Public Transit" -> public-transit); its label is the item as
// typed. Matching is by whole words, so "parks" never matches "Sparkshop" or "Parking"; a simple plural counts as the
// same word ("park" = "parks", "bus" = "buses"; side agent 2026-11-11), nothing looser.
std::vector<std::string> map_words(const std::string& text) {
    std::vector<std::string> out;
    std::string cur;
    auto flush = [&]() { if (!cur.empty()) out.push_back(cur); cur.clear(); };
    const auto* s = reinterpret_cast<const unsigned char*>(text.data());
    for (std::size_t i = 0, n = text.size(); i < n; ++i) {
        const unsigned char c = s[i];
        // curly quotes and apostrophes (U+2018/2019/201C/201D, what an iPhone types: "Kids’") separate words like ' does
        if (c == 0xE2 && i + 2 < n && s[i + 1] == 0x80 && (s[i + 2] == 0x98 || s[i + 2] == 0x99 || s[i + 2] == 0x9C || s[i + 2] == 0x9D)) {
            flush(); i += 2; continue;
        }
        // accented capitals (Latin-1, U+00C0-00DE except the multiplication sign) lowercase like ASCII: CAFÉ = café
        if (c == 0xC3 && i + 1 < n && s[i + 1] >= 0x80 && s[i + 1] <= 0x9E && s[i + 1] != 0x97) {
            cur.push_back(static_cast<char>(0xC3)); cur.push_back(static_cast<char>(s[i + 1] + 0x20)); ++i; continue;
        }
        if (std::isalnum(c) || c >= 0x80) cur.push_back(static_cast<char>(std::tolower(c))); // bytes >= 0x80: UTF-8 letters
        else flush();
    }
    flush();
    return out;
}
// A word and the singulars it could be: parks -> {parks, park}, buses -> {buses, buse, bus}, houses -> {houses, house,
// hous}, boxes -> {boxes, boxe, box}, cities -> {cities, city}. Two words match when they share a form, so park = parks,
// bus = buses, house = houses; "parking" and "sparks" never meet "park". "-es" is dropped only after s, x, z, ch, sh
// (where English adds it), so James never becomes "jam" (Grok Bot's check 2026-11-11); a few words that only look
// plural keep their s ("news" is not "new"). Words of 3 letters or fewer are left alone ("bus", "gas").
std::vector<std::string> map_word_forms(const std::string& w) {
    static const std::set<std::string> kNotPlural{"news", "series", "species"};
    std::vector<std::string> f{w};
    const auto n = w.size();
    if (kNotPlural.count(w)) return f;
    auto ends = [&](const char* s) { const std::string e(s); return n >= e.size() && w.compare(n - e.size(), e.size(), e) == 0; };
    if (n > 3 && ends("s") && !ends("ss")) f.push_back(w.substr(0, n - 1));
    if (n > 4 && (ends("ses") || ends("xes") || ends("zes") || ends("ches") || ends("shes"))) f.push_back(w.substr(0, n - 2));
    if (n > 4 && ends("ies")) f.push_back(w.substr(0, n - 3) + "y");
    return f;
}
std::set<std::string> map_folded_words(const std::string& text) {
    std::set<std::string> out;
    for (const auto& w : map_words(text)) for (auto& f : map_word_forms(w)) out.insert(std::move(f));
    return out;
}
// Every word of the interest appears as a whole word in the text (any shared form).
bool map_interest_matches(const std::string& interest, const std::set<std::string>& text_words) {
    const auto words = map_words(interest);
    if (words.empty()) return false;
    for (const auto& w : words) {
        bool hit = false;
        for (const auto& f : map_word_forms(w)) if (text_words.count(f)) { hit = true; break; }
        if (!hit) return false;
    }
    return true;
}
struct MapInterest { std::string key, label; };
std::vector<MapInterest> map_interests_of(const std::string& user) {
    std::vector<MapInterest> out;
    std::set<std::string> seen;
    auto add = [&](std::string item) {
        while (!item.empty() && std::isspace(static_cast<unsigned char>(item.front()))) item.erase(item.begin());
        while (!item.empty() && std::isspace(static_cast<unsigned char>(item.back()))) item.pop_back();
        std::string key;
        for (const auto& w : map_words(item)) key += (key.empty() ? "" : "-") + w;
        if (key.empty() || key.size() > 60 || out.size() >= 12 || !seen.insert(key).second) return;
        std::string label;  // runs of spaces inside an item become one ("Public   Transit" -> "Public Transit")
        for (char c : item) if (!(c == ' ' && !label.empty() && label.back() == ' ')) label.push_back(c);
        out.push_back({key, label});
    };
    std::string cur;
    for (char c : g_store.profile_of(user).interests) {
        if (c == ',' || c == ';') { add(cur); cur.clear(); } else cur.push_back(c);
    }
    add(cur);
    return out;
}

// ---- The interactive map's data: one box and its connections ------------
// Box ids: root, sitewide, city:<key>, group:<slug>, user:<name>,
// dept:<key>|<tag>, prop:<id>, post:<id>, pain:<id>. /map/node?n=<id> answers with the
// box and every box it connects to that this viewer may see, as JSON. The
// "page" of a box is what the right-hand field shows.
struct GNode { std::string id, label, kind, page, sub; }; // sub: a short second line (your kind boxes only, M1.1 look)
struct GLink { GNode to; std::string edge; };

std::optional<GNode> map_gnode(const std::string& id, const std::string& viewer) {
    auto colon = id.find(':');
    const std::string type = colon == std::string::npos ? id : id.substr(0, colon);
    const std::string rest = colon == std::string::npos ? "" : id.substr(colon + 1);
    auto as_u64 = [](const std::string& s) -> std::optional<std::uint64_t> {
        if (s.empty() || s.size() > 19) return std::nullopt;
        for (char c : s) if (c < '0' || c > '9') return std::nullopt;
        return std::stoull(s);
    };
    if (id == "root") return GNode{"root", "MAINstream", "root", ""};
    if (id == "sitewide") return GNode{"sitewide", "Sitewide", "sitewide", "/proposals"};
    // M1.1 (Brandon 2026-11-10): the signed-in map starts at you, and its first ring is kinds of connection. Both are
    // the viewer's own, derived at read time (no record type); "you" is always the person asking, never anyone else.
    if (id == "you") {
        if (viewer.empty()) return std::nullopt;
        return GNode{"you", viewer, "you", "/u/" + url_encode(viewer)};
    }
    if (type == "mine") {
        if (viewer.empty()) return std::nullopt;
        if (rest == "friends") return GNode{id, "Friends", "mine", "/friends"};
        if (rest == "groups") return GNode{id, "Groups", "mine", "/communities"};
        if (rest == "places") return GNode{id, "Places", "mine", ""};
        if (rest == "politics") return GNode{id, "Politics", "mine", "/proposals"};
        if (rest == "problems") return GNode{id, "Problems", "mine", "/problems"};
        // map SPEC phase 1 (Brandon 2026-11-11): Events stays empty until M1.4 (shown anyway, "None yet"), Feeds
        // (never Drawings), Interests from your profile
        if (rest == "events") return GNode{id, "Events", "mine", ""};
        if (rest == "feeds") return GNode{id, "Feeds", "mine", "/feed"};
        if (rest == "interests") return GNode{id, "Interests", "mine", "/me"};
        return std::nullopt;
    }
    if (type == "int") { // one of YOUR interests; anyone else asking for it gets nothing
        if (viewer.empty()) return std::nullopt;
        for (const auto& i : map_interests_of(viewer))
            if (i.key == rest) return GNode{id, i.label, "interest", ""};
        return std::nullopt;
    }
    if (type == "feed") { // one of your feeds; its posts are what you may see anyway (map_post_ok)
        if (viewer.empty()) return std::nullopt;
        if (rest == "friends") return GNode{id, "Friends' posts", "feed", "/feed"};
        if (rest == "groups") return GNode{id, "Your groups' posts", "feed", "/feed"};
        if (rest == "anybody-near") return GNode{id, "Posts near you", "feed", "/feed"};
        if (rest == "mine") return GNode{id, "Your posts", "feed", "/u/" + url_encode(viewer)};
        return std::nullopt;
    }
    if (type == "city") {
        if (rest.empty() || city_key(rest) != rest) return std::nullopt;
        std::string name;
        for (const auto& cu : g_store.cities_in_use()) if (cu.key == rest) name = cu.name;
        if (name.empty() && g_store.proposals_in_city(rest).empty()) return std::nullopt;
        return GNode{id, name.empty() ? rest : name, "city", "/city/" + url_encode(rest)};
    }
    if (type == "group") {
        auto c = g_store.community(rest);
        if (!c) return std::nullopt;
        return GNode{id, c->name, "group", "/c/" + url_encode(c->slug)};
    }
    if (type == "user") {
        if (!map_person_ok(rest, viewer)) return std::nullopt;
        return GNode{id, rest, "person", "/u/" + url_encode(rest)};
    }
    if (type == "dept") {
        auto bar = rest.find('|');
        if (bar == std::string::npos) return std::nullopt;
        const std::string key = rest.substr(0, bar), tag = rest.substr(bar + 1);
        for (const auto& t : g_store.city_tags(key))
            if (t == tag) return GNode{id, tag, "dept", "/proposals?city=" + url_encode(key)};
        return std::nullopt;
    }
    if (type == "prop") {
        auto n = as_u64(rest);
        if (!n) return std::nullopt;
        auto p = g_store.proposal_by_id(*n);
        if (!p) return std::nullopt;
        return GNode{id, p->title, "prop", "/proposals?id=" + std::to_string(p->id)};
    }
    if (type == "pain") { // a kept wish or pain point. Private until its owner Flows it.
        auto n = as_u64(rest);
        if (!n || viewer.empty()) return std::nullopt;
        auto p = g_store.pain_by_id(*n);
        if (!p || !p->kept) return std::nullopt;
        if (p->owner == viewer)
            return GNode{id, p->phrase, "pain", "/problems#pain" + std::to_string(p->id)};
        // Other people see the subject only. The href is not the owner's list.
        if (!p->flowed || !map_person_ok(p->owner, viewer)) return std::nullopt;
        return GNode{id, p->phrase, "pain", "/pain/" + std::to_string(p->id)};
    }
    if (type == "post") {
        auto n = as_u64(rest);
        if (!n) return std::nullopt;
        auto p = g_store.post_by_id(*n);
        if (!p || !map_post_ok(*p, viewer)) return std::nullopt;
        std::string label = p->text.empty() ? "(attachment)" : p->text;
        for (char& ch : label) if (ch == '\n' || ch == '\r' || ch == '\t') ch = ' ';
        return GNode{id, label, "post", "/post/" + std::to_string(p->id)};
    }
    return std::nullopt;
}

// The items under one of your kind boxes (defined after map_ctx, which it uses).
void map_mine_links(const std::string& kind, const std::string& viewer,
                    const std::function<void(const std::string&, const std::string&)>& link);
// The posts of one of your feeds, newest first, at most `limit`, each one you may see (map_post_ok). emit returns false
// to stop early. Defined after map_ctx (Near you uses your cities).
void map_feed_posts(const std::string& feed, const std::string& viewer, std::size_t limit,
                    const std::function<bool(const Post&)>& emit);
// The groups, proposals and people under one of your interests (defined after map_ctx).
void map_interest_links(const std::string& key, const std::string& viewer,
                        const std::function<void(const std::string&, const std::string&)>& link);

std::vector<GLink> map_links(const std::string& id, const std::string& viewer) {
    constexpr std::size_t kMax = 40; // per kind, newest first where it matters
    std::vector<GLink> out;
    std::set<std::string> seen;
    auto link = [&](const std::string& to, const std::string& edge) {
        if (to == id || seen.count(to)) return;
        auto n = map_gnode(to, viewer);
        if (!n) return;
        seen.insert(to);
        out.push_back({*n, edge});
    };
    auto person_groups_ok = [&](const std::string& u) { return u == viewer || g_store.map_prefs_of(u).groups; };
    auto person_comments_ok = [&](const std::string& u) { return u == viewer || g_store.map_prefs_of(u).comments; };
    auto prop_links = [&](const Proposal& p) {
        if (!p.city.empty()) link("city:" + p.city, "in");
        for (const auto& t : p.tags) link("dept:" + p.city + "|" + t, "touches");
        link("user:" + p.author, "wrote");
        for (const auto& r : g_store.proposal_replies_of(p.id))
            if (person_comments_ok(r.author)) link("user:" + r.author, "commented");
    };
    auto colon = id.find(':');
    const std::string type = colon == std::string::npos ? id : id.substr(0, colon);
    const std::string rest = colon == std::string::npos ? "" : id.substr(colon + 1);

    if (id == "you") {
        if (viewer.empty()) return out;
        // A kind with nothing in it is left off; Places always has the whole site. Clockwise from the top in the order
        // of the map SPEC's mockup A (Brandon's seven kinds, then your own Problems). Events always shows, even empty
        // (Brandon 2026-11-11 4:30 PM decision 1), so people see the map has a place for events before M1.4.
        for (const char* k : {"politics", "friends", "events", "feeds", "places", "groups", "interests", "problems"}) {
            bool any = std::string(k) == "events";
            map_mine_links(k, viewer, [&](const std::string& to, const std::string&) {
                if (!any && to != "you" && map_gnode(to, viewer)) any = true;
            });
            if (any) link(std::string("mine:") + k, "yours");
        }
    } else if (type == "mine") {
        if (!map_gnode(id, viewer)) return out;
        link("you", "yours");
        map_mine_links(rest, viewer, link);
    } else if (type == "feed") {
        if (!map_gnode(id, viewer)) return out;
        link("mine:feeds", "yours");
        map_feed_posts(rest, viewer, 12, [&](const Post& p) { link("post:" + std::to_string(p.id), "posted"); return true; });
    } else if (type == "int") {
        if (!map_gnode(id, viewer)) return out; // only your own interests
        link("mine:interests", "yours");
        map_interest_links(rest, viewer, link);
    } else if (id == "root") {
        if (!viewer.empty()) link("you", "in");
        for (const auto& cu : g_store.cities_in_use()) link("city:" + cu.key, "in");
        link("sitewide", "in");
    } else if (id == "sitewide") {
        auto props = g_store.all_proposals();
        std::reverse(props.begin(), props.end());
        std::size_t n = 0;
        for (const auto& p : props) if (p.city.empty() && n++ < kMax) link("prop:" + std::to_string(p.id), "in");
        n = 0;
        for (const auto& c : g_store.all_communities()) if (c.city.empty() && n++ < kMax) link("group:" + c.slug, "in");
    } else if (type == "city") {
        link("root", "in");
        for (const auto& c : g_store.communities_in_city(rest)) link("group:" + c.slug, "in");
        for (const auto& t : g_store.city_tags(rest)) link("dept:" + rest + "|" + t, "in");
        auto props = g_store.proposals_in_city(rest);
        std::reverse(props.begin(), props.end());
        for (std::size_t i = 0; i < props.size() && i < kMax; ++i)
            if (props[i].tags.empty()) link("prop:" + std::to_string(props[i].id), "in");
        for (const auto& u : g_store.people_listed_in_city(rest)) link("user:" + u, "lives");
    } else if (type == "group") {
        auto c = g_store.community(rest);
        if (!c) return out;
        link(c->city.empty() ? std::string("sitewide") : "city:" + city_key(c->city), "in");
        for (const auto& m : g_store.members_of(rest)) if (person_groups_ok(m)) link("user:" + m, "member");
        auto posts = g_store.posts_in(rest, kMax);
        for (const auto& p : posts) if (map_post_ok(p, viewer)) link("post:" + std::to_string(p.id), "posted");
    } else if (type == "user") {
        if (!map_person_ok(rest, viewer)) return out;
        if (person_groups_ok(rest))
            for (const auto& c : g_store.communities_of(rest)) link("group:" + c.slug, "member");
        for (const auto& cu : g_store.cities_in_use())
            if (g_store.is_city_listed(cu.key, rest)) link("city:" + cu.key, "lives");
        for (const auto& p : g_store.all_proposals()) {
            if (p.author == rest) link("prop:" + std::to_string(p.id), "wrote");
            else if (person_comments_ok(rest))
                for (const auto& r : g_store.proposal_replies_of(p.id))
                    if (r.author == rest) { link("prop:" + std::to_string(p.id), "commented"); break; }
        }
        auto posts = g_store.posts_by(rest);
        std::reverse(posts.begin(), posts.end());
        std::size_t n = 0;
        for (const auto& p : posts) if (n < kMax && map_post_ok(p, viewer)) { link("post:" + std::to_string(p.id), "posted"); ++n; }
        // Kept boxes on this person. Theirs are all visible to them. Anyone
        // else's map shows a box only after that person Flows it (map_gnode
        // also applies the block and show-me gates).
        for (const auto& p : g_store.pains_of(rest)) {
            if (!p.kept) continue;
            if (rest != viewer && !p.flowed) continue;
            link("pain:" + std::to_string(p.id), "noticed");
        }
        // Friends: you always see your own; anyone else's only when both agreed.
        for (const auto& f : g_store.friends_of(rest)) {
            const bool mine = rest == viewer || f == viewer;
            if (mine || (g_store.map_prefs_of(rest).friends && g_store.map_prefs_of(f).friends)) link("user:" + f, "friends");
        }
    } else if (type == "dept") {
        auto bar = rest.find('|');
        if (bar == std::string::npos) return out;
        const std::string key = rest.substr(0, bar), tag = rest.substr(bar + 1);
        link("city:" + key, "in");
        for (const auto& p : g_store.proposals_in_city(key))
            if (std::find(p.tags.begin(), p.tags.end(), tag) != p.tags.end()) link("prop:" + std::to_string(p.id), "touches");
    } else if (type == "prop") {
        auto n = map_gnode(id, viewer);
        if (!n) return out;
        auto p = g_store.proposal_by_id(std::stoull(rest));
        if (!p) return out;
        if (p->city.empty()) link("sitewide", "in");
        prop_links(*p);
    } else if (type == "pain") {
        if (!map_gnode(id, viewer)) return out; // private, hidden, or missing
        auto p = g_store.pain_by_id(std::stoull(rest));
        if (!p) return out;
        link("user:" + p->owner, "noticed");
        // Departments, proposals and groups stay on the owner's own view.
        if (viewer == p->owner) {
            const auto m = pain_matches(*p);
            for (const auto& [key, tag] : m.depts) link("dept:" + key + "|" + tag, "could-act");
            for (const auto& pr : m.props) link("prop:" + std::to_string(pr.id), "about");
            for (const auto& c : m.groups) link("group:" + c.slug, "working-on");
        }
        // Same subject, only for someone who Flowed it themselves. A person
        // who can see the box but did not Flow it gets no match edges.
        // Hidden people fail map_gnode and are skipped. No edge carries a count.
        if (p->kept && p->flowed && viewer_flowed_subject(viewer, p->phrase)) {
            std::set<std::string> owners;
            owners.insert(p->owner);
            std::size_t n = 0;
            for (const auto& q : g_store.flowed_with_subject(p->phrase)) {
                if (n >= 8) break;
                if (!owners.insert(q.owner).second) continue;
                const auto before = out.size();
                link("pain:" + std::to_string(q.id), "same subject");
                if (out.size() > before) ++n;
            }
        }
    } else if (type == "post") {
        auto n = map_gnode(id, viewer);
        if (!n) return out;
        auto p = g_store.post_by_id(std::stoull(rest));
        if (!p) return out;
        link("user:" + p->author, "posted");
        if (!p->community.empty()) link("group:" + p->community, "posted");
        for (const auto& g : p->extra_groups) link("group:" + g, "posted");
        if (p->repost_of_id) link("post:" + std::to_string(p->repost_of_id), "shared");
    }
    return out;
}

// ---- How relevant a box is to the person looking (Brandon 2026-11-06) ----
// 1 = something you are in or did (green), about .5 = near you (yellow),
// 0 = far from you (red). Built only from the viewer's own involvement, and
// only shown to that viewer: their groups, their cities (a group there, or
// listed there), their friends, proposals they wrote, voted on, reacted to or
// commented on (and those proposals' departments), and the words of their
// kept problems and wants.
struct MapCtx {
    std::string me;
    std::set<std::string> groups, cities, friends, depts; // depts as "<key>|<tag>"
    std::set<std::uint64_t> props;
    std::vector<std::string> keywords;
};
MapCtx map_ctx(const std::string& me) {
    MapCtx c;
    c.me = me;
    if (me.empty()) return c;
    for (const auto& g : g_store.communities_of(me)) {
        c.groups.insert(g.slug);
        const auto k = city_key(g.city);
        if (!k.empty()) c.cities.insert(k);
    }
    for (const auto& cu : g_store.cities_in_use()) if (g_store.is_city_listed(cu.key, me)) c.cities.insert(cu.key);
    for (const auto& f : g_store.friends_of(me)) c.friends.insert(f);
    for (const auto& p : g_store.all_proposals()) {
        bool mine = p.author == me || g_store.proposal_vote_of(p.id, me).has_value() ||
                    g_store.ballot_value_of(p.id, me).has_value() || g_store.proposal_reaction_of(p.id, me).has_value();
        if (!mine) for (const auto& r : g_store.proposal_replies_of(p.id)) if (r.author == me) { mine = true; break; }
        if (!mine) continue;
        c.props.insert(p.id);
        for (const auto& t : p.tags) c.depts.insert(p.city + "|" + t);
    }
    for (const auto& pp : g_store.pains_of(me))
        if (pp.kept) for (const auto& k : pain_keywords(pp.phrase)) c.keywords.push_back(k);
    return c;
}

void map_mine_links(const std::string& kind, const std::string& viewer,
                    const std::function<void(const std::string&, const std::string&)>& link) {
    if (viewer.empty()) return;
    if (kind == "friends") {
        for (const auto& f : g_store.friends_of(viewer)) link("user:" + f, "friends");
    } else if (kind == "groups") {
        for (const auto& c : g_store.communities_of(viewer)) link("group:" + c.slug, "member");
    } else if (kind == "places") {
        // your cities (a group of yours there, or you listed yourself), then the whole site as its own branch
        for (const auto& k : map_ctx(viewer).cities) link("city:" + k, "yours");
        link("root", "in");
    } else if (kind == "politics") {
        // proposals you wrote, voted on, reacted to or commented on: the same set the relevance colours use
        const auto ctx = map_ctx(viewer);
        for (auto it = ctx.props.rbegin(); it != ctx.props.rend(); ++it) link("prop:" + std::to_string(*it), "yours");
    } else if (kind == "problems") {
        for (const auto& p : g_store.pains_of(viewer)) if (p.kept) link("pain:" + std::to_string(p.id), "noticed");
    } else if (kind == "feeds") {
        // a feed with nothing you may see in it is left off (Drawings is never one of them, Brandon 2026-11-11)
        for (const char* f : {"friends", "groups", "anybody-near", "mine"}) {
            bool any = false;
            map_feed_posts(f, viewer, 1, [&](const Post&) { any = true; return false; });
            if (any) link(std::string("feed:") + f, "yours");
        }
    } else if (kind == "interests") {
        for (const auto& i : map_interests_of(viewer)) link("int:" + i.key, "yours");
    }
    // "events": nothing exists until M1.4, so the box stays empty (the ring still shows it, "None yet")
}

void map_feed_posts(const std::string& feed, const std::string& viewer, std::size_t limit,
                    const std::function<bool(const Post&)>& emit) {
    if (viewer.empty() || limit == 0) return;
    std::set<std::string> mygroups, near;
    if (feed == "groups") for (const auto& c : g_store.communities_of(viewer)) mygroups.insert(c.slug);
    if (feed == "anybody-near")
        for (const auto& k : map_ctx(viewer).cities)
            for (const auto& u : g_store.people_listed_in_city(k)) near.insert(u);
    auto wanted = [&](const Post& p) {
        if (feed == "mine") return p.author == viewer;
        if (p.author == viewer) return false;
        if (feed == "friends") return g_store.are_friends(viewer, p.author);
        if (feed == "groups") {
            if (mygroups.count(p.community)) return true;
            for (const auto& g : p.extra_groups) if (mygroups.count(g)) return true;
            return false;
        }
        if (feed == "anybody-near") return p.audience == PostAudience::Anybody && near.count(p.author) > 0;
        return false;
    };
    const auto posts = feed == "mine" ? g_store.posts_by(viewer) : g_store.all_posts(); // both oldest first
    std::size_t n = 0;
    for (auto it = posts.rbegin(); it != posts.rend() && n < limit; ++it) {
        if (!wanted(*it) || !map_post_ok(*it, viewer)) continue;
        ++n;
        if (!emit(*it)) return;
    }
}

void map_interest_links(const std::string& key, const std::string& viewer,
                        const std::function<void(const std::string&, const std::string&)>& link) {
    std::string label;
    for (const auto& i : map_interests_of(viewer)) if (i.key == key) label = i.label;
    if (label.empty()) return;
    constexpr std::size_t kMax = 40, kPeople = 8;
    std::size_t n = 0;
    for (const auto& c : g_store.all_communities())
        if (n < kMax && map_interest_matches(label, map_folded_words(c.name + " " + c.about + " " + c.subcategory))) {
            link("group:" + c.slug, "interest"); ++n;
        }
    auto props = g_store.all_proposals();
    n = 0;
    for (auto it = props.rbegin(); it != props.rend() && n < kMax; ++it)
        if (map_interest_matches(label, map_folded_words(it->title + " " + it->body))) {
            link("prop:" + std::to_string(it->id), "interest"); ++n;
        }
    // People (SPEC 3.7): only those who list the same interest, are on the map (map_person_ok, via link -> map_gnode),
    // findable by name, and listed in one of your cities. At most 8, newest-listed order not implied; never a count.
    std::set<std::string> seen;
    n = 0;
    for (const auto& k : map_ctx(viewer).cities) {
        for (const auto& u : g_store.people_listed_in_city(k)) {
            if (n >= kPeople) return;
            if (u == viewer || !seen.insert(u).second) continue;
            if (g_store.prefs_of(u).findability != Findability::Typeahead) continue;
            if (!map_person_ok(u, viewer)) continue;
            bool same = false;
            for (const auto& i : map_interests_of(u))
                if (map_interest_matches(label, map_folded_words(i.label)) && map_interest_matches(i.label, map_folded_words(label))) {
                    same = true; break;
                }
            if (!same) continue;
            link("user:" + u, "interest");
            ++n;
        }
    }
}

// The second line on the "you" box and your kind boxes (Grok Bot's mockup 1, Brandon 2026-11-11). Built only from the
// viewer's own connections, the same ones the kind box opens onto; nothing about anyone else's lists.
void map_add_sub(GNode& n, const std::string& viewer) {
    if (viewer.empty()) return;
    auto plural = [](std::size_t k, const char* one, const char* many) {
        return std::to_string(k) + " " + (k == 1 ? one : many);
    };
    if (n.kind == "you") { n.sub = "YOU"; return; }
    if (n.kind != "mine") return;
    const std::string kind = n.id.substr(5);
    if (kind == "friends") {
        n.sub = plural(g_store.friends_of(viewer).size(), "person", "people");
    } else if (kind == "groups") {
        const auto gs = g_store.communities_of(viewer);
        if (!gs.empty()) n.sub = gs.front().name + (gs.size() > 1 ? ", and " + std::to_string(gs.size() - 1) + " more" : "");
    } else if (kind == "places") {
        const auto ctx = map_ctx(viewer);
        std::string first;
        if (!ctx.cities.empty())
            for (const auto& cu : g_store.cities_in_use()) if (cu.key == *ctx.cities.begin()) first = cu.name;
        n.sub = first.empty() ? "the whole site" : first + ", then the rest of the site";
    } else if (kind == "politics") {
        n.sub = plural(map_ctx(viewer).props.size(), "proposal you touched", "proposals you touched");
    } else if (kind == "problems") {
        std::size_t k = 0;
        for (const auto& p : g_store.pains_of(viewer)) if (p.kept) ++k;
        n.sub = plural(k, "kept", "kept");
    } else if (kind == "interests") {
        std::string s;
        for (const auto& i : map_interests_of(viewer)) s += (s.empty() ? "" : ", ") + i.label;
        n.sub = s;
    } else if (kind == "feeds") {
        // the feeds that have something for you, by name (never Drawings)
        std::string s;
        for (const auto& [f, name] : std::initializer_list<std::pair<const char*, const char*>>{
                 {"friends", "Friends"}, {"groups", "Groups"}, {"anybody-near", "Near you"}, {"mine", "Yours"}}) {
            bool any = false;
            map_feed_posts(f, viewer, 1, [&](const Post&) { any = true; return false; });
            if (any) s += (s.empty() ? "" : " \xC2\xB7 ") + std::string(name);
        }
        n.sub = s;
    } else if (kind == "events") {
        n.sub = "None yet"; // always on the ring, even empty (Brandon 2026-11-11 4:30 PM decision 1)
    }
}
// -1 = not scored (the MAINstream itself).
double map_relevance(const GNode& n, const MapCtx& c) {
    auto colon = n.id.find(':');
    const std::string rest = colon == std::string::npos ? "" : n.id.substr(colon + 1);
    auto words = [&](const std::string& text) { return pain_words_overlap(c.keywords, text) ? 0.2 : 0.0; };
    auto clamp = [](double v) { return v < 0 ? 0.0 : v > 1 ? 1.0 : v; };
    if (n.kind == "root" || n.kind == "you") return -1; // the base boxes are not scored
    if (n.kind == "mine") return 1;                      // your own kinds of connection
    if (n.kind == "pain") {
        std::uint64_t id = 0;
        try { id = std::stoull(rest); } catch (...) { return 0.35; }
        auto p = g_store.pain_by_id(id);
        if (p && p->owner == c.me) return 1; // their own box
        return 0.35;                         // someone else's flowed subject
    }
    if (n.kind == "interest" || n.kind == "feed") return 1; // your own interests and feeds (viewer-only boxes)
    if (n.kind == "sitewide") return 0.5;
    if (n.kind == "city") return c.cities.count(rest) ? 1.0 : 0.15;
    if (n.kind == "person") {
        if (rest == c.me) return 1;
        if (c.friends.count(rest)) return 0.9;
        for (const auto& g : c.groups) if (g_store.is_member(g, rest)) return 0.75;
        for (const auto& k : c.cities) if (g_store.is_city_listed(k, rest) || g_store.in_city(k, rest)) return 0.6;
        return 0.2;
    }
    if (n.kind == "group") {
        if (c.groups.count(rest)) return 1;
        auto g = g_store.community(rest);
        if (!g) return 0.2;
        const double w = words(g->name + " " + g->about + " " + g->subcategory);
        if (!g->city.empty() && c.cities.count(city_key(g->city))) return clamp(0.65 + w);
        return clamp((g->city.empty() ? 0.35 : 0.15) + w);
    }
    if (n.kind == "dept") {
        if (c.depts.count(rest)) return 0.95;
        auto bar = rest.find('|');
        const std::string key = bar == std::string::npos ? rest : rest.substr(0, bar);
        const std::string tag = bar == std::string::npos ? "" : rest.substr(bar + 1);
        return clamp((c.cities.count(key) ? 0.65 : 0.15) + words(tag));
    }
    if (n.kind == "prop") {
        std::uint64_t id = 0;
        try { id = std::stoull(rest); } catch (...) { return 0.2; }
        if (c.props.count(id)) return 1;
        auto p = g_store.proposal_by_id(id);
        if (!p) return 0.2;
        const double w = words(p->title);
        if (!p->city.empty() && c.cities.count(p->city)) return clamp(0.6 + w);
        return clamp((p->city.empty() ? 0.4 : 0.15) + w);
    }
    if (n.kind == "post") {
        std::uint64_t id = 0;
        try { id = std::stoull(rest); } catch (...) { return 0.3; }
        auto p = g_store.post_by_id(id);
        if (!p) return 0.3;
        if (p->author == c.me) return 1;
        if (c.friends.count(p->author)) return 0.85;
        if (!p->community.empty() && c.groups.count(p->community)) return 0.75;
        for (const auto& g : p->extra_groups) if (c.groups.count(g)) return 0.75;
        return clamp(0.3 + words(p->text));
    }
    return 0.3;
}

std::string map_node_json(const GNode& n, double rel = -1) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%.2f", rel);
    return "{\"id\":\"" + json_escape(n.id) + "\",\"label\":\"" + json_escape(n.label) + "\",\"kind\":\"" +
           json_escape(n.kind) + "\",\"page\":\"" + json_escape(n.page) + "\"" +
           (n.sub.empty() ? std::string() : ",\"sub\":\"" + json_escape(n.sub) + "\"") +
           (rel < 0 ? std::string() : std::string(",\"rel\":") + buf) + "}";
}

// ---- City map view (city proposals, slice 6) ----------------------------
// A picture of one city, drawn by the server as SVG (no framework): People
// here | Groups | the city | Departments and councils | Proposals, with lines
// for who is in which group, which proposal touches which department, and how
// listed people are tied to proposals (wrote, commented; never reacted, which is
// only ever shown as a count). Only what
// the city page already shows: people appear only if they chose to be listed,
// and people blocked either way are left out, same as "People here".

// First `max` characters (UTF-8 code points) of s, with "..." when cut.
std::string map_label(const std::string& s, std::size_t max) {
    std::size_t chars = 0, i = 0;
    while (i < s.size()) {
        if (chars == max) return s.substr(0, i) + "...";
        unsigned char c = static_cast<unsigned char>(s[i]);
        std::size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
        i += len;
        ++chars;
    }
    return s;
}

struct MapNode {
    std::string id;     // "c", "g0", "u0", "d0", "p0", or "more-..."
    std::string label;
    std::string title;  // full text for the hover tooltip
    std::string href;   // empty: not a link
    std::string kind;   // city, group, person, dept, prop, more
    int col = 0;
    int row = 0;
};
struct MapEdge { std::string a, b, kind; }; // kind: in, member, touches, wrote, commented, reacted

std::string city_map_svg(const std::string& key, const std::string& name, const std::string& viewer) {
    constexpr std::size_t kCap = 20; // boxes per column before "+N more"
    std::vector<MapNode> nodes;
    std::vector<MapEdge> edges;
    std::map<std::string, std::string> group_id, person_id, dept_id; // value -> node id
    std::map<std::uint64_t, std::string> prop_id;
    int rows[5] = {0, 0, 0, 0, 0};
    auto add = [&](int col, const std::string& id, const std::string& label, const std::string& title,
                   const std::string& href, const std::string& kind) {
        MapNode n{id, map_label(label, 22), title, href, kind, col, rows[col]++};
        nodes.push_back(n);
    };
    auto more = [&](int col, std::size_t extra, const std::string& href) {
        if (extra == 0) return;
        add(col, "more-" + std::to_string(col), "+" + std::to_string(extra) + " more", "", href, "more");
    };
    const std::string city_href_str = "/city/" + url_encode(key);

    add(2, "c", name, name, city_href_str, "city");

    auto groups = g_store.communities_in_city(key);
    for (std::size_t i = 0; i < groups.size() && i < kCap; ++i) {
        const auto& c = groups[i];
        group_id[c.slug] = "g" + std::to_string(i);
        add(1, group_id[c.slug], c.name, c.name + " (/" + c.slug + ")", "/c/" + url_encode(c.slug), "group");
        edges.push_back({"c", group_id[c.slug], "in"});
    }
    more(1, groups.size() > kCap ? groups.size() - kCap : 0, city_href_str);

    std::vector<std::string> people;
    for (const auto& n : g_store.people_listed_in_city(key))
        if (map_person_ok(n, viewer)) people.push_back(n); // blocks, "Show me on the map", profile visibility
    for (std::size_t i = 0; i < people.size() && i < kCap; ++i) {
        person_id[people[i]] = "u" + std::to_string(i);
        add(0, person_id[people[i]], people[i], people[i], "/u/" + url_encode(people[i]), "person");
        if (people[i] != viewer && !g_store.map_prefs_of(people[i]).groups) continue; // "Show the groups I'm in"
        for (const auto& [slug, gid] : group_id)
            if (g_store.is_member(slug, people[i])) edges.push_back({person_id[people[i]], gid, "member"});
    }
    more(0, people.size() > kCap ? people.size() - kCap : 0, city_href_str);

    auto tags = g_store.city_tags(key);
    for (std::size_t i = 0; i < tags.size() && i < kCap; ++i) {
        dept_id[tags[i]] = "d" + std::to_string(i);
        add(3, dept_id[tags[i]], tags[i], tags[i], "", "dept");
        edges.push_back({"c", dept_id[tags[i]], "in"});
    }
    more(3, tags.size() > kCap ? tags.size() - kCap : 0, city_href_str);

    auto props = g_store.proposals_in_city(key);
    std::reverse(props.begin(), props.end()); // newest first, so the cap keeps recent ones
    const std::string props_href = "/proposals?city=" + url_encode(key);
    for (std::size_t i = 0; i < props.size() && i < kCap; ++i) {
        const auto& p = props[i];
        const std::string pid = "p" + std::to_string(i);
        prop_id[p.id] = pid;
        add(4, pid, p.title, "Proposal " + std::to_string(p.id) + ": " + p.title, props_href, "prop");
        bool tagged = false;
        for (const auto& t : p.tags) {
            auto d = dept_id.find(t);
            if (d != dept_id.end()) { edges.push_back({d->second, pid, "touches"}); tagged = true; }
        }
        if (!tagged) edges.push_back({"c", pid, "in"});
        // One line per listed person: wrote beats commented. Reactions are NOT
        // drawn: proposal pages show them only as counts, so who reacted is not
        // public, and the map must not make it so.
        std::map<std::string, std::string> role;
        for (const auto& r : g_store.proposal_replies_of(p.id))
            if (person_id.count(r.author) && (r.author == viewer || g_store.map_prefs_of(r.author).comments))
                role[r.author] = "commented";
        if (person_id.count(p.author)) role[p.author] = "wrote";
        for (const auto& [who, kind] : role) edges.push_back({person_id[who], pid, kind});
    }
    more(4, props.size() > kCap ? props.size() - kCap : 0, props_href);

    // Geometry: five columns in a 1100-wide viewBox; the city sits mid-height.
    constexpr int kColX[5] = {20, 240, 470, 700, 910};
    constexpr int kBoxW[5] = {170, 180, 160, 170, 170};
    constexpr int kRowH = 46, kBoxH = 34, kTop = 56;
    int max_rows = 1;
    for (int r : rows) max_rows = std::max(max_rows, r);
    const int height = std::max(300, kTop + max_rows * kRowH + 20);
    auto node_y = [&](const MapNode& n) {
        const int col_h = rows[n.col] * kRowH;
        const int start = kTop + (max_rows * kRowH - col_h) / 2; // centre each column
        return start + n.row * kRowH;
    };
    std::map<std::string, const MapNode*> by_id;
    for (const auto& n : nodes) by_id[n.id] = &n;

    std::ostringstream os;
    os << "<div class=\"cmapwrap\"><svg class=\"cmap\" viewBox=\"0 0 1100 " << height
       << "\" role=\"img\" aria-label=\"Map of " << html_escape(name) << "\" xmlns=\"http://www.w3.org/2000/svg\">";
    const char* heads[5] = {"People here", "Groups", "City", "Departments and councils", "Proposals"};
    for (int c = 0; c < 5; ++c)
        os << "<text class=\"cmaph\" x=\"" << kColX[c] + kBoxW[c] / 2 << "\" y=\"28\">" << heads[c] << "</text>";
    os << "<g class=\"cmapedges\">";
    for (const auto& e : edges) {
        auto ia = by_id.find(e.a), ib = by_id.find(e.b);
        if (ia == by_id.end() || ib == by_id.end()) continue;
        const MapNode* a = ia->second; const MapNode* b = ib->second;
        if (a->col > b->col) std::swap(a, b);
        const int x1 = kColX[a->col] + kBoxW[a->col], y1 = node_y(*a) + kBoxH / 2;
        const int x2 = kColX[b->col], y2 = node_y(*b) + kBoxH / 2;
        const int dx = std::max(30, (x2 - x1) / 2);
        os << "<path class=\"ce ce-" << e.kind << "\" data-a=\"" << e.a << "\" data-b=\"" << e.b << "\" d=\"M" << x1 << " " << y1
           << " C" << x1 + dx << " " << y1 << " " << x2 - dx << " " << y2 << " " << x2 << " " << y2 << "\"/>";
    }
    os << "</g>";
    for (const auto& n : nodes) {
        const int x = kColX[n.col], y = node_y(n), w = kBoxW[n.col];
        if (!n.href.empty()) os << "<a href=\"" << html_escape(n.href) << "\">";
        os << "<g class=\"cn cn-" << n.kind << "\" data-n=\"" << n.id << "\">";
        if (!n.title.empty()) os << "<title>" << html_escape(n.title) << "</title>";
        os << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w << "\" height=\"" << kBoxH << "\" rx=\"8\"/>"
           << "<text x=\"" << x + w / 2 << "\" y=\"" << y + kBoxH / 2 + 5 << "\">" << html_escape(n.label) << "</text></g>";
        if (!n.href.empty()) os << "</a>";
    }
    if (rows[0] == 0)
        os << "<text class=\"cmapnote\" x=\"" << kColX[0] + kBoxW[0] / 2 << "\" y=\"" << kTop + 20 << "\">Nobody listed yet</text>";
    os << "</svg></div>";
    return os.str();
}

// The root of the map (Brandon 2026-11-06: "a mAInstream Media Map and then things
// branch off from there"): MAINstream -> each city's Current -> a few of its groups
// and proposals, plus a Sitewide branch for proposals with no city. Clicking a city
// opens its own map (city_map_svg). Only groups and proposals, which every signed-in
// member can already see; no people at this level.
std::string main_stream_svg() {
    constexpr std::size_t kCities = 30, kLeaves = 3, kSitewide = 5;
    struct Leaf { std::string label, title, href, kind; };
    struct Branch { std::string label, title, href, kind; std::vector<Leaf> leaves; };
    std::vector<Branch> branches;
    auto cities = g_store.cities_in_use();
    for (std::size_t i = 0; i < cities.size() && i < kCities; ++i) {
        const auto& cu = cities[i];
        const std::string map_href = "/city/" + url_encode(cu.key) + "/map";
        Branch b{cu.name, cu.name + "'s Current", map_href, "city", {}};
        auto groups = g_store.communities_in_city(cu.key);
        for (std::size_t g = 0; g < groups.size() && g < kLeaves; ++g)
            b.leaves.push_back({groups[g].name, "Group: " + groups[g].name, "/c/" + url_encode(groups[g].slug), "group"});
        auto props = g_store.proposals_in_city(cu.key);
        std::reverse(props.begin(), props.end());
        for (std::size_t p = 0; p < props.size() && p < kLeaves; ++p)
            b.leaves.push_back({props[p].title, "Proposal: " + props[p].title, "/proposals?city=" + url_encode(cu.key), "prop"});
        const std::size_t extra = (groups.size() > kLeaves ? groups.size() - kLeaves : 0) +
                                  (props.size() > kLeaves ? props.size() - kLeaves : 0);
        if (extra) b.leaves.push_back({"+" + std::to_string(extra) + " more in " + cu.name, "", map_href, "more"});
        branches.push_back(std::move(b));
    }
    if (cities.size() > kCities)
        branches.push_back({"+" + std::to_string(cities.size() - kCities) + " more cities", "", "/communities", "more", {}});
    {
        Branch s{"Sitewide", "Proposals not about any one city", "/proposals", "dept", {}};
        std::vector<Proposal> sitewide;
        for (const auto& p : g_store.all_proposals()) if (p.city.empty() && !p.is_office) sitewide.push_back(p);
        std::reverse(sitewide.begin(), sitewide.end());
        for (std::size_t i = 0; i < sitewide.size() && i < kSitewide; ++i)
            s.leaves.push_back({sitewide[i].title, "Proposal: " + sitewide[i].title, "/proposals", "prop"});
        if (sitewide.size() > kSitewide)
            s.leaves.push_back({"+" + std::to_string(sitewide.size() - kSitewide) + " more", "", "/proposals", "more"});
        branches.push_back(std::move(s));
    }

    // Geometry: root | branches | leaves. Each branch gets as many rows as it has
    // leaves (at least one) and sits centred on them.
    constexpr int kX[3] = {20, 250, 540}, kW[3] = {170, 210, 300};
    constexpr int kRowH = 42, kBoxH = 32, kTop = 52;
    int total_rows = 0;
    for (const auto& b : branches) total_rows += std::max<int>(1, static_cast<int>(b.leaves.size()));
    const int height = std::max(260, kTop + total_rows * kRowH + 20);
    std::ostringstream edges, boxes;
    auto box = [&](int col, int y, const std::string& id, const std::string& label, const std::string& title,
                   const std::string& href, const std::string& kind) {
        const int x = kX[col], w = kW[col];
        if (!href.empty()) boxes << "<a href=\"" << html_escape(href) << "\">";
        boxes << "<g class=\"cn cn-" << kind << "\" data-n=\"" << id << "\">";
        if (!title.empty()) boxes << "<title>" << html_escape(title) << "</title>";
        boxes << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w << "\" height=\"" << kBoxH << "\" rx=\"8\"/>"
              << "<text x=\"" << x + w / 2 << "\" y=\"" << y + kBoxH / 2 + 5 << "\">"
              << html_escape(map_label(label, col == 2 ? 38 : 26)) << "</text></g>";
        if (!href.empty()) boxes << "</a>";
    };
    auto edge = [&](int c1, int y1, int c2, int y2, const std::string& a, const std::string& b, const std::string& kind) {
        const int x1 = kX[c1] + kW[c1], x2 = kX[c2], dx = (x2 - x1) / 2;
        y1 += kBoxH / 2; y2 += kBoxH / 2;
        edges << "<path class=\"ce ce-" << kind << "\" data-a=\"" << a << "\" data-b=\"" << b << "\" d=\"M" << x1 << " " << y1
              << " C" << x1 + dx << " " << y1 << " " << x2 - dx << " " << y2 << " " << x2 << " " << y2 << "\"/>";
    };
    const int root_y = kTop + (total_rows * kRowH) / 2 - kRowH / 2;
    int row = 0;
    for (std::size_t i = 0; i < branches.size(); ++i) {
        const auto& b = branches[i];
        const int n = std::max<int>(1, static_cast<int>(b.leaves.size()));
        const int by = kTop + row * kRowH + (n - 1) * kRowH / 2;
        const std::string bid = "b" + std::to_string(i);
        box(1, by, bid, b.label, b.title, b.href, b.kind);
        edge(0, root_y, 1, by, "root", bid, "in");
        for (std::size_t j = 0; j < b.leaves.size(); ++j) {
            const auto& l = b.leaves[j];
            const int ly = kTop + (row + static_cast<int>(j)) * kRowH;
            const std::string lid = bid + "-" + std::to_string(j);
            box(2, ly, lid, l.label, l.title, l.href, l.kind);
            edge(1, by, 2, ly, bid, lid, l.kind == "prop" ? "touches" : "in");
        }
        row += n;
    }
    box(0, root_y, "root", "MAINstream", "MAINstream", "", "city");
    std::ostringstream os;
    os << "<div class=\"cmapwrap\"><svg class=\"cmap\" viewBox=\"0 0 860 " << height
       << "\" role=\"img\" aria-label=\"MAINstream map\" xmlns=\"http://www.w3.org/2000/svg\">"
       << "<text class=\"cmaph\" x=\"" << kX[1] + kW[1] / 2 << "\" y=\"26\">Currents</text>"
       << "<text class=\"cmaph\" x=\"" << kX[2] + kW[2] / 2 << "\" y=\"26\">Groups and proposals</text>"
       << "<g class=\"cmapedges\">" << edges.str() << "</g>" << boxes.str() << "</svg></div>";
    return os.str();
}

// Styles and the hover script for the map: point at a box and its lines light up.
const char* kCityMapStyle =
    "<style>"
    ".cmapwrap{overflow-x:auto;border:1px solid var(--rule);border-radius:var(--rmd,10px);background:var(--panel)}"
    ".cmap{display:block;min-width:820px;width:100%;height:auto;font-family:var(--body,inherit)}"
    ".cmaph{font-size:13px;font-weight:600;fill:var(--stone);text-anchor:middle}"
    ".cmapnote{font-size:12px;fill:var(--stone);text-anchor:middle;font-style:italic}"
    ".cn rect{fill:var(--paper);stroke:var(--rule);stroke-width:1.5}"
    ".cn text{font-size:13px;fill:var(--ink);text-anchor:middle;pointer-events:none}"
    ".cn-city rect{fill:var(--accent);stroke:var(--accent)}.cn-city text{fill:var(--paper);font-weight:700;font-size:15px}"
    ".cn-group rect{stroke:var(--accent)}.cn-person rect{stroke:var(--ok)}.cn-dept rect{stroke:#8a5cc2}"
    ".cn-prop rect{stroke:var(--warn)}.cn-more rect{stroke-dasharray:4 3}"
    ".cn{cursor:pointer}.cn:hover rect,.cn:focus rect{stroke-width:3}"
    ".ce{fill:none;stroke:var(--rule);stroke-width:1.5;opacity:.75;transition:opacity .15s}"
    ".ce-member{stroke:var(--ok)}.ce-touches{stroke:#8a5cc2}.ce-wrote{stroke:var(--warn);stroke-width:2}"
    ".ce-commented{stroke:var(--warn);stroke-dasharray:6 4}.ce-reacted{stroke:var(--warn);stroke-dasharray:2 4}"
    ".cmap.focus .ce{opacity:.08}.cmap.focus .ce.hi{opacity:1;stroke-width:3}"
    ".cmaplegend{display:flex;flex-wrap:wrap;gap:.4rem 1rem;margin:.6rem 0 0;font-size:.85rem}"
    ".cmaplegend span::before{content:'';display:inline-block;width:22px;height:0;margin-right:.35rem;vertical-align:middle;border-top:3px solid var(--rule)}"
    ".lg-member::before{border-color:var(--ok)!important}.lg-touches::before{border-color:#8a5cc2!important}"
    ".lg-wrote::before{border-color:var(--warn)!important}.lg-commented::before{border-top-style:dashed!important;border-color:var(--warn)!important}"
    ".lg-reacted::before{border-top-style:dotted!important;border-color:var(--warn)!important}"
    "</style>"
    "<script>(function(){var s=document.querySelector('.cmap');if(!s)return;"
    "function on(id){s.classList.add('focus');s.querySelectorAll('.ce').forEach(function(e){"
    "if(e.getAttribute('data-a')===id||e.getAttribute('data-b')===id)e.classList.add('hi');});}"
    "function off(){s.classList.remove('focus');s.querySelectorAll('.ce.hi').forEach(function(e){e.classList.remove('hi');});}"
    "s.querySelectorAll('.cn').forEach(function(n){var id=n.getAttribute('data-n');"
    "var t=n.parentNode&&n.parentNode.tagName.toLowerCase()==='a'?n.parentNode:n;"
    "t.addEventListener('mouseenter',function(){on(id);});t.addEventListener('mouseleave',off);"
    "t.addEventListener('focus',function(){on(id);});t.addEventListener('blur',off);});})();</script>";

// Suggested download name only (the browser lets the user rename); no
// original filename is stored, so this is derived from the mime that
// ingest_file actually assigned after content sniffing.
std::string attachment_filename_for_mime(const std::string& mime) {
    if (mime == "image/png") return "attachment.png";
    if (mime == "application/pdf") return "attachment.pdf";
    if (mime.rfind("text/plain", 0) == 0) return "attachment.txt";
    if (const ProgramKindInfo* k = program_kind_by_mime(mime)) return std::string("program") + k->extension;
    return "attachment";
}

// ---- Programs in mail (archives, installers, executables) ----
// Operators send every kind up to the server ceiling; anyone else only
// what an operator granted them on /admin/accounts (FileStore ProgramGrant).
// The ceiling is the upload protocol's maximum: the whole upload is held in
// memory while it's checked and stored, and this server has 0.5 GB.
std::uint64_t program_ceiling_bytes() { return g_mail_caps.protocol_max_bytes; }

std::optional<ProgramPolicy> program_policy_for(const std::string& username) {
    if (username.empty()) return std::nullopt;
    if (is_operator(username)) return ProgramPolicy{kAllProgramKinds, program_ceiling_bytes()};
    auto g = g_store.program_grant(username);
    if (!g || g->kinds == 0) return std::nullopt;
    return ProgramPolicy{g->kinds & kAllProgramKinds, std::min<std::uint64_t>(g->max_bytes, program_ceiling_bytes())};
}

std::string program_kinds_text(std::uint32_t kinds) {
    std::string out;
    for (const auto& k : program_kinds()) {
        if ((kinds & static_cast<std::uint32_t>(k.kind)) == 0) continue;
        if (!out.empty()) out += ", ";
        out += k.slug;
    }
    return out.empty() ? "none" : out;
}

std::string fmt_size(std::uint64_t bytes) {
    char b[32];
    if (bytes >= 1024ull * 1024) std::snprintf(b, sizeof b, "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    else if (bytes >= 1024) std::snprintf(b, sizeof b, "%.0f KB", static_cast<double>(bytes) / 1024.0);
    else std::snprintf(b, sizeof b, "%llu bytes", static_cast<unsigned long long>(bytes));
    return b;
}

// A program's download name: the sender's file name without any folder,
// only [A-Za-z0-9 ._()+-], at most 80 characters, ending in the extension
// of the kind the content actually is (a disguised .exe still saves as
// .exe). Safe to put in a Content-Disposition header as-is.
std::string sanitize_download_name(const std::string& raw, const ProgramKindInfo& kind) {
    std::string base = raw;
    auto slash = base.find_last_of("/\\");
    if (slash != std::string::npos) base = base.substr(slash + 1);
    std::string name;
    for (char c : base) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                        c == ' ' || c == '.' || c == '_' || c == '(' || c == ')' || c == '+' || c == '-';
        if (ok) name += c;
    }
    while (!name.empty() && (name.front() == '.' || name.front() == ' ')) name.erase(0, 1);
    while (!name.empty() && (name.back() == '.' || name.back() == ' ')) name.pop_back();
    const std::string ext = kind.extension;
    auto lower = [](std::string s) { for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; };
    const bool has_ext = !ext.empty() && name.size() >= ext.size() &&
                         lower(name.substr(name.size() - ext.size())) == ext;
    if (has_ext) name.resize(name.size() - ext.size());
    if (name.size() > 80 - ext.size()) name.resize(80 - ext.size());
    if (name.empty()) name = "program";
    return name + ext;
}

// How an attachment shows in a mail bubble. A program gets a card with its
// name, size and who sent it, and a plain warning; anything else keeps the
// existing "Download attachment" link.
// `name_key` is the message's attachment_name_key; `sender_label` is how
// the bubble names the sender.
std::string mail_attachment_html(const std::string& blob_id, const std::string& mime, const std::string& name_key,
                                 const std::string& sender_label, bool mine) {
    const std::string href = "/mail/attachment/" + html_escape(blob_id);
    const ProgramKindInfo* kind = program_kind_by_mime(mime);
    if (!kind) return "<div><a href=\"" + href + "\">Download attachment</a></div>";
    const std::string name = g_store.attachment_name(name_key).value_or(attachment_filename_for_mime(mime));
    const auto meta = g_blobs.meta(blob_id);
    std::string html = "<div class=\"mailprog\"><a href=\"" + href + "\" download>"
                       "<span aria-hidden=\"true\">\xF0\x9F\x93\xA6</span> <strong>" + html_escape(name) + "</strong></a>"
                       "<span class=\"muted\"> " + html_escape(kind->label) +
                       (meta ? " \xC2\xB7 " + fmt_size(meta->size) : std::string()) + "</span>";
    if (!mine) html += "<p class=\"muted\">A program from " + html_escape(sender_label) +
                       ". Open it only if you trust them and expected it. Windows may warn that it comes from an "
                       "unknown publisher.</p>";
    return html + "</div>";
}

// Error text for a refused mail attachment, shared by 1:1 and group mail.
std::string mail_attachment_error_html(const std::string& e, const std::string& user) {
    if (e == "type") return "<p class=\"warn\">That attachment isn't a usable type. Allowed: PNG/JPEG/GIF, PDF, plain text, "
                            "macro-free Word/Excel/PowerPoint. No SVG.</p>";
    if (e == "large") {
        auto policy = program_policy_for(user);
        return "<p class=\"warn\">That attachment is too large (pictures 10&nbsp;MB, documents 25&nbsp;MB, video 50&nbsp;MB" +
               (policy ? ", programs " + fmt_size(policy->max_bytes) + " for you" : std::string()) + ").</p>";
    }
    if (e == "program") {
        auto policy = program_policy_for(user);
        if (!policy) return "<p class=\"warn\">That's a program (an archive, installer or app). Sending programs needs an "
                            "admin's permission; ask an admin if you need it.</p>";
        return "<p class=\"warn\">You can send these kinds of programs: " + html_escape(program_kinds_text(policy->kinds)) +
               ". That file is a different kind; ask an admin to add it.</p>";
    }
    if (e == "active") return "<p class=\"warn\">That document was refused: it declares a macro, embedded object, or OLE content.</p>";
    if (e == "scan") return "<p class=\"warn\">That attachment could not be stored.</p>";
    return {};
}

// The composer's file picker: what it offers, plus a line telling a
// program sender what they may send. `big_action` is the composer's
// /mail/big door: a program sender's form posts there only while a file
// over the ordinary limit is attached (second review 2026-10-27: posting
// every message there refused all mail from a small grant and put chat
// under the upload rate limit).
std::string mail_file_input_html(const std::string& user, const std::string& big_action = {}) {
    std::string accept = "image/png,image/jpeg,image/gif,application/pdf,text/plain,.docx,.xlsx,.pptx";
    auto policy = program_policy_for(user);
    // The picked file's name and size show under the composer (.mailfile): the paperclip alone gave no sign a file
    // was attached (2026-10-27).
    std::string onchange = " onchange=\"var f=this.files&amp;&amp;this.files[0],F=this.form,s=F.querySelector('.mailfile');"
                           "if(s)s.textContent=f?'Attached: '+f.name+' ('+(f.size/1048576).toFixed(1)+' MB)':'';";
    if (policy) {
        for (const auto& k : program_kinds())
            if ((policy->kinds & static_cast<std::uint32_t>(k.kind)) && k.extension[0]) accept += std::string(",") + k.extension;
        if (!big_action.empty()) {
            const std::size_t ordinary = kMaxBody - kMultipartRoom; // what fits through plain /mail with its form fields
            onchange += "if(!F.dataset.small)F.dataset.small=F.getAttribute('action');"
                        "F.setAttribute('action',f&amp;&amp;f.size&gt;" + std::to_string(ordinary) +
                        "?'" + big_action + "':F.dataset.small);";
        }
    }
    return "<input type=\"file\" name=\"attachment\" accept=\"" + accept + "\"" + onchange + "\">";
}

// The composer form's onsubmit: Send says "Sending..." and a second press does nothing, on the full mail page
// (the browser shows the upload's progress) and in the pop-up (which also counts the percent, see its XHR).
const char* const kComposerOnSubmit =
    " onsubmit=\"if(this.dataset.sent)return false;this.dataset.sent='1';"
    "var b=this.querySelector('button[type=submit]');if(b)b.textContent='Sending...';\"";
// Where the picked file's name shows (filled by the file input's onchange).
const char* const kComposerFileLine = "<p class=\"muted mailfile\" style=\"margin:.3rem 0 0\"></p>";

std::string mail_program_note_html(const std::string& user) {
    auto policy = program_policy_for(user);
    if (!policy) return {};
    return "<p class=\"muted\" style=\"margin:.2rem 0 0\">You can send programs: " +
           html_escape(program_kinds_text(policy->kinds)) + ", up to " + fmt_size(policy->max_bytes) + ".</p>";
}

// A mail send's attachment, taken out of the request: the file bytes and
// the name the browser gave it. The body is released right after (it held
// the same bytes), so a 100 MB upload is in memory twice at most, briefly.
struct MailUpload {
    std::optional<std::vector<std::uint8_t>> file;
    std::string filename;
};
MailUpload take_mail_upload(const std::string& ctype, std::string& body) {
    MailUpload up;
    if (ctype.find("multipart/form-data") == std::string::npos) return up;
    up.filename = multipart_filename(ctype, body, "attachment");
    up.file = multipart_file(ctype, body, "attachment");
    std::string().swap(body);
    return up;
}

// Ingest a mail attachment with the sender's program policy. On success
// `download_name` is the sanitized name for a program ("" otherwise); the
// caller stores it with set_attachment_name once the message exists.
// Returns the ?err= code on refusal, "" on success.
std::string ingest_mail_attachment(const std::string& user, const MailUpload& up, std::optional<std::string>& blob_id,
                                   std::optional<std::string>& mime, std::string& download_name) {
    auto policy = program_policy_for(user);
    auto r = ingest_file(g_blobs, g_mail_scanner, g_mail_caps, *up.file, policy ? &*policy : nullptr);
    if (r.error != FileTransferError::None || !r.blob_id) {
        switch (r.error) {
            case FileTransferError::TooLarge: return "large";
            case FileTransferError::ActiveContentBlocked: return "active";
            case FileTransferError::FailedScan: return "scan";
            case FileTransferError::ProgramNotAllowed: return "program";
            default: return "type";
        }
    }
    blob_id = r.blob_id;
    mime = r.stored_mime_type;
    if (const ProgramKindInfo* kind = program_kind_by_mime(mime.value_or("")))
        download_name = sanitize_download_name(up.filename, *kind);
    return {};
}

// The download of a mail attachment the caller already authorized
// (`name_key`: that message's attachment_name_key). With
// LIQUIDGOV_BLOB_ACCEL set to the internal nginx location that maps to the
// blob folder ("/_lgblob/", deploy/nginx-program-mail.conf) every
// attachment goes out through nginx, streamed from disk. Without it only
// files up to 26 MB are sent from here (a 100 MB one would be held about
// four times on a 0.5 GB server); bigger ones get a 503 (review 2026-10-27).
std::string mail_attachment_response(const std::string& blob_id, const std::string& mime, const std::string& name_key) {
    const ProgramKindInfo* kind = program_kind_by_mime(mime);
    const std::string name = kind ? g_store.attachment_name(name_key).value_or(attachment_filename_for_mime(mime))
                                  : attachment_filename_for_mime(mime);
    const std::string extra = "Content-Disposition: attachment; filename=\"" + name + "\"\r\n"
                              "Cache-Control: private, no-store\r\n";
    const auto meta = g_blobs.meta(blob_id);
    if (!meta || blob_id.size() <= 2) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
    const char* accel = std::getenv("LIQUIDGOV_BLOB_ACCEL");
    if (accel && accel[0]) {
        return http_raw("200 OK", mime.c_str(), "",
                        extra + "X-Accel-Redirect: " + accel + blob_id.substr(0, 2) + "/" + blob_id + "\r\n");
    }
    if (meta->size > kMaxBody)
        return http_raw("503 Service Unavailable", "text/plain; charset=utf-8",
                        "This file is too big to send without the server's download handoff.");
    auto bytes = g_blobs.get(blob_id);
    if (!bytes) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
    return http_raw("200 OK", mime.c_str(), std::string(bytes->begin(), bytes->end()), extra);
}

// Big uploads have one door: POST /mail/big (1:1) and /mail/big/g/<id>/send
// (groups). nginx lets a body over 26 MB through only there, and only after
// GET /internal/upload-ok said yes for that cookie and Content-Length
// (deploy/nginx-program-mail.conf), so nobody else can make nginx or this
// process buffer 100 MB (review 2026-10-27).
bool is_big_upload_target(const std::string& target) {
    if (target == "/mail/big") return true;
    return target.rfind("/mail/big/g/", 0) == 0 && target.size() > 17 &&
           target.compare(target.size() - 5, 5, "/send") == 0;
}

// The body limit for a program sender on the big door: their limit plus
// room for the multipart wrapping; kMaxBody for everyone and everything
// else. Caller holds g_mu.
std::size_t program_body_limit_locked(const std::string& headers) {
    auto sess = g_store.get_session(cookie_of(headers));
    if (!sess) return kMaxBody;
    auto acc = g_store.find_by_id(sess->account);
    if (!acc) return kMaxBody;
    auto policy = program_policy_for(acc->username);
    if (!policy) return kMaxBody;
    return std::max<std::size_t>(kMaxBody, static_cast<std::size_t>(policy->max_bytes) + kMultipartRoom);
}

// The limit read_http_request enforces, from the headers alone (before the
// body is read). Doesn't touch the session (handle() does that).
std::size_t request_body_limit(const std::string& headers) {
    auto le = headers.find("\r\n");
    std::istringstream is(headers.substr(0, le));
    std::string method, target;
    is >> method >> target;
    target = target.substr(0, target.find('?'));
    if (method != "POST" || !is_big_upload_target(target)) return kMaxBody;
    std::lock_guard<std::mutex> lock(g_mu);
    return program_body_limit_locked(headers);
}

// nginx's auth_request for /mail/big: 204 when this cookie's account may
// send programs and X-Original-Length fits its limit (or the ordinary one,
// whichever is bigger), 403 otherwise (nginx then never reads the body).
// No length (a chunked upload) is a no: nginx couldn't hold it to a size.
// Caller holds g_mu.
std::string upload_ok_response(const std::string& headers) {
    std::string uri = header_value(headers, "X-Original-URI");
    uri = uri.substr(0, uri.find('?'));
    std::size_t length = 0;
    try { length = static_cast<std::size_t>(std::stoull(header_value(headers, "X-Original-Length"))); } catch (...) { length = 0; }
    bool granted = false;
    if (auto sess = g_store.get_session(cookie_of(headers)))
        if (auto acc = g_store.find_by_id(sess->account)) granted = program_policy_for(acc->username).has_value();
    const bool ok = granted && is_big_upload_target(uri) && length > 0 && length <= program_body_limit_locked(headers);
    return ok ? http_raw("204 No Content", "text/plain; charset=utf-8", "")
              : http_raw("403 Forbidden", "text/plain; charset=utf-8", "no");
}

// No picture: the person's first letter in the circle, not an empty grey circle that reads as a broken image (sweep
// 2026-11-09). The first UTF-8 code point, so a name starting with a non-ASCII letter is not cut mid-character.
std::string portrait_placeholder(const std::string& username, const char* cls) {
    std::string initial;
    if (!username.empty()) {
        const unsigned char c0 = static_cast<unsigned char>(username[0]);
        std::size_t len = c0 < 0x80 ? 1 : (c0 >> 5) == 0x6 ? 2 : (c0 >> 4) == 0xE ? 3 : (c0 >> 3) == 0x1E ? 4 : 1;
        initial = username.substr(0, std::min(len, username.size()));
        if (initial.size() == 1) initial[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(initial[0])));
    }
    return std::string("<span class=\"portrait ")+cls+" ph\" aria-hidden=\"true\">"+html_escape(initial)+"</span>";
}

std::string portrait_tag(const std::string& username, const char* cls) {
    auto acc = g_store.find_by_username(username);
    if (!acc) return portrait_placeholder(username, cls);
    auto av = g_store.get(acc->id);
    if (!av || !g_blobs.meta(av->blob_id).has_value())
        return portrait_placeholder(username, cls);
    // /portrait/<username> is served with Cache-Control: max-age=300 at
    // this same URL regardless of which image is behind it, so a browser
    // that already fetched it once shows the old picture for up to five
    // minutes after a re-upload even though the server side updated
    // immediately. blob_id changes on every save (a fresh sha256 of the
    // new bytes -- see save_avatar), so using it as a cache-busting query
    // param forces a real re-fetch the moment the picture actually
    // changes, without weakening the cache header for repeat views of the
    // same picture.
    return std::string("<img class=\"portrait ")+cls+"\" src=\"/portrait/"+html_escape(username)+
           "?v="+html_escape(av->blob_id)+"\" alt=\"\" width=\"96\" height=\"96\">";
}

std::vector<std::string> people_hits(const std::string& viewer, const std::string& term) {
    std::vector<std::string> hits;
    if (term.empty()) return hits;
    if (g_store.is_findable_exact(viewer, term)) {
        if (auto found = g_store.find_by_username(term)) hits.push_back(found->username);
    }
    if (term.size() >= 3) {
        for (auto& n : g_store.typeahead(viewer, term)) {
            if (std::find(hits.begin(), hits.end(), n) == hits.end()) hits.push_back(n);
        }
    }
    return hits;
}

// Category browse for the floating widget's Search tab: which live
// jukeboxes (personal or community) a viewer can tune the widget to,
// filtered by a free-text term within one category. Not a recommendation
// engine -- just filtered listing, same posture as discover_people/
// discover_communities. "Others" and "All" route through typeahead(),
// so Findability and blocks apply exactly as they do everywhere else
// that's used -- a miss looks like a miss.
std::string jukebox_browse_json(const std::string& viewer, const std::string& category, const std::string& term) {
    struct Item { std::string id, name; };
    std::vector<Item> items;
    auto to_lower = [](std::string s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    const std::string term_lower = to_lower(term);
    auto matches = [&](const std::string& hay) {
        if (term.empty()) return true;
        return to_lower(hay).find(term_lower) != std::string::npos;
    };
    auto add_unique = [&](const std::string& id, const std::string& name) {
        for (auto& it : items) if (it.id == id) return;
        items.push_back({id, name});
    };
    if (category == "self") {
        add_unique("u:" + viewer, viewer);
    } else if (category == "group") {
        for (auto& c : g_store.communities_of(viewer)) {
            if (matches(c.name) || matches(c.slug)) add_unique(c.slug, c.name);
            if (items.size() >= 20) break;
        }
    } else if (category == "friends") {
        for (auto& f : g_store.friends_of(viewer)) {
            if (matches(f)) add_unique("u:" + f, f);
            if (items.size() >= 20) break;
        }
    } else if (category == "others") {
        if (term.size() >= 3) {
            for (auto& n : g_store.typeahead(viewer, term)) {
                if (n == viewer || g_store.are_friends(viewer, n)) continue;
                add_unique("u:" + n, n);
                if (items.size() >= 20) break;
            }
        }
    } else { // "all"
        for (auto& f : g_store.friends_of(viewer)) if (matches(f)) add_unique("u:" + f, f);
        for (auto& c : g_store.communities_of(viewer)) if (matches(c.name) || matches(c.slug)) add_unique(c.slug, c.name);
        if (term.size() >= 3) {
            for (auto& n : g_store.typeahead(viewer, term)) {
                if (n == viewer || g_store.are_friends(viewer, n)) continue;
                add_unique("u:" + n, n);
                if (items.size() >= 20) break;
            }
        }
        if (items.size() > 20) items.resize(20);
    }
    std::string out = "{\"items\":[";
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i) out += ",";
        out += "{\"id\":\"" + json_escape(items[i].id) + "\",\"name\":\"" + json_escape(items[i].name) + "\"}";
    }
    out += "]}";
    return out;
}

size_t mutual_friend_count(const std::string& a, const std::string& b) {
    auto fa = g_store.friends_of(a);
    std::set<std::string> sa(fa.begin(), fa.end());
    size_t n = 0;
    for (auto& x : g_store.friends_of(b)) if (sa.count(x)) ++n;
    return n;
}

std::string person_result_html(const std::string& viewer, const std::string& name) {
    auto found = g_store.find_by_username(name);
    const bool self = viewer == name;
    const bool pals = !self && g_store.are_friends(viewer, name);
    auto outg = g_store.outgoing_requests(viewer);
    auto inc = g_store.incoming_requests(viewer);
    const bool pending_out = std::find(outg.begin(), outg.end(), name) != outg.end();
    const bool pending_in = std::find(inc.begin(), inc.end(), name) != inc.end();

    std::string meta;
    if (self) meta = "That's you";
    else if (pals) meta = "Friends";
    else if (pending_out) meta = "Request sent";
    else if (pending_in) meta = "Wants to be friends";
    else if (found) meta = "Member since " + fmt_time(found->created_at);
    auto mutual = (self || !found) ? 0 : mutual_friend_count(viewer, name);
    if (mutual) meta += (meta.empty() ? "" : " · ") + std::to_string(mutual) +
                        (mutual == 1 ? " mutual friend" : " mutual friends");
    std::string blurb;
    if (found) {
        blurb = g_store.blurb(found->id);
        if (blurb.size() > 100) blurb = blurb.substr(0, 97) + "...";
    }

    std::string acts;
    if (self) {
        acts = "<a class=\"btn ghost\" href=\"/u/" + html_escape(name) + "\">Your profile</a>";
    } else {
        if (pending_in) {
            acts += "<form class=\"inline\" method=\"POST\" action=\"/friends/accept\">"
                    "<input type=\"hidden\" name=\"who\" value=\"" + html_escape(name) + "\">"
                    "<button type=\"submit\">Accept</button></form>";
        } else if (pals) {
            /* already friends */
        } else if (pending_out) {
            acts += "<span class=\"muted\">Request sent</span>";
        } else {
            acts += "<form class=\"inline\" method=\"POST\" action=\"/friends\">"
                    "<input type=\"hidden\" name=\"who\" value=\"" + html_escape(name) + "\">"
                    "<input type=\"hidden\" name=\"next\" value=\"/search?q=" + html_escape(name) + "\">"
                    "<button type=\"submit\">Add friend</button></form>";
        }
        if (pals) {
            acts += "<a class=\"btn\" href=\"/mail?with=" + html_escape(name) + "\">Message</a>";
        } else {
            acts += "<button class=\"ghost\" type=\"button\" disabled "
                    "title=\"Mail is friends-only until they accept\">Message</button>";
        }
        acts += "<a class=\"btn ghost\" href=\"/u/" + html_escape(name) + "\">Profile</a>";
    }

    return "<div class=\"hit\">" + portrait_tag(name, "sm") +
           "<div class=\"hitinfo\"><a href=\"/u/" + html_escape(name) + "\"><strong>" +
           html_escape(name) + "</strong></a>"
           "<span class=\"muted\">" + html_escape(meta) + "</span>" +
           (blurb.empty() ? "" : "<p class=\"muted\" style=\"margin:.2rem 0 0\">" + html_escape(blurb) + "</p>") +
           "</div><div class=\"hitacts\">" + acts + "</div></div>";
}

std::string people_results_html(const std::string& viewer, const std::string& term) {
    if (term.empty())
        return "<p class=\"muted\">Type a name. Matches update as you type. "
               "Typeahead needs three letters unless the name is exact.</p>";
    auto hits = people_hits(viewer, term);
    if (hits.empty())
        return "<p class=\"muted\">No match. A miss is a miss &mdash; missing, opted out, and blocked look the same.</p>";
    std::string html;
    for (auto& n : hits) html += person_result_html(viewer, n);
    return html;
}

std::string people_search_script() {
    return "<script>(function(){"
           "var q=document.getElementById('peopleq'),box=document.getElementById('peoplehits');"
           "if(!q||!box)return;"
           "var t=null,last='';"
           "function go(){"
           "var v=q.value.trim();if(v===last)return;last=v;"
           "if(!v){box.innerHTML='';return;}"
           "fetch('/search?q='+encodeURIComponent(v)+'&partial=1',{credentials:'same-origin'})"
           ".then(function(r){return r.text();})"
           ".then(function(h){if(q.value.trim()===v)box.innerHTML=h;})"
           ".catch(function(){});"
           "}"
           "q.addEventListener('input',function(){clearTimeout(t);t=setTimeout(go,180);});"
           "if(q.value.trim())go();"
           "})();</script>";
}

// ---- People you may know (Brandon 2026-11-10) ----------------------------
// "you shouldnt have to know someones specific handle. you should be able to see suggestions before even typing
// anything in. like people from groups youre in, or from the same town, or anything that might be relevant."
// Only people findable as you type (his answer); never yourself, a friend, a pending request either way, or a
// block either way. Friends in common are NAMED, like other social sites, but only through friendships both
// friends left visible on the map (MapPrefs::friends). Strongest reason first: shared groups, friends in common,
// the same town, the same problem shared publicly. tests/e2e/friend_suggestions.sh
struct PersonSuggestion { std::string name; std::vector<std::string> why; int score = 0; };

std::vector<PersonSuggestion> people_suggestions(const std::string& me, std::size_t limit) {
    std::vector<PersonSuggestion> out;
    if (me.empty()) return out;
    const auto mine = g_store.friends_of(me);
    std::set<std::string> skip(mine.begin(), mine.end());
    skip.insert(me);
    for (const auto& u : g_store.incoming_requests(me)) skip.insert(u);
    for (const auto& u : g_store.outgoing_requests(me)) skip.insert(u);
    auto eligible = [&](const std::string& u) {
        return !skip.count(u) && !g_store.is_blocked(me, u) && !g_store.is_blocked(u, me) &&
               g_store.find_by_username(u).has_value() && g_store.prefs_of(u).findability == Findability::Typeahead;
    };
    std::map<std::string, std::vector<std::string>> groups_with, friends_with, towns, problems;
    for (const auto& c : g_store.communities_of(me))
        for (const auto& m : g_store.members_of(c.slug)) if (eligible(m)) groups_with[m].push_back(c.name);
    for (const auto& f : mine) {
        if (!g_store.map_prefs_of(f).friends) continue; // f hides their friends
        for (const auto& c : g_store.friends_of(f))
            if (eligible(c) && g_store.map_prefs_of(c).friends) friends_with[c].push_back(f);
    }
    std::set<std::string> my_towns;
    for (const auto& cu : g_store.cities_in_use()) {
        bool in = g_store.is_city_listed(cu.key, me);
        if (!in) for (const auto& c : g_store.communities_of(me)) if (city_key(c.city) == cu.key) { in = true; break; }
        if (!in || !my_towns.insert(cu.key).second) continue;
        for (const auto& u : g_store.people_listed_in_city(cu.key)) if (eligible(u)) towns[u].push_back(cu.name);
    }
    for (const auto& p : g_store.pains_of(me)) {
        if (!p.kept || !p.flowed) continue;
        for (const auto& q : g_store.flowed_with_subject(p.phrase))
            if (q.owner != me && eligible(q.owner)) problems[q.owner].push_back(p.phrase);
    }
    std::set<std::string> everyone;
    for (const auto* m : {&groups_with, &friends_with, &towns, &problems}) for (const auto& [u, v] : *m) everyone.insert(u);
    auto names = [](const std::vector<std::string>& v) {
        if (v.size() == 1) return v[0];
        if (v.size() == 2) return v[0] + " and " + v[1];
        return v[0] + ", " + v[1] + " and " + std::to_string(v.size() - 2) + (v.size() == 3 ? " other" : " others");
    };
    for (const auto& u : everyone) {
        PersonSuggestion s{u, {}, 0};
        if (groups_with.count(u)) {
            const auto& g = groups_with[u];
            s.why.push_back(g.size() == 1 ? "In " + g[0] + " with you" : "In " + std::to_string(g.size()) + " of your groups");
            s.score += 30 * static_cast<int>(g.size());
        }
        if (friends_with.count(u)) {
            s.why.push_back("Friends with " + names(friends_with[u]));
            s.score += 20 * static_cast<int>(friends_with[u].size());
        }
        if (towns.count(u)) { s.why.push_back("Also in " + towns[u][0]); s.score += 10; }
        if (problems.count(u)) { s.why.push_back("Also noticed " + problems[u][0]); s.score += 8; }
        out.push_back(std::move(s));
    }
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        return a.score != b.score ? a.score > b.score : a.name < b.name;
    });
    if (out.size() > limit) out.resize(limit);
    return out;
}

// The card: face, name, why, and a one-tap Add friend that comes back to `back`. Empty when nobody fits.
std::string people_suggestions_html(const std::string& me, std::size_t limit, const std::string& back) {
    const auto list = people_suggestions(me, limit);
    if (list.empty()) return "";
    std::string html = "<div class=\"card\" id=\"suggest\"><h2>People you may know</h2>";
    for (const auto& s : list) {
        std::string why;
        for (const auto& w : s.why) why += (why.empty() ? "" : " &middot; ") + html_escape(w);
        html += "<div class=\"person suggest\" data-user=\"" + html_escape(s.name) + "\"><div class=\"who\">" +
                portrait_tag(s.name, "sm") + "<div><a href=\"/u/" + html_escape(s.name) + "\"><strong>" +
                html_escape(s.name) + "</strong></a><div class=\"muted why\">" + why + "</div></div></div>"
                "<form class=\"inline\" method=\"POST\" action=\"/friends\">"
                "<input type=\"hidden\" name=\"who\" value=\"" + html_escape(s.name) + "\">"
                "<input type=\"hidden\" name=\"next\" value=\"" + html_escape(back) + "\">"
                "<button type=\"submit\">Add friend</button></form></div><!--/row-->";
    }
    if (back != "/friends") html += "<p class=\"muted\"><a href=\"/friends\">More people</a></p>";
    return html + "</div><!--/suggest-->";
}

// One-time switch (Brandon 2026-11-10): everyone who never saved Map settings gets the new friends-on default
// written down, plus a note saying so. Anyone with a saved record keeps what they chose. The record it writes makes
// this run once per person. New accounts get their record at signup (no note: on was already their default).
void map_friends_default_on() {
    for (const auto& u : g_store.all_usernames()) {
        if (g_store.has_map_prefs(u)) continue;
        g_store.set_map_prefs(u, MapPrefs{});
        g_store.add_notice(u, "map_friends_on", "", "");
    }
}

// Shared by /notices (full page) and GET /notices/panel (the fragment the
// header's notifications widget fetches on open) -- one implementation,
// two callers, so the widget and the page can never drift apart.
std::string notices_list_html(const std::string& user) {
    auto notes = g_store.notices_for(user, 50);
    int fresh = 0;
    for (const auto& n : notes) if (!n.read) ++fresh;
    std::string html = "<div class=\"card\" data-unread=\"" + std::to_string(fresh) + "\">";
    if (fresh > 0) html += "<p class=\"notif-banner\">" + std::to_string(fresh) + (fresh==1?" new":" new") + "</p>";
    if (notes.empty()) html += "<p class=\"empty\">Nothing waiting.</p>";
    for (auto& n : notes) {
        std::string line;
        std::string href;
        const bool pending_fr = n.type=="friend_request" &&
            ([&]{ auto inc=g_store.incoming_requests(user); return std::find(inc.begin(),inc.end(),n.from)!=inc.end(); })();
        if (n.type=="friend_request") { line = html_escape(n.from)+" sent a friend request."; href="/u/"+html_escape(n.from); }
        else if (n.type=="friend_accept") { line = html_escape(n.from)+" accepted your request."; href="/u/"+html_escape(n.from); }
        else if (n.type=="message") { line = html_escape(n.from)+" sent mail."; href="/mail?with="+html_escape(n.from); }
        else if (n.type=="mail_group") { line = html_escape(n.from)+" invited you to a group."; href="/mail/g/"+html_escape(n.extra); }
        else if (n.type=="group_invite") { line = html_escape(n.from)+" added you to /"+html_escape(n.extra)+"."; href="/c/"+html_escape(n.extra); }
        else if (n.type=="map_friends_on") { line = "Your friends now show on the map and in friend suggestions, so people can find each other. You can change it in Settings &rsaquo; Map."; href="/settings#map"; }
        else line = html_escape(n.type);
        html += "<div class=\"person" + std::string(n.read?"":" notice-unread") + "\">";
        if (!n.from.empty()) html += portrait_tag(n.from, "sm");
        else html += "<span class=\"portrait sm ph\" aria-hidden=\"true\">&#9673;</span>"; // a site note: same indent as the rest
        if (!href.empty()) html += "<a href=\""+href+"\" style=\"flex:1;min-width:0;color:inherit;text-decoration:none\">";
        else html += "<div style=\"flex:1;min-width:0\">";
        html += "<div>"+line+"<div class=\"muted\">"+html_escape(fmt_time(n.ts))+"</div></div>";
        if (!href.empty()) html += "</a>"; else html += "</div>";
        if (pending_fr) {
            html += "<form class=\"inline notice-accept\" method=\"POST\" action=\"/friends/accept\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(n.from)+"\">"
                    "<button type=\"submit\">Accept</button></form>";
        }
        html += "</div>";
    }
    html += "<p class=\"muted\">A declined friend request does not send a notice.</p></div>";
    return html;
}

// The friends section of the mail widget/page: real hrefs (/mail?with=X)
// so it still works with JS off or as a direct link, plus a data-with the
// widget's own delegated click handler uses to fetch the thread in place
// instead of navigating -- same progressive-enhancement shape as the
// site's own soft-navigation (see jukebox_script's click interceptor).
// One chronological inbox: 1:1 threads that actually have mail, plus groups.
// Friends with no thread live under "Start a conversation", not the list.
std::string mail_inbox_html(const std::string& user, AccountId acc_id) {
    auto pals = g_store.friends_of(user);
    auto inner = g_store.inner_of(user);
    std::set<std::string> inset(inner.begin(), inner.end());
    std::vector<std::string> top, rest;
    for (auto& p : inner) {
        if (g_store.are_friends(user, p) && !g_store.is_blocked(user, p) && !g_store.is_blocked(p, user))
            top.push_back(p);
    }
    for (auto& p : pals) {
        if (inset.count(p)) continue;
        if (g_store.is_blocked(user, p) || g_store.is_blocked(p, user)) continue;
        rest.push_back(p);
    }
    std::sort(rest.begin(), rest.end());
    auto rows_for = [&](const std::vector<std::string>& people) {
        std::string out;
        for (auto& other : people) {
            auto ts = g_store.last_mail_ts(user, other);
            auto prev = g_store.last_mail_preview(user, other);
            if (prev.size() > 80) prev = prev.substr(0, 77) + "...";
            bool unread = ts > g_store.thread_last_read(user, "d:" + other);
            out += "<a class=\"inboxrow mailwith";
            if (unread) out += " unread";
            out += "\" href=\"/mail?with=" + html_escape(other) + "\" data-with=\"" + html_escape(other) + "\">";
            out += portrait_tag(other, "sm");
            out += "<span style=\"min-width:0;flex:1\"><span class=\"inboxtitle\">" + html_escape(other) + "</span>";
            out += "<span class=\"inboxpreview\">" + html_escape(prev) + "</span></span>";
            if (ts) out += "<span class=\"inboxmeta\">" + html_escape(fmt_time(ts)) + "</span>";
            out += "</a>";
        }
        return out;
    };
    auto checks = [&](const std::vector<std::string>& people) {
        std::string out;
        for (auto& other : people) {
            out += "<label class=\"grouppickrow\" style=\"display:flex;align-items:center;gap:.5rem;min-height:44px;padding:.3rem .4rem;cursor:pointer\">";
            out += portrait_tag(other, "sm") + "<span style=\"flex:1\">" + html_escape(other) + "</span>";
            out += "<input type=\"checkbox\" data-who=\"" + html_escape(other) + "\"></label>";
        }
        return out;
    };
    std::string html = "<div class=\"mailsection\">";
    html += "<button type=\"button\" class=\"ghost mailgroupbtn\">Group</button>";
    html += "<div class=\"mailgroupbox\" hidden>";
    html += "<form method=\"POST\" action=\"/mail/group/new\" class=\"mailnewgroup mailgroupform\">";
    html += "<input name=\"title\" maxlength=\"80\" placeholder=\"Group name (optional)\">";
    html += "<input type=\"hidden\" name=\"who\" class=\"mailgroupwho\" value=\"\">";
    html += "<p class=\"muted\">Pick who to invite</p><div class=\"grouppick\" style=\"max-height:min(40vh,320px);overflow-y:auto\">";
    html += checks(top) + checks(rest);
    if (top.empty() && rest.empty()) html += "<p class=\"muted\">Add friends first.</p>";
    html += "</div><button type=\"submit\">Start group</button></form>";
    std::string groups;
    for (auto& conv : g_store.conversations_for(acc_id)) {
        if (!conv.is_group) continue;
        bool pending = false;
        for (auto& m : conv.members) if (m.account == acc_id) pending = m.state == MemberState::Pending;
        auto ts = g_store.last_group_ts(conv.id);
        auto prev = g_store.last_group_preview(conv.id);
        if (prev.size() > 80) prev = prev.substr(0, 77) + "...";
        bool unread = pending || ts > g_store.thread_last_read(user, "g:" + std::to_string(conv.id));
        groups += "<a class=\"inboxrow mailgroup";
        if (unread) groups += " unread";
        groups += "\" href=\"/mail/g/" + std::to_string(conv.id) + "\">";
        groups += "<span class=\"portrait sm ph\" aria-hidden=\"true\"></span>";
        groups += "<span style=\"min-width:0;flex:1\"><span class=\"inboxtitle\">" + html_escape(group_label(conv, acc_id)) + "</span>";
        groups += "<span class=\"inboxpreview\">" + html_escape(prev);
        if (pending) groups += " · invite";
        groups += "</span></span></a>";
    }
    if (!groups.empty()) html += "<p class=\"eyebrow\">Your groups</p><div class=\"inboxlist\">" + groups + "</div>";
    html += "</div>";
    if (top.empty() && rest.empty()) {
        html += "<p class=\"muted\">No friends yet. <a href=\"/friends\">Find people</a></p>";
    } else {
        if (!top.empty()) html += "<p class=\"eyebrow\">Inner circle</p><div class=\"inboxlist\">" + rows_for(top) + "</div>";
        if (!rest.empty()) html += "<p class=\"eyebrow\">Friends</p><div class=\"inboxlist\">" + rows_for(rest) + "</div>";
    }
    html += "<p class=\"maildisclose\">Messages on this copy are stored so the operator can read them. "
            "Direct mail is friends-only until age assurance exists. Unfriending leaves shared groups.</p></div>";
    return html;
}
// Kept for the widget's "start a group" only — inbox is the unified list.
std::string mail_groups_list_html(const std::string& user, AccountId acc_id) {
    return mail_inbox_html(user, acc_id);
}

std::string mail_friends_list_html(const std::string& user) {
    auto acc = g_store.find_by_username(user);
    if (!acc) return "";
    return mail_inbox_html(user, acc->id);
}

// The open thread: message bubbles (mine/theirs) plus the composer. Shared
// by the full /mail page and GET /mail/panel?with=<friend> -- same
// friends-only/not-blocked gate either way, empty string means "not
// viewable" (unknown other, not actually friends, or blocked either
// direction), which the caller renders as simply no thread shown.
std::string mail_thread_html(const std::string& user, const std::string& other,
                              const std::map<std::string,std::string>& qs) {
    if (other.empty() || !g_store.are_friends(user, other) || g_store.is_blocked(user, other)) return "";
    g_store.mark_thread_read(user, "d:" + other);
    std::string html = "<div class=\"mailthread\">"
        "<div class=\"mailthreadhead\"><a class=\"mailback\" data-with=\"\" href=\"/mail\" aria-label=\"Back to messages\">\xE2\x86\x90</a>"
        + portrait_tag(other, "sm") +
        "<strong><a href=\"/u/"+html_escape(other)+"\">"+html_escape(other)+"</a></strong></div>";
    if (qs.count("err")) {
        const std::string& e = qs.at("err");
        if (e=="notfriends") html += "<p class=\"warn\">Mail is friends-only until they accept.</p>";
        else html += mail_attachment_error_html(e, user);
    }
    auto thread = g_store.mail_with(user, other);
    html += "<div class=\"msglist\" id=\"msglist\">";
    if (thread.empty()) html += "<p class=\"empty\">No messages yet.</p>";
    for (auto& m : thread) {
        const bool mine = m.from == user;
        html += "<div class=\"msg "+std::string(mine ? "mine" : "theirs")+"\"><div class=\"bubble\">";
        if (!m.text.empty()) html += html_escape(m.text);
        if (m.attachment_blob_id) {
            html += mail_attachment_html(*m.attachment_blob_id, m.attachment_mime.value_or(""), attachment_name_key(m), m.from, mine);
        }
        html += "<span class=\"bubtime\">"+html_escape(fmt_time(m.ts))+"</span></div></div>";
    }
    html += "</div>"
        "<form method=\"POST\" action=\"/mail\" enctype=\"multipart/form-data\" class=\"mailcomposer\" id=\"mailcomposer\""
        + std::string(kComposerOnSubmit) + ">"
        "<input type=\"hidden\" name=\"to\" value=\""+html_escape(other)+"\">"
        "<div class=\"row\" style=\"align-items:flex-end;gap:.4rem;flex-wrap:nowrap\">"
        "<textarea name=\"text\" id=\"mailtext\" maxlength=\"2000\" rows=\"2\" placeholder=\"Message\" "
        "autocomplete=\"off\" style=\"flex:1;min-width:0;min-height:44px;resize:vertical;background:#fff;color:#14110c;caret-color:#14110c\"></textarea>"
        "<label class=\"mailattbtn\" title=\"Attach a file\">"
        + mail_file_input_html(user, "/mail/big") +
        "<span aria-hidden=\"true\">\xF0\x9F\x93\x8E</span></label>"
        "<button type=\"submit\">Send</button></div>"
        + std::string(kComposerFileLine) +
        "<p class=\"muted\" style=\"margin:.3rem 0 0\">Enter sends. Shift+Enter is a new line.</p>"
        + mail_program_note_html(user) + "</form></div>"
        "<script>(function(){"
        "var list=document.getElementById('msglist');if(list)list.scrollTop=list.scrollHeight;"
        // the full Messages page scrolls as a whole (Brief 4 item 8): open at the newest message, composer in view
        "if(list&&list.closest('.mailpage')){var mc=document.getElementById('mailcomposer');if(mc)mc.scrollIntoView({block:'end'});}"
        "var form=document.getElementById('mailcomposer'),ta=document.getElementById('mailtext');"
        "if(!form||!ta)return;"
        "ta.addEventListener('keydown',function(e){"
        "if(e.key==='Enter'&&!e.shiftKey){e.preventDefault();form.requestSubmit();}"
        "});"
        "})();</script>";
    return html;
}

// Drives the Notifications/Messages header widgets: open/close, fetching
// each panel's fragment (/notices/panel, /mail/panel), switching between
// the friends list and an open thread inside the mail panel without a
// real navigation, sending a message via fetch, and polling the open
// thread every few seconds so it behaves like an actual instant messenger
// instead of a static page you have to manually refresh.
//
// Lives in .topinner as a sibling of <nav class="accountmenu">, same as
// the buttons and panels it drives -- the site's own soft-navigation (see
// jukebox_script) only ever replaces the *inside* of that <nav> element,
// never this script tag or the panels themselves, so this widget's state
// (an open thread, the poll timer) survives browsing to another page the
// same way the floating jukebox widget already does.
std::string mail_widget_script() {
    return
        "<script>(function(){"
        "var notifToggle=document.getElementById('notiftoggle'),notifPanel=document.getElementById('notifpanel'),"
        "notifRoot=document.getElementById('notifroot');"
        "var mailToggle=document.getElementById('mailtoggle'),mailPanel=document.getElementById('mailpanel'),"
        "mailRoot=document.getElementById('mailroot');"
        "var navc=document.getElementById('navc');"
        "if(!notifToggle||!mailToggle)return;"
        "var lastN=-1,lastM=-1;"
        "document.addEventListener('pointerdown',function(){try{if(!window.__lgAC)window.__lgAC=new(window.AudioContext||window.webkitAudioContext)();if(window.__lgAC.resume)window.__lgAC.resume();}catch(err){}},{once:true});"
        "function lgDing(){try{var ac=window.__lgAC;if(!ac)return;if(ac.resume)ac.resume();var o=ac.createOscillator(),g=ac.createGain();o.frequency.value=880;g.gain.value=0.07;o.connect(g);g.connect(ac.destination);o.start();g.gain.exponentialRampToValueAtTime(0.001,ac.currentTime+0.2);o.stop(ac.currentTime+0.22);}catch(err){}}"
        "function lgSetBadge(btn,v){if(!btn)return;var b=btn.querySelector('.badge');if(v<=0){if(b)b.remove();return;}if(!b){b=document.createElement('span');b.className='badge';btn.appendChild(b);}b.textContent=String(v);}"
        "function lgApplyUnread(n,m){n=n|0;m=m|0;var t=n+m;try{if(navigator.setAppBadge){if(t)navigator.setAppBadge(t);else if(navigator.clearAppBadge)navigator.clearAppBadge();}}catch(err){}lgSetBadge(notifToggle,n);lgSetBadge(mailToggle,m);if(lastN>=0&&(n>lastN||m>lastM)){lgDing();if('Notification' in window&&Notification.permission==='granted'){try{new Notification('LiquidGov',{body:m>lastM?'New message':'New notice',tag:'lg-unread',icon:'/favicon.svg'});}catch(err){}}}lastN=n;lastM=m;}"
        "function lgPollUnread(){fetch('/me/unread',{credentials:'same-origin'}).then(function(r){return r.ok?r.json():null;}).then(function(d){if(d)lgApplyUnread(d.n,d.m);}).catch(function(){});}"
        "lgPollUnread();setInterval(lgPollUnread,20000);"
        "function lgAskNotify(){if(!('Notification' in window))return;if(Notification.permission==='default'){try{Notification.requestPermission();}catch(err){}}}"
        "var activeWith=null,mailFetching=false,pollTimer=null;"
        "function nearBottom(el){return el.scrollHeight-el.scrollTop-el.clientHeight<60;}"
        "function stopPoll(){if(pollTimer){clearInterval(pollTimer);pollTimer=null;}}"
        "function hideNotif(){notifPanel.hidden=true;}"
        "function hideMail(){mailPanel.hidden=true;stopPoll();activeWith=null;}"
        "function closeMenu(){if(navc)navc.checked=false;}"
        "function loadMail(withUser,forceScroll){"
        "var ae=document.activeElement;"
        "if(!forceScroll&&ae&&mailRoot.contains(ae)&&(ae.tagName==='TEXTAREA'||ae.tagName==='INPUT'||(ae.closest&&ae.closest('.mailcomposer'))))return;"
        "var t0=mailRoot.querySelector('#mailtext');"
        "var f0=mailRoot.querySelector('.mailcomposer input[type=file],#mailcomposer input[type=file]');"
        "if(!forceScroll&&f0&&f0.files&&f0.files.length)return;"
        "var gbox=mailRoot.querySelector('.mailgroupbox');"
        "if(!forceScroll&&gbox&&!gbox.hidden)return;"
        "var draft=t0?t0.value:'',ss=t0?t0.selectionStart:0,se=t0?t0.selectionEnd:0;"
        "if(mailFetching)return;"
        "mailFetching=true;"
        "var url='/mail/panel'+(withUser?('?with='+encodeURIComponent(withUser)):'');"
        "fetch(url).then(function(r){return r.text();}).then(function(html){"
        "mailFetching=false;"
        "var list=document.getElementById('msglist');"
        "var wasNear=list?nearBottom(list):true;"
        "mailRoot.innerHTML=html;"
        "var t1=mailRoot.querySelector('#mailtext');"
        "if(t1&&draft){t1.value=draft;try{t1.setSelectionRange(ss,se);}catch(err){}}"
        "activeWith=withUser||null;"
        "var newList=document.getElementById('msglist');"
        "if(newList&&(forceScroll||wasNear))newList.scrollTop=newList.scrollHeight;"
        "if(activeWith&&!pollTimer)pollTimer=setInterval(function(){loadMail(activeWith,false);},3000);"
        "if(!activeWith)stopPoll();"
        "}).catch(function(){mailFetching=false;});"
        "}"
        "notifToggle.addEventListener('click',function(){"
        "lgAskNotify();"
        "if(!notifPanel.hidden){hideNotif();return;}"
        "hideMail();closeMenu();"
        "notifPanel.hidden=false;"
        "fetch('/notices/panel').then(function(r){return r.text();}).then(function(html){"
        "notifRoot.innerHTML=html;"
        "var b=notifToggle.querySelector('.badge');if(b)b.remove();"
        "});"
        "});"
        "document.addEventListener('click',function(e){"
        "var btn=e.target.closest&&e.target.closest('.mailgroupbtn');"
        "if(!btn)return;"
        "e.preventDefault();e.stopPropagation();"
        "var box=btn.parentNode.querySelector('.mailgroupbox');"
        "if(box)box.hidden=!box.hidden;"
        "});"
        "document.addEventListener('submit',function(e){"
        "var form=e.target&&e.target.closest&&e.target.closest('.mailgroupform');"
        "if(!form)return;"
        "var names=[];"
        "form.querySelectorAll('input[data-who]:checked').forEach(function(c){names.push(c.getAttribute('data-who'));});"
        "var h=form.querySelector('.mailgroupwho');"
        "if(h)h.value=names.join(' ');"
        "if(!names.length)e.preventDefault();"
        "});"
        "mailToggle.addEventListener('click',function(){"
        "lgAskNotify();"
        "if(!mailPanel.hidden){hideMail();return;}"
        "hideNotif();closeMenu();"
        "mailPanel.hidden=false;"
        "var b=mailToggle.querySelector('.badge');if(b)b.remove();"
        "loadMail(null,false);"
        "});"
        "if(navc)navc.addEventListener('change',function(){if(navc.checked){hideNotif();hideMail();}});"
        // The account menu's own links (Feed, Settings, Log out, ...) go
        // through a real or soft navigation either way -- nothing about
        // that unchecks navc on its own, so the dropdown was left looking
        // stuck open on top of whatever page you landed on. Delegated on
        // the <nav> itself, not its links: soft-navigation only ever
        // replaces this nav's *contents* (see jukebox_script's swap),
        // never the <nav> element itself, so one listener here survives
        // every page the account menu's own links take you to.
        "var accountMenu=document.querySelector('nav.accountmenu');"
        "if(accountMenu)accountMenu.addEventListener('click',function(e){"
        "if(e.target.closest('a'))closeMenu();"
        "});"
        "function clickInsideChrome(t){"
        "if(!t||!t.closest)return false;"
        "return !!(t.closest('nav.accountmenu')||t.closest('#notifpanel')||t.closest('#mailpanel')"
        "||t.closest('.navavatar')||t.closest('#notiftoggle')||t.closest('#mailtoggle')"
        "||t.closest('label.navbtn')||t.closest('#navc'));"
        "}"
        "document.addEventListener('click',function(e){"
        "if(clickInsideChrome(e.target))return;"
        "closeMenu();hideNotif();hideMail();"
        "});"
        "document.addEventListener('keydown',function(e){"
        "if(e.key==='Escape'){closeMenu();hideNotif();hideMail();}"
        "});"
        // Delegated, not per-link: mailRoot's own contents are replaced
        // wholesale on every load, so listeners bound to specific elements
        // would be gone after the very first fetch.
        "mailRoot.addEventListener('click',function(e){"
        "var a=e.target.closest('a.mailwith,a.mailback,a.inboxrow.mailwith');"
        "if(a){e.preventDefault();loadMail(a.classList.contains('mailback')?null:a.dataset.with,true);return;}"
        // A group link (unlike mailwith/mailback) is a real navigation,
        // not fetched into the panel -- same "left looking stuck open"
        // problem as the account menu, fixed the same way. Attachment
        // downloads are deliberately not matched here: those carry
        // Content-Disposition:attachment, so the browser never actually
        // navigates away and the panel closing on one would just be wrong.
        "if(e.target.closest('a.mailgroup'))hideMail();"
        "});"
        "mailRoot.addEventListener('submit',function(e){"
        "var form=e.target.closest('form.mailcomposer');"
        "if(!form)return;"
        "e.preventDefault();"
        "var fd=new FormData(form);"
        // XHR, not fetch, for the upload's progress: the Send button reads "Sending 37%" while a file goes up
        // (2026-10-27: a 17 MB program looked like nothing was happening, and leaving the panel cancelled it)
        "var btn=form.querySelector('button[type=submit]');"
        "if(form.dataset.sending)return;form.dataset.sending='1';if(btn)btn.textContent='Sending...';"
        "var x=new XMLHttpRequest();"
        // the form's own action (/mail/big for a program sender); a refusal (?err=, /mail/too-large) is shown as a
        // page, since the panel reload would drop its message
        "x.open('POST',form.getAttribute('action')||'/mail');"
        "x.upload.onprogress=function(p){if(btn&&p.lengthComputable)btn.textContent='Sending '+Math.floor(p.loaded*100/p.total)+'%';};"
        "x.onload=function(){var u=x.responseURL||'';"
        "if(/[?&]err=|\\/mail\\/too-large/.test(u)){location.href=u;return;}"
        // nginx's own refusals of an upload (too large 413, too many at once 429) never reach the app
        "if(x.status<200||x.status>=300){location.href='/mail/too-large?status='+x.status;return;}loadMail(activeWith,true);};"
        "x.onerror=function(){location.href='/mail/too-large?status=0';};"
        "x.send(fd);"
        "});"
        "mailRoot.addEventListener('keydown',function(e){"
        "if(e.key!=='Enter'||e.shiftKey)return;"
        "var ta=e.target.closest('textarea[name=text]');"
        "if(!ta)return;"
        "var form=ta.closest('form.mailcomposer');"
        "if(!form)return;"
        "e.preventDefault();"
        "form.requestSubmit();"
        "});"
        "})();</script>";
}

// Shared drawing-tool engine: freehand brushes (pen, marker, spray, eraser,
// smudge), shape tools (line, rectangle, circle, with an optional filled
// variant), a paint-bucket flood fill, a rectangular select tool (copy/cut/
// paste, plus crop and resize against the selection), a text tool, up to
// three independently-undoable layers, image/video/gif import, and paste
// of an external image straight from the OS clipboard. Powers three pages
// that share every tool but differ in what "Save" does:
//   portrait=false, post_target empty -- /me/canvas, the profile-background
//     tool. Captures a PNG after every completed action (capped, see
//     kCanvasMaxFrames) by flattening the layers and uploading through
//     /me/canvas/frame -- the exact same normalize/scan pipeline as an
//     avatar upload, just appending to an ordered list instead of
//     replacing a single slot. Save turns that list into the profile
//     page's own identity-card background: last frame as a static image,
//     or all of them as a CSS-only looping animation (see
//     profile_canvas_css) -- no JS-driven replay, just background-image
//     keyframes, same reasoning a sandboxed document would have forced
//     anyway had this stayed there.
//   portrait=true -- /me/portrait, the self-portrait tool. No frame
//     history needed (an avatar isn't a sequence): Save just flattens the
//     current layers and POSTs the one image straight to /me/avatar.
//   post_target non-empty -- a post composer (/me/post/canvas, /feed/canvas,
//     /c/<slug>/canvas): the canvas itself IS the post's photo/video editor.
//     No frame history either; Save flattens the layers, pairs the image
//     with the caption textarea, and POSTs both as one multipart request
//     straight to post_target (/me/post, /feed, or /c/<slug>) -- the exact
//     same read_composer_post()-backed route a plain file-upload composer
//     already posts to, so no new backend ingestion path was needed for
//     this at all, only a richer way to produce the "media" file first.
// Everything else -- the whole brush/shape/layer/select/import engine -- is
// identical, built once here rather than forked into three copies that
// would drift apart.
std::string canvas_tool_page(const std::string& username, const std::string& err, bool portrait,
                             const std::string& post_target = "") {
    bool post_mode = !post_target.empty();
    // Reopening the profile-background tool used to always start from a
    // blank canvas, even when a drawing was already saved -- the only way
    // to "edit" it was to redraw from scratch. Looked up once here so both
    // the frame-count label and the actual pixel reload below can use it.
    std::size_t existing_frames = 0;
    bool existing_timelapse = false;
    if (!portrait && post_target.empty()) {
        if (auto owner = g_store.find_by_username(username)) {
            existing_frames = g_store.canvas_frames(owner->id).size();
            existing_timelapse = g_store.canvas_timelapse(owner->id);
        }
    }
    // A post drawing is shown at the post's full width, so its bitmap is 1280 x 960 (Brief 6 item 5: sharp on a
    // phone's 3x screen; the post path takes images up to SizeCaps::image_bytes, 10 MB, with no edge limit). PX is
    // bitmap pixels per pixel of the old 640 x 480 canvas: the script scales every size the person picks (brush,
    // shape, smudge, text, marquee) by it, so a size-8 brush looks as thick as before. /me/canvas stays 640 x 440:
    // its frames go through save_avatar, which refuses an edge over kAvatarMaxEdgePx (1024) or 2 MB.
    const int px = post_mode ? 2 : 1;
    int cw = portrait ? 400 : 640 * px;
    int ch = portrait ? 400 : (post_mode ? 480 * px : 440);
    std::string cws = std::to_string(cw), chs = std::to_string(ch);
    std::string aspect = cws + "/" + chs;
    // `err` is already-rendered HTML from flash_error(qs) at the call site
    // (frameimage/framelarge/framefull), not a raw code -- this function
    // doesn't need to know the code table, just where to put the result.
    std::string errHtml;
    if (!portrait && !post_mode) {
        errHtml = err;
    }
    // Icon buttons, not text labels: each of these already has a symbol
    // recognized from every other editor -- currentColor so they follow
    // the button's own text colour (and the light/dark theme) for free.
    const std::string kIconCopy =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" aria-hidden=\"true\"><rect x=\"5.5\" y=\"5.5\" width=\"8\" height=\"8\" rx=\"1\"/>"
        "<path d=\"M3 10.5V3a1 1 0 0 1 1-1h7.5\"/></svg>";
    const std::string kIconCut =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" aria-hidden=\"true\"><circle cx=\"4\" cy=\"4\" r=\"1.6\"/>"
        "<circle cx=\"4\" cy=\"12\" r=\"1.6\"/><path d=\"M5.2 5.2 13 13M5.2 10.8 13 3\"/></svg>";
    const std::string kIconPaste =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" aria-hidden=\"true\"><rect x=\"3\" y=\"3\" width=\"10\" height=\"11.5\" rx=\"1\"/>"
        "<rect x=\"6\" y=\"1.3\" width=\"4\" height=\"2.4\" rx=\"0.6\"/></svg>";
    const std::string kIconCrop =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" aria-hidden=\"true\"><path d=\"M4 1v10.5a1 1 0 0 0 1 1H15\"/>"
        "<path d=\"M12 15V4.5a1 1 0 0 0-1-1H1\"/></svg>";
    // Same icon on all three layer tabs, deliberately -- a layer tab's job
    // is "switch to this one" and "is this the active one" (shown by the
    // ghost/highlighted background), not "remember which number this is".
    const std::string kIconLayer =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.1\" aria-hidden=\"true\"><path d=\"M8 1 14 4.2 8 7.4 2 4.2Z\"/>"
        "<path d=\"M2 7.6 8 10.8 14 7.6\"/><path d=\"M2 11 8 14.2 14 11\"/></svg>";
    // The tool picker, same reasoning: eleven recognizable shapes read
    // faster than eleven words in a dropdown you have to open first.
    const std::string kIconPen =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><path d=\"M3 13l.6-2.8L10.6 3.2a1 1 0 0 1 1.4 0l.8.8a1 1 0 0 1 0 1.4"
        "L5.8 12.4 3 13Z\"/><path d=\"M9.3 4.5 11.5 6.7\"/></svg>";
    // A bare thick diagonal line read as just "a thicker Line tool", not
    // "marker" -- a recognizable pen body (the barrel) plus its own ink
    // mark reads as the tool, not the stroke it happens to draw.
    const std::string kIconMarker =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><rect x=\"2.3\" y=\"2.3\" width=\"4\" height=\"10.5\" rx=\"1\" "
        "transform=\"rotate(40 4.3 7.55)\"/><path d=\"M2.3 13.8h4\" stroke-width=\"2\" stroke-linecap=\"round\"/></svg>";
    const std::string kIconSpray =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><circle cx=\"5.5\" cy=\"9.5\" r=\"2.2\"/>"
        "<circle cx=\"11\" cy=\"3\" r=\".7\" fill=\"currentColor\" stroke=\"none\"/>"
        "<circle cx=\"13\" cy=\"6\" r=\".7\" fill=\"currentColor\" stroke=\"none\"/>"
        "<circle cx=\"9.5\" cy=\"3.5\" r=\".7\" fill=\"currentColor\" stroke=\"none\"/>"
        "<circle cx=\"11.5\" cy=\"8.5\" r=\".7\" fill=\"currentColor\" stroke=\"none\"/></svg>";
    // A single wavy line just read as "squiggle," not "blend/drag" --
    // three parallel strokes fading out reads as a motion trail, the same
    // visual shorthand smudge/blur tools use elsewhere.
    const std::string kIconSmudge =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.4\" stroke-linecap=\"round\" aria-hidden=\"true\"><path d=\"M3.5 12.5 7.5 8.5\"/>"
        "<path d=\"M6 13.5 10 9.5\" opacity=\".6\"/><path d=\"M8.5 14.5 12.5 10.5\" opacity=\".3\"/></svg>";
    const std::string kIconEraser =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\">"
        "<rect x=\"3.3\" y=\"6.3\" width=\"9.4\" height=\"5.4\" rx=\"1\" transform=\"rotate(-28 8 9)\"/></svg>";
    const std::string kIconLine =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" stroke-linecap=\"round\" aria-hidden=\"true\"><path d=\"M2.5 13.5 13.5 2.5\"/></svg>";
    const std::string kIconRect =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" aria-hidden=\"true\"><rect x=\"2.5\" y=\"3.5\" width=\"11\" height=\"9\" rx=\"1\"/></svg>";
    const std::string kIconCircle =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" aria-hidden=\"true\"><circle cx=\"8\" cy=\"8\" r=\"5.3\"/></svg>";
    // The old abstract polygon-plus-drip didn't read as a bucket at 16px --
    // a real trapezoid body with a rim and a single drip is the shape
    // paint-bucket icons actually use everywhere else.
    // The bucket's body and drip are filled with the actual selected
    // colour (kept in sync by ccolor's own input listener below), not a
    // static outline -- so the tool icon itself shows what it's about to
    // pour, the way a real paint bucket would.
    const std::string kIconBucket =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><path id=\"bucketpaint\" fill=\"#1a1814\" "
        "d=\"M3 5h8.5l-1.1 6.3a1 1 0 0 1-1 .8H5.1a1 1 0 0 1-1-.8L3 5Z\"/>"
        "<path d=\"M2.5 5 4.3 2h5.4l1.8 3\"/>"
        "<circle id=\"bucketdrip\" cx=\"13\" cy=\"12.2\" r=\"1.2\" fill=\"#1a1814\" stroke=\"none\"/></svg>";
    const std::string kIconSelect =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" stroke-dasharray=\"2.3 2\" aria-hidden=\"true\">"
        "<rect x=\"2.5\" y=\"2.5\" width=\"11\" height=\"11\" rx=\"1\"/></svg>";
    const std::string kIconText =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.3\" stroke-linecap=\"round\" aria-hidden=\"true\">"
        "<path d=\"M2.8 3.5h10.4\"/><path d=\"M8 3.5v9\"/></svg>";
    // The standard "highlight colour" glyph everywhere from Word to Google
    // Docs: a capital A with a solid colour bar underneath it.
    const std::string kIconHighlight =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><path d=\"M4 10.5 7 3h2l3 7.5\"/><path d=\"M5.1 8h5.8\"/>"
        "<rect x=\"2.5\" y=\"12.3\" width=\"11\" height=\"1.8\" fill=\"currentColor\" stroke=\"none\"/></svg>";
    // One dial glyph, reused at two sizes for the size/opacity wheels below
    // -- a bigger circle reads as "coarser steps" and a smaller one as
    // "finer steps" on its own, without needing a second icon design to
    // tell them apart (and risk being just as unclear as an abstract pair
    // would be).
    const std::string kIconDial =
        "<svg viewBox=\"0 0 16 16\" width=\"100%\" height=\"100%\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><circle cx=\"8\" cy=\"8\" r=\"6.5\"/><path d=\"M8 2.5v3\"/></svg>";
    // The standard diagonal-dot resize grip every native textarea already
    // uses in its own bottom-right corner.
    const std::string kIconResizeGrip =
        "<svg viewBox=\"0 0 16 16\" width=\"100%\" height=\"100%\" fill=\"currentColor\" aria-hidden=\"true\">"
        "<circle cx=\"12\" cy=\"4\" r=\"1.1\"/><circle cx=\"12\" cy=\"8\" r=\"1.1\"/><circle cx=\"12\" cy=\"12\" r=\"1.1\"/>"
        "<circle cx=\"8\" cy=\"8\" r=\"1.1\"/><circle cx=\"8\" cy=\"12\" r=\"1.1\"/><circle cx=\"4\" cy=\"12\" r=\"1.1\"/></svg>";
    // Glow/Mirror/Fill were the last checkbox-plus-label modifiers left --
    // converted to the same toggle-button pattern as Bold/Italic/Underline
    // now, for the same reason: faster to scan as a row of symbols than a
    // row of checkboxes each needing its own word read next to it.
    const std::string kIconGlow =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><path d=\"M8 1.5v2.4M8 12.1v2.4M1.5 8h2.4M12.1 8h2.4\"/>"
        "<path d=\"M3.8 3.8l1.7 1.7M10.5 10.5l1.7 1.7M12.2 3.8l-1.7 1.7M5.5 10.5l-1.7 1.7\"/>"
        "<circle cx=\"8\" cy=\"8\" r=\"1.8\"/></svg>";
    const std::string kIconMirror =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" "
        "stroke-width=\"1.2\" aria-hidden=\"true\"><path d=\"M8 1v14\" stroke-dasharray=\"2 1.6\"/>"
        "<path d=\"M4 5 2 8l2 3M12 5l2 3-2 3\"/></svg>";
    const std::string kIconFillShape =
        "<svg viewBox=\"0 0 16 16\" width=\"16\" height=\"16\" fill=\"currentColor\" aria-hidden=\"true\">"
        "<rect x=\"3\" y=\"3\" width=\"10\" height=\"10\" rx=\"1.5\"/></svg>";
    std::string title = portrait ? "Self-portrait" : (post_mode ? "Compose a post" : "Canvas");
    std::string intro = portrait
        ? "Draw your self-portrait with the same tools as the profile-background canvas &mdash; "
          "pen, shapes, layers, all of it."
        : post_mode
        ? "Draw, crop, layer, paste in a photo, or drop text on it &mdash; then post it, caption included."
        : "Draw, then save it as your profile background &mdash; a still image, "
          "or a loop of the drawing as it happened.";
    std::string html =
        "<div class=\"card canvascard\"><h1>"+title+"</h1>"
        "<p class=\"muted\">"+intro+"</p>"+errHtml+
        // Everything about "what am I about to draw with" lives in one
        // bounded panel -- tool, then that tool's own properties, then
        // (only for Text) that tool's extra properties -- separate from
        // the panel below it for "what can I do to the canvas as a
        // whole", and separate again from the canvas itself. Three
        // questions, three visually distinct answers, instead of one
        // long run of rows a reader has to parse to find which control
        // belongs to which question.
        // One frame around the whole workspace, not a separate panel
        // floating above the canvas: at desktop widths (see .canvas-frame
        // in css()) this becomes a real toolbox-and-canvas layout -- tools
        // in a column down the left, properties in a strip across the
        // top, canvas and its layer tabs filling the rest -- the same
        // wrap-the-canvas shape a real paint app uses, built as five grid
        // areas placed by CSS rather than by moving any of this markup;
        // below that width it's simply the original stacked column, no
        // media query applies at all.
        "<div class=\"card canvas-frame\">"
        // One toolbar above the drawing surface, four groups in order: tools | size, opacity, colour (and the Text
        // tool's own row) | layers | undo, redo, clipboard (Brandon 2026-11-11, Brief 4 item 7). It wraps at narrow widths.
        "<div class=\"ctoolbar\" role=\"toolbar\" aria-label=\"Drawing tools\">"
        // Three clusters, not one undifferentiated run of eleven icons:
        // freehand media, then shapes, then whole-canvas/utility tools --
        // each its own visual group with a hairline divider between,
        // read the same way a real paint app's toolbar groups by kind.
        "<div class=\"ctgroup ctools\" role=\"group\" aria-label=\"Tools\">"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"pen\" title=\"Pen\" "
        "style=\"padding:.5rem .55rem\">"+kIconPen+"</button>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"marker\" title=\"Marker\" "
        "style=\"padding:.5rem .55rem\">"+kIconMarker+"</button>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"spray\" title=\"Spray\" "
        "style=\"padding:.5rem .55rem\">"+kIconSpray+"</button>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"smudge\" title=\"Smudge\" "
        "style=\"padding:.5rem .55rem\">"+kIconSmudge+"</button>"
        // Eraser gets a colour of its own -- pink, not currentColor --
        // so it reads as visually distinct from the ink-laying tools
        // around it at a glance, not just by its shape.
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"eraser\" title=\"Eraser\" "
        "style=\"padding:.5rem .55rem;color:#ec6fa0\">"+kIconEraser+"</button>"
        "<span class=\"toolgroup-sep\" aria-hidden=\"true\"></span>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"line\" title=\"Line\" "
        "style=\"padding:.5rem .55rem\">"+kIconLine+"</button>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"rect\" title=\"Rectangle\" "
        "style=\"padding:.5rem .55rem\">"+kIconRect+"</button>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"circle\" title=\"Circle\" "
        "style=\"padding:.5rem .55rem\">"+kIconCircle+"</button>"
        "<span class=\"toolgroup-sep\" aria-hidden=\"true\"></span>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"bucket\" title=\"Paint bucket\" "
        "style=\"padding:.5rem .55rem\">"+kIconBucket+"</button>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"select\" title=\"Select\" "
        "style=\"padding:.5rem .55rem\">"+kIconSelect+"</button>"
        "<button type=\"button\" class=\"ghost toolbtn\" data-tool=\"text\" title=\"Text\" "
        "style=\"padding:.5rem .55rem\">"+kIconText+"</button>"
        "</div>"
        // The three property/modifier/text rows below share one wrapping
        // div, not separate grid placements -- multiple items placed at
        // the exact same grid-row/grid-column overlap each other rather
        // than stacking (that's what actually broke the first version of
        // this layout, live), so they need to be one single grid item
        // that stacks its own children by normal block flow internally.
        "<div class=\"ctgroup cprops\" role=\"group\" aria-label=\"Size, opacity and color\">"
        "<div class=\"row\" style=\"flex-wrap:wrap;gap:.4rem 1rem;align-items:center;margin-bottom:.4rem\">"
        "<input type=\"color\" id=\"ccolor\" value=\"#1a1814\" aria-label=\"Color\" "
        "style=\"width:36px;min-height:36px;padding:2px;flex:0 0 auto\">"
        // Coarse/fine wheels instead of a single slider: drag one
        // vertically, or scroll it, and it nudges the number beside it --
        // the bigger circle moves in bigger steps, the smaller one in
        // smaller steps, same size-implies-precision logic as a real
        // jog/shuttle pair, so which is which never needs explaining.
        "<label>Size</label>"
        "<div class=\"row\" style=\"gap:.3rem;align-items:center\">"
        "<div class=\"wheel\" id=\"csizec\" data-target=\"csize\" data-step=\"5\" title=\"Size, coarse (drag or scroll, &plusmn;5)\" "
        "style=\"width:30px;height:30px;border-radius:50%;border:1px solid var(--rule);cursor:ns-resize;"
        "touch-action:none;flex:0 0 auto\">"+kIconDial+"</div>"
        "<div class=\"wheel\" id=\"csizef\" data-target=\"csize\" data-step=\"1\" title=\"Size, fine (drag or scroll, &plusmn;1)\" "
        "style=\"width:20px;height:20px;border-radius:50%;border:1px solid var(--rule);cursor:ns-resize;"
        "touch-action:none;flex:0 0 auto\">"+kIconDial+"</div>"
        "<input type=\"number\" id=\"csize\" min=\"1\" max=\"48\" value=\"8\" style=\"width:4rem\">"
        "</div>"
        "<label>Opacity</label>"
        "<div class=\"row\" style=\"gap:.3rem;align-items:center\">"
        "<div class=\"wheel\" id=\"copacityc\" data-target=\"copacity\" data-step=\"20\" "
        "title=\"Opacity, coarse (drag or scroll, &plusmn;20)\" "
        "style=\"width:30px;height:30px;border-radius:50%;border:1px solid var(--rule);cursor:ns-resize;"
        "touch-action:none;flex:0 0 auto\">"+kIconDial+"</div>"
        "<div class=\"wheel\" id=\"copacityf\" data-target=\"copacity\" data-step=\"5\" "
        "title=\"Opacity, fine (drag or scroll, &plusmn;5)\" "
        "style=\"width:20px;height:20px;border-radius:50%;border:1px solid var(--rule);cursor:ns-resize;"
        "touch-action:none;flex:0 0 auto\">"+kIconDial+"</div>"
        "<input type=\"number\" id=\"copacity\" min=\"5\" max=\"100\" value=\"100\" style=\"width:4rem\">"
        "</div>"
        // Glow/Mirror/Fill are stroke modifiers, not stroke properties,
        // but still belong in this same single strip -- a second row here
        // would make the wrap two rows thick instead of one.
        "<span class=\"toolgroup-sep\" aria-hidden=\"true\"></span>"
        "<button type=\"button\" class=\"ghost fmtbtn\" id=\"cglow\" data-on=\"0\" title=\"Glow\" "
        "style=\"padding:.5rem .55rem\">"+kIconGlow+"</button>"
        "<button type=\"button\" class=\"ghost fmtbtn\" id=\"csymmetry\" data-on=\"0\" title=\"Mirror\" "
        "style=\"padding:.5rem .55rem\">"+kIconMirror+"</button>"
        "<button type=\"button\" class=\"ghost fmtbtn\" id=\"cfill\" data-on=\"0\" title=\"Fill shape\" "
        "style=\"padding:.5rem .55rem\">"+kIconFillShape+"</button>"
        "</div>"
        // Text tool controls -- font/weight/style/underline/background all
        // apply at commit time (see addTextBox), read the same way colour
        // and size already are for every other tool. Drag the Text tool
        // itself on the canvas to size the box: vertical drag sets the
        // font size, horizontal sets the field width text wraps within.
        // Only meaningful for one of eleven tools, so this whole row stays
        // hidden (see refreshToolBtns) until the Text tool is actually
        // selected, instead of cluttering every other tool's controls.
        // Bold/Italic/Underline are their own established symbols --
        // literally the styled B/I/U every text editor uses -- so they're
        // toggle buttons showing that styling directly, not checkboxes
        // with a redundant word next to them; Background gets the
        // standard highlight-colour glyph for the same reason.
        "<div class=\"row\" id=\"ctextrow\" hidden "
        "style=\"flex-wrap:wrap;gap:.4rem 1rem;align-items:center;margin-bottom:.6rem\">"
        "<select id=\"cfont\" style=\"width:auto;min-height:36px\">"
        "<option value=\"sans-serif\">Sans-serif</option>"
        "<option value=\"serif\">Serif</option>"
        "<option value=\"monospace\">Monospace</option>"
        "<option value=\"cursive\">Cursive</option>"
        "</select>"
        "<div class=\"row\" style=\"gap:.3rem\">"
        "<button type=\"button\" class=\"ghost fmtbtn\" id=\"cbold\" data-on=\"0\" title=\"Bold\" "
        "style=\"padding:.5rem .75rem;font-weight:bold\">B</button>"
        "<button type=\"button\" class=\"ghost fmtbtn\" id=\"citalic\" data-on=\"0\" title=\"Italic\" "
        "style=\"padding:.5rem .75rem;font-style:italic\">I</button>"
        "<button type=\"button\" class=\"ghost fmtbtn\" id=\"cunderline\" data-on=\"0\" title=\"Underline\" "
        "style=\"padding:.5rem .75rem;text-decoration:underline\">U</button>"
        "<button type=\"button\" class=\"ghost fmtbtn\" id=\"ctextbg\" data-on=\"0\" title=\"Text background\" "
        "style=\"padding:.5rem .6rem\">"+kIconHighlight+"</button>"
        "<input type=\"color\" id=\"ctextbgcolor\" value=\"#ffffff\" aria-label=\"Text background color\" "
        "style=\"width:36px;min-height:36px;padding:2px;flex:0 0 auto\">"
        "</div>"
        "</div>"
        "</div>"
        // the layer tabs, each over its own opacity slider (Brief 4 item 7: in the toolbar, not beside the canvas)
        "<div class=\"ctgroup clayers\" role=\"group\" aria-label=\"Layers\">"
        "<div style=\"display:flex;flex-direction:column;gap:.15rem;align-items:center\">"
        "<button type=\"button\" class=\"ghost layerbtn\" data-layer=\"0\" title=\"Switch to layer 1\" "
        "style=\"width:2.4rem;padding:.5rem 0\">"+kIconLayer+"</button>"
        "<input type=\"range\" class=\"layeropacity\" data-layer=\"0\" min=\"0\" max=\"100\" value=\"100\" "
        "title=\"Layer 1 opacity\" style=\"width:2.4rem\">"
        "</div>"
        "<div style=\"display:flex;flex-direction:column;gap:.15rem;align-items:center\">"
        "<button type=\"button\" class=\"ghost layerbtn\" data-layer=\"1\" title=\"Switch to layer 2\" "
        "style=\"width:2.4rem;padding:.5rem 0\">"+kIconLayer+"</button>"
        "<input type=\"range\" class=\"layeropacity\" data-layer=\"1\" min=\"0\" max=\"100\" value=\"100\" "
        "title=\"Layer 2 opacity\" style=\"width:2.4rem\">"
        "</div>"
        "<div style=\"display:flex;flex-direction:column;gap:.15rem;align-items:center\">"
        "<button type=\"button\" class=\"ghost layerbtn\" data-layer=\"2\" title=\"Switch to layer 3\" "
        "style=\"width:2.4rem;padding:.5rem 0\">"+kIconLayer+"</button>"
        "<input type=\"range\" class=\"layeropacity\" data-layer=\"2\" min=\"0\" max=\"100\" value=\"100\" "
        "title=\"Layer 3 opacity\" style=\"width:2.4rem\">"
        "</div>"
        "</div>"
        // Undo/redo/clear and the selection actions (copy/cut/paste/crop)
        // complete the frame with a third edge along the canvas's bottom,
        // one row thick like the other two -- both groups act on the
        // canvas as a whole rather than being "what the next stroke looks
        // like", which is why they're grouped apart from the tool/
        // properties edges above rather than mixed into them.
        "<div class=\"ctgroup cedit\" role=\"group\" aria-label=\"Undo and selection\">"
        "<button type=\"button\" class=\"ghost\" id=\"cundo\" title=\"Undo\">&#8617;</button>"
        "<button type=\"button\" class=\"ghost\" id=\"credo\" title=\"Redo\">&#8618;</button>"
        "<button type=\"button\" class=\"ghost\" id=\"cclear\">Clear layer</button>"
        "<span class=\"toolgroup-sep\" aria-hidden=\"true\"></span>"
        // Drag a rectangle with the Select tool first -- these four
        // buttons all act on that same selection (copy/cut sample it,
        // paste drops the last-copied piece at your next click, crop
        // trims the whole canvas down to it). Icons, not labels: each of
        // these already has a symbol everyone recognizes from every other
        // editor (clipboard stack, scissors, crop corners) -- a text
        // label would just be slower to scan than the glyph itself.
        "<button type=\"button\" class=\"ghost\" id=\"ccopy\" title=\"Copy selection\" "
        "style=\"padding:.5rem .55rem\">"+kIconCopy+"</button>"
        "<button type=\"button\" class=\"ghost\" id=\"ccut\" title=\"Cut selection\" "
        "style=\"padding:.5rem .55rem\">"+kIconCut+"</button>"
        "<button type=\"button\" class=\"ghost\" id=\"cpaste\" title=\"Paste\" "
        "style=\"padding:.5rem .55rem\">"+kIconPaste+"</button>"
        "<button type=\"button\" class=\"ghost\" id=\"ccrop\" title=\"Apply crop to selection\" "
        "style=\"padding:.5rem .55rem\">"+kIconCrop+"</button>"
        "</div>"
        "</div>" // end of the toolbar
        // Canvas, with its layer tabs on the right, as the fourth and
        // final area of .canvas-frame -- filling the rest of the frame
        // beside/under the tool column and properties strip above. No
        // numbers on the layer tabs themselves: switching to one and
        // seeing which is active (the highlighted background) is all a
        // tab needs to communicate, and all three do the same job -- a
        // number would just be one more thing to read, not new
        // information. Each still gets its own opacity slider, a real
        // per-layer property applied both live (CSS opacity on that
        // layer's own <canvas>) and in flatten() so a saved/posted image
        // matches what's on screen.
        "<div class=\"ccanvasrow\" style=\"display:flex;gap:.5rem;align-items:flex-start\">"
        "<div id=\"ccanvaswrap\" style=\"position:relative;flex:1;min-width:0;"
        "aspect-ratio:"+aspect+";background:#fff;border:1px solid var(--rule);"
        "border-radius:var(--rsm);overflow:hidden;touch-action:none\">"
        "<canvas id=\"clayer0\" width=\""+cws+"\" height=\""+chs+"\" "
        "style=\"position:absolute;inset:0;width:100%;height:100%\"></canvas>"
        "<canvas id=\"clayer1\" width=\""+cws+"\" height=\""+chs+"\" "
        "style=\"position:absolute;inset:0;width:100%;height:100%\"></canvas>"
        "<canvas id=\"clayer2\" width=\""+cws+"\" height=\""+chs+"\" "
        "style=\"position:absolute;inset:0;width:100%;height:100%\"></canvas>"
        "<canvas id=\"coverlay\" width=\""+cws+"\" height=\""+chs+"\" "
        "style=\"position:absolute;inset:0;width:100%;height:100%;cursor:crosshair\"></canvas>"
        "<div id=\"cresizehandle\" title=\"Drag to resize the canvas\" "
        "style=\"position:absolute;right:0;bottom:0;width:16px;height:16px;padding:2px;"
        "cursor:nwse-resize;z-index:11;touch-action:none;color:var(--stone)\">"+kIconResizeGrip+"</div>"
        "</div>"
        "</div>"
        "</div>";
    if (!portrait) {
        html +=
        "<div class=\"card\">"
        "<div class=\"row\" style=\"flex-wrap:wrap;gap:.4rem 1rem;align-items:center\">"
        "<label class=\"ghost\" style=\"cursor:pointer;padding:.3rem .6rem;border-radius:var(--rsm)\">"
        "Import image/video<input type=\"file\" id=\"cimport\" accept=\"image/*,video/*\" "
        "style=\"display:none\"></label>"
        "</div>"
        "<p class=\"muted\" style=\"margin:.5rem 0 0\">Ctrl+V also pastes an image straight from your "
        "OS clipboard onto the active layer.</p>"
        "</div>";
    } else {
        html += "<p class=\"muted\">Self-portraits are drawn here. Photo upload and paste are off on this page.</p>";
    }
        // A starting texture under whatever gets drawn, the way real paper
        // has a grain before any ink touches it -- four options, Plain
        // (today's blank default) among them, exactly one active at a
        // time like the tool row's own .toolbtn pattern. Lives on
        // #ccanvaswrap's own CSS background (layers stay untouched, fully
        // transparent as always), so it's visible while editing and
        // switchable freely, but NOT yet part of flatten()'s output: a
        // save/post still composites the transparent layers onto a plain
        // white backing, same as before this control existed. Baking a
        // choice into the actual saved pixels is a separate, larger change
        // (drawing real texture onto layer 0 at canvas-open time) --
        // this is deliberately just the live, switchable preview of the idea.
        "<div class=\"row\" style=\"margin-bottom:.5rem;align-items:center;gap:.4rem;flex-wrap:wrap\">"
        "<label class=\"muted\" style=\"font-size:.8rem\">Canvas background</label>"
        "<button type=\"button\" class=\"ghost cbgbtn\" data-bg=\"hatch\">Hatch</button>"
        "<button type=\"button\" class=\"ghost cbgbtn\" data-bg=\"plain\" data-on=\"1\">Plain</button>"
        "<button type=\"button\" class=\"ghost cbgbtn\" data-bg=\"wash\">Wash</button>"
        "<button type=\"button\" class=\"ghost cbgbtn\" data-bg=\"ruled\">Ruled</button>"
        "</div>";
    if (!portrait && !post_mode) {
        html +=
            "<p class=\"muted\" id=\"cframecount\" style=\"margin-top:.6rem\">"+std::to_string(existing_frames)+
            "/24 frames captured</p>"
            "<label style=\"display:block;margin-top:.4rem\"><input type=\"checkbox\" id=\"ctimelapse\""+
            (existing_timelapse ? " checked" : "")+"> "
            "Loop the drawing as my background instead of a still image</label>";
    }
    if (post_mode) {
        html +=
            "<label style=\"display:block;margin-top:.6rem\">Caption</label>"
            "<textarea id=\"cpcaption\" maxlength=\"2000\" placeholder=\"What's on your mind?\"></textarea>";
    }
    std::string saveLabel = portrait ? "Save as self-portrait" : (post_mode ? "Post" : "Save to profile");
    html +=
        "<div class=\"row\" style=\"margin-top:.6rem\">"
        "<button type=\"button\" id=\"csave\">"+saveLabel+"</button>"
        "</div>"
        "<p class=\"muted\" id=\"cstatus\"></p></div>"
        "<script>(function(){"
        "var LAYN=3,layers=[],PX="+std::to_string(px)+";"
        "for(var li=0;li<LAYN;li++){"
        "var lc=document.getElementById('clayer'+li);"
        "layers.push({cv:lc,ctx:lc.getContext('2d'),undo:[],redo:[],opacity:1});"
        "}"
        + (existing_frames == 0 ? "" : std::string(
        // Reopening with a drawing already saved: paint its last frame
        // onto the bottom layer before anything else runs, so editing
        // continues the existing picture instead of starting blank. The
        // eventual Save/first-capture still clears old frame history on
        // the server (see captureFrame's clearedOnce below) -- flatten()
        // by then already includes these loaded pixels plus whatever new
        // marks were added, so nothing already drawn is lost.
        "(function(){"
        "var img=new Image();"
        "img.onload=function(){layers[0].ctx.drawImage(img,0,0,layers[0].cv.width,layers[0].cv.height);};"
        "img.src='/canvas-frame/'+"+js_string_literal(username)+"+'/"+std::to_string(existing_frames-1)+"';"
        "})();"
        )) +
        "document.querySelectorAll('.layeropacity').forEach(function(s){"
        "s.addEventListener('input',function(){"
        "var L=layers[Number(s.dataset.layer)];"
        "L.opacity=Number(s.value)/100;L.cv.style.opacity=L.opacity;"
        "});"
        "});"
        "var CBG={"
        "plain:'#fff',"
        "wash:\"radial-gradient(circle at 30% 35%,rgba(201,168,106,.35) 0%,rgba(201,168,106,0) 65%),\""
        "+\"radial-gradient(circle at 75% 70%,rgba(162,130,59,.28) 0%,rgba(162,130,59,0) 55%),\""
        "+\"radial-gradient(circle at 60% 15%,rgba(110,59,162,.10) 0%,rgba(110,59,162,0) 45%),#f7f1e4\","
        "hatch:\"repeating-linear-gradient(45deg,rgba(26,24,20,.16) 0,rgba(26,24,20,.16) 1px,transparent 1px,transparent 9px),#f7f1e4\","
        "ruled:\"repeating-linear-gradient(to bottom,transparent 0,transparent 17px,rgba(26,24,20,.22) 17px,rgba(26,24,20,.22) 18px),#f7f1e4\""
        "};"
        "var cbgBtns=document.querySelectorAll('.cbgbtn');"
        "cbgBtns.forEach(function(b){"
        "b.addEventListener('click',function(){"
        "cbgBtns.forEach(function(o){o.dataset.on='0';o.style.background='';});"
        "b.dataset.on='1';b.style.background='var(--rule)';"
        "document.getElementById('ccanvaswrap').style.background=CBG[b.dataset.bg];"
        "});"
        "});"
        "cbgBtns.forEach(function(b){if(b.dataset.on==='1')b.style.background='var(--rule)';});"
        "var activeLayer=0;"
        "function acv(){return layers[activeLayer].cv;}"
        "function actx(){return layers[activeLayer].ctx;}"
        "var overlay=document.getElementById('coverlay'),octx=overlay.getContext('2d');"
        "function clearOverlay(){octx.clearRect(0,0,overlay.width,overlay.height);}"
        "var drawing=false,lastX=0,lastY=0,shapeStart=null;"
        "function snap(){"
        "var L=layers[activeLayer];"
        "L.undo.push(L.ctx.getImageData(0,0,L.cv.width,L.cv.height));"
        "if(L.undo.length>20)L.undo.shift();"
        "L.redo=[];"
        "}"
        "function pos(e){"
        "var r=overlay.getBoundingClientRect();var p=e.touches?e.touches[0]:e;"
        "return{x:(p.clientX-r.left)*overlay.width/r.width,y:(p.clientY-r.top)*overlay.height/r.height};"
        "}"
        "var curTool='pen';"
        "function toolVal(){return curTool;}"
        // The bucket icon's own fill tracks whatever colour is loaded,
        // live -- both on page load (the input's own default) and on
        // every change, so the tool always shows what it's about to pour.
        "function syncBucketColor(){"
        "var c=document.getElementById('ccolor').value;"
        "var p=document.getElementById('bucketpaint'),d=document.getElementById('bucketdrip');"
        "if(p)p.setAttribute('fill',c);if(d)d.setAttribute('fill',c);"
        "}"
        "document.getElementById('ccolor').addEventListener('input',syncBucketColor);"
        "syncBucketColor();"
        "var toolBtns=document.querySelectorAll('.toolbtn');"
        "function refreshToolBtns(){"
        "toolBtns.forEach(function(b){b.style.background=b.dataset.tool===curTool?'var(--rule)':'';});"
        // .row{display:flex} outranks the browser's default [hidden]{display:
        // none} in the cascade (same specificity, .row comes later in the
        // stylesheet), so the hidden attribute alone silently does nothing
        // on an element that also has class="row" -- toggling style.display
        // directly sidesteps that instead of fighting the cascade.
        "document.getElementById('ctextrow').style.display=curTool==='text'?'flex':'none';"
        "}"
        "toolBtns.forEach(function(b){"
        "b.addEventListener('click',function(){curTool=b.dataset.tool;refreshToolBtns();});"
        "});"
        "refreshToolBtns();"
        "document.querySelectorAll('.fmtbtn').forEach(function(b){"
        "b.addEventListener('click',function(){"
        "var on=b.dataset.on==='1';b.dataset.on=on?'0':'1';b.style.background=on?'':'var(--rule)';"
        "});"
        "});"
        "function drawSeg(ax,ay,bx,by){"
        "var b=toolVal(),color=document.getElementById('ccolor').value;"
        "var size=Number(document.getElementById('csize').value)*PX;"
        "var opacity=Number(document.getElementById('copacity').value)/100;"
        "var glow=document.getElementById('cglow').dataset.on==='1';"
        "var cx=actx();"
        "cx.save();"
        "if(glow){cx.shadowColor=color;cx.shadowBlur=size*1.4;}"
        "if(b==='eraser'){"
        "cx.globalCompositeOperation='destination-out';cx.globalAlpha=1;"
        "cx.lineCap='round';cx.lineJoin='round';cx.lineWidth=size*2;"
        "cx.beginPath();cx.moveTo(ax,ay);cx.lineTo(bx,by);cx.stroke();"
        "}else if(b==='spray'){"
        "cx.globalAlpha=opacity;cx.fillStyle=color;"
        "var n=Math.max(6,Math.floor(size));"
        "for(var i=0;i<n;i++){"
        "var ang=Math.random()*Math.PI*2,rad=Math.random()*size;"
        "cx.beginPath();cx.arc(bx+Math.cos(ang)*rad,by+Math.sin(ang)*rad,PX,0,Math.PI*2);cx.fill();"
        "}"
        "}else{"
        "cx.lineCap='round';cx.lineJoin='round';cx.strokeStyle=color;"
        "if(b==='marker'){cx.lineWidth=size*1.6;cx.globalAlpha=Math.min(opacity,.45);}"
        "else{cx.lineWidth=size;cx.globalAlpha=opacity;}"
        "cx.beginPath();cx.moveTo(ax,ay);cx.lineTo(bx,by);cx.stroke();"
        "}"
        "cx.restore();"
        "}"
        "function strokeAt(x0,y0,x1,y1){"
        "drawSeg(x0,y0,x1,y1);"
        "if(document.getElementById('csymmetry').dataset.on==='1')"
        "drawSeg(acv().width-x0,y0,acv().width-x1,y1);"
        "}"
        // Shared by the live drag preview (drawn onto the overlay) and the
        // final commit (drawn onto the active layer) -- same geometry
        // either way, only the target context differs.
        "function drawShape(x0,y0,x1,y1,targetCtx){"
        "var t=toolVal(),color=document.getElementById('ccolor').value;"
        "var size=Number(document.getElementById('csize').value)*PX;"
        "var opacity=Number(document.getElementById('copacity').value)/100;"
        "var fill=document.getElementById('cfill').dataset.on==='1';"
        "var glow=document.getElementById('cglow').dataset.on==='1';"
        "targetCtx.save();"
        "targetCtx.globalAlpha=opacity;targetCtx.strokeStyle=color;targetCtx.fillStyle=color;"
        "targetCtx.lineWidth=size;targetCtx.lineCap='round';targetCtx.lineJoin='round';"
        "if(glow){targetCtx.shadowColor=color;targetCtx.shadowBlur=size*1.4;}"
        "if(t==='line'){"
        "targetCtx.beginPath();targetCtx.moveTo(x0,y0);targetCtx.lineTo(x1,y1);targetCtx.stroke();"
        "}else if(t==='rect'){"
        "var rx=Math.min(x0,x1),ry=Math.min(y0,y1),rw=Math.abs(x1-x0),rh=Math.abs(y1-y0);"
        "if(fill)targetCtx.fillRect(rx,ry,rw,rh);else targetCtx.strokeRect(rx,ry,rw,rh);"
        "}else if(t==='circle'){"
        "var mx=(x0+x1)/2,my=(y0+y1)/2,rxr=Math.abs(x1-x0)/2,ryr=Math.abs(y1-y0)/2;"
        "targetCtx.beginPath();targetCtx.ellipse(mx,my,rxr,ryr,0,0,Math.PI*2);"
        "if(fill)targetCtx.fill();else targetCtx.stroke();"
        "}"
        "targetCtx.restore();"
        "}"
        // Iterative 4-way flood fill on the active layer only, with a small
        // colour-distance tolerance so anti-aliased stroke edges don't
        // leave a ring of unfilled pixels around the bucket click.
        "function floodFill(fx,fy){"
        "var c=acv(),cx=actx();fx=Math.floor(fx);fy=Math.floor(fy);"
        "if(fx<0||fy<0||fx>=c.width||fy>=c.height)return;"
        "var img=cx.getImageData(0,0,c.width,c.height),data=img.data,w=c.width,h=c.height;"
        "var startI=(fy*w+fx)*4;"
        "var tr=data[startI],tg=data[startI+1],tb=data[startI+2],ta=data[startI+3];"
        "var hex=document.getElementById('ccolor').value;"
        "var opacity=Number(document.getElementById('copacity').value)/100;"
        "var fr=parseInt(hex.slice(1,3),16),fg=parseInt(hex.slice(3,5),16),fb=parseInt(hex.slice(5,7),16);"
        "var fa=Math.round(opacity*255);"
        "if(tr===fr&&tg===fg&&tb===fb&&ta===fa)return;"
        "var tol=32,tol2=tol*tol*4;"
        "function matches(i){"
        "var dr=data[i]-tr,dg=data[i+1]-tg,db=data[i+2]-tb,da=data[i+3]-ta;"
        "return dr*dr+dg*dg+db*db+da*da<=tol2;"
        "}"
        "var visited=new Uint8Array(w*h);visited[fy*w+fx]=1;"
        "var stack=[startI];"
        "while(stack.length){"
        "var i=stack.pop();data[i]=fr;data[i+1]=fg;data[i+2]=fb;data[i+3]=fa;"
        "var px=(i/4)|0,x=px%w,y=(px/w)|0;"
        "var nbrs=[[x-1,y],[x+1,y],[x,y-1],[x,y+1]];"
        "for(var k=0;k<4;k++){"
        "var nx=nbrs[k][0],ny=nbrs[k][1];"
        "if(nx<0||ny<0||nx>=w||ny>=h)continue;"
        "var vi=ny*w+nx;if(visited[vi])continue;"
        "var ni=vi*4;"
        "if(matches(ni)){visited[vi]=1;stack.push(ni);}"
        "}"
        "}"
        "cx.putImageData(img,0,0);"
        "}"
        // Smudge: sample a small patch of the active layer around the
        // stroke's previous point and re-draw it, partially transparent,
        // at the new point -- dragging existing pixels along the path
        // instead of laying down a fresh colour.
        "var smudgeBuf=document.createElement('canvas');"
        "function smudgeAt(x,y){"
        "var cx=actx(),c=acv();"
        "var r=Math.max(4,Number(document.getElementById('csize').value))*PX;"
        "var sx=Math.max(0,Math.floor(lastX-r)),sy=Math.max(0,Math.floor(lastY-r));"
        "var sw=Math.min(c.width-sx,r*2),sh=Math.min(c.height-sy,r*2);"
        "if(sw<=0||sh<=0)return;"
        "smudgeBuf.width=sw;smudgeBuf.height=sh;"
        "var sctx=smudgeBuf.getContext('2d');sctx.clearRect(0,0,sw,sh);"
        "sctx.drawImage(c,sx,sy,sw,sh,0,0,sw,sh);"
        "cx.save();cx.globalAlpha=.45;"
        "cx.drawImage(smudgeBuf,0,0,sw,sh,x-r,y-r,sw,sh);"
        "cx.restore();"
        "}"
        // Select: drag a marquee (no pixel change of its own -- just
        // records selRect and draws a dashed outline). Copy/Cut sample
        // that rectangle from the active layer into an internal clip
        // canvas; Paste arms a "next click places it" mode rather than
        // dropping it at a fixed spot, since a canvas has no cursor to
        // paste "at" the way a text editor does.
        "var selRect=null,clip=null,pastePending=false;"
        "function rectFrom(a,b){return{x:Math.min(a.x,b.x),y:Math.min(a.y,b.y),w:Math.abs(b.x-a.x),h:Math.abs(b.y-a.y)};}"
        "function drawMarquee(r){octx.save();octx.strokeStyle='#000';octx.lineWidth=PX;octx.setLineDash([5*PX,4*PX]);"
        "octx.strokeRect(Math.round(r.x)+.5,Math.round(r.y)+.5,Math.round(r.w),Math.round(r.h));octx.restore();}"
        "function copySel(){"
        "if(!selRect||selRect.w<1||selRect.h<1)return;"
        "var c=document.createElement('canvas');c.width=Math.round(selRect.w);c.height=Math.round(selRect.h);"
        "c.getContext('2d').drawImage(acv(),selRect.x,selRect.y,c.width,c.height,0,0,c.width,c.height);"
        "clip=c;"
        "}"
        "document.getElementById('ccopy').addEventListener('click',copySel);"
        "document.getElementById('ccut').addEventListener('click',function(){"
        "if(!selRect||selRect.w<1||selRect.h<1)return;"
        "copySel();snap();actx().clearRect(selRect.x,selRect.y,selRect.w,selRect.h);captureFrame(false);"
        "});"
        "document.getElementById('cpaste').addEventListener('click',function(){if(clip)pastePending=true;});"
        "function placeClipboardAt(x,y){"
        "if(!clip)return;snap();actx().drawImage(clip,x-clip.width/2,y-clip.height/2);captureFrame(false);"
        "}"
        // Crop trims every layer's canvas down to the selection (sampled
        // BEFORE each layer's own canvas is resized, since resizing a
        // <canvas> element's width/height attribute always clears it).
        // Resize instead scales each layer's whole existing content into
        // a new canvas size -- no selection involved.
        "function resizeAllLayers(nw,nh,sampleRect){"
        "layers.forEach(function(L){"
        "var tmp=document.createElement('canvas');"
        "if(sampleRect){"
        "tmp.width=nw;tmp.height=nh;"
        "tmp.getContext('2d').drawImage(L.cv,sampleRect.x,sampleRect.y,nw,nh,0,0,nw,nh);"
        "}else{"
        "tmp.width=L.cv.width;tmp.height=L.cv.height;tmp.getContext('2d').drawImage(L.cv,0,0);"
        "}"
        "L.cv.width=nw;L.cv.height=nh;"
        "if(sampleRect)L.ctx.drawImage(tmp,0,0);else L.ctx.drawImage(tmp,0,0,tmp.width,tmp.height,0,0,nw,nh);"
        "L.undo=[];L.redo=[];"
        "});"
        "overlay.width=nw;overlay.height=nh;"
        "var wrap=document.getElementById('ccanvaswrap');"
        "wrap.style.maxWidth=(nw/PX)+'px';wrap.style.aspectRatio=nw+'/'+nh;"
        "selRect=null;clearOverlay();captureFrame(false);"
        "}"
        "document.getElementById('ccrop').addEventListener('click',function(){"
        "if(!selRect||selRect.w<2||selRect.h<2)return;"
        "resizeAllLayers(Math.round(selRect.w),Math.round(selRect.h),"
        "{x:Math.round(selRect.x),y:Math.round(selRect.y)});"
        "});"
        // Drag the corner grip to resize the canvas -- live CSS-only
        // preview while dragging (cheap: just the wrap's own box, the
        // actual bitmaps untouched so the existing drawing visibly
        // stretches to match, same as dragging an image placeholder in
        // any editor), then the real per-layer resize commits once on
        // release via the same resizeAllLayers() the old numeric-input
        // form used.
        "(function(){"
        "var handle=document.getElementById('cresizehandle'),wrap=document.getElementById('ccanvaswrap');"
        "var dragging=false,startX=0,startY=0,startCw=0,startCh=0,cssToCanvas=1;"
        "handle.addEventListener('pointerdown',function(e){"
        "dragging=true;startX=e.clientX;startY=e.clientY;"
        "var r=wrap.getBoundingClientRect();"
        "startCw=r.width;startCh=r.height;cssToCanvas=acv().width/r.width;"
        "handle.setPointerCapture(e.pointerId);e.preventDefault();e.stopPropagation();"
        "});"
        "handle.addEventListener('pointermove',function(e){"
        "if(!dragging)return;"
        "var nw=Math.max(40,startCw+(e.clientX-startX));"
        "var nh=Math.max(40,startCh+(e.clientY-startY));"
        "wrap.style.maxWidth=nw+'px';wrap.style.aspectRatio=nw+'/'+nh;"
        "e.preventDefault();e.stopPropagation();"
        "});"
        "handle.addEventListener('pointerup',function(e){"
        "if(!dragging)return;dragging=false;"
        "var nw=Math.max(16,Math.min(2000,Math.round((startCw+(e.clientX-startX))*cssToCanvas)));"
        "var nh=Math.max(16,Math.min(2000,Math.round((startCh+(e.clientY-startY))*cssToCanvas)));"
        "resizeAllLayers(nw,nh,null);"
        "e.preventDefault();e.stopPropagation();"
        "});"
        "})();"
        // Coarse/fine wheels: drag vertically or scroll over one to nudge
        // its target number input by its own step, clamped to that
        // input's own min/max. A little rotation on each notch is purely
        // cosmetic feedback that the wheel actually registered the input.
        "document.querySelectorAll('.wheel').forEach(function(w){"
        "var target=document.getElementById(w.dataset.target),step=Number(w.dataset.step),rotation=0;"
        "function apply(dir){"
        "var min=Number(target.min)||0,max=Number(target.max)||9999;"
        "target.value=Math.max(min,Math.min(max,Number(target.value)+dir*step));"
        "rotation+=dir*20;w.style.transform='rotate('+rotation+'deg)';"
        "}"
        "w.addEventListener('wheel',function(e){e.preventDefault();apply(e.deltaY<0?1:-1);},{passive:false});"
        "var dragging=false,lastY=0;"
        "w.addEventListener('pointerdown',function(e){"
        "dragging=true;lastY=e.clientY;w.setPointerCapture(e.pointerId);e.preventDefault();"
        "});"
        "w.addEventListener('pointermove',function(e){"
        "if(!dragging)return;"
        "var dy=lastY-e.clientY;"
        "if(Math.abs(dy)>=6){apply(dy>0?1:-1);lastY=e.clientY;}"
        "});"
        "w.addEventListener('pointerup',function(){dragging=false;});"
        "});"
        // Text: drag the Text tool on the canvas -- vertical distance
        // sets the font size, horizontal sets the field width the text
        // wraps within (a plain click falls back to a sensible default
        // box instead of doing nothing). Opens a real <textarea> over the
        // canvas, styled live from the font/bold/italic/underline
        // controls, so what you see while typing already looks like what
        // commit() is about to bake onto the active layer as pixels
        // (word-wrapped to the box width, underline drawn by hand since
        // canvas text has no native underline, background filled first if
        // enabled) -- blur commits, Escape cancels without committing.
        "function addTextBox(x,y,boxW,fontPx){"
        "var r=overlay.getBoundingClientRect(),scale=r.width/overlay.width;"
        "var ta=document.createElement('textarea');"
        "ta.placeholder='Text';"
        "var bold=document.getElementById('cbold').dataset.on==='1';"
        "var italic=document.getElementById('citalic').dataset.on==='1';"
        "var underline=document.getElementById('cunderline').dataset.on==='1';"
        "var fam=document.getElementById('cfont').value;"
        "var color=document.getElementById('ccolor').value;"
        // width/min-height:auto overrides the site-wide
        // input,textarea{width:100%;min-height:44px} rule (meant for real
        // form fields), which would otherwise stretch this floating box
        // across the whole canvas -- the same class of bug already hit
        // input[type=color]/[type=checkbox] once each.
        "ta.style.cssText='position:absolute;z-index:10;width:auto;min-height:0;resize:both;overflow:auto;'+"
        "'padding:2px;box-sizing:border-box;background:rgba(255,255,255,.85);'+"
        "'border:1px dashed '+color+';color:'+color+';font-family:'+fam+';'+"
        "'font-size:'+(fontPx*scale)+'px;'+(bold?'font-weight:bold;':'')+(italic?'font-style:italic;':'')+"
        "(underline?'text-decoration:underline;':'');"
        "ta.style.left=(x*scale)+'px';ta.style.top=(y*scale)+'px';"
        "ta.style.width=(boxW*scale)+'px';ta.style.height=Math.max(fontPx*1.6,40*PX)*scale+'px';"
        "document.getElementById('ccanvaswrap').appendChild(ta);"
        "ta.focus();"
        "var done=false;"
        "function commit(){"
        "if(done)return;done=true;"
        "var val=ta.value;ta.remove();"
        "if(!val)return;"
        "snap();var cx=actx();cx.save();"
        "cx.font=(italic?'italic ':'')+(bold?'bold ':'')+fontPx+'px '+fam;"
        "cx.textBaseline='top';"
        "var lineH=fontPx*1.25,lines=[];"
        "val.split('\\n').forEach(function(paragraph){"
        "var words=paragraph.split(' '),cur='';"
        "words.forEach(function(w){"
        "var test=cur?cur+' '+w:w;"
        "if(cur&&cx.measureText(test).width>boxW){lines.push(cur);cur=w;}else{cur=test;}"
        "});"
        "lines.push(cur);"
        "});"
        "if(document.getElementById('ctextbg').dataset.on==='1'){"
        "cx.fillStyle=document.getElementById('ctextbgcolor').value;"
        "cx.fillRect(x,y,boxW,lines.length*lineH);"
        "}"
        "cx.fillStyle=color;"
        "lines.forEach(function(line,i){"
        "var ly=y+i*lineH;cx.fillText(line,x,ly);"
        "if(underline){"
        "var w=cx.measureText(line).width;"
        "cx.fillRect(x,ly+fontPx*.92,w,Math.max(1,fontPx*.06));"
        "}"
        "});"
        "cx.restore();"
        "captureFrame(false);"
        "}"
        "ta.addEventListener('keydown',function(e){if(e.key==='Escape'){done=true;ta.remove();}});"
        "ta.addEventListener('blur',commit);"
        "}"
        // Paste from the OS clipboard (e.g. a screenshot copied outside
        // the browser) -- separate from the internal Copy/Cut/Paste
        // above, which only ever moves pixels around inside this canvas.
        "document.addEventListener('paste',function(e){"
        + std::string(portrait ? "return;" : "") +
        "var items=(e.clipboardData||{}).items||[];"
        "for(var i=0;i<items.length;i++){"
        "if(items[i].type.indexOf('image')!==0)continue;"
        "var file=items[i].getAsFile();if(!file)continue;"
        "var url=URL.createObjectURL(file),img=new Image();"
        "img.onload=function(){"
        "snap();var cvw=acv().width,cvh=acv().height;"
        "var scale=Math.min(cvw/img.width,cvh/img.height,1);"
        "var dw=img.width*scale,dh=img.height*scale;"
        "actx().drawImage(img,(cvw-dw)/2,(cvh-dh)/2,dw,dh);"
        "captureFrame(false);URL.revokeObjectURL(url);"
        "};"
        "img.src=url;e.preventDefault();break;"
        "}"
        "});"
        "overlay.addEventListener('pointerdown',function(e){"
        "var t=toolVal(),p=pos(e);"
        "if(pastePending){pastePending=false;placeClipboardAt(p.x,p.y);e.preventDefault();return;}"
        "if(t==='text'||t==='select'){shapeStart=p;e.preventDefault();return;}"
        "if(t==='bucket'){snap();floodFill(p.x,p.y);captureFrame(false);e.preventDefault();return;}"
        "if(t==='line'||t==='rect'||t==='circle'){snap();shapeStart=p;e.preventDefault();return;}"
        "snap();drawing=true;lastX=p.x;lastY=p.y;"
        "if(t!=='smudge')strokeAt(p.x,p.y,p.x+.1,p.y+.1);"
        "e.preventDefault();"
        "});"
        "overlay.addEventListener('pointermove',function(e){"
        "var t=toolVal(),p=pos(e);"
        "if(shapeStart&&(t==='select'||t==='text')){clearOverlay();drawMarquee(rectFrom(shapeStart,p));e.preventDefault();return;}"
        "if(shapeStart){clearOverlay();drawShape(shapeStart.x,shapeStart.y,p.x,p.y,octx);e.preventDefault();return;}"
        "if(!drawing)return;"
        "if(t==='smudge')smudgeAt(p.x,p.y);else strokeAt(lastX,lastY,p.x,p.y);"
        "lastX=p.x;lastY=p.y;e.preventDefault();"
        "});"
        "window.addEventListener('pointerup',function(e){"
        "if(shapeStart&&toolVal()==='select'){"
        "var p=pos(e);var r=rectFrom(shapeStart,p);clearOverlay();"
        "if(r.w>=2&&r.h>=2){selRect=r;drawMarquee(r);}"
        "shapeStart=null;return;"
        "}"
        "if(shapeStart&&toolVal()==='text'){"
        "var p=pos(e);var r=rectFrom(shapeStart,p);var start=shapeStart;clearOverlay();shapeStart=null;"
        // A near-zero drag (effectively just a click) falls back to a
        // sensible default box instead of creating an unusable sliver.
        "var fontPx=r.h>=6*PX?Math.round(r.h):24*PX;"
        "var boxW=r.w>=20*PX?Math.round(r.w):200*PX;"
        "var tx=r.w>=6*PX?r.x:start.x,ty=r.h>=6*PX?r.y:start.y;"
        "addTextBox(tx,ty,boxW,fontPx);"
        "return;"
        "}"
        "if(shapeStart){"
        "var p=pos(e);clearOverlay();drawShape(shapeStart.x,shapeStart.y,p.x,p.y,actx());"
        "shapeStart=null;captureFrame(false);return;"
        "}"
        "if(!drawing)return;drawing=false;captureFrame(false);"
        "});"
        "var layerBtns=document.querySelectorAll('.layerbtn');"
        "function refreshLayerBtns(){"
        "layerBtns.forEach(function(b){"
        "b.style.background=Number(b.dataset.layer)===activeLayer?'var(--rule)':'';"
        "});"
        "}"
        "layerBtns.forEach(function(b){"
        "b.addEventListener('click',function(){activeLayer=Number(b.dataset.layer);refreshLayerBtns();});"
        "});"
        "refreshLayerBtns();";
    if (!portrait) {
        html +=
        "document.getElementById('cimport').addEventListener('change',function(e){"
        "var f=e.target.files[0];if(!f)return;"
        "var status=document.getElementById('cstatus');"
        "function place(imgLike,iw,ih){"
        "if(!iw||!ih){status.textContent='Could not read that file.';return;}"
        "snap();"
        "var cvw=acv().width,cvh=acv().height;"
        "var scale=Math.min(cvw/iw,cvh/ih);"
        "var dw=iw*scale,dh=ih*scale,dx=(cvw-dw)/2,dy=(cvh-dh)/2;"
        "actx().drawImage(imgLike,dx,dy,dw,dh);"
        "captureFrame(false);status.textContent='';"
        "}"
        "if(f.type.indexOf('video')===0){"
        "status.textContent='Reading video frame\\u2026';"
        "var url=URL.createObjectURL(f);"
        "var vid=document.createElement('video');"
        "vid.muted=true;vid.playsInline=true;vid.src=url;"
        "vid.addEventListener('loadeddata',function(){"
        "try{vid.currentTime=Math.min(.1,vid.duration||0);}catch(err){}"
        "});"
        "vid.addEventListener('seeked',function(){"
        "place(vid,vid.videoWidth,vid.videoHeight);URL.revokeObjectURL(url);"
        "});"
        "vid.addEventListener('error',function(){"
        "status.textContent='Could not read that video.';URL.revokeObjectURL(url);"
        "});"
        "}else{"
        "status.textContent='Loading image\\u2026';"
        "var reader=new FileReader();"
        "reader.onload=function(ev){"
        "var img=new Image();"
        "img.onload=function(){place(img,img.naturalWidth,img.naturalHeight);};"
        "img.onerror=function(){status.textContent='That file is not a usable image.';};"
        "img.src=ev.target.result;"
        "};"
        "reader.readAsDataURL(f);"
        "}"
        "e.target.value='';"
        "});";
    }
    html +=
        "function flatten(){"
        "var out=document.createElement('canvas');"
        "out.width=layers[0].cv.width;out.height=layers[0].cv.height;"
        "var octx2=out.getContext('2d');"
        "octx2.fillStyle='#fff';octx2.fillRect(0,0,out.width,out.height);"
        "for(var i=0;i<layers.length;i++){"
        "octx2.globalAlpha=layers[i].opacity;octx2.drawImage(layers[i].cv,0,0);"
        "}"
        "octx2.globalAlpha=1;"
        "return out;"
        "}"
        "document.getElementById('cundo').addEventListener('click',function(){"
        "var L=layers[activeLayer];if(!L.undo.length)return;"
        "L.redo.push(L.ctx.getImageData(0,0,L.cv.width,L.cv.height));"
        "L.ctx.putImageData(L.undo.pop(),0,0);"
        "});"
        "document.getElementById('credo').addEventListener('click',function(){"
        "var L=layers[activeLayer];if(!L.redo.length)return;"
        "L.undo.push(L.ctx.getImageData(0,0,L.cv.width,L.cv.height));"
        "L.ctx.putImageData(L.redo.pop(),0,0);"
        "});"
        "document.getElementById('cclear').addEventListener('click',function(){"
        "snap();var L=layers[activeLayer];L.ctx.clearRect(0,0,L.cv.width,L.cv.height);"
        "});";
    if (portrait) {
        // No frame history for a self-portrait -- captureFrame is called
        // from the shared pointer/import handlers above regardless of
        // mode, so it stays a real function here too, just a no-op; Save
        // does the only upload, straight to the existing /me/avatar route.
        html +=
            "function captureFrame(force,done){if(done)done();}"
            "document.getElementById('csave').addEventListener('click',function(){"
            "var status=document.getElementById('cstatus');status.textContent='Saving\\u2026';"
            "flatten().toBlob(function(blob){"
            "var fd=new FormData();fd.append('photo',blob,'portrait.png');"
            "fetch('/me/avatar',{method:'POST',body:fd}).then(function(r){"
            "window.location=r.url||'/me';"
            "}).catch(function(){status.textContent='Could not save. Try again.';});"
            "},'image/png');"
            "});";
    } else if (post_mode) {
        // No frame history for a post either -- one flattened image, paired
        // with the caption, POSTed as one multipart request straight to
        // whichever composer route this page was opened for. r.url is the
        // real final URL fetch landed on after following that route's own
        // redirect, so a media error still surfaces as that route's own
        // ?err= message -- same pattern as the self-portrait Save above.
        html +=
            "function captureFrame(force,done){if(done)done();}"
            "document.getElementById('csave').addEventListener('click',function(){"
            "var status=document.getElementById('cstatus');status.textContent='Posting\\u2026';"
            "flatten().toBlob(function(blob){"
            "var fd=new FormData();"
            "fd.append('text',document.getElementById('cpcaption').value);"
            "fd.append('media',blob,'post.png');"
            "fetch("+js_string_literal(post_target)+",{method:'POST',body:fd}).then(function(r){"
            "window.location=r.url||"+js_string_literal(post_target)+";"
            "}).catch(function(){status.textContent='Could not post. Try again.';});"
            "},'image/png');"
            "});";
    } else {
        html +=
            "var clearedOnce=false,frameCount="+std::to_string(existing_frames)+";"
            "function updateCounter(){"
            "document.getElementById('cframecount').textContent=frameCount+'/24 frames captured';"
            "}"
            // done() always fires, whether this capture actually uploaded
            // or was skipped (cap hit, no force) -- callers that need to
            // know the frame really landed (Save, below) depend on that.
            "function captureFrame(force,done){"
            "if(frameCount>=23&&!force){if(done)done();return;}"
            "flatten().toBlob(function(blob){"
            "var fd=new FormData();fd.append('frame',blob,'frame.png');"
            "function go(){"
            "fetch('/me/canvas/frame',{method:'POST',body:fd}).then(function(r){"
            "if(r.ok){frameCount++;updateCounter();}"
            "if(done)done();"
            "}).catch(function(){if(done)done();});"
            "}"
            "if(!clearedOnce){"
            "clearedOnce=true;"
            "fetch('/me/canvas/clear',{method:'POST'}).then(go).catch(go);"
            "}else{go();}"
            "},'image/png');"
            "}"
            "document.getElementById('csave').addEventListener('click',function(){"
            "var status=document.getElementById('cstatus');status.textContent='Saving\\u2026';"
            "var timelapse=document.getElementById('ctimelapse').checked?'1':'0';"
            "captureFrame(true,function(){"
            "fetch('/me/canvas/mode',{method:'POST',"
            "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
            "body:'timelapse='+timelapse}).then(function(){"
            "window.location='/u/'+"+js_string_literal(username)+";"
            "});"
            "});"
            "});";
    }
    html += "})();</script>";
    return html;
}

// World client page: a <canvas>, WASD/arrow movement, one WebSocket to
// /world/ws. Sends {"x":n,"y":n} on movement; server broadcasts the full
// player snapshot back on an interval regardless of whether this client
// moved, so remote players animate smoothly too. Matches the documented
// MVP: "One shared room, two people see each other move."
std::string world_page(const std::string& username) {
    std::string html =
        "<div class=\"card\"><h1>World</h1>"
        "<p class=\"muted\">One shared room. Arrow keys or WASD to move. Closing this page removes you.</p>"
        "<canvas id=\"wcanvas\" width=\"800\" height=\"500\" "
        "style=\"border:1px solid var(--rule);border-radius:var(--rsm);background:#eee7d6;"
        "max-width:100%;touch-action:none\"></canvas>"
        "<p class=\"muted\" id=\"wstatus\">Connecting&hellip;</p></div>"
        "<script>(function(){"
        "var me="+js_string_literal(username)+";"
        "var cv=document.getElementById('wcanvas'),ctx=cv.getContext('2d');"
        "var st=document.getElementById('wstatus');"
        "var players={};"
        "var proto=location.protocol==='https:'?'wss:':'ws:';"
        "var ws=new WebSocket(proto+'//'+location.host+'/world/ws');"
        "var keys={};"
        "var opened=false;"
        "ws.onopen=function(){opened=true;st.textContent='Connected. Arrow keys or WASD to move.';};"
        "ws.onclose=function(){"
        "st.textContent=opened?'Disconnected. Reload to rejoin.':'Could not reach the room. Reload to try again.';"
        "};"
        "ws.onerror=function(){"
        "if(!opened)st.textContent='Could not reach the room. Reload to try again.';"
        "};"
        "setTimeout(function(){"
        "if(!opened)st.textContent='Still connecting. If this hangs, reload.';"
        "},4000);"
        "ws.onmessage=function(ev){"
        "try{var data=JSON.parse(ev.data);players=data.players||{};}catch(e){}"
        "draw();"
        "};"
        "function draw(){"
        "ctx.clearRect(0,0,cv.width,cv.height);"
        "Object.keys(players).forEach(function(id){"
        "var p=players[id];"
        "ctx.beginPath();ctx.arc(p.x,p.y,14,0,Math.PI*2);"
        "ctx.fillStyle=p.color;ctx.fill();"
        "if(p.username===me){ctx.lineWidth=2;ctx.strokeStyle='#1a1814';ctx.stroke();}"
        "ctx.fillStyle='#1a1814';ctx.font='12px sans-serif';ctx.textAlign='center';"
        "ctx.fillText(p.username,p.x,p.y-20);"
        "});"
        "}"
        "document.addEventListener('keydown',function(e){keys[e.key.toLowerCase()]=true;});"
        "document.addEventListener('keyup',function(e){keys[e.key.toLowerCase()]=false;});"
        "var x=cv.width/2,y=cv.height/2;"
        "setInterval(function(){"
        "if(ws.readyState!==1)return;"
        "var dx=0,dy=0,speed=5;"
        "if(keys['arrowleft']||keys['a'])dx-=speed;"
        "if(keys['arrowright']||keys['d'])dx+=speed;"
        "if(keys['arrowup']||keys['w'])dy-=speed;"
        "if(keys['arrowdown']||keys['s'])dy+=speed;"
        "if(dx||dy){"
        "x=Math.max(14,Math.min(cv.width-14,x+dx));"
        "y=Math.max(14,Math.min(cv.height-14,y+dy));"
        "ws.send(JSON.stringify({x:x,y:y}));"
        "}"
        "},50);"
        "})();</script>";
    return html;
}

// One thread per connected /world/ws client. Runs the RFC 6455 handshake
// response, then loops: try to read this client's new position (short
// timeout so the loop can broadcast even when nobody moved), update the
// shared registry, and push everyone's current positions back to this
// client. Removes the client from the registry on disconnect.
void run_world_session(int fd, std::string username) {
    timeval tv{}; tv.tv_sec = 0; tv.tv_usec = 100000; // 100ms
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    {
        std::lock_guard<std::mutex> lock(g_world_mu);
        WorldPlayer p;
        p.username = username;
        p.color = world_color_for(username);
        g_world_players[fd] = p;
    }

    for (;;) {
        auto result = ws_read_frame(fd);
        if (result.status == WsReadStatus::Closed) break;
        if (result.status == WsReadStatus::Ok) {
            if (result.frame.opcode == WsOpcode::Close) break;
            if (result.frame.opcode == WsOpcode::Text) {
                // Minimal ad-hoc parse of {"x":<num>,"y":<num>} — the only
                // shape this endpoint ever receives, so a general JSON
                // parser would be pure overhead here.
                const std::string& s = result.frame.payload;
                auto extract = [&](const char* key) -> std::optional<double> {
                    auto p = s.find(key);
                    if (p == std::string::npos) return std::nullopt;
                    p = s.find(':', p);
                    if (p == std::string::npos) return std::nullopt;
                    try { return std::stod(s.substr(p + 1)); } catch (...) { return std::nullopt; }
                };
                auto nx = extract("\"x\"");
                auto ny = extract("\"y\"");
                if (nx && ny) {
                    std::lock_guard<std::mutex> lock(g_world_mu);
                    auto it = g_world_players.find(fd);
                    if (it != g_world_players.end()) { it->second.x = *nx; it->second.y = *ny; }
                }
            }
        }
        // status == Timeout falls through here too — always re-broadcast.

        std::string snapshot;
        {
            std::lock_guard<std::mutex> lock(g_world_mu);
            if (g_world_players.find(fd) == g_world_players.end()) break;
            std::ostringstream os;
            os << "{\"players\":{";
            bool first = true;
            for (auto& [id, p] : g_world_players) {
                if (!first) os << ",";
                first = false;
                os << "\"" << id << "\":{\"x\":" << p.x << ",\"y\":" << p.y
                   << ",\"username\":\"" << p.username << "\",\"color\":\"" << p.color << "\"}";
            }
            os << "}}";
            snapshot = os.str();
        }
        if (!ws_write_frame(fd, WsOpcode::Text, snapshot)) break;
    }

    {
        std::lock_guard<std::mutex> lock(g_world_mu);
        g_world_players.erase(fd);
    }
    close(fd);
}

// Minimal ad-hoc extractor for a top-level {"key":"value"} string field —
// same "only shape this endpoint ever receives" reasoning as the world
// socket's x/y number parser. Not escape-aware: fine for URLs, which never
// contain a literal quote character.
std::optional<std::string> extract_json_string_field(const std::string& s, const char* key) {
    auto p = s.find(key);
    if (p == std::string::npos) return std::nullopt;
    p = s.find(':', p);
    if (p == std::string::npos) return std::nullopt;
    auto open = s.find('"', p);
    if (open == std::string::npos) return std::nullopt;
    auto close_pos = s.find('"', open + 1);
    if (close_pos == std::string::npos) return std::nullopt;
    return s.substr(open + 1, close_pos - open - 1);
}

// One thread per connected station client (/community/ws for the "" site-
// wide station, /c/<slug>/jukebox/ws for a community's own) — same shape
// as run_world_session, entirely separate shared state (g_community_mu).
// Membership (for a real station) was already checked before this thread
// was spawned, same as everything else here trusts the caller. Any
// connected member can add a track (open queue within the station, same
// posture the site-wide one always had); the queue auto-advances when any
// connected client's player reports the current track ended.
void run_community_session(int fd, const std::string& username, const std::string& station) {
    timeval tv{}; tv.tv_sec = 0; tv.tv_usec = 500000; // 500ms — a shared queue doesn't need world's 100ms cadence
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Joining is a 0->1 transition on this station's listener count: if
    // nobody else is here either, resume from wherever the freeze left
    // off (durable jpos, so this survives a process restart too, not just
    // a run of the same process with everyone gone and back). If someone
    // else is already listening (count already >0), there's nothing to
    // do here -- community_elapsed_locked already gives this new
    // connection the same live position everyone else has.
    {
        std::lock_guard<std::mutex> lock(g_community_mu);
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        int& count = g_community_listeners_by_station[station];
        const bool was_zero = (count == 0);
        ++count;
        if (was_zero) {
            if (auto pos = g_store.jukebox_pos(station)) {
                if (pos->offset_sec > kJukeboxResumeMaxSec) {
                    g_store.clear_jukebox_pos(station);
                } else {
                    auto& np = g_community_now_by_station[station];
                    np.video_id = pos->video_id;
                    np.frozen_offset = pos->offset_sec;
                    np.resumed_at = now;
                    np.active = true;
                }
            } else if (g_community_now_by_station[station].active) {
                g_community_now_by_station[station].resumed_at = now;
            }
        }
    }

    std::string reported_unplayable; // this connection's one vote per track (see g_community_unplayable_by_station)
    for (;;) {
        auto result = ws_read_frame(fd);
        if (result.status == WsReadStatus::Closed) break;
        if (result.status == WsReadStatus::Ok) {
            if (result.frame.opcode == WsOpcode::Close) break;
            if (result.frame.opcode == WsOpcode::Text) {
                const std::string& s = result.frame.payload;
                if (auto raw = extract_json_string_field(s, "\"add\"")) {
                    if (auto id = extract_youtube_id(trim_ws(*raw))) {
                        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();
                        std::lock_guard<std::mutex> lock(g_community_mu);
                        auto& now_playing = g_community_now_by_station[station];
                        auto& upcoming = g_community_upcoming_by_station[station];
                        if (!now_playing.active && upcoming.empty()) {
                            now_playing = CommunityNowPlaying{*id, username, 0, now, true};
                            g_store.set_jukebox_pos(station, *id, 0, now);
                            g_community_last_flush_by_station[station] = now;
                        } else {
                            upcoming.push_back(CommunityTrack{*id, username});
                        }
                    }
                } else if (auto raw = extract_json_string_field(s, "\"play\"")) {
                    // A click starts the video when this listener is the only one
                    // connected, or when the station is idle. With someone else
                    // listening, it queues, so one person cannot skip the room.
                    if (auto id = extract_youtube_id(trim_ws(*raw))) {
                        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();
                        std::lock_guard<std::mutex> lock(g_community_mu);
                        auto& now_playing = g_community_now_by_station[station];
                        auto& upcoming = g_community_upcoming_by_station[station];
                        const int listeners = g_community_listeners_by_station[station];
                        const bool alone = listeners <= 1;
                        if (alone || (!now_playing.active && upcoming.empty())) {
                            if (now_playing.active && now_playing.video_id != *id) {
                                auto& hist = g_community_history_by_station[station];
                                hist.push_back(CommunityTrack{now_playing.video_id, now_playing.queued_by});
                                if (hist.size() > kCommunityHistoryCap) hist.pop_front();
                            }
                            now_playing = CommunityNowPlaying{*id, username, 0, now, true};
                            g_store.set_jukebox_pos(station, *id, 0, now);
                            g_community_last_flush_by_station[station] = now;
                        } else {
                            upcoming.push_back(CommunityTrack{*id, username});
                        }
                        lglog::emit("jukebox", "play "+station+" "+*id+" by "+username);
                    }
                } else if (s.find("\"ended\"") != std::string::npos || s.find("\"unplayable\"") != std::string::npos) {
                    const bool unplayable = s.find("\"unplayable\"") != std::string::npos;
                    // {"ended":"<id>"} / {"unplayable":"<id>"} name the track; an old client's {"ended":true} names none.
                    const auto named = extract_json_string_field(s, unplayable ? "\"unplayable\"" : "\"ended\"");
                    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    std::lock_guard<std::mutex> lock(g_community_mu);
                    auto& now_playing = g_community_now_by_station[station];
                    const int listeners = g_community_listeners_by_station[station];
                    const bool alone = listeners <= 1;
                    bool advance = false;
                    if (!now_playing.active) {
                        // nothing to end
                    } else if (named && *named != now_playing.video_id) {
                        lglog::emit("jukebox", "ended-stale " + station + " by " + username);
                    } else if (unplayable) {
                        if (named && reported_unplayable != now_playing.video_id) {
                            reported_unplayable = now_playing.video_id;
                            auto& votes = g_community_unplayable_by_station[station];
                            if (votes.first != now_playing.video_id) votes = {now_playing.video_id, 0};
                            ++votes.second;
                        }
                        const auto& votes = g_community_unplayable_by_station[station];
                        const int n = votes.first == now_playing.video_id ? votes.second : 0;
                        // Brandon 2026-11-08: skip "if it wont play for at least 25% or more people trying to use
                        // juketube" -- so one silent listener (background tab, old page) can't stall the room.
                        advance = n > 0 && (alone || n * 4 >= listeners);
                        lglog::emit("jukebox", "unplayable " + station + " " + now_playing.video_id + " " +
                                    std::to_string(n) + "/" + std::to_string(listeners) + " by " + username);
                    } else {
                        advance = alone || (named && community_elapsed_locked(station, now) >= kCommunityMinEndedSec);
                        if (!advance) lglog::emit("jukebox", "ended-ignored " + station + " by " + username);
                    }
                    if (advance) {
                        // Bug (Brandon: "i dont see an accurate history"): nothing ever
                        // recorded a shared station's plays anywhere, so the History
                        // section's /jukebox/history?station= call always came back empty
                        // and silently fell back to the viewer's OWN personal plays --
                        // "inaccurate," not missing. Recorded here, the one place a track
                        // on this station is actually known to be done.
                        auto& hist = g_community_history_by_station[station];
                        hist.push_back(CommunityTrack{now_playing.video_id, now_playing.queued_by});
                        if (hist.size() > kCommunityHistoryCap) hist.pop_front();
                        community_advance_locked(station, now);
                        const auto& np = g_community_now_by_station[station];
                        if (np.active) g_store.set_jukebox_pos(station, np.video_id, 0, now);
                        else g_store.clear_jukebox_pos(station);
                        g_community_last_flush_by_station[station] = now;
                    }
                }
            }
        }

        std::string snapshot;
        {
            std::lock_guard<std::mutex> lock(g_community_mu);
            const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            const auto& now_playing = g_community_now_by_station[station];
            const auto elapsed = community_elapsed_locked(station, now);
            // Roughly every 15s while playing, not every ~500ms tick --
            // shared across however many listeners are connected (only
            // whichever one's tick crosses the threshold actually writes).
            if (now_playing.active) {
                auto& last_flush = g_community_last_flush_by_station[station];
                if (now - last_flush >= 15) {
                    last_flush = now;
                    g_store.set_jukebox_pos(station, now_playing.video_id, elapsed, now);
                }
            }
            std::ostringstream os;
            os << "{";
            if (now_playing.active) {
                os << "\"now\":{\"video_id\":\"" << now_playing.video_id
                   << "\",\"queued_by\":\"" << now_playing.queued_by
                   << "\",\"elapsed\":" << elapsed << "}";
            } else {
                os << "\"now\":null";
            }
            os << ",\"listeners\":" << g_community_listeners_by_station[station];
            os << ",\"upcoming\":" << g_community_upcoming_by_station[station].size();
            os << ",\"queue\":[";
            {
                bool qfirst = true;
                for (const auto& t : g_community_upcoming_by_station[station]) {
                    if (!qfirst) os << ",";
                    qfirst = false;
                    os << "{\"video_id\":\"" << t.video_id << "\",\"queued_by\":\"" << t.queued_by << "\"}";
                }
            }
            os << "]}";
            snapshot = os.str();
        }
        if (!ws_write_frame(fd, WsOpcode::Text, snapshot)) break;
    }

    // Leaving is a ...->0 transition (or not, if someone else is still
    // here): only the actual last listener freezes and saves. WS close is
    // enough on its own to catch this -- no client-side pagehide beacon
    // needed for community/site-wide stations, unlike the personal
    // (HTTP-polled) ones below.
    {
        std::lock_guard<std::mutex> lock(g_community_mu);
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        // Elapsed has to be computed while the count still reflects this
        // connection as live -- community_elapsed_locked only adds live
        // time when count>0, so doing this after decrementing would
        // freeze at frozen_offset alone, silently dropping this session's
        // own listening time (caught live: a 5s hold froze at 0, not 5).
        auto& np = g_community_now_by_station[station];
        const auto elapsed = community_elapsed_locked(station, now);
        int& count = g_community_listeners_by_station[station];
        if (count > 0) --count;
        if (count == 0 && np.active) {
            if (elapsed > kJukeboxResumeMaxSec) {
                np = CommunityNowPlaying{};
                g_store.clear_jukebox_pos(station);
            } else {
                np.frozen_offset = elapsed;
                g_store.set_jukebox_pos(station, np.video_id, elapsed, now);
            }
        }
    }
    close(fd);
}

// Floating jukebox widget: a phone-icon button, present on every logged-in
// page (injected from wrap()), that expands into a panel with two modes —
// "Community" (synced to the site-wide queue over /community/ws, same
// real-time pattern as /world/ws) and "My search" (private, local-only
// playback, never touches the server). One YT.Player instance is reused
// across both modes so play/pause/mute/volume always act on whatever's
// currently loaded.
//
// Honest limitation, not hidden: this site is deliberately a multi-page
// app (LIQUIDGOV_CONTEXT_HANDOFF.md's Jukebox section — no site-wide SPA).
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
    std::string s =
        "<script>(function(){"
        "var STANDALONE="+std::string(standalone ? "true" : "false")+";"
        "var OWN_USERNAME="+js_string_literal(own_username)+";"
        "var tabC=document.getElementById('jbtab-community'),tabS=document.getElementById('jbtab-search');"
        "var playerDiv=document.getElementById('jbplayer');"
        "var playPauseBtn=document.getElementById('jbplaypause'),muteBtn=document.getElementById('jbmute');"
        "var volumeSlider=document.getElementById('jbvolume');"
        "var nowEl=document.getElementById('jbnow'),upcomingEl=document.getElementById('jbupcoming');"
        "var addInput=document.getElementById('jbadd'),addResults=document.getElementById('jbaddresults');"
        "var addErr=document.getElementById('jbadderr');"
        "var searchInput=document.getElementById('jbsearch'),searchResults=document.getElementById('jbsearchresults');"
        "var searchCat=document.getElementById('jbsearchcat'),historySel=document.getElementById('jbhistorysel');"
        "var communityUi=document.getElementById('jbcommunity-ui'),searchUi=document.getElementById('jbsearch-ui');"
        "var stationList=document.getElementById('jbstationlist'),stationAdd=document.getElementById('jbstationadd');"
        "var mode='community',ytPlayer=null,ws=null,muted=false;"
        "var pastEndReportedFor=null;" // see onStateChange's own comment -- one past-end self-heal report per video_id
        // ytPlayer itself goes non-null the instant `new YT.Player(...)`
        // is called -- its actual methods (loadVideoById etc.) aren't
        // guaranteed to exist until the async onReady event fires later.
        // playerReady/pendingPlayerCbs track real readiness separately so
        // a second ensurePlayer() call arriving in that gap queues behind
        // onReady instead of firing on a half-constructed player (caught
        // live: "ytPlayer.loadVideoById is not a function").
        "var playerReady=false,pendingPlayerCbs=[];"
        "var lastCommunityId=null,lastCommunityElapsed=0,lastKnownDuration=0,stationLoadedAt=0,ignoreEndedUntil=0;"
        "function saneStart(elapsed){"
        "elapsed=Number(elapsed);"
        "if(!isFinite(elapsed)||elapsed<0)return 0;"
        "if(elapsed>3*3600)return 0;"
        "if(lastKnownDuration>1&&elapsed>=lastKnownDuration-0.5)return 0;"
        "return elapsed;"
        "}"
        "function loadStationVideo(id,elapsed){"
        "lastKnownDuration=0;"
        "lastCommunityId=id;"
        "lastCommunityElapsed=saneStart(elapsed);"
        "stationLoadedAt=Date.now();"
        "ignoreEndedUntil=Date.now()+4000;ytPlayer.loadVideoById({videoId:id,startSeconds:0});"
        "}"
        "var pollTimer=null,activePersonalOwner=null;"
        "var followInput=document.getElementById('jbfollow'),followBtn=document.getElementById('jbfollowbtn'),"
        "followErr=document.getElementById('jbfollowerr');"
        // A personal channel's station id is "u:<username>" -- see
        // jukebox_stations_js. Everything else (site-wide "" or a
        // community slug) is a live WebSocket station.
        "function personalOwnerFromId(id){return id&&id.indexOf('u:')===0?id.slice(2):null;}"
        // Stations: which live jukeboxes (the site-wide one, id "", plus any
        // community this account belongs to) are tuned in on *this* widget,
        // in priority order (index 0 = plays). This is the device's own
        // widget config -- localStorage, not an account setting -- same
        // reasoning as the panel's own open/closed state never being saved
        // server-side. tempOverride is a click-to-listen-now override: set
        // by tapping a station's name, cleared by tapping it again or
        // choosing a different one, never persisted (it's explicitly
        // temporary -- see jukebox_panel_body's station row markup).
        "var STATIONS_AVAILABLE="+stations_js+";"
        "var STATIONS_KEY='liquidgov.jukebox.stations';"
        "var enabledOrder=[''];"
        "try{"
        "var saved=JSON.parse(localStorage.getItem(STATIONS_KEY)||'null');"
        "if(saved&&saved.length){"
        "var filtered=saved.filter(function(id){"
        "return STATIONS_AVAILABLE.some(function(s){return s.id===id;});"
        "});"
        "if(filtered.length)enabledOrder=filtered;"
        "}"
        "}catch(e){}"
        "var tempOverride=null,currentConnectedStation=undefined;"
        "function saveStations(){try{localStorage.setItem(STATIONS_KEY,JSON.stringify(enabledOrder));}catch(e){}}"
        "function stationName(id){"
        "var found=STATIONS_AVAILABLE.filter(function(s){return s.id===id;})[0];"
        "return found?found.name:id;"
        "}"
        "function activeStationId(){"
        "if(tempOverride!==null)return tempOverride;"
        "return enabledOrder.length?enabledOrder[0]:null;"
        "}"
        "function esc(s){var ta=document.createElement('textarea');ta.innerHTML=s;"
        "var d=document.createElement('div');d.textContent=ta.value;return d.innerHTML;}"
        "function looksLikeUrl(s){return /^https?:\\/\\//i.test(s)||/youtu\\.?be/i.test(s);}"
        "function updateTabs(){"
        "tabC.style.background=mode==='community'?'var(--accent)':'';tabC.style.color=mode==='community'?'var(--paper)':'';"
        "tabS.style.background=mode==='search'?'var(--accent)':'';tabS.style.color=mode==='search'?'var(--paper)':'';"
        "}"
        "function ensureYT(cb){"
        "if(window.YT&&window.YT.Player){cb();return;}"
        "if(!document.getElementById('jbytapi')){"
        "var s=document.createElement('script');s.id='jbytapi';s.src='https://www.youtube.com/iframe_api';"
        "document.head.appendChild(s);}"
        "var prev=window.onYouTubeIframeAPIReady;"
        "window.onYouTubeIframeAPIReady=function(){if(prev)prev();cb();};"
        "}"
        "function ensurePlayer(cb){"
        "if(playerReady){cb();return;}"
        "pendingPlayerCbs.push(cb);"
        "if(ytPlayer)return;" // construction already in flight -- onReady below drains the queue
        "ensureYT(function(){"
        "ytPlayer=new YT.Player('jbplayer',{height:'100%',width:'100%',"
        "playerVars:{rel:0,playsinline:1},events:{"
        "onReady:function(){"
        "playerReady=true;"
        "ytPlayer.setVolume(Number(volumeSlider.value));"
        "var cbs=pendingPlayerCbs;pendingPlayerCbs=[];"
        "cbs.forEach(function(fn){fn();});"
        "},"
        "onStateChange:function(e){"
        "if(e.data===YT.PlayerState.PLAYING){playPauseBtn.textContent='Pause';try{var dd=ytPlayer.getDuration();if(dd>1)lastKnownDuration=dd;}catch(err){}}"
        "else if(e.data===YT.PlayerState.PAUSED)playPauseBtn.textContent='Play';"
        "function reportEnded(){"
        "if(Date.now()<ignoreEndedUntil)return;"
        "var d=0,t=0;"
        "try{d=ytPlayer.getDuration()||0;t=ytPlayer.getCurrentTime()||0;}catch(err){}"
        "if(d>1)lastKnownDuration=d;"
        "if(d<1||lastCommunityElapsed>=d-0.5||(t<3&&(lastCommunityElapsed>5||Date.now()-stationLoadedAt<2500))){"
        "lastCommunityElapsed=0;"
        "try{ytPlayer.seekTo(0,true);ytPlayer.playVideo();}catch(err){}"
        "return;"
        "}"
        "if(ws&&ws.readyState===1)ws.send(JSON.stringify({ended:lastCommunityId||true}));"
        "else if(activePersonalOwner&&lastCommunityId)fetch("
        "'/u/'+encodeURIComponent(activePersonalOwner)+'/jukebox/complete',{method:'POST',"
        "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
        "body:'video_id='+encodeURIComponent(lastCommunityId)});"
        "}"
        "if(e.data===YT.PlayerState.ENDED&&mode==='community')reportEnded();"
        // The shared elapsed clock only tracks real wall-clock time since
        // the last freeze/resume (see community_elapsed_locked's own
        // comment) -- it has no idea how long the actual video is, so
        // nothing stops it drifting past a short video's real end if
        // nobody's been actually watching (caught live: a 59s video
        // frozen at 734s, from an earlier round of WS testing that held
        // connections open without ever loading a real player). Loading
        // at or past the real duration would otherwise just sit there
        // glitching -- self-heal by reporting it finished immediately,
        // the same path a genuine ENDED event already takes, so the
        // queue advances instead of getting stuck.
        "if((e.data===YT.PlayerState.PLAYING||e.data===YT.PlayerState.CUED)&&mode==='community'&&"
        "typeof ytPlayer.getDuration==='function'){"
        "var dur=ytPlayer.getDuration();"
        "if(dur>0&&ytPlayer.getCurrentTime()>=dur-1&&pastEndReportedFor!==lastCommunityId){"
        "pastEndReportedFor=lastCommunityId;"
        "reportEnded();"
        "}"
        "}"
        // This closes, in order: the PLAYING/CUED-duration if-block above
        // (the one just before it), onStateChange's own function body,
        // and the events:{...} object -- three closes, not two. Missing
        // one of these left the *entire* jukebox_script IIFE with an
        // unbalanced brace, which is a parse-time SyntaxError: the whole
        // script tag silently fails to run, not just this block -- no
        // listener anywhere in the widget ever attaches. That's the real
        // explanation for "click never does anything, no console error":
        // there's no error to see because the script never started.
        "}}});"
        "});"
        "}"
        "function teardownWs(){"
        "if(ws){try{ws.close();}catch(e){}ws=null;}"
        "lastCommunityId=null;"
        "}"
        "function teardownPoll(){"
        "if(pollTimer){clearInterval(pollTimer);pollTimer=null;}"
        "activePersonalOwner=null;lastCommunityId=null;"
        "}"
        "function teardownActive(){teardownWs();teardownPoll();}"
        "function teardown(){"
        "teardownActive();"
        "if(ytPlayer){try{ytPlayer.destroy();}catch(e){}ytPlayer=null;}"
        "playerReady=false;pendingPlayerCbs=[];"
        "}"
        // Personal channels have no live sync -- a shared but ordinary HTTP
        // queue (see jukebox_widget), same one the profile page itself
        // shows at the front. Polling /jukebox/now every few seconds is
        // the widget's substitute for the WebSocket push the site-wide/
        // community stations get; good enough since a personal queue only
        // changes when someone queues or a track ends, not continuously.
        "function pollPersonal(owner){"
        "if(pollTimer)return;"
        "activePersonalOwner=owner;"
        "function tick(){"
        "fetch('/u/'+encodeURIComponent(owner)+'/jukebox/now').then(function(r){"
        "if(!r.ok)throw new Error('unavailable');return r.json();"
        "}).then(function(data){"
        "if(activePersonalOwner!==owner)return;"
        "if(data.now){"
        "nowEl.textContent='Now playing on '+esc(stationName('u:'+owner))+' \\u2014 queued by '+esc(data.now.queued_by);"
        "if(data.now.video_id!==lastCommunityId){"
        "lastCommunityId=data.now.video_id;lastCommunityElapsed=saneStart(data.now.elapsed||0);"
        "if(mode==='community')ensurePlayer(function(){"
        "loadStationVideo(lastCommunityId,lastCommunityElapsed);"
        "});"
        "}else{"
        "lastCommunityElapsed=saneStart(data.now.elapsed||0);"
        // Same shared-playhead drift correction the WS-backed stations
        // get from connectWs's onmessage -- the server clock is the one
        // that matters, this just catches a personal station's own
        // player up to it every few seconds instead of only on a track
        // change.
        "if(mode==='community'&&playerReady&&typeof ytPlayer.getCurrentTime==='function'"
        "&&ytPlayer.getPlayerState()===YT.PlayerState.PLAYING){"
        "var cur=ytPlayer.getCurrentTime();"
        "if(lastKnownDuration>1){var tgt=saneStart(lastCommunityElapsed);if(tgt<lastKnownDuration-0.5&&Math.abs(cur-tgt)>2)ytPlayer.seekTo(tgt,true);}"
        "}"
        "}"
        "}else{"
        "nowEl.textContent=esc(stationName('u:'+owner))+': nothing playing \\u2014 add a track below.';"
        "lastCommunityId=null;"
        "if(mode==='community'&&playerReady)ytPlayer.stopVideo();"
        "}"
        "upcomingEl.textContent=data.upcoming?(data.upcoming+' track'+(data.upcoming===1?'':'s')+' queued next'):'';"
        "}).catch(function(){"
        "if(activePersonalOwner!==owner)return;"
        "nowEl.textContent=esc(stationName('u:'+owner))+': can\\u2019t reach this channel right now.';"
        "});"
        "}"
        "tick();pollTimer=setInterval(tick,4000);"
        "}"
        // The polling widget's substitute for a WebSocket connect/
        // disconnect (see personal_heartbeat_locked/personal_freeze_locked
        // server-side) -- fires only while a personal station is both the
        // active one AND actually playing, not just open in the
        // background. video_id only, on purpose: the server clock is what
        // advances the shared position, never a client-reported
        // currentTime (two listeners drift differently; the server can't
        // referee that from a bare heartbeat, only from an explicit
        // freeze with its own reconciliation -- see personalFreezeBeacon).
        "function personalHeartbeatTick(){"
        "if(mode!=='community'||!activePersonalOwner||!playerReady||!lastCommunityId)return;"
        "if(ytPlayer.getPlayerState()!==YT.PlayerState.PLAYING)return;"
        "fetch('/u/'+encodeURIComponent(activePersonalOwner)+'/jukebox/pos',{method:'POST',"
        "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
        "body:'video_id='+encodeURIComponent(lastCommunityId)});"
        "}"
        // Pause or pagehide on a personal station: stop right now instead
        // of waiting out the server's grace window, so the next listener
        // (or this same person, back later) resumes from here, not up to
        // 30s short of it. sendBeacon survives the page actually
        // unloading; a plain fetch might not finish in time.
        "function personalFreezeBeacon(){"
        "if(mode!=='community'||!activePersonalOwner||!playerReady||!lastCommunityId)return;"
        "var cur=0;try{cur=Math.round(ytPlayer.getCurrentTime());}catch(e){}"
        "var b='video_id='+encodeURIComponent(lastCommunityId)+'&freeze=1&offset='+cur;"
        "var url='/u/'+encodeURIComponent(activePersonalOwner)+'/jukebox/pos';"
        "if(navigator.sendBeacon){"
        "navigator.sendBeacon(url,new Blob([b],{type:'application/x-www-form-urlencoded'}));"
        "}else{"
        "fetch(url,{method:'POST',keepalive:true,"
        "headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b});"
        "}"
        "}"
        "setInterval(personalHeartbeatTick,15000);"
        "window.addEventListener('pagehide',personalFreezeBeacon);"
        "function connectActive(id){"
        "if(id===null){nowEl.textContent='No station selected \\u2014 add one below.';upcomingEl.textContent='';"
        "if(playerReady)ytPlayer.stopVideo();return;}"
        "var owner=personalOwnerFromId(id);"
        "if(owner)pollPersonal(owner);else connectWs(id);"
        "}"
        "function connectWs(stationId){"
        "if(ws)return;"
        "var proto=location.protocol==='https:'?'wss:':'ws:';"
        "var path=stationId?('/c/'+encodeURIComponent(stationId)+'/jukebox/ws'):'/community/ws';"
        "ws=new WebSocket(proto+'//'+location.host+path);"
        "ws.onmessage=function(ev){"
        "var data;try{data=JSON.parse(ev.data);}catch(e){return;}"
        "if(data.now){"
        "nowEl.textContent='Now playing on '+esc(stationName(stationId))+' \\u2014 queued by '+esc(data.now.queued_by);"
        "if(data.now.video_id!==lastCommunityId){"
        "lastCommunityId=data.now.video_id;lastCommunityElapsed=saneStart(data.now.elapsed);"
        "if(mode==='community')ensurePlayer(function(){"
        "loadStationVideo(lastCommunityId,lastCommunityElapsed);"
        "});"
        "}else{"
        "lastCommunityElapsed=saneStart(data.now.elapsed);"
        // WS ticks every ~500ms, so drift here is normally tiny (a
        // reconnect or a stalled player are the real cases this catches);
        // same >2s threshold and reasoning as the personal/poll path.
        "if(mode==='community'&&playerReady&&typeof ytPlayer.getCurrentTime==='function'"
        "&&ytPlayer.getPlayerState()===YT.PlayerState.PLAYING){"
        "var cur=ytPlayer.getCurrentTime();"
        "if(lastKnownDuration>1){var tgt=saneStart(lastCommunityElapsed);if(tgt<lastKnownDuration-0.5&&Math.abs(cur-tgt)>2)ytPlayer.seekTo(tgt,true);}"
        "}"
        "}"
        "}else{"
        "nowEl.textContent=esc(stationName(stationId))+': nothing playing \\u2014 add a track below.';"
        "lastCommunityId=null;"
        "if(mode==='community'&&playerReady)ytPlayer.stopVideo();"
        "}"
        "upcomingEl.textContent=data.upcoming?(data.upcoming+' track'+(data.upcoming===1?'':'s')+' queued next'):'';"
        "};"
        "}"
        // Called after enabledOrder or tempOverride changes: re-renders the
        // station list (so the active one's highlight is right) and, only
        // when the *actual* active station changed, tears down and
        // reconnects the live socket to it -- a reorder that doesn't move
        // anything above the active one, or a temp-override toggle back to
        // the same station that was already playing, is a no-op here.
        "function onStationsChanged(persist){"
        "if(persist)saveStations();"
        "renderStations();renderStationAdd();"
        "var id=activeStationId();"
        "if(id===currentConnectedStation)return;"
        "currentConnectedStation=id;"
        "if(mode==='community'){"
        "teardownActive();"
        "connectActive(id);"
        "}"
        "}"
        "function renderStations(){"
        "var active=activeStationId();"
        "stationList.innerHTML='';"
        "enabledOrder.forEach(function(id){"
        "var row=document.createElement('div');"
        "row.className='jbstation-row'+(id===active?' active':'');"
        "row.dataset.id=id;"
        "var handle=document.createElement('span');handle.className='jbdraghandle';"
        "handle.textContent='\\u283F';handle.setAttribute('aria-hidden','true');"
        "var name=document.createElement('span');name.className='jbstation-name';"
        "name.textContent=stationName(id);name.title='Tap to listen to this now';"
        "name.addEventListener('click',function(){"
        "tempOverride=(tempOverride===id)?null:id;"
        "onStationsChanged(false);"
        "});"
        "var remove=document.createElement('button');remove.type='button';remove.className='jbstation-remove';"
        "remove.setAttribute('aria-label','Remove station');remove.textContent='\\u2715';"
        "remove.addEventListener('click',function(e){"
        "e.stopPropagation();"
        "enabledOrder=enabledOrder.filter(function(x){return x!==id;});"
        "if(tempOverride===id)tempOverride=null;"
        "onStationsChanged(true);"
        "var removedOwner=personalOwnerFromId(id);"
        "if(removedOwner)fetch('/u/'+encodeURIComponent(removedOwner)+'/jukebox/unsubscribe',{method:'POST'});"
        "});"
        "row.appendChild(handle);row.appendChild(name);row.appendChild(remove);"
        "stationList.appendChild(row);"
        "});"
        "}"
        "function renderStationAdd(){"
        "stationAdd.innerHTML='';"
        "var ph=document.createElement('option');ph.value='__placeholder__';ph.textContent='Add a station\\u2026';"
        "stationAdd.appendChild(ph);"
        "STATIONS_AVAILABLE.forEach(function(s){"
        "if(enabledOrder.indexOf(s.id)!==-1)return;"
        "var opt=document.createElement('option');opt.value=s.id;opt.textContent=s.name;"
        "stationAdd.appendChild(opt);"
        "});"
        "stationAdd.value='__placeholder__';"
        "}"
        "stationAdd.addEventListener('change',function(){"
        "var id=stationAdd.value;"
        "if(id==='__placeholder__')return;"
        "if(enabledOrder.indexOf(id)===-1){enabledOrder.push(id);onStationsChanged(true);}"
        "});"
        // Following someone's personal Juketube is a real, server-recorded
        // subscription (not just this device's local station list) so the
        // owner can see who's tuned in -- see FileStore::subscribe_jukebox.
        // Unlike the fixed dropdown above, any username can be typed here.
        "function doFollow(){"
        "var name=followInput.value.trim();"
        "followErr.hidden=true;"
        "if(!name)return;"
        "followBtn.disabled=true;"
        "fetch('/u/'+encodeURIComponent(name)+'/jukebox/subscribe',{method:'POST'}).then(function(r){"
        "followBtn.disabled=false;"
        "if(!r.ok){"
        "followErr.textContent=r.status===404?'No one by that name.':'Couldn\\u2019t follow that channel.';"
        "followErr.hidden=false;return;"
        "}"
        "var id='u:'+name;"
        "if(!STATIONS_AVAILABLE.some(function(s){return s.id===id;}))"
        "STATIONS_AVAILABLE.push({id:id,name:name+'\\u2019s Juketube'});"
        "followInput.value='';"
        "if(enabledOrder.indexOf(id)===-1){enabledOrder.push(id);onStationsChanged(true);}"
        "}).catch(function(){"
        "followBtn.disabled=false;followErr.textContent='Couldn\\u2019t reach the server.';followErr.hidden=false;"
        "});"
        "}"
        "followBtn.addEventListener('click',doFollow);"
        "followInput.addEventListener('keydown',function(e){if(e.key==='Enter'){e.preventDefault();doFollow();}});"
        // Pointer-based reorder (not native HTML5 drag, which doesn't work
        // on touch): drag the handle, the row under the pointer's midpoint
        // is where it lands, dropped position becomes the new saved order.
        "(function attachStationDrag(){"
        "var dragRow=null;"
        "function rowUnder(clientY){"
        "var rows=Array.prototype.slice.call(stationList.querySelectorAll('.jbstation-row'));"
        "var after=null;"
        "rows.forEach(function(r){"
        "if(r===dragRow)return;"
        "var box=r.getBoundingClientRect();"
        "if(clientY>box.top+box.height/2)after=r;"
        "});"
        "return after;"
        "}"
        "stationList.addEventListener('pointerdown',function(e){"
        "var row=e.target.closest&&e.target.closest('.jbstation-row');"
        "var handle=e.target.closest&&e.target.closest('.jbdraghandle');"
        "if(!row||!handle)return;"
        "dragRow=row;"
        "try{row.setPointerCapture(e.pointerId);}catch(err){}"
        "row.style.zIndex='5';row.style.opacity='.85';"
        "});"
        "stationList.addEventListener('pointermove',function(e){"
        "if(!dragRow)return;"
        "var after=rowUnder(e.clientY);"
        "var target=after?after.nextSibling:stationList.firstChild;"
        "if(target!==dragRow)stationList.insertBefore(dragRow,target);"
        "});"
        "function drop(){"
        "if(!dragRow)return;"
        "dragRow.style.zIndex='';dragRow.style.opacity='';"
        "dragRow=null;"
        "enabledOrder=Array.prototype.slice.call(stationList.querySelectorAll('.jbstation-row'))"
        ".map(function(r){return r.dataset.id;});"
        "onStationsChanged(true);"
        "}"
        "stationList.addEventListener('pointerup',drop);"
        "stationList.addEventListener('pointercancel',drop);"
        "})();"
        "function switchMode(m){"
        "mode=m;updateTabs();"
        "communityUi.hidden=(m!=='community');searchUi.hidden=(m!=='search');"
        "if(m==='community'){"
        "var id=activeStationId();"
        "if(id!==currentConnectedStation||(!ws&&!pollTimer)){"
        "currentConnectedStation=id;teardownActive();"
        "connectActive(id);"
        "}"
        "/* keep current video on Stations tab */"
        "}"
        "}"
        "function attachLiveSearch(input,resultsEl,onPick){"
        "var timer=null,gen=0;"
        "input.addEventListener('input',function(){"
        "var val=input.value.trim();clearTimeout(timer);"
        "if(!val||looksLikeUrl(val)){resultsEl.innerHTML='';return;}"
        "timer=setTimeout(function(){"
        "var my=++gen;"
        "resultsEl.textContent='Searching...';"
        "fetch('/jukebox/search?q='+encodeURIComponent(val),{credentials:'same-origin'})"
        ".then(function(r){if(!r.ok)throw new Error('http '+r.status);return r.json();})"
        ".then(function(data){"
        "if(my!==gen)return;"
        "resultsEl.innerHTML='';"
        "var items=data.items||[];"
        "if(!items.length){resultsEl.textContent='No matches.';return;}"
        "items.forEach(function(it){"
        "var id=(it.id&&it.id.videoId)||'';"
        "var title=(it.snippet&&it.snippet.title)||'untitled';"
        "var th=it.snippet&&it.snippet.thumbnails&&(it.snippet.thumbnails.default||it.snippet.thumbnails.medium);"
        "var thumb=(th&&th.url)||'';"
        "if(!id)return;"
        "var row=document.createElement('div');row.className='row';"
        "row.style.cssText='align-items:center;cursor:pointer;padding:.25rem;border-radius:var(--rsm)';"
        "row.innerHTML='<img src=\"'+thumb+'\" width=\"48\" height=\"36\" alt=\"\" style=\"border-radius:4px\">"
        "<span style=\"flex:1;font-size:.85rem\">'+esc(title)+'</span>';"
        "row.addEventListener('click',function(){"
        "input.value='';resultsEl.innerHTML='';onPick('https://www.youtube.com/watch?v='+id);"
        "});"
        "resultsEl.appendChild(row);"
        "});"
        "}).catch(function(){if(my===gen)resultsEl.textContent='Search failed.';});"
        "},300);"
        "});"
        "input.addEventListener('keydown',function(e){"
        "if(e.key==='Enter'){"
        "e.preventDefault();"
        "var val=input.value.trim();"
        "if(val&&looksLikeUrl(val)){input.value='';resultsEl.innerHTML='';onPick(val);}"
        "}"
        "});"
        "}"
        "attachLiveSearch(addInput,addResults,function(url){"
        "if(addErr)addErr.hidden=true;"
        // Every mutating fetch below either updates something real (the
        // widget re-tunes) or, on failure, tells the person right next to
        // the input -- a POST that just vanishes into an empty .catch is
        // how "add to queue" used to silently do nothing.
        "function fail(){if(addErr){addErr.textContent='Could not reach the server. Try again.';addErr.hidden=false;}}"
        "if(activePersonalOwner){"
        "fetch('/u/'+encodeURIComponent(activePersonalOwner)+'/jukebox/add',{method:'POST',credentials:'same-origin',"
        "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
        "body:'url='+encodeURIComponent(url)}).then(function(r){if(!r.ok)fail();}).catch(fail);"
        "}else if(ws&&ws.readyState===1){"
        "ws.send(JSON.stringify({add:url}));"
        "}else{"
        // Neither a personal channel nor a live community socket is
        // connected -- tuning to nothing was a silent no-op click. Add to
        // the viewer's own queue instead and tune the widget there, so
        // the click both persists (jukebox_of(OWN_USERNAME) actually
        // gains the track) and plays immediately rather than waiting on
        // a poll tick that was never going to happen with no station
        // connected.
        "fetch('/me/jukebox/add',{method:'POST',credentials:'same-origin',"
        "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
        "body:'url='+encodeURIComponent(url)}).then(function(r){"
        "if(!r.ok){fail();return;}"
        // tempOverride/onStationsChanged/switchMode are all defined
        // unconditionally further up this same script -- used directly
        // (not through window.__lgTuneStation, which only exists in the
        // embedded, non-standalone widget) so this fallback works the
        // same way on the /listen popout too.
        "if(!OWN_USERNAME)return;"
        "tempOverride='u:'+OWN_USERNAME;"
        "if(mode!=='community')switchMode('community');else onStationsChanged(false);"
        "}).catch(fail);"
        "}"
        "});"
        // Search tab: browse people/communities by category and tune the
        // widget straight to one -- not a YouTube search (that stays on
        // the Stations tab's "Add to the queue" input above, the existing
        // /jukebox/search proxy). Clicking a row calls the same
        // __lgTuneStation bridge a profile's own "Play in Juketube"
        // button uses, so there's still only ever one player on the page.
        "var searchTimer=null,searchGen=0;"
        "function runSearch(){"
        "var term=searchInput.value.trim();"
        "clearTimeout(searchTimer);"
        "searchTimer=setTimeout(function(){"
        "var my=++searchGen;"
        "fetch('/jukebox/browse?category='+encodeURIComponent(searchCat.value)+"
        "'&q='+encodeURIComponent(term))"
        ".then(function(r){return r.json();})"
        ".then(function(data){"
        "if(my!==searchGen)return;"
        "searchResults.innerHTML='';"
        "(data.items||[]).forEach(function(it){"
        "var row=document.createElement('div');row.className='row';"
        "row.style.cssText='align-items:center;cursor:pointer;padding:.3rem;border-radius:var(--rsm)';"
        "row.textContent=it.name;"
        "row.addEventListener('click',function(){window.__lgTuneStation(it.id);});"
        "searchResults.appendChild(row);"
        "});"
        "}).catch(function(){});"
        "},250);"
        "}"
        "searchCat.addEventListener('change',runSearch);"
        "searchInput.addEventListener('input',runSearch);"
        "runSearch();"
        // History: newest first. A personal channel's history is scoped by
        // owner (?for=); a community/site-wide station's own history (bug
        // fix: this used to always send ?for= with an empty owner, which
        // isn't a station lookup at all -- it silently fell back to the
        // VIEWER'S OWN plays, not the station's) is scoped by ?station=
        // instead, including "" for the site-wide station itself.
        "function loadHistory(){"
        "var sid=activeStationId();var owner=personalOwnerFromId(sid);"
        "var q=owner?('for='+encodeURIComponent(owner)):('station='+encodeURIComponent(sid||''));"
        "fetch('/jukebox/history?'+q)"
        ".then(function(r){return r.json();})"
        ".then(function(data){"
        "historySel.innerHTML='<option value=\"\">Something you\\u2019ve played before\\u2026</option>';"
        "(data.items||[]).forEach(function(it,idx){"
        "var opt=document.createElement('option');opt.value=String(idx);"
        "opt.textContent=it.queued_by;"
        "opt.dataset.videoId=it.video_id;"
        "historySel.appendChild(opt);"
        "});"
        "}).catch(function(){});"
        "}"
        "historySel.addEventListener('focus',loadHistory);"
        "historySel.addEventListener('change',function(){"
        "var opt=historySel.selectedOptions[0];"
        "if(!opt||!opt.dataset.videoId)return;"
        "var videoId=opt.dataset.videoId;"
        "if(ws&&ws.readyState===1)ws.send(JSON.stringify({play:'https://www.youtube.com/watch?v='+videoId}));"
        "switchMode('community');"
        "ensurePlayer(function(){ytPlayer.loadVideoById(videoId);});"
        "historySel.value='';"
        "});"
        "if(!STANDALONE){"
        "var toggle=document.getElementById('jbtoggle'),panel=document.getElementById('jbpanel');"
        "toggle.addEventListener('click',function(e){"
        "e.stopPropagation();"
        "var opening=panel.style.display==='none';"
        "panel.style.display=opening?'block':'none';"
        // Deferred a frame so the un-hide paints before ensurePlayer's
        // synchronous new YT.Player(...) iframe creation runs -- see
        // #jbpanel's own comment for why same-tick was the actual bug.
        "if(opening)requestAnimationFrame(function(){"
        "ensurePlayer(function(){});"
        "if(mode==='community'&&!ws&&!pollTimer){currentConnectedStation=activeStationId();connectActive(currentConnectedStation);}"
        "});"
        "});"
        // No Pop out or Close in the embedded widget anymore -- the round
        // phone button (jbtoggle, above) is the only collapse/expand
        // control. window.__lgTuneStation exposes a way for other scripts
        // on the page (a profile's "Play in Juketube" button, the
        // autoplay-on-visit script) to open this same panel, switch it to
        // Stations, and tune it at a station id -- "" for site-wide, a
        // community slug, or "u:<username>" for a personal channel --
        // without spinning up a second YT.Player anywhere on the page.
        "window.__lgTuneStation=function(id){"
        "panel.style.display='block';"
        "tempOverride=id;"
        "if(mode!=='community')switchMode('community');"
        "else onStationsChanged(false);"
        "};"
        // Soft navigation: intercept plain same-origin link clicks and swap
        // <main>'s content via fetch instead of a full page load, so the
        // widget (a sibling of <main>, see wrap()) and its live
        // WebSocket/YT player are simply never touched by an ordinary click
        // around the site. Forms/POSTs are untouched (this only hooks
        // anchor clicks) and still do a normal full-page round trip.
        "document.addEventListener('click',function(e){"
        "if(e.defaultPrevented||e.button!==0||e.metaKey||e.ctrlKey||e.shiftKey||e.altKey)return;"
        "var a=e.target.closest&&e.target.closest('a[href]');"
        "if(!a||a.target&&a.target!=='_self'||a.hasAttribute('download'))return;"
        "var url;try{url=new URL(a.href,location.href);}catch(err){return;}"
        "if(url.origin!==location.origin||url.pathname===location.pathname&&url.search===location.search&&url.hash)return;"
        // /theme/<mode> flips the data-theme attribute on <html>, which sits
        // outside both regions softNavigate ever touches (main.wrap and
        // header.top nav) -- a soft nav here sets the cookie and swaps
        // content that already assumed the OLD theme, so nothing visibly
        // changes until some later real page load re-renders <html> from
        // scratch. Reads as "the toggle doesn't work" (Brandon, live on
        // liquidgov.us) until you hit refresh, at which point it's already
        // correct. Letting this one path fall through to a real navigation
        // is simpler and safer than teaching softNavigate to also patch
        // <html>'s attribute on every navigation just for this one case.
        "if(url.pathname.indexOf('/theme/')===0)return;"
        "e.preventDefault();"
        "softNavigate(url.href,true);"
        "});"
        "window.addEventListener('popstate',function(){softNavigate(location.href,false);});"
        "function reExecuteScripts(container){"
        "Array.prototype.forEach.call(container.querySelectorAll('script'),function(old){"
        "var fresh=document.createElement('script');"
        "for(var i=0;i<old.attributes.length;i++)fresh.setAttribute(old.attributes[i].name,old.attributes[i].value);"
        "fresh.textContent=old.textContent;"
        "old.parentNode.replaceChild(fresh,old);"
        "});"
        "}"
        "function softNavigate(url,pushState){"
        "fetch(url,{headers:{'X-Soft-Nav':'1'}}).then(function(r){"
        "if(!r.ok)throw new Error('nav failed');"
        "return r.text().then(function(html){return{html:html,finalUrl:r.url};});"
        "}).then(function(res){"
        "var doc=new DOMParser().parseFromString(res.html,'text/html');"
        "var newMain=doc.querySelector('main.wrap');"
        "if(!newMain){location.href=url;return;}"
        "document.querySelector('main.wrap').innerHTML=newMain.innerHTML;"
        "reExecuteScripts(document.querySelector('main.wrap'));"
        "var newNav=doc.querySelector('header.top nav');"
        "var navEl=document.querySelector('header.top nav');"
        "if(navEl&&newNav)navEl.innerHTML=newNav.innerHTML;"
        "document.title=doc.title;"
        "if(pushState)history.pushState({},'',res.finalUrl);"
        "window.scrollTo(0,0);"
        "}).catch(function(){location.href=url;});"
        "}"
        "}else{"
        // This whole branch only runs when STANDALONE is true (see its
        // own var declaration above), which is exactly when jbclose
        // exists in the DOM -- but guarding the lookup anyway costs
        // nothing and survives this branch ever being restructured
        // later without silently reintroducing a null-deref crash.
        "var closeBtn=document.getElementById('jbclose');"
        "if(closeBtn)closeBtn.addEventListener('click',function(){window.close();});"
        "ensurePlayer(function(){});"
        "currentConnectedStation=activeStationId();connectActive(currentConnectedStation);"
        "var floatBtn=document.getElementById('jbfloat');"
        "if('documentPictureInPicture' in window){"
        "floatBtn.hidden=false;"
        "floatBtn.addEventListener('click',function(){"
        "documentPictureInPicture.requestWindow({width:340,height:300}).then(function(pipWin){"
        "Array.prototype.forEach.call(document.styleSheets,function(sheet){"
        "try{"
        "if(sheet.ownerNode&&sheet.ownerNode.tagName==='STYLE'){"
        "var style=document.createElement('style');"
        "style.textContent=sheet.ownerNode.textContent;"
        "pipWin.document.head.appendChild(style);"
        "}"
        "}catch(e){}"
        "});"
        "pipWin.document.body.style.margin='0';"
        "pipWin.document.body.appendChild(document.getElementById('jbroot'));"
        "pipWin.addEventListener('pagehide',function(){"
        "document.body.appendChild(document.getElementById('jbroot'));"
        "});"
        "}).catch(function(e){"
        "console.error('Float above failed:',e);"
        "floatBtn.textContent='Couldn\\u2019t float \\u2014 try again';"
        "setTimeout(function(){floatBtn.textContent='Float above';},2500);"
        "});"
        "});"
        "}"
        "}"
        "tabC.addEventListener('click',function(){switchMode('community');});"
        "tabS.addEventListener('click',function(){switchMode('search');});"
        "playPauseBtn.addEventListener('click',function(){"
        "if(!playerReady)return;"
        "if(ytPlayer.getPlayerState()===YT.PlayerState.PLAYING){ytPlayer.pauseVideo();personalFreezeBeacon();}"
        "else{if(mode==='community'&&lastCommunityId)ytPlayer.seekTo(saneStart(lastCommunityElapsed),true);ytPlayer.playVideo();}"
        "});"
        "muteBtn.addEventListener('click',function(){"
        "if(!ytPlayer)return;"
        "muted=!muted;"
        "if(muted){ytPlayer.mute();muteBtn.textContent='Unmute';}else{ytPlayer.unMute();muteBtn.textContent='Mute';}"
        "});"
        "volumeSlider.addEventListener('input',function(){"
        "if(ytPlayer)ytPlayer.setVolume(Number(volumeSlider.value));"
        "});"
        // YouTube's own embedded player frequently pauses itself when its
        // tab goes to the background (a documented IFrame Player quirk,
        // distinct from — and in addition to — generic browser tab
        // throttling). Catch back up to the live community position
        // instead of leaving it stalled whenever the tab is visible again.
        "document.addEventListener('visibilitychange',function(){"
        "if(!document.hidden&&mode==='community'&&playerReady&&lastCommunityId){"
        "if(ytPlayer.getPlayerState()!==YT.PlayerState.PLAYING){"
        "ytPlayer.seekTo(saneStart(lastCommunityElapsed),true);ytPlayer.playVideo();"
        "}"
        "}"
        "});"
        "updateTabs();renderStations();renderStationAdd();"
        "})();</script>";
    return s;
}

// The site-wide station (id "") plus every community this account belongs
// to, as a JS array-literal source -- not strict JSON, embedded directly
// into jukebox_script's <script> via string concatenation, so each field
// goes through js_string_literal the same way every other server string
// gets safely embedded into inline JS on this site.
std::string jukebox_stations_js(const std::string& user) {
    std::ostringstream os;
    os << "[{id:" << js_string_literal("") << ",name:" << js_string_literal("Site-wide") << "}";
    if (!user.empty()) {
        for (const auto& c : g_store.communities_of(user)) {
            os << ",{id:" << js_string_literal(c.slug) << ",name:" << js_string_literal(c.name) << "}";
        }
        // Personal channels this account has already subscribed to (see
        // FileStore::subscribe_jukebox) -- embedded here so a returning
        // visit shows the right name in the station list without the
        // widget needing to ask the server about each id one at a time.
        // Anyone can still *add* a new personal channel from the widget by
        // typing a username, which is what actually calls /subscribe.
        for (const auto& owner : g_store.jukebox_subscriptions_of(user)) {
            os << ",{id:" << js_string_literal("u:" + owner) << ",name:" << js_string_literal(owner + "\xE2\x80\x99s Juketube") << "}";
        }
    }
    os << "]";
    return os.str();
}

// Floating embedded widget: jukebox-icon toggle + collapsible panel,
// injected on every logged-in page via wrap(). "Pop out" opens /listen in a real
// separate window — see jukebox_script's comment for why that, and not
// requestPictureInPicture, is what actually survives navigating this
// (deliberately multi-page) site.
std::string global_jukebox_widget(const std::string& yt_key, const std::string& user) {
    (void)yt_key; (void)user;
    return "<link rel=\"stylesheet\" href=\""+asset_url("juketube.css")+"\">"
           "<div id=\"juketube-root\"></div>"
           "<script src=\""+asset_url("juketube.js")+"\" defer></script><script src=\""+asset_url("push.js")+"\" defer></script>";
}

std::string jukebox_standalone_page(const std::string& yt_key, const std::string& user) {
    return "<div id=\"jbroot\" style=\"max-width:400px;margin:0 auto\">"
           + jukebox_panel_body(true) + "</div>"
           + jukebox_script(yt_key, true, jukebox_stations_js(user), user);
}

// The "Add a track" input + live search box, shared between the owner's own
// queue and a guest's one-shot request -- identical UI, different POST
// target and different note underneath it.
std::string jukebox_add_form(const std::string& post_target, const std::string& note) {
    return "<div><label>Add a track</label>"
            "<div class=\"row\" style=\"align-items:flex-start\">"
            "<input id=\"jinput\" placeholder=\"Paste a link or type to search\" autocomplete=\"off\" style=\"flex:1\">"
            "<button type=\"button\" id=\"jaddbtn\">Add</button>"
            "</div>"
            "<div id=\"jsresults\" style=\"margin-top:.4rem;display:flex;flex-direction:column;gap:.2rem\"></div>"
            "<p class=\"warn\" id=\"jaerr\" style=\"margin:.3rem 0 0\" hidden></p>"
            "</div>"
            "<p class=\"muted\">"+note+"</p>"
            "<script>(function(){"
            // Restores the scroll position saved by queueUrl()'s reload
            // path below, on the very next load of this same page.
            "var savedY=sessionStorage.getItem('lg_scroll');"
            "if(savedY!==null){sessionStorage.removeItem('lg_scroll');window.scrollTo(0,parseInt(savedY,10)||0);}"
            "var target="+js_string_literal(post_target)+";"
            "var input=document.getElementById('jinput'),out=document.getElementById('jsresults');"
            "var errEl=document.getElementById('jaerr');"
            "var lastTopId=null,timer=null,gen=0;"
            "function esc(s){"
            "var ta=document.createElement('textarea');ta.innerHTML=s;"
            "var d=document.createElement('div');d.textContent=ta.value;return d.innerHTML;"
            "}"
            "function looksLikeUrl(s){return /^https?:\\/\\//i.test(s)||/youtu\\.?be/i.test(s);}"
            // A picked/pasted track used to always window.location=... ,
            // a full navigation that scrolls the page back to the top
            // even though nothing about the layout actually moved -- so
            // this used to try updating #jbplayer's src in place instead
            // of reloading. But #jbplayer on THIS page is the floating
            // widget's own player -- always a <div> here, now that the
            // profile's own embedded iframe is gone -- and setting .src
            // on a div does nothing. Only take that shortcut when
            // #jbplayer is genuinely an <iframe> (the /listen standalone
            // page still embeds one); otherwise a plain reload (with
            // scroll restored, not reset to the top) is the only way the
            // new track actually shows up in this list.
            "function queueUrl(url){"
            "errEl.hidden=true;"
            "fetch(target,{method:'POST',credentials:'same-origin',"
            "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
            "body:'url='+encodeURIComponent(url)}).then(function(r){"
            "var dest=r.url||location.href;"
            "var failed=/[?&]err=/.test(dest);"
            "var player=document.getElementById('jbplayer');"
            "var m=url.match(/[?&]v=([A-Za-z0-9_-]{11})/)||url.match(/youtu\\.be\\/([A-Za-z0-9_-]{11})/);"
            "if(!failed&&player&&player.tagName==='IFRAME'&&m){"
            "player.src='https://www.youtube-nocookie.com/embed/'+m[1]+'?rel=0&enablejsapi=1';"
            "return;"
            "}"
            "sessionStorage.setItem('lg_scroll',String(window.scrollY));"
            "location.reload();"
            "}).catch(function(){"
            "errEl.textContent='Could not reach the server. Try again.';errEl.hidden=false;"
            "});"
            "}"
            "function render(items){"
            "out.innerHTML='';lastTopId=null;"
            "if(!items||!items.length)return;"
            "items.forEach(function(it,idx){"
            "var id=it.id.videoId,title=it.snippet.title,thumb=it.snippet.thumbnails.default.url;"
            "var row=document.createElement('div');row.className='row';"
            "row.style.cssText='align-items:center;cursor:pointer;padding:.3rem;border-radius:var(--rsm)';"
            "row.innerHTML='<img src=\"'+thumb+'\" width=\"60\" height=\"45\" alt=\"\" style=\"border-radius:4px\">"
            "<span style=\"flex:1\">'+esc(title)+'</span>';"
            "row.title='Click to queue this track';"
            "row.addEventListener('click',function(){"
            "queueUrl('https://www.youtube.com/watch?v='+id);"
            "});"
            "out.appendChild(row);"
            "if(idx===0){lastTopId=id;row.style.background='var(--parchment)';}"
            "});"
            "}"
            "function search(term){"
            "if(!term){out.textContent='';return;}"
            "var my=++gen;"
            "out.textContent='Searching...';"
            "fetch('/jukebox/search?q='+encodeURIComponent(term),{credentials:'same-origin'})"
            ".then(function(r){if(!r.ok)throw new Error('http');return r.json();})"
            ".then(function(data){if(my!==gen)return;render(data.items||[]);})"
            ".catch(function(){if(my!==gen)return;out.textContent='Search failed.';});"
            "}"
            "input.addEventListener('input',function(){"
            "var val=input.value.trim();"
            "clearTimeout(timer);"
            "if(!val||looksLikeUrl(val)){out.innerHTML='';lastTopId=null;return;}"
            "timer=setTimeout(function(){search(val);},300);"
            "});"
            "function doQueue(){"
            "var val=input.value.trim();if(!val)return;"
            "if(looksLikeUrl(val))queueUrl(val);"
            "else if(lastTopId)queueUrl('https://www.youtube.com/watch?v='+lastTopId);"
            "else search(val);"
            "}"
            "input.addEventListener('keydown',function(e){if(e.key==='Enter'){e.preventDefault();doQueue();}});"
            "var addBtn=document.getElementById('jaddbtn');"
            "if(addBtn)addBtn.addEventListener('click',doQueue);"
            "})();</script>";
}

// Page-local jukebox widget (navigation option A: MPA, own instance per
// page, stops on navigate away — no site-wide SPA, no WebSocket sync).
// The owner always curates their own queue -- persistent, Prev/Next
// browsing, no cap beyond kJukeboxQueueCap. Depending on the owner's
// JukeboxPolicy, a signed-in friend or anyone else may also queue one
// track at a time: a one-shot guest request that's removed and logged to
// history the moment the embedded player reports it ended (see
// complete_jukebox_track). `viewer` is the signed-in username, or empty
// for an anonymous visitor.
// A CoverFlow-style browser for the owner's own queue on /me: the focused
// track's thumbnail faces the viewer, the rest recede at an angle to
// either side like records on a shelf. Arrow clicks and swipes (a harder
// swipe skips further, scaled by release velocity) move the focus;
// title/creator for the focused track comes from GET /jukebox/oembed (no
// API key needed, unlike the Data API v3 search this file also proxies)
// fetched on demand and cached client-side per session -- JukeboxTrack
// itself still only ever stores a video id, exactly as before.
std::string jukebox_coverflow_html(const std::vector<JukeboxTrack>& tracks, const std::string& owner) {
    std::string tracks_json = "[";
    for (std::size_t i = 0; i < tracks.size(); ++i) {
        if (i) tracks_json += ",";
        tracks_json += "{\"id\":\""+json_escape(tracks[i].video_id)+"\",\"by\":\""+
                        json_escape(tracks[i].queued_by)+"\"}";
    }
    tracks_json += "]";
    return
        "<style>"
        "#jbcoverflow{position:relative;display:flex;align-items:center;justify-content:center;"
        "height:160px;margin:.8rem 0;touch-action:pan-y;user-select:none}"
        "#jbcftrack{position:relative;width:100%;height:100%;perspective:900px}"
        ".jbcf-item{position:absolute;top:50%;left:50%;width:120px;height:120px;margin:-60px 0 0 -60px;"
        "border-radius:var(--rsm);overflow:hidden;cursor:pointer;background:#000;"
        "transition:transform .35s ease,opacity .35s ease;box-shadow:0 4px 16px rgba(0,0,0,.3)}"
        ".jbcf-item img{width:100%;height:100%;object-fit:cover;display:block}"
        ".jbcf-arrow{background:var(--panel);border:1px solid var(--rule);border-radius:50%;"
        "width:36px;height:36px;font-size:1.3rem;line-height:1;cursor:pointer;z-index:20;flex-shrink:0;"
        "color:var(--ink)}"
        "#jbcfcaption{text-align:center;margin:.2rem 0 .5rem;min-height:2.6em}"
        "#jbcfcaption strong{display:block}"
        "</style>"
        "<div id=\"jbcoverflow\">"
        "<button type=\"button\" class=\"jbcf-arrow\" id=\"jbcfprev\" aria-label=\"Previous track\">&#8249;</button>"
        "<div id=\"jbcftrack\"></div>"
        "<button type=\"button\" class=\"jbcf-arrow\" id=\"jbcfnext\" aria-label=\"Next track\">&#8250;</button>"
        "</div>"
        "<p id=\"jbcfcaption\"><strong id=\"jbcftitle\"></strong><span class=\"muted\" id=\"jbcfmeta\"></span></p>"
        "<p class=\"row\"><button type=\"button\" class=\"ghost\" id=\"jbcfremove\">Remove this track</button></p>"
        "<script>(function(){"
        "var TRACKS="+tracks_json+";"
        "var OWNER="+js_string_literal(owner)+";"
        "var focus=0;"
        "var track=document.getElementById('jbcftrack');"
        "var titleEl=document.getElementById('jbcftitle'),metaEl=document.getElementById('jbcfmeta');"
        "var wrap=document.getElementById('jbcoverflow'),cap=document.getElementById('jbcfcaption');"
        "var removeBtn=document.getElementById('jbcfremove');"
        "var oembedCache={};"
        "function render(){"
        "track.innerHTML='';"
        "TRACKS.forEach(function(t,i){"
        "var off=i-focus;"
        "if(Math.abs(off)>3)return;"
        "var el=document.createElement('div');el.className='jbcf-item';"
        "var img=document.createElement('img');"
        "img.src='https://img.youtube.com/vi/'+t.id+'/hqdefault.jpg';img.alt='';"
        "el.appendChild(img);"
        "var angle=off*32,tx=off*72,tz=-Math.abs(off)*70,scale=off===0?1:.82;"
        "el.style.transform='translateX('+tx+'px) translateZ('+tz+'px) rotateY('+(-angle)+'deg) scale('+scale+')';"
        "el.style.zIndex=String(100-Math.abs(off));"
        "el.style.opacity=String(Math.max(0,1-Math.abs(off)*.3));"
        "el.addEventListener('click',function(){if(i!==focus){focus=i;update();}});"
        "track.appendChild(el);"
        "});"
        "}"
        "function loadCaption(){"
        "var t=TRACKS[focus];"
        "if(!t){titleEl.textContent='';metaEl.textContent='';return;}"
        // "queued by <you>" on your own curated pick is just noise here --
        // only worth saying when a guest actually requested it.
        "var pos='Track '+(focus+1)+' of '+TRACKS.length;"
        "metaEl.textContent=(t.by&&t.by!==OWNER)?(pos+' \\u00b7 queued by '+t.by):pos;"
        "if(oembedCache[t.id]){titleEl.textContent=oembedCache[t.id];return;}"
        "titleEl.textContent='Loading\\u2026';"
        "fetch('/jukebox/oembed?id='+encodeURIComponent(t.id)).then(function(r){return r.json();})"
        ".then(function(d){"
        "var txt=d.title?(d.title+(d.author_name?' \\u2014 '+d.author_name:'')):t.id;"
        "oembedCache[t.id]=txt;"
        "if(TRACKS[focus]===t)titleEl.textContent=txt;"
        "}).catch(function(){if(TRACKS[focus]===t)titleEl.textContent=t.id;});"
        "}"
        "function update(){"
        "if(!TRACKS.length){wrap.hidden=true;cap.hidden=true;removeBtn.parentNode.hidden=true;return;}"
        "render();loadCaption();"
        "}"
        "function move(delta){"
        "focus=Math.max(0,Math.min(TRACKS.length-1,focus+delta));"
        "update();"
        "}"
        "document.getElementById('jbcfprev').addEventListener('click',function(){move(-1);});"
        "document.getElementById('jbcfnext').addEventListener('click',function(){move(1);});"
        "removeBtn.addEventListener('click',function(){"
        "var t=TRACKS[focus];if(!t)return;"
        "fetch('/me/jukebox/remove',{method:'POST',credentials:'same-origin',"
        "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
        "body:'video_id='+encodeURIComponent(t.id)}).then(function(){"
        "TRACKS.splice(focus,1);"
        "if(focus>=TRACKS.length)focus=TRACKS.length-1;"
        "update();"
        "});"
        "});"
        // Swipe: drag distance decides direction, release velocity decides
        // how many tracks to skip -- a light flick moves one, a hard flick
        // moves several.
        "var startX=null,startT=0;"
        "wrap.addEventListener('pointerdown',function(e){startX=e.clientX;startT=Date.now();});"
        "wrap.addEventListener('pointerup',function(e){"
        "if(startX===null)return;"
        "var dx=e.clientX-startX,dt=Math.max(1,Date.now()-startT);"
        "startX=null;"
        "if(Math.abs(dx)<20)return;"
        "var velocity=Math.abs(dx)/dt;"
        "var skip=Math.max(1,Math.round(velocity*3.5));"
        "move(dx<0?skip:-skip);"
        "});"
        "wrap.addEventListener('pointercancel',function(){startX=null;});"
        "update();"
        "})();</script>";
}

std::string jukebox_widget(const std::string& owner, bool owner_view, const std::string& viewer,
                            const std::map<std::string,std::string>& qs) {
    auto tracks = g_store.jukebox_of(owner);
    auto owner_prefs = g_store.prefs_of(owner);
    const char* policy_badge = owner_prefs.jukebox_policy==JukeboxPolicy::Anyone ? "anyone may queue"
        : owner_prefs.jukebox_policy==JukeboxPolicy::Friends ? "friends may queue" : "owner curated";
    std::string html = "<div class=\"card jukebox\"><h2>Jukebox</h2>"
                        "<p class=\"muted eyebrow\">"+std::string(policy_badge);
    if (owner_view) {
        const auto n = g_store.jukebox_subscriber_count(owner);
        html += " &middot; "+std::to_string(n)+(n==1?" subscriber":" subscribers");
    }
    html += "</p>";
    html += flash_error(qs);

    const bool is_guest = !owner_view && !viewer.empty();
    const bool is_friend = is_guest && g_store.are_friends(viewer, owner);
    const bool blocked = is_guest && g_store.is_blocked(owner, viewer);
    bool has_pending = false;
    if (is_guest) for (const auto& t : tracks) if (t.queued_by == viewer) { has_pending = true; break; }
    bool policy_allows = false;
    if (is_guest && !blocked) {
        if (owner_prefs.jukebox_policy == JukeboxPolicy::Anyone) policy_allows = true;
        else if (owner_prefs.jukebox_policy == JukeboxPolicy::Friends) policy_allows = is_friend;
    }
    const bool guest_can_add = is_guest && policy_allows && !has_pending;

    // What's actually playing here can be a relay (see "Play this Juketube
    // instead" on /me) -- resolved at most one hop, never chased further.
    // Two players on one page was the actual bug this replaces: no more
    // embedded iframe, no Prev/Next-as-page-nav, no second YT.Player. This
    // card is just text + a thumbnail; the real playback -- for owner and
    // visitor alike -- happens once, in the floating widget, via "Play in
    // Juketube" below.
    const std::string playback_owner = g_store.jukebox_effective_owner(owner);
    const bool relayed = playback_owner != owner;
    auto playing = g_store.jukebox_of(playback_owner);
    if (relayed) {
        html += "<p class=\"muted\">Playing <a href=\"/u/"+html_escape(playback_owner)+"\">"+
                html_escape(playback_owner)+"</a>'s Juketube.</p>";
    }
    if (playing.empty()) {
        html += "<p class=\"empty\">Nothing playing.</p>";
    } else {
        const auto& t = playing.front();
        html += "<div class=\"row\" style=\"align-items:center;gap:.6rem\">"
                "<img src=\"https://img.youtube.com/vi/"+html_escape(t.video_id)+"/hqdefault.jpg\" "
                "width=\"96\" height=\"72\" alt=\"\" style=\"border-radius:var(--rsm);flex-shrink:0\">"
                "<div><strong>Now playing</strong><br><span class=\"muted\">queued by "+
                html_escape(t.queued_by)+"</span></div></div>"
                "<p class=\"muted\">"+std::to_string(playing.size()-1)+
                (playing.size()==2?" track":" tracks")+" queued next</p>";
    }
    if (!viewer.empty()) {
        html += "<p class=\"row\"><button type=\"button\" class=\"btn\" "
                "onclick=\"window.__lgTuneStation&amp;&amp;window.__lgTuneStation('u:"+owner+"')\">"
                "Play in Juketube</button></p>";
    }
    if (owner_view) {
        html += "<div style=\"margin-top:.6rem\"><label>Your playlist</label>";
        if (tracks.empty()) {
            html += "<p class=\"empty\">Nothing queued yet.</p>";
        } else {
            html += jukebox_coverflow_html(tracks, owner);
        }
        html += "</div>";
    }

    if (owner_view) {
        // Only true of this in-page embed: a visitor who followed this
        // channel in their own jukebox widget instead (see jukebox_script's
        // Follow button, /jukebox/subscribe) keeps listening as they browse
        // elsewhere -- the widget polls /jukebox/now independently of this
        // page. Who-can-queue is an access-control setting, not profile
        // content -- moved to Settings > Privacy (Brandon: "its not
        // appropriate" on the profile page); the eyebrow badge above still
        // shows the current policy here, read-only.
        html += jukebox_add_form("/me/jukebox/add",
            "Visitors listen here as long as they stay on this page -- following it in the jukebox widget keeps it playing as they browse.");
    } else if (guest_can_add) {
        html += jukebox_add_form("/u/"+owner+"/jukebox/add",
            "One request at a time. It plays once, then it's gone. Follow this channel in the jukebox widget to keep listening as you browse elsewhere.");
    } else if (is_guest && has_pending) {
        html += "<p class=\"muted\">You already have a track queued here.</p>";
    } else if (is_guest && !blocked && owner_prefs.jukebox_policy == JukeboxPolicy::Friends) {
        html += "<p class=\"muted\">Only "+html_escape(owner)+"'s friends can queue a track here.</p>";
    }

    auto hist = g_store.jukebox_history_of(playback_owner);
    if (!hist.empty()) {
        // One row per track, most recently played first, with its play count and last played date; 5 show and the
        // rest sit under "Show more" (Brandon 2026-11-11 4:37 PM, Brief 4 item 6: nine of ten rows were one track,
        // each a bare "watch again"). The title comes from /jukebox/oembed in the page (signed-in viewers; nothing
        // stores titles); until then, and for visitors who are signed out, the link says "YouTube video".
        struct HistRow { std::string id; std::size_t plays = 0; std::int64_t last = 0; };
        std::vector<HistRow> rows; // history is oldest first
        for (auto it = hist.rbegin(); it != hist.rend(); ++it) {
            auto r = std::find_if(rows.begin(), rows.end(), [&](const HistRow& x) { return x.id == it->video_id; });
            if (r == rows.end()) rows.push_back(HistRow{it->video_id, 1, it->played_at});
            else ++r->plays;
        }
        auto row = [](const HistRow& r) {
            const std::string id = html_escape(r.id);
            return "<li class=\"jbhist-row muted\" data-yt=\""+id+"\" style=\"display:flex;align-items:center;gap:.4rem;margin:.3rem 0\">"
                   "<img src=\"https://img.youtube.com/vi/"+id+"/default.jpg\" "
                   "width=\"40\" height=\"30\" alt=\"\" style=\"border-radius:4px;flex:none\">"
                   "<span style=\"min-width:0;overflow-wrap:anywhere\"><a class=\"jbtitle\" data-yt=\""+id+"\" "
                   "href=\"https://www.youtube.com/watch?v="+id+"\" target=\"_blank\" rel=\"noopener\">YouTube video</a>"
                   " &middot; "+std::to_string(r.plays)+(r.plays==1 ? " play" : " plays")+
                   " &middot; last "+html_escape(fmt_time(r.last))+"</span></li>";
        };
        const std::string ul = "<ul style=\"list-style:none;padding:0;margin:0\">";
        html += "<div class=\"jbhist\" style=\"margin-top:1rem\"><label>Recently played</label>"+ul;
        for (std::size_t i = 0; i < rows.size() && i < 5; ++i) html += row(rows[i]);
        html += "</ul>";
        if (rows.size() > 5) {
            html += "<details class=\"jbhist-more\"><summary>Show more</summary>"+ul;
            for (std::size_t i = 5; i < rows.size(); ++i) html += row(rows[i]);
            html += "</ul></details>";
        }
        html += "</div>";
        if (!viewer.empty())
            html += "<script>(function(){"
                    "document.querySelectorAll('a.jbtitle[data-yt]').forEach(function(a){"
                    "fetch('/jukebox/oembed?id='+encodeURIComponent(a.getAttribute('data-yt')))"
                    ".then(function(r){return r.ok?r.json():{};})"
                    ".then(function(d){if(d&&d.title)a.textContent=d.title;})"
                    // err-ok: no title is fine, the link keeps saying YouTube video
                    ".catch(function(){});"
                    "});"
                    "})();</script>";
    }

    html += "</div>";
    return html;
}

std::string genesis_body() {
    // A public stats strip, seen in the mobile design's landing screen --
    // real counts (account_count/all_communities), not the design's own
    // placeholder "1,284"; elections held stays honestly 0, same posture
    // as the /control voting card.
    std::string stats =
        "<div class=\"stattiles\">"
        "<div class=\"stattile\"><strong>" + std::to_string(g_store.account_count()) + "</strong><span>" +
        std::string(g_store.account_count()==1?"Account":"Accounts") + "</span></div>"
        "<div class=\"stattile\"><strong>" + std::to_string(g_store.all_communities().size()) + "</strong><span>" +
        std::string(g_store.all_communities().size()==1?"Group":"Groups") + "</span></div>"
        "<div class=\"stattile\"><strong>0</strong><span>Elections held</span></div>"
        "</div>";
    // The scrolling problems ticker and the two pull-quotes, pulled from
    // the actual authored content in the mobile design's own source (its
    // PROBLEMS array and its two quote blocks) -- not from the rendered
    // preview, which never resolved its {{ }} bindings locally. Same list
    // doubled back-to-back so the CSS loop has no visible seam.
    static const std::vector<std::string> problems = {
        "Taxpayer money funding pedophilia", "State-funded propaganda",
        "Overclassification of official information", "Corporate money buying legislation",
        "Bureaucracy that answers to no one", "Endless foreign wars",
        "Federal land locked away from use", "Agricultural mismanagement",
        "One law for them, another for you", "Qualified immunity for abuse of power",
        "Emergency powers that never expire",
    };
    std::string ticker;
    for (int rep = 0; rep < 2; ++rep)
        for (const auto& p : problems)
            ticker += "<div>" + html_escape(p) + "</div>";
    std::string solution =
        "<div class=\"row solution-row\" style=\"align-items:stretch;gap:1rem;flex-wrap:wrap\">"
        "<div style=\"flex:2 1 12rem;min-width:12rem;display:flex;flex-direction:column\">"
        "<p class=\"eyebrow\" style=\"color:var(--accent)\">A legitimate solution to:</p>"
        "<div class=\"ticker-wrap\"><div class=\"ticker-roll\">" + ticker + "</div></div>"
        "</div>"
        "<div class=\"card isnt-card\">"
        "<p class=\"eyebrow\" style=\"color:var(--accent)\">Lead Yourself &mdash; together</p>"
        "<p style=\"margin:.6rem 0\"><strong>Not a party</strong><br>"
        "<span class=\"muted\">Nothing here asks you to pick a side, and no side owns it.</span></p>"
        "<p style=\"margin:.6rem 0\"><strong>Not a government</strong><br>"
        "<span class=\"muted\">No election has been held. Nothing here binds you to anything.</span></p>"
        "<p style=\"margin:.6rem 0\"><strong>Not a protest</strong><br>"
        "<span class=\"muted\">It is a structure you can test, argue with, and walk away from.</span></p>"
        "<p style=\"margin:.6rem 0\"><strong>Not for sale</strong><br>"
        "<span class=\"muted\">Your account is not a product and it is not anyone else's leverage.</span></p>"
        "</div></div>";
    // Two full-width cards, stacked -- not side by side. Matches the
    // source exactly: only the second (anonymous) quote is centered and
    // carries stars; the first (attributed) quote is left-aligned,
    // unstarred, on a plain card, same as the design's own two different
    // treatments for a named vs. an anonymous testimonial.
    std::string quotes =
        "<p class=\"eyebrow\" style=\"color:var(--accent)\">Consider Incentives</p>"
        "<div class=\"card quote\"><p>&ldquo;These aren't &lsquo;people problems.&rsquo; They're structural "
        "problems. Good people placed inside broken structures produce broken outcomes. Broken people placed "
        "inside well-designed structures are naturally corrected by them.&rdquo;</p>"
        "<p class=\"attrib\">&mdash; Brandon Bottin</p></div>"
        "<div class=\"card quote centered\"><p class=\"stars\">&#9733;&#9733;&#9733;&#9733;&#9733;</p>"
        "<p>&ldquo;It's like Myspace had a three-way with a ballot box and a gameboy.&rdquo;</p>"
        "<p class=\"attrib muted\">&mdash; Anonymous</p></div>";
    // The decorative emblem from the top of the design's own welcome
    // screen -- a triangle (the three laws) with a droplet at its
    // center and a ringed node at each vertex, wrapped in a faint
    // circle. Pure SVG with no text content, which is why it never
    // showed up in an accessibility-tree read of the design -- found by
    // reading the .dc.html's raw markup instead.
    std::string emblem =
        "<div style=\"text-align:center\"><svg viewBox=\"0 0 180 172\" width=\"140\" height=\"134\" aria-hidden=\"true\">"
        "<circle cx=\"90\" cy=\"96\" r=\"54\" fill=\"none\" style=\"stroke:var(--rule)\" stroke-width=\"1\"/>"
        "<path d=\"M90 30 L32 130 L148 130 Z\" fill=\"none\" style=\"stroke:var(--stone)\" stroke-width=\"1\"/>"
        "<path d=\"M90 70c0 0 13 17.5 13 24.5a13 13 0 0 1-26 0C77 87.5 90 70 90 70z\" style=\"fill:var(--accent)\"/>"
        "<circle cx=\"90\" cy=\"30\" r=\"6\" style=\"fill:var(--paper);stroke:var(--accent)\" stroke-width=\"1.4\"/>"
        "<circle cx=\"32\" cy=\"130\" r=\"6\" style=\"fill:var(--paper);stroke:var(--accent)\" stroke-width=\"1.4\"/>"
        "<circle cx=\"148\" cy=\"130\" r=\"6\" style=\"fill:var(--paper);stroke:var(--accent)\" stroke-width=\"1.4\"/>"
        "</svg></div>";
    std::string lawstrip =
        "<div class=\"lawstrip\">"
        "<div><p class=\"n\">01</p><p class=\"t\">Do not steal</p></div>"
        "<div><p class=\"n\">02</p><p class=\"t\">Do not damage</p></div>"
        "<div><p class=\"n\">03</p><p class=\"t\">Do not deprive of liberty</p></div>"
        "</div>"
        "<p class=\"muted\" style=\"text-align:center;max-width:22rem;margin:.7rem auto 0\">"
        "Three rules hold. Whoever you are, wherever you're starting from, the rest is your call.</p>";
    // The Commons is closed until it is built. No walk-in on this page.
    std::string inside;
    // Real recorded narration (Helios), served from /assets/chapter1.mp3.
    // The latest file already starts on the first sentence (no spoken
    // title), so no #t= skip is needed anymore. No native <audio controls>
    // chrome -- the pulsing round button IS the control, a bare <audio>
    // with no `controls` attribute renders nothing on its own. Icon
    // markup is defined once in C++ and reused for both the server-
    // rendered initial state and the JS reset-on-ended/pause swap (via
    // js_string_literal, not hand-escaped) so the two can't drift.
    // A play triangle reads unambiguously as "tap to hear this" at 16px;
    // the earlier speaker-with-sound-waves glyph didn't (too many thin
    // strokes at this size, and a pale outline against parchment barely
    // registered as a button at all). Solid accent-filled circle + paper-
    // colored icon fixes both the contrast and the "is this tappable"
    // question in one move.
    const char* listenPlayIcon =
        "<svg viewBox=\"0 0 24 24\" width=\"16\" height=\"16\" fill=\"currentColor\" aria-hidden=\"true\">"
        "<path d=\"M6 4l14 8-14 8V4z\"/></svg>";
    const char* listenPauseIcon =
        "<svg viewBox=\"0 0 24 24\" width=\"16\" height=\"16\" fill=\"currentColor\" aria-hidden=\"true\">"
        "<rect x=\"5\" y=\"4\" width=\"5\" height=\"16\" rx=\"1\"/>"
        "<rect x=\"14\" y=\"4\" width=\"5\" height=\"16\" rx=\"1\"/></svg>";
    std::string listen_audio =
        "<button type=\"button\" class=\"listen-pulse\" aria-label=\"Listen to Chapter 1\" "
        "aria-pressed=\"false\">" + std::string(listenPlayIcon) + "</button>"
        "<audio id=\"ch1audio\" preload=\"none\" src=\"/assets/chapter1.mp3\"></audio>"
        "<script>(function(){"
        "var btn=document.querySelector('.listen-pulse');"
        "if(!btn)return;"
        "var audio=document.getElementById('ch1audio');"
        "var playIcon="+js_string_literal(listenPlayIcon)+";"
        "var pauseIcon="+js_string_literal(listenPauseIcon)+";"
        "function reset(){"
        "btn.classList.remove('playing');"
        "btn.setAttribute('aria-pressed','false');"
        "btn.innerHTML=playIcon;"
        "}"
        "audio.addEventListener('ended',reset);"
        // The intro overlay (#lgintro) removes itself from the DOM once
        // dismissed/finished -- its presence is enough to know it's still
        // up, no separate flag needed. Ignoring the tap rather than
        // queuing it: the visitor can just tap again once it's gone.
        "btn.addEventListener('click',function(){"
        "if(document.getElementById('lgintro'))return;"
        "if(audio.paused){"
        "audio.play();"
        "btn.classList.add('playing');"
        "btn.setAttribute('aria-pressed','true');"
        "btn.innerHTML=pauseIcon;"
        "}else{"
        "audio.pause();"
        "reset();"
        "}"
        "});"
        "})();</script>";
    // position:relative on the hero block + position:absolute on the
    // button (see .listen-pulse) pins it to the block's own top-right
    // corner. Scrolls away with the page like everything else here --
    // not position:fixed to the viewport, which would either need to
    // clear the sticky header by a guessed pixel amount or risk sitting
    // over the iOS install banner (also viewport-bottom-fixed) when
    // that's showing.
    return "<div style=\"position:relative\">" +
           listen_audio +
           emblem +
           "<p class=\"eyebrow\" style=\"color:var(--accent)\">A Voluntary Natural Democracy</p>"
           "<h1>This is YOUR government!</h1>"
           "<p class=\"lede\">Here you represent yourself, and accountability is universally distributed. "
           "Officials are incentivized to serve the people, not the other way around.</p>"
           "</div>"
           "<hr style=\"border:0;border-top:1px solid var(--rule);margin:1rem 0\">" +
           lawstrip +
           solution + quotes + inside +
           "<div class=\"card\"><p>This isn't a legal government entity — not a city, state, or federal "
           "authority, and it doesn't replace the one you already live under; that government still governs "
           "you exactly as it did before you found this site. What it is: a space to organize, collaborate, "
           "and build the systems a real government will need, so they're ready when the time comes to "
           "actually become one. No office seated from these pages carries legal authority over anyone who "
           "hasn't chosen to associate with it.</p>"
           "<p>Website control is temporary. When a working election exists among enrolled citizens, "
           "that control is meant to end. The internal shape of that election is itself a citizen vote. "
           "Real governmental authority is a later, explicit landslide — Phase 2 — not this page and not "
           "the first people who show up.</p>"
           "<p class=\"warn\">Until then, genesis operators run the machines and say so in public. "
           "That is a disclosure, not a claim of sovereignty.</p></div>"
           "<div class=\"clocks\">"
           "<div class=\"card\"><h2>Two clocks</h2><p><strong>Site control</strong> ends when a working election exists among enrolled citizens.</p>"
           "<p><strong>Governmental authority</strong> stays a later landslide — more than half of the eligible citizen population. However long that takes.</p></div>"
           "<div class=\"card\"><h2>This website is the door</h2><p>Feed, friends, mail, and groups live here. "
           "Voting weight, offices, land, and judgments live in a separate governance kernel. "
           "A platform account is not a citizen identity. Combined verification for civic acts is not live.</p></div>"
           "</div>" +
           stats;
}

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

std::string home_dashboard(const std::string& user) {
    const auto posts = g_store.posts_by(user).size();
    const auto friends = g_store.friends_of(user).size();
    const auto committees = g_store.communities_of(user);

    // The Commons walk-in card stays off until Brandon says the game is built.
    std::string quadCard;

    std::string rooms;
    for (const auto& c : committees) {
        bool on_air = false;
        {
            std::lock_guard<std::mutex> lock(g_community_mu);
            auto it = g_community_now_by_station.find(c.slug);
            on_air = (it != g_community_now_by_station.end() && it->second.active);
        }
        char ini_c = c.name.empty() ? '?' : static_cast<char>(std::toupper(static_cast<unsigned char>(c.name[0])));
        rooms += "<a class=\"roomrow\" href=\"/c/" + html_escape(c.slug) + "\"><span class=\"roomini\">" +
                 html_escape(std::string(1, ini_c)) + "</span>"
                 "<span class=\"roomname\">" + html_escape(c.name) + "<br><span class=\"muted\">/" +
                 html_escape(c.slug) + "</span></span>"
                 "<span class=\"roomlive\">" + (on_air ? "on air" : "quiet") + "</span></a>";
    }

    const bool first_hour = posts == 0 && friends == 0 && committees.empty();
    std::string next;
    if (first_hour) {
        next = "<p class=\"muted\">This is a platform account, not a citizen identity. Start with one of these:</p>"
               "<p class=\"row\"><a class=\"btn\" href=\"/friends\">Find someone</a> "
               "<a class=\"btn ghost\" href=\"/me\">Set a portrait</a> "
               "<a class=\"btn ghost\" href=\"/feed\">Write a post</a></p>";
    } else {
        next = "<p class=\"row\"><a class=\"btn\" href=\"/feed\">Open the feed</a> "
               "<a class=\"btn ghost\" href=\"/u/" + html_escape(user) + "\">Your profile</a></p>";
    }

    std::string out =
        "<div class=\"card\"><p class=\"eyebrow\">" + std::string(first_hour ? "You're in" : "Welcome back") +
        "</p><h1>@" + html_escape(user) + "</h1>"
        "<div class=\"stattiles\">"
        "<div class=\"stattile\"><strong>" + std::to_string(posts) + "</strong><span>" +
        std::string(posts==1?"Post":"Posts") + "</span></div>"
        "<div class=\"stattile\"><strong>" + std::to_string(friends) + "</strong><span>" +
        std::string(friends==1?"Friend":"Friends") + "</span></div>"
        "<div class=\"stattile\"><strong>" + std::to_string(committees.size()) + "</strong><span>" +
        std::string(committees.size()==1?"Group":"Groups") + "</span></div>"
        "</div>"
        + next + "</div>" +
        quadCard;

    auto friend_posts = g_store.feed_view(user, 6, FeedFilter::Friends);
    if (!friend_posts.empty()) {
        out += "<div class=\"card\"><h2>From people you know</h2>";
        for (const auto& p : friend_posts) out += post_block(p, user);
        out += "<p><a href=\"/feed\">Open the feed</a></p></div>";
    }

    Profile prof = g_store.profile_of(user);
    auto ci_has = [](std::string hay, std::string needle) {
        for (char& c : hay) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        for (char& c : needle) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (needle.size() < 3) return false;
        return hay.find(needle) != std::string::npos;
    };
    std::vector<std::string> interest_bits;
    {
        std::string cur;
        for (char c : prof.interests + "," + prof.hometown + "," + prof.living + "," + prof.career + "," + prof.school) {
            if (c==',' || c==';') {
                while (!cur.empty() && cur.front()==' ') cur.erase(cur.begin());
                while (!cur.empty() && cur.back()==' ') cur.pop_back();
                if (cur.size()>=3) interest_bits.push_back(cur);
                cur.clear();
            } else cur.push_back(c);
        }
        while (!cur.empty() && cur.front()==' ') cur.erase(cur.begin());
        while (!cur.empty() && cur.back()==' ') cur.pop_back();
        if (cur.size()>=3) interest_bits.push_back(cur);
    }
    std::string matched;
    for (const auto& c : g_store.all_communities()) {
        bool mine = false;
        for (const auto& m : committees) if (m.slug==c.slug) mine = true;
        if (mine) continue;
        std::string hay = c.name + " " + c.about + " " + c.subcategory + " " + c.slug;
        bool hit = false;
        for (const auto& bit : interest_bits) if (ci_has(hay, bit)) { hit = true; break; }
        if (!hit) continue;
        matched += "<a class=\"roomrow\" href=\"/c/" + html_escape(c.slug) + "\"><span class=\"roomname\">" +
                   html_escape(c.name) + "</span></a>";
    }
    if (!matched.empty())
        out += "<div class=\"card\"><h2>Might fit you</h2>" + matched + "</div>";
    if (!rooms.empty())
        out += "<div class=\"card\"><h2>Your groups</h2>" + rooms + "</div>";
    // a few people you may know, also the first thing after signup (it lands here)
    out += people_suggestions_html(user, 3, "/");

    if (friend_posts.empty() && matched.empty() && rooms.empty()) {
        auto sample = g_store.findable_sample(user, 6);
        auto allc = g_store.all_communities();
        std::string people, comms;
        for (auto& n : sample)
            people += "<div class=\"person\"><div class=\"who\">"+portrait_tag(n,"sm")+
                      "<a href=\"/u/"+html_escape(n)+"\"><strong>"+html_escape(n)+"</strong></a></div>"
                      "<a class=\"btn ghost\" href=\"/search?q="+html_escape(n)+"\">View</a></div>";
        std::size_t shown = 0;
        for (const auto& c : allc) {
            if (shown >= 6) break;
            comms += "<a class=\"roomrow\" href=\"/c/" + html_escape(c.slug) + "\"><span class=\"roomname\">" +
                     html_escape(c.name) + "</span></a>";
            ++shown;
        }
        out += "<div class=\"card\"><h2>Looking around</h2>"
               "<p class=\"muted\">Nothing is tagged to you yet, so this is a mix of people and groups on the site.</p>"
               + people + comms + "</div>";
    }
    return out;
}

// The mobile design reads the charter as a chaptered, one-screen-at-a-time
// flow ("Chapter 2 of 8 · 6 min", a "You can stop here" closer). The
// design's own 8-title outline had no body text behind it -- the real
// source turned out to be the actual AVND charter document Brandon
// pointed to directly, which already splits into 14 real, numbered
// sections. Used those 14 as-is rather than forcing the design's looser
// 8-title grouping onto text that doesn't actually divide that way.
//
// The charter describes the full Phase 2 civic mechanism (blockchain
// voting, land ledger, sortition courts, donation-based pay) that this
// platform does not implement and isn't claiming to -- but the document
// already says so itself (Section 13: "This initial adoption period
// constitutes Phase 1... Phase 2 does not begin automatically"), which
// is exactly this site's own existing posture elsewhere ("Real
// governmental authority is a later, explicit landslide -- Phase 2").
// Ported the substantive 14 sections verbatim; cut only the document's
// own closing meta paragraphs (a call for a co-builder, a P.S. email
// solicitation) since those are about writing the document, not part of
// its civic content.
struct LawChapter { std::string title; int minutes; std::string body; };

const std::vector<LawChapter>& law_chapters() {
    static const std::vector<LawChapter> chapters = {
        {"The Problem", 2,
         "<p>Every governance system in human history has faced the same fundamental vulnerability: power without consequence.</p>"
         "<p>When those who govern are insulated from the direct results of their decisions, three things reliably follow. "
         "Corruption takes root, because concentrated power attracts those seeking to exploit it. Incentives misalign, because "
         "officials become focused on keeping their position rather than serving the people who granted it. And accountability "
         "disappears, because the feedback loop between citizen dissatisfaction and official behavior is too slow, too indirect, "
         "and too easy to manipulate.</p>"
         "<p>These aren't &ldquo;people problems.&rdquo; They're structural problems. Good people placed inside broken structures "
         "produce broken outcomes. Broken people placed inside well-designed structures are naturally corrected by them.</p>"
         "<p>Representative democracy was designed for a world without the technology to allow direct citizen participation at "
         "scale. That world no longer exists. Yet we keep operating systems built on the assumption that citizens can't govern "
         "themselves — that they need professional intermediaries to translate their will into policy. Those intermediaries have, "
         "predictably, accumulated power, shielded themselves from consequence, and optimized for their own survival instead of "
         "the people they were meant to serve.</p>"
         "<p>Complex legal systems compound the problem. Law that requires professional interpretation to navigate inevitably "
         "benefits whoever can afford that navigation, while everyone else pays the toll. Law becomes a tool of consolidation "
         "instead of protection.</p>"
         "<p>This framework proposes something different — not a reform of existing systems, but a reconstruction from the ground "
         "up, built on natural law, voluntary participation, radical transparency, and real-time accountability. A system where "
         "everyone in it, from the newest citizen to the highest official, reaps exactly what they sow.</p>"},
        {"Foundational Principles", 2,
         "<p>A Voluntary Natural Democracy isn't built on ideology. It's built on observation. Ask one question — what kind of "
         "structure produces the best outcomes for the most people over the longest time? — and these principles are the answer.</p>"
         "<p><strong>Voluntarism.</strong> No participation is coerced. Citizens opt in. Officials serve because citizens choose "
         "to support them. Laws beyond the three universal laws are recommendations, not mandates. A government that has to earn "
         "its legitimacy every day is a government that can't afford to stop serving its people.</p>"
         "<p><strong>Anti-Authoritarianism.</strong> Authority here is always elected, temporary, conditional, and reversible. No "
         "official holds power that can't be withdrawn in real time. No law exists that can't be revised.</p>"
         "<p><strong>Radical Transparency.</strong> Every vote, donation, land transaction, and official decision is publicly "
         "documented on the national blockchain. Corruption requires darkness. Accountability requires light.</p>"
         "<p><strong>Natural Consequence.</strong> You reap what you sow. This runs through the whole system — the universal "
         "laws, the donation-based pay system, the land system, the judicial outcome record.</p>"
         "<p><strong>Diversity and Flexibility.</strong> Monoculture is toxic to any living system. This framework doesn't "
         "prescribe one way of living. It prescribes a structure within which communities determine those things for themselves, "
         "evolve, and correct their own mistakes.</p>"},
        {"The Three Universal Laws", 2,
         "<p>Most legal systems fail the same way governance systems fail — they accumulate complexity, and complexity "
         "concentrates power in the hands of whoever can navigate it.</p>"
         "<p>A Voluntary Natural Democracy recognizes three universal laws. They apply to every person on national territory, "
         "citizen or not, regardless of wealth or position. No one is above them, and no one is beneath their protection:</p>"
         "<p><strong>Do not steal. Do not damage. Do not deprive of liberty.</strong></p>"
         "<p>Consequence, if a court ever sits, is proportional and judicial &mdash; a ceiling, not a license for private revenge:</p>"
         "<p>If you steal, something of yours may be stolen.<br>If you give damage, you may take damage.<br>"
         "If you strip someone of their liberty, similarly, you may find your liberty being stripped from you.</p>"
         "<p><strong>&ldquo;May&rdquo; is deliberate.</strong> These laws set a ceiling on proportional response, not a mandatory "
         "sentence. A judge isn't required to impose the maximum — they're empowered to judge gracefully and creatively within "
         "the range the law allows.</p>"
         "<p><strong>Consequence is judicial, not personal.</strong> The universal laws describe what a court may impose — they "
         "are not a license for a wronged party to take matters into their own hands. Private retaliation is a new violation of "
         "the law, not an exercise of it.</p>"
         "<p><strong>The laws are self-referential.</strong> A judge who systematically strips liberty from any particular group "
         "is themselves in violation of the third law. The enforcers are bound by them as tightly as anyone they judge.</p>"
         "<p><strong>Simplicity as equality.</strong> Three laws a child can memorize are three laws any citizen can invoke "
         "without a lawyer. That simplicity is itself a form of justice.</p>"},
        {"Identity", 2,
         "<p>Every mechanism in this framework — voting, land, courts, donations — depends on one thing working correctly: "
         "knowing that each citizen is one real, unique person. Get this wrong and every other safeguard is decoration.</p>"
         "<p><strong>No national ID authority.</strong> A single issuing body is a single point of capture. Instead, identity "
         "verification is a private, competitive market, governed by the same anti-monopolism rules as any other service.</p>"
         "<p><strong>How it works.</strong> Independent verifiers confirm a registrant is a unique living person, typically "
         "through a biometric check, and bind that check to the citizen's blockchain key. The biometric itself never touches "
         "the chain — only a signed, hashed proof of &ldquo;this key belongs to one unique person&rdquo; does.</p>"
         "<p><strong>Fraud is expensive, not free.</strong> A verifier that knowingly certifies a fake or duplicate identity is "
         "enabling theft of voting power and is prosecutable under the first universal law.</p>"
         "<p><strong>Non-citizens have standing too.</strong> Legal identity and citizenship are separate things. Anyone present "
         "on the territory enrolls with a verifier to have legal standing in court, since the three universal laws bind everyone "
         "regardless of status. Citizenship — voting rights, land entitlement, point accumulation — is a separate, later opt-in "
         "layered on top.</p>"},
        {"Governance Structure", 3,
         "<p>Three branches, each with a distinct function, each checking the others, all of them accountable to the citizenry "
         "in real time. There is no legislative branch — the citizenry performs that function directly through the blockchain "
         "platform.</p>"
         "<p><strong>The Executive Branch</strong> administers the nation, enforces the universal laws, maintains the military, "
         "and implements the outcomes of citizen votes. It's headed by a president and vice president serving as a unified pair. "
         "No official draws a fixed salary — every official and service member earns exactly what citizens choose to donate to "
         "them individually.</p>"
         "<p><strong>The Officials' Contract.</strong> Before assuming office, every official signs a lawful, binding guarantee "
         "of honesty for the duration of their term, with no exception clause. If the guarantee is broken, consequence follows, "
         "determined by citizen vote.</p>"
         "<p><strong>The Judicial Branch</strong> interprets and applies the universal laws at local and national levels. Local "
         "judges handle day-to-day cases with wide creative discretion. Higher review for cases involving officials or systemic "
         "violations is handled by <strong>sortition panels</strong> — citizens randomly selected for a single case, the way a "
         "jury is called today. No permanent seat exists to bribe or pressure, because no panel outlives its case.</p>"
         "<p><strong>The Civic Branch</strong> is structurally independent from the other two. Its sole job is to maintain, "
         "communicate, and honestly document the reasoning and historical record of the nation. It holds no enforcement power "
         "and controls no resources beyond its own donation-based operating funds. It doesn't propagandize. It documents.</p>"},
        {"The Citizen Legislature and Vote Points", 2,
         "<p>There's no professional legislative class here. Citizens propose, debate, and vote on policy directly through the "
         "blockchain. Any citizen can submit a proposal; every proposal is public; the most supported proposals rise through "
         "engagement, not lobbying: citizen proposes &rarr; blockchain vote &rarr; executive implements &rarr; judiciary reviews "
         "for universal-law compliance.</p>"
         "<p><strong>Vote points.</strong> Every citizen earns one vote point per complete year of citizenship, regardless of "
         "age or background — a point total is tied to a living person and disappears at death. It can't be willed, sold, or "
         "inherited.</p>"
         "<p>Points can be applied in full to one candidate or proposal, split across several, or delegated to a representative "
         "who can use or further delegate them, and withdrawn or reassigned at any time.</p>"
         "<p><strong>Ballots are secret; delegation is not a loophole for coercion.</strong> A citizen's own vote is encrypted — "
         "they can verify it themselves but can't be compelled to prove it to anyone else. Who delegated to whom is not "
         "individually traceable, only the delegate's total power is public. No delegate may hold more than a fixed ceiling of "
         "total national vote points, and delegation chains are one-directional — no citizen can delegate back into a loop that "
         "returns power to themselves.</p>"
         "<p>There's no minimum voting age and no criminal disqualification. The only requirement is that a citizen is capable "
         "of truly and safely casting their own vote.</p>"},
        {"Succession, and the National Blockchain Platform", 1,
         "<p><strong>Succession.</strong> The president and vice president serve as a pair. If the president loses majority "
         "support, the office passes immediately to the new majority leader and their partner. For all other positions, "
         "succession moves to the next role in the organizational hierarchy in a caretaker capacity, restricted from major "
         "unilateral decisions until a new official earns majority approval.</p>"
         "<p><strong>The platform.</strong> The blockchain is the voting system, the public record, the donation mechanism, the "
         "idea registry, the judicial outcome database, the civic communication channel, and the land ledger, all at once. Every "
         "citizen's profile documents their vote history, idea record, land holdings, donations given and received, and public "
         "civic contributions — accessible continuously, because accountability doesn't keep business hours.</p>"},
        {"The Land System", 4,
         "<p>Land is the foundation of independence. A person with secure access to arable land can feed and shelter themselves "
         "without depending on an employer, landlord, or government. This framework treats that as a design constraint, not an "
         "afterthought.</p>"
         "<p>Total land divides into three pools: <strong>citizen-allocable land</strong> (the pool the per-citizen cap is "
         "calculated against), <strong>reserved nature land</strong> (held in permanent public trust, never transactable by "
         "anyone, including the government), and <strong>infrastructure land</strong> (what actual government function needs). "
         "The reserved and infrastructure pools may only grow from land that is unclaimed or voluntarily dedicated, never from "
         "land already held under an individual's bond.</p>"
         "<p><strong>The cap.</strong> Every citizen is entitled to an allotment equal to the citizen-allocable pool divided by "
         "the population, measured in total assessed property bond value — not raw acreage. No individual may hold above the "
         "cap regardless of wealth or status. Within it, land is freely divided, combined, bought, sold, or gifted.</p>"
         "<p><strong>Bonds and auctions.</strong> Every parcel is held with a bond equal to its assessed value, reassessed "
         "periodically using recent blockchain transaction history. When a parcel sells, it's listed at bond value and "
         "auctioned publicly.</p>"
         "<p><strong>Inheritance and gifting.</strong> Property passing to an heir still goes through public auction — the "
         "recipient must meet bond value or match the highest bid. <strong>Exception:</strong> an heir or long-term occupant of "
         "a primary residence has right of first refusal at bond value.</p>"
         "<p><strong>No corporate land ownership.</strong> Businesses lease from individual citizen owners at market rate. "
         "Aggregate leasehold size per business is tracked and published on the blockchain.</p>"
         "<p><strong>Community infrastructure.</strong> Roads, parks, and shared spaces are built through voluntary dedication "
         "— citizens allocate part of their personal allotment to shared use, publicly recorded.</p>"
         "<p><strong>Transition at adoption.</strong> Existing owners keep their current holdings; nothing is seized. Owners "
         "over the new cap divest the excess through the standard auction process within a transition window set by citizen "
         "vote.</p>"},
        {"The Economic Framework", 3,
         "<p>A Voluntary Natural Democracy is a free market. Nearly all goods and services come from private individuals and "
         "businesses competing openly. The framework's job is to keep the conditions for genuine competition structurally "
         "intact, not to run the economy itself.</p>"
         "<p><strong>Privatization.</strong> Everything beyond the executive office and military is privately owned and "
         "operated. Citizens fund the two institutions that can't reasonably be privatized — governance and defense — through "
         "voluntary donation.</p>"
         "<p><strong>Anti-monopolism.</strong> A monopoly isn't the product of a free market; it's the product of a market "
         "that's stopped being free. Full transparency of revenue, market share, and pricing on the blockchain lets citizens "
         "and competitors see concentration developing in real time; judicial review under the third universal law applies "
         "when control of a necessity crosses from inconvenient into a form of leverage over people's lives.</p>"
         "<p><strong>Donation-based compensation.</strong> Officials and service members earn only what citizens choose to "
         "donate them individually, fully visible on the blockchain. Reputational weight for officials rewards "
         "<strong>breadth of support over raw donation size</strong>, so an official funded by ten thousand ordinary citizens "
         "outweighs one funded by ten wealthy ones giving the same total.</p>"
         "<p><strong>Idea credit.</strong> Every adopted proposal is permanently credited to its originator, building a public "
         "record of civic contribution that naturally surfaces capable people for public roles.</p>"},
        {"Cultural Infrastructure", 2,
         "<p>Structure alone doesn't sustain a nation — culture does. A well-designed system inside a culture of apathy gets "
         "hollowed out over time. A well-designed system inside a culture of voluntary participation and honest accountability "
         "strengthens with every generation.</p>"
         "<p>The foundational values — voluntarism and anti-authoritarianism — aren't slogans. They're practical orientations "
         "toward power that have to be actively taught, not assumed. That teaching is the civic branch's entire job.</p>"
         "<p>The transition from Phase 1 into Phase 2 is, itself, ultimately a cultural shift more than a structural one — and "
         "when the time comes, it should be pushed to happen quickly and gracefully, or we risk permanent mission failure.</p>"
         "<p>Accountability alone isn't the whole picture. Once Phase 1 closes and Phase 2 begins, this culture has to actively "
         "cultivate an environment in which officials are genuinely well-regarded, not merely tolerated or presumed suspect.</p>"
         "<p>Diversity of community norms isn't a flaw here — it's how the system learns. Different communities try different "
         "approaches within the universal-law floor, generate real outcome data, and that data is visible to every other "
         "community deciding what to try next.</p>"},
        {"Population and Immigration", 2,
         "<p>Open citizenship is a core value here, but unmanaged growth has a real cost: since land entitlement is calculated "
         "per capita, every new citizen dilutes existing citizens' allotment. The fix can't be a gatekeeper deciding who's "
         "worthy of entry — that recreates exactly the kind of unaccountable authority this system exists to prevent.</p>"
         "<p><strong>The rate is set democratically, not administratively.</strong> Immigration quota is a standing proposal "
         "in the citizen legislature, voted on and re-votable through the same blockchain pipeline as any other policy.</p>"
         "<p><strong>The cap is equal for everyone, and restraint is cultural, not mandated.</strong> Every citizen is entitled "
         "to the same per-capita cap. Whatever portion of a citizen's cap they choose not to claim — their "
         "<strong>personal safety buffer</strong> — is pooled back into the citizen-allocable land, forming the practical "
         "reserve new citizens draw their entitlement from.</p>"
         "<p>There's also a natural brake built into the system for free: as population grows, per-capita land shrinks, bond "
         "values rise, and leasing gets more expensive — a price signal visible to prospective migrants before they ever "
         "arrive.</p>"},
        {"Emergency Provisions", 1,
         "<p>Direct citizen legislation is a strength in ordinary governance and a liability in a genuine crisis — a natural "
         "disaster or an active attack doesn't wait for a blockchain vote to resolve.</p>"
         "<p>To close that gap without creating a permanent emergency-powers backdoor, the executive may be granted "
         "<strong>narrow, time-boxed authority</strong> — but only under objective trigger conditions defined in advance by "
         "citizen vote, never self-declared by the executive in the moment. Emergency authority automatically sunsets after a "
         "fixed window unless renewed by a fresh citizen vote, and every action taken under it is fully logged and subject to "
         "post-hoc judicial review. An emergency clause that lets the executive decide when the emergency exists isn't an "
         "emergency clause — it's a coup provision. This one is built so it can't become that.</p>"},
        {"Initial Adoption Votes", 2,
         "<p>This framework defines the structure. It deliberately does not pre-decide every parameter a real community needs "
         "to run on — those are values a community sets for itself, through the same citizen-vote pipeline that governs "
         "everything afterward.</p>"
         "<p>This initial adoption period constitutes <strong>Phase 1</strong>. <strong>Phase 2</strong> does not begin "
         "automatically or by informal consensus — it requires a formal citizen vote demonstrating landslide support, defined "
         "as support exceeding 50% of the eligible citizen population, confirming the framework and its Phase 1 parameters as "
         "settled, ongoing practice. Until that vote passes, the nation continues operating under Phase 1, however long that "
         "takes.</p>"
         "<p>Items voted at adoption span land pool sizes and reassessment intervals, immigration quota and verifier "
         "accreditation standards, which existing government functions map to which branch, the specific trigger conditions "
         "and sunset window for emergency authority, and the selection method for sortition-based higher review panels. "
         "Citizens vote on each with full visibility into the current default, who it affects, and how reversible it is — "
         "informed consent to the rules they're about to live under.</p>"},
        {"A Living Document", 1,
         "<p>This framework is a living recommendation, not a monument. It's a starting point, not a final word. Every element "
         "of it is open to revision, improvement, and democratic evolution by the people who choose to live inside it. Good "
         "ideas that improve it should be proposed, credited to whoever proposed them, debated openly, and adopted if they "
         "earn majority support.</p>"
         "<p>The only elements not subject to revision are the three universal laws. They're the constitutional floor beneath "
         "which no majority may reach — because a majority that can vote away the prohibition on theft, damage, and liberty "
         "violation is a majority that can vote itself into tyranny. Everything else is negotiable. Everything else can be "
         "improved. That's the point.</p>"
         "<p>A Voluntary Natural Democracy isn't a destination. It's a direction — toward a society where power is always "
         "accountable, participation is always meaningful, the land beneath your feet is genuinely yours, and the most "
         "reliable path to personal prosperity is making the people around you more prosperous too.</p>"
         "<p>The rest is up to you!</p>"},
    };
    return chapters;
}

std::string laws_body() {
    const auto& chapters = law_chapters();
    std::string list;
    for (size_t i = 0; i < chapters.size(); ++i) {
        list += "<a class=\"roomrow\" href=\"/laws/" + std::to_string(i+1) + "\">"
                "<span class=\"roomini\">" + std::to_string(i+1) + "</span>"
                "<span class=\"roomname\">" + html_escape(chapters[i].title) + "</span>"
                "<span class=\"roomlive\">~" + std::to_string(chapters[i].minutes) +
                (chapters[i].minutes==1?" min":" min") + "</span></a>";
    }
    return "<h1>A Voluntary Natural Democracy</h1>"
           "<p class=\"lede\">The three universal laws are the floor — the one part of this no majority can vote away. "
           "Everything past that is proposed structure, not something this platform runs today. Read as much or as little "
           "as you want.</p>"
           "<div class=\"card\"><h2>Chapters</h2>" + list + "</div>"
           "<p class=\"muted\"><a href=\"/control\">Who holds control of this site</a> · <a href=\"/\">Genesis disclosure</a></p>";
}

// "Listen" on a Laws chapter (Brandon 2026-11-09: "i want the audio"): the chapter's recorded narration
// (/assets/chapter<n>.mp3). Shown only when the file is there. Unlike the Genesis button, a failed play (missing file,
// iPhone refusing, network) resets the button and says so instead of showing "playing" over silence.
std::string chapter_narration_html(size_t n) {
    const std::string num = std::to_string(n);
    std::ifstream probe("assets/chapter" + num + ".mp3", std::ios::binary);
    if (!probe) return "";
    return "<p class=\"row narr\"><button type=\"button\" class=\"btn ghost\" id=\"narrbtn\" aria-pressed=\"false\">"
           "&#9654; Listen to this chapter</button>"
           "<span class=\"muted\" id=\"narrmsg\" role=\"status\"></span></p>"
           "<audio id=\"narraudio\" preload=\"none\" src=\"/assets/chapter" + num + ".mp3\"></audio>"
           "<script>(function(){"
           "var b=document.getElementById('narrbtn'),a=document.getElementById('narraudio'),"
           "m=document.getElementById('narrmsg');if(!b||!a)return;"
           "function idle(){b.innerHTML='&#9654; Listen to this chapter';b.setAttribute('aria-pressed','false');}"
           "function fail(){idle();m.textContent='The narration could not play. Try again in a moment.';}"
           "a.addEventListener('ended',idle);a.addEventListener('error',fail);"
           "b.addEventListener('click',function(){m.textContent='';"
           "if(a.paused){var p=a.play();b.innerHTML='&#10074;&#10074; Pause';b.setAttribute('aria-pressed','true');"
           "if(p&&p.catch)p.catch(fail);}else{a.pause();idle();}});"
           "})();</script>";
}

std::string laws_chapter_body(size_t n) {
    const auto& chapters = law_chapters();
    const auto& ch = chapters[n-1];
    std::string nav = "<p class=\"row\"><a class=\"btn ghost\" href=\"/laws\">&larr; Back to overview</a>";
    if (n < chapters.size())
        nav += " <a class=\"btn\" href=\"/laws/" + std::to_string(n+1) + "\">Next: " +
               html_escape(chapters[n].title) + " &rarr;</a>";
    nav += "</p>";
    return "<p class=\"eyebrow\">Chapter " + std::to_string(n) + " of " + std::to_string(chapters.size()) + "</p>"
           "<h1>" + html_escape(ch.title) + "</h1>" + chapter_narration_html(n) +
           "<div class=\"card\">" + ch.body + "</div>" +
           (n == chapters.size()
             ? "<p class=\"muted\">You can stop here. Most people do, and that's fine — the rest is mechanism, "
               "and most of that is still a citizen vote.</p>"
             : "") +
           nav;
}

std::string control_body() {
    return "<h1>Who holds control</h1>"
           "<p class=\"lede\">Genesis operators run the machines and say so in public.</p>"
           "<div class=\"card\"><p><strong>Operator of this copy:</strong> Brandon Bottin.</p>"
           "<p><strong>As of:</strong> September 2026.</p>"
           "<p>That name is a disclosure of who can change this software and this store today. "
           "It is not an office. It is not a claim that the association already governs anyone.</p>"
           "<p class=\"warn\">Control of the website is meant to end when a working election exists among enrolled citizens. "
           "Governmental authority is a later landslide, not this page.</p>"
           "<p class=\"muted\">This is a Beta build of the association’s door — some of what's described here is still being built.</p></div>"
           "<div class=\"card\"><p class=\"eyebrow\">Who runs this</p>"
           "<table class=\"record\">"
           "<tr><td>This site</td><td>Held by one person. Temporary, and replaced the moment there is anything to replace it with.</td></tr>"
           "<tr><td>Your account</td><td>A platform account. Not a citizen identity, not a vote, not standing.</td></tr>"
           "<tr><td>Elections</td><td>None held. Real authority would come later, from a working election.</td></tr>"
           "<tr><td>Your data</td><td>The phone number itself is not kept &mdash; only a fingerprint, so a deleted account cannot immediately reopen under the same number. "
           "Sign in to <a href=\"/login?next=/export\">export</a> or delete from Settings.</td></tr>"
           "</table></div>"
           "<div class=\"card\"><p class=\"eyebrow\">Voting</p><h3>Nothing is awaiting your vote.</h3>"
           "<p class=\"muted\">No election has been held. This exists so the mechanism is testable, not "
           "because anyone here holds a seat.</p>"
           "<p class=\"muted\">Your account is a platform account. It is not a citizen identity and it "
           "casts no vote.</p>"
           "<p><a href=\"/login?next=/proposals\">Proposals</a> are an unofficial straw poll any member can open or vote on, "
           "explicitly not a binding vote. Sign in to see them.</p></div>";
}

// Honest, short, and already true of the live site. Not a lawyer's privacy
// policy — the same facts /control already names, on their own page so
// a shared link and a footer have somewhere to land. /about is the
// operator disclosure; /terms is how the association works.
std::string privacy_body() {
    return "<h1>What we keep</h1>"
           "<p class=\"lede\">Not a government. A platform account is not a citizen identity.</p>"
           "<div class=\"card\"><h2>Sign-in</h2>"
           "<p>Username and password. A phone is bound at signup so a deleted account cannot immediately reopen under the same number. "
           "The number itself is not kept &mdash; only a fingerprint.</p>"
           "<p>A session cookie (<code>lg_session</code>) keeps you signed in.</p></div>"
           "<div class=\"card\"><h2>What you put here</h2>"
           "<p>Posts, mail, and drawings stay until you delete them, or until an operator removes spam. "
           "Messages on this copy are stored so the operator can read them. " // Brandon 2026-11-11 decision 5
           "Friends are mutual accept. A block hides you from that person on civic pages.</p>"
           "<p>Sign in to <a href=\"/login?next=/export\">export</a> or delete from <a href=\"/login?next=/settings\">Settings</a>.</p></div>"
           "<p class=\"muted\">The operator of this copy is named on <a href=\"/control\">who holds control</a>.</p>";
}

std::string terms_body() {
    return "<h1>How this association works</h1>"
           "<p class=\"lede\">LiquidGov.US is a voluntary association. Three rules hold: do not steal, do not damage, do not deprive of liberty.</p>"
           "<div class=\"card\">"
           "<p>Nothing here is a government. Nothing here binds you unless you chose it. "
           "Proposals are an unofficial straw poll, not a binding vote. No election has been held.</p>"
           "<p>Your account is a platform account, not a citizen identity, not a vote, and not standing.</p>"
           "<p>The operator can change this software and this store today. That is disclosed on "
           "<a href=\"/control\">who holds control</a>. It is not an office.</p>"
           "</div>"
           "<p class=\"muted\"><a href=\"/laws\">The three laws</a> · <a href=\"/privacy\">What we keep</a></p>";
}

std::string install_body() {
    return "<h1>Install this as an app</h1>"
           "<p class=\"lede\">Not a download from a store &mdash; your browser turning this site into its "
           "own window, with its own icon, and no address bar in the way.</p>"
           "<div class=\"card\"><h3>iPhone or iPad (Safari)</h3>"
           "<table class=\"record\">"
           "<tr><td>1</td><td>Open liquidgov.us in Safari or Brave (Chrome on iPhone cannot add to Home Screen).</td></tr>"
           "<tr><td>2</td><td>Tap the Share icon (a square with an arrow pointing up), in the bottom toolbar.</td></tr>"
           "<tr><td>3</td><td>Scroll down and tap <strong>Add to Home Screen</strong>.</td></tr>"
           "<tr><td>4</td><td>Tap <strong>Add</strong>. The icon now opens LiquidGov.US on its own, no browser chrome.</td></tr>"
           "</table></div>"
           "<div class=\"card\"><h3>Android (Chrome)</h3>"
           "<table class=\"record\">"
           "<tr><td>1</td><td>Open liquidgov.us in Chrome.</td></tr>"
           "<tr><td>2</td><td>Tap the &vellip; menu in the top-right corner.</td></tr>"
           "<tr><td>3</td><td>Tap <strong>Install app</strong> (some versions show it as \"Add to Home screen\").</td></tr>"
           "<tr><td>4</td><td>Confirm. Chrome may also offer this itself as a banner or an install icon in the "
           "address bar &mdash; either one does the same thing.</td></tr>"
           "</table></div>"
           "<div class=\"card\"><h3>Windows, Mac, or Linux (Chrome or Edge)</h3>"
           "<table class=\"record\">"
           "<tr><td>1</td><td>Open liquidgov.us in Chrome or Edge.</td></tr>"
           "<tr><td>2</td><td>Look for an install icon in the address bar (a small monitor with a down arrow, "
           "or a &oplus;) &mdash; or open the &vellip; menu and look for <strong>Install LiquidGov.US&hellip;</strong>.</td></tr>"
           "<tr><td>3</td><td>Click <strong>Install</strong>. It opens in its own window from now on, and shows up "
           "in your Start menu, Dock, or applications list like anything else installed.</td></tr>"
           "</table></div>"
           "<div class=\"card\"><p class=\"muted\">Any other browser: look for <strong>Add to Home Screen</strong> or "
           "<strong>Install</strong> in its share or menu button &mdash; the exact wording varies, but nearly every "
           "current mobile and desktop browser has some version of it. Nothing here needs installing to just use "
           "the site in a normal browser tab; this only changes how you open it.</p></div>";
}

// Genesis operator tools -- /admin and friends. Not Phase 2, not CitizenId,
// not an election: this is whoever is_operator() names keeping the
// *website* running (locking a spam account, deleting a post, closing a
// community that's gone bad), not a second office. Same login as everyone
// else -- is_operator() gates by username, not a second credential -- no
// shell, no direct file access, no impersonation. Every route below checks
// is_operator() on both GET and POST; a signed-in non-operator gets a
// plain 404 (see handle()'s admin_404 lambda), so /admin isn't advertised
// to anyone it isn't for, and it never appears in the public nav -- only
// in the operator's own account menu (see nav()).
std::string admin_nav(const char* active) {
    auto tab = [&](const char* href, const char* key, const char* label) {
        const bool on = std::string(active) == key;
        return "<a class=\"btn"+std::string(on?"":" ghost")+"\" href=\""+href+"\">"+label+"</a>";
    };
    return "<div class=\"row\" style=\"margin:.5rem 0 1.2rem\">"
        +tab("/admin","overview","Overview")
        +tab("/admin/accounts","accounts","Accounts")
        +tab("/admin/posts","posts","Posts")
        +tab("/admin/proposals","proposals","Proposals")
        +tab("/admin/comments","comments","Comments")
        +tab("/admin/departments","departments","Departments")
        +tab("/admin/communities","communities","Groups")
        +tab("/admin/log","log","Log")
        +"</div>";
}

std::string admin_header() {
    return "<h1>Operator tools</h1>"
           "<p class=\"muted\">Temporary. Not a seat. See <a href=\"/control\">who holds control</a> "
           "for the public disclosure this backs.</p>";
}

std::string fmt_bytes(std::size_t n) {
    if (n < 1024) return std::to_string(n)+" B";
    if (n < 1024*1024) return std::to_string(n/1024)+" KiB";
    return std::to_string(n/(1024*1024))+" MiB";
}

std::string admin_overview_page() {
    std::size_t store_bytes = 0;
    {
        std::ifstream in(store_path_from_env(), std::ios::binary | std::ios::ate);
        if (in) store_bytes = static_cast<std::size_t>(in.tellg());
    }
    return admin_header() + admin_nav("overview") +
        "<div class=\"stattiles\">"
        "<div class=\"stattile\"><strong>"+std::to_string(g_store.account_count())+"</strong><span>Accounts</span></div>"
        "<div class=\"stattile\"><strong>"+std::to_string(g_store.all_communities().size())+"</strong><span>Groups</span></div>"
        "<div class=\"stattile\"><strong>"+std::to_string(g_store.post_count())+"</strong><span>Posts</span></div>"
        "<div class=\"stattile\"><strong>"+std::to_string(g_store.all_proposals().size())+"</strong><span>Open proposals</span></div>"
        "</div>"
        "<p class=\"muted\" style=\"margin-top:.8rem\">Store file: "+fmt_bytes(store_bytes)+"</p>"
        "<p class=\"muted\">You cannot restart the process from here &mdash; that's still SSH and systemd.</p>";
}

// The "Programs in mail" cell: what this person may send, and a form to
// choose the kinds and the size (one person at a time -- each gets their
// own list and limit). Operators always have every kind up to the ceiling.
std::string admin_programs_cell(const std::string& username) {
    const std::uint64_t ceiling_mb = program_ceiling_bytes() / (1024 * 1024);
    if (is_operator(username))
        return "<span class=\"muted\">Every kind, up to " + fmt_size(program_ceiling_bytes()) + " (admin)</span>";
    auto g = g_store.program_grant(username);
    std::string html = g ? html_escape(program_kinds_text(g->kinds)) + "<br><span class=\"muted\">up to " +
                               fmt_size(std::min<std::uint64_t>(g->max_bytes, program_ceiling_bytes())) + "</span>"
                         : std::string("<span class=\"muted\">None</span>");
    const std::uint64_t cur_mb = g ? std::max<std::uint64_t>(1, std::min<std::uint64_t>(g->max_bytes / (1024 * 1024), ceiling_mb)) : 25;
    html += "<details><summary class=\"muted\" style=\"cursor:pointer\">" + std::string(g ? "Change&hellip;" : "Allow&hellip;") +
            "</summary><form method=\"POST\" action=\"/admin/accounts/programs\" style=\"margin-top:.3rem\">"
            "<input type=\"hidden\" name=\"username\" value=\"" + html_escape(username) + "\">";
    for (const auto& k : program_kinds()) {
        const bool on = g && (g->kinds & static_cast<std::uint32_t>(k.kind));
        html += std::string("<label style=\"display:block\"><input type=\"checkbox\" name=\"k_") + k.slug + "\"" +
                (on ? " checked" : "") + "> " + html_escape(k.label) + "</label>";
    }
    html += "<label style=\"display:block;margin-top:.3rem\">Largest file (MB) "
            "<input type=\"number\" name=\"max_mb\" min=\"1\" max=\"" + std::to_string(ceiling_mb) +
            "\" step=\"1\" value=\"" + std::to_string(cur_mb) + "\" style=\"width:5rem\"></label>"
            "<p class=\"muted\" style=\"margin:.2rem 0\">Server limit " + fmt_size(program_ceiling_bytes()) +
            ". Untick everything to stop them sending programs. Programs are not virus-checked: allow only people "
            "you trust, and only the kinds they need.</p>"
            "<button class=\"ghost\" type=\"submit\">Save</button></form></details>";
    return html;
}

std::string admin_accounts_page(const std::string& viewer, const std::string& q) {
    std::string html = admin_header() + admin_nav("accounts") +
        "<form method=\"GET\" action=\"/admin/accounts\" class=\"row\" style=\"margin-bottom:.8rem\">"
        "<input name=\"q\" value=\""+html_escape(q)+"\" placeholder=\"Filter by username\" style=\"flex:1\">"
        "<button type=\"submit\" class=\"ghost\">Filter</button></form>";
    auto accounts = g_store.all_accounts();
    std::vector<PlatformAccount> shown;
    for (const auto& a : accounts) {
        if (q.empty() || a.username.compare(0, q.size(), q) == 0) shown.push_back(a);
    }
    if (shown.empty()) {
        html += "<p class=\"empty\">No accounts match.</p>";
        return html;
    }
    html += "<div style=\"overflow-x:auto\"><table class=\"register\"><tr>"
            "<th>Username</th><th>Created</th><th>Locked</th><th>Sessions</th><th>Programs in mail</th><th>Actions</th></tr>";
    for (const auto& a : shown) {
        const bool locked = g_store.is_locked(a.username);
        const bool self = a.username == viewer;
        html += "<tr><td><a href=\"/u/"+html_escape(a.username)+"\">"+html_escape(a.username)+"</a></td>"
                "<td>"+html_escape(fmt_time(a.created_at))+"</td>"
                "<td>"+std::string(locked?"Yes":"No")+"</td>"
                "<td>"+std::to_string(g_store.sessions_for(a.id).size())+"</td>"
                "<td>"+admin_programs_cell(a.username)+"</td><td>";
        if (self) {
            html += "<span class=\"muted\">(you)</span>";
        } else {
            html += "<form class=\"inline\" method=\"POST\" action=\"/admin/accounts/"+
                    std::string(locked?"unlock":"lock")+"\">"
                    "<input type=\"hidden\" name=\"username\" value=\""+html_escape(a.username)+"\">"
                    "<button class=\"ghost\" type=\"submit\">"+std::string(locked?"Unlock":"Lock")+"</button></form> "
                    "<form class=\"inline\" method=\"POST\" action=\"/admin/accounts/logout-all\">"
                    "<input type=\"hidden\" name=\"username\" value=\""+html_escape(a.username)+"\">"
                    "<button class=\"ghost\" type=\"submit\">Sign out all devices</button></form> "
                    "<details style=\"display:inline-block\"><summary class=\"muted\" style=\"cursor:pointer;display:inline\">Delete&hellip;</summary>"
                    "<form method=\"POST\" action=\"/admin/accounts/delete\" style=\"margin-top:.3rem\">"
                    "<input type=\"hidden\" name=\"username\" value=\""+html_escape(a.username)+"\">"
                    "<input name=\"confirm\" placeholder=\"type &ldquo;delete&rdquo;\" autocomplete=\"off\">"
                    "<button class=\"danger\" type=\"submit\">Delete account</button></form></details>";
        }
        html += "</td></tr>";
    }
    html += "</table></div>";
    return html;
}

// Case-insensitive "does hay contain needle" for operator searches.
bool admin_matches(const std::string& hay, const std::string& needle) {
    auto low = [](std::string s) { for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); return s; };
    return low(hay).find(low(needle)) != std::string::npos;
}

std::string admin_search_form(const char* action, const std::string& q, const char* hint = "Author or words") {
    return "<form method=\"GET\" action=\"" + std::string(action) + "\" class=\"row\" style=\"margin:0 0 1rem\">"
           "<input name=\"q\" placeholder=\"" + std::string(hint) + "\" value=\"" + html_escape(q) + "\" style=\"flex:1;font-size:16px\">"
           "<button type=\"submit\">Search</button></form>";
}

std::string admin_posts_page(const std::string& q) {
    std::string html = admin_header() + admin_nav("posts") + admin_search_form("/admin/posts", q);
    // Without a search: the 80 newest. With one: every post whose author is q or whose text contains q, newest first
    // (Brandon 2026-11-08: older abusive posts could not be reached from the 80-newest list).
    std::vector<Post> posts;
    if (q.empty()) {
        posts = g_store.recent_posts(80);
    } else {
        auto all = g_store.all_posts();
        for (auto it = all.rbegin(); it != all.rend() && posts.size() < 500; ++it)
            if (admin_matches(it->author, q) || admin_matches(it->text, q)) posts.push_back(*it);
        html += "<p class=\"muted\">" + std::to_string(posts.size()) + (posts.size() == 500 ? "+" : "") + " matching.</p>";
    }
    if (posts.empty()) {
        html += "<p class=\"empty\">" + std::string(q.empty() ? "No posts yet." : "No post matches.") + "</p>";
        return html;
    }
    html += "<div style=\"overflow-x:auto\"><table class=\"register\"><tr>"
            "<th>Id</th><th>Author</th><th>When</th><th>Text</th><th></th></tr>";
    for (const auto& p : posts) {
        std::string excerpt = p.text.substr(0, 80);
        if (p.text.size() > 80) excerpt += "\xE2\x80\xA6";
        html += "<tr><td>"+std::to_string(p.id)+"</td>"
                "<td><a href=\"/u/"+html_escape(p.author)+"\">"+html_escape(p.author)+"</a></td>"
                "<td>"+html_escape(fmt_time(p.ts))+"</td>"
                "<td>"+html_escape(excerpt)+"</td>"
                "<td><form method=\"POST\" action=\"/admin/posts/delete\" "
                "onsubmit=\"return confirm('Delete this post?');\">"
                "<input type=\"hidden\" name=\"id\" value=\""+std::to_string(p.id)+"\">"
                "<button class=\"danger\" type=\"submit\">Delete</button></form></td></tr>";
    }
    html += "</table></div>";
    return html;
}

// Operator moderation of proposals (Brandon 2026-11-08: "make it so i can delete and edit proposals for now? people
// are abusing it"). Before a delete, the proposal and its comments are appended to an archive next to the store
// (<store>.moderation.jsonl; its name matches the deploy sync's '*.lgp.*' exclusion, so no deploy can touch it), so
// the evidence survives and a hand restore is possible. If the archive can't be written, the delete is refused.
bool archive_deleted_proposal(const Proposal& p, const std::string& by) {
    std::string line = "{\"deleted_by\":\"" + json_escape(by) + "\",\"deleted_at\":" + std::to_string(std::time(nullptr)) +
        ",\"id\":" + std::to_string(p.id) + ",\"author\":\"" + json_escape(p.author) + "\",\"ts\":" + std::to_string(p.ts) +
        ",\"city\":\"" + json_escape(p.city) + "\",\"title\":\"" + json_escape(p.title) + "\",\"body\":\"" + json_escape(p.body) +
        "\",\"reactions\":{";
    bool first = true;
    for (const auto& rc : g_store.proposal_reaction_counts(p.id)) {
        line += std::string(first ? "" : ",") + "\"" + json_escape(rc.first) + "\":" + std::to_string(rc.second);
        first = false;
    }
    line += "},\"comments\":[";
    first = true;
    for (const auto& r : g_store.proposal_replies_of(p.id)) {
        line += std::string(first ? "" : ",") + "{\"author\":\"" + json_escape(r.author) + "\",\"ts\":" + std::to_string(r.ts) +
                ",\"text\":\"" + json_escape(r.text) + "\"}";
        first = false;
    }
    line += "]}\n";
    const std::string path = std::string(store_path_from_env()) + ".moderation.jsonl";
    std::ofstream out(path, std::ios::app | std::ios::binary);
    if (!out) {
        std::cerr << "moderation archive not writable: " << path << "\n";
        return false;
    }
    out << line;
    out.flush();
    return static_cast<bool>(out);
}

std::string admin_proposals_page(const std::string& msg) {
    std::string html = admin_header() + admin_nav("proposals");
    if (!msg.empty()) html += "<p class=\"muted\" role=\"status\">" + html_escape(msg) + "</p>";
    auto props = g_store.all_proposals();
    if (props.empty()) {
        html += "<p class=\"empty\">No proposals yet.</p>";
        return html;
    }
    html += "<p class=\"muted\">Newest first. Delete keeps a copy (with its comments) in the moderation archive on the "
            "server; it removes the proposal, its comments, reactions and votes from the site.</p>"
            "<div style=\"overflow-x:auto\"><table class=\"register\"><tr>"
            "<th>Id</th><th>Author</th><th>When</th><th>Proposal</th><th></th></tr>";
    for (auto it = props.rbegin(); it != props.rend(); ++it) {
        const auto& p = *it;
        const std::string id = std::to_string(p.id);
        std::string excerpt = p.body.substr(0, 120);
        if (p.body.size() > 120) excerpt += "\xE2\x80\xA6";
        html += "<tr><td><a href=\"/proposals?id=" + id + "\">" + id + "</a></td>"
                "<td><a href=\"/u/" + html_escape(p.author) + "\">" + html_escape(p.author) + "</a></td>"
                "<td>" + html_escape(fmt_time(p.ts)) + "</td>"
                // runs for office (/officials) are proposals too; label them so the operator knows where they show
                "<td>" + std::string(p.is_office ? "<span class=\"chip\">Officials: " + html_escape(p.jurisdiction) + "</span> " : "") +
                "<strong>" + html_escape(p.title) + "</strong><br><span class=\"muted\">" + html_escape(excerpt) + "</span>"
                "<details><summary>Edit</summary>"
                "<form method=\"POST\" action=\"/admin/proposals/edit\">"
                "<input type=\"hidden\" name=\"id\" value=\"" + id + "\">"
                "<label>Title <input name=\"title\" required maxlength=\"200\" value=\"" + html_escape(p.title) + "\"></label>"
                "<label>Text <textarea name=\"body\" rows=\"5\" maxlength=\"10000\">" + html_escape(p.body) + "</textarea></label>"
                "<button type=\"submit\">Save</button></form></details></td>"
                "<td><form method=\"POST\" action=\"/admin/proposals/delete\" "
                "onsubmit=\"return confirm('Delete proposal " + id + " with its comments and votes? A copy is kept in the moderation archive.');\">"
                "<input type=\"hidden\" name=\"id\" value=\"" + id + "\">"
                "<button class=\"danger\" type=\"submit\">Delete</button></form></td></tr>";
    }
    html += "</table></div>";
    return html;
}

// One deleted comment, archived like a deleted proposal (same <store>.moderation.jsonl; refused if unwritable).
bool archive_deleted_comment(const char* on, std::uint64_t id, std::uint64_t parent, const std::string& author,
                             std::int64_t ts, const std::string& text, const std::string& by) {
    const std::string line = "{\"deleted_by\":\"" + json_escape(by) + "\",\"deleted_at\":" + std::to_string(std::time(nullptr)) +
        ",\"comment_on\":\"" + on + "\",\"comment_id\":" + std::to_string(id) + ",\"parent_id\":" + std::to_string(parent) +
        ",\"author\":\"" + json_escape(author) + "\",\"ts\":" + std::to_string(ts) + ",\"text\":\"" + json_escape(text) + "\"}\n";
    const std::string path = std::string(store_path_from_env()) + ".moderation.jsonl";
    std::ofstream out(path, std::ios::app | std::ios::binary);
    if (!out) {
        std::cerr << "moderation archive not writable: " << path << "\n";
        return false;
    }
    out << line;
    out.flush();
    return static_cast<bool>(out);
}

// Comments on posts and on proposals, newest first, each deletable on its own (Brandon 2026-11-08). q searches
// author and text across all of them.
std::string admin_comments_page(const std::string& q, const std::string& msg) {
    std::string html = admin_header() + admin_nav("comments") + admin_search_form("/admin/comments", q);
    if (!msg.empty()) html += "<p class=\"muted\" role=\"status\">" + html_escape(msg) + "</p>";
    const std::size_t limit = q.empty() ? 100 : 100000;
    struct Row { std::string on, parent_href, author, text; std::uint64_t id; std::int64_t ts; };
    std::vector<Row> rows;
    for (const auto& r : g_store.recent_post_replies(limit))
        rows.push_back({"post", "/post/" + std::to_string(r.post_id), r.author, r.text, r.id, r.ts});
    for (const auto& r : g_store.recent_proposal_replies(limit))
        rows.push_back({"proposal", "/proposals?id=" + std::to_string(r.proposal_id), r.author, r.text, r.id, r.ts});
    if (!q.empty())
        rows.erase(std::remove_if(rows.begin(), rows.end(), [&](const Row& r) {
            return !admin_matches(r.author, q) && !admin_matches(r.text, q); }), rows.end());
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.ts > b.ts; });
    if (rows.size() > 300) rows.resize(300);
    if (rows.empty()) {
        html += "<p class=\"empty\">" + std::string(q.empty() ? "No comments yet." : "No comment matches.") + "</p>";
        return html;
    }
    html += "<p class=\"muted\">Delete removes only that comment; a copy goes to the moderation archive first.</p>"
            "<div style=\"overflow-x:auto\"><table class=\"register\"><tr><th>On</th><th>Author</th><th>When</th><th>Comment</th><th></th></tr>";
    for (const auto& r : rows) {
        std::string excerpt = r.text.substr(0, 160);
        if (r.text.size() > 160) excerpt += "\xE2\x80\xA6";
        html += "<tr><td><a href=\"" + r.parent_href + "\">" + r.on + "</a></td>"
                "<td><a href=\"/u/" + html_escape(r.author) + "\">" + html_escape(r.author) + "</a></td>"
                "<td>" + html_escape(fmt_time(r.ts)) + "</td><td>" + html_escape(excerpt) + "</td>"
                "<td><form method=\"POST\" action=\"/admin/comments/delete\" onsubmit=\"return confirm('Delete this comment?');\">"
                "<input type=\"hidden\" name=\"on\" value=\"" + r.on + "\"><input type=\"hidden\" name=\"id\" value=\"" + std::to_string(r.id) + "\">"
                "<button class=\"danger\" type=\"submit\">Delete</button></form></td></tr>";
    }
    html += "</table></div>";
    return html;
}

// One deleted city department/council, archived like a deleted comment (same <store>.moderation.jsonl; refused if
// unwritable), with the proposals that named it so a hand restore can re-tag them.
bool archive_deleted_city_tag(const std::string& key, const std::string& tag, const std::vector<std::uint64_t>& props,
                              const std::string& by) {
    std::string ids;
    for (auto id : props) ids += (ids.empty() ? "" : ",") + std::to_string(id);
    const std::string line = "{\"deleted_by\":\"" + json_escape(by) + "\",\"deleted_at\":" + std::to_string(std::time(nullptr)) +
        ",\"city_tag_city\":\"" + json_escape(key) + "\",\"city_tag\":\"" + json_escape(tag) + "\",\"proposals\":[" + ids + "]}\n";
    const std::string path = std::string(store_path_from_env()) + ".moderation.jsonl";
    std::ofstream out(path, std::ios::app | std::ios::binary);
    if (!out) {
        std::cerr << "moderation archive not writable: " << path << "\n";
        return false;
    }
    out << line;
    out.flush();
    return static_cast<bool>(out);
}

// Departments and councils named on city pages, each deletable (open item "moderation gap left: delete city
// departments/councils", Brandon OK'd 2026-11-08). q searches the city and the name.
std::string admin_departments_page(const std::string& q, const std::string& msg) {
    std::string html = admin_header() + admin_nav("departments") + admin_search_form("/admin/departments", q, "City or name");
    if (!msg.empty()) html += "<p class=\"muted\" role=\"status\">" + html_escape(msg) + "</p>";
    // the city's name ("Fresno, CA"), not its key ("fresno-ca"), as the city page shows it (sweep 2026-11-09)
    std::map<std::string, std::string> city_names;
    for (const auto& cu : g_store.cities_in_use()) city_names[cu.key] = cu.name;
    auto city_name = [&](const std::string& key) { auto it = city_names.find(key); return it == city_names.end() ? key : it->second; };
    auto rows = g_store.all_city_tags();
    if (!q.empty())
        rows.erase(std::remove_if(rows.begin(), rows.end(), [&](const std::pair<std::string, std::string>& r) {
            return !admin_matches(r.first, q) && !admin_matches(city_name(r.first), q) && !admin_matches(r.second, q); }),
            rows.end());
    if (rows.empty()) {
        html += "<p class=\"empty\">" + std::string(q.empty() ? "No departments or councils yet." : "Nothing matches.") + "</p>";
        return html;
    }
    html += "<p class=\"muted\">Delete removes it from the city page and from every proposal that names it; a copy goes "
            "to the moderation archive first.</p>"
            "<div style=\"overflow-x:auto\"><table class=\"register\"><tr><th>City</th><th>Department or council</th>"
            "<th>Proposals</th><th></th></tr>";
    for (const auto& [key, tag] : rows) {
        const auto n = g_store.proposals_with_city_tag(key, tag).size();
        html += "<tr><td><a href=\"/city/" + url_encode(key) + "\">" + html_escape(city_name(key)) + "</a></td>"
                "<td>" + html_escape(tag) + "</td><td>" + std::to_string(n) + "</td>"
                "<td><form method=\"POST\" action=\"/admin/departments/delete\" "
                "onsubmit=\"return confirm('Delete this department or council?');\">"
                "<input type=\"hidden\" name=\"city\" value=\"" + html_escape(key) + "\">"
                "<input type=\"hidden\" name=\"tag\" value=\"" + html_escape(tag) + "\">"
                "<button class=\"danger\" type=\"submit\">Delete</button></form></td></tr>";
    }
    html += "</table></div>";
    return html;
}

std::string admin_communities_page() {
    std::string html = admin_header() + admin_nav("communities");
    auto communities = g_store.all_communities();
    if (communities.empty()) {
        html += "<p class=\"empty\">No groups yet.</p>";
        return html;
    }
    html += "<div style=\"overflow-x:auto\"><table class=\"register\"><tr>"
            "<th>Name</th><th>Owner</th><th>Members</th><th>Actions</th></tr>";
    for (const auto& c : communities) {
        html += "<tr><td><a href=\"/c/"+html_escape(c.slug)+"\">"+html_escape(c.name)+"</a> "
                "<span class=\"muted\">/"+html_escape(c.slug)+"</span></td>"
                "<td><a href=\"/u/"+html_escape(c.owner)+"\">"+html_escape(c.owner)+"</a></td>"
                "<td>"+std::to_string(g_store.members_of(c.slug).size())+"</td>"
                "<td><details style=\"display:inline-block\"><summary class=\"muted\" style=\"cursor:pointer;display:inline\">Delete&hellip;</summary>"
                "<form method=\"POST\" action=\"/admin/communities/delete\" style=\"margin-top:.3rem\">"
                "<input type=\"hidden\" name=\"slug\" value=\""+html_escape(c.slug)+"\">"
                "<input name=\"confirm\" placeholder=\"type &ldquo;delete&rdquo;\" autocomplete=\"off\">"
                "<button class=\"danger\" type=\"submit\">Delete group</button></form></details>"
                "<p class=\"muted\" style=\"margin:.3rem 0 0\">Posts already made here keep their room "
                "name but stop being reachable -- they are not deleted.</p></td></tr>";
    }
    html += "</table></div>";
    return html;
}

std::string admin_log_page() {
    std::string html = admin_header() + admin_nav("log");
    auto entries = g_store.recent_audit_log(50);
    if (entries.empty()) {
        html += "<p class=\"empty\">No operator actions yet.</p>";
        return html;
    }
    html += "<div style=\"overflow-x:auto\"><table class=\"register\"><tr>"
            "<th>When</th><th>Operator</th><th>Action</th><th>Target</th></tr>";
    for (const auto& e : entries) {
        html += "<tr><td>"+html_escape(fmt_time(e.ts))+"</td>"
                "<td>"+html_escape(e.operator_name)+"</td>"
                "<td>"+html_escape(e.action)+"</td>"
                "<td>"+html_escape(e.target)+"</td></tr>";
    }
    html += "</table></div>";
    return html;
}

struct SignupDraft {
    std::string username, phone, email, display_name;
    std::string hometown, living, bio, interests, career, school;
};

bool looks_like_email(const std::string& s) {
    auto at = s.find('@');
    if (at == std::string::npos || at == 0 || at + 1 >= s.size()) return false;
    auto dot = s.find('.', at + 1);
    if (dot == std::string::npos || dot + 1 >= s.size()) return false;
    if (s.find(' ') != std::string::npos) return false;
    return s.size() <= 200;
}

// Google Identity Services' own button, wired to POST straight to
// /login/google as a plain form submit (data-ux_mode="redirect") -- no
// custom JS callback needed, GIS handles the credential POST itself.
// Renders nothing at all if no client ID is configured yet, rather than
// a button that would just fail every click.
std::string google_signin_button_html() {
    const char* cid = google_client_id_from_env();
    if (!cid[0]) return "";
    return "<div style=\"margin:.8rem 0\">"
           "<script src=\"https://accounts.google.com/gsi/client\" async defer></script>"
           "<div id=\"g_id_onload\" data-client_id=\""+html_escape(cid)+"\" "
           "data-login_uri=\"https://liquidgov.us/login/google\" data-ux_mode=\"redirect\"></div>"
           "<div class=\"g_id_signin\" data-type=\"standard\" data-shape=\"pill\" data-width=\"300\"></div>"
           "</div>";
}

// First-time Google sign-in with no prior LiquidGov account: derives a
// username from the email's local-part rather than asking for one (the
// whole point of one-click). Strips anything valid_username() wouldn't
// accept, pads if too short, then appends a numeric suffix until free.
std::string username_from_email_local_part(const std::string& email) {
    auto at = email.find('@');
    std::string local = at == std::string::npos ? email : email.substr(0, at);
    std::string base;
    for (unsigned char c : local) {
        if ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='-')
            base += static_cast<char>(c);
    }
    if (base.size() > 28) base.resize(28); // leaves room for a numeric suffix under the 32 cap
    if (base.size() < 3) base = "user" + base;
    std::string candidate = base;
    int suffix = 2;
    while (g_store.username_exists(candidate)) {
        candidate = base + std::to_string(suffix);
        ++suffix;
    }
    return candidate;
}

std::string signup_page(const std::string& navuser, int unread, const std::string& err,
                        const SignupDraft& d) {
    std::string e;
    if (!err.empty()) e = "<p class=\"warn\">"+html_escape(err)+"</p>";
    return wrap("Sign up", navuser, unread,
        "<div class=\"card\"><h1>Create an account</h1>"
        "<p class=\"muted\">Username is 3–32 characters: letters, numbers, dots, underscores, hyphens. "
        "No spaces, no @. Password at least 8 characters. Phone is a signup binding "
        "(not used to log in, the number itself is not stored). Email is kept so you can "
        "reset your password if you forget it &mdash; it is not used to log in either. "
        "This is a platform account, not a citizen identity.</p>"+e+
        google_signin_button_html()+
        "<form method=\"POST\" action=\"/signup\">"
        "<label>Username</label><input name=\"username\" required minlength=\"3\" maxlength=\"32\" "
        "pattern=\"[A-Za-z0-9._-]{3,32}\" title=\"3–32 letters, numbers, dots, underscores, hyphens. No spaces.\" "
        "autocomplete=\"username\" placeholder=\"alice\" value=\""+html_escape(d.username)+"\">"
        "<label>Display name</label><input name=\"display_name\" required maxlength=\"80\" "
        "autocomplete=\"name\" placeholder=\"How you want to be shown\" value=\""+html_escape(d.display_name)+"\">"
        "<label>Password</label><input type=\"password\" name=\"password\" required minlength=\"8\" maxlength=\"200\" autocomplete=\"new-password\">"
        "<label>Email</label><input name=\"email\" type=\"email\" required maxlength=\"200\" autocomplete=\"email\" "
        "placeholder=\"you@example.com\" value=\""+html_escape(d.email)+"\">"
        "<label>Phone</label><input name=\"phone\" type=\"tel\" required autocomplete=\"tel\" inputmode=\"tel\" "
        "placeholder=\"10+ digits\" value=\""+html_escape(d.phone)+"\">"
        "<p class=\"eyebrow\" style=\"margin-top:1.2rem\">Optional</p>"
        "<label>Hometown</label><input name=\"hometown\" maxlength=\"80\" value=\""+html_escape(d.hometown)+"\">"
        "<label>Where you live now</label><input name=\"living\" maxlength=\"80\" value=\""+html_escape(d.living)+"\">"
        "<label>Bio</label><textarea name=\"bio\" maxlength=\"500\">"+html_escape(d.bio)+"</textarea>"
        "<label>Interests</label><input name=\"interests\" maxlength=\"200\" placeholder=\"comma-separated\" value=\""+html_escape(d.interests)+"\">"
        "<label>Career</label><input name=\"career\" maxlength=\"80\" value=\""+html_escape(d.career)+"\">"
        "<label>School</label><input name=\"school\" maxlength=\"80\" value=\""+html_escape(d.school)+"\">"
        "<button>Continue</button></form></div>");
}

// viewer is the logged-in username rendering this post (empty for an
// anonymous visitor to a public profile) -- only a logged-in viewer gets a
// Repost button. Reposting always credits the true original author (see
// the repost_of_author comment on Post), so a chain of reposts never grows
// deeper than one hop of attribution.
std::string post_block(const Post& p, const std::string& viewer) {
    std::string where;
    if (!p.community.empty())
        where = " in <a href=\"/c/"+html_escape(p.community)+"\">"+html_escape(p.community)+"</a>";
    std::string when = fmt_time(p.ts);
    std::string repostLine;
    if (!p.repost_of_author.empty())
        repostLine = "<p class=\"muted\">&#8635; Reposted from <a href=\"/u/"+html_escape(p.repost_of_author)+
                     "\">"+html_escape(p.repost_of_author)+"</a></p>";
    std::string repostForm;
    if (!viewer.empty())
        repostForm = "<form class=\"inline\" method=\"POST\" action=\"/me/repost\">"
                     "<input type=\"hidden\" name=\"post_id\" value=\""+std::to_string(p.id)+"\">"
                     "<button class=\"ghost\" type=\"submit\">&#8635; Repost</button></form>";
    const int seconds = g_store.second_count(p.id);
    std::string secondForm;
    if (!viewer.empty()) {
        const bool seconded = g_store.has_seconded(p.id, viewer);
        secondForm = "<form class=\"inline\" method=\"POST\" action=\"/post/"+std::to_string(p.id)+"/second\">"
                     "<button class=\""+std::string(seconded?"":"ghost")+"\" type=\"submit\">"+
                     std::to_string(seconds)+(seconds==1?" second":" seconded")+
                     (seconded?" &#10003;":"")+"</button></form>";
    } else if (seconds > 0) {
        secondForm = "<p class=\"muted\">"+std::to_string(seconds)+(seconds==1?" second":" seconded")+"</p>";
    }
    std::string deleteForm;
    if (viewer == p.author)
        deleteForm = "<form class=\"inline\" method=\"POST\" action=\"/post/"+std::to_string(p.id)+"/delete\" "
                     "onsubmit=\"return confirm('Delete this post?');\">"
                     "<button class=\"ghost\" type=\"submit\">Delete</button></form>";
    std::string actionsRow;
    if (!repostForm.empty() || !secondForm.empty() || !deleteForm.empty())
        actionsRow = "<div class=\"row\" style=\"margin-top:.3rem\">"+repostForm+secondForm+deleteForm+"</div>";
    std::string repliesBlock;
    {
        auto replies = g_store.replies_of(p.id);
        std::string list;
        for (const auto& r : replies) {
            std::string rwhen = fmt_time(r.ts);
            list += "<div class=\"who\" style=\"margin:.5rem 0\">"+portrait_tag(r.author,"sm")+
                    "<div><a href=\"/u/"+html_escape(r.author)+"\"><strong>"+html_escape(r.author)+
                    "</strong></a> <span class=\"muted\">"+html_escape(rwhen)+"</span>"
                    "<p style=\"margin:.2rem 0 0\">"+html_escape(r.text)+"</p></div></div>";
        }
        std::string replyForm;
        if (!viewer.empty()) {
            replyForm = "<form method=\"POST\" action=\"/post/"+std::to_string(p.id)+"/reply\" style=\"margin-top:.3rem\">"
                        "<textarea name=\"text\" maxlength=\"1000\" placeholder=\"Write a reply\"></textarea>"
                        "<button class=\"ghost\" type=\"submit\">Reply</button></form>";
        }
        if (!replies.empty() || !viewer.empty()) {
            repliesBlock = "<details style=\"margin-top:.4rem\"><summary class=\"muted\" style=\"cursor:pointer\">"+
                std::to_string(replies.size())+(replies.size()==1?" reply":" replies")+"</summary>"
                "<div style=\"padding-left:.8rem;border-left:2px solid var(--rule);margin-top:.3rem\">"+
                list+replyForm+"</div></details>";
        }
    }
    std::string mediaHtml;
    if (!p.attachment_blob_id.empty()) {
        std::string src = "/post-media/"+html_escape(p.attachment_blob_id);
        // width:100% (not max-width) so a small source image still fills
        // the card instead of rendering at its native pixel size -- the
        // bug that first surfaced this: a 120x80 test PNG showed up
        // genuinely postage-stamp-sized, since max-width alone only ever
        // scales large images down, never scales small ones up.
        mediaHtml = (p.attachment_mime.rfind("video/",0)==0)
            ? "<video controls preload=\"metadata\" style=\"width:100%;height:auto;display:block;border-radius:var(--rsm);margin-top:.4rem\" src=\""+src+"\"></video>"
            : "<img style=\"width:100%;height:auto;display:block;border-radius:var(--rsm);margin-top:.4rem\" src=\""+src+"\" alt=\"\">";
    }
    return "<article class=\"post\" id=\"post-"+std::to_string(p.id)+"\"><div class=\"who\">"+portrait_tag(p.author,"sm")+"<div><a href=\"/u/"+html_escape(p.author)+"\"><strong>"+
           html_escape(p.author)+"</strong></a>"+where+
           (when.empty()?"":" <span class=\"muted\">"+html_escape(when)+"</span>")+
           repostLine+
           "<p>"+linkify_post_text(p.text)+"</p>"+mediaHtml+actionsRow+repliesBlock+"</div></div></article>";
}

// A proportional Yes/No/Abstain bar plus one-click vote buttons, the
// current voter's own choice picked out with a filled button instead of a
// ghost one. Number / Date / Choice / Scale use the same card, with a
// median (or option counts) instead of the yes/no bar. viewer empty
// (logged out) renders the tally with no voting UI.
std::string fmt_ballot_num(double v) {
    std::ostringstream o;
    if (std::fabs(v - std::round(v)) < 1e-9) o << static_cast<long long>(std::round(v));
    else { o.setf(std::ios::fixed); o.precision(2); o << v; }
    return o.str();
}

// The one-line how-to on /proposals and /officials (Brief 7 item 2, the audit's wording).
std::string how_proposals_work_html() {
    return "<p class=\"muted\"><strong>How this works:</strong> Anyone with an account can start a question or add "
           "their answer. You can change your answer at any time. This is a straw poll, not an official election, and "
           "nothing is sent to the city.</p>";
}

std::string proposal_talk(const Proposal& p, const std::string& viewer, const std::string& next_in);

// The Choice answer: one button per option with its count, posting the option INDEX (payload_ok takes only an
// index). Shared by proposal_block and /officials, so a race and a Choice proposal vote and count the same way
// (Brief 7 item 1: /officials posted the name and every vote was dropped). noun: "choice" or "candidate".
std::string choice_ballot_body(const Proposal& p, const std::string& viewer, const std::string& next_in,
                               const std::string& noun) {
    auto t = g_store.value_tally_of(p.id);
    auto mine = viewer.empty() ? std::optional<std::string>{} : g_store.ballot_value_of(p.id, viewer);
    int total = t.n;
    std::string bar = "<div style=\"display:flex;height:8px;border-radius:4px;overflow:hidden;background:var(--rule);margin:.5rem 0\">";
    if (total > 0) {
        for (std::size_t i = 0; i < t.option_counts.size(); ++i) {
            if (t.option_counts[i])
                bar += "<div style=\"flex:"+std::to_string(t.option_counts[i])+";background:var(--accent)\"></div>";
        }
    }
    bar += "</div>";
    std::string list = "<div class=\"row\" style=\"flex-wrap:wrap;gap:.4rem\">";
    for (std::size_t i = 0; i < p.ballot.options.size(); ++i) {
        int n = (i < t.option_counts.size()) ? t.option_counts[i] : 0;
        const bool active = mine && *mine == std::to_string(i);
        if (!viewer.empty()) {
            list += "<form class=\"inline\" method=\"POST\" action=\"/proposals/vote\">"
                    "<input type=\"hidden\" name=\"id\" value=\""+std::to_string(p.id)+"\">"
                    + next_in +
                    "<input type=\"hidden\" name=\"value\" value=\""+std::to_string(i)+"\">"
                    "<button class=\""+std::string(active ? "" : "ghost")+"\" type=\"submit\">"+
                    html_escape(p.ballot.options[i])+" · "+std::to_string(n)+
                    (active ? " &#10003;" : "")+"</button></form>";
        } else {
            list += "<span class=\"muted\">"+html_escape(p.ballot.options[i])+" · "+std::to_string(n)+"</span>";
        }
    }
    list += "</div>";
    if (!viewer.empty() && p.ballot.open_choice) {
        bool already = false;
        for (const auto& a : p.ballot.option_authors) if (a == viewer) { already = true; break; }
        const bool can_add = (viewer == p.author) || !already;
        if (can_add) {
            list += "<form method=\"POST\" action=\"/proposals/option\" style=\"margin-top:.7rem\">"
                    "<input type=\"hidden\" name=\"id\" value=\""+std::to_string(p.id)+"\">"
                    + next_in +
                    "<label>Add a "+noun
                    + std::string(viewer == p.author ? "" : " <span class=\"muted\">(one per person)</span>")
                    + "</label>"
                    "<div class=\"row\"><input name=\"label\" maxlength=\"80\" required placeholder=\""+std::string(noun == "candidate" ? "A name" : "Your option")+"\">"
                    "<button type=\"submit\">Add</button></div></form>";
        } else {
            list += "<p class=\"muted\">You already added a "+noun+" on this one.</p>";
        }
    }
    return bar + "<p class=\"muted\">"+std::to_string(total)+" responses</p>" + list;
}

std::string proposal_block(const Proposal& p, const std::string& viewer, const std::string& next = "") {
    const std::string back = safe_next_path(next);
    const std::string next_in = back.empty() ? std::string()
        : "<input type=\"hidden\" name=\"next\" value=\""+html_escape(back)+"\">";
    std::string kindEyebrow;
    switch (p.ballot.kind) {
        case BallotKind::YesNo: kindEyebrow = "Yes, no, or skip"; break;
        case BallotKind::Number: kindEyebrow = p.ballot.unit.empty() ? "Number" : html_escape(p.ballot.unit); break;
        case BallotKind::Date: kindEyebrow = "Date"; break;
        case BallotKind::Choice: kindEyebrow = p.ballot.open_choice ? "Pick one (people can add options)" : "Pick one"; break;
        case BallotKind::Scale: kindEyebrow = "Scale"; break;
    }
    std::string body;
    if (p.ballot.kind == BallotKind::YesNo) {
        auto t = g_store.tally_of(p.id);
        auto my_vote = viewer.empty() ? std::optional<ProposalVote>{} : g_store.proposal_vote_of(p.id, viewer);
        const int total = t.yes + t.no + t.abstain;
        std::string bar = "<div style=\"display:flex;height:8px;border-radius:4px;overflow:hidden;background:var(--rule);margin:.5rem 0\">";
        if (total > 0) {
            if (t.yes) bar += "<div style=\"flex:"+std::to_string(t.yes)+";background:var(--ok)\"></div>";
            if (t.no) bar += "<div style=\"flex:"+std::to_string(t.no)+";background:var(--danger)\"></div>";
            if (t.abstain) bar += "<div style=\"flex:"+std::to_string(t.abstain)+";background:var(--stone)\"></div>";
        }
        bar += "</div><p class=\"muted\">"+std::to_string(t.yes)+" yes · "+std::to_string(t.no)+" no · "+
               std::to_string(t.abstain)+" abstain · "+std::to_string(total)+" total</p>";
        std::string voteForm;
        if (!viewer.empty()) {
            auto btn = [&](ProposalVote v, const char* label) {
                const bool active = my_vote.has_value() && *my_vote == v;
                return "<form class=\"inline\" method=\"POST\" action=\"/proposals/vote\">"
                       "<input type=\"hidden\" name=\"id\" value=\""+std::to_string(p.id)+"\">"
                       + next_in +
                       "<input type=\"hidden\" name=\"choice\" value=\""+std::to_string(static_cast<int>(v))+"\">"
                       "<button class=\""+std::string(active ? "" : "ghost")+"\" type=\"submit\">"+label+
                       (active ? " &#10003;" : "")+"</button></form>";
            };
            voteForm = "<div class=\"row\">"+btn(ProposalVote::Yes,"Yes")+btn(ProposalVote::No,"No")+
                       btn(ProposalVote::Abstain,"Abstain")+"</div>";
        }
        body = bar + voteForm;
    } else if (p.ballot.kind == BallotKind::Choice) {
        body = choice_ballot_body(p, viewer, next_in, "choice");
    } else {
        auto t = g_store.value_tally_of(p.id);
        auto mine = viewer.empty() ? std::optional<std::string>{} : g_store.ballot_value_of(p.id, viewer);
        std::string summary;
        if (p.ballot.kind == BallotKind::Date) {
            if (t.n == 0) summary = "<p class=\"muted\">No dates yet.</p>";
            else summary = "<p>Median date: <strong>"+html_escape(t.median_date)+"</strong></p>"
                           "<p class=\"muted\">"+std::to_string(t.n)+" responses · earliest "+html_escape(t.lo_date)+
                           " · latest "+html_escape(t.hi_date)+"</p>";
        } else {
            std::string unit = p.ballot.unit.empty() ? "" : " "+html_escape(p.ballot.unit);
            if (p.ballot.kind == BallotKind::Scale) {
                std::string left = (p.ballot.options.empty() || p.ballot.options[0].empty()) ? "0" : html_escape(p.ballot.options[0]);
                std::string right = (p.ballot.options.size() < 2 || p.ballot.options[1].empty()) ? "100" : html_escape(p.ballot.options[1]);
                if (t.n == 0) summary = "<p class=\"muted\">"+left+" — "+right+". No responses yet.</p>";
                else summary = "<p>"+left+" — <strong>"+fmt_ballot_num(t.median)+"</strong> — "+right+"</p>"
                               "<p class=\"muted\">median of "+std::to_string(t.n)+" · range "+fmt_ballot_num(t.lo)+
                               "–"+fmt_ballot_num(t.hi)+"</p>";
            } else {
                if (t.n == 0) summary = "<p class=\"muted\">No numbers yet.</p>";
                else summary = "<p>Median: <strong>"+fmt_ballot_num(t.median)+unit+"</strong></p>"
                               "<p class=\"muted\">"+std::to_string(t.n)+" responses · range "+fmt_ballot_num(t.lo)+
                               "–"+fmt_ballot_num(t.hi)+unit+"</p>";
            }
        }
        std::string voteForm;
        if (!viewer.empty()) {
            voteForm = "<form method=\"POST\" action=\"/proposals/vote\">"
                       "<input type=\"hidden\" name=\"id\" value=\""+std::to_string(p.id)+"\">"
                       + next_in;
            if (p.ballot.kind == BallotKind::Date) {
                voteForm += "<label>Your date</label><input type=\"date\" name=\"value\" required";
                if (mine) voteForm += " value=\""+html_escape(*mine)+"\"";
                voteForm += ">";
            } else if (p.ballot.kind == BallotKind::Scale) {
                std::string cur = mine ? html_escape(*mine) : "50";
                voteForm += "<label>Your mark <span class=\"muted\" id=\"scv"+std::to_string(p.id)+"\">"+cur+"</span></label>"
                            "<input type=\"range\" name=\"value\" min=\"0\" max=\"100\" value=\""+cur+
                            "\" oninput=\"document.getElementById('scv"+std::to_string(p.id)+"').textContent=this.value\">";
            } else {
                voteForm += "<label>Your number";
                if (!p.ballot.unit.empty()) voteForm += " ("+html_escape(p.ballot.unit)+")";
                voteForm += "</label><input type=\"number\" name=\"value\" required step=\"any\"";
                if (p.ballot.min_v != 0 || p.ballot.max_v != 0) {
                    voteForm += " min=\""+fmt_ballot_num(p.ballot.min_v)+"\" max=\""+fmt_ballot_num(p.ballot.max_v)+"\"";
                }
                if (mine) voteForm += " value=\""+html_escape(*mine)+"\"";
                voteForm += ">";
            }
            voteForm += "<button>Save my answer</button></form>";
        }
        body = summary + voteForm;
    }
    std::string when = fmt_time(p.ts);
    std::string place;
    if (!p.city.empty() || !p.who_acts.empty()) {
        place = "<p class=\"muted\">";
        if (!p.city.empty()) {
            std::string name = p.city;
            for (const auto& cu : g_store.cities_in_use()) if (cu.key == p.city) { name = cu.name; break; }
            place += "City: <a href=\"/city/"+url_encode(p.city)+"\">"+html_escape(name)+"</a>";
        }
        if (!p.who_acts.empty()) {
            if (!p.city.empty()) place += " · ";
            place += "Who could act: "+html_escape(p.who_acts);
        }
        place += "</p>";
        if (!p.tags.empty()) {
            place += "<p class=\"muted\">";
            bool first_tag = true;
            for (const auto& tag : p.tags) {
                if (!first_tag) place += " · ";
                first_tag = false;
                place += html_escape(tag);
            }
            place += "</p>";
        }
        if (!p.city.empty())
            place += "<p class=\"muted\">Discussion only. Nothing is sent to the city.</p>";
    }
    return "<p class=\"muted eyebrow\" id=\"proposal-"+std::to_string(p.id)+"\">Proposal "+std::to_string(p.id)+" · "+kindEyebrow+"</p>"
           "<h3 style=\"margin:.1rem 0\">"+html_escape(p.title)+"</h3>"
           "<p class=\"muted\">by <a href=\"/u/"+html_escape(p.author)+"\">"+html_escape(p.author)+"</a>"+
           (when.empty()?"":" · "+html_escape(when))+"</p>"
           + place +
           "<p style=\"white-space:pre-wrap\">"+html_escape(p.body)+"</p>"+body
           + proposal_talk(p, viewer, next_in);
}

// Comments and one reaction per person. A reaction is not a vote.
// Clicking the reaction you already have removes it.
std::string proposal_talk(const Proposal& p, const std::string& viewer, const std::string& next_in) {
    auto label_of = [](const std::string& kind) {
        if (kind.empty()) return kind;
        std::string s = kind;
        if (s[0] >= 'a' && s[0] <= 'z') s[0] = static_cast<char>(s[0] - 'a' + 'A');
        return s;
    };
    const auto counts = g_store.proposal_reaction_counts(p.id);
    auto mine = viewer.empty() ? std::optional<std::string>{} : g_store.proposal_reaction_of(p.id, viewer);
    std::string row = "<div class=\"row\" style=\"flex-wrap:wrap;gap:.4rem;margin-top:.6rem\">";
    for (const auto& item : counts) {
        const std::string& kind = item.first;
        const int n = item.second;
        const bool active = mine && *mine == kind;
        const std::string label = label_of(kind);
        if (!viewer.empty()) {
            row += "<form class=\"inline\" method=\"POST\" action=\"/proposals/react\">"
                   "<input type=\"hidden\" name=\"id\" value=\""+std::to_string(p.id)+"\">"
                   + next_in +
                   "<input type=\"hidden\" name=\"kind\" value=\""+html_escape(kind)+"\">"
                   "<button class=\""+std::string(active ? "" : "ghost")+"\" type=\"submit\">"+
                   html_escape(label)+" · "+std::to_string(n)+
                   (active ? " &#10003;" : "")+"</button></form>";
        } else if (n > 0) {
            row += "<span class=\"muted\">"+html_escape(label)+" · "+std::to_string(n)+"</span>";
        }
    }
    row += "</div><p class=\"muted\">A reaction is not a vote.</p>";
    auto replies = g_store.proposal_replies_of(p.id);
    std::string list;
    for (const auto& r : replies) {
        list += "<div class=\"who\" style=\"margin:.5rem 0\">"+portrait_tag(r.author,"sm")+
                "<div><a href=\"/u/"+html_escape(r.author)+"\"><strong>"+html_escape(r.author)+
                "</strong></a> <span class=\"muted\">"+html_escape(fmt_time(r.ts))+"</span>"
                "<p style=\"margin:.2rem 0 0\">"+html_escape(r.text)+"</p></div></div>";
    }
    std::string replyForm;
    if (!viewer.empty()) {
        replyForm = "<form method=\"POST\" action=\"/proposals/reply\" style=\"margin-top:.3rem\">"
                    "<input type=\"hidden\" name=\"id\" value=\""+std::to_string(p.id)+"\">"
                    + next_in +
                    "<textarea name=\"text\" maxlength=\"1000\" placeholder=\"Write a comment\"></textarea>"
                    "<button class=\"ghost\" type=\"submit\">Comment</button></form>";
    }
    std::string repliesBlock;
    if (!replies.empty() || !viewer.empty()) {
        repliesBlock = "<details style=\"margin-top:.4rem\"><summary class=\"muted\" style=\"cursor:pointer\">"+
            std::to_string(replies.size())+(replies.size()==1?" comment":" comments")+"</summary>"
            "<div style=\"padding-left:.8rem;border-left:2px solid var(--rule);margin-top:.3rem\">"+
            list+replyForm+"</div></details>";
    }
    return row + repliesBlock;
}

// Shared by every post composer (/me/post, /feed, /c/<slug>): pulls the
// text field and an optional media file out of either a multipart or
// urlencoded body, and -- if a file was attached -- ingests it through the
// exact same pipeline as a mail attachment (content-sniffed, malware
// scanned, images normalized; video stored as-is after a real container
// signature match, see file_transfer.hpp). ok==false means the media
// itself failed and no post should be created at all, rather than
// silently dropping the attachment and posting text-only.
struct ComposerPost {
    bool ok = true;
    std::string text;
    std::string attachment_blob_id;
    std::string attachment_mime;
    const char* err = nullptr; // "large" | "type" | "active" | "scan"
};
ComposerPost read_composer_post(const std::string& hdrs, const std::string& body) {
    ComposerPost r;
    std::string ctype = header_value(hdrs, "Content-Type");
    std::optional<std::vector<std::uint8_t>> file;
    if (ctype.find("multipart/form-data") != std::string::npos) {
        r.text = multipart_field(ctype, body, "text");
        file = multipart_file(ctype, body, "media");
    } else {
        r.text = parse_form(body)["text"];
    }
    if (r.text.size() > 2000) r.text.resize(2000);
    if (file && !file->empty()) {
        auto res = ingest_file(g_blobs, g_mail_scanner, g_mail_caps, *file);
        if (res.error != FileTransferError::None || !res.blob_id) {
            r.ok = false;
            switch (res.error) {
                case FileTransferError::TooLarge: r.err = "large"; break;
                case FileTransferError::ActiveContentBlocked: r.err = "active"; break;
                case FileTransferError::FailedScan: r.err = "scan"; break;
                default: r.err = "type"; break;
            }
            return r;
        }
        r.attachment_blob_id = *res.blob_id;
        r.attachment_mime = res.stored_mime_type.value_or("");
    }
    return r;
}

// Every page's "something went wrong" banner reads this one query key now
// -- collapses what used to be three (err/perr/jerr), each with its own
// scattered if/else copied into whichever GET handler needed it. That
// scattering is exactly how ?err=short (settings, new password too short)
// ended up wired on the write side but never read on the get side: the
// redirect fired, the message never rendered. One table, one place to add
// a code, nothing to forget on the read side ever again.
//
// A short code is reused across pages ONLY when the message is actually
// the same sentence (e.g. "invite", the same contact-settings conflict on
// a mail group and a community); where two contexts used the same word for
// a genuinely different limit -- the avatar's 2 MiB cap vs. a post
// attachment's 10/50 MiB cap, or a canvas frame's own cap -- they get their
// own name instead (portraitlarge / large / framelarge). That's the
// discipline this exists to enforce: a code names one sentence, never two.
std::string flash_error(const std::map<std::string,std::string>& qs) {
    auto it = qs.find("err");
    if (it == qs.end()) return "";
    const std::string& e = it->second;
    if (e=="login") return "<p class=\"warn\">Invalid username or password.</p>";
    if (e=="ratelimit") return "<p class=\"warn\">Too many attempts. Wait a moment and try again.</p>";
    if (e=="empty") return "<p class=\"warn\">Need a title and a body.</p>";
    if (e=="image") return "<p class=\"warn\">That file was not a usable image (PNG/JPEG/GIF, 2&nbsp;MiB, 1024px). SVG is not accepted.</p>";
    if (e=="portraitlarge") return "<p class=\"warn\">That file is too large. Cap is 2&nbsp;MiB and 1024 pixels on the long edge.</p>";
    if (e=="scan") return "<p class=\"warn\">That file could not be stored.</p>";
    if (e=="profile") return "<p class=\"warn\">Display name is required (up to 80 characters).</p>";
    if (e=="css") {
        auto rit = qs.find("reason");
        return "<p class=\"warn\">"+html_escape(css_reject_message(rit==qs.end()?"":rit->second))+"</p>";
    }
    if (e=="inner") return "<p class=\"warn\">Inner circle holds twelve. Take someone out first.</p>";
    if (e=="invite") return "<p class=\"warn\">Conflicts with another member’s contact settings.</p>";
    if (e=="address") return "<p class=\"warn\">Need a free address: letters, numbers, dots, underscores, hyphens.</p>";
    if (e=="subcategory") return "<p class=\"warn\">Pick or name a subcategory.</p>";
    if (e=="city") return "<p class=\"warn\">Only the person who started this group can change its city.</p>";
    if (e=="list") return "<p class=\"warn\">Join a group in this city before you list yourself here.</p>";
    if (e=="pcity") return "<p class=\"warn\">Name a city that a group already uses, or leave the city blank.</p>";
    if (e=="ctag") return "<p class=\"warn\">Join a group in this city before you add a department or council.</p>";
    if (e=="ctagname") return "<p class=\"warn\">Name a department or council.</p>";
    if (e=="password") return "<p class=\"warn\">Could not change password. Check the current one.</p>";
    if (e=="short") return "<p class=\"warn\">New password needs at least eight characters.</p>";
    if (e=="delete") return "<p class=\"warn\">Type your username to delete.</p>";
    if (e=="large") return "<p class=\"warn\">That file was too large (images 10&nbsp;MiB, video 50&nbsp;MiB).</p>";
    if (e=="active") return "<p class=\"warn\">That document contains active content and was rejected.</p>";
    if (e=="type") return "<p class=\"warn\">That file wasn't a supported image or video type.</p>";
    if (e=="nogroup") return "<p class=\"warn\">Pick at least one group, or turn Groups off.</p>";
    if (e=="options") return "<p class=\"warn\">Need 2–12 options, or 1–12 if people can add their own.</p>";
    if (e=="option") return "<p class=\"warn\">Could not add that choice. Duplicate, empty, or you already added one.</p>";
    if (e=="url") return "<p class=\"warn\">That link wasn't recognized as a YouTube video.</p>";
    if (e=="full") return "<p class=\"warn\">Queue already holds "+std::to_string(kJukeboxQueueCap)+" tracks. Remove one first.</p>";
    if (e=="denied") return "<p class=\"warn\">You can't queue a track here.</p>";
    if (e=="pending") return "<p class=\"warn\">You already have a track queued. Wait for it to play first.</p>";
    if (e=="google") return "<p class=\"warn\">Couldn&rsquo;t verify that Google sign-in. Try again.</p>";
    if (e=="google-down") return "<p class=\"warn\">Couldn&rsquo;t reach Google to check your sign-in. Try again in a moment.</p>";
    if (e=="reset") return "<p class=\"warn\">That link has expired or was already used. Request a new one.</p>";
    return "";
}

// One checkbox per name, sharing a form field name -- used for both the
// Anybody hide-list and the Friends show-list (same friend roster, opposite
// meaning) and for the Groups show-list (community slugs).
std::string composer_checkbox_list(const std::string& field_name,
                                    const std::vector<std::pair<std::string,std::string>>& value_label_pairs,
                                    const char* empty_note) {
    if (value_label_pairs.empty())
        return "<p class=\"muted\" style=\"margin:.3rem 0 0\">"+std::string(empty_note)+"</p>";
    std::string html;
    for (const auto& vl : value_label_pairs) {
        html += "<label style=\"display:flex;align-items:center;gap:.4rem;font-weight:400;margin:.25rem 0\">"
                "<input type=\"checkbox\" name=\""+field_name+"\" value=\""+html_escape(vl.first)+"\">"+
                html_escape(vl.second)+"</label>";
    }
    return html;
}

// The /feed composer card. Three icon chips replace the old static
// "Friends only" label -- the glyph carries the reach (globe/people/rooms),
// so nobody has to read a sentence to know what "Anybody" or "Groups"
// means, and each opens its own exceptions/allow-list picker (Grok's
// model, not a Members-relabel and not "post to every group I'm in"):
//   Anybody  -- a personal post, audience=Anybody. Reaches friends in
//              /feed same as a Friends post; ALSO shows on the author's own
//              profile to any other viewer (signed in or not) EXCEPT
//              whoever is checked in its "Exceptions" hide-list. Never
//              lands in a stranger's own /feed -- that stays a friends
//              graph, not a public firehose.
//   Friends  -- audience=Friends, today's only behavior before this
//              existed and still the default. Its "Which friends" list is
//              a show-list: checked names (plus the author) see it in
//              /feed; empty means every current friend, unchanged.
//   Groups   -- one or more real community posts (Post.community + extra
//              extra_groups), same as posting from each community's own
//              page -- a destination picker, not a visibility bit. Only
//              offered when the author actually belongs to a community
//              (join-to-post unchanged), and refused if turned on with
//              nothing picked.
// Anybody is exclusive with the other two (public vs. not is a ladder);
// Friends and Groups can both be on at once -- one Post row, audience
// stays Friends, with community/extra_groups also set, per the "pau"
// LGP1 tag's own comment.
//
// There used to also be a permanent paragraph under the box spelling all
// of this out ("Friends shows in your friends' feed... This is not a civic
// act.") -- gone now. The reach explanation only exists inside each
// picker's own disclosure, read by whoever actually opens it, not by
// everyone on every visit (matches the same "don't stack four empty-state
// lines" note this composer already followed for its media errors).
std::string feed_composer_card(const std::string& user) {
    auto friends = g_store.friends_of(user);
    std::vector<std::pair<std::string,std::string>> friend_pairs;
    for (const auto& f : friends) friend_pairs.push_back({f, f});
    auto my_groups = g_store.communities_of(user);
    std::vector<std::pair<std::string,std::string>> group_pairs;
    for (const auto& c : my_groups) group_pairs.push_back({c.slug, c.name});
    const bool has_groups = !my_groups.empty();

    auto ichip = [](const char* id, const char* svg, const char* label, bool on) {
        return "<button type=\"button\" class=\"ichip"+std::string(on?" on":"")+"\" id=\""+id+"\">"
               +svg+"<span>"+label+"</span></button>";
    };
    std::string groups_chip = has_groups ? ichip("vis-groups", groups_svg(), "Groups", false) : "";
    std::string groups_picker = has_groups
        ? "<details id=\"opts-groups\" hidden><summary class=\"muted\">Which groups</summary>"
          "<p class=\"muted\" style=\"margin:.3rem 0 .5rem\">Pick one or more rooms you belong to.</p>"
          +composer_checkbox_list("group", group_pairs, "")+ "</details>"
        : "";

    return
        "<div class=\"card\"><form method=\"POST\" action=\"/feed\" enctype=\"multipart/form-data\" id=\"feedform\">"
        "<label style=\"margin:0 0 .5rem;display:block\">Write to people who know you</label>"
        "<div class=\"row\" style=\"gap:.5rem;margin:0 0 .3rem\">"
        +ichip("vis-anybody", globe_svg(), "Anybody", false)+
        ichip("vis-friends", friends_svg(), "Friends", true)+
        groups_chip+
        "</div>"
        "<details id=\"opts-anybody\" hidden><summary class=\"muted\">Exceptions</summary>"
        "<p class=\"muted\" style=\"margin:.3rem 0 .5rem\">Hide-list &mdash; checked friends won't see "
        "this, in feed or on your profile.</p>"
        +composer_checkbox_list("except", friend_pairs, "No friends yet to except -- Anybody reaches everyone.")+
        "</details>"
        "<details id=\"opts-friends\" hidden><summary class=\"muted\">Which friends</summary>"
        "<p class=\"muted\" style=\"margin:.3rem 0 .5rem\">Show-list &mdash; leave everyone unchecked to "
        "reach all your friends.</p>"
        +composer_checkbox_list("friend", friend_pairs, "No friends yet -- add some first.")+
        "</details>"
        + groups_picker +
        "<input type=\"hidden\" name=\"anybody\" id=\"anybodyfield\" value=\"0\">"
        "<input type=\"hidden\" name=\"friends\" id=\"friendsfield\" value=\"1\">"
        "<input type=\"hidden\" name=\"groups\" id=\"groupsfield\" value=\"0\">"
        "<textarea name=\"text\" maxlength=\"2000\" placeholder=\"Plain text.\" "
        "oninput=\"document.getElementById('feedchars').textContent=(2000-this.value.length)+' left'\"></textarea>"
        "<p class=\"muted\" id=\"feedchars\" style=\"margin:.2rem 0 0;text-align:right\">2000 left</p>"
        "<div class=\"row\" style=\"justify-content:space-between;align-items:center;margin-top:.4rem\">"
        "<button type=\"button\" class=\"attachbtn\" id=\"feedattachbtn\" aria-label=\"Attach a photo or video\" "
        "title=\"Attach a photo or video\">"+std::string(paperclip_svg())+"</button>"
        "<button type=\"submit\">Post</button></div>"
        "<input type=\"file\" name=\"media\" id=\"feedmedia\" accept=\"image/*,video/*\" hidden>"
        "<p class=\"muted\" id=\"feedfilename\" style=\"margin:.3rem 0 0\" hidden></p>"
        "</form>"
        "<p class=\"muted\" style=\"margin-top:.5rem\">Or <a href=\"/feed/canvas\">open the canvas</a> "
        "to draw, crop, layer, or paste one together first.</p></div>"
        "<script>(function(){"
        "var state={anybody:false,friends:true,groups:false};"
        "var chipA=document.getElementById('vis-anybody'),chipF=document.getElementById('vis-friends'),"
        "chipG=document.getElementById('vis-groups');"
        "var optsA=document.getElementById('opts-anybody'),optsF=document.getElementById('opts-friends'),"
        "optsG=document.getElementById('opts-groups');"
        "var fldA=document.getElementById('anybodyfield'),fldF=document.getElementById('friendsfield'),"
        "fldG=document.getElementById('groupsfield');"
        "function render(){"
        "if(!state.anybody&&!state.friends&&!state.groups)state.friends=true;"
        "chipA.classList.toggle('on',state.anybody);"
        "chipF.classList.toggle('on',state.friends);"
        "if(chipG)chipG.classList.toggle('on',state.groups);"
        "optsA.hidden=!state.anybody;optsF.hidden=!state.friends;"
        "if(optsG)optsG.hidden=!state.groups;"
        "fldA.value=state.anybody?'1':'0';fldF.value=state.friends?'1':'0';fldG.value=state.groups?'1':'0';"
        "}"
        "chipA.addEventListener('click',function(){"
        "state.anybody=!state.anybody;"
        "if(state.anybody){state.friends=false;state.groups=false;}"
        "render();"
        "});"
        "chipF.addEventListener('click',function(){state.friends=!state.friends;if(state.friends)state.anybody=false;render();});"
        "if(chipG)chipG.addEventListener('click',function(){state.groups=!state.groups;if(state.groups)state.anybody=false;render();});"
        "render();"
        "var attachBtn=document.getElementById('feedattachbtn'),mediaInput=document.getElementById('feedmedia'),"
        "fileNameEl=document.getElementById('feedfilename');"
        "attachBtn.addEventListener('click',function(){mediaInput.click();});"
        "mediaInput.addEventListener('change',function(){"
        "var has=mediaInput.files&&mediaInput.files.length>0;"
        "attachBtn.classList.toggle('has-file',has);"
        "fileNameEl.hidden=!has;fileNameEl.textContent=has?mediaInput.files[0].name:'';"
        "});"
        "document.getElementById('feedform').addEventListener('submit',function(e){"
        "if(state.groups&&optsG&&!optsG.querySelector('input:checked')){e.preventDefault();optsG.open=true;optsG.scrollIntoView({block:'center'});}"
        "});"
        "})();</script>";
}

// /me saves itself (Brandon 2026-11-11 4:50 PM, Brief 4 item 11; replaced the "Editing /me" bar with Publish
// revision and Discard): each form[data-draft] -- About you, Profile CSS and Play settings (Brief 6 item 4) -- is
// sent to its own, unchanged endpoint with fetch about 800 ms after the last change, so there is no page load (no
// jump to the top). A small status under the form says Saving..., Saved, or Couldn't save with a Retry button: a
// refused value (the endpoint redirects with err=) or a failed request. These forms have no Save button (Brief 6
// item 4); a submit button, if one is ever added, is hidden here. Enter or a submit saves at once. The name is kept
// (draft_publish_script) so its callers and checks still find it.
std::string draft_publish_script() {
    return
        "<script>(function(){"
        "var forms=Array.prototype.slice.call(document.querySelectorAll('form[data-draft]'));"
        "forms.forEach(function(f){"
        "var btn=f.querySelector('button[type=submit],button:not([type])');"
        "if(btn)btn.hidden=true;"
        "var box=document.createElement('p');box.className='muted';box.style.margin='.35rem 0 0';"
        "var st=document.createElement('span');st.setAttribute('data-draft-status','');"
        "st.setAttribute('role','status');st.setAttribute('aria-live','polite');"
        "var rb=document.createElement('button');rb.type='button';rb.className='ghost';rb.textContent='Retry';"
        "rb.hidden=true;rb.setAttribute('data-draft-retry','');rb.style.marginLeft='.5rem';"
        "box.appendChild(st);box.appendChild(rb);f.parentNode.insertBefore(box,f.nextSibling);"
        "function ser(){var p=[];new FormData(f).forEach(function(v,k){p.push(k+'='+(typeof v==='string'?v:'[file]'));});"
        "return p.sort().join('&');}"
        "var saved=ser(),timer=null,busy=false,again=false;"
        "function show(t,retry){st.textContent=t;rb.hidden=!retry;}"
        // Without a Save button the browser no longer points at a required field left empty (a Google-only account
        // has no display name, so /me/profile refuses every About you save): name the field in the status instead.
        "function why(){var e=f.querySelector(':invalid');if(!e)return '';"
        "var l=e.previousElementSibling,n=l&&l.tagName==='LABEL'?l.textContent.trim():e.name;"
        "return ': '+n+' \\u2014 '+e.validationMessage;}"
        "function save(){"
        "clearTimeout(timer);"
        "if(busy){again=true;return;}"
        "var now=ser();if(now===saved&&rb.hidden)return;"
        "busy=true;show('Saving…',false);"
        "fetch(f.getAttribute('action'),{method:'POST',credentials:'same-origin',"
        "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
        "body:new URLSearchParams(new FormData(f)).toString()})"
        ".then(function(r){if(!r.ok||/[?&]err=/.test(r.url||''))throw new Error('refused');saved=now;show('Saved',false);})"
        ".catch(function(){show('Couldn\\u2019t save'+why(),true);})"
        ".then(function(){busy=false;if(again){again=false;save();}});"
        "}"
        "function later(){clearTimeout(timer);timer=setTimeout(save,800);}"
        "f.addEventListener('input',later);"
        "f.addEventListener('change',later);"
        "f.addEventListener('submit',function(e){e.preventDefault();save();});"
        "rb.addEventListener('click',save);"
        "});"
        "})();</script>";
}

// Personal jukeboxes ("u:<owner>" stations) share the exact same
// frozen_offset + resumed_at + listener_count shape as the WebSocket
// community stations (see CommunityNowPlaying / community_elapsed_locked
// above) -- just driven by HTTP heartbeats instead of a live socket,
// since a personal jukebox has never had one (jukebox_widget's own
// comment: "an ordinary HTTP queue with no live sync"). No second
// WebSocket type just for this: a POST roughly every 15s while actually
// playing stands in for "still connected," pruned on a grace window
// generous enough to absorb one missed beat plus iOS timer jitter
// without mistaking a briefly-backgrounded tab for an empty room.
inline constexpr std::int64_t kPersonalHeartbeatGraceSeconds = 30;
struct PersonalPlayhead {
    std::string video_id; // which track frozen_offset/resumed_at apply to
    std::int64_t frozen_offset = 0;
    std::int64_t resumed_at = 0;
};
std::mutex g_personal_mu;
std::map<std::string, PersonalPlayhead> g_personal_playhead_by_station;                     // "u:<owner>" -> playhead
std::map<std::string, std::map<std::string, std::int64_t>> g_personal_listeners_by_station; // "u:<owner>" -> session id -> last_seen_unix

// Caller must hold g_personal_mu. A different track than last time
// (completed/skipped/reordered) always starts the playhead over at 0,
// same as community's track-advance -- persisted immediately so a
// restart right after a track change doesn't fall back to a stale entry
// for the wrong video. The only place any of the three functions below
// resets frozen_offset to 0; none of them mix an old elapsed value into
// a new track.
PersonalPlayhead& personal_playhead_for_locked(const std::string& station, const std::string& current_video_id,
                                                std::int64_t now) {
    auto& ph = g_personal_playhead_by_station[station];
    if (ph.video_id != current_video_id) {
        ph = PersonalPlayhead{current_video_id, 0, now};
        g_personal_listeners_by_station[station].clear();
        g_store.set_jukebox_pos(station, current_video_id, 0, now);
    }
    return ph;
}

// Caller must hold g_personal_mu. Pure cleanup, no opinion about what a
// live->empty transition means -- each caller below decides that for
// itself, in its own correct order (see each function's own comment;
// getting this order wrong is exactly what turned a real bug -- the
// community WS side's disconnect handler -- into a freeze that silently
// landed at 0 instead of the real elapsed, caught only by testing the
// running server directly, not by the build or the unit tests).
void personal_prune_locked(const std::string& station, std::int64_t now) {
    auto& listeners = g_personal_listeners_by_station[station];
    for (auto it = listeners.begin(); it != listeners.end();) {
        if (now - it->second > kPersonalHeartbeatGraceSeconds) it = listeners.erase(it);
        else ++it;
    }
}

// Safe to call from GET /jukebox/now, which must never itself count as a
// listen heartbeat (a paused-but-open widget still polls this) -- never
// touches or adds a heartbeat entry, only ever removes stale ones.
// Elapsed is snapshotted BEFORE pruning, using the true pre-prune live
// state, so the number returned here is never affected by whether this
// particular call happens to be the one that notices a stale listener.
std::int64_t personal_jukebox_elapsed(const std::string& station, const std::string& current_video_id) {
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::lock_guard<std::mutex> lock(g_personal_mu);
    auto& ph = personal_playhead_for_locked(station, current_video_id, now);
    auto& listeners = g_personal_listeners_by_station[station];
    const bool live_before = !listeners.empty();
    const std::int64_t elapsed = ph.frozen_offset + (live_before ? std::max<std::int64_t>(0, now - ph.resumed_at) : 0);
    personal_prune_locked(station, now);
    // A listener who vanished without ever sending a pause/pagehide
    // freeze (tab killed, network loss, crash): freeze right here at the
    // value already snapshotted above, instead of silently losing their
    // progress back to whatever frozen_offset was before this session
    // started, and persist it so a restart around now still resumes
    // from the right place.
    if (live_before && listeners.empty()) {
        ph.frozen_offset = elapsed;
        g_store.set_jukebox_pos(station, ph.video_id, elapsed, now);
    }
    return elapsed;
}

// Called only from POST /jukebox/pos while actually playing (no freeze
// flag) -- registers/refreshes this session as currently listening. Does
// NOT trust any client-supplied offset; the server clock is the only
// thing that ever advances resumed_at, precisely so two listeners
// reporting drifted currentTime values can't fight over the shared
// position. Touches this session's own heartbeat BEFORE pruning, so its
// own (possibly late) arrival can never be the reason the prune sees an
// empty station and invents a freeze in the middle of what is actually a
// live check-in; 0->1 (nobody else was here) just resumes -- it never
// writes jpos itself, since jpos already correctly holds wherever the
// last listener actually left off.
void personal_heartbeat_locked(const std::string& station, const std::string& current_video_id,
                                const std::string& session_id, std::int64_t now) {
    auto& ph = personal_playhead_for_locked(station, current_video_id, now);
    auto& listeners = g_personal_listeners_by_station[station];
    const bool was_live = !listeners.empty();
    listeners[session_id] = now;
    personal_prune_locked(station, now); // only genuinely-stale *other* sessions can be removed now
    if (!was_live) ph.resumed_at = now;
}

// Called only from POST /jukebox/pos with freeze=1 (a manual pause, or a
// pagehide sendBeacon) -- this session is stopping right now, not
// waiting out the grace window. Elapsed is computed while this session's
// own entry still counts (before it's erased) -- erasing or pruning
// first would silently drop this session's own listening time, the same
// ordering mistake already caught and fixed on the community WS side.
// client_offset is trusted only if it's within 3s of the server's own
// idea of elapsed; otherwise the server clock wins, same distrust as the
// live heartbeat path. Only actually freezes (and persists) if this was
// the last listener once other genuinely-stale sessions are pruned too
// -- someone else still fresh means the shared playhead keeps running
// for them.
void personal_freeze_locked(const std::string& station, const std::string& current_video_id,
                             const std::string& session_id, std::optional<std::int64_t> client_offset,
                             std::int64_t now) {
    auto& ph = personal_playhead_for_locked(station, current_video_id, now);
    auto& listeners = g_personal_listeners_by_station[station];
    const bool was_live = !listeners.empty();
    const std::int64_t server_elapsed = ph.frozen_offset +
        (was_live ? std::max<std::int64_t>(0, now - ph.resumed_at) : 0);
    listeners.erase(session_id);
    personal_prune_locked(station, now);
    if (!listeners.empty()) return; // someone else is still listening -- their playhead keeps running
    const std::int64_t diff = client_offset ?
        (*client_offset > server_elapsed ? *client_offset - server_elapsed : server_elapsed - *client_offset) : -1;
    const std::int64_t offset = (client_offset && diff <= 3) ? *client_offset : server_elapsed;
    ph.frozen_offset = offset;
    g_store.set_jukebox_pos(station, ph.video_id, offset, now);
}


static bool push_subs_upsert(const std::string& user, const std::string& endpoint,
                             const std::string& p256dh, const std::string& auth) {
    if (user.empty() || endpoint.size() < 12 || endpoint.compare(0, 8, "https://") != 0) return false;
    if (endpoint.find('\t') != std::string::npos || p256dh.find('\t') != std::string::npos) return false;
    if (auth.find('\t') != std::string::npos) return false;
    std::vector<std::string> keep;
    std::ifstream in("push_subs.txt");
    std::string line;
    int mine = 0;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto t1 = line.find('\t');
        auto t2 = line.find('\t', t1 == std::string::npos ? 0 : t1 + 1);
        auto t3 = line.find('\t', t2 == std::string::npos ? 0 : t2 + 1);
        if (t1 == std::string::npos || t2 == std::string::npos || t3 == std::string::npos) continue;
        std::string ep = line.substr(t1 + 1, t2 - t1 - 1);
        if (ep == endpoint) continue;
        if (line.compare(0, t1, user) == 0) {
            if (mine >= 7) continue;
            ++mine;
        }
        keep.push_back(line);
    }
    keep.push_back(user + "\t" + endpoint + "\t" + p256dh + "\t" + auth);
    std::ofstream out("push_subs.txt.tmp", std::ios::trunc);
    if (!out) return false;
    for (auto& l : keep) out << l << "\n";
    out.close();
    return std::rename("push_subs.txt.tmp", "push_subs.txt") == 0;
}

// `req` is the request line and headers only; the body arrives on its own
// (read_http_request keeps them apart so an upload is never copied).
std::string handle(const std::string& req, const std::string& peer_ip, std::string body) {
    auto line_end=req.find("\r\n");
    if(line_end==std::string::npos) line_end=req.find('\n');
    if(line_end==std::string::npos) return http_ok(wrap("Error","",0,"<p>bad request</p>"));
    std::string method, rawpath; { std::istringstream is(req.substr(0,line_end)); is>>method>>rawpath; }
    if (method == "HEAD") method = "GET";
    std::string path = rawpath;
    auto qpos=path.find('?'); if(qpos!=std::string::npos) path=path.substr(0,qpos);
    // Program uploads come in through /mail/big (nginx lets big bodies only
    // in there, after asking /internal/upload-ok); past the door they are
    // the ordinary /mail and /mail/g/<id>/send posts.
    if (method == "POST" && (path == "/mail/big" || path.rfind("/mail/big/", 0) == 0)) path = "/mail" + path.substr(9);
    auto qs = parse_qs(rawpath);
    std::lock_guard<std::mutex> lock(g_mu);
    auto proto = header_value(req, "X-Forwarded-Proto");
    for (char& c : proto) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    g_https = (proto == "https");
    {
        auto dest = header_value(req, "Sec-Fetch-Dest");
        for (char& c : dest) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        auto eq = qs.find("embed");
        g_embed = dest == "iframe" || (eq != qs.end() && eq->second == "1");
    }
    const std::string ip = forwarded_ip(req, peer_ip);
    g_theme_mode = theme_of(req);
    auto acc = account_from_req(req);
    std::string user = acc ? acc->username : "";
    const std::string sid = cookie_of(req);
    const int unread = acc ? g_store.unread_count(user) : 0;
    auto pg = [&](const std::string& title, const std::string& b) {
        return http_ok(wrap(title, user, unread, b));
    };
    auto need_login = [&]() {
        if (method == "GET") return http_redir(login_url(path));
        return http_redir("/login");
    };
    // A signed-in non-operator gets exactly the same body as a genuinely
    // missing page -- /admin must not be distinguishable from a 404 to
    // anyone it isn't for.
    auto admin_404 = [&]() {
        return http_code("404 Not Found", wrap("Not found", user, unread,
            "<div class=\"card\"><h1>Not found</h1><p>That page is not here.</p></div>"));
    };

    // nginx's auth_request for the /mail/big upload door (nginx blocks
    // /internal/ from outside).
    if (path == "/internal/upload-ok") return upload_ok_response(req);

    if (path.size() > 1 && path.back() == '/') {
        std::string dest = path.substr(0, path.size() - 1);
        auto q = rawpath.find('?');
        if (q != std::string::npos) dest += rawpath.substr(q);
        return http_redir(dest);
    }

    if(method=="GET"&&path=="/robots.txt")
        return http_raw("200 OK", "text/plain; charset=utf-8",
            "User-agent: *\nAllow: /\nDisallow: /settings\nDisallow: /mail\nDisallow: /export\nDisallow: /internal\n");
    if(method=="GET"&&path=="/favicon.ico")
        return http_redir("/favicon.svg");

    // Same-box only: the commons-daemon sibling forwards a visitor's raw
    // Cookie header here to resolve it into an account, exactly the way
    // account_from_req() already does for civic's own pages (cookie-bind.md,
    // Persistence/TheCommons). Never reachable from the public internet --
    // nginx's only path in sets X-Forwarded-For (nginx-liquidgov.us.conf), so
    // its presence means this request came through the proxy, not directly
    // from another loopback process, and is refused the same way /logs
    // refuses a non-operator: a 404 indistinguishable from no route at all.
    if(method=="GET"&&path=="/internal/session-user") {
        if (!header_value(req, "X-Forwarded-For").empty() ||
            (peer_ip != "127.0.0.1" && peer_ip != "::1"))
            return http_raw("404 Not Found", "text/plain", "not found\n");
        return http_raw("200 OK", "application/json",
            acc ? ("{\"username\":\"" + json_escape(user) + "\"}") : std::string("{}"),
            "Cache-Control: no-store\r\n");
    }

    // Same-box only, same gate as /internal/session-user above (refused as
    // a plain 404 if X-Forwarded-For is present or the raw peer isn't
    // loopback). The commons-daemon forwards a visitor's own Cookie here to
    // learn who that session's mutual friends are (glue.md "Friends query").
    // The name comes from the session -- never from a query parameter, so
    // there is no way to ask about anyone but yourself. Read-only: mutual
    // accepts only (friends_of), minus anyone blocked in either direction.
    if(method=="GET"&&path=="/internal/friends") {
        if (!header_value(req, "X-Forwarded-For").empty() ||
            (peer_ip != "127.0.0.1" && peer_ip != "::1"))
            return http_raw("404 Not Found", "text/plain", "not found\n");
        std::string body = "{}";
        if (acc) {
            body = "{\"username\":\"" + json_escape(user) + "\",\"friends\":[";
            bool first = true;
            for (const auto& f : g_store.friends_of(user)) {
                if (g_store.is_blocked(user, f) || g_store.is_blocked(f, user)) continue;
                if (!first) body += ",";
                first = false;
                body += "\"" + json_escape(f) + "\"";
            }
            body += "]}";
        }
        return http_raw("200 OK", "application/json", body, "Cache-Control: no-store\r\n");
    }

    // Same-box only, same gate as /internal/session-user and /internal/friends
    // above (a plain 404 if X-Forwarded-For is present or the raw peer isn't
    // loopback -- nginx's only path in sets X-Forwarded-For, so this can never be
    // reached from the public internet). The commons-daemon asks it whether a
    // visitor may walk into an `open` house: the VIEWER is the session cookie
    // (never a parameter), the path is the house OWNER. {"blocked":true} if
    // either side has blocked the other. It FAILS CLOSED: no session, an unknown
    // owner or a malformed name all answer {"blocked":true} (still a 200, so the
    // daemon can tell it from the public 404), and only a real
    // {"blocked":false} can open a door. Read-only; nothing is written.
    if(method=="GET"&&path.rfind("/internal/blocked/",0)==0) {
        if (!header_value(req, "X-Forwarded-For").empty() ||
            (peer_ip != "127.0.0.1" && peer_ip != "::1"))
            return http_raw("404 Not Found", "text/plain", "not found\n");
        bool blocked = true;
        const std::string owner = path.substr(18);
        if (acc && valid_username(owner)) {
            auto found = g_store.find_by_username(owner);
            if (found)
                blocked = g_store.is_blocked(user, found->username) ||
                          g_store.is_blocked(found->username, user);
        }
        return http_raw("200 OK", "application/json",
            blocked ? std::string("{\"blocked\":true}") : std::string("{\"blocked\":false}"),
            "Cache-Control: no-store\r\n");
    }

    // Same-box only, same gate as /internal/blocked above. Does this username
    // exist as a LiquidGov account right now? The commons-daemon uses it to find
    // game rows whose owner is GONE (orphans: civic lets a deleted username be
    // registered again, and the new owner would otherwise inherit the old
    // player's house and inventory). No cookie and no identity: it asks only
    // about existence. It answers {"exists":false} ONLY for a well-formed name
    // that is not in the store; a malformed name answers {"exists":null}, never
    // false, so nothing weird can ever read as "gone". Read-only.
    if(method=="GET"&&path.rfind("/internal/user-exists/",0)==0) {
        if (!header_value(req, "X-Forwarded-For").empty() ||
            (peer_ip != "127.0.0.1" && peer_ip != "::1"))
            return http_raw("404 Not Found", "text/plain", "not found\n");
        const std::string who = path.substr(std::string("/internal/user-exists/").size());
        std::string exists_body = "{\"exists\":null}";
        if (valid_username(who))
            exists_body = g_store.find_by_username(who) ? "{\"exists\":true}" : "{\"exists\":false}";
        return http_raw("200 OK", "application/json", exists_body, "Cache-Control: no-store\r\n");
    }

    if(method=="GET"&&path=="/logs") {
        // is_operator(), like every other operator page: this read LIQUIDGOV_OPERATOR (singular), which the live
        // service never sets (it sets LIQUIDGOV_OPERATORS), so the page was a 404 for everyone (sweep 2026-11-09).
        if (!acc || !is_operator(user))
            return http_raw("404 Not Found", "text/plain", "not found\n");
        return http_raw("200 OK", "text/plain; charset=utf-8", lglog::dump_text(),
            "Cache-Control: no-store\r\n");
    }
    if(method=="POST"&&path=="/log") {
        if (!acc) return http_raw("401 Unauthorized", "text/plain", "no\n");
        static std::map<std::string, long long> lg_last_ms;
        const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        auto& last = lg_last_ms[user];
        if (now_ms - last < 400)
            return http_raw("204 No Content", "text/plain", "");
        last = now_ms;
        lglog::emit("client", user + " " + lglog::clip_client(body));
        return http_raw("204 No Content", "text/plain", "");
    }

    if(method=="GET"&&path=="/jukebox/search") {
        if(!acc) return http_raw("401 Unauthorized", "application/json", "{\"items\":[]}");
        // Type-to-search used to embed YOUTUBE_API_KEY in every logged-in
        // page. The key stays on the server now, read straight from the
        // environment and never placed in any response body or log line.
        // Proxied via the system curl binary (popen), not a new HTTP/TLS
        // stack -- this binary doesn't link libcurl, and curl is already
        // on the box. term is run through url_encode() before it ever
        // touches the shell string, which leaves only
        // [A-Za-z0-9._~%-] -- none of those can break out of the
        // single-quoted argument, so this is not shell-injectable.
        auto term = trim_ws(qs["q"]);
        std::string q;
        for (unsigned char ch : term) {
            if (std::isalnum(ch) || ch==' ' || ch=='.' || ch=='_' || ch=='-') q.push_back((char)ch);
        }
        if (q.size() < 3)
            return http_raw("200 OK", "application/json",
                "{\"items\":[],\"error\":\"Type at least 3 letters.\"}",
                "Cache-Control: no-store\r\n");
        std::string cache_key = q;
        for (char& c : cache_key) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
        const std::int64_t now_unix = now();
        {
            std::lock_guard<std::mutex> lock(g_yt_mu);
            auto it = g_yt_cache.find(cache_key);
            if (it != g_yt_cache.end() && it->second.exp > now_unix) {
                lglog::emit("jukebox", "search-cache qlen="+std::to_string(q.size()));
                return http_raw("200 OK", "application/json", it->second.body,
                    "Cache-Control: no-store\r\n");
            }
        }
        std::string cmd = "timeout 12 python3 /opt/liquidgov/scripts/yt_search.py '" + q + "' 2>/dev/null";
        std::string resp;
        if (FILE* fp = popen(cmd.c_str(), "r")) {
            char buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) resp.append(buf, n);
            pclose(fp);
        }
        if (resp.empty() || resp[0] != '{' || resp.find("\"items\"") == std::string::npos) {
            lglog::emit("jukebox", "search-bad qlen="+std::to_string(q.size()));
            return http_raw("200 OK", "application/json",
                "{\"items\":[],\"error\":\"Search failed.\"}",
                "Cache-Control: no-store\r\n");
        }
        {
            std::lock_guard<std::mutex> lock(g_yt_mu);
            g_yt_cache[cache_key] = YtCacheEntry{resp, now_unix + 600};
            if (g_yt_cache.size() > 64) g_yt_cache.erase(g_yt_cache.begin());
        }
        lglog::emit("jukebox", "search-ok qlen="+std::to_string(q.size())+" bytes="+std::to_string(resp.size()));
        return http_raw("200 OK", "application/json", resp,
            "Cache-Control: no-store\r\n");
    }
    if(method=="GET"&&path=="/jukebox/meta") {
        if(!acc) return http_raw("401 Unauthorized", "application/json", "{}");
        auto jesc = [](const std::string& in) {
            std::string o = "\"";
            for (unsigned char c : in) {
                if (c=='"' || c=='\\') { o += '\\'; o += char(c); }
                else if (c>=32) o += char(c);
            }
            o += "\"";
            return o;
        };
        std::ostringstream os;
        os << "{\"me\":" << jesc(user) << ",\"stations\":[";
        os << "{\"id\":\"\",\"name\":\"Site-wide\",\"kind\":\"site\"}";
        os << ",{\"id\":" << jesc(std::string("u:")+user) << ",\"name\":\"My Juketube\",\"kind\":\"person\"}";
        for (const auto& c : g_store.communities_of(user)) {
            os << ",{\"id\":" << jesc(c.slug) << ",\"name\":" << jesc(c.name) << ",\"kind\":\"group\"}";
        }
        for (const auto& owner : g_store.jukebox_subscriptions_of(user)) {
            os << ",{\"id\":" << jesc(std::string("u:")+owner) << ",\"name\":" << jesc(owner) << ",\"kind\":\"person\"}";
        }
        os << "],\"friends\":[";
        bool first = true;
        for (const auto& n : g_store.friends_of(user)) {
            if (!first) os << ",";
            first = false;
            os << jesc(n);
        }
        os << "],\"groups\":[";
        first = true;
        for (const auto& c : g_store.communities_of(user)) {
            if (!first) os << ",";
            first = false;
            os << "{\"id\":" << jesc(c.slug) << ",\"name\":" << jesc(c.name) << "}";
        }
        os << "]}";
        return http_raw("200 OK", "application/json", os.str(), "Cache-Control: no-store\r\n");
    }
    if(method=="GET"&&path=="/jukebox/people") {
        if(!acc) return http_raw("401 Unauthorized", "application/json", "{\"items\":[]}");
        auto jesc = [](const std::string& in) {
            std::string o = "\"";
            for (unsigned char c : in) {
                if (c=='"' || c=='\\') { o += '\\'; o += char(c); }
                else if (c>=32) o += char(c);
            }
            o += "\"";
            return o;
        };
        auto term = trim_ws(qs["q"]);
        auto hits = people_hits(user, term);
        std::ostringstream os;
        os << "{\"items\":[";
        bool first = true;
        for (const auto& n : hits) {
            if (n == user || g_store.are_friends(user, n)) continue;
            if (!first) os << ",";
            first = false;
            os << "{\"name\":" << jesc(n) << "}";
        }
        os << "]}";
        return http_raw("200 OK", "application/json", os.str(), "Cache-Control: no-store\r\n");
    }
    if(method=="GET"&&path=="/jukebox/browse") {
        if(!acc) return http_raw("401 Unauthorized", "application/json", "{\"items\":[]}");
        auto category = qs["category"];
        static const std::set<std::string> kCats = {"all","group","friends","others","self"};
        if (!kCats.count(category)) category = "all";
        return http_raw("200 OK", "application/json",
            jukebox_browse_json(user, category, trim_ws(qs["q"])),
            "Cache-Control: no-store\r\n");
    }
    if(method=="GET"&&path=="/jukebox/history") {
        if(!acc) return http_raw("401 Unauthorized", "application/json", "{\"items\":[]}");
        // ?station=<id> (bug fix: the History section never showed a shared station's own
        // history -- the standalone jukebox page always sent `station=`, but this route only
        // ever understood `for=`, so it silently fell back to the viewer's OWN personal
        // plays instead. "" is the site-wide station, a real and distinct value from "no
        // station param at all" -- qs.count, not qs[].empty(), is what tells them apart.
        if (qs.count("station")) {
            std::deque<CommunityTrack> hist;
            {
                std::lock_guard<std::mutex> lock(g_community_mu);
                hist = g_community_history_by_station[qs.at("station")];
            }
            std::string out = "{\"items\":[";
            for (auto it = hist.rbegin(); it != hist.rend(); ++it) {
                if (it != hist.rbegin()) out += ",";
                out += "{\"video_id\":\"" + json_escape(it->video_id) + "\",\"queued_by\":\"" +
                       json_escape(it->queued_by) + "\"}";
            }
            out += "]}";
            return http_raw("200 OK", "application/json", out, "Cache-Control: no-store\r\n");
        }
        // ?for=<personal-owner> scopes to whatever personal channel is
        // actively tuned in on the caller's widget; empty (nothing tuned yet)
        // falls back to the viewer's own plays, same as an owner with no
        // plays of their own yet falls back to nothing rather than erroring.
        auto for_owner = trim_ws(qs["for"]);
        auto hist = (!for_owner.empty() && valid_username(for_owner)) ? g_store.jukebox_history_of(for_owner)
                                                                        : std::vector<JukeboxHistoryEntry>{};
        if (hist.empty()) hist = g_store.jukebox_history_of(user);
        std::reverse(hist.begin(), hist.end());
        if (hist.size() > 15) hist.resize(15);
        std::string out = "{\"items\":[";
        for (std::size_t i = 0; i < hist.size(); ++i) {
            if (i) out += ",";
            out += "{\"video_id\":\"" + json_escape(hist[i].video_id) + "\",\"queued_by\":\"" +
                   json_escape(hist[i].queued_by) + "\"}";
        }
        out += "]}";
        return http_raw("200 OK", "application/json", out, "Cache-Control: no-store\r\n");
    }
    // Title/creator for one video in the coverflow carousel (see
    // jukebox_widget's owner-view build). oEmbed needs no API key at all
    // (unlike the Data API v3 search this file already proxies) -- id is
    // never anything but an already-queued, already-validated 11-char
    // YouTube id, checked again here since it's about to go straight into
    // a shell command via popen.
    if(method=="GET"&&path=="/jukebox/channel") {
        if(!acc) return http_raw("401 Unauthorized", "application/json", "{\"items\":[]}");
        auto cid = trim_ws(qs["id"]);
        if (cid.size() < 10)
            return http_raw("200 OK", "application/json",
                "{\"items\":[],\"error\":\"bad channel\"}", "Cache-Control: no-store\r\n");
        for (char ch : cid) {
            if (!(std::isalnum(static_cast<unsigned char>(ch)) || ch=='_' || ch=='-'))
                return http_raw("200 OK", "application/json",
                    "{\"items\":[],\"error\":\"bad channel\"}", "Cache-Control: no-store\r\n");
        }
        std::string cmd = "timeout 12 python3 /opt/liquidgov/scripts/yt_search.py --channel '" + cid + "' 2>/dev/null";
        std::string resp;
        if (FILE* fp = popen(cmd.c_str(), "r")) {
            char buf[4096]; size_t n;
            while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) resp.append(buf, n);
            pclose(fp);
        }
        if (resp.empty() || resp[0] != '{' || resp.find("\"items\"") == std::string::npos)
            return http_raw("200 OK", "application/json",
                "{\"items\":[],\"error\":\"No videos on that channel.\"}", "Cache-Control: no-store\r\n");
        return http_raw("200 OK", "application/json", resp, "Cache-Control: no-store\r\n");
    }
    if(method=="GET"&&path=="/jukebox/oembed") {
        if(!acc) return http_raw("401 Unauthorized", "application/json", "{}");
        auto id = qs["id"];
        bool valid_id = id.size() == 11;
        if (valid_id) for (unsigned char c : id)
            if (!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-')) { valid_id = false; break; }
        if (!valid_id) return http_raw("400 Bad Request", "application/json", "{}");
        std::string url = "https://www.youtube.com/oembed?format=json&url=" +
            url_encode("https://www.youtube.com/watch?v=" + id);
        std::string resp = curl_get(url, 5);
        // Juketube B (Brandon 2026-11-08): YouTube's oEmbed answers 401 "Unauthorized" when the owner blocks
        // embedding and 404 "Not Found" / 400 "Bad Request" for a removed or private video (curl_get keeps the body),
        // so the picker can say so before the room tries it. Empty or anything else = unknown: "{}" as before.
        if (resp.rfind("Unauthorized", 0) == 0 || resp.rfind("Forbidden", 0) == 0)
            return http_raw("200 OK", "application/json", "{\"embeddable\":false}", "Cache-Control: no-store\r\n");
        if (resp.rfind("Not Found", 0) == 0 || resp.rfind("Bad Request", 0) == 0)
            return http_raw("200 OK", "application/json", "{\"embeddable\":false,\"missing\":true}", "Cache-Control: no-store\r\n");
        if (resp.empty() || resp[0] != '{')
            return http_raw("200 OK", "application/json", "{}", "Cache-Control: no-store\r\n");
        auto title = oembed_json_string_field(resp, "title");
        auto author = oembed_json_string_field(resp, "author_name");
        std::string out = "{\"title\":\""+json_escape(title)+"\",\"author_name\":\""+json_escape(author)+"\",\"embeddable\":true}";
        // A video's title/uploader essentially never changes -- fine to
        // cache in the browser across the session instead of re-curling
        // every time the same track scrolls back into focus.
        return http_raw("200 OK", "application/json", out, "Cache-Control: private, max-age=86400\r\n");
    }
    if(method=="GET"&&path=="/mail/group/new") {
        if(!acc) return need_login();
        return http_redir("/mail");
    }
    // A day of caching (2026-11-08: it had no Cache-Control, so every page fetched it again; measured in headless Edge).
    // The icon only changes with a deploy, and a day-old tab icon is harmless.
    if(method=="GET"&&path=="/favicon.svg")
        return http_raw("200 OK","image/svg+xml", favicon_svg(), "Cache-Control: public, max-age=86400\r\n");
    if(method=="GET"&&path=="/me/unread") {
        if(!acc) return http_raw("401 Unauthorized","application/json","{\"n\":0,\"m\":0}");
        const int mcnt = g_store.unread_message_count(user);
        const int ncnt = unread>mcnt ? unread-mcnt : 0;
        return http_raw("200 OK","application/json",
            "{\"n\":"+std::to_string(ncnt)+",\"m\":"+std::to_string(mcnt)+"}");
    }
    if(method=="GET"&&path=="/sw.js") {
        std::ifstream in("assets/sw.js", std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","text/javascript; charset=utf-8", ss.str(),
            "Service-Worker-Allowed: /\r\nCache-Control: no-store\r\n");
    }
    if(method=="GET"&&path=="/assets/push.js") {
        std::ifstream in("assets/push.js", std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","text/javascript; charset=utf-8", ss.str(),
            asset_cache_header(rawpath));
    }
    if(method=="GET"&&path=="/push/vapid") {
        std::ifstream in("deploy/vapid.public");
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","no vapid");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","text/plain; charset=utf-8", ss.str(),
            "Cache-Control: no-store\r\n");
    }
    if(method=="POST"&&path=="/push/subscribe") {
        if(!acc) return http_code("401 Unauthorized", "");
        auto endpoint = extract_json_string_field(body, "\"endpoint\"");
        auto p256dh = extract_json_string_field(body, "\"p256dh\"");
        auto authk = extract_json_string_field(body, "\"auth\"");
        if(!endpoint || !p256dh || !authk) return http_code("400 Bad Request", "bad json");
        if(!push_subs_upsert(user, trim_ws(*endpoint), trim_ws(*p256dh), trim_ws(*authk)))
            return http_code("400 Bad Request", "bad sub");
        return http_raw("200 OK","application/json","{\"ok\":true}");
    }

    if(method=="GET"&&path=="/manifest.webmanifest") {
        return http_raw("200 OK", "application/manifest+json",
            "{"
            "\"name\":\"LiquidGov.US\","
            "\"short_name\":\"LiquidGov\","
            "\"description\":\"A voluntary natural democracy -- civic association and working software.\","
            "\"start_url\":\"/\","
            "\"display\":\"standalone\","
            "\"background_color\":\"#f7f1e4\","
            "\"theme_color\":\"#8a6620\","
            "\"icons\":[{\"src\":\"/icon-192.png\",\"sizes\":\"192x192\",\"type\":\"image/png\",\"purpose\":\"any\"},{\"src\":\"/icon-512.png\",\"sizes\":\"512x512\",\"type\":\"image/png\",\"purpose\":\"any maskable\"},{\"src\":\"/favicon.svg\",\"sizes\":\"any\",\"type\":\"image/svg+xml\",\"purpose\":\"any\"}]"
            "}");
    }

    // Linked only from a page rendered with title "Messages" (currently
    // just /mail -- see page_wrap's manifest href branch). start_url is
    // /mail, not /, so installing *from that page* on a manifest-reading
    // platform (Chrome/Edge/Android; iOS Safari ignores this file
    // entirely and has no install banner of its own) opens straight back
    // into the inbox instead of the Genesis landing page.
    if(method=="GET"&&path=="/manifest-mail.webmanifest") {
        return http_raw("200 OK", "application/manifest+json",
            "{"
            "\"name\":\"Messages\","
            "\"short_name\":\"Messages\","
            "\"description\":\"LiquidGov.US mail and conversations.\","
            "\"start_url\":\"/mail\","
            "\"display\":\"standalone\","
            "\"background_color\":\"#f7f1e4\","
            "\"theme_color\":\"#d1a85c\","
            "\"icons\":[{\"src\":\"/icon-mail-192.png\",\"sizes\":\"192x192\",\"type\":\"image/png\",\"purpose\":\"any\"},{\"src\":\"/icon-mail-512.png\",\"sizes\":\"512x512\",\"type\":\"image/png\",\"purpose\":\"any maskable\"}]"
            "}");
    }

    // Same pattern as /manifest-mail.webmanifest, linked only from a page rendered with title "Social" or "Civic"
    // (the /app/social and /app/civic install shells, which /apps sends people to). start_url is the real app page,
    // exactly where each shell already sends a standalone launch (see assets/app-social.html / app-civic.html), so an
    // installed Social opens the feed and an installed Civic opens proposals instead of the Genesis landing page.
    // Icons and theme colours are the ones page_wrap already uses for those titles. (iOS ignores manifests.)
    if(method=="GET"&&path=="/manifest-social.webmanifest") {
        return http_raw("200 OK", "application/manifest+json",
            "{"
            "\"name\":\"Social\","
            "\"short_name\":\"Social\","
            "\"description\":\"LiquidGov.US feed, people, and groups.\","
            "\"start_url\":\"/feed\","
            "\"display\":\"standalone\","
            "\"background_color\":\"#f7f1e4\","
            "\"theme_color\":\"#c17a4a\","
            "\"icons\":[{\"src\":\"/icon-social-192.png\",\"sizes\":\"192x192\",\"type\":\"image/png\",\"purpose\":\"any\"},{\"src\":\"/icon-social-512.png\",\"sizes\":\"512x512\",\"type\":\"image/png\",\"purpose\":\"any maskable\"}]"
            "}");
    }
    if(method=="GET"&&path=="/manifest-civic.webmanifest") {
        return http_raw("200 OK", "application/manifest+json",
            "{"
            "\"name\":\"Civic\","
            "\"short_name\":\"Civic\","
            "\"description\":\"LiquidGov.US proposals, laws, and who holds control.\","
            "\"start_url\":\"/proposals\","
            "\"display\":\"standalone\","
            "\"background_color\":\"#f7f1e4\","
            "\"theme_color\":\"#6b7d5a\","
            "\"icons\":[{\"src\":\"/icon-civic-192.png\",\"sizes\":\"192x192\",\"type\":\"image/png\",\"purpose\":\"any\"},{\"src\":\"/icon-civic-512.png\",\"sizes\":\"512x512\",\"type\":\"image/png\",\"purpose\":\"any maskable\"}]"
            "}");
    }

    if(method=="GET"&&(path=="/apple-touch-icon.png"||path=="/apple-touch-icon-precomposed.png"||path=="/icon-192.png"||path=="/icon-512.png"||path=="/assets/icon-192.png"||path=="/assets/icon-512.png"||path=="/assets/apple-touch-icon.png")) {
        std::string file="assets/apple-touch-icon.png";
        if(path.find("192")!=std::string::npos) file="assets/icon-192.png";
        else if(path.find("512")!=std::string::npos) file="assets/icon-512.png";
        std::ifstream in(file, std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","image/png", ss.str(), "Cache-Control: public, max-age=86400\r\n");
    }

    if(method=="GET"&&(path=="/icon-mail.png"||path=="/icon-social.png"||path=="/icon-civic.png"
        ||path=="/icon-mail-192.png"||path=="/icon-social-192.png"||path=="/icon-civic-192.png"
        ||path=="/icon-mail-512.png"||path=="/icon-social-512.png"||path=="/icon-civic-512.png")) {
        std::ifstream in(("assets"+path), std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","image/png", ss.str(), "Cache-Control: public, max-age=86400\r\n");
    }
    if(method=="GET"&&path=="/assets/setup.js") {
        std::ifstream in("assets/setup.js", std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","text/javascript; charset=utf-8", ss.str(), asset_cache_header(rawpath));
    }
    if(method=="GET"&&path=="/assets/setup.css") {
        std::ifstream in("assets/setup.css", std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","text/css; charset=utf-8", ss.str(), asset_cache_header(rawpath));
    }
    if(method=="GET"&&path=="/assets/juketube.js") {
        std::ifstream in("assets/juketube.js", std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","text/javascript; charset=utf-8", ss.str(),
            asset_cache_header(rawpath));
    }
    if(method=="GET"&&path=="/assets/juketube.css") {
        std::ifstream in("assets/juketube.css", std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK","text/css; charset=utf-8", ss.str(),
            asset_cache_header(rawpath));
    }
    if(method=="GET"&&path=="/assets/intro.mp4") {
        static std::string cached;
        static bool tried = false;
        if (!tried) {
            tried = true;
            std::ifstream in("assets/intro.mp4", std::ios::binary);
            if (in) { std::ostringstream ss; ss << in.rdbuf(); cached = ss.str(); }
        }
        if (cached.empty()) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        const std::string cache_hdr = "Cache-Control: public, max-age=31536000, immutable\r\nAccept-Ranges: bytes\r\n";
        // Real browsers request video with a Range header even on first
        // play; answering 200-whole-body every time (no 206 support) is
        // a genuine small smoothness cost, independent of anything else.
        // Falls straight through to the full 200 response on anything
        // malformed or out of bounds -- never worth 500ing a video load.
        const std::string range = header_value(req, "Range");
        if (range.rfind("bytes=", 0) == 0) {
            auto dash = range.find('-', 6);
            if (dash != std::string::npos) {
                try {
                    std::string start_s = range.substr(6, dash - 6);
                    std::string end_s = range.substr(dash + 1);
                    std::size_t start = start_s.empty() ? 0 : std::stoul(start_s);
                    std::size_t end = end_s.empty() ? cached.size() - 1 : std::stoul(end_s);
                    if (start < cached.size() && end < cached.size() && start <= end) {
                        std::string slice = cached.substr(start, end - start + 1);
                        std::ostringstream extra;
                        extra << cache_hdr << "Content-Range: bytes " << start << "-" << end << "/" << cached.size() << "\r\n";
                        return http_raw("206 Partial Content", "video/mp4", slice, extra.str());
                    }
                } catch (...) { /* fall through */ }
            }
        }
        return http_raw("200 OK", "video/mp4", cached, cache_hdr);
    }
    if(method=="GET"&&path=="/assets/og-image-v4.jpg") {
        static std::string cached;
        static bool tried = false;
        if (!tried) {
            tried = true;
            std::ifstream in("assets/og-image-v4.jpg", std::ios::binary);
            if (in) { std::ostringstream ss; ss << in.rdbuf(); cached = ss.str(); }
        }
        if (cached.empty()) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        return http_raw("200 OK", "image/jpeg", cached,
            "Cache-Control: public, max-age=31536000, immutable\r\n");
    }
    // v3, kept so an old cached share still resolves. The tags now point at
    // og-image-v4.jpg (page_wrap).
    if(method=="GET"&&path=="/assets/og-image-v3.jpg") {
        static std::string cached;
        static bool tried = false;
        if (!tried) {
            tried = true;
            std::ifstream in("assets/og-image-v3.jpg", std::ios::binary);
            if (in) { std::ostringstream ss; ss << in.rdbuf(); cached = ss.str(); }
        }
        if (cached.empty()) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        return http_raw("200 OK", "image/jpeg", cached,
            "Cache-Control: public, max-age=31536000, immutable\r\n");
    }
    if(method=="GET"&&path=="/assets/og-image-v2.jpg") {
        static std::string cached;
        static bool tried = false;
        if (!tried) {
            tried = true;
            std::ifstream in("assets/og-image-v2.jpg", std::ios::binary);
            if (in) { std::ostringstream ss; ss << in.rdbuf(); cached = ss.str(); }
        }
        if (cached.empty()) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        return http_raw("200 OK", "image/jpeg", cached,
            "Cache-Control: public, max-age=31536000, immutable\r\n");
    }
    // v1, kept so an old cached share of /assets/og-image.png still resolves.
    // The tags now point at og-image-v4.jpg (page_wrap).
    if(method=="GET"&&path=="/assets/og-image.png") {
        static std::string cached;
        static bool tried = false;
        if (!tried) {
            tried = true;
            std::ifstream in("assets/og-image.png", std::ios::binary);
            if (in) { std::ostringstream ss; ss << in.rdbuf(); cached = ss.str(); }
        }
        if (cached.empty()) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        return http_raw("200 OK", "image/png", cached,
            "Cache-Control: public, max-age=31536000, immutable\r\n");
    }
    // Real recorded narration (Helios) for the Chapter 1 Listen control --
    // replaces the earlier browser speechSynthesis approach entirely.
    // Narration for every Laws chapter (Helios, recorded 2026-09-13; Brandon 2026-11-09 "i want the audio"):
    // /assets/chapter<N>.mp3 for N = 1..number of chapters. Chapter 1 used to be the only route (a whole-file copy
    // held in memory for good); all 14 that way would pin ~18 MB on the 512 MB droplet, so each request reads the
    // file from disk. Byte ranges are honoured (iPhone Safari asks for them before playing audio). Cached a day,
    // not a year: a chapter whose text changes gets re-recorded at the same address.
    if(method=="GET"&&path.rfind("/assets/chapter",0)==0&&path.size()>19&&path.compare(path.size()-4,4,".mp3")==0) {
        const std::string num = path.substr(15, path.size()-15-4);
        size_t n = 0;
        if (num.empty() || num.size() > 2 || num[0] == '0' || !std::all_of(num.begin(), num.end(), ::isdigit))
            return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        n = static_cast<size_t>(std::stoi(num));
        if (n < 1 || n > law_chapters().size()) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        // the name is built, so lgp1_consistency (literal asset names) cannot see it; narration_files_check.py checks
        // that assets/chapter1..N.mp3 exist for every Laws chapter instead
        const std::string file = "assets/chapter" + num + ".mp3";
        std::ifstream in(file, std::ios::binary);
        if (!in) {
            lglog::emit("narration", "missing assets/chapter" + num + ".mp3");
            return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        }
        std::ostringstream ss; ss << in.rdbuf();
        const std::string all = ss.str();
        const std::string cache = "Cache-Control: public, max-age=86400\r\nAccept-Ranges: bytes\r\n";
        const std::string range = header_value(req, "Range");
        unsigned long long a = 0, b = 0;
        if (!range.empty() && !all.empty() && std::sscanf(range.c_str(), "bytes=%llu-%llu", &a, &b) >= 1) {
            if (range.find('-') == range.size() - 1 || b >= all.size()) b = all.size() - 1;
            if (a > b || a >= all.size())
                return http_raw("416 Range Not Satisfiable", "text/plain; charset=utf-8", "",
                    "Content-Range: bytes */" + std::to_string(all.size()) + "\r\n");
            return http_raw("206 Partial Content", "audio/mpeg", all.substr(a, b - a + 1), cache +
                "Content-Range: bytes " + std::to_string(a) + "-" + std::to_string(b) + "/" + std::to_string(all.size()) + "\r\n");
        }
        return http_raw("200 OK", "audio/mpeg", all, cache);
    }

    if(method=="GET"&&(path=="/theme/dark"||path=="/theme/light")) {
        const std::string mode = path.substr(7);
        std::string back = header_value(req, "Referer");
        if (back.empty() || back.rfind("http",0)!=0) back = "/";
        std::string extra = "Set-Cookie: lg_theme="+mode+"; Path=/; SameSite=Lax; Max-Age=31536000";
        if (g_https) extra += "; Secure";
        extra += "\r\n";
        return http_redir(back, extra);
    }

    // The old signed-in landing page (counts, your rooms), still one tap away from the map (2026-11-07).
    if(method=="GET"&&path=="/home") {
        if(!acc) return need_login();
        return http_ok(wrap("Home", user, unread, home_dashboard(user)+
                "<div class=\"card\"><h2>What this is</h2>"
                "<p class=\"muted\">Genesis, not a government. Three rules, and the rest is up to you.</p>"
                "<p class=\"muted\"><a href=\"/laws\">Read the disclosure</a></p></div>"));
    }
    if(method=="GET"&&path=="/") {
        if(acc) {
            // A returning member's "landing page" is here, not the
            // logged-out Genesis page below -- so that's where the opted-in
            // replay actually has to happen for the Settings toggle to mean
            // anything for someone who can reach Settings at all. Bypasses
            // pg() (which never shows the intro) specifically for this;
            // show_intro is otherwise only ever true for the Genesis path.
            const bool replay_intro = g_store.prefs_of(user).intro_every_visit;
            return http_ok(wrap("MAINstream", user, unread, map_page_body(user, ""), replay_intro));
        }
        std::string extra =
            "<div class=\"card\"><p class=\"row\"><a class=\"btn\" href=\"/signup\">Create an account</a> "
            "<a class=\"btn ghost\" href=\"/login\">Log in</a></p></div>";
        return http_ok(wrap("Home", user, unread, genesis_body()+extra, true));
    }
    if(method=="GET"&&path=="/laws") return pg("Laws", laws_body());
    if(method=="GET"&&path.rfind("/laws/",0)==0) {
        size_t n = 0;
        try { n = std::stoull(path.substr(6)); } catch (...) {}
        const auto& chapters = law_chapters();
        if (n < 1 || n > chapters.size())
            return http_code("404 Not Found", pg("Not found", "<div class=\"card\"><p>No such chapter.</p></div>"));
        return pg("Laws · " + chapters[n-1].title, laws_chapter_body(n));
    }
    if(method=="GET"&&path=="/control") return pg("Who holds control", control_body());
    if(method=="GET"&&path=="/privacy") return pg("Privacy", privacy_body());
    if(method=="GET"&&path=="/terms") return pg("How this association works", terms_body());
    if(method=="GET"&&path=="/about") return http_redir("/control");
    if(method=="GET"&&path=="/profile") return http_redir(user.empty() ? login_url("/me") : "/me");
    if(method=="GET"&&(path=="/install"||path=="/apps")) {
        std::ifstream in("assets/apps.html"); std::ostringstream body; if(in) body<<in.rdbuf();
        return pg("Get the app", body.str());
    }
    // The separate Messages/Social/Civic apps are gone (one app now). Their old
    // install pages send people to the real page those apps opened.
    if(method=="GET"&&path=="/app/mail") return http_redir("/mail");
    if(method=="GET"&&path=="/app/social") return http_redir("/feed");
    if(method=="GET"&&path=="/app/civic") return http_redir("/proposals");
    if(method=="GET"&&path=="/signup") return http_ok(signup_page(user, unread, "", SignupDraft{}));
    if(method=="GET"&&path=="/login") {
        // Deliberately one generic message for bad-username and
        // bad-password alike (and phone-not-verified folded in too) --
        // splitting them would let a caller enumerate which usernames
        // exist. Rate-limit is the one case safe to name specifically:
        // it doesn't depend on whether the account exists.
        const std::string err = flash_error(qs);
        const std::string next = safe_next_path(qs["next"]);
        return pg("Log in", "<div class=\"card\"><h1>Log in</h1>"
            "<p class=\"muted\">Log in with your username and password.</p>"+err+
            google_signin_button_html()+
            "<form method=\"POST\" action=\"/login\"><label>Username</label>"
            "<input name=\"username\" required autocomplete=\"username\" value=\""+html_escape(qs["username"])+"\">"
            "<label>Password</label><input type=\"password\" name=\"password\" required autocomplete=\"current-password\">"
            +(next.empty()?std::string():"<input type=\"hidden\" name=\"next\" value=\""+html_escape(next)+"\">")+
            "<button>Continue</button></form>"
            "<p class=\"muted\" style=\"margin-top:.7rem\">"
            "<a href=\"/forgot\">Forgot password?</a> &middot; "
            "New here? <a href=\"/signup\">Create an account</a>.</p></div>");
    }
    if(method=="GET"&&path=="/forgot") {
        return pg("Forgot password", "<div class=\"card\"><h1>Forgot your password?</h1>"
            "<p class=\"muted\">Used Google? <a href=\"/login\">Sign in with Google</a> instead "
            "&mdash; no reset needed.</p>"
            +flash_error(qs)+
            "<form method=\"POST\" action=\"/forgot\">"
            "<label>Email</label><input type=\"email\" name=\"email\" required autocomplete=\"email\">"
            "<button>Send reset link</button></form>"
            "<p class=\"muted\" style=\"margin-top:.7rem\"><a href=\"/login\">Back to log in</a></p></div>");
    }
    // GET renders the form regardless of whether t looks valid -- POST
    // does the actual checking (see its own comment), so this route
    // can't be used to probe token validity separately from consuming
    // one.
    if(method=="GET"&&path=="/reset") {
        return pg("Reset password", "<div class=\"card\"><h1>Set a new password</h1>"
            +flash_error(qs)+
            "<form method=\"POST\" action=\"/reset\">"
            "<input type=\"hidden\" name=\"t\" value=\""+html_escape(qs["t"])+"\">"
            "<label>New password</label><input type=\"password\" name=\"password\" required "
            "minlength=\"8\" autocomplete=\"new-password\">"
            "<button>Set password</button></form></div>");
    }
    if(method=="GET"&&path=="/logout") {
        logout(g_store, sid);
        return http_redir("/", clear_cookie());
    }

    if(method=="GET"&&path=="/world") {
        // Old Quad bookmarks land on the closed Commons page.
        return http_redir("/commons/");
    }
    if(method=="GET"&&path=="/me/canvas") {
        if(!acc) return need_login();
        return pg("Canvas", canvas_tool_page(user, flash_error(qs), false));
    }
    if(method=="GET"&&path=="/me/portrait") {
        if(!acc) return need_login();
        return pg("Self-portrait", canvas_tool_page(user, flash_error(qs), true));
    }
    if(method=="GET"&&path=="/me/post/canvas") {
        if(!acc) return need_login();
        return pg("Compose a post", canvas_tool_page(user, "", false, "/me/post"));
    }
    if(method=="GET"&&path=="/feed/canvas") {
        if(!acc) return need_login();
        return pg("Compose a post", canvas_tool_page(user, "", false, "/feed"));
    }
    if(method=="GET"&&path=="/listen") {
        // Deliberately not wrap()/pg(): this window IS the player, full-page
        // nav chrome and a second (redundant) floating-widget toggle would
        // just be noise in a small popout.
        if(!acc) return need_login();
        return http_ok(page_wrap("Listen", "",
            "<div style=\"padding:.75rem\">" + jukebox_standalone_page(youtube_api_key_from_env(), user) + "</div>"));
    }
    if(method=="GET"&&path=="/feed") {
        if(!acc) return need_login();
        std::string filter_key = qs.count("filter") ? qs.at("filter") : "all";
        FeedFilter filter = FeedFilter::All;
        if (filter_key == "friends") filter = FeedFilter::Friends;
        else if (filter_key == "committees") filter = FeedFilter::Committees;
        else filter_key = "all"; // anything else, an old ?filter=drawings link included, is All (no Drawings, Brandon 2026-11-10)
        auto tab = [&](const char* key, const char* label) {
            const bool active = filter_key == key;
            return "<a class=\"btn"+std::string(active?"\" aria-current=\"page":" ghost")+"\" href=\"/feed?filter="+key+"\">"+label+"</a>";
        };
        std::string html = "<h1>Feed</h1><p class=\"muted eyebrow\">"+html_escape(fmt_time(now()))+
            " &middot; "+std::to_string(g_store.account_count())+" members</p>"
            "<div class=\"segtabs\">"+
            tab("all","All")+tab("friends","Friends")+tab("committees","Groups")+
            "</div>"+
            flash_error(qs)+
            feed_composer_card(user);
        auto posts = g_store.feed_view(user, 80, filter);
        if (posts.empty()) html += "<div class=\"card\"><p class=\"empty\">Nothing in this view yet.</p>"
            "<p class=\"muted\">Write one above, or <a href=\"/friends\">add a friend</a> so theirs show up.</p></div>";
        for (const auto& p : posts) html += "<div class=\"card\">"+post_block(p, user)+"</div>";
        return pg("Feed", html);
    }

    if(method=="GET"&&path=="/proposals") {
        if(!acc) return need_login();
        // /proposals?id=N: one proposal on its own (the map's right-hand field
        // opens this), with its vote, reactions and comments.
        if (qs.count("id")) {
            std::uint64_t pid = 0;
            try { pid = std::stoull(qs.at("id")); } catch (...) { pid = 0; }
            auto one = pid ? g_store.proposal_by_id(pid) : std::nullopt;
            if (!one) return http_code("404 Not Found", wrap("Not found", user, unread,
                "<div class=\"card\"><p>No such proposal.</p><p><a href=\"/proposals\">Proposals</a></p></div>"));
            // A race is shown and voted on /officials only (Brief 7 item 1: one object, one page).
            if (one->is_office) return http_redir("/officials#proposal-" + std::to_string(one->id));
            const std::string self = "/proposals?id=" + std::to_string(one->id);
            return pg(one->title, "<p class=\"muted\"><a href=\"/proposals\">All proposals</a></p>"
                      "<div class=\"card\">" + proposal_block(*one, user, self) + "</div>");
        }
        std::string filter_key;
        if (qs.count("city")) {
            const std::string raw = qs.at("city");
            const std::string key = city_key(raw);
            if (!raw.empty() && key.empty()) return http_redir("/proposals");
            if (!key.empty() && key != raw) return http_redir("/proposals?city="+url_encode(key));
            filter_key = key;
        }
        std::string city_value;
        if (!filter_key.empty()) {
            std::string name = filter_key;
            for (const auto& cu : g_store.cities_in_use()) if (cu.key == filter_key) { name = cu.name; break; }
            city_value = " value=\""+html_escape(name)+"\"";
        }
        const std::string list_next = filter_key.empty() ? std::string("/proposals")
            : "/proposals?city="+url_encode(filter_key);
        std::string tag_fields;
        if (!filter_key.empty()) {
            for (const auto& tag : g_store.city_tags(filter_key)) {
                tag_fields += "<label class=\"row\" style=\"gap:.4rem;align-items:center\">"
                              "<input type=\"checkbox\" name=\"tag\" value=\"" + html_escape(tag) + "\"> "
                              + html_escape(tag) + "</label>";
            }
            if (!tag_fields.empty())
                tag_fields = "<label>Departments and councils</label>" + tag_fields;
        }
        std::string html = "<h1>Proposals</h1>"
            // Brief 7 item 2 (PROPOSALS-AUDIT section 4): how to use the page, in one line. The Phase 2 link to /laws/13 sent
            // members into charter text; it stays in Laws and on /control.
            + how_proposals_work_html();
        html += "<p class=\"muted\"><a href=\"/officials\">Races for office</a> are on their own page.</p>";
        if (!filter_key.empty()) {
            std::string name = filter_key;
            for (const auto& cu : g_store.cities_in_use()) if (cu.key == filter_key) { name = cu.name; break; }
            html += "<p class=\"muted\">Showing proposals for <a href=\"/city/"+url_encode(filter_key)+"\">"
                    +html_escape(name)+"</a>. <a href=\"/proposals\">All proposals</a></p>";
        }
        html += flash_error(qs);
        // ?title= prefills the title ("Start a proposal about this" from /problems). Never posts by itself.
        std::string title_prefill = qs.count("title") ? trim_ws(qs.at("title")) : std::string();
        if (title_prefill.size() > 140) title_prefill.resize(140);
        auto all = filter_key.empty() ? g_store.all_proposals() : g_store.proposals_in_city(filter_key);
        std::reverse(all.begin(), all.end());
        // Races for office are listed on /officials only, not twice (Brief 7 item 1); a link below says where they are.
        all.erase(std::remove_if(all.begin(), all.end(), [](const Proposal& x) { return x.is_office; }), all.end());
        // The proposals first, the form behind a "New proposal" button (sweep 2026-11-09: on a phone the page opened
        // on an empty form and the proposals began a full screen down). Open on its own when it is what you came for:
        // a ?title= prefill, an error to fix, nothing to read yet, or a link to #new (the script below).
        const bool open_form = !title_prefill.empty() || qs.count("err") || all.empty();
        html += std::string("<details class=\"card newprop\" id=\"new\"") + (open_form ? " open" : "") +
                "><summary>New proposal</summary><form method=\"POST\" action=\"/proposals\">"
                "<label>Title</label><input name=\"title\" maxlength=\"140\" required value=\""+html_escape(title_prefill)+"\">"
                "<label>What's being proposed</label>"
                "<textarea name=\"body\" maxlength=\"4000\" required placeholder=\"Plain text.\"></textarea>"
                "<label>How should people answer?</label>"
                "<select name=\"kind\" id=\"bkind\">"
                "<option value=\"0\">Yes, no, or skip</option>"
                "<option value=\"1\">A number</option>"
                "<option value=\"2\">A date</option>"
                "<option value=\"3\">Pick one option</option>"
                "<option value=\"4\">A scale (0–100)</option>"
                "</select>"
                "<div id=\"bnum\" hidden>"
                "<label>Unit (optional)</label><input name=\"unit\" maxlength=\"24\" placeholder=\"days, dollars, people\">"
                "<label>Minimum (optional)</label><input name=\"min\" type=\"number\" step=\"any\">"
                "<label>Maximum (optional)</label><input name=\"max\" type=\"number\" step=\"any\">"
                "</div>"
                "<div id=\"bchoice\" hidden>"
                "<label>Options (one per line)</label>"
                "<textarea name=\"options\" maxlength=\"800\" placeholder=\"One option per line\"></textarea>"
                "<label class=\"row\" style=\"gap:.4rem;align-items:center;margin-top:.4rem\">"
                "<input type=\"checkbox\" name=\"open\" value=\"1\"> "
                "Let people add another option <span class=\"muted\">(one each, besides you)</span></label>"
                "</div>"
                "<div id=\"bscale\" hidden>"
                "<label>Left label</label><input name=\"left\" maxlength=\"40\" placeholder=\"Too little\">"
                "<label>Right label</label><input name=\"right\" maxlength=\"40\" placeholder=\"Too much\">"
                "</div>"
                "<label>City (optional)</label>"
                "<input name=\"city\" list=\"citylist\" maxlength=\"80\" autocomplete=\"off\" placeholder=\"Fresno, CA\""
                + city_value + ">"
                "<label>Who could act on this? <span class=\"muted\">(optional, for example &ldquo;The city council&rdquo;)</span></label>"
                "<input name=\"who\" maxlength=\"80\" placeholder=\"The city council\">"
                + tag_fields +
                "<button>Publish</button></form>"
                + city_datalist_html() +
                "<script>"
                "(function(){var s=document.getElementById('bkind');if(!s)return;"
                "function show(){var v=s.value;"
                "document.getElementById('bnum').hidden=(v!=='1');"
                "document.getElementById('bchoice').hidden=(v!=='3');"
                "document.getElementById('bscale').hidden=(v!=='4');}"
                "s.addEventListener('change',show);show();})();"
                "(function(){var d=document.getElementById('new');if(d&&location.hash==='#new')d.open=true;})();"
                "</script></details>";
        if (all.empty()) html += "<div class=\"card\"><p class=\"empty\">None open.</p>"
            "<p class=\"muted\">The form above starts an unofficial straw poll. Not a binding vote.</p></div>";
        for (const auto& p : all) {
            html += "<div class=\"card\">"+proposal_block(p, user, list_next)+"</div>";
        }
        return pg("Proposals", html);
    }

    if(method=="GET"&&path.rfind("/portrait/",0)==0) {
        const std::string who = path.substr(10);
        if (!valid_username(who)) return http_code("404 Not Found", "not found");
        auto found = g_store.find_by_username(who);
        if (!found) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        if (acc && g_store.is_blocked(user, found->username))
            return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        auto av = g_store.get(found->id);
        if (!av) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        auto bytes = g_blobs.get(av->blob_id);
        if (!bytes) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        std::string png(bytes->begin(), bytes->end());
        return http_raw("200 OK", "image/png", png,
            "Cache-Control: private, max-age=300\r\nContent-Disposition: inline; filename=\"portrait.png\"\r\n");
    }

    // /canvas-frame/<username>/latest -- the newest saved snapshot, for the
    // commons wall slot (glue.md "Wall-slot bitmap"). A separate path token,
    // registered ahead of the index route below on purpose: that handler
    // std::stoi()s the last segment, so "latest" must never reach it (it
    // would 404 today, and any looser parse would read as frame 0 -- the
    // first stroke, often near-blank). Same checks as the index route
    // (unknown user, the viewer's own block check -- anonymous skips it --
    // no frames, missing blob); every one is a plain 404, which the game
    // reads as "empty wall slot", not a broken image. Read-only.
    if(method=="GET"&&path.rfind("/canvas-frame/",0)==0&&path.size()>21&&
       path.compare(path.size()-7,7,"/latest")==0) {
        const std::string who = path.substr(14, path.size()-14-7);
        std::string body404 = "not found";
        if (!valid_username(who)) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        auto found = g_store.find_by_username(who);
        if (!found) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        if (acc && g_store.is_blocked(user, found->username))
            return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        auto frames = g_store.canvas_frames(found->id);
        if (frames.empty()) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        auto bytes = g_blobs.get(frames.back());
        if (!bytes) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        std::string png(bytes->begin(), bytes->end());
        return http_raw("200 OK", "image/png", png,
            "Cache-Control: private, max-age=300\r\nContent-Disposition: inline; filename=\"frame.png\"\r\n");
    }

    // /canvas-frame/<username>/<index> -- one saved drawing snapshot.
    // Requested by the profile page's own background-image CSS
    // (server-generated, not user CSS -- see profile_canvas_css), and
    // by /me/canvas itself to reload a saved drawing back onto the
    // editing layer -- public either way, same as /portrait/.
    if(method=="GET"&&path.rfind("/canvas-frame/",0)==0) {
        auto rest = path.substr(14);
        auto slash = rest.find('/');
        std::string body404 = "not found";
        if (slash == std::string::npos) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        const std::string who = rest.substr(0, slash);
        int idx = -1;
        try { idx = std::stoi(rest.substr(slash + 1)); } catch (...) { idx = -1; }
        if (!valid_username(who) || idx < 0) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        auto found = g_store.find_by_username(who);
        if (!found) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        if (acc && g_store.is_blocked(user, found->username))
            return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        auto frames = g_store.canvas_frames(found->id);
        if (static_cast<std::size_t>(idx) >= frames.size()) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        auto bytes = g_blobs.get(frames[static_cast<std::size_t>(idx)]);
        if (!bytes) return http_raw("404 Not Found", "text/plain; charset=utf-8", body404);
        std::string png(bytes->begin(), bytes->end());
        return http_raw("200 OK", "image/png", png,
            "Cache-Control: private, max-age=300\r\nContent-Disposition: inline; filename=\"frame.png\"\r\n");
    }

    // /post-media/<blob_id> -- an image or video attached to a post.
    // Public and unauthenticated, same posture as /portrait/ and
    // /canvas-frame/: a post's own visibility is already decided by
    // wherever it renders (feed, group, profile), and the blob id itself
    // is a content hash, not a secret. blob_id comes straight from the
    // URL, though, so it's validated as a bare hex sha256 digest before
    // ever reaching BlobStore -- FilesystemBlobStore::path_for()
    // concatenates the id directly into a filesystem path with no
    // sanitization of its own, so anything looser here would be a real
    // path-traversal hole, not just a style nit.
    if(method=="GET"&&path.rfind("/post-media/",0)==0) {
        const std::string blob_id = path.substr(12);
        bool valid_hex = blob_id.size() == 64;
        if (valid_hex) {
            for (unsigned char c : blob_id) {
                if (!std::isxdigit(c)) { valid_hex = false; break; }
            }
        }
        if (!valid_hex) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        auto meta = g_blobs.meta(blob_id);
        auto bytes = g_blobs.get(blob_id);
        if (!meta || !bytes) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        std::string data(bytes->begin(), bytes->end());
        return http_raw("200 OK", meta->mime_type.c_str(), data,
            "Cache-Control: private, max-age=300\r\nContent-Disposition: inline\r\n");
    }

    if(method=="GET"&&path=="/me") {
        if(!acc) return need_login();
        std::string err = flash_error(qs);
        std::string html;
        // No Publish/Discard bar any more (Brandon 2026-11-11 4:50 PM, Brief 4 item 11): every form[data-draft] here
        // saves itself, see draft_publish_script.
        html += "<div class=\"card\"><div class=\"who\">"+portrait_tag(user,"")+"<div><h1>"+html_escape(user)+"</h1>"
            "<p class=\"muted\">Platform profile · <a href=\"/u/"+html_escape(user)+"\">public page</a> · member since "+
            html_escape(fmt_time(acc->created_at))+"</p></div></div>"+err+
            "<p class=\"muted\">Self-portrait &mdash; draw one with the same tools as the "
            "<a href=\"/me/portrait\">portrait tool</a>.</p>";
        html += "<p class=\"muted\">A platform account is not a citizen identity.</p></div>";
        {
            auto prof = g_store.profile_of(user);
            auto bio = g_store.blurb(acc->id);
            html += "<div class=\"card\"><h2>About you</h2>"
                "<form method=\"POST\" action=\"/me/profile\" data-draft=\"1\">"
                "<label>Display name</label><input name=\"display_name\" required maxlength=\"80\" autocomplete=\"nickname\" value=\""+html_escape(prof.display_name)+"\">"
                "<label>Hometown</label><input name=\"hometown\" maxlength=\"80\" value=\""+html_escape(prof.hometown)+"\">"
                "<label>Where you live now</label><input name=\"living\" maxlength=\"80\" value=\""+html_escape(prof.living)+"\">"
                "<label>Bio</label><textarea name=\"bio\" maxlength=\"500\">"+html_escape(bio)+"</textarea>"
                "<label>Interests</label><input name=\"interests\" maxlength=\"200\" placeholder=\"comma-separated\" value=\""+html_escape(prof.interests)+"\">"
                "<label>Career</label><input name=\"career\" maxlength=\"80\" value=\""+html_escape(prof.career)+"\">"
                "<label>School</label><input name=\"school\" maxlength=\"80\" value=\""+html_escape(prof.school)+"\">"
                "</form>"
                "<p class=\"muted\">Email stays a fingerprint from signup. It is not shown and cannot be changed here.</p></div>";
        }
        {
            auto n_frames = g_store.canvas_frames(acc->id).size();
            std::string canvas_status = n_frames == 0
                ? "No drawing yet &mdash; your profile uses the default background."
                : (std::to_string(n_frames) + " frame" + (n_frames==1?"":"s") + " saved, "
                   + (g_store.canvas_timelapse(acc->id) ? "looping as your background." : "last one shown as your background."));
            html += "<div class=\"card\"><h2>Canvas</h2>"
                "<p class=\"muted\">"+canvas_status+"</p>"
                "<p class=\"row\"><a class=\"btn\" href=\"/me/canvas\">Open the canvas</a></p></div>";
        }
        {
            auto current_css = g_store.author_css(acc->id);
            std::string css_msg;
            // The page-top banner (see `err` above) already covers ?err=css
            // -- this is just the success case, which has no other home.
            if (qs["ok"]=="css") css_msg = "<p class=\"ok\">Saved. See it at <a href=\"/u/"+html_escape(user)+"\">your public profile</a>.</p>";
            html += "<div class=\"card\"><h2>Profile CSS</h2>"
                "<p class=\"muted\">Restyles your public profile page (/u/"+html_escape(user)+
                "), shown inside a sandboxed frame that can never run scripts. "
                "@import, url(), and anything script-adjacent are rejected outright, not silently stripped. "
                "Limit " + std::to_string(kAuthorCssMaxBytes/1000) + "KB. Leave empty to use the default look.</p>"
                + css_msg +
                "<form method=\"POST\" action=\"/me/css\" data-draft=\"1\">"
                "<textarea name=\"css\" rows=\"8\" spellcheck=\"false\" "
                // 16px, not .85rem: under 16px iOS Safari zooms the page in on focus and leaves it zoomed (2026-11-07)
                "style=\"font-family:ui-monospace,Consolas,monospace;font-size:16px\" "
                "placeholder=\".lg-theme h1{color:#a23b3b}\">"+html_escape(current_css)+"</textarea>"
                "</form></div>";
        }
        html += jukebox_widget(user, true, user, qs);
        {
            // Off by default -- visiting a profile shouldn't ambush anyone
            // with sound. "Play this Juketube instead" is stored as-is;
            // an invalid value (self, a stranger who blocked you, a cycle)
            // is never rejected here, just quietly inert at read time (see
            // jukebox_effective_owner) -- same posture as findability
            // elsewhere: a miss looks like a miss, not an error message.
            std::string jmsg = qs["ok"]=="jplay" ? "<p class=\"ok\">Saved.</p>" : "";
            html += "<div class=\"card\"><h2>Play settings</h2>"+jmsg+
                "<form method=\"POST\" action=\"/me/jukebox/play-settings\" data-draft=\"1\">"
                "<label><input type=\"checkbox\" name=\"autoplay\" value=\"1\""+
                (g_store.jukebox_autoplay(user) ? " checked" : "")+
                "> Play my Juketube when someone opens my profile</label>"
                "<label style=\"display:block;margin-top:.5rem\">Play this Juketube instead</label>"
                "<input name=\"relay\" placeholder=\"another person&rsquo;s Juketube, or empty for your own\" autocomplete=\"off\" "
                "value=\""+html_escape(g_store.jukebox_relay_target(user))+"\">"
                "</form></div>";
        }
        html += draft_publish_script();
        return pg("Profile", html);
    }

    // Checked before the general /u/ handler below, since that one treats
    // everything after "/u/" as a bare username.
    if(method=="GET"&&path.rfind("/u/",0)==0&&path.size()>7&&path.compare(path.size()-6,6,"/theme")==0) {
        const std::string who = path.substr(3, path.size()-3-6);
        std::string body;
        if (!valid_username(who)) {
            body = "<!doctype html><html><head><meta charset=\"utf-8\"></head>"
                   "<body><p>No such profile.</p></body></html>";
        } else {
            auto found = g_store.find_by_username(who);
            if (!found) {
                body = "<!doctype html><html><head><meta charset=\"utf-8\"></head>"
                       "<body><p>No such profile.</p></body></html>";
            } else {
                auto author_css = g_store.author_css(found->id);
                // Deliberately not wrap()/page_wrap(): no nav, no jukebox
                // widget, no session cookie context, no scripts at all --
                // this document only ever renders inside the sandboxed,
                // scriptless iframe below. See sanitize_author_css's
                // comment for what's actually defended here and why a gap
                // in the text filter still can't reach anything.
                // No canvas background here anymore -- a profile-background
                // drawing renders directly on the real /u/<username> page
                // now (see profile_canvas_css), not through this document.
                // This route/iframe exists purely for a custom .lg-theme
                // author_css, so it's only ever reached when that's set.
                body = "<!doctype html><html><head><meta charset=\"utf-8\">"
                       "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                       "<style>body{margin:0;padding:1rem;font:16px/1.5 system-ui,sans-serif;"
                       "background:#fff;color:#1a1814}h1{margin:.2rem 0}"
                       "p{white-space:pre-wrap}</style>"
                       "<style>" + author_css + "</style></head>"
                       "<body><div class=\"lg-theme\"><h1>" + html_escape(found->username) + "</h1>"
                       "</div></body></html>";
            }
        }
        return http_raw("200 OK", "text/html; charset=utf-8", body);
    }
    // Polled by the floating widget every few seconds while a personal
    // channel is the active station (see pollPersonal in jukebox_script) --
    // the widget's substitute for the WebSocket push the site-wide/
    // community stations get, since a personal jukebox is an ordinary HTTP
    // queue with no live sync (jukebox_widget's own comment). Shape matches
    // the community WS snapshot exactly so the client can share one handler.
    // Must come before the generic GET /u/<username> handler just below --
    // that one only expects a bare username after "/u/" and would otherwise
    // 404 this as an invalid username before this more specific route ever
    // gets a look (caught live: fetching this returned the generic
    // "Not found" page instead of JSON).
    if(method=="GET"&&path.rfind("/u/",0)==0&&path.size()>15&&
       path.compare(path.size()-12,12,"/jukebox/now")==0) {
        const std::string who = path.substr(3, path.size()-3-12);
        if (!valid_username(who)) return http_code("404 Not Found", "");
        auto found_now = g_store.find_by_username(who);
        if (!found_now) return http_code("404 Not Found", "");
        if (acc && g_store.is_blocked(who, user)) return http_code("404 Not Found", "");
        auto target_prefs = g_store.prefs_of(who);
        const bool is_owner = acc && user == who;
        const bool is_friend = acc && !is_owner && g_store.are_friends(user, who);
        if (!visible_to(target_prefs.profile_visibility, is_owner, acc.has_value(), is_friend))
            return http_code("404 Not Found", "");
        // "Play this Juketube instead" (see /me): who's page this is and
        // whose queue actually plays here can differ, one hop only.
        const std::string effective_owner = g_store.jukebox_effective_owner(who);
        auto tracks = g_store.jukebox_of(effective_owner);
        std::ostringstream os;
        os << "{";
        if (!tracks.empty()) {
            const auto& t = tracks.front();
            os << "\"now\":{\"video_id\":\"" << t.video_id << "\",\"queued_by\":\"" << t.queued_by
               << "\",\"elapsed\":" << personal_jukebox_elapsed("u:"+effective_owner, t.video_id) << "}";
        } else {
            os << "\"now\":null";
        }
        os << ",\"upcoming\":" << (tracks.empty() ? 0 : tracks.size() - 1);
        os << ",\"queue\":[";
        {
            bool qfirst = true;
            for (size_t qi = 1; qi < tracks.size(); ++qi) {
                if (!qfirst) os << ",";
                qfirst = false;
                os << "{\"video_id\":\"" << tracks[qi].video_id << "\",\"queued_by\":\"" << tracks[qi].queued_by << "\"}";
            }
        }
        os << "]}";
        return http_raw("200 OK", "application/json", os.str());
    }
    if(method=="GET"&&path.rfind("/u/",0)==0) {
        const std::string who = path.substr(3);
        if (!valid_username(who))
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such profile.</p></div>"));
        auto found = g_store.find_by_username(who);
        if (!found)
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such profile.</p></div>"));
        if (acc && g_store.is_blocked(user, found->username))
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such profile.</p></div>"));
        const bool is_owner = acc && acc->username == found->username;
        const bool is_friend = acc && !is_owner && g_store.are_friends(user, found->username);
        auto target_prefs = g_store.prefs_of(found->username);
        if (!visible_to(target_prefs.profile_visibility, is_owner, acc.has_value(), is_friend)) {
            return http_code("200 OK", wrap(found->username, user, unread,
                "<div class=\"card\"><p>This profile is private.</p></div>"));
        }
        auto committees = g_store.communities_of(found->username);
        std::string committees_html;
        if (!committees.empty()) {
            committees_html = "<p class=\"eyebrow\" style=\"margin-top:1rem\">Groups</p>"
                               "<div style=\"display:flex;flex-wrap:wrap;gap:.4rem\">";
            for (const auto& c : committees) {
                committees_html += "<a class=\"chip\" href=\"/c/"+html_escape(c.slug)+"\" style=\"text-decoration:none\">"+
                                    html_escape(c.name)+"</a>";
            }
            committees_html += "</div>";
        }
        // A real button, centered at the top of the page -- not the old
        // underlined text link buried under the portrait, easy to miss
        // and inconsistent with every other primary action on the site.
        // Owner-only: is_owner is the same check the rest of this page
        // already uses to decide what a stranger versus the account
        // itself gets to see here.
        std::string html = !is_owner ? "" :
            "<p class=\"row\" style=\"justify-content:center;margin:0 0 1rem\">"
            "<a class=\"btn\" href=\"/me\">Edit profile</a></p>";
        // A profile-background drawing is the identity card's own
        // background now, not a separate sandboxed iframe (that iframe
        // used to show a near-empty white box -- just a duplicate
        // username heading plus the drawing bleeding through -- for
        // anyone with frames but no custom theme). #profilecard only
        // gets the id (and the <style> block only gets emitted) when
        // there's actually a drawing to show.
        auto canvas_frame_ids = g_store.canvas_frames(found->id);
        std::string card_css = profile_canvas_css(found->username, canvas_frame_ids.size(),
                                                    g_store.canvas_timelapse(found->id));
        html += card_css.empty() ? "" : "<style>"+card_css+"</style>";
        html += std::string("<div class=\"card\"") + (canvas_frame_ids.empty() ? "" : " id=\"profilecard\"") +
            "><div class=\"who\">"+portrait_tag(found->username,"")+
            "<div><h1>"+html_escape(found->username)+"</h1>";
        {
            auto pr = g_store.profile_of(found->username);
            if (!pr.display_name.empty())
                html += "<p class=\"lede\" style=\"margin:.2rem 0 .4rem\">"+html_escape(pr.display_name)+"</p>";
            auto bio = g_store.blurb(found->id);
            std::string bits;
            if (!pr.living.empty()) bits += html_escape(pr.living);
            if (!pr.hometown.empty()) bits += (bits.empty()?"":" · ")+std::string("from ")+html_escape(pr.hometown);
            if (!pr.career.empty()) bits += (bits.empty()?"":" · ")+html_escape(pr.career);
            if (!pr.school.empty()) bits += (bits.empty()?"":" · ")+html_escape(pr.school);
            if (!bits.empty()) html += "<p class=\"muted\">"+bits+"</p>";
            if (!bio.empty()) html += "<p>"+html_escape(bio)+"</p>";
            if (!pr.interests.empty()) html += "<p class=\"muted\">"+html_escape(pr.interests)+"</p>";
        }
        html +=
            "<table class=\"record\">"
            "<tr><td>Standing</td><td>Associate &middot; good</td></tr>"
            "<tr><td>Enrolled</td><td>"+html_escape(fmt_time(found->created_at))+"</td></tr>"
            "</table></div></div>"+committees_html;
        if (!g_store.author_css(found->id).empty()) {
            // Custom theme only -- a profile-background drawing renders
            // directly on this page now (see profile_canvas_css above),
            // not through this iframe at all.
            // sandbox="allow-same-origin", not sandbox="" (opaque origin):
            // confirmed live that opaque-origin sandboxed iframes get
            // silently dropped by real-world browser privacy/tracking
            // protections (reproduced in two independent browsers, not a
            // one-off) -- the iframe element rendered but its document
            // never loaded. allow-same-origin fixes that. This does NOT
            // reopen the security hole it looks like: allow-scripts is
            // still withheld, so nothing can ever execute inside this
            // frame regardless of origin -- and with zero script
            // execution possible, having the same origin as the rest of
            // the site is not exploitable (there's no JS to act on it).
            // The two real defenses -- sanitize_author_css rejecting
            // script-adjacent CSS, and no script execution ever being
            // possible in this frame -- are both still fully intact.
            html += "<iframe src=\"/u/"+html_escape(found->username)+"/theme\" sandbox=\"allow-same-origin\" "
                    "title=\"Profile theme\" "
                    "style=\"width:100%;min-height:320px;border:1px solid var(--rule);"
                    "border-radius:var(--rsm);background:#fff\"></iframe>";
        }
        if (is_owner) {
            // Edit profile is now the centered button at the top of the
            // page (see above) -- nothing further needed here.
        } else if (acc && g_store.are_friends(user, found->username)) {
            html += "<p class=\"row\"><a class=\"btn\" href=\"/mail?with="+html_escape(found->username)+"\">Message</a>"
                    "<form class=\"inline\" method=\"POST\" action=\"/friends/unfriend\" "
                    "onsubmit=\"return confirm('Unfriend "+html_escape(found->username)+"? You will leave every group you share with them. They stay in those groups.');\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(found->username)+"\">"
                    "<button class=\"ghost\" type=\"submit\">Unfriend</button></form></p>";
        } else if (acc) {
            html += "<form method=\"POST\" action=\"/friends\"><input type=\"hidden\" name=\"who\" value=\""+
                    html_escape(found->username)+"\"><button>Send friend request</button></form>";
        }
        html += "</div>";
        html += jukebox_widget(found->username, is_owner, user, qs);
        // "Play my Juketube when someone opens my profile" -- off by
        // default. Signed-in visitors only (the floating widget doesn't
        // exist for a logged-out guest, so there'd be nothing to tune),
        // and never for the owner's own visit to their own page --
        // is_owner is already computed above for exactly this branch.
        if (acc && !is_owner && g_store.jukebox_autoplay(found->username)) {
            // wrap() appends the floating widget's own markup+script right
            // after </main> (see its comment -- keeps the widget alive
            // across soft-nav, which only ever touches <main>'s innerHTML),
            // which means __lgTuneStation isn't defined yet at the point
            // this inline script -- inside <main> -- actually runs.
            // setTimeout(0) pushes the call past the rest of this initial
            // parse, by which point the widget's script further down the
            // document has already executed and defined it.
            html += "<script>(function(){"
                    "setTimeout(function(){"
                    "if(window.__lgTuneStation)window.__lgTuneStation('u:"+found->username+"');"
                    "},0);"
                    "})();</script>";
        }
        {
            // A post's own audience (the /feed "Anybody"/"Friends"/"Groups"
            // chips) gates it here, not the account-wide posts_visibility
            // setting -- that blanket still governs only a GroupsOnly post
            // shown on this list (unchanged; a community's real audience is
            // its membership, this is just "does this profile show its
            // posts at all" for those).
            auto theirPosts = g_store.posts_by(found->username);
            std::reverse(theirPosts.begin(), theirPosts.end());
            std::vector<Post> shown;
            for (const auto& p : theirPosts) {
                bool ok;
                if (is_owner) {
                    ok = true;
                } else if (p.audience == PostAudience::GroupsOnly) {
                    ok = visible_to(target_prefs.posts_visibility, is_owner, acc.has_value(), is_friend);
                } else if (p.audience == PostAudience::Anybody) {
                    ok = std::find(p.exceptions.begin(), p.exceptions.end(), user) == p.exceptions.end();
                } else { // Friends, possibly also posted into a community
                    ok = is_friend && target_prefs.posts_visibility != Visibility::OnlyMe &&
                         (p.friend_allowlist.empty() ||
                          std::find(p.friend_allowlist.begin(), p.friend_allowlist.end(), user) !=
                              p.friend_allowlist.end());
                }
                if (ok) shown.push_back(p);
            }
            if (is_owner || !shown.empty()) {
                html += "<h2>Posts</h2><p class=\"muted eyebrow\">"+std::to_string(shown.size())+
                        (shown.size()==1?" entry":" entries")+"</p>";
                for (const auto& p : shown) html += "<div class=\"card\">"+post_block(p, user)+"</div>";
            }
        }
        return pg(found->username, html);
    }

    if(method=="GET"&&path=="/friends") {
        if(!acc) return need_login();
        std::string err = flash_error(qs);
        std::string html = "<h1>People</h1>"+err+ people_suggestions_html(user, 12, "/friends") +
            "<div class=\"card\"><h2>Find someone</h2>"
            "<form method=\"GET\" action=\"/search\"><label>Type a name</label>"
            "<div class=\"row\"><input name=\"q\" id=\"peopleq\" autocomplete=\"off\" "
            "placeholder=\"Starts matching as you type\"><button>Search</button></div></form>"
            "<div id=\"peoplehits\"></div>"
            "<p class=\"muted\">Friends always appear. Everyone else follows their findability. "
            "Mail is friends-only until they accept.</p></div>"
            + people_search_script() +
            "<div class=\"card\"><h2>Add by exact name</h2>"
            "<form method=\"POST\" action=\"/friends\"><input name=\"who\" placeholder=\"account name\" autocomplete=\"off\" required>"
            "<button>Send request</button></form></div>";
        html += "<div class=\"card\"><h2>Friends</h2>";
        auto pals = g_store.friends_of(user);
        auto inner = g_store.inner_of(user);
        std::set<std::string> inner_set(inner.begin(), inner.end());
        html += "<p class=\"muted eyebrow\">"+std::to_string(pals.size())+
                (pals.size()==1?" friend":" friends")+" &middot; "+std::to_string(inner_set.size())+" inner</p>";
        if(pals.empty()) html += "<p class=\"empty\">No friends yet. Search a username above, or send a request by exact name.</p>";
        for (auto& other : pals) {
            const bool inn = inner_set.count(other);
            html += "<div class=\"person\"><div class=\"who\">"+portrait_tag(other,"sm")+"<div><a href=\"/u/"+html_escape(other)+"\"><strong>"+html_escape(other)+"</strong></a>"
                    +(inn?" <span class=\"muted\">inner circle</span>":"")+
                    "</div></div><div class=\"row\">"
                    "<a class=\"btn ghost\" href=\"/mail?with="+html_escape(other)+"\">Mail</a>";
            if (inn) {
                html += "<form class=\"inline\" method=\"POST\" action=\"/friends/inner-remove\">"
                        "<input type=\"hidden\" name=\"who\" value=\""+html_escape(other)+"\">"
                        "<button class=\"ghost\" type=\"submit\">From inner</button></form>";
            } else {
                html += "<form class=\"inline\" method=\"POST\" action=\"/friends/inner\">"
                        "<input type=\"hidden\" name=\"who\" value=\""+html_escape(other)+"\">"
                        "<button class=\"ghost\" type=\"submit\">Inner circle</button></form>";
            }
            html += "<form class=\"inline\" method=\"POST\" action=\"/friends/unfriend\" "
                    "onsubmit=\"return confirm('Unfriend this person? You will leave every group you share with them. They stay in those groups.');\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(other)+"\">"
                    "<button class=\"ghost\" type=\"submit\">Unfriend</button></form>"
                    "<form class=\"inline\" method=\"POST\" action=\"/friends/block\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(other)+"\">"
                    "<button class=\"danger\" type=\"submit\">Block</button></form></div></div>";
        }
        html += "<p class=\"muted\">Inner circle is up to twelve people. Everyone else you've accepted is still a friend.</p></div>";
        html += "<div class=\"card\"><h2>Incoming</h2>";
        auto inc = g_store.incoming_requests(user);
        if(inc.empty()) html += "<p class=\"empty\">None.</p>";
        // a face and a link on every request, Accept and Decline side by side (Brandon 2026-11-10: friendlier)
        auto who_html = [](const std::string& n) {
            return "<div class=\"who\">" + portrait_tag(n, "sm") + "<a href=\"/u/" + html_escape(n) + "\"><strong>" +
                   html_escape(n) + "</strong></a></div>";
        };
        for (auto& from : inc) {
            html += "<div class=\"person\">" + who_html(from) + "<div class=\"row\">"
                    "<form class=\"inline\" method=\"POST\" action=\"/friends/accept\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(from)+"\"><button type=\"submit\">Accept</button></form>"
                    "<form class=\"inline\" method=\"POST\" action=\"/friends/decline\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(from)+"\"><button class=\"ghost\" type=\"submit\">Decline</button></form>"
                    "</div></div>";
        }
        html += "</div><div class=\"card\"><h2>Outgoing</h2>"
                "<p class=\"muted\">If someone does not accept, you will not get a notice. That is intentional.</p>";
        auto outg = g_store.outgoing_requests(user);
        if(outg.empty()) html += "<p class=\"empty\">None.</p>";
        for (auto& to : outg) html += "<div class=\"person\">" + who_html(to) + "<span class=\"muted\">waiting</span></div>";
        html += "</div>";
        return pg("People", html);
    }

    if(method=="GET"&&path=="/search") {
        if(!acc) return need_login();
        auto term = trim_ws(qs["q"]);
        if (qs["partial"]=="1")
            return http_raw("200 OK", "text/html; charset=utf-8", people_results_html(user, term),
                "Cache-Control: no-store\r\n");
        std::string html = "<h1>Search</h1><div class=\"card\">"
            "<form method=\"GET\" action=\"/search\"><label>Find a person</label>"
            "<div class=\"row\"><input name=\"q\" id=\"peopleq\" value=\""+html_escape(term)+"\" autocomplete=\"off\">"
            "<button>Search</button></div></form>"
            "<p class=\"muted\">Friends always appear. Everyone else follows their findability. "
            "A miss is a miss &mdash; missing, opted out, and blocked look the same. "
            "Mail is friends-only until they accept.</p>"
            "<div id=\"peoplehits\">"+people_results_html(user, term)+"</div></div>"
            + people_search_script();
        return pg("Search", html);
    }

    // Where nginx sends a /mail/big upload it refused before reading it.
    if(method=="GET"&&path=="/mail/too-large") {
        std::string why = "That file is bigger than you can send.";
        if (!acc || !program_policy_for(user)) why = "Only an admin, or someone an admin allowed, can send programs and large files.";
        if (qs["status"] == "429") why = "Too many uploads at once. Wait a minute, then send it again.";
        else if (!qs["status"].empty() && qs["status"] != "413") why = "It wasn't sent (the site didn't answer). Try again in a moment.";
        return pg("Not sent", "<div class=\"card\"><h1>Not sent</h1><p>" + why + "</p>"
                  "<p><a href=\"/mail\">Back to mail</a></p></div>");
    }

    if(method=="GET"&&path=="/mail") {
        if(!acc) return need_login();
        g_store.mark_notices_read_of_type(user, "message");
        std::string thread = mail_thread_html(user, qs["with"], qs);
        if (!qs["with"].empty() && thread.empty()) {
            thread = "<div class=\"card\"><p class=\"warn\">Mail is friends-only until they accept. "
                     "<a href=\"/search?q="+html_escape(qs["with"])+"\">Find "+html_escape(qs["with"])+"</a></p></div>";
        }
        // Brief 4 item 8 (Brandon 2026-11-11 4:37 PM): the heading matches the nav ("Messages", not "Mail"), and the
        // age-assurance note is one line plus "more" so the conversation starts higher up. .mailpage scopes the layout
        // (list beside the thread on wide screens, one scroll region, composer pinned; see css()).
        std::string html = "<div class=\"mailpage\"><h1>Messages</h1>"
            "<div class=\"muted mailage\">Direct messages are friends-only for now. "
            "<details class=\"mailmore\"><summary>more</summary>"
            "Open messaging waits on age assurance so a stranger cannot reach a minor. We will not treat a checkbox as age."
            "</details></div>";
        if (qs["skipped"].size()) html += "<p class=\"warn\">Couldn’t include: "+html_escape(qs["skipped"])+
            ". Unknown, blocked, and contact-settings conflicts are skipped — start a new thread with just them if you still want to talk.</p>";
        html += "<div class=\"card mailwidget"+std::string(thread.empty() ? "" : " hasthread")+"\">"+thread+mail_inbox_html(user, acc->id)+"</div></div>";
        // No separate "Messages" app any more (one app), so no "add Messages
        // to your Home Screen" nudge here; mail_install_hint() is unused.
        return pg("Messages", html);
    }
    if(method=="GET"&&path=="/mail/panel") {
        if(!acc) return http_code("401 Unauthorized", "");
        g_store.mark_notices_read_of_type(user, "message");
        std::string thread = mail_thread_html(user, qs["with"], qs);
        std::string html = thread+mail_inbox_html(user, acc->id);
        return http_ok(html);
    }

    if(method=="GET"&&path.rfind("/mail/attachment/",0)==0) {
        if(!acc) return need_login();
        const std::string blob_id = path.substr(17);
        auto found = g_store.find_mail_attachment(user, blob_id);
        if (!found) {
            auto gfound = g_store.find_group_attachment(acc->id, blob_id);
            if (!gfound) return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
            return mail_attachment_response(blob_id,
                gfound->attachment_mime.empty() ? "application/octet-stream" : gfound->attachment_mime,
                attachment_name_key(*gfound));
        }
        const std::string other = (found->from == user) ? found->to : found->from;
        // Same gate as the thread view: attachments do not outlive the
        // friendship they were sent inside of. Do not open stranger DMs.
        if (!g_store.are_friends(user, other) || g_store.is_blocked(user, other) || g_store.is_blocked(other, user))
            return http_raw("404 Not Found", "text/plain; charset=utf-8", "not found");
        // (the same bytes sent twice under two names: the download is named after the message the store finds)
        return mail_attachment_response(blob_id, found->attachment_mime.value_or("application/octet-stream"),
                                        attachment_name_key(*found));
    }

    if(method=="GET"&&path.rfind("/mail/g/",0)==0) {
        if(!acc) return need_login();
        ConversationId conv_id = 0;
        try { conv_id = std::stoull(path.substr(8)); } catch(...) { conv_id = 0; }
        auto conv = conv_id ? g_store.get_conversation(conv_id) : std::nullopt;
        const ConversationMember* self_member = nullptr;
        if (conv) for (auto& m : conv->members) if (m.account == acc->id) self_member = &m;
        if (!conv || !conv->is_group || !self_member)
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such group.</p></div>"));
        g_store.mark_thread_read(user, "g:" + std::to_string(conv_id));
        const std::string base = "/mail/g/" + std::to_string(conv_id);
        const bool active = self_member->state == MemberState::Active;

        std::string html = "<p><a href=\"/mail\">\xE2\x86\x90 Mail</a></p>"
            "<h1>"+html_escape(group_label(*conv, acc->id))+"</h1>"
            "<p class=\"muted\">Platform group conversation. Not an AVND locality.</p>";
        if (qs["err"]=="invite") html += "<p class=\"warn\">Couldn’t add them — conflicts with someone’s contact settings. "
            "Start a new thread with just them if you still want to talk.</p>";
        if (!qs["skipped"].empty()) html += "<p class=\"warn\">Couldn’t include: "+html_escape(qs["skipped"])+". "
            "Unknown, blocked, and contact-settings conflicts are skipped.</p>";
        if (qs["err"]!="invite") html += mail_attachment_error_html(qs["err"], user);

        if (!active) {
            html += "<div class=\"mailpending\"><p><strong>Waiting for you to join.</strong> "
                    "You can’t send until you accept. Declining is never announced to the room.</p>"
                    "<form method=\"POST\" action=\""+base+"/accept\"><button>Accept</button></form>"
                    "<form method=\"POST\" action=\""+base+"/leave\"><button class=\"ghost\">Decline</button></form></div>";
        }

        html += "<div class=\"card\">";
        auto msgs = g_store.messages_in(conv_id);
        html += "<div class=\"msglist\" id=\"msglist\" style=\"max-height:none\">";
        bool any = false;
        for (auto& m : msgs) {
            if (m.deletion==DeletionState::DeletedForSender && m.sender==acc->id) continue;
            any = true;
            const bool mine = m.sender==acc->id;
            html += "<div class=\"msg "+std::string(mine?"mine":"theirs")+"\"><div class=\"bubble\">";
            if (!mine) html += "<strong>"+html_escape(display_name_for(m.sender))+"</strong> ";
            if (m.deletion==DeletionState::DeletedForEveryone) {
                html += "<em class=\"muted\">Message deleted.</em>";
            } else {
                if (!m.text.empty()) html += html_escape(m.text);
                if (!m.attachment_blob_id.empty())
                    html += mail_attachment_html(m.attachment_blob_id, m.attachment_mime, attachment_name_key(m),
                                                 display_name_for(m.sender), mine);
                if (m.edited_at>0) html += "<span class=\"muted\"> (edited)</span>";
            }
            html += "<span class=\"bubtime\">"+html_escape(fmt_time(m.sent_at))+"</span></div></div>";
        }
        if (!any) html += "<p class=\"empty\">No messages yet.</p>";
        html += "</div>";
        if (active) {
            html += "<form method=\"POST\" action=\""+base+"/send\" enctype=\"multipart/form-data\" class=\"mailcomposer\" id=\"mailcomposer\""
                    + std::string(kComposerOnSubmit) + ">"
                    "<div class=\"row\" style=\"align-items:flex-end;gap:.4rem;flex-wrap:nowrap\">"
                    "<textarea name=\"text\" id=\"mailtext\" maxlength=\"2000\" rows=\"2\" placeholder=\"Message\" "
                    "style=\"flex:1;min-width:0;min-height:44px;resize:vertical\"></textarea>"
                    "<label class=\"mailattbtn\" title=\"Attach a file\">"
                    + mail_file_input_html(user, "/mail/big/g/" + std::to_string(conv_id) + "/send") +
                    "<span aria-hidden=\"true\">\xF0\x9F\x93\x8E</span></label>"
                    "<button type=\"submit\">Send</button></div>"
                    + std::string(kComposerFileLine) +
                    "<p class=\"muted\" style=\"margin:.3rem 0 0\">Enter sends. Shift+Enter is a new line.</p>"
                    + mail_program_note_html(user) + "</form>";
        }
        html += "</div>";

        html += "<details class=\"card\"><summary>Members</summary><ul class=\"plain\">";
        for (auto& m : conv->members) {
            html += "<li>"+html_escape(display_name_for(m.account))+
                    (m.state==MemberState::Pending?" <span class=\"muted\">invited \xE2\x80\x94 not in yet</span>":"")+"</li>";
        }
        html += "</ul>";
        if (active) {
            html += "<form method=\"POST\" action=\""+base+"/invite\"><label>Invite</label>"
                    "<div class=\"row\"><input name=\"who\" required autocomplete=\"off\"><button>Invite</button></div></form>"
                    "<form method=\"POST\" action=\""+base+"/title\" class=\"mailnewgroup\">"
                    "<label>Group name</label><input name=\"title\" maxlength=\"80\" value=\""+html_escape(conv->title)+"\">"
                    "<button class=\"ghost\">Save name</button></form>"
                    "<form method=\"POST\" action=\""+base+"/leave\" onsubmit=\"return confirm('Leave this group? You will not be told if others stay.');\">"
                    "<button class=\"ghost\">Leave group</button></form>";
        }
        html += "</details>"
            "<script>(function(){"
            "var list=document.getElementById('msglist');if(list)list.scrollTop=list.scrollHeight;"
            "var form=document.getElementById('mailcomposer'),ta=document.getElementById('mailtext');"
            "if(!form||!ta)return;"
            "ta.addEventListener('keydown',function(e){"
            "if(e.key==='Enter'&&!e.shiftKey){e.preventDefault();form.requestSubmit();}"
            "});"
            "})();</script>";
        return pg("Group", html);
    }

    if(method=="GET"&&path=="/communities") {
        if(!acc) return need_login();
        std::string err = flash_error(qs);
        std::string html = "<h1>Groups</h1>"+err+
            "<p class=\"lede\">Platform groups. Not AVND localities, and they do not confer standing.</p>";
        auto all = g_store.all_communities();
        std::vector<Community> mine;
        for (auto& c : all) if (g_store.is_member(c.slug, user)) mine.push_back(c);
        if (!mine.empty()) {
            html += "<div class=\"card\"><h2>Yours</h2><ul class=\"plain\">";
            for (auto& c : mine) {
                html += "<li class=\"who\" style=\"margin:.4rem 0\"><div><a href=\"/c/"+html_escape(c.slug)+"\"><strong>"+
                        html_escape(c.name)+"</strong></a> <span class=\"muted\">/"+html_escape(c.slug)+"</span></div></li>";
            }
            html += "</ul></div>";
        }
        auto cities = g_store.cities_in_use();
        if (!cities.empty()) {
            html += "<div class=\"card\"><h2>Cities</h2>"
                    "<p class=\"muted\">Groups that name the same city are listed together, so people there can find each other. "
                    "<a href=\"/map\">See them on the MAINstream map</a>.</p><p>";
            for (const auto& cu : cities) {
                html += "<a class=\"chip\" href=\"/city/"+url_encode(cu.key)+"\" style=\"text-decoration:none\">"+
                        html_escape(cu.name)+" &middot; "+std::to_string(cu.groups)+"</a> ";
            }
            html += "</p></div>";
        }
        html += "<div class=\"card\"><h2>Find a group</h2>"
            "<input id=\"commfilter\" type=\"search\" placeholder=\"Search by name or address\" "
            "oninput=\"var q=this.value.toLowerCase();document.querySelectorAll('.commrow').forEach(function(el){"
            "el.style.display=el.getAttribute('data-q').indexOf(q)>=0?'':'none';});\">";
        if (all.empty()) html += "<p class=\"empty\">None yet. Start one below.</p>";
        else {
            std::map<int, std::map<std::string, std::vector<Community>>> grouped;
            for (auto& c : all) grouped[static_cast<int>(c.category)][c.subcategory].push_back(c);
            for (const auto& ci : kCommunityCategories) {
                auto git = grouped.find(static_cast<int>(ci.value));
                if (git == grouped.end()) continue;
                std::size_t total = 0;
                for (auto& [subcat, list] : git->second) total += list.size();
                html += "<details open><summary>"+std::string(ci.label)+" &middot; "+std::to_string(total)+"</summary>";
                for (auto& [subcat, list] : git->second) {
                    html += "<div style=\"margin:.5rem 0 .5rem 1rem\">"
                            "<p class=\"eyebrow\">"+html_escape(subcat.empty() ? "General" : subcat)+"</p><ul class=\"plain\">";
                    for (auto& c : list) {
                        std::string q = c.name + " " + c.slug + " " + c.about + " " + c.subcategory + " " + c.city;
                        for (char& ch : q) if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
                        const std::string chref = city_href(c.city);
                        html += "<li class=\"commrow\" data-q=\""+html_escape(q)+"\"><a href=\"/c/"+html_escape(c.slug)+"\"><strong>"+
                                html_escape(c.name)+"</strong></a> <span class=\"muted\">/"+html_escape(c.slug)+"</span>"+
                                (chref.empty() ? std::string() : " <a class=\"muted\" href=\""+chref+"\">"+html_escape(c.city)+"</a>")+
                                "<p class=\"muted\">"+html_escape(c.about)+"</p></li>";
                    }
                    html += "</ul></div>";
                }
                html += "</details>";
            }
        }
        // /communities?city=<text> (the city page's "Start a group here")
        // opens the form with the city filled in.
        auto city_qs = qs.find("city");
        const std::string city_prefill = city_qs == qs.end() ? std::string() : clean_city(city_qs->second);
        html += "</div><div class=\"card\"><details"+std::string(city_prefill.empty() ? "" : " open")+"><summary>Start a group</summary>"
            "<form method=\"POST\" action=\"/communities\" style=\"margin-top:.7rem\">"
            "<label>Name</label><input name=\"name\" required maxlength=\"80\" placeholder=\"Neighborhood watch\">"
            "<label>Link</label><input name=\"slug\" required maxlength=\"32\" pattern=\"[A-Za-z0-9._-]+\" placeholder=\"watch\">"
            "<p class=\"muted\" style=\"margin-top:-.3rem\">Becomes /c/watch &mdash; letters, numbers, dots, dashes.</p>"
            "<label>Category</label><select name=\"category\" id=\"catselect\">";
        for (const auto& ci : kCommunityCategories) {
            html += "<option value=\""+std::to_string(static_cast<int>(ci.value))+"\">"+ci.label+"</option>";
        }
        html += "</select>"
            "<label>Subcategory</label>"
            "<input name=\"subcategory\" id=\"subcatinput\" required maxlength=\"60\" list=\"subcatlist\" "
            "placeholder=\"e.g. Neighborhood Watch\" autocomplete=\"off\">"
            "<datalist id=\"subcatlist\"></datalist>"
            "<p class=\"muted\" style=\"margin-top:-.3rem\">Pick an existing one, or type a new one.</p>"
            "<label>City <span class=\"muted\">(optional)</span></label>"
            "<input name=\"city\" maxlength=\"80\" list=\"citylist\" placeholder=\"e.g. Fresno, CA\" autocomplete=\"off\" value=\""+
            html_escape(city_prefill)+"\">"+city_datalist_html()+
            "<p class=\"muted\" style=\"margin-top:-.3rem\">Groups that name the same city are listed together. "
            "Pick an existing spelling if yours is there. A city here is a label, not a government.</p>"
            "<label>About</label><textarea name=\"about\" maxlength=\"2000\" placeholder=\"What this group is for.\"></textarea>"
            "<button>Create</button></form>"
            "<script>(function(){"
            "var SUB="+community_subcats_js()+";"
            "var sel=document.getElementById('catselect'),dl=document.getElementById('subcatlist');"
            "function refresh(){"
            "dl.innerHTML='';"
            "(SUB[sel.value]||[]).forEach(function(s){"
            "var o=document.createElement('option');o.value=s;dl.appendChild(o);"
            "});"
            "}"
            "sel.addEventListener('change',refresh);refresh();"
            "})();</script>"
            "</details></div>";
        return pg("Groups", html);
    }

    if(method=="GET"&&path.rfind("/c/",0)==0&&path.size()>10&&path.compare(path.size()-7,7,"/canvas")==0) {
        if(!acc) return need_login();
        const std::string slug = path.substr(3, path.size()-3-7);
        if (!g_store.community(slug) || !g_store.is_member(slug, user)) return http_redir("/c/"+slug);
        return pg("Compose a post", canvas_tool_page(user, "", false, "/c/"+slug));
    }

    if(method=="GET"&&path.rfind("/c/",0)==0) {
        if(!acc) return need_login();
        const std::string slug = path.substr(3);
        auto c = g_store.community(slug);
        if (!c)
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such group.</p></div>"));
        const bool member = g_store.is_member(slug, user);
        const bool owner = c->owner == user;
        std::string html = "<h1>"+html_escape(c->name)+"</h1>"
            "<p class=\"muted\">"+html_escape(category_label(c->category))+" &middot; "+
            html_escape(c->subcategory.empty() ? "General" : c->subcategory)+
            " · /"+html_escape(c->slug)+" · started by <a href=\"/u/"+html_escape(c->owner)+"\">"+html_escape(c->owner)+"</a>"+
            (city_href(c->city).empty() ? std::string()
                : " · <a href=\""+city_href(c->city)+"\">"+html_escape(c->city)+"</a>")+"</p>";
        html += flash_error(qs);
        html += "<div class=\"card\"><p>"+html_escape(c->about.empty()?"No description yet.":c->about)+"</p>";
        if (!member) {
            html += "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"/join\"><button>Join this group</button></form>";
        } else if (!owner) {
            html += "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"/leave\"><button class=\"ghost\">Leave</button></form>";
        } else {
            html += "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"/delete\" onsubmit=\"return confirm('Delete this group and its posts?');\">"
                    "<button class=\"ghost\">Delete group</button></form>";
            html += "<details style=\"margin-top:.6rem\"><summary>"+
                    std::string(c->city.empty() ? "Add a city" : "Change city")+"</summary>"
                    "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"/city\" style=\"margin-top:.5rem\">"
                    "<div class=\"row\"><input name=\"city\" maxlength=\"80\" list=\"citylist\" placeholder=\"e.g. Fresno, CA\" "
                    "autocomplete=\"off\" value=\""+html_escape(c->city)+"\"><button>Save</button></div>"+city_datalist_html()+
                    "<p class=\"muted\">Groups that name the same city are listed together so people there can find each other. "
                    "Leave it empty to remove the city.</p></form></details>";
        }
        html += "</div>";
        if (member) {
            html += "<div class=\"card\">"
                    "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"\" enctype=\"multipart/form-data\">"
                    "<textarea name=\"text\" maxlength=\"2000\" placeholder=\"Post in "+html_escape(c->name)+"\" "
                    "oninput=\"document.getElementById('cchars').textContent=(2000-this.value.length)+' left'\"></textarea>"
                    + composer_attach_bar("cchars") +
                    "</form>"
                    "<p class=\"muted\" style=\"margin-top:.5rem\"><a href=\"/c/"+html_escape(slug)+"/canvas\">Draw instead</a></p></div>";
        }
        auto posts = g_store.posts_in(slug, 80);
        if (posts.empty()) html += "<div class=\"card\"><p class=\"empty\">No posts yet.</p>"
            "<p class=\"muted\">"+(member ? std::string("You're in. Write the first one.") : "Join to post.")+"</p></div>";
        for (const auto& p : posts) html += "<div class=\"card\">"+post_block(p, user)+"</div>";
        html += "<div class=\"card\"><details><summary>Members · "+std::to_string(g_store.members_of(slug).size())+"</summary><ul class=\"plain\">";
        // Add friend beside each member you could ask (Brandon 2026-11-10): not you, a friend, a pending request or a block
        std::set<std::string> asked;
        if (!user.empty()) {
            for (const auto& u : g_store.incoming_requests(user)) asked.insert(u);
            for (const auto& u : g_store.outgoing_requests(user)) asked.insert(u);
        }
        for (auto& n : g_store.members_of(slug)) {
            const bool can_add = !user.empty() && n != user && !asked.count(n) && !g_store.are_friends(user, n) &&
                                 !g_store.is_blocked(user, n) && !g_store.is_blocked(n, user);
            html += "<li class=\"who\" style=\"margin:.35rem 0;align-items:center\">"+portrait_tag(n,"sm")+
                    "<div><a href=\"/u/"+html_escape(n)+"\">"+html_escape(n)+"</a>"+
                    (n==c->owner?" <span class=\"muted\">starter</span>":"")+"</div>";
            if (can_add)
                // same gold Add friend as People you may know (one look everywhere), centred on the row
                html += "<form class=\"inline\" style=\"margin:0 0 0 auto;align-self:center\" method=\"POST\" action=\"/friends\" data-add=\""+html_escape(n)+"\">"
                        "<input type=\"hidden\" name=\"who\" value=\""+html_escape(n)+"\">"
                        "<input type=\"hidden\" name=\"next\" value=\"/c/"+html_escape(slug)+"\">"
                        "<button type=\"submit\">Add friend</button></form>";
            html += "</li>";
        }
        html += "</ul>";
        if (member) {
            html += "<form method=\"POST\" action=\"/c/"+html_escape(slug)+"/invite\" style=\"margin-top:.6rem\">"
                    "<label>Invite by account name</label><div class=\"row\"><input name=\"who\" required placeholder=\"account name\" autocomplete=\"off\"><button>Invite</button></div></form>";
        }
        html += "</details></div>";
        return pg(c->name, html);
    }

    // /city/<key>: every group that names this city (city proposals build
    // plan, slice 1). A city is a label people chose, not an AVND locality.
    // Map view (slice 6): /map is the MAINstream; /city/<key>/map is one city's branch.
    if(method=="GET"&&path=="/map") {
        if(!acc) return need_login();
        // Interactive map (assets/lgmap.js): click a box and its connections
        // grow out of it; its page opens in the field on the right, which can
        // fill the window; MAINstream shrinks it back. <noscript> keeps the
        // plain drawing for browsers without JavaScript.
        std::string focus;
        if (qs.count("focus") && map_gnode(qs.at("focus"), user)) focus = qs.at("focus");
        return pg("Map", map_page_body(user, focus));
    }
    // Your own wishes and pain points. Owner only; nobody else has a way in.
    if(method=="GET"&&path=="/problems") {
        if(!acc) return need_login();
        const bool on = g_store.map_prefs_of(user).pains;
        std::string html = "<h1>Your problems and solutions</h1>"
            "<p class=\"lede\">Things you said you wish for or are tired of, so you can find what and who could help. "
            "Only you can see this page. Flow puts one kept subject on the map for people who can see you. "
            "Flowing it also shows you who else Flowed that same subject.</p>";
        const bool looked_none = on && qs.count("looked") && qs.at("looked") == "none";
        if (!on) html += "<div class=\"card\"><p>This is turned off, so nothing is being noticed. "
                         "<a href=\"/settings#map-privacy\">Turn it on in Settings</a>.</p></div>";
        else {
            html += "<p class=\"muted\">It reads your new posts, proposals, proposal comments, and messages you sent. "
                    "<a href=\"/settings#map-privacy\">Change this</a></p>";
            html += "<form method=\"POST\" action=\"/problems/look\">"
                    "<button type=\"submit\">Look at what I've written</button></form>";
            html += "<p class=\"muted\">This reads posts, proposals, proposal comments, and messages you already wrote. "
                    "Each one is offered here as Keep or Forget before it is a box. "
                    "Friends posts, group posts, and messages stay on your map only. "
                    "It does not read the other person's messages, or reposts.</p>";
            if (looked_none) html += "<p class=\"muted\">Nothing new in what you've written.</p>";
        }
        auto all = g_store.pains_of(user);
        auto source_link = [](const PainPoint& p) -> std::string {
            auto post_href = [](const std::string& source, std::size_t skip, const char* label) {
                return " <a class=\"muted\" href=\"/post/" + html_escape(source.substr(skip)) + "\">" + label + "</a>";
            };
            if (p.source.rfind("post:", 0) == 0) return post_href(p.source, 5, "from your post");
            if (p.source.rfind("fpost:", 0) == 0) return post_href(p.source, 6, "from a friends post");
            if (p.source.rfind("gpost:", 0) == 0) return post_href(p.source, 6, "from a group post");
            if (p.source.rfind("prop:", 0) == 0) return " <a class=\"muted\" href=\"/proposals?id=" + html_escape(p.source.substr(5)) + "\">from your proposal</a>";
            if (p.source.rfind("mail:", 0) == 0) return " <span class=\"muted\">from a message you sent</span>";
            if (p.source.rfind("gmsg:", 0) == 0) return " <span class=\"muted\">from a group message you sent</span>";
            return " <span class=\"muted\">from your comment</span>";
        };
        auto label = [](const PainPoint& p) { return std::string(p.kind == "wish" ? "Want" : "Problem"); };
        auto button = [](const char* action, std::uint64_t id, const char* text, bool ghost) {
            return std::string("<form method=\"POST\" action=\"/problems/") + action + "\" style=\"display:inline\">"
                   "<input type=\"hidden\" name=\"id\" value=\"" + std::to_string(id) + "\"><button" +
                   (ghost ? " class=\"ghost\"" : "") + ">" + text + "</button></form> ";
        };
        std::string sugg, kept;
        for (const auto& p : all) {
            if (!p.kept) {
                sugg += "<li style=\"margin:.6rem 0\"><span class=\"eyebrow\">" + label(p) + "</span> <strong>" +
                        html_escape(p.phrase) + "</strong>" + source_link(p) + "<div style=\"margin-top:.3rem\">" +
                        button("keep", p.id, "Keep", false) + button("forget", p.id, "Forget", true) + "</div></li>";
                continue;
            }
            const auto m = pain_matches(p);
            std::string body;
            if (!m.depts.empty()) {
                body += "<p class=\"muted\" style=\"margin:.4rem 0 .1rem\">Who could act</p><p>";
                for (const auto& [key, tag] : m.depts)
                    body += "<a class=\"chip\" href=\"/proposals?city=" + url_encode(key) + "\">" + html_escape(tag) + "</a> ";
                body += "</p>";
            }
            if (!m.props.empty()) {
                body += "<p class=\"muted\" style=\"margin:.4rem 0 .1rem\">Proposals about it</p><ul class=\"plain\">";
                for (const auto& pr : m.props)
                    body += "<li><a href=\"/proposals?id=" + std::to_string(pr.id) + "\">" + html_escape(pr.title) + "</a></li>";
                body += "</ul>";
            }
            if (!m.groups.empty()) {
                body += "<p class=\"muted\" style=\"margin:.4rem 0 .1rem\">Groups working on it</p><p>";
                for (const auto& c : m.groups)
                    body += "<a class=\"chip\" href=\"/c/" + url_encode(c.slug) + "\">" + html_escape(c.name) + "</a> ";
                body += "</p>";
            }
            if (body.empty()) body = "<p class=\"muted\">No possible solutions on the site yet. You could start one.</p>";
            else body = "<p class=\"eyebrow\" style=\"margin-top:.6rem\">Possible solutions</p>" + body;
            const std::string start = "/proposals?title=" + url_encode(p.phrase) +
                                      (m.city_key.empty() ? std::string() : "&city=" + url_encode(m.city_key)) + "#new";
            const bool shareable = pain_can_flow(p.source);
            const std::string flow_note = !shareable
                ? std::string("This stays on your map. Other people do not see it.")
                : p.flowed
                    ? std::string("This subject is on the map for people who can see you.")
                    : std::string("Flow puts this subject on the map for people who can see you.");
            const std::string flow_btn = !shareable ? std::string()
                : p.flowed ? button("still", p.id, "Still", true)
                           : button("flow", p.id, "Flow", false);
            const std::string also = also_flowed_html(subject_matches(user, p));
            kept += "<div class=\"card\" id=\"pain" + std::to_string(p.id) + "\"><span class=\"eyebrow\">" + label(p) +
                    "</span><h3 style=\"margin:.1rem 0\">" + html_escape(p.phrase) + "</h3>" + source_link(p) + body +
                    "<p style=\"margin-top:.6rem\"><a href=\"" + html_escape(start) + "\">Start a proposal about this</a> &middot; "
                    "<a href=\"/map?focus=pain:" + std::to_string(p.id) + "\">See it on your map</a></p>" +
                    "<p class=\"muted\">" + flow_note + "</p>" + also +
                    flow_btn + button("forget", p.id, "Forget", true) + "</div>";
        }
        if (!sugg.empty())
            html += "<div class=\"card\"><h2>Noticed</h2><p class=\"muted\">Keep the ones that are real. "
                    "Ones you don't keep disappear after 30 days.</p><ul class=\"plain\">" + sugg + "</ul></div>";
        if (!kept.empty()) html += "<h2>Kept</h2>" + kept;
        if (all.empty() && on && !looked_none)
            html += "<div class=\"card\"><p class=\"empty\">Nothing yet.</p><p class=\"muted\">When you write something like "
                    "&ldquo;I wish the buses ran later&rdquo; in a public post, a proposal or a comment, it shows up here.</p></div>";
        return pg("Problems and solutions", html);
    }
    if(method=="POST"&&(path=="/problems/keep"||path=="/problems/forget"||path=="/problems/flow"||path=="/problems/still")) {
        if(!acc) return need_login();
        auto f = parse_form(body);
        std::uint64_t id = 0;
        const std::string raw = f["id"];
        if (!raw.empty() && raw.size() <= 19 && raw.find_first_not_of("0123456789") == std::string::npos)
            try { id = std::stoull(raw); } catch (...) { id = 0; }
        // Owner-only checks live in the store; someone else's id just does nothing.
        if (id && path == "/problems/keep") g_store.keep_pain(user, id);
        else if (id && path == "/problems/forget") g_store.forget_pain(user, id);
        else if (id && path == "/problems/flow") {
            auto owned = g_store.pain_by_id(id);
            if (owned && owned->owner == user && pain_can_flow(owned->source)) g_store.flow_pain(user, id, true);
        }
        else if (id && path == "/problems/still") g_store.flow_pain(user, id, false);
        const bool stay = id && path != "/problems/forget";
        return http_redir(stay ? "/problems#pain" + std::to_string(id) : std::string("/problems"));
    }
    // Older public writing, only when this person asks. Same Keep/Forget
    // suggestions as a new post. A switch that is off reads nothing.
    if(method=="POST"&&path=="/problems/look") {
        if(!acc) return need_login();
        const int added = look_public_words(user);
        if (added < 0 || added > 0) return http_redir("/problems");
        return http_redir("/problems?looked=none");
    }
    // One flowed subject. The owner is sent back to their private list.
    // Anyone else sees the phrase and who flowed it, and only when that
    // person is visible to them. A Still box looks the same as a missing one.
    if(method=="GET"&&path.rfind("/pain/",0)==0&&path.size()>6&&path.find('/',6)==std::string::npos) {
        if(!acc) return need_login();
        std::uint64_t id = 0;
        const std::string digits = path.substr(6);
        if (digits.size() <= 19 && digits.find_first_not_of("0123456789") == std::string::npos)
            try { id = std::stoull(digits); } catch (...) { id = 0; }
        auto p = id ? g_store.pain_by_id(id) : std::nullopt;
        auto missing = [&]() {
            return http_code("404 Not Found", wrap("Not found", user, unread,
                "<div class=\"card\"><p>No such subject.</p></div>"));
        };
        if (!p) return missing();
        if (p->owner == user) return http_redir("/problems#pain" + std::to_string(p->id));
        if (!p->kept || !p->flowed || !map_person_ok(p->owner, user)) return missing();
        const std::string eyebrow = p->kind == "wish" ? std::string("Want") : std::string("Problem");
        std::string html = "<div class=\"card\"><span class=\"eyebrow\">" + eyebrow +
            "</span><h1 style=\"margin:.1rem 0\">" + html_escape(p->phrase) + "</h1>"
            "<p>Flowed by <a href=\"/u/" + html_escape(p->owner) + "\">" + html_escape(p->owner) + "</a>.</p>";
        html += also_flowed_html(subject_matches(user, *p));
        html += "<p class=\"muted\">This is the subject they chose to show.</p>"
            "<p><a href=\"/map?focus=pain:" + std::to_string(p->id) + "\">See it on the map</a></p></div>";
        return pg(p->phrase, html);
    }
    // One box and its connections, as JSON, for assets/lgmap.js.
    if(method=="GET"&&path=="/map/node") {
        if(!acc) return http_raw("401 Unauthorized","application/json; charset=utf-8","{\"error\":\"login\"}");
        const std::string id = qs.count("n") ? qs.at("n") : std::string("root");
        auto n = map_gnode(id, user);
        if (!n) return http_raw("404 Not Found","application/json; charset=utf-8","{\"error\":\"not found\"}");
        const MapCtx ctx = map_ctx(user); // relevance colours, for this viewer only
        map_add_sub(*n, user);
        std::string out = "{\"node\":" + map_node_json(*n, map_relevance(*n, ctx)) + ",\"links\":[";
        bool first = true;
        for (auto l : map_links(id, user)) {
            if (!first) out += ",";
            first = false;
            map_add_sub(l.to, user);
            out += "{\"node\":" + map_node_json(l.to, map_relevance(l.to, ctx)) + ",\"edge\":\"" + json_escape(l.edge) + "\"}";
        }
        out += "]}";
        return http_raw("200 OK","application/json; charset=utf-8", out, "Cache-Control: no-store\r\n");
    }
    if(method=="GET"&&(path=="/assets/lgmap.js"||path=="/assets/lgmap.css")) {
        const bool js = path=="/assets/lgmap.js";
        std::ifstream in(js ? "assets/lgmap.js" : "assets/lgmap.css", std::ios::binary);
        if(!in) return http_raw("404 Not Found","text/plain; charset=utf-8","not found");
        std::ostringstream ss; ss << in.rdbuf();
        return http_raw("200 OK", js ? "text/javascript; charset=utf-8" : "text/css; charset=utf-8", ss.str(),
                        asset_cache_header(rawpath));
    }
    // One post on its own (the map's right-hand field opens this). Same
    // audience rules as the author's profile, via map_post_ok.
    if(method=="GET"&&path.rfind("/post/",0)==0&&path.size()>6&&path.find('/',6)==std::string::npos) {
        if(!acc) return need_login();
        std::uint64_t pid = 0;
        const std::string digits = path.substr(6);
        if (digits.size() <= 19 && digits.find_first_not_of("0123456789") == std::string::npos)
            try { pid = std::stoull(digits); } catch (...) { pid = 0; }
        auto p = pid ? g_store.post_by_id(pid) : std::nullopt;
        if (!p || !map_post_ok(*p, user))
            return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><p>No such post.</p></div>"));
        return pg("Post", "<div class=\"card\">" + post_block(*p, user) + "</div>");
    }
    if(method=="GET"&&path.rfind("/city/",0)==0&&path.size()>10&&path.compare(path.size()-4,4,"/map")==0) {
        if(!acc) return need_login();
        const std::string raw = url_decode(path.substr(6, path.size()-6-4));
        const std::string key = city_key(raw);
        if (key.empty()) return http_redir("/map");
        if (key != raw) return http_redir("/city/"+url_encode(key)+"/map");
        auto groups = g_store.communities_in_city(key);
        if (groups.empty() && g_store.proposals_in_city(key).empty())
            return http_code("404 Not Found", wrap("No map here yet", user, unread,
                "<h1>Nothing names this city yet</h1><div class=\"card\"><p><a href=\"/map\">MAINstream</a></p></div>"));
        std::string name = groups.empty() ? key : groups.front().city;
        for (const auto& cu : g_store.cities_in_use()) if (cu.key == key) { name = cu.name; break; }
        std::string html = "<p class=\"muted\"><a href=\"/map\">MAINstream</a> &rsaquo; "+html_escape(name)+"</p>"
            "<h1>"+html_escape(name)+"</h1>"
            "<p class=\"lede\">Point at a box to light up its lines. Click it to open it. "
            "<a href=\"/city/"+url_encode(key)+"\">List view</a></p>"
            + city_map_svg(key, name, user) + kCityMapStyle +
            "<p class=\"cmaplegend\"><span class=\"lg-in\">in this city</span><span class=\"lg-member\">member of</span>"
            "<span class=\"lg-touches\">touches</span><span class=\"lg-wrote\">wrote</span>"
            "<span class=\"lg-commented\">commented</span></p>"
            "<p class=\"muted\">Only people who chose to be listed here appear. A city on LiquidGov is a label, not a government.</p>";
        return pg(name+" map", html);
    }
    if(method=="GET"&&path=="/city") return http_redir("/communities"); // the city list lives on Groups
    if(method=="GET"&&path.rfind("/city/",0)==0) {
        if(!acc) return need_login();
        const std::string raw = url_decode(path.substr(6));
        const std::string key = city_key(raw);
        if (key.empty()) return http_redir("/communities");
        if (key != raw) return http_redir("/city/"+url_encode(key)); // /city/Fresno,%20CA -> /city/fresno-ca
        auto groups = g_store.communities_in_city(key);
        if (groups.empty()) {
            // Proposals and tags for a city are kept while no group names it
            // (Brandon 2026-11-06), so point to them instead of hiding them.
            const auto kept = g_store.proposals_in_city(key).size();
            std::string kept_html;
            if (kept > 0) {
                kept_html = "<p>"+std::to_string(kept)+(kept==1?" proposal is":" proposals are")+
                    " still about this city. <a href=\"/proposals?city="+url_encode(key)+"\">See "+
                    (kept==1?"it":"them")+"</a>. They show here again once a group names the city.</p>";
            }
            return http_code(kept > 0 ? "200 OK" : "404 Not Found", wrap("No groups here yet", user, unread,
                "<h1>No groups name this city yet</h1><div class=\"card\">"
                "<p>Anyone can start one. Give it a city when you do, and it shows up here.</p>"+kept_html+
                "<p><a href=\"/communities\">Groups</a></p></div>"));
        }
        std::string name = groups.front().city;
        for (const auto& cu : g_store.cities_in_use()) if (cu.key == key) { name = cu.name; break; }
        std::set<std::string> people;
        for (const auto& c : groups) for (const auto& m : g_store.members_of(c.slug)) people.insert(m);
        std::string html = "<h1>"+html_escape(name)+"</h1>"
            "<p class=\"lede\">People here started these groups. A city on LiquidGov is a label, not a government.</p>"
            "<p><a href=\"/city/"+url_encode(key)+"/map\">Map view</a> &middot; <a href=\"/map\">MAINstream</a></p>"
            + flash_error(qs) +
            "<p class=\"muted\">"+std::to_string(groups.size())+(groups.size()==1?" group":" groups")+" &middot; "+
            std::to_string(people.size())+(people.size()==1?" person":" people")+" in them</p>"
            "<div class=\"card\"><ul class=\"plain\">";
        for (const auto& c : groups) {
            const auto members = g_store.members_of(c.slug).size();
            html += "<li style=\"margin:.6rem 0\"><a href=\"/c/"+html_escape(c.slug)+"\"><strong>"+html_escape(c.name)+"</strong></a> "
                    "<span class=\"muted\">/"+html_escape(c.slug)+" &middot; "+html_escape(category_label(c.category))+
                    (c.subcategory.empty() ? std::string() : " &middot; "+html_escape(c.subcategory))+" &middot; "+
                    std::to_string(members)+(members==1?" member":" members")+
                    (g_store.is_member(c.slug, user) ? " &middot; you're in" : "")+"</span>"+
                    (c.about.empty() ? std::string() : "<p class=\"muted\">"+html_escape(c.about)+"</p>")+"</li>";
        }
        html += "</ul></div>";
        {
            const auto tags = g_store.city_tags(key);
            bool viewer_in_city = false;
            for (const auto& c : groups) if (g_store.is_member(c.slug, user)) { viewer_in_city = true; break; }
            html += "<div class=\"card\"><h2>Departments and councils</h2>"
                    "<p class=\"muted\">Named by people in this city's groups. A proposal can say which ones it touches. "
                    "Nothing here is sent to the city.</p>";
            if (viewer_in_city) {
                html += "<form method=\"POST\" action=\"/city/"+url_encode(key)+"/tag\">"
                        "<label>Add one</label><div class=\"row\">"
                        "<input name=\"tag\" maxlength=\"80\" required placeholder=\"Parks, city council\">"
                        "<button>Add</button></div></form>";
            } else {
                html += "<p class=\"muted\">Join a group in this city to add one.</p>";
            }
            if (tags.empty()) html += "<p class=\"empty\">None named yet.</p>";
            else {
                html += "<ul class=\"plain\">";
                for (const auto& tag : tags) html += "<li>"+html_escape(tag)+"</li>";
                html += "</ul>";
            }
            html += "</div>";

            auto city_props = g_store.proposals_in_city(key);
            std::reverse(city_props.begin(), city_props.end());
            const std::string back = "/city/"+url_encode(key);
            html += "<div class=\"card\"><h2>Proposals</h2>"
                    "<p class=\"muted\">Unofficial straw poll. Discussion only. Nothing is sent to the city.</p>";
            if (city_props.empty()) html += "<p class=\"empty\">None for this city yet.</p>";
            else {
                auto carries = [](const Proposal& p, const std::string& tag) {
                    for (const auto& have : p.tags) if (have == tag) return true;
                    return false;
                };
                for (const auto& tag : tags) {
                    std::vector<Proposal> bucket;
                    for (const auto& p : city_props) if (carries(p, tag)) bucket.push_back(p);
                    if (bucket.empty()) continue;
                    html += "<h3>"+html_escape(tag)+"</h3>";
                    for (const auto& p : bucket) html += proposal_block(p, user, back);
                }
                std::vector<Proposal> untagged;
                for (const auto& p : city_props) if (p.tags.empty()) untagged.push_back(p);
                if (!untagged.empty()) {
                    html += "<h3>Not tagged</h3>";
                    for (const auto& p : untagged) html += proposal_block(p, user, back);
                }
            }
            html += "<p><a href=\"/proposals?city="+url_encode(key)+"\">Open a proposal for this city</a></p></div>";
        }
        html += "<div class=\"card\"><h2>People here</h2>"
                "<p class=\"muted\">People in these groups who chose to be listed. Nobody is listed unless they ask.</p>";
        bool viewer_in = false;
        for (const auto& c : groups) if (g_store.is_member(c.slug, user)) { viewer_in = true; break; }
        if (viewer_in) {
            const bool listed = g_store.is_city_listed(key, user);
            html += "<form method=\"POST\" action=\"/city/"+url_encode(key)+"/list\" style=\"margin:.4rem 0\">"
                    "<input type=\"hidden\" name=\"on\" value=\""+std::string(listed ? "0" : "1")+"\">"
                    "<button>"+std::string(listed ? "Stop listing me" : "List me here")+"</button></form>";
        } else {
            html += "<p class=\"muted\">Join a group in this city to be listed here.</p>";
        }
        auto listed = g_store.people_listed_in_city(key);
        if (listed.empty()) html += "<p class=\"empty\">Nobody has chosen to be listed yet.</p>";
        else {
            html += "<ul class=\"plain\">";
            for (const auto& name : listed) {
                if (name != user && (g_store.is_blocked(user, name) || g_store.is_blocked(name, user))) continue;
                html += "<li class=\"who\" style=\"margin:.4rem 0\">"+portrait_tag(name, "sm")+
                        "<div><a href=\"/u/"+html_escape(name)+"\">"+html_escape(name)+"</a>";
                if (name == user) html += " <span class=\"muted\">you</span>";
                else if (g_store.are_friends(user, name)) html += " <span class=\"muted\">Friends</span>";
                else {
                    html += "<form class=\"inline\" method=\"POST\" action=\"/friends\">"
                            "<input type=\"hidden\" name=\"who\" value=\""+html_escape(name)+"\">"
                            "<input type=\"hidden\" name=\"next\" value=\""+html_escape("/city/"+url_encode(key))+"\">"
                            "<button type=\"submit\">Add friend</button></form>";
                }
                html += "</div></li>";
            }
            html += "</ul>";
        }
        html += "</div><div class=\"card\"><p><a href=\"/communities?city="+url_encode(name)+"\">Start a group in "+
                html_escape(name)+"</a></p><p class=\"muted\">Anyone can start one. There's no minimum size.</p></div>";
        return pg(name, html);
    }

    if (method=="POST" && path.rfind("/city/", 0)==0 && path.size() > 11
        && path.compare(path.size()-5, 5, "/list")==0) {
        if (!acc) return need_login();
        const std::string raw = url_decode(path.substr(6, path.size() - 6 - 5));
        const std::string key = city_key(raw);
        if (key.empty()) return http_redir("/communities");
        const bool on = parse_form(body)["on"] != "0";
        if (!g_store.set_city_listed(key, user, on))
            return http_redir("/city/"+url_encode(key)+"?err=list");
        return http_redir("/city/"+url_encode(key));
    }

    if (method=="POST" && path.rfind("/city/", 0)==0 && path.size() > 10
        && path.compare(path.size()-4, 4, "/tag")==0) {
        if (!acc) return need_login();
        const std::string raw = url_decode(path.substr(6, path.size() - 6 - 4));
        const std::string key = city_key(raw);
        if (key.empty()) return http_redir("/communities");
        const std::string dest = "/city/"+url_encode(key);
        if (!g_store.in_city(key, user)) return http_redir(dest+"?err=ctag");
        if (!g_store.add_city_tag(key, user, parse_form(body)["tag"]))
            return http_redir(dest+"?err=ctagname");
        return http_redir(dest);
    }

    if(method=="GET"&&path=="/notices") {
        if(!acc) return need_login();
        // Not mark_notices_read: a "message" notice is the Messages icon's
        // badge now, not this page's -- clearing it here would make an
        // unread message vanish from its own badge without the message
        // ever having been opened.
        g_store.mark_notices_read_of_type(user, "friend_accept");
        g_store.mark_notices_read_of_type(user, "group_invite");
        return pg("Notices", "<h1>Notices</h1>"+notices_list_html(user));
    }
    // Fetched by the header's Notifications widget on open -- same content
    // as the full page (same helper), as a bare fragment for innerHTML.
    if(method=="GET"&&path=="/notices/panel") {
        if(!acc) return http_code("401 Unauthorized", "");
        g_store.mark_notices_read_of_type(user, "friend_accept");
        g_store.mark_notices_read_of_type(user, "group_invite");
        return http_ok(notices_list_html(user));
    }

    if(method=="GET"&&path=="/settings/password") {
        if(!acc) return need_login();
        // Only this page has password fields (CODE-SCAN-UX B1). The read-only account field gives the password manager
        // the username half of the pair, so it saves and fills here and stops looking elsewhere.
        std::string html = "<h1>Change password</h1>"
            "<p class=\"muted\"><a href=\"/settings\">&lsaquo; Settings</a></p>" + flash_error(qs) +
            "<div class=\"card\"><form method=\"POST\" action=\"/settings/password\">"
            "<label>Account</label><input type=\"text\" name=\"username\" autocomplete=\"username\" value=\""+html_escape(user)+"\" readonly>"
            "<label>Current password</label><input type=\"password\" name=\"current\" required autocomplete=\"current-password\">"
            "<label>New password</label><input type=\"password\" name=\"next\" required autocomplete=\"new-password\">"
            "<button>Change password</button></form></div>";
        return pg("Change password", html);
    }
    if(method=="GET"&&path=="/settings") {
        if(!acc) return need_login();
        auto p = g_store.prefs_of(user);
        auto find_sel = [&](Findability f){ return p.findability==f?" checked":""; };
        std::string html = "<h1>Settings</h1>"
            "<p class=\"muted\">Account, privacy, data, and notifications.</p>"
            // jump links (sweep 2026-11-09): nine sections were one long scroll on a phone
            "<div class=\"setidx\" role=\"navigation\" aria-label=\"Settings sections\"><a href=\"#security\">Security</a><a href=\"#sessions\">Sessions</a><a href=\"#map\">Map and connections</a><a href=\"#privacy\">Privacy</a><a href=\"#blocks\">Block list</a><a href=\"#jukebox\">Jukebox</a><a href=\"#identity\">Identity</a><a href=\"#data\">Data</a><a href=\"#notifications\">Notifications</a></div>";  // a div: the phone CSS hides every <nav> (nav{display:none} is the mobile menu)
        if (qs["ok"]=="password") html += "<p class=\"ok\">Password changed.</p>";
        html += flash_error(qs);

        // Change password is its own page (CODE-SCAN-UX B1, Brief 4 item 9): a password field anywhere on this long
        // page made iOS/Chrome treat all of it as a login form and fill saved credentials into the block, delete and
        // other boxes further down.
        html += "<div class=\"card\"><h2 id=\"security\">Security</h2>"
            "<p><a class=\"btn ghost\" href=\"/settings/password\">Change password</a></p><h2 id=\"sessions\">Sessions</h2>";
        auto all = g_store.sessions_for(acc->id);
        int shown = 0;
        for (const auto& s : all) {
            if (s.revoked) continue;
            if (s.expires_at <= now()) continue;
            ++shown;
            const bool current = s.id == sid;
            html += "<div class=\"sess\"><div><strong>"+html_escape(s.device_label.empty()?"web":s.device_label)+"</strong>"
                    +(current?" <span class=\"muted\">this device</span>":"")+
                    "<div class=\"muted\">Started "+html_escape(fmt_time(s.created_at))+
                    " · last seen "+html_escape(fmt_time(s.last_seen))+"</div></div>";
            if (!current) {
                html += "<form method=\"POST\" action=\"/settings/revoke\">"
                        "<input type=\"hidden\" name=\"sid\" value=\""+html_escape(s.id)+"\">"
                        "<button class=\"danger\" type=\"submit\">Revoke</button></form>";
            }
            html += "</div>";
        }
        if (!shown) html += "<p class=\"muted\">None.</p>";
        html += "<form method=\"POST\" action=\"/settings/logout-all\" style=\"margin-top:1rem\">"
                "<button class=\"danger\" type=\"submit\">Log out other devices</button></form></div>";

        {
            // Map and connections (slice 6): what /map may show about you.
            const auto mp = g_store.map_prefs_of(user);
            auto box = [](const char* name, bool on, const char* label, const char* help) {
                return std::string("<label class=\"row\" style=\"gap:.5rem;align-items:flex-start;margin:.45rem 0\">"
                       "<input type=\"checkbox\" name=\"") + name + "\" value=\"1\"" + (on ? " checked" : "") + "> <span><strong>" +
                       label + "</strong><br><span class=\"muted\">" + help + "</span></span></label>";
            };
            html += "<div class=\"card\" id=\"map-privacy\"><h2 id=\"map\">Map and connections</h2>"
                    "<p class=\"muted\">The <a href=\"/map\">Map</a> connects people, groups, cities and proposals so you can find "
                    "the people worth talking to. Choose what it shows about you. You always see your own.</p>"
                    "<form method=\"POST\" action=\"/settings/map\">"
                    + box("show", mp.show, "Show me on the map", "Off hides you everywhere on the map, even in groups and cities.")
                    + box("groups", mp.groups, "Show the groups I'm in", "Lines from you to your groups.")
                    + box("posts", mp.posts, "Show my posts", "Posts others can already see, as boxes on the map.")
                    + box("comments", mp.comments, "Show what I commented on", "Lines from you to proposals you commented on.")
                    + box("friends", mp.friends, "Show my friends", "Off by default. A friendship shows to others only when both of you turn this on.")
                    + box("pains", mp.pains, "Notice my problems and wants",
                          "Reads your new posts, proposals, proposal comments, and messages you sent for phrases like "
                          "&ldquo;I wish&hellip;&rdquo; or &ldquo;I hate when&hellip;&rdquo;, and suggests them to you. "
                          "Friends posts, group posts, and messages stay on your map only. The other person's messages are not read. "
                          "On your problems page, Look at what I've written reads older writing of those same kinds when you ask. "
                          "Only you see the private ones. Turning this off deletes everything it found. "
                          "Note: the site's operator can read what is stored on the server.")
                    + "<p class=\"muted\"><a href=\"/problems\">See your problems and solutions</a></p>"
                    + "<button>Save map settings</button></form></div>";
        }
        html += std::string("<div class=\"card\"><h2 id=\"privacy\">Privacy</h2><form method=\"POST\" action=\"/settings/privacy\">")
            + "<label>Who can find you</label>"
            + "<p class=\"muted\">One axis, three states. Each level includes the one above it.</p>"
            + "<p><label><input type=\"radio\" name=\"findability\" value=\"0\"" + find_sel(Findability::FriendsOnly) + "> Friends only</label>"
            + "<br><span class=\"muted\">Only people who are already your friends can find you. Strangers can't search you up at all.</span></p>"
            + "<p><label><input type=\"radio\" name=\"findability\" value=\"1\"" + find_sel(Findability::ExactMatch) + "> Exact match</label>"
            + "<br><span class=\"muted\">Anyone who types your exact username can find you, but you won't show up as they type.</span></p>"
            + "<p><label><input type=\"radio\" name=\"findability\" value=\"2\"" + find_sel(Findability::Typeahead) + "> Typeahead</label>"
            + "<br><span class=\"muted\">Anyone can find you, including as a partial match while they're still typing.</span></p>"
            + "<p><label><input type=\"checkbox\" name=\"group_invite_confirm\" value=\"1\"" + (p.group_invite_confirm?" checked":"") + "> "
            + "Don't add me to groups without asking</label></p>"
            + "<p class=\"muted\">Direct messages are friends-only until we can tell who is a minor without a checkbox. "
            + "Unfriending or blocking leaves every group conversation you share with that person. "
            + "Messages on this copy are stored so the operator can read them.</p>"
            + "<label>Visibility register</label>"
            + "<p class=\"muted\">Each level admits every level after it too: public sees everything members do, "
            + "members see everything friends do, friends see everything only-me hides from everyone else.</p>";
        auto vis_select = [&](const char* field, Visibility current) {
            auto opt = [&](Visibility v, const char* label) {
                return std::string("<option value=\"") + std::to_string(static_cast<int>(v)) + "\""
                    + (current == v ? " selected" : "") + ">" + label + "</option>";
            };
            return std::string("<select name=\"") + field + "\">"
                + opt(Visibility::Public, "Public")
                + opt(Visibility::Members, "Members")
                + opt(Visibility::Friends, "Friends")
                + opt(Visibility::OnlyMe, "Only me")
                + "</select>";
        };
        html += "<table class=\"register\"><tr><th>Surface</th><th>Who can see it</th></tr>"
                "<tr><td>Profile</td><td>" + vis_select("profile_visibility", p.profile_visibility) + "</td></tr>"
                "<tr><td>Posts</td><td>" + vis_select("posts_visibility", p.posts_visibility) + "</td></tr>"
                "<tr><td></td><td><span class=\"muted\">Each post on /feed now picks its own reach "
                "(Anybody/Friends/Groups) -- this row is the fallback for Groups-only posts on your "
                "profile, and Only me still hides everything from everyone but you.</span></td></tr>"
                "</table>"
                "<button>Save privacy</button></form><h2 id=\"blocks\">Block list</h2>";
        auto bl = g_store.block_entries_of(user);
        html += "<p class=\"muted eyebrow\">"+std::to_string(bl.size())+
                (bl.size()==1?" member":" members")+" &middot; not visible to them</p>";
        if (bl.empty()) html += "<p class=\"empty\">No one blocked.</p>";
        for (auto& b : bl) {
            html += "<div class=\"person\"><div><span>"+html_escape(b.other)+"</span>"
                    "<div class=\"muted\">Blocked "+html_escape(fmt_time(b.at))+
                    (b.reason.empty() ? "" : " · " + html_escape(b.reason))+"</div></div>"
                    "<form method=\"POST\" action=\"/settings/unblock\">"
                    "<input type=\"hidden\" name=\"who\" value=\""+html_escape(b.other)+"\">"
                    "<button class=\"ghost\">Unblock</button></form></div>";
        }
        html += "<form method=\"POST\" action=\"/settings/block\" style=\"margin-top:.75rem\">"
                "<div class=\"row\"><input name=\"who\" placeholder=\"account name\" autocomplete=\"off\" required style=\"flex:1\">"
                "<button class=\"danger\" type=\"submit\">Add block</button></div>"
                "<input name=\"reason\" placeholder=\"Reason (optional, only you see it)\" maxlength=\"200\"></form>";
        {
            // Moved off the profile page: who can queue a track on your
            // jukebox is an access-control setting like block list and
            // visibility above, not something that belongs on a page
            // visitors see (Brandon: "its not appropriate" there).
            auto jsel = [&](JukeboxPolicy pol) -> std::string { return p.jukebox_policy==pol?" checked":""; };
            html += "<h2 id=\"jukebox\">Jukebox</h2>"
                    "<form method=\"POST\" action=\"/me/jukebox/policy\">"
                    "<label>Who else can queue a track on your jukebox</label>"
                    "<p><label><input type=\"radio\" name=\"policy\" value=\"0\""+jsel(JukeboxPolicy::OwnerOnly)+"> Just me</label></p>"
                    "<p><label><input type=\"radio\" name=\"policy\" value=\"1\""+jsel(JukeboxPolicy::Friends)+"> Friends</label></p>"
                    "<p><label><input type=\"radio\" name=\"policy\" value=\"2\""+jsel(JukeboxPolicy::Anyone)+"> Anyone signed in</label></p>"
                    "<p class=\"muted\">One request at a time per person; it plays once, then it's gone. "
                    "Anyone you've blocked above can never queue, regardless.</p>"
                    "<button>Save</button></form>";
        }
        html += "</div>";

        html += "<div class=\"card\"><h2 id=\"identity\">Identity</h2>"
            "<p>No civic identity is linked to this account.</p>"
            "<p class=\"muted\">Voting is not live yet. Linking a citizen identity later will not happen from this page until it is.</p></div>";

        html += "<div class=\"card\"><h2 id=\"data\">Data</h2>"
            "<p><a class=\"btn ghost\" href=\"/export\">Download your export</a></p>"
            "<p class=\"muted\">Export includes username, blurb, profile CSS, your friend list, groups you started or joined, "
            "posts you wrote, settings, and messages you sent. It does not include other people’s text or your password hash.</p>"
            "<form method=\"POST\" action=\"/settings/delete\">"
            // the name only in the label, never as a placeholder (CODE-SCAN-UX B1: a keychain-style fill target)
            "<label>Delete account — type your account name, <strong>"+html_escape(user)+"</strong></label>"
            "<input name=\"confirm\" autocomplete=\"off\">"
            "<button class=\"danger\">Delete account</button></form>"
            "<p class=\"muted\">Sessions end immediately. Mail you sent is shown as [deleted account]. "
            "A fingerprint of the phone number is held ninety days so a deleted account cannot immediately reopen under the same number.</p></div>";

        html += std::string("<div class=\"card\"><h2 id=\"notifications\">Notifications</h2><form method=\"POST\" action=\"/settings/notify\">")
            + "<p><label><input type=\"checkbox\" name=\"notify_friend\" value=\"1\"" + (p.notify_friend?" checked":"") + "> Friend requests and accepts</label></p>"
            + "<p><label><input type=\"checkbox\" name=\"notify_message\" value=\"1\"" + (p.notify_message?" checked":"") + "> New mail</label></p>"
            + "<p><label><input type=\"checkbox\" name=\"notify_group\" value=\"1\"" + (p.notify_group?" checked":"") + "> Group invites</label></p>"
            + "<p class=\"muted\">Login alerts stay on.</p>"
            + "<p><label><input type=\"checkbox\" name=\"intro_every_visit\" value=\"1\"" + (p.intro_every_visit?" checked":"") + "> Play the landing intro every visit</label></p>"
            + "<p class=\"muted\">Off plays it just the first time you land on the site, on this browser.</p>"
            + "<button>Save notifications</button></form></div>";
        return pg("Settings", html);
    }

    if(method=="GET"&&path=="/export") {
        if(!acc) return need_login();
        std::ostringstream o;
        o << "LiquidGov.US platform export\n";
        o << "This is platform data. It is not the governance kernel.\n\n";
        o << "username: " << user << "\n";
        o << "created_at: " << fmt_time(acc->created_at) << "\n";
        o << "blurb: " << g_store.blurb(acc->id) << "\n";
        o << "profile css:\n" << g_store.author_css(acc->id) << "\n\n";
        o << "friends:\n";
        for (auto& n : g_store.friends_of(user)) o << "  " << n << "\n";
        o << "\ncommunities:\n";
        for (auto& c : g_store.communities_of(user)) o << "  " << c.slug << " " << c.name << "\n";
        o << "\nposts you wrote:\n";
        for (auto& p : g_store.posts_by(user)) {
            o << "  [" << fmt_time(p.ts) << "] ";
            if (!p.community.empty()) o << "/" << p.community << " ";
            o << p.text << "\n";
        }
        o << "\nmessages you sent:\n";
        for (auto& m : g_store.mail_sent_by(user)) {
            o << "  to " << m.to << " [" << fmt_time(m.ts) << "] " << m.text << "\n";
        }
        auto pr = g_store.prefs_of(user);
        o << "\nsettings:\n";
        o << "  findability: " << static_cast<int>(pr.findability) << "\n";
        o << "  group_invite_confirm: " << (pr.group_invite_confirm?"yes":"no") << "\n";
        std::string extra = "Content-Disposition: attachment; filename=\"liquidgov-export.txt\"\r\n";
        extra += "Cache-Control: no-store\r\n";
        return http_raw("200 OK", "text/plain; charset=utf-8", o.str(), extra);
    }

    if(method=="POST"&&path=="/signup") {
        auto f=parse_form(body);
        SignupDraft d;
        d.username=trim_ws(f["username"]);
        d.phone=trim_ws(f["phone"]);
        d.email=trim_ws(f["email"]);
        d.display_name=trim_ws(f["display_name"]);
        d.hometown=trim_ws(f["hometown"]);
        d.living=trim_ws(f["living"]);
        d.bio=trim_ws(f["bio"]);
        d.interests=trim_ws(f["interests"]);
        d.career=trim_ws(f["career"]);
        d.school=trim_ws(f["school"]);
        auto pass=f["password"];
        auto fail=[&](const std::string& err){ return http_ok(signup_page(user, unread, err, d)); };
        if (!valid_username(d.username) || d.username.size() < 3) {
            std::string err = d.username.empty()
                ? "Need a username: 3–32 letters/numbers (dots, underscores, hyphens ok). No spaces, no @."
                : d.username.size() < 3
                    ? "Username needs at least three characters."
                    : ("“"+d.username+"” isn’t a valid username. Letters, numbers, dots, underscores, hyphens. No spaces.");
            return fail(err);
        }
        if (d.display_name.empty() || d.display_name.size() > 80)
            return fail("Need a display name (up to 80 characters).");
        if (pass.size() < 8)
            return fail("Password needs at least eight characters.");
        if (!looks_like_email(d.email))
            return fail("Need a real email address, kept only so you can reset your password if you forget it.");
        if (count_digits(d.phone) < 10)
            return fail("Need a real phone number (at least ten digits). It is a signup binding, not stored as the number itself, and not used to log in.");
        if (pass.empty() || d.phone.empty())
            return fail("Need username, password, email, phone, and a display name.");
        if (g_store.phone_quarantined(fingerprint_phone(d.phone)))
            return fail("Could not create that account.");
        auto r=create_account(g_store,g_breach,d.username,pass,d.phone);
        if(r.error==AuthError::None) {
            if (auto created = g_store.find_by_username(d.username)) {
                g_store.set_phone_fp(created->id, fingerprint_phone(d.phone));
                if (!d.bio.empty()) g_store.set_blurb(created->id, d.bio.substr(0, 500));
                g_store.set_email(d.username, lowercase_ascii(d.email));
                Profile p;
                p.display_name = d.display_name.substr(0, 80);
                p.email_fp = fingerprint_phone(d.email);
                p.hometown = d.hometown.substr(0, 80);
                p.living = d.living.substr(0, 80);
                p.interests = d.interests.substr(0, 200);
                p.career = d.career.substr(0, 80);
                p.school = d.school.substr(0, 80);
                g_store.set_profile(d.username, p);
                g_store.set_map_prefs(d.username, MapPrefs{}); // today's defaults written down: no "now show" note later
            }
            r=login(g_store,g_limiter,d.username,pass,device_label_from(req,ip),ip);
        }
        if(r.error==AuthError::UsernameTaken)
            return fail("That username is taken. Pick another.");
        if(r.error==AuthError::PasswordTooShort)
            return fail("Password needs at least eight characters.");
        if(r.error!=AuthError::None||!r.session)
            return fail("Could not create that account.");
        return http_redir("/", set_cookie(r.session->id));
    }
    if(method=="POST"&&path=="/login") {
        auto f=parse_form(body);
        auto uname = trim_ws(f["username"]);
        auto r=login(g_store,g_limiter,uname,f["password"],device_label_from(req,ip),ip);
        if(r.error!=AuthError::None||!r.session) {
            const char* code = (r.error==AuthError::RateLimited) ? "ratelimit" : "login";
            std::string loc = "/login?err=" + std::string(code) + "&username=" + url_encode(uname);
            auto next = safe_next_path(trim_ws(f["next"]));
            if (!next.empty()) loc += "&next=" + url_encode(next);
            return http_redir(loc);
        }
        auto dest = safe_next_path(trim_ws(f["next"]));
        if (dest.empty()) dest = "/feed";
        return http_redir(dest, set_cookie(r.session->id));
    }
    // Always the same response whether or not the address is on an
    // account -- checked deliberately before ever looking anything up,
    // not just by coincidence of the code path. Never let mail-delivery
    // status (or even the timing of a real lookup+send vs. a no-op)
    // leak back to the caller; that's exactly how account enumeration
    // via a password-reset form usually happens.
    if(method=="POST"&&path=="/forgot") {
        auto f = parse_form(body);
        std::string email = lowercase_ascii(trim_ws(f["email"]));
        if (looks_like_email(email)) {
            if (auto username = g_store.username_by_email(email)) {
                std::string token = generate_session_id(); // same crypto-random 32-byte-hex primitive as a session id
                g_store.set_reset_token(*username, sha256_hex_of(token), now() + 3600);
                send_password_reset_email(email, "https://liquidgov.us/reset?t=" + token);
            }
        }
        return pg("Forgot password", "<div class=\"card\"><h1>Check your email</h1>"
            "<p class=\"muted\">If that address is on an account, a link is on its way. "
            "It works once, for the next hour.</p>"
            "<p class=\"muted\"><a href=\"/login\">Back to log in</a></p></div>");
    }
    if(method=="POST"&&path=="/reset") {
        auto f = parse_form(body);
        std::string t = trim_ws(f["t"]);
        std::string pass = f["password"];
        if (t.empty()) return http_redir("/reset?err=reset");
        if (pass.size() < 8) return http_redir("/reset?t="+url_encode(t)+"&err=short");
        if (g_breach.is_breached(pass)) return http_redir("/reset?t="+url_encode(t)+"&err=password");
        // consume_reset_token erases it either way (found-and-expired or
        // found-and-valid) -- a token is good for exactly one attempt,
        // successful or not, same as any other one-time credential.
        auto username = g_store.consume_reset_token(sha256_hex_of(t));
        if (!username) return http_redir("/reset?err=reset");
        auto acc2 = g_store.find_by_username(*username);
        if (!acc2) return http_redir("/reset?err=reset");
        g_store.set_password_hash(acc2->id, hash_password(pass));
        logout_all_devices(g_store, acc2->id, "");
        auto session = create_session(g_store, acc2->id, device_label_from(req, ip));
        return http_redir("/", set_cookie(session.id));
    }
    // Google Identity Services posts here directly (data-ux_mode="redirect"
    // in google_signin_button_html()) -- no custom JS callback, just a
    // plain form POST carrying the ID token as "credential". Verified
    // against Google's own tokeninfo endpoint (a plain HTTPS GET, no JWT/
    // JWKS library needed) rather than trusting anything the browser says
    // about itself.
    if(method=="POST"&&path=="/login/google") {
        const char* client_id = google_client_id_from_env();
        if (!client_id[0]) return http_redir("/login?err=google");
        auto f = parse_form(body);
        std::string credential = trim_ws(f["credential"]);
        if (credential.empty()) return http_redir("/login?err=google");

        int curl_exit = 0;
        std::string resp = curl_get(
            "https://oauth2.googleapis.com/tokeninfo?id_token=" + url_encode(credential), 8, &curl_exit);
        // nothing came back (no network, timeout): say Google could not be reached, not that the sign-in was bad
        if (curl_exit != 0 && resp.empty()) return http_redir("/login?err=google-down");
        if (resp.empty() || resp[0] != '{') return http_redir("/login?err=google");

        std::string aud = oembed_json_string_field(resp, "aud");
        std::string iss = oembed_json_string_field(resp, "iss");
        std::string sub = oembed_json_string_field(resp, "sub");
        std::string email = lowercase_ascii(oembed_json_string_field(resp, "email"));
        std::string email_verified = oembed_json_string_field(resp, "email_verified");

        if (aud != client_id || sub.empty()) return http_redir("/login?err=google");
        if (iss != "accounts.google.com" && iss != "https://accounts.google.com")
            return http_redir("/login?err=google");
        if (email_verified != "true") return http_redir("/login?err=google");

        const std::string device_label = device_label_from(req, ip);

        // gid hit: this Google identity is already linked to an account.
        if (auto existing = g_store.username_by_google_sub(sub)) {
            auto acc2 = g_store.find_by_username(*existing);
            if (!acc2) return http_redir("/login?err=google");
            auto session = create_session(g_store, acc2->id, device_label);
            return http_redir("/", set_cookie(session.id));
        }
        // gid miss, ml hit: same email as an existing password account --
        // link it (never auto-link on anything looser than a verified
        // email match).
        if (!email.empty()) {
            if (auto linked = g_store.username_by_email(email)) {
                auto acc2 = g_store.find_by_username(*linked);
                if (acc2) {
                    g_store.set_google_id(*linked, sub);
                    auto session = create_session(g_store, acc2->id, device_label);
                    return http_redir("/", set_cookie(session.id));
                }
            }
        }
        // Neither: brand new account, no phone required on this path
        // (bypasses create_account()'s phone-required contract on
        // purpose -- see username_from_email_local_part's own comment --
        // by calling the raw AccountStore::create directly, same as
        // every other FileStore-only account field this file sets right
        // after signup).
        PlatformAccount account;
        account.username = username_from_email_local_part(email.empty() ? sub : email);
        account.password_hash = dummy_hash();
        account.phone_verified = false;
        account.created_at = now();
        AccountId new_id = g_store.create(account);
        if (!email.empty()) g_store.set_email(account.username, email);
        g_store.set_google_id(account.username, sub);
        g_store.set_map_prefs(account.username, MapPrefs{}); // today's defaults written down: no "now show" note later
        auto session = create_session(g_store, new_id, device_label);
        return http_redir("/", set_cookie(session.id));
    }
    if(method=="POST"&&path=="/me/post") {
        if(!acc) return need_login();
        auto hdr_end = req.find("\r\n\r\n");
        std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
        auto c = read_composer_post(hdrs, body);
        if (!c.ok) return http_redir(std::string("/me?err=")+c.err);
        if (!c.text.empty() || !c.attachment_blob_id.empty())
            g_store.add_post(user, c.text, "", "", 0, c.attachment_blob_id, c.attachment_mime);
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/me/repost") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        std::uint64_t pid = 0;
        try { pid = std::stoull(f["post_id"]); } catch (...) { return http_redir("/me"); }
        auto target = g_store.post_by_id(pid);
        if (!target) return http_redir("/me");
        // A repost of a repost still credits the true original author, not
        // the intermediate reposter -- see the repost_of_author comment on
        // Post.
        const std::string& origAuthor = target->repost_of_author.empty() ? target->author : target->repost_of_author;
        std::uint64_t origId = target->repost_of_author.empty() ? target->id : target->repost_of_id;
        if (g_store.is_blocked(user, origAuthor)) return http_redir("/me");
        std::string blob_id, mime;
        if (!target->attachment_blob_id.empty()) {
            // put() is content-addressed and idempotent -- re-putting the
            // same bytes lands on the exact same blob id while correctly
            // incrementing its refcount for this new post referencing it.
            auto bytes = g_blobs.get(target->attachment_blob_id);
            if (bytes) {
                blob_id = g_blobs.put(*bytes, target->attachment_mime);
                mime = target->attachment_mime;
            }
        }
        g_store.add_post(user, target->text, "", origAuthor, origId, blob_id, mime);
        return http_redir(back_to_post(header_value(req, "Referer"), pid)); // the page it was tapped on (was /me)
    }
    // Reply and second both need to land back on whatever page the post was
    // actually being viewed on (feed, a profile, a community) -- post_block
    // renders in all of those, so unlike most POST handlers here a fixed
    // redirect target would be wrong more often than not. Referer, same
    // pattern the theme toggle already uses, falling back to /feed.
    if(method=="POST"&&path.rfind("/post/",0)==0&&path.size()>12&&
       path.compare(path.size()-6,6,"/reply")==0) {
        if(!acc) return need_login();
        std::string back = header_value(req, "Referer");
        if (back.empty() || back.rfind("http",0)!=0) back = "/feed";
        std::uint64_t pid = 0;
        try { pid = std::stoull(path.substr(6, path.size()-6-6)); } catch (...) { return http_redir(back); }
        if (!g_store.post_by_id(pid)) return http_redir(back);
        auto text = trim_ws(parse_form(body)["text"]);
        if (!text.empty()) g_store.add_reply(pid, user, text);
        return http_redir(back_to_post(back, pid));
    }
    if(method=="POST"&&path.rfind("/post/",0)==0&&path.size()>13&&
       path.compare(path.size()-7,7,"/second")==0) {
        if(!acc) return need_login();
        std::string back = header_value(req, "Referer");
        if (back.empty() || back.rfind("http",0)!=0) back = "/feed";
        std::uint64_t pid = 0;
        try { pid = std::stoull(path.substr(6, path.size()-6-7)); } catch (...) { return http_redir(back); }
        if (!g_store.post_by_id(pid)) return http_redir(back);
        g_store.toggle_second(pid, user);
        return http_redir(back_to_post(back, pid));
    }
    if(method=="POST"&&path.rfind("/post/",0)==0&&path.size()>13&&
       path.compare(path.size()-7,7,"/delete")==0) {
        if(!acc) return need_login();
        std::string back = header_value(req, "Referer");
        if (back.empty() || back.rfind("http",0)!=0) back = "/feed";
        std::uint64_t pid = 0;
        try { pid = std::stoull(path.substr(6, path.size()-6-7)); } catch (...) { return http_redir(back); }
        auto target = g_store.post_by_id(pid);
        if (!target) return http_redir(back); // already gone
        if (target->author != user) return http_code("403 Forbidden", "not your post");
        g_store.delete_post(pid, user);
        return http_redir(back);
    }
    if(method=="POST"&&path=="/me/profile") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        auto display = trim_ws(f["display_name"]);
        if (display.empty() || display.size() > 80) return http_redir("/me?err=profile");
        Profile p = g_store.profile_of(user);
        p.display_name = display;
        p.hometown = trim_ws(f["hometown"]).substr(0, 80);
        p.living = trim_ws(f["living"]).substr(0, 80);
        p.interests = trim_ws(f["interests"]).substr(0, 200);
        p.career = trim_ws(f["career"]).substr(0, 80);
        p.school = trim_ws(f["school"]).substr(0, 80);
        g_store.set_profile(user, p);
        g_store.set_blurb(acc->id, trim_ws(f["bio"]).substr(0, 500));
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/me/css") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        const std::string& raw_css = f["css"];
        auto check = sanitize_author_css(raw_css);
        if (!check.ok) return http_redir("/me?err=css&reason=" + check.code);
        g_store.set_author_css(acc->id, raw_css);
        return http_redir("/me?ok=css");
    }
    if(method=="POST"&&path=="/me/avatar") {
        if(!acc) return need_login();
        auto hdr_end = req.find("\r\n\r\n");
        std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
        auto file = multipart_file(header_value(hdrs, "Content-Type"), body, "photo");
        if (!file || file->empty()) return http_redir("/me?err=image");
        auto r = save_avatar(g_blobs, g_store, g_scanner, acc->id, *file);
        if (r.error == SaveAvatarError::TooLarge) return http_redir("/me?err=portraitlarge");
        if (r.error == SaveAvatarError::FailedScan) return http_redir("/me?err=scan");
        if (r.error != SaveAvatarError::None) return http_redir("/me?err=image");
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/me/avatar/clear") {
        if(!acc) return need_login();
        clear_avatar(g_blobs, g_store, acc->id);
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/me/canvas/clear") {
        if(!acc) return need_login();
        g_store.clear_canvas_frames(acc->id);
        return http_redir("/me/canvas");
    }
    if(method=="POST"&&path=="/me/canvas/frame") {
        if(!acc) return need_login();
        // Reuses save_avatar's exact normalize/scan/size-cap pipeline (the
        // security-relevant part) via a throwaway AvatarStore that just
        // captures the resulting blob id instead of writing the account's
        // real single avatar slot -- canvas frames are an ordered list,
        // not a single value, so FileStore's own AvatarStore side can't be
        // reused directly for this.
        struct CaptureBlobId : AvatarStore {
            std::optional<Avatar> get(AccountId) const override { return std::nullopt; }
            void set(const Avatar& a) override { blob_id = a.blob_id; }
            void erase(AccountId) override {}
            std::string blob_id;
        } capture;
        auto hdr_end = req.find("\r\n\r\n");
        std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
        auto file = multipart_file(header_value(hdrs, "Content-Type"), body, "frame");
        if (!file || file->empty()) return http_redir("/me/canvas?err=frameimage");
        if (g_store.canvas_frames(acc->id).size() >= kCanvasMaxFrames)
            return http_redir("/me/canvas?err=framefull");
        auto r = save_avatar(g_blobs, capture, g_scanner, acc->id, *file);
        if (r.error == SaveAvatarError::TooLarge) return http_redir("/me/canvas?err=framelarge");
        if (r.error != SaveAvatarError::None) return http_redir("/me/canvas?err=frameimage");
        g_store.add_canvas_frame(acc->id, capture.blob_id);
        return http_ok("ok");
    }
    if(method=="POST"&&path=="/me/canvas/mode") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        g_store.set_canvas_timelapse(acc->id, f["timelapse"]=="1");
        return http_redir("/me/canvas?ok=1");
    }
    if(method=="POST"&&path=="/me/jukebox/add") {
        if(!acc) return need_login();
        auto url = trim_ws(parse_form(body)["url"]);
        auto id = extract_youtube_id(url);
        if (!id) return http_redir("/me?err=url");
        if (!g_store.add_jukebox_track(user, *id, user)) return http_redir("/me?err=full");
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/me/jukebox/remove") {
        if(!acc) return need_login();
        g_store.remove_jukebox_track(user, parse_form(body)["video_id"]);
        return http_redir("/me");
    }
    if(method=="POST"&&path=="/me/jukebox/play-settings") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        g_store.set_jukebox_autoplay(user, f["autoplay"]=="1");
        g_store.set_jukebox_relay_target(user, trim_ws(f["relay"]));
        return http_redir("/me?ok=jplay");
    }
    if(method=="POST"&&path=="/me/jukebox/policy") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        int pv = 0;
        try { pv = std::stoi(f["policy"]); } catch (...) { pv = 0; }
        if (pv < 0 || pv > 2) pv = 0;
        auto p = g_store.prefs_of(user);
        p.jukebox_policy = static_cast<JukeboxPolicy>(pv);
        g_store.set_prefs(user, p);
        return http_redir("/settings");
    }
    // A guest's one-shot request on someone else's jukebox. Gated by the
    // owner's JukeboxPolicy, the owner's block list, and one outstanding
    // request per guest -- add_jukebox_track itself only enforces the
    // account/id/cap checks it always has. Also reachable by the owner
    // themselves (posting from their own /u/<username> page rather than
    // /me), in which case none of the guest gates apply -- same as /me/jukebox/add.
    if(method=="POST"&&path.rfind("/u/",0)==0&&path.size()>15&&
       path.compare(path.size()-12,12,"/jukebox/add")==0) {
        const std::string who = path.substr(3, path.size()-3-12);
        if(!acc) return need_login();
        if (!valid_username(who)) return http_redir("/");
        auto found = g_store.find_by_username(who);
        if (!found) return http_redir("/");
        if (who != user) {
            if (g_store.is_blocked(who, user)) return http_redir("/u/"+who+"?err=denied");
            auto policy = g_store.prefs_of(who).jukebox_policy;
            bool allowed = policy == JukeboxPolicy::Anyone ||
                           (policy == JukeboxPolicy::Friends && g_store.are_friends(user, who));
            if (!allowed) return http_redir("/u/"+who+"?err=denied");
            for (const auto& t : g_store.jukebox_of(who)) {
                if (t.queued_by == user) return http_redir("/u/"+who+"?err=pending");
            }
        }
        auto url = trim_ws(parse_form(body)["url"]);
        auto id = extract_youtube_id(url);
        if (!id) return http_redir("/u/"+who+"?err=url");
        if (!g_store.add_jukebox_track(who, *id, user)) return http_redir("/u/"+who+"?err=full");
        return http_redir("/u/"+who);
    }
    // Fired by the floating widget's own player when a personal channel's
    // current track actually finishes -- see pollPersonal's onStateChange
    // handling in jukebox_script. complete_jukebox_track looks up the
    // track's own stored queued_by itself (never trusts anything from the
    // client beyond the video id), so this is safe to call from anyone
    // signed in, owner or visitor. Resolved through the same one-hop
    // relay as /jukebox/now -- a completion has to land on whichever
    // queue is actually playing, not the page it was fired from.
    if(method=="POST"&&path.rfind("/u/",0)==0&&path.size()>20&&
       path.compare(path.size()-17,17,"/jukebox/complete")==0) {
        const std::string who = path.substr(3, path.size()-3-17);
        if(!acc) return need_login();
        if (valid_username(who)) {
            auto jf = parse_form(body);
            const auto jb_owner = g_store.jukebox_effective_owner(who);
            if (jf["skip"] == "1") g_store.skip_jukebox_track(jb_owner, jf["video_id"]);
            else g_store.complete_jukebox_track(jb_owner, jf["video_id"]);
        }
        return http_ok("ok");
    }
    // The polling widget's substitute for a WebSocket connect/disconnect
    // on a personal station (see personal_heartbeat_locked/
    // personal_freeze_locked above) -- a personal jukebox has never had a
    // live socket. Signed-in only, unlike listening itself (open to any
    // visitor who can view the profile): an anonymous visitor can still
    // hear it, it just never counts toward who's "still here" for
    // freeze/resume. video_id must match what's actually at the front of
    // the effective queue right now -- same distrust as complete_
    // jukebox_track, so a stale or mismatched client can't stomp another
    // track's saved position.
    if(method=="POST"&&path.rfind("/u/",0)==0&&path.size()>15&&
       path.compare(path.size()-12,12,"/jukebox/pos")==0) {
        const std::string who = path.substr(3, path.size()-3-12);
        if(!acc) return need_login();
        if (!valid_username(who)) return http_ok("ok");
        auto found_pos = g_store.find_by_username(who);
        if (!found_pos) return http_ok("ok");
        const std::string effective_owner = g_store.jukebox_effective_owner(who);
        auto tracks = g_store.jukebox_of(effective_owner);
        if (tracks.empty()) return http_ok("ok");
        auto form = parse_form(body);
        if (form["video_id"] != tracks.front().video_id) return http_ok("ok");
        const std::string station = "u:" + effective_owner;
        const std::string sid = cookie_of(req);
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        std::lock_guard<std::mutex> lock(g_personal_mu);
        if (form["freeze"] == "1") {
            std::optional<std::int64_t> client_offset;
            if (!form["offset"].empty()) {
                try { client_offset = std::stoll(form["offset"]); } catch (...) {}
            }
            personal_freeze_locked(station, tracks.front().video_id, sid, client_offset, now);
        } else {
            personal_heartbeat_locked(station, tracks.front().video_id, sid, now);
        }
        return http_ok("ok");
    }
    // A signed-in viewer follows someone's personal jukebox as a "channel"
    // in their own floating widget (jukebox_script's Follow button). This
    // is a real, server-recorded subscription -- not just this device's
    // own station list -- specifically so the owner can see how many
    // people are tuned in (jukebox_widget's subscriber count, owner_view
    // only). Listening itself was already open to anyone who could view
    // the profile; the same profile_visibility/block gates as the page
    // itself apply here so this can't be used to probe a private profile.
    if(method=="POST"&&path.rfind("/u/",0)==0&&path.size()>21&&
       path.compare(path.size()-18,18,"/jukebox/subscribe")==0) {
        const std::string who = path.substr(3, path.size()-3-18);
        if(!acc) return http_code("401 Unauthorized", "");
        if (!valid_username(who) || who == user) return http_code("404 Not Found", "");
        auto found = g_store.find_by_username(who);
        if (!found) return http_code("404 Not Found", "");
        if (g_store.is_blocked(who, user)) return http_code("403 Forbidden", "");
        auto target_prefs = g_store.prefs_of(who);
        if (!visible_to(target_prefs.profile_visibility, false, true, g_store.are_friends(user, who)))
            return http_code("403 Forbidden", "");
        g_store.subscribe_jukebox(user, who);
        return http_ok("ok");
    }
    if(method=="POST"&&path.rfind("/u/",0)==0&&path.size()>23&&
       path.compare(path.size()-20,20,"/jukebox/unsubscribe")==0) {
        const std::string who = path.substr(3, path.size()-3-20);
        if(!acc) return http_code("401 Unauthorized", "");
        if (valid_username(who)) g_store.unsubscribe_jukebox(user, who);
        return http_ok("ok");
    }
    if(method=="POST"&&path=="/feed") {
        if(!acc) return need_login();
        auto hdr_end = req.find("\r\n\r\n");
        std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
        auto c = read_composer_post(hdrs, body);
        if (!c.ok) return http_redir(std::string("/feed?err=")+c.err);
        auto text = trim_ws(c.text);
        if (!text.empty() || !c.attachment_blob_id.empty()) {
            // anybody/friends/groups and the except/friend/group lists come
            // from feed_composer_card's own hidden fields and checkbox
            // groups -- absent (JS-off, or an older cached page) means the
            // same static defaults that markup ships with: Friends, all
            // friends, no group, exactly today's one and only behavior
            // before this existed.
            std::string ctype = header_value(hdrs, "Content-Type");
            bool anybody, friends_on, groups_on;
            std::vector<std::string> except_raw, friend_raw, group_raw;
            if (ctype.find("multipart/form-data") != std::string::npos) {
                anybody = multipart_field(ctype, body, "anybody") == "1";
                friends_on = multipart_field(ctype, body, "friends") == "1";
                groups_on = multipart_field(ctype, body, "groups") == "1";
                except_raw = multipart_field_all(ctype, body, "except");
                friend_raw = multipart_field_all(ctype, body, "friend");
                group_raw = multipart_field_all(ctype, body, "group");
            } else {
                auto f = parse_form(body);
                anybody = f["anybody"] == "1";
                friends_on = f["friends"] == "1";
                groups_on = f["groups"] == "1";
                except_raw = parse_form_multi(body, "except");
                friend_raw = parse_form_multi(body, "friend");
                group_raw = parse_form_multi(body, "group");
            }
            // Anybody is exclusive; "nothing chosen" falls back to today's
            // only behavior (Friends, all friends) -- never trust the
            // client's own idea of these three past this normalization.
            if (anybody) { friends_on = false; groups_on = false; }
            if (!anybody && !friends_on && !groups_on) friends_on = true;

            const auto my_friends = g_store.friends_of(user);
            auto is_a_friend = [&](const std::string& n) {
                return std::find(my_friends.begin(), my_friends.end(), n) != my_friends.end();
            };
            std::vector<std::string> exceptions, friend_allowlist, groups;
            if (anybody) {
                for (const auto& n : except_raw) if (is_a_friend(trim_ws(n))) exceptions.push_back(trim_ws(n));
            }
            if (friends_on) {
                for (const auto& n : friend_raw) if (is_a_friend(trim_ws(n))) friend_allowlist.push_back(trim_ws(n));
            }
            // A group needs an actual, current membership -- same
            // join-to-post rule as posting from the community's own page
            // (see POST /c/<slug>). A checked slug that isn't (anymore) a
            // real membership is just dropped, not trusted from the client.
            if (groups_on) {
                for (const auto& s : group_raw) {
                    auto slug = trim_ws(s);
                    if (!slug.empty() && g_store.is_member(slug, user)) groups.push_back(slug);
                }
                if (groups.empty()) return http_redir("/feed?err=nogroup");
            }

            std::string community = groups.empty() ? "" : groups.front();
            std::vector<std::string> extra_groups(groups.empty() ? groups : std::vector<std::string>(groups.begin()+1, groups.end()));
            const PostAudience audience = anybody ? PostAudience::Anybody
                : (groups_on && !friends_on) ? PostAudience::GroupsOnly
                : PostAudience::Friends;
            const auto new_post = g_store.add_post(user, text, community, "", 0, c.attachment_blob_id, c.attachment_mime,
                              audience, exceptions, friend_allowlist, extra_groups);
            // Reposts are someone else's words. Anything else this person
            // wrote is noticed: Anybody can be Flowed; the rest stay private.
            if (new_post) {
                Post noted;
                noted.id = new_post;
                noted.audience = audience;
                noted.community = community;
                noted.extra_groups = extra_groups;
                notice_pains(user, text, post_pain_source(noted));
            }
        }
        return http_redir("/feed");
    }
    if(method=="POST"&&path=="/proposals") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        auto title = trim_ws(f["title"]);
        auto ptext = trim_ws(f["body"]);
        if (title.empty() || ptext.empty()) return http_redir("/proposals?err=empty");
        BallotSpec spec;
        int kind = 0;
        try { kind = std::stoi(f["kind"]); } catch (...) { kind = 0; }
        if (kind < 0 || kind > 4) kind = 0;
        spec.kind = static_cast<BallotKind>(kind);
        if (spec.kind == BallotKind::Number) {
            spec.unit = trim_ws(f["unit"]);
            if (spec.unit.size() > 24) spec.unit.resize(24);
            auto mins = trim_ws(f["min"]);
            auto maxs = trim_ws(f["max"]);
            if (!mins.empty() || !maxs.empty()) {
                try { spec.min_v = std::stod(mins); } catch (...) { return http_redir("/proposals?err=range"); }
                try { spec.max_v = std::stod(maxs); } catch (...) { return http_redir("/proposals?err=range"); }
                if (!(spec.min_v < spec.max_v)) return http_redir("/proposals?err=range");
            }
        } else if (spec.kind == BallotKind::Choice) {
            spec.open_choice = !trim_ws(f["open"]).empty();
            std::istringstream in(f["options"]);
            std::string line;
            while (std::getline(in, line)) {
                auto o = trim_ws(line);
                if (!o.empty()) spec.options.push_back(o);
            }
            const std::size_t mn = spec.open_choice ? 1 : 2;
            if (spec.options.size() < mn || spec.options.size() > 12) return http_redir("/proposals?err=options");
        } else if (spec.kind == BallotKind::Scale) {
            auto left = trim_ws(f["left"]);
            auto right = trim_ws(f["right"]);
            spec.options = {left, right};
        }
        const std::string city_text = trim_ws(f["city"]);
        const std::string who_text = trim_ws(f["who"]);
        std::string city_key_chosen;
        if (!city_text.empty()) {
            city_key_chosen = city_key(clean_city(city_text));
            if (city_key_chosen.empty() || g_store.communities_in_city(city_key_chosen).empty())
                return http_redir("/proposals?err=pcity");
        }
        auto id = g_store.add_proposal(user, title, ptext, spec);
        if (id == 0) return http_redir("/proposals?err=range");
        notice_pains(user, title + ". " + ptext, "prop:" + std::to_string(id));
        if (!city_key_chosen.empty()) g_store.set_proposal_city(id, city_text);
        if (!who_text.empty()) g_store.set_proposal_actor(id, who_text);
        if (!city_key_chosen.empty()) g_store.set_proposal_tags(id, parse_form_multi(body, "tag"));
        if (!city_key_chosen.empty()) return http_redir("/proposals?city="+url_encode(city_key_chosen));
        return http_redir("/proposals");
    }
    if(method=="POST"&&path=="/proposals/option") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        std::uint64_t id = 0;
        try { id = std::stoull(f["id"]); } catch (...) { return http_redir("/proposals"); }
        if (!g_store.add_proposal_option(id, user, trim_ws(f["label"])))
            return http_redir("/proposals?err=option");
        auto p = g_store.proposal_by_id(id);
        if (!p) return http_redir("/proposals");
        return http_redir(back_to_proposal(trim_ws(f["next"]), *p));
    }
    if(method=="GET"&&path=="/officials") {
        std::string cards;
        for (const auto& pr : g_store.all_proposals()) {
            if (!pr.is_office) continue;
            cards += "<div class=\"card\" id=\"proposal-"+std::to_string(pr.id)+"\"><h2>"+html_escape(pr.title)+"</h2>"
                     "<p class=\"muted\">"+html_escape(pr.jurisdiction.empty()?pr.body:pr.jurisdiction)+"</p>";
            const std::string next_in = user.empty() ? std::string()
                : "<input type=\"hidden\" name=\"next\" value=\"/officials\">";
            // The same Choice body as a proposal: index payload, "name · count", your check mark (Brief 7 item 1).
            cards += choice_ballot_body(pr, user, next_in, "candidate");
            cards += proposal_talk(pr, user, next_in);
            cards += "</div>";
        }
        if (cards.empty()) cards = "<p class=\"muted\">No races yet.</p>";
        std::string form;
        if (acc) {
            form = "<div class=\"card\"><h2>Open a race</h2>"
                   "<p class=\"muted\">Anyone with an account can vote. Not a certified election.</p>"
                   "<form method=\"POST\" action=\"/officials\">"
                   "<label>Office</label><input name=\"title\" maxlength=\"80\" required placeholder=\"Mayor\">"
                   "<label>Place</label><input name=\"place\" maxlength=\"80\" required placeholder=\"Fresno, CA\">"
                   "<label>Candidates (one per line)</label>"
                   "<textarea name=\"options\" maxlength=\"800\" placeholder=\"One name per line\"></textarea>"
                   "<label class=\"row\" style=\"gap:.4rem;align-items:center;margin-top:.4rem\">"
                   "<input type=\"checkbox\" name=\"open\" value=\"1\" checked> Allow people to add a candidate</label>"
                   "<button type=\"submit\">Open race</button></form></div>";
        } else {
            form = "<p class=\"muted\"><a href=\"/login?next=/officials\">Log in</a> to vote or open a race.</p>";
        }
        return http_ok(wrap("Officials", user, unread,
            std::string("<h1>Officials</h1>"
            "<p class=\"muted\">Account vote only. Not a certified election and not proof of residency.</p>")
            + how_proposals_work_html()
            + cards + form));
    }
    if(method=="POST"&&path=="/officials") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        auto title = trim_ws(f["title"]);
        auto place = trim_ws(f["place"]);
        if (title.empty() || title.size() > 80 || place.empty() || place.size() > 80)
            return http_redir("/officials");
        BallotSpec spec;
        spec.kind = BallotKind::Choice;
        spec.open_choice = !trim_ws(f["open"]).empty();
        std::istringstream in(f["options"]);
        std::string line;
        while (std::getline(in, line)) {
            auto o = trim_ws(line);
            if (!o.empty()) spec.options.push_back(o);
        }
        const std::size_t mn = spec.open_choice ? 1 : 2;
        if (spec.options.size() < mn || spec.options.size() > 12)
            return http_redir("/officials");
        auto nid = g_store.add_proposal(user, title, place, spec);
        if (!nid || !g_store.mark_proposal_office(nid, place))
            return http_redir("/officials");
        return http_redir("/officials");
    }
    if(method=="POST"&&path=="/proposals/reply") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        std::uint64_t id = 0;
        try { id = std::stoull(f["id"]); } catch (...) { return http_redir("/proposals"); }
        auto p = g_store.proposal_by_id(id);
        if (!p) return http_redir("/proposals");
        auto text = trim_ws(f["text"]);
        if (!text.empty()) {
            const auto rid = g_store.add_proposal_reply(id, user, text);
            if (rid) notice_pains(user, text, "prply:" + std::to_string(rid));
        }
        return http_redir(back_to_proposal(trim_ws(f["next"]), *p));
    }
    if(method=="POST"&&path=="/proposals/react") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        std::uint64_t id = 0;
        try { id = std::stoull(f["id"]); } catch (...) { return http_redir("/proposals"); }
        auto p = g_store.proposal_by_id(id);
        if (!p) return http_redir("/proposals");
        const std::string kind = trim_ws(f["kind"]);
        auto cur = g_store.proposal_reaction_of(id, user);
        if (kind.empty() || (cur && *cur == kind)) g_store.set_proposal_reaction(id, user, "");
        else g_store.set_proposal_reaction(id, user, kind);
        return http_redir(back_to_proposal(trim_ws(f["next"]), *p));
    }
    if(method=="POST"&&path=="/proposals/vote") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        std::uint64_t id = 0;
        try { id = std::stoull(f["id"]); } catch (...) { return http_redir("/proposals"); }
        auto p = g_store.proposal_by_id(id);
        if (!p) return http_redir("/proposals");
        if (p->ballot.kind == BallotKind::YesNo) {
            int cv = 2;
            try { cv = std::stoi(f["choice"]); } catch (...) { cv = 2; }
            if (cv < 0 || cv > 2) cv = 2;
            g_store.cast_proposal_vote(id, user, static_cast<ProposalVote>(cv));
        } else {
            g_store.cast_ballot_value(id, user, trim_ws(f["value"]));
        }
        return http_redir(back_to_proposal(trim_ws(f["next"]), *p));
    }
    if(method=="POST"&&path=="/friends") {
        if(!acc) return need_login();
        auto f=parse_form(body);
        auto who=trim_ws(f["who"]);
        if(valid_username(who) && who!=user && g_store.find_by_username(who)
           && !g_store.is_blocked(user, who) && !g_store.is_blocked(who, user)) {
            g_store.add_friend_request(user, who);
        }
        auto dest = safe_next_path(trim_ws(f["next"]));
        if (dest.empty()) dest = "/friends";
        return http_redir(dest);
    }
    if(method=="POST"&&path=="/friends/accept") {
        if(!acc) return need_login();
        auto who=parse_form(body)["who"];
        g_store.accept_friend_request(who, user);
        if (g_store.incoming_requests(user).empty())
            g_store.mark_notices_read_of_type(user, "friend_request");
        return http_redir("/friends");
    }
    // Decline (2026-11-10): the request just goes away; the sender is not told (same as never answering).
    if(method=="POST"&&path=="/friends/decline") {
        if(!acc) return need_login();
        auto who=trim_ws(parse_form(body)["who"]);
        g_store.decline_friend_request(who, user);
        if (g_store.incoming_requests(user).empty())
            g_store.mark_notices_read_of_type(user, "friend_request");
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/friends/unfriend") {
        if(!acc) return need_login();
        g_store.unfriend(user, parse_form(body)["who"]);
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/friends/block") {
        if(!acc) return need_login();
        auto who=parse_form(body)["who"];
        if (valid_username(who) && who!=user) g_store.block(user, who);
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/friends/inner") {
        if(!acc) return need_login();
        if (!g_store.add_inner(user, parse_form(body)["who"]))
            return http_redir("/friends?err=inner");
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/friends/inner-remove") {
        if(!acc) return need_login();
        g_store.remove_inner(user, parse_form(body)["who"]);
        return http_redir("/friends");
    }
    if(method=="POST"&&path=="/mail") {
        if(!acc) return need_login();
        auto hdr_end = req.find("\r\n\r\n");
        std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
        std::string ctype = header_value(hdrs, "Content-Type");
        std::string to, text;
        if (ctype.find("multipart/form-data") != std::string::npos) {
            to = multipart_field(ctype, body, "to");
            text = multipart_field(ctype, body, "text");
        } else {
            auto f = parse_form(body);
            to = f["to"]; text = f["text"];
        }
        const MailUpload up = take_mail_upload(ctype, body); // releases the body
        if(text.size()>2000) text.resize(2000);
        // Do not open stranger DMs: friends-only, both directions unblocked,
        // exactly as before attachments existed.
        if(to.empty() || !g_store.are_friends(user,to) || g_store.is_blocked(user,to) || g_store.is_blocked(to,user))
            return http_redir("/mail?with="+to+"&err=notfriends");

        std::optional<std::string> blob_id, mime;
        std::string download_name;
        if (up.file && !up.file->empty()) {
            const std::string code = ingest_mail_attachment(user, up, blob_id, mime, download_name);
            if (!code.empty()) return http_redir("/mail?with="+to+"&err="+code);
        }
        if (!text.empty() || blob_id) {
            g_store.add_mail(user,to,text,blob_id,mime);
            if (!text.empty()) {
                const auto thread = g_store.mail_with(user, to);
                for (auto it = thread.rbegin(); it != thread.rend(); ++it) {
                    if (it->from == user && it->text == text) {
                        notice_pains(user, text, "mail:" + std::to_string(it->ts) + ":" + to);
                        break;
                    }
                }
            }
            if (!download_name.empty()) { // named once the message exists (review 2026-10-27)
                const auto thread = g_store.mail_with(user, to);
                for (auto it = thread.rbegin(); it != thread.rend(); ++it)
                    if (it->from == user && it->attachment_blob_id == blob_id) {
                        g_store.set_attachment_name(attachment_name_key(*it), download_name);
                        break;
                    }
            }
        }
        return http_redir("/mail?with="+to);
    }

    if(method=="POST"&&path=="/mail/group/new") {
        if(!acc) return need_login();
        auto form = parse_form(body);
        const auto who = form["who"];
        auto title = trim_ws(form["title"]);
        if (title.size() > 80) title.resize(80);
        std::vector<AccountId> invitees;
        std::vector<std::string> skipped;
        std::string cur;
        auto flush_tok = [&]() {
            const auto name = trim_ws(cur);
            cur.clear();
            if (name.empty() || name == user) return;
            auto found = g_store.find_by_username(name);
            if (!found || g_store.is_blocked(user, name) || g_store.is_blocked(name, user)) {
                skipped.push_back(name);
                return;
            }
            for (auto id : invitees) if (id == found->id) return;
            invitees.push_back(found->id);
        };
        for (char c : who) {
            if (c==','||c==' '||c=='\t'||c=='\n'||c=='\r') flush_tok();
            else cur += c;
        }
        flush_tok();
        auto result = create_conversation(g_store, g_social_graph, acc->id, invitees, /*is_group=*/true);
        auto conv = g_store.get_conversation(*result.conversation_id);
        if (conv && !title.empty()) {
            conv->title = title;
            g_store.save_conversation(*conv);
        }
        if (conv) {
            std::set<AccountId> kept;
            for (auto& m : conv->members) kept.insert(m.account);
            for (auto id : invitees) {
                if (!kept.count(id)) {
                    if (auto a = g_store.find_by_id(id)) skipped.push_back(a->username);
                } else if (auto a = g_store.find_by_id(id)) {
                    auto pr = g_store.prefs_of(a->username);
                    if (pr.notify_message) g_store.add_notice(a->username, "mail_group", user, std::to_string(conv->id));
                }
            }
        }
        std::string loc = "/mail/g/"+std::to_string(*result.conversation_id);
        if (!skipped.empty()) {
            loc += "?skipped=";
            for (size_t i=0;i<skipped.size();++i) {
                if (i) loc += ",";
                loc += url_encode(skipped[i]);
            }
        }
        return http_redir(loc);
    }

    if(method=="POST"&&path.rfind("/mail/g/",0)==0) {
        if(!acc) return need_login();
        std::string rest = path.substr(8);
        auto slash = rest.find('/');
        const std::string id_str = (slash==std::string::npos) ? rest : rest.substr(0,slash);
        const std::string action = (slash==std::string::npos) ? std::string() : rest.substr(slash+1);
        ConversationId conv_id = 0;
        try { conv_id = std::stoull(id_str); } catch(...) { return http_redir("/mail"); }
        auto conv = g_store.get_conversation(conv_id);
        if (!conv || !conv->is_group) return http_redir("/mail");
        const ConversationMember* self_member = nullptr;
        for (auto& m : conv->members) if (m.account == acc->id) self_member = &m;
        const std::string base = "/mail/g/"+id_str;

        if (action=="accept") {
            if (!self_member) return http_redir("/mail");
            accept_invite(g_store, acc->id, conv_id);
            return http_redir(base);
        }
        if (action=="leave") {
            if (!self_member) return http_redir("/mail");
            leave_conversation(g_store, acc->id, conv_id); // Pending "decline" and Active "leave" alike; never notified
            return http_redir("/mail");
        }
        // Everything below requires being an Active member.
        if (!self_member || self_member->state != MemberState::Active) return http_redir("/mail");

        if (action=="invite") {
            const auto who = trim_ws(parse_form(body)["who"]);
            auto found = g_store.find_by_username(who);
            bool refuse = !found || g_store.is_blocked(user, who) || g_store.is_blocked(who, user);
            if (!refuse) {
                auto r = add_member(g_store, g_social_graph, acc->id, found->id, conv_id);
                refuse = r.error != MessengerError::None;
                if (!refuse) {
                    auto pr = g_store.prefs_of(found->username);
                    if (pr.notify_message) g_store.add_notice(found->username, "mail_group", user, id_str);
                }
            }
            return http_redir(refuse ? (base+"?err=invite") : base);
        }
        if (action=="title") {
            auto title = trim_ws(parse_form(body)["title"]);
            if (title.size() > 80) title.resize(80);
            conv->title = title;
            g_store.save_conversation(*conv);
            return http_redir(base);
        }
        if (action=="send") {
            auto hdr_end = req.find("\r\n\r\n");
            std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
            std::string ctype = header_value(hdrs, "Content-Type");
            std::string text;
            if (ctype.find("multipart/form-data") != std::string::npos) {
                text = multipart_field(ctype, body, "text");
            } else {
                text = parse_form(body)["text"];
            }
            const MailUpload up = take_mail_upload(ctype, body); // releases the body
            text = trim_ws(text);
            if (text.size()>2000) text.resize(2000);
            std::optional<std::string> blob_id, mime;
            std::string download_name;
            if (up.file && !up.file->empty()) {
                const std::string code = ingest_mail_attachment(user, up, blob_id, mime, download_name);
                if (!code.empty()) return http_redir(base+"?err="+code);
            }
            if (!text.empty() || blob_id) {
                auto sent = send_message(g_store, acc->id, conv_id, text);
                if (sent.message_id && !text.empty())
                    notice_pains(user, text, "gmsg:" + std::to_string(*sent.message_id));
                if (sent.message_id && blob_id) {
                    auto msg = g_store.get_message(*sent.message_id);
                    if (msg) {
                        msg->attachment_blob_id = *blob_id;
                        msg->attachment_mime = mime.value_or("");
                        g_store.update_message(*msg);
                        if (!download_name.empty()) // named once the message exists (review 2026-10-27)
                            g_store.set_attachment_name(attachment_name_key(*msg), download_name);
                    }
                } else if (blob_id) {
                    g_blobs.release(*blob_id); // not sent (left the group, blocked...): don't keep up to 100 MB for nothing
                }
            }
            return http_redir(base);
        }
        if (action.rfind("message/",0)==0) {
            const std::string mrest = action.substr(8);
            const auto mslash = mrest.find('/');
            if (mslash==std::string::npos) return http_redir(base);
            MessageId msg_id = 0;
            try { msg_id = std::stoull(mrest.substr(0,mslash)); } catch(...) { return http_redir(base); }
            const std::string mact = mrest.substr(mslash+1);
            if (mact=="edit") {
                auto text = trim_ws(parse_form(body)["text"]);
                if (!text.empty()) {
                    if (text.size()>2000) text.resize(2000);
                    edit_message(g_store, acc->id, msg_id, text);
                }
            } else if (mact=="delete") {
                delete_message(g_store, acc->id, msg_id, /*for_everyone=*/true);
            }
            return http_redir(base);
        }
        return http_redir(base);
    }
    if(method=="POST"&&path=="/communities") {
        if(!acc) return need_login();
        auto f=parse_form(body);
        auto slug=trim_ws(f["slug"]);
        auto name=trim_ws(f["name"]);
        auto about=f["about"]; if(about.size()>2000) about.resize(2000);
        int cat = 10;
        try { cat = std::stoi(f["category"]); } catch (...) { cat = 10; }
        if (cat < 0 || cat > 10) cat = 10;
        auto subcategory = trim_ws(f["subcategory"]);
        if (subcategory.size() > 60) subcategory.resize(60);
        if (subcategory.empty()) return http_redir("/communities?err=subcategory");
        Community c;
        c.slug = slug; c.name = name.empty()?slug:name; c.owner = user; c.about = about; c.created_at = now();
        c.category = static_cast<CommunityCategory>(cat);
        c.subcategory = subcategory;
        c.city = f["city"]; // optional; FileStore tidies it (clean_city) and drops text with no letters
        if (!g_store.create_community(c)) return http_redir("/communities?err=address");
        return http_redir("/c/"+slug);
    }
    if(method=="POST"&&path.rfind("/c/",0)==0) {
        if(!acc) return need_login();
        std::string rest = path.substr(3);
        std::string slug, action;
        auto slash = rest.find('/');
        if (slash==std::string::npos) { slug = rest; action = "post"; }
        else { slug = rest.substr(0,slash); action = rest.substr(slash+1); }
        if (!g_store.community(slug)) return http_redir("/communities");
        if (action=="join") { g_store.join_community(slug, user); return http_redir("/c/"+slug); }
        if (action=="city") {
            // Owner only (set_community_city checks); empty clears it.
            auto f=parse_form(body);
            if (!g_store.set_community_city(slug, user, f["city"])) return http_redir("/c/"+slug+"?err=city");
            return http_redir("/c/"+slug);
        }
        if (action=="leave") { g_store.leave_community(slug, user); return http_redir("/c/"+slug); }
        if (action=="delete") { g_store.delete_community(slug, user); return http_redir("/communities"); }
        if (action=="invite") {
            auto who=trim_ws(parse_form(body)["who"]);
            if (!g_store.invite_community(slug, user, who)) return http_redir("/c/"+slug+"?err=invite");
            return http_redir("/c/"+slug);
        }
        if (action=="post") {
            if (!g_store.is_member(slug, user)) return http_redir("/c/"+slug);
            auto hdr_end = req.find("\r\n\r\n");
            std::string hdrs = hdr_end==std::string::npos ? req : req.substr(0, hdr_end);
            auto c = read_composer_post(hdrs, body);
            if (!c.ok) return http_redir("/c/"+slug+"?err="+c.err);
            auto text = trim_ws(c.text);
            if (!text.empty() || !c.attachment_blob_id.empty()) {
                const auto gid = g_store.add_post(user, text, slug, "", 0, c.attachment_blob_id, c.attachment_mime);
                if (gid) notice_pains(user, text, "gpost:" + std::to_string(gid));
            }
            return http_redir("/c/"+slug);
        }
        return http_redir("/c/"+slug);
    }
    if(method=="POST"&&path=="/settings/revoke") {
        if(!acc) return need_login();
        auto target = parse_form(body)["sid"];
        bool current = target == sid;
        bool owned = false;
        for (const auto& s : g_store.sessions_for(acc->id)) {
            if (s.id == target) { owned = true; break; }
        }
        if (owned) logout(g_store, target);
        if (current) return http_redir("/login", clear_cookie());
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/logout-all") {
        if(!acc) return need_login();
        logout_all_devices(g_store, acc->id, sid);
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/password") {
        if(!acc) return need_login();
        auto f=parse_form(body);
        // A Google-only account has no real password yet -- its hash is
        // the same dummy_hash() placeholder login() uses for a timing-
        // safe "unknown username" rejection, which by design never
        // verifies against anything. Skip the current-password check
        // only for that exact placeholder; a real account still has to
        // prove its current password like always.
        const bool has_real_password = acc->password_hash != dummy_hash();
        if ((has_real_password && !verify_password(f["current"], acc->password_hash)) || f["next"].empty())
            return http_redir("/settings/password?err=password");
        if (f["next"].size() < 8)
            return http_redir("/settings/password?err=short");
        if (g_breach.is_breached(f["next"]))
            return http_redir("/settings/password?err=password");
        g_store.set_password_hash(acc->id, hash_password(f["next"]));
        return http_redir("/settings?ok=password");
    }
    if(method=="POST"&&path=="/settings/map") {
        if(!acc) return need_login();
        auto f=parse_form(body);
        MapPrefs mp;
        mp.show = f["show"] == "1";
        mp.groups = f["groups"] == "1";
        mp.posts = f["posts"] == "1";
        mp.comments = f["comments"] == "1";
        mp.friends = f["friends"] == "1";
        mp.pains = f["pains"] == "1";
        g_store.set_map_prefs(user, mp); // pains off deletes what it found
        return http_redir("/settings#map-privacy");
    }
    if(method=="POST"&&path=="/settings/privacy") {
        if(!acc) return need_login();
        auto f=parse_form(body);
        auto p = g_store.prefs_of(user);
        int fv = 1;
        try { fv = std::stoi(f["findability"]); } catch (...) { fv = 1; }
        if (fv < 0 || fv > 2) fv = 1;
        p.findability = static_cast<Findability>(fv);
        p.group_invite_confirm = f["group_invite_confirm"] == "1";
        p.presence_hide = f["presence_hide"] == "1";
        auto vis_of = [&](const std::string& field, Visibility fallback) {
            int v = static_cast<int>(fallback);
            try { v = std::stoi(f[field]); } catch (...) { v = static_cast<int>(fallback); }
            if (v < 0 || v > 3) v = static_cast<int>(fallback);
            return static_cast<Visibility>(v);
        };
        p.profile_visibility = vis_of("profile_visibility", p.profile_visibility);
        p.posts_visibility = vis_of("posts_visibility", p.posts_visibility);
        g_store.set_prefs(user, p);
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/notify") {
        if(!acc) return need_login();
        auto f=parse_form(body);
        auto p = g_store.prefs_of(user);
        p.notify_friend = f["notify_friend"]=="1";
        p.notify_message = f["notify_message"]=="1";
        p.notify_group = f["notify_group"]=="1";
        p.intro_every_visit = f["intro_every_visit"]=="1";
        g_store.set_prefs(user, p);
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/unblock") {
        if(!acc) return need_login();
        g_store.unblock(user, parse_form(body)["who"]);
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/block") {
        if(!acc) return need_login();
        auto f = parse_form(body);
        auto who = trim_ws(f["who"]);
        if (valid_username(who) && who != user) g_store.block(user, who, trim_ws(f["reason"]));
        return http_redir("/settings");
    }
    if(method=="POST"&&path=="/settings/delete") {
        if(!acc) return need_login();
        auto confirm = trim_ws(parse_form(body)["confirm"]);
        if (confirm != user) return http_redir("/settings?err=delete");
        clear_avatar(g_blobs, g_store, acc->id);
        g_store.delete_account(acc->id);
        return http_redir("/", clear_cookie());
    }

    if(method=="GET"&&path=="/admin") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        return pg("Operator tools", admin_overview_page());
    }
    if(method=="GET"&&path=="/admin/accounts") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        return pg("Operator tools", admin_accounts_page(user, trim_ws(qs["q"])));
    }
    if(method=="GET"&&path=="/admin/posts") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        return pg("Operator tools", admin_posts_page(qs.count("q") ? trim_ws(qs.at("q")) : ""));
    }
    if(method=="GET"&&path=="/admin/communities") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        return pg("Operator tools", admin_communities_page());
    }
    if(method=="GET"&&path=="/admin/proposals") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        const std::string msg = qs.count("done") ? (qs.at("done") == "deleted" ? "Deleted; a copy is in the moderation archive."
                              : qs.at("done") == "edited" ? "Saved." : qs.at("done") == "archive-failed"
                              ? "Not deleted: the moderation archive could not be written (see the server log)." : "Nothing changed.") : "";
        return pg("Operator tools", admin_proposals_page(msg));
    }
    if(method=="POST"&&path=="/admin/proposals/delete") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        std::uint64_t pid = 0;
        try { pid = std::stoull(parse_form(body)["id"]); } catch (...) { return http_redir("/admin/proposals?done=none"); }
        auto target = g_store.proposal_by_id(pid);
        if (!target) return http_redir("/admin/proposals?done=none");
        if (!archive_deleted_proposal(*target, user)) return http_redir("/admin/proposals?done=archive-failed");
        g_store.delete_proposal(pid);
        g_store.add_audit_log(user, "delete_proposal", std::to_string(pid) + " by " + target->author + ": " + target->title.substr(0, 60));
        return http_redir("/admin/proposals?done=deleted");
    }
    if(method=="GET"&&path=="/admin/comments") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        const std::string done = qs.count("done") ? qs.at("done") : "";
        const std::string msg = done == "deleted" ? "Deleted; a copy is in the moderation archive."
                              : done == "archive-failed" ? "Not deleted: the moderation archive could not be written (see the server log)."
                              : done.empty() ? "" : "Nothing changed.";
        return pg("Operator tools", admin_comments_page(qs.count("q") ? trim_ws(qs.at("q")) : "", msg));
    }
    if(method=="POST"&&path=="/admin/comments/delete") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto f = parse_form(body);
        std::uint64_t cid = 0;
        try { cid = std::stoull(f["id"]); } catch (...) { return http_redir("/admin/comments?done=none"); }
        if (f["on"] == "post") {
            auto r = g_store.post_reply_by_id(cid);
            if (!r) return http_redir("/admin/comments?done=none");
            if (!archive_deleted_comment("post", r->id, r->post_id, r->author, r->ts, r->text, user))
                return http_redir("/admin/comments?done=archive-failed");
            g_store.delete_post_reply(cid);
            g_store.add_audit_log(user, "delete_comment", "post comment " + std::to_string(cid) + " by " + r->author);
        } else if (f["on"] == "proposal") {
            auto r = g_store.proposal_reply_by_id(cid);
            if (!r) return http_redir("/admin/comments?done=none");
            if (!archive_deleted_comment("proposal", r->id, r->proposal_id, r->author, r->ts, r->text, user))
                return http_redir("/admin/comments?done=archive-failed");
            g_store.delete_proposal_reply(cid);
            g_store.add_audit_log(user, "delete_comment", "proposal comment " + std::to_string(cid) + " by " + r->author);
        } else {
            return http_redir("/admin/comments?done=none");
        }
        return http_redir("/admin/comments?done=deleted");
    }
    if(method=="GET"&&path=="/admin/departments") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        const std::string done = qs.count("done") ? qs.at("done") : "";
        const std::string msg = done == "deleted" ? "Deleted; a copy is in the moderation archive."
                              : done == "archive-failed" ? "Not deleted: the moderation archive could not be written (see the server log)."
                              : done.empty() ? "" : "Nothing changed.";
        return pg("Operator tools", admin_departments_page(qs.count("q") ? trim_ws(qs.at("q")) : "", msg));
    }
    if(method=="POST"&&path=="/admin/departments/delete") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto f = parse_form(body);
        const std::string key = f["city"], tag = f["tag"];
        bool known = false;
        for (const auto& t : g_store.city_tags(key)) if (t == tag) { known = true; break; }
        if (key.empty() || !known) return http_redir("/admin/departments?done=none");
        const auto props = g_store.proposals_with_city_tag(key, tag);
        if (!archive_deleted_city_tag(key, tag, props, user)) return http_redir("/admin/departments?done=archive-failed");
        if (!g_store.delete_city_tag(key, tag)) return http_redir("/admin/departments?done=none");
        g_store.add_audit_log(user, "delete_city_tag", key + ": " + tag.substr(0, 80) + " (" + std::to_string(props.size()) + " proposal(s))");
        return http_redir("/admin/departments?done=deleted");
    }
    if(method=="POST"&&path=="/admin/proposals/edit") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto f = parse_form(body);
        std::uint64_t pid = 0;
        try { pid = std::stoull(f["id"]); } catch (...) { return http_redir("/admin/proposals?done=none"); }
        const std::string title = trim_ws(f["title"]).substr(0, 200);
        const std::string text = f["body"].substr(0, 10000);
        if (!g_store.edit_proposal(pid, title, text)) return http_redir("/admin/proposals?done=none");
        g_store.add_audit_log(user, "edit_proposal", std::to_string(pid));
        return http_redir("/admin/proposals?done=edited");
    }
    if(method=="GET"&&path=="/admin/log") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        return pg("Operator tools", admin_log_page());
    }
    if(method=="POST"&&path=="/admin/accounts/lock") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto target = trim_ws(parse_form(body)["username"]);
        if (!target.empty() && target != user && g_store.find_by_username(target)) {
            g_store.set_locked(target, true);
            g_store.add_audit_log(user, "lock", target);
        }
        return http_redir("/admin/accounts");
    }
    if(method=="POST"&&path=="/admin/accounts/programs") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto form = parse_form(body);
        auto target = trim_ws(form["username"]);
        if (!target.empty() && g_store.find_by_username(target) && !is_operator(target)) {
            ProgramGrant g;
            g.username = target;
            for (const auto& k : program_kinds())
                if (!form[std::string("k_") + k.slug].empty()) g.kinds |= static_cast<std::uint32_t>(k.kind);
            std::uint64_t mb = 0;
            try { mb = std::stoull(trim_ws(form["max_mb"])); } catch (...) { mb = 0; }
            const std::uint64_t ceiling_mb = program_ceiling_bytes() / (1024 * 1024);
            mb = std::max<std::uint64_t>(1, std::min<std::uint64_t>(mb, ceiling_mb));
            g.max_bytes = mb * 1024 * 1024;
            g.granted_by = user;
            g.granted_at = now();
            const bool had = g_store.program_grant(target).has_value();
            g_store.set_program_grant(g);
            if (g.kinds) g_store.add_audit_log(user, "programs_grant", target);
            else if (had) g_store.add_audit_log(user, "programs_revoke", target);
        }
        return http_redir("/admin/accounts");
    }
    if(method=="POST"&&path=="/admin/accounts/unlock") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto target = trim_ws(parse_form(body)["username"]);
        if (!target.empty() && g_store.find_by_username(target)) {
            g_store.set_locked(target, false);
            g_store.add_audit_log(user, "unlock", target);
        }
        return http_redir("/admin/accounts");
    }
    if(method=="POST"&&path=="/admin/accounts/logout-all") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto target = trim_ws(parse_form(body)["username"]);
        if (!target.empty() && target != user) {
            if (auto ta = g_store.find_by_username(target)) {
                logout_all_devices(g_store, ta->id, "");
                g_store.add_audit_log(user, "logout_all", target);
            }
        }
        return http_redir("/admin/accounts");
    }
    if(method=="POST"&&path=="/admin/accounts/delete") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto f = parse_form(body);
        auto target = trim_ws(f["username"]);
        auto confirm = trim_ws(f["confirm"]);
        if (!target.empty() && target != user && confirm_says_delete(confirm)) {
            if (auto ta = g_store.find_by_username(target)) {
                clear_avatar(g_blobs, g_store, ta->id);
                g_store.delete_account(ta->id);
                g_store.add_audit_log(user, "delete_account", target);
            }
        }
        return http_redir("/admin/accounts");
    }
    if(method=="POST"&&path=="/admin/posts/delete") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        std::uint64_t pid = 0;
        try { pid = std::stoull(parse_form(body)["id"]); } catch (...) { return http_redir("/admin/posts"); }
        if (auto target = g_store.post_by_id(pid)) {
            g_store.delete_post(pid, target->author);
            g_store.add_audit_log(user, "delete_post", std::to_string(pid));
        }
        return http_redir("/admin/posts");
    }
    if(method=="POST"&&path=="/admin/communities/delete") {
        if(!acc) return need_login();
        if(!is_operator(user)) return admin_404();
        auto f = parse_form(body);
        auto slug = trim_ws(f["slug"]);
        auto confirm = trim_ws(f["confirm"]);
        if (!slug.empty() && confirm_says_delete(confirm)) {
            if (auto c = g_store.community(slug)) {
                g_store.delete_community(slug, c->owner);
                g_store.add_audit_log(user, "delete_community", slug);
            }
        }
        return http_redir("/admin/communities");
    }

    return http_code("404 Not Found", wrap("Not found", user, unread, "<div class=\"card\"><h1>Not found</h1><p>That page is not here.</p></div>"));
}
}
int main() {
    // A write to a socket the other side already closed (a /world or jukebox player who just left, a browser
    // that hung up mid-download) or to the password-reset mail pipe after curl quit raises SIGPIPE, whose
    // default action kills this whole process -- every route, every connected user, all in-memory room state.
    // Ignored, that write just fails with EPIPE, which every write here already handles (ws_write_frame returns
    // false, page sends close the socket, the mail path logs pclose's status). Found by a capacity bench of the
    // /world room code: 200 players leaving at once killed it with exit 141 (2026-10-30).
    std::signal(SIGPIPE, SIG_IGN);

    // FilesystemBlobStore::get()/meta() both gate on an in-memory refcount
    // entry that ONLY put() or adopt() ever create -- bytes sitting on
    // disk from a previous process are otherwise invisible, not just
    // unreferenced. Every blob id this server can later re-fetch by id
    // (as opposed to one only ever read back inside the same put() call
    // that created it) has to be re-adopted here on every startup, or it
    // 404s the moment the process restarts. Canvas frames and post
    // attachments were missed when those features were first built --
    // found via a live report of a "missing" image that was actually
    // sitting on disk the whole time, invisible only because of this.
    for (const auto& av : g_store.all_avatars())
        g_blobs.adopt(av.blob_id, "image/png");
    for (const auto& m : g_store.all_mail_with_attachments())
        g_blobs.adopt(*m.attachment_blob_id, m.attachment_mime.value_or("application/octet-stream"));
    for (const auto& m : g_store.all_group_messages_with_attachments())
        g_blobs.adopt(m.attachment_blob_id, m.attachment_mime.empty() ? "application/octet-stream" : m.attachment_mime);
    constexpr std::int64_t kMailRetention = 7 * 24 * 60 * 60;
    purge_expired_deletions(g_store, now(), kMailRetention);
    map_friends_default_on(); // once per person: writes the friends-on default + a note (2026-11-10)
    for (const auto& blob_id : g_store.all_canvas_frame_blob_ids())
        g_blobs.adopt(blob_id, "image/png");
    for (const auto& p : g_store.all_posts_with_attachments())
        g_blobs.adopt(p.attachment_blob_id, p.attachment_mime);
    int fd=socket(AF_INET,SOCK_STREAM,0); if(fd<0) return 1;
    int yes=1; setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
    // LIQUIDGOV_PORT only for a local test copy (8080 is taken on the dev PC); production leaves it unset.
    int port = kPort;
    if (const char* p = std::getenv("LIQUIDGOV_PORT")) { try { port = std::stoi(p); } catch (...) { port = kPort; } }
    sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons(static_cast<std::uint16_t>(port)); addr.sin_addr.s_addr=htonl(INADDR_ANY);
    if(bind(fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))<0){ perror("bind"); return 1; }
    listen(fd,16);
    lglog::emit("boot", "listen");
    std::cerr<<"LiquidGov http://0.0.0.0:"<<port<<" store="<<g_store.path()
             <<" blobs="<<blob_path_from_env()<<" (auth.cpp + FileStore LGP1)\n";
    for(;;){
        sockaddr_in peer{}; socklen_t plen=sizeof(peer);
        int c=accept(fd,reinterpret_cast<sockaddr*>(&peer),&plen); if(c<0) continue;
        timeval tv{}; tv.tv_sec=30; tv.tv_usec=0;
        setsockopt(c,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
        char ipbuf[INET_ADDRSTRLEN]="0.0.0.0"; inet_ntop(AF_INET,&peer.sin_addr,ipbuf,sizeof(ipbuf));
        std::string req, body;
        bool too_large = false;
        std::size_t declared = 0;
        read_http_request(c, req, body, request_body_limit, &too_large, &declared);
        if (too_large) {
            // Answer instead of handling a cut-off upload, then read and drop what the client is still sending:
            // closing with unread bytes resets the connection, which nginx reports as a 502, not this page
            // (review 2026-10-27). Bounded: the declared length, 2 s per read, 15 s in all.
            std::string resp = http_code("413 Payload Too Large",
                page_wrap("Too large", nav("",0), "<div class=\"card\"><p>That upload is too large to send. "
                          "<a href=\"/mail\">Back to mail</a></p></div>"));
            send(c,resp.data(),resp.size(),0);
            shutdown(c, SHUT_WR);
            timeval drain_tv{}; drain_tv.tv_sec = 2;
            setsockopt(c,SOL_SOCKET,SO_RCVTIMEO,&drain_tv,sizeof(drain_tv));
            const auto drain_until = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            char sink[65536];
            std::size_t drained = 0;
            while ((declared == 0 || drained < declared) && std::chrono::steady_clock::now() < drain_until) {
                ssize_t n = recv(c, sink, sizeof(sink), 0);
                if (n <= 0) break;
                drained += static_cast<std::size_t>(n);
            }
            close(c);
            continue;
        }

        // /world/ws is the one route that doesn't get the ordinary
        // request/response treatment: a successful upgrade hands the fd to
        // its own thread and keeps it open, instead of sending one response
        // and closing. Every other route is untouched — still handled
        // synchronously on this single accept-loop thread, exactly as
        // before this feature existed.
        {
            auto line_end = req.find("\r\n");
            std::string method, rawpath;
            if (line_end != std::string::npos) {
                std::istringstream is(req.substr(0, line_end));
                is >> method >> rawpath;
            }
            if (method == "GET" && rawpath == "/world/ws" &&
                header_value(req, "Upgrade").find("websocket") != std::string::npos) {
                std::string username;
                {
                    std::lock_guard<std::mutex> lock(g_mu);
                    auto acc = account_from_req(req);
                    if (acc) username = acc->username;
                }
                auto key = header_value(req, "Sec-WebSocket-Key");
                if (!username.empty() && !key.empty()) {
                    std::string accept_val = websocket_accept_value(key);
                    std::ostringstream resp101;
                    resp101 << "HTTP/1.1 101 Switching Protocols\r\n"
                               "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                               "Sec-WebSocket-Accept: " << accept_val << "\r\n\r\n";
                    std::string r = resp101.str();
                    if (send(c, r.data(), r.size(), 0) > 0) {
                        std::thread(run_world_session, c, username).detach();
                        continue; // socket now owned by the world session thread
                    }
                }
                close(c);
                continue;
            }
            if (method == "GET" && rawpath == "/community/ws" &&
                header_value(req, "Upgrade").find("websocket") != std::string::npos) {
                std::string username;
                {
                    std::lock_guard<std::mutex> lock(g_mu);
                    auto acc = account_from_req(req);
                    if (acc) username = acc->username;
                }
                auto key = header_value(req, "Sec-WebSocket-Key");
                if (!username.empty() && !key.empty()) {
                    std::string accept_val = websocket_accept_value(key);
                    std::ostringstream resp101;
                    resp101 << "HTTP/1.1 101 Switching Protocols\r\n"
                               "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                               "Sec-WebSocket-Accept: " << accept_val << "\r\n\r\n";
                    std::string r = resp101.str();
                    if (send(c, r.data(), r.size(), 0) > 0) {
                        std::thread(run_community_session, c, username, std::string("")).detach();
                        continue;
                    }
                }
                close(c);
                continue;
            }
            // A group's own live jukebox -- same real-time station as
            // /community/ws above, just keyed by this community's slug
            // instead of the "" site-wide one, and gated to members (the
            // site-wide station stays open to anyone logged in; a group's
            // own station is for the group).
            if (method == "GET" && rawpath.rfind("/c/",0)==0 && rawpath.size()>11 &&
                rawpath.compare(rawpath.size()-11,11,"/jukebox/ws")==0 &&
                header_value(req, "Upgrade").find("websocket") != std::string::npos) {
                const std::string slug = rawpath.substr(3, rawpath.size()-3-11);
                std::string username;
                bool member = false;
                {
                    std::lock_guard<std::mutex> lock(g_mu);
                    auto acc = account_from_req(req);
                    if (acc) {
                        username = acc->username;
                        member = valid_username(slug) && g_store.is_member(slug, username);
                    }
                }
                auto key = header_value(req, "Sec-WebSocket-Key");
                if (!username.empty() && member && !key.empty()) {
                    std::string accept_val = websocket_accept_value(key);
                    std::ostringstream resp101;
                    resp101 << "HTTP/1.1 101 Switching Protocols\r\n"
                               "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                               "Sec-WebSocket-Accept: " << accept_val << "\r\n\r\n";
                    std::string r = resp101.str();
                    if (send(c, r.data(), r.size(), 0) > 0) {
                        std::thread(run_community_session, c, username, slug).detach();
                        continue;
                    }
                }
                close(c);
                continue;
            }
        }

        std::string resp;
        try {
            resp = handle(req, ipbuf, std::move(body));
            lglog::access(req, resp);
        } catch (const std::exception& e) {
            lglog::emit("error", e.what());
            std::cerr<<"handle error: "<<e.what()<<"\n";
            resp = http_code("500 Internal Server Error",
                page_wrap("Error", nav("",0), "<div class=\"card\"><p>Could not save. Try again.</p></div>"));
        }
        {
            auto le = req.find("\r\n");
            std::string meth;
            if (le != std::string::npos) { std::istringstream is(req.substr(0, le)); is >> meth; }
            if (meth == "HEAD") {
                auto pos = resp.find("\r\n\r\n");
                if (pos != std::string::npos) resp.resize(pos + 4);
            }
        }
        send(c,resp.data(),resp.size(),0); close(c);
    }
}
